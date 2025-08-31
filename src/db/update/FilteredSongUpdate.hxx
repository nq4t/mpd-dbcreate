// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project
// Modified for database creation with channel filtering

#pragma once

struct Song;

namespace FilteredSongUpdate {

/**
 * Check if a song should be included based on channel filtering rules.
 * Returns true if the song should be included, false if it should be filtered out.
 */
bool
ShouldIncludeSong(Song &song) noexcept;

/**
 * Process song tags to clean up SACD-specific formatting.
 */
void
ProcessSongTags(Song &song) noexcept;

} // namespace FilteredSongUpdate