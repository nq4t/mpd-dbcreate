// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project
// mpd-dbcreate "Hot diggity daffodil!"
// Jay Moore - dewdude@pickmy.org

#include "config.h"
#include "Instance.hxx"
#include "Main.hxx"
#include "config/Data.hxx"
#include "config/Param.hxx"
#include "config/Block.hxx"
#include "decoder/DecoderList.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "fs/AllocatedPath.hxx"
#include "lib/icu/Init.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "Log.hxx"
#include "LogInit.hxx"
#include "tag/Config.hxx"
#include "db/Configured.hxx"
#include "db/plugins/simple/SimpleDatabasePlugin.hxx"
#include "db/update/Service.hxx"
#include "storage/Configured.hxx"
#include "storage/CompositeStorage.hxx"
#include "input/Init.hxx"
#include "util/UriExtract.hxx"

#ifdef ENABLE_ARCHIVE
#include "archive/ArchiveList.hxx"
#endif

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "event/CoarseTimerEvent.hxx"
#include "util/BindMethod.hxx"

// Channel mode shared with FilteredSongUpdate
enum class ChannelMode {
	STEREO,
	MULTICHANNEL,
	ALL
};

// Helper class to check update completion with a timer
class UpdateChecker {
	Instance &instance;
	CoarseTimerEvent timer;
	bool verbose;
	int progress_counter = 0;
	
public:
	UpdateChecker(Instance &_instance, bool _verbose)
		: instance(_instance),
		  timer(instance.event_loop, BIND_THIS_METHOD(OnTimer)),
		  verbose(_verbose) {}
	
	void Start() {
		timer.Schedule(std::chrono::milliseconds(100));
	}
	
private:
	void OnTimer() noexcept {
		if (instance.update->GetId() == 0) {
			// Update complete, break the event loop
			instance.event_loop.Break();
		} else {
			// Not done yet, check again in 100ms
			timer.Schedule(std::chrono::milliseconds(100));
			
			// Show progress every 10 seconds (100 checks)
			if (verbose && ++progress_counter >= 100) {
				progress_counter = 0;
				std::cerr << "." << std::flush;
			}
		}
	}
};

static ChannelMode channel_mode = ChannelMode::ALL;
static std::string music_directory;
static AllocatedPath database_path = nullptr;
static bool verbose = false;
static bool update_mode = false;

// Global instance pointer required by other MPD components  
// This must be defined here as we're not linking with Main.cxx
Instance *global_instance = nullptr;

extern "C" __attribute__((visibility("default"))) ChannelMode GetChannelMode() noexcept {
	return channel_mode;
}

static void PrintUsage() {
	std::cout << "mpd-dbcreate | Jay's MPD DB Creator - Hot Diggity Daffodil!\n"
		  << "Usage: mpd-dbcreate --music-dir /path/to/scan --database /path/to/mpd.db [options]\n\n"
		  << "Options:\n"
		  << "  --music-dir <path>   Music directory\n"
		  << "  --database <path>    Database file\n"
		  << "  --update             Update existing database (incremental scan)\n"
		  << "  --stereo             Stereo only\n"
		  << "  --multichannel       Multichannel only\n"
		  << "  --all                All (default)\n"
		  << "  --verbose            Verbose output\n"
		  << "  --help               Show help\n";
}

static void ParseArgs(int argc, char *argv[]) {
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "--help") {
			PrintUsage();
			exit(0);
		} else if (arg == "--update") {
			update_mode = true;
		} else if (arg == "--stereo") {
			channel_mode = ChannelMode::STEREO;
		} else if (arg == "--multichannel") {
			channel_mode = ChannelMode::MULTICHANNEL;
		} else if (arg == "--all") {
			channel_mode = ChannelMode::ALL;
		} else if (arg == "--verbose") {
			verbose = true;
		} else if (arg == "--music-dir") {
			if (++i >= argc)
				throw std::runtime_error("--music-dir needs arg");
			music_directory = argv[i];
		} else if (arg == "--database") {
			if (++i >= argc)
				throw std::runtime_error("--database needs arg");
			database_path = AllocatedPath::FromUTF8Throw(argv[i]);
		} else {
			throw FmtRuntimeError("Unknown: {}", arg);
		}
	}
	if (music_directory.empty() || database_path.IsNull())
		throw std::runtime_error("--music-dir and --database required");
}

