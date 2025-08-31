// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project
// CUE file validation for database creation

#pragma once

class Storage;

/**
 * Check if a CUE file should be ignored based on our rules:
 * - If the number of tracks in the CUE equals the number of media files, ignore it
 * - If the number of tracks is one more than media files (data track), ignore it
 * - Otherwise, use the CUE file
 *
 * @param storage The storage interface
 * @param directory_path Path to the directory containing the CUE file
 * @param cue_filename Name of the CUE file
 * @return true if the CUE file should be ignored, false if it should be used
 */
bool
ShouldIgnoreCueFile(Storage &storage, const char *directory_path,
		    const char *cue_filename) noexcept;