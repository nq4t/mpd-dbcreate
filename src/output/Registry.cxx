// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "Registry.hxx"
#include "OutputPlugin.hxx"
#include "output/Features.h"
#include "plugins/NullOutputPlugin.hxx"
#include "util/StringAPI.hxx"

// Only include the NullOutputPlugin for mpd-dbcreate
// since we don't need actual audio output for database creation
constinit const AudioOutputPlugin *const audio_output_plugins[] = {
	&null_output_plugin,
	nullptr
};

const AudioOutputPlugin *
GetAudioOutputPluginByName(const char *name) noexcept
{
	for (const auto &plugin : GetAllAudioOutputPlugins()) {
		if (StringIsEqual(plugin.name, name))
			return &plugin;
	}

	return nullptr;
}