int main(int argc, char *argv[]) {
	try {
		ParseArgs(argc, argv);
		
		// Initialize
		const ScopeIcuInit icu_init;
		log_early_init(verbose);
		// Steal logging back from systemd for verbose
		if (!verbose) {
			setup_log_output();
		}
		
		// Config
		ConfigData config;
		config.AddParam(ConfigOption::MUSIC_DIR,
				ConfigParam(music_directory.c_str()));
		
		ConfigBlock db_block;
		db_block.AddBlockParam("plugin", "simple");
		db_block.AddBlockParam("path", database_path.ToUTF8().c_str());
		config.AddBlock(ConfigBlockOption::DATABASE, std::move(db_block));
		
		// Initialize subsystems
		TagLoadConfig(config);
		decoder_plugin_init_all(config);
		const ScopePlaylistPluginsInit playlist_init(config);
#ifdef ENABLE_ARCHIVE
		const ScopeArchivePluginsInit archive_init;
#endif
		
		// Create Instance - this contains event loop
		Instance instance;
		global_instance = &instance;
		instance.io_thread.Start();
		instance.rtio_thread.Start();
		
		// Initialize input plugins with IO thread event loop
		const ScopeInputPluginsInit input_init(config,
							instance.io_thread.GetEventLoop());
		
		// Create database
		instance.database = CreateConfiguredDatabase(config,
							      instance.event_loop,
							      instance.io_thread.GetEventLoop(),
							      instance);
		
		auto *simple_db = dynamic_cast<SimpleDatabase *>(instance.database.get());
		if (!simple_db)
			throw std::runtime_error("Not simple database");
		
		instance.database->Open();
		
		// Create storage
		auto configured_storage = CreateConfiguredStorage(config,
							    instance.io_thread.GetEventLoop());
		
		if (!configured_storage) {
			throw std::runtime_error("Failed to create storage for music directory");
		}
		
		// Create CompositeStorage and mount the configured storage
		auto *composite = new CompositeStorage();
		instance.storage = composite;
		
		if (configured_storage) {
			composite->Mount("", std::move(configured_storage));
		}
		
		if (verbose) {
			std::cerr << "Music directory: " << music_directory << "\n";
			std::cerr << "Database path: " << database_path.ToUTF8() << "\n";
			std::cerr << "Mode: " << (update_mode ? "UPDATE (incremental)" : "CREATE (full scan)") << "\n";
			std::cerr << "Channel Mode: ";
			if (channel_mode == ChannelMode::STEREO) {
				std::cerr << "STEREO (filtering out multichannel)\n";
			} else if (channel_mode == ChannelMode::MULTICHANNEL) {
				std::cerr << "MULTICHANNEL (filtering out stereo)\n";
			} else {
				std::cerr << "ALL (no filtering)\n";
			}
			std::cerr << (update_mode ? "Updating" : "Scanning");
			std::cerr.flush();
		}
		
		// Create update service
		instance.update = new UpdateService(config,
						     instance.event_loop,
						     *simple_db,
						     *composite,
						     instance);
		
		// Start scan - use 'false' for discard parameter when in update mode
		// to perform incremental update, 'true' for full rescan
		instance.update->Enqueue("", !update_mode);
		
		// Create update checker to monitor completion
		UpdateChecker checker(instance, verbose);
		checker.Start();
		
		// Run the event loop - it will process update events and our timer
		// The loop will break when the timer detects update completion
		instance.event_loop.Run();
		
		if (verbose)
			std::cerr << "\n";
		
		// Save
		simple_db->Save();
		
		// Clean up update service first while event loops are still running
		delete instance.update;
		instance.update = nullptr;
		
		// Close database
		instance.database->Close();
		instance.database.reset();
		
		// Clean up storage
		delete instance.storage;
		instance.storage = nullptr;
		
		// Now stop threads after cleanup
		instance.rtio_thread.Stop();
		instance.io_thread.Stop();
		
		if (verbose)
			std::cerr << "Done!\n";
		
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << "\n";
		return 1;
	}
}
