/*
* MPD DVD-Audio Decoder plugin
* Copyright (c) 2014-2025 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
*
* DVD-Audio Decoder is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* DVD-Audio Decoder is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with FFmpeg; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#pragma once

#include "audio_track.h"
#include "audio_stream.h"
#include "stream_buffer.h"
#include "dvda_reader.h"
#include "dvda_filesystem.h"
#include "dvda_zone.h"

#include <memory>

class dvda_disc_t : public dvda_reader_t {
private:
	dvda_media_t*                      dvda_media;
	std::unique_ptr<dvda_filesystem_t> dvda_filesystem;
	std::unique_ptr<dvda_zone_t>       dvda_zone;
	track_list_t                       track_list;

	stream_buffer_t<uint8_t, int>   track_stream;
	std::vector<uint8_t>            ps1_data;
	std::unique_ptr<audio_stream_t> audio_stream;
	audio_track_t                   audio_track;

	uint64_t      stream_size;
	double        stream_duration;
	sub_header_t  stream_ps1_info;
	uint32_t      stream_block_current;
	bool          stream_downmix;
	bool          stream_needs_reinit;
	bool          major_sync_0;
	unsigned      stream_channel_map;
	int           stream_channels;
	int           stream_bits;
	int           stream_samplerate;

	int           sel_titleset_index;
	int           sel_track_index;
	size_t        sel_track_offset;
	uint32_t      sel_track_length_lsn;
public:
	dvda_disc_t();
	~dvda_disc_t();
	dvda_filesystem_t* get_filesystem();
	audio_track_t get_track(uint32_t track_index);
	uint32_t get_tracks() override;
	uint32_t get_channels() override;
	uint32_t get_loudspeaker_config() override;
	uint32_t get_samplerate() override;
	double get_duration() override;
	double get_duration(uint32_t track_index) override;
	bool can_downmix() override;
	void get_info(uint32_t track_index, bool downmix, TagHandler& handler) override;
	uint32_t get_track_length_lsn();
	bool open(dvda_media_t* dvda_media) override;
	bool close() override;
	bool select_track(uint32_t track_index, size_t offset = 0) override;
	bool get_downmix() override;
	bool set_downmix(bool downmix) override;
	bool read_frame(uint8_t* frame_data, size_t* frame_size) override;
	bool seek(double seconds) override;
private:
	bool create_audio_stream(sub_header_t& p_ps1_info, uint8_t* p_buf, int p_buf_size, bool p_downmix);
	void stream_buffer_read();
};
