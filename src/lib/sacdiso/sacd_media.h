/*
* MPD SACD Decoder plugin
* Copyright (c) 2014-2025 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with FFmpeg; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#pragma once

#include "config.h"
#include <stdint.h>
#include <stddef.h>
#include "input/InputStream.hxx"
#include "Log.hxx"

class sacd_media_t {
public:
	sacd_media_t() {}
	virtual ~sacd_media_t() {}
	virtual bool    open(const char* path) = 0;
	virtual bool    close() = 0;
	virtual bool    seek(int64_t position) = 0;
	virtual int64_t get_position() = 0;
	virtual int64_t get_size() = 0;
	virtual size_t  read(void* data, size_t size) = 0;
	virtual int64_t skip(int64_t bytes) = 0;
};

class sacd_media_file_t : public sacd_media_t {
	int fd;
public:
	sacd_media_file_t();
	~sacd_media_file_t();
	bool    open(const char* path) override;
	bool    close() override;
	bool    seek(int64_t position) override;
	int64_t get_position() override;
	int64_t get_size() override;
	size_t  read(void* data, size_t size) override;
	int64_t skip(int64_t bytes) override;
};

class sacd_media_stream_t : public sacd_media_t {
	Mutex mutex;
	InputStreamPtr is;
public:
	sacd_media_stream_t();
	~sacd_media_stream_t();
	bool    open(const char* path) override;
	bool    close() override;
	bool    seek(int64_t position) override;
	int64_t get_position() override;
	int64_t get_size() override;
	size_t  read(void* data, size_t size) override;
	int64_t skip(int64_t bytes) override;
};
