# mpd-dbcreate | Jay's MPD Database Creator - "Hot Diggity Daffodil!"

It's mpd without the daemon and playback functionality. Creates an mpd database based on "better" rules regarding CUE sheets, multi-channel files, and SACD content.


## Overview

mpd-dbcreate is a command-line tool that creates MPD-compatible database files without running the full MPD daemon. This is particularly useful for:

- Creating databases for large music collections without running MPD - *You got hit by the systemd timeout first launch, didn't you?*
- Building databases on systems where MPD isn't installed - *Run it directly on the RAID...just symlink or awk your paths*
- Filtering collections by channel configuration (stereo/multichannel) - *Even that niche format.*
- Supports updating an existing database. - *But only with this utility and not from within mpd.*
- Slightly smaller than the mpd binary! - *We at Monolithic Binary love you; as much as you love us.*


## Features

- Scans music directories and creates MPD-compatible database files - *At least the way I want them made.*
- Supports all audio formats that MPD supports - *SACD playback support requires Maxim's fork of mpd. Milage may vary. Hi Maxim!*
- Handles large collections (tested with multi-TB libraries) - *and I did it over the network in about an hour! Like reasonable frames with your perscription!*
- Looks for every reason to not use a .CUE sheet. - *There are valid reasons and that's why I wrote this!*
- ~~Network filesystem support (NFS, WebDAV)~~ - *URI's are supported, but broken? Please report. SMB is disabled.*
- Channel filtering options (stereo-only, multichannel-only, or all) - *Because it's not like I listen to the Britney Spears in 5.1 on a regular basis.*


## "Better" Media Handling

I created this for the basic reason that mpd was not generating databases that worked for me, it angered me, and I thought I could do better:

### CUE Sheet Handling

You don't always need a CUE sheet for playback. If your media files are already split up, as they usually already are, then a CUE sheet doesn't give you any advantage on playback. In fact, this is a disadvantage. If mpd sees a cue sheet; it will only index that cue sheet. This is fine except when it doesn't properly parse the sheet and you wind up with most of an album unplayable. 

The solution is to just not use CUE sheets unless necessary. It compares the number of tracks in the CUE sheet to the number of media files in the folder and makes that decision based on the following rules:

  - If the cue sheet track count matches the media files, ignore the cue.
  - If the cue sheet track count is one more than media files, assume multi-session disc and ignore the cue.
  - Anything else, the CUE is the rule.


### Multi-Channel Filtering

If you don't have multi-channel playback; you don't need the multi-channel files in the database! So let's not even put them in! This was primarily developed for the SACD side as many of those contain both stereo and 5.1; however you may still have 5.1/multichannel releases floating around. I'm stereo-only playback and I have a ton of 5.1; and not all of them on SACD! 

You can specify if you want to keep just stereo, just multichannel, or everything! Be warned, *this is literal!* A multichannel database *will not* contain stereo files. If you support both types of playback, then the -all feature will work for you.

### SACD Cleanup

SACD's are presented to mpd in a non-standard way; so we had to go down in to that code and implement some changes. The primary one is that it follows stereo/multichannel rules like for the rest of the media. So a stereo only database will not have 5.1 SACD stuff mixed in. 

We also cleaned up the tags! The plugin added technical information to the album and track title tags. It's ugly. Primarily putting the channel configuration and track number in the track title is a bit much. So...we clean everything up! Track titles are sanitized/cleaned to be just the title, same with the album titles. After all...if you're only running stereo content, then it's all stereo; you're not at a risk of picking the multichannel version. Likewise, if you've got a multi-channel database; you won't be getting any stereo content.

And if you kept everything, you're still covered! We append (Stereo) or (Multichannel) to the album title. Clean, more human-readable. But we only do this on SACD. You'll have to tag your non-SACD multichannel releases yourself. (For now. It's literally not that difficult to have it mark non-SACD multi-channel; I just haven't done that yet.)

## Building

I've built this on CachyOS. If you have OSX or Windows good luck. It probably won't work. You guys aren't using mpd anyway; what am I worried about?

### Prerequisites

- Meson build system (>= 1.2)
- Ninja
- C++23 compatible compiler
- Required libraries (same as MPD):
  - libfmt
  - libicu
  - SQLite3 (optional, for sticker database support)
  - Various audio format libraries (FLAC, Vorbis, etc.)
  
*If you can build mpd, you can build this.*

### Build Instructions

```bash
meson setup builddir
ninja -C builddir
```

The executable will be created at `builddir/mpd-dbcreate`. You can customize for just formats you need by editing meson_options.txt

## Installation

There is no installation. mpd-dbcreate is a static binary. You may put it in your $PATH if you wish.

## Usage

```bash
mpd-dbcreate --music-dir <path> --database <path> [options]

Options:
  --update             Updates an existing database.
  --music-dir <path>   Music directory to scan (required)
  --database <path>    Output database file path (required)
  --stereo             Include only stereo files
  --multichannel       Include only multichannel files  
  --all                Include all files (default)
  --verbose            Output messages to console
  --help               Show help message
```

I have found it's best to feed full paths for everything. It should be obvious if it doesn't like your music-dir argument. Not liking --database argument has shown up as a simple database plugin error.

### Examples

Create a database of only stereo content:
```bash
mpd-dbcreate --music-dir /path/to/media --database /path/to/file.db --stereo
```

Scan a database of only multichannel content:
```bash
mpd-dbcreate --music-dir /path/to/media --database /path/to/file.db --multichannel
```

Create a database of all content:
```bash
mpd-dbcreate --music-dir /path/to/media --database /path/to/file.db --all
```

Update an existing database:

```bash
mpd-dbcreate --music-dir /path/to/media --database /path/to/file.db (--stereo|--multichannel|--all) --update
```

## Changes
```
28-AUG-2025 - Initial hacking of database tool from mpd-sacd itself. Multichannel, CUE, SACD logic updates.
29-AUG-2025 - Removed systemd, output, and daemonization features where possible. Restored verbose output from systemd hijack. Got --update working.
```

## License

GPL-2.0-or-later (inherited from MPD). Please see the `COPYING` or `LICENSE` files.

## Credits

Based on Maxim's fork of mpd with SACD and DVD-A plguins: https://sourceforge.net/projects/mpd.sacddecoder.p/ - Thanks Maxim!

Original MPD project: https://www.musicpd.org/
