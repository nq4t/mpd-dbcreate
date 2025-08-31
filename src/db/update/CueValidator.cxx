// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project
// CUE file validation for database creation

#include "CueValidator.hxx"
#include "playlist/cue/CueParser.hxx"
#include "util/Domain.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "input/InputStream.hxx"
#include "input/TextInputStream.hxx"
#include "input/WaitReady.hxx"
#include "fs/Path.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "fs/DirectoryReader.hxx"
#include "util/StringCompare.hxx"
#include "thread/Mutex.hxx"
#include "Log.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"

#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cctype>

static constexpr Domain cue_validator_domain("cue_validator");

static bool
IsSupportedMediaFile(const char *filename) noexcept
{
	// Check for common audio file extensions
	static constexpr const char *extensions[] = {
		".flac", ".wav", ".ape", ".wv", ".dsf", ".dff", 
		".mp3", ".mp4", ".m4a", ".aac", ".ogg", ".opus"
	};
	
	for (const char *ext : extensions) {
		if (StringEndsWithIgnoreCase(filename, ext))
			return true;
	}
	
	return false;
}

static unsigned
CountMediaFilesInDirectory(Storage &storage, const char *directory_path) noexcept
{
	unsigned count = 0;
	
	try {
		// Get directory listing
		auto dir = storage.OpenDirectory(directory_path);
		if (!dir)
			return 0;
			
		StorageFileInfo info;
		const char *name;
		
		while ((name = dir->Read()) != nullptr) {
			if (name[0] == '.')
				continue; // Skip hidden files
				
			info = dir->GetInfo(true);
			if (info.IsRegular() && IsSupportedMediaFile(name)) {
				count++;
			}
		}
	} catch (...) {
		FmtError(cue_validator_domain,
			 "Error counting media files in {}: {}",
			 directory_path, std::current_exception());
	}
	
	return count;
}

static unsigned
CountTracksInCueFile(Storage &storage, const char *cue_path) noexcept
{
	unsigned track_count = 0;
	
	try {
		Mutex mutex;
		auto is = storage.OpenFile(cue_path, mutex);
		LockWaitReady(*is);
		
		TextInputStream tis(std::move(is));
		
		// Simple track counting - look for TRACK lines
		const char *line;
		while ((line = tis.ReadLine()) != nullptr) {
			// Look for "TRACK XX" lines
			if (strncmp(line, "TRACK", 5) == 0) {
				const char *p = line + 5;
				while (*p == ' ' || *p == '\t') p++;
				if (isdigit(*p)) {
					unsigned track_num = atoi(p);
					if (track_num > track_count)
						track_count = track_num;
				}
			}
		}
		
	} catch (...) {
		FmtError(cue_validator_domain,
			 "Error reading CUE file {}: {}",
			 cue_path, std::current_exception());
	}
	
	return track_count;
}

bool
ShouldIgnoreCueFile(Storage &storage, const char *directory_path,
		    const char *cue_filename) noexcept
{
	// Build full path to CUE file
	std::string cue_path = directory_path;
	if (!cue_path.empty() && cue_path.back() != '/')
		cue_path += '/';
	cue_path += cue_filename;
	
	// Count tracks in CUE file
	unsigned cue_tracks = CountTracksInCueFile(storage, cue_path.c_str());
	if (cue_tracks == 0) {
		FmtDebug(cue_validator_domain,
			 "CUE file {} has no tracks, ignoring",
			 cue_path);
		return true; // Invalid CUE file
	}
	
	// Count media files in directory
	unsigned media_files = CountMediaFilesInDirectory(storage, directory_path);
	
	FmtDebug(cue_validator_domain,
		 "CUE file {} has {} tracks, directory has {} media files",
		 cue_path, cue_tracks, media_files);
	
	// Apply our rules:
	// 1. If track count equals media file count, ignore CUE
	// 2. If track count is one more than media files (data track), ignore CUE
	// 3. Otherwise, use the CUE file
	
	if (cue_tracks == media_files) {
		FmtNotice(cue_validator_domain,
			  "Ignoring CUE file {} - track count matches media files",
			  cue_path);
		return true;
	}
	
	if (cue_tracks == media_files + 1) {
		FmtNotice(cue_validator_domain,
			  "Ignoring CUE file {} - appears to have data track",
			  cue_path);
		return true;
	}
	
	// Use the CUE file
	return false;
}