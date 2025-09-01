/*
 * Copyright (C) 2003-2025 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include <audio_stream.h>
#include <audio_track.h>
#include <stream_buffer.h>
#include <log_trunk.h>
#include <dvda_disc.h>
#include <dvda_metabase.h>
#include "DvdaIsoDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "pcm/CheckAudioFormat.hxx"
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "song/DetachedSong.hxx"
#include "fs/Path.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "thread/Cond.hxx"
#include "thread/Mutex.hxx"
#include "util/BitReverse.hxx"
#include "util/UriExtract.hxx"
#include "util/Domain.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "Log.hxx"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <vector>

static constexpr Domain dvdaiso_domain("dvdaiso");

namespace dvdaiso {

constexpr auto DVDA_TRACKXXX_FMT{ "AUDIO_TS__TRACK%03u%c.%3s" };
constexpr double SHORT_TRACK_SEC{ 2.0 };

bool        param_no_downmixes;
bool        param_no_short_tracks;
bool        param_no_untagged_tracks;
chmode_e    param_playable_area;
std::string param_tags_path;
bool        param_tags_with_iso;
bool        param_use_stdio;

AllocatedPath                      dvda_path{ nullptr };
std::unique_ptr<dvda_media_t>      dvda_media;
std::unique_ptr<dvda_reader_t>     dvda_reader;
std::unique_ptr<dvda_metabase_t>   dvda_metabase;

static bool
get_subsong(Path path_fs, unsigned& index, bool& downmix) {
	auto ptr = path_fs.GetBase().c_str();
	char area = '\0';
	char suffix[4];
	auto params = sscanf(ptr, DVDA_TRACKXXX_FMT, &index, &area, suffix);
	index--;
	downmix = area == 'D';
	return params == 3;
}

static bool
container_update(Path path_fs) {
	auto curr_path = AllocatedPath(path_fs);
	if (dvda_path == curr_path) {
		return true;
	}
	if (dvda_reader) {
		dvda_reader->close();
		dvda_reader.reset();
	}
	if (dvda_media) {
		dvda_media->close();
		dvda_media.reset();
	}
	dvda_metabase.reset();
	dvda_path.SetNull();
	if (FileExists(curr_path)) {
		if (param_use_stdio) {
			dvda_media = std::make_unique<dvda_media_file_t>();
		}
		else {
			dvda_media = std::make_unique<dvda_media_stream_t>();
		}
		dvda_reader = std::make_unique<dvda_disc_t>();
		if (!dvda_media->open(curr_path.c_str())) {
			std::string err;
			err  = "dvda_media->open('";
			err += curr_path.c_str();
			err += "') failed";
			LogWarning(dvdaiso_domain, err.c_str());
			return false;
		}
		if (!dvda_reader->open(dvda_media.get())) {
			//LogWarning(dvdaiso_domain, "dvda_reader->open(...) failed");
			return false;
		}
		if (!param_tags_path.empty() || param_tags_with_iso) {
			std::string tags_file;
			if (param_tags_with_iso) {
				tags_file = curr_path.c_str();
				tags_file.resize(tags_file.rfind('.') + 1);
				tags_file.append("xml");
			}
			dvda_metabase = std::make_unique<dvda_metabase_t>(static_cast<dvda_disc_t*>(dvda_reader.get()), param_tags_path.empty() ? nullptr : param_tags_path.c_str(), tags_file.empty() ? nullptr : tags_file.c_str());
		}
		dvda_path = curr_path;
	}
	return static_cast<bool>(dvda_reader);
}

static void
scan_info(unsigned track_index, bool downmix, TagHandler& handler) {
	auto tag_value = std::to_string(track_index + 1);
	handler.OnTag(TAG_TRACK, tag_value.c_str());
	handler.OnDuration(SongTime::FromS(dvda_reader->get_duration(track_index)));
	if (!dvda_metabase || (dvda_metabase && !dvda_metabase->get_track_info(track_index + 1, downmix, handler))) {
		dvda_reader->get_info(track_index, downmix, handler);
	}
	if (handler.WantPicture()) {
		auto has_albumart{ false };
		if (dvda_metabase) {
			has_albumart = dvda_metabase->get_albumart(handler);
		}
		if (!has_albumart) {
			static constexpr auto art_names = std::array {
				"cover.png",
				"cover.jpg",
				"cover.webp",
			};
			for (const auto art_name : art_names) {
				auto art_file = AllocatedPath::Build(dvda_path.GetDirectoryName(), art_name);
				try {
					Mutex mutex;
					auto is = InputStream::OpenReady(art_file.c_str(), mutex);
					if (is && is->KnownSize()) {
						std::unique_lock lock{mutex};
						std::vector<std::byte> art_data;
						art_data.resize(is->GetSize());
						is->ReadFull(lock, art_data);
						handler.OnPicture(nullptr, art_data);
						break;
					}
				}
				catch (...) {
				}
			}
		}
	}
}

static bool
init(const ConfigBlock& block) {
	my_av_log_set_callback(mpd_av_log_callback);
	param_no_downmixes = block.GetBlockValue("no_downmixes", true);
	param_no_short_tracks = block.GetBlockValue("no_short_tracks", true);
	param_no_untagged_tracks = block.GetBlockValue("no_untagged_tracks", true);
	auto playable_area = block.GetBlockValue("playable_area", nullptr);
	param_playable_area = CHMODE_BOTH;
	if (playable_area != nullptr) {
		if (strcmp(playable_area, "stereo") == 0) {
			param_playable_area = CHMODE_TWOCH;
		}
		if (strcmp(playable_area, "multichannel") == 0) {
			param_playable_area = CHMODE_MULCH;
		}
	}
	param_tags_path = block.GetBlockValue("tags_path", "");
	param_tags_with_iso = block.GetBlockValue("tags_with_iso", false);
	param_use_stdio = block.GetBlockValue("use_stdio", true);
	return true;
}

static void
finish() noexcept {
	container_update(nullptr);
	my_av_log_set_default_callback();
}

// External function to get channel mode for database creation
enum class ChannelMode {
	STEREO,
	MULTICHANNEL,
	ALL
};
extern "C" __attribute__((weak)) ChannelMode GetChannelMode() noexcept {
	return ChannelMode::ALL; // Default for normal MPD
}

static std::forward_list<DetachedSong>
container_scan(Path path_fs) {
	std::forward_list<DetachedSong> list;
	if (!container_update(path_fs)) {
		return list;
	}
	TagBuilder tag_builder;
	auto tail = list.before_begin();
	auto suffix = path_fs.GetExtension();
	
	// Check our channel mode for database creation
	auto channel_mode = GetChannelMode();
	FmtDebug(dvdaiso_domain, "container_scan: GetChannelMode returned {}",
		(channel_mode == ChannelMode::STEREO ? "STEREO" :
		 channel_mode == ChannelMode::MULTICHANNEL ? "MULTICHANNEL" : "ALL"));
	
	for (auto track_index = 0u; track_index < dvda_reader->get_tracks(); track_index++) {
		if (dvda_reader->select_track(track_index)) {
			auto duration = dvda_reader->get_duration();
			if (param_no_short_tracks && duration < SHORT_TRACK_SEC) {
				continue;
			}
			
			auto channels = dvda_reader->get_channels();
			bool is_multichannel = channels > 2;
			
			// Filter based on channel mode
			bool process_track = false;
			bool process_downmix = false;
			
			// First check database creation channel mode
			if (channel_mode == ChannelMode::STEREO) {
				// Only process stereo tracks or downmixes of multichannel
				if (!is_multichannel) {
					process_track = true;
				} else if (!param_no_downmixes && dvda_reader->can_downmix()) {
					process_downmix = true;
				}
			} else if (channel_mode == ChannelMode::MULTICHANNEL) {
				// Only process multichannel tracks
				if (is_multichannel) {
					process_track = true;
				}
			} else { // ChannelMode::ALL
				// Process everything based on param_playable_area
				switch (param_playable_area) {
				case CHMODE_MULCH:
					if (is_multichannel) {
						process_track = true;
					}
					break;
				case CHMODE_TWOCH:
					if (!is_multichannel) {
						process_track = true;
					}
					if (!param_no_downmixes && dvda_reader->can_downmix()) {
						process_downmix = true;
					}
					break;
				default:
					process_track = true;
					if (!param_no_downmixes && dvda_reader->can_downmix()) {
						process_downmix = true;
					}
					break;
				}
			}
			
			char area;
			char track_name[64];
			if (process_track) {
				AddTagHandler h(tag_builder);
				area = is_multichannel ? 'M' : 'S';
				scan_info(track_index, false, h);
				
				// Add channel indicator to album title in ALL mode
				if (channel_mode == ChannelMode::ALL) {
					auto tag = tag_builder.Commit();
					const char *album = tag.GetValue(TAG_ALBUM);
					if (album != nullptr) {
						TagBuilder modified_builder(std::move(tag));
						std::string new_album(album);
						new_album += is_multichannel ? " (Multichannel)" : " (Stereo)";
						modified_builder.RemoveType(TAG_ALBUM);
						modified_builder.AddItem(TAG_ALBUM, new_album);
						tag = modified_builder.Commit();
					}
					std::sprintf(track_name, DVDA_TRACKXXX_FMT, track_index + 1, area, suffix);
					tail = list.emplace_after(
						tail,
						track_name,
						std::move(tag)
					);
				} else {
					std::sprintf(track_name, DVDA_TRACKXXX_FMT, track_index + 1, area, suffix);
					tail = list.emplace_after(
						tail,
						track_name,
						tag_builder.Commit()
					);
				}
			}
			if (process_downmix) {
				AddTagHandler h(tag_builder);
				area = 'D';
				scan_info(track_index, true, h);
				
				// Add channel indicator to album title in ALL mode for downmixes
				if (channel_mode == ChannelMode::ALL) {
					auto tag = tag_builder.Commit();
					const char *album = tag.GetValue(TAG_ALBUM);
					if (album != nullptr) {
						TagBuilder modified_builder(std::move(tag));
						std::string new_album(album);
						new_album += " (Downmix)";
						modified_builder.RemoveType(TAG_ALBUM);
						modified_builder.AddItem(TAG_ALBUM, new_album);
						tag = modified_builder.Commit();
					}
					std::sprintf(track_name, DVDA_TRACKXXX_FMT, track_index + 1, area, suffix);
					tail = list.emplace_after(
						tail,
						track_name,
						std::move(tag)
					);
				} else {
					std::sprintf(track_name, DVDA_TRACKXXX_FMT, track_index + 1, area, suffix);
					tail = list.emplace_after(
						tail,
						track_name,
						tag_builder.Commit()
					);
				}
			}
		}
		else {
			LogError(dvdaiso_domain, "cannot select track");
		}
	}
	return list;
}

static void
file_decode(DecoderClient &client, Path path_fs) {
	if (!container_update(path_fs.GetDirectoryName())) {
		return;
	}
	unsigned track;
	bool downmix;
	if (!get_subsong(path_fs, track, downmix)) {
		LogError(dvdaiso_domain, "cannot get track number");
		return;
	}

	// initialize reader
	if (!dvda_reader->select_track(track)) {
		LogError(dvdaiso_domain, "cannot select track");
		return;
	}
	if (!dvda_reader->set_downmix(downmix)) {
		LogError(dvdaiso_domain, "cannot downmix track");
		return;
	}
	auto samplerate = dvda_reader->get_samplerate();
	auto channels = dvda_reader->get_downmix() ? 2u : dvda_reader->get_channels();
	std::vector<uint8_t> pcm_data(192000);

	// initialize decoder
	auto audio_format = CheckAudioFormat(samplerate, SampleFormat::S32, channels);
	auto songtime = SongTime::FromS(dvda_reader->get_duration(track));
	client.Ready(audio_format, true, songtime);

	// play
	auto cmd = client.GetCommand();
	for (;;) {
		auto pcm_size = pcm_data.size();
		if (dvda_reader->read_frame(pcm_data.data(), &pcm_size)) {
			if (pcm_size > 0) {
				auto kbit_rate = 24 * channels * samplerate / 1000;
				cmd = client.SubmitAudio(nullptr, std::span{ pcm_data.data(), pcm_size }, kbit_rate);
				if (cmd == DecoderCommand::STOP) {
					break;
				}
				if (cmd == DecoderCommand::SEEK) {
					auto seconds = client.GetSeekTime().ToDoubleS();
					if (dvda_reader->seek(seconds)) {
						client.CommandFinished();
					}
					else {
						client.SeekError();
					}
					cmd = client.GetCommand();
				}
			}
		}
		else {
			break;
		}
	}
}

static bool
scan_file(Path path_fs, TagHandler& handler) noexcept {
	if (!container_update(path_fs.GetDirectoryName())) {
		return false;
	}
	unsigned track_index;
	bool downmix;
	if (!get_subsong(path_fs, track_index, downmix)) {
		LogError(dvdaiso_domain, "cannot get track number");
		return false;
	}
	
	// Check our channel mode for database creation
	auto channel_mode = GetChannelMode();
	
	// Select the track to get channel info
	if (!dvda_reader->select_track(track_index)) {
		LogError(dvdaiso_domain, "cannot select track for scan");
		return false;
	}
	
	auto channels = dvda_reader->get_channels();
	bool is_multichannel = channels > 2;
	
	// Filter based on channel mode
	if (channel_mode == ChannelMode::STEREO) {
		// Skip multichannel tracks unless it's a downmix
		if (is_multichannel && !downmix) {
			return false;
		}
	} else if (channel_mode == ChannelMode::MULTICHANNEL) {
		// Skip stereo tracks and downmixes
		if (!is_multichannel || downmix) {
			return false;
		}
	}
	// For ChannelMode::ALL, scan everything
	
	scan_info(track_index, downmix, handler);
	return true;
}

static const char* const suffixes[] {
	"iso",
	nullptr
};

}

constexpr DecoderPlugin dvdaiso_decoder_plugin =
	DecoderPlugin("dvdaiso", dvdaiso::file_decode, dvdaiso::scan_file)
	.WithInit(dvdaiso::init, dvdaiso::finish)
	.WithContainer(dvdaiso::container_scan)
	.WithSuffixes(dvdaiso::suffixes);
	