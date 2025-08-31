// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project
// Modified for database creation with channel filtering

#include "config.h"
#include "FilteredSongUpdate.hxx"
#include "db/plugins/simple/Song.hxx"
#include "pcm/AudioFormat.hxx"

// External function to get the channel mode
enum class ChannelMode {
	STEREO,
	MULTICHANNEL,
	ALL
};

// Weak symbol allows override in DbMain
extern "C" __attribute__((weak)) ChannelMode GetChannelMode() noexcept {
	// Default to ALL mode (no filtering) for normal MPD
	return ChannelMode::ALL;
}


static bool
ShouldFilterByChannelCount(const AudioFormat &format) noexcept
{
	auto mode = GetChannelMode();
	if (mode == ChannelMode::ALL || !format.IsDefined())
		return false; // Don't filter if ALL mode or format unknown
	
	unsigned channels = format.channels;
	
	// If we can't determine channels, don't filter
	if (channels == 0)
		return false;
	
	// Filter based on channel count
	if (mode == ChannelMode::STEREO && channels > 2)
		return true; // Filter out multichannel
	if (mode == ChannelMode::MULTICHANNEL && channels <= 2)
		return true; // Filter out stereo
		
	return false;
}


bool
FilteredSongUpdate::ShouldIncludeSong(Song &song) noexcept
{
	auto mode = GetChannelMode();
	if (mode == ChannelMode::ALL)
		return true; // Keep everything
	
	// SACD filtering is now handled in the decoder plugin itself
	// We only need to filter non-SACD files by channel count
	
	// Check actual channel count from audio format (for non-SACD files)
	if (ShouldFilterByChannelCount(song.audio_format))
		return false;
	
	return true;
}

void
FilteredSongUpdate::ProcessSongTags(Song &song) noexcept
{
	// Tag processing for SACD is now handled directly in the SACD decoder plugin
	// This function is kept for potential future tag cleanup needs
	(void)song; // Suppress unused parameter warning
}