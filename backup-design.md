# StreamUP Backup — design

Status: built and tested. Research done 2026-08-05 against a real OBS 32.2.1 portable
install and the OBS 32.2 frontend source; the notes below record what shipped, including
the things only testing revealed.

## The problem

There is no easy way to back up an OBS setup. People are told to "copy your config folder",
which is wrong in three ways: it is enormous, it misses things that live outside that folder,
and what it produces often will not restore cleanly onto another machine.

Measured on the reference install (`D:\Streaming\SOFTWARE\OBS NEW`):

| | Size |
|---|---|
| Whole `config/obs-studio` | 563 MB |
| What actually matters | **4 MB** |

The 559 MB difference is `plugin_config/obs-browser` (Chromium cache: 333 MB `Cache`,
122 MB `Code Cache`, 19 MB WidevineCdm, 15 MB component cache, plus GPU and shader caches),
along with `logs`, `crashes`, `profiler_data` and `updates`. All of it regenerates.

## What gets backed up

| Item | Path | Why |
|---|---|---|
| App config | `global.ini` | Renderer, update branch, `[Locations]` |
| User config | `user.ini` | Dock layout, theme choice, language (OBS 31+) |
| Profiles | `basic/profiles/<name>/` | `basic.ini`, `service.json`, `streamEncoder.json`, `recordEncoder.json` |
| Scene collections | `basic/scenes/*.json` | Scenes, sources, filters, transitions, groups, canvases, projectors, per-collection hotkeys, and the `modules` blob |
| Plugin settings | `plugin_config/*` except `obs-browser` | Per-plugin config for every installed plugin |
| Plugin manager state | `plugin_manager/modules.json` | Which plugins are enabled or disabled |
| Themes | `%APPDATA%\obs-studio\themes` or `<install>/data/obs-studio/themes` | `.obt` / `.ovt`, including the StreamUP theme |
| Inventory (generated) | `streamup-backup.json` | OBS version, platform, portable flag, plugin list with versions, source path map, media manifest |

The `modules` blob inside each scene collection is worth calling out: it is where every
plugin stores per-collection data. The reference collection carries 33 entries including
downstream keyers, transition table, scripts-tool, source-dock and Aitum Vertical. Backing up
scene collections without it silently loses plugin state.

### Excluded, always

`plugin_config/obs-browser/**`, `logs/`, `crashes/`, `profiler_data/`, `updates/`, and every
`*.bak` OBS writes beside its own files.

`obs_profile_cookies` (49 MB) is excluded too, but deserves its own note: it holds live
browser-dock logins. It is both bulky and credential-bearing, so it is not something to sweep
into a backup by default.

`.sentinel` and `safe_mode` are runtime state, not configuration, and restoring them would do
active harm: a restored `.sentinel` makes OBS believe it crashed last run, and a restored
`safe_mode` file makes it start in safe mode with every plugin disabled. Collection works from
an allow-list of known areas rather than sweeping the config root, so anything OBS adds in
future is left out until it is deliberately included.

## Decisions taken

**Credentials are stripped by default.** `service.json` holds the stream key in plain text and
`basic.ini` holds `[Auth]` and `[Twitch]` OAuth tokens. Default behaviour removes both, so a
backup can be shared, uploaded or posted in a support thread safely. An "include credentials"
toggle puts them back for people backing up for themselves, and the resulting file is labelled
as credential-bearing in the UI and in the manifest.

**Media collection is optional.** Default is audit-only: the backup records every referenced
external path and the restore reports which are missing. Ticking "collect media" copies the
referenced files in and rewrites paths on restore. This matters more than it sounds: scene
collections reference external files across `file`, `local_file`, `shader_file_name`, `url` and
`path` keys, and on the reference install **13 of 17 referenced files are already missing on
disk**. Audit-only alone will tell people something they do not currently know.

**Scope is manual plus automatic.** A Backup button that writes a dated archive, a Restore that
shows what is inside before touching anything, and automatic backups with a keep-last-N rule.

**Automatic backups run on OBS exit, at most once a day.** Shutdown is when nothing is being
edited and a pause costs nothing; a timer while OBS runs would fire during a stream. One a day
because closing OBS five times in an evening should not produce five archives. Ten are kept by
default, and they include credentials: they never leave the machine, and a safety net that needs
the stream key re-entering is a poor safety net. Default location is beside the OBS config so it
travels with a portable install, with a folder override in Settings for another drive or a synced
folder.

**A per-file size ceiling of 100 MB.** Found the hard way: a test install had a 2.95 GB Whisper
model under `plugin_config/obs-localvocal/models`, and compressing it on the UI thread hung OBS
until it died. These files are the worst backup candidates going, huge and re-downloaded by the
plugin on demand. Skipped files are listed in the log, the manifest and the completion window, so
the omission is never silent.

## Things the implementation must get right

1. **Resolve `[Locations]`, never assume paths.** `OBSApp.cpp:578` shows scene collections,
   profiles, config and plugin manager settings can each be relocated. The reference install
   points all four at `../../config`. Portable mode ignores the overrides and uses defaults;
   installed mode uses the override when the path exists. Both branches need handling.

2. **Force a save before archiving.** OBS flushes config on exit, so a backup taken mid-session
   captures the last flush, not current state. Call the frontend save first, then archive.

3. **Restore is destructive, so make it reversible.** Restore takes an automatic safety backup
   of the current state first, and refuses to run while streaming or recording.

4. **Warn about plugins before restoring, not after.** The backup carries its plugin inventory.
   On restore, diff it against what is installed and list what is missing or outdated before
   anything is written. We already have the requirements checker and version data for this, and
   it is the part a generic backup tool cannot do.

5. **Restore cannot be applied to a running OBS.** See "How restore works" below.

6. **Archive format.** There was no zip support anywhere in the plugin. ZLIB is available (libobs
   already requires it), so this is a small zip writer and reader over deflate, with ZIP64 for the
   collect-media case where archives can exceed 4 GB. Every archive is reopened and verified after
   writing, and every extraction is CRC-checked: a corrupt backup has to be caught at restore time,
   not discovered.

7. **Collected media cannot be flattened.** `media/<filename>` collides, and overlay packs are full
   of files called `index.html`: the reference install had six. Entries are keyed by a hash of the
   full source path, with the mapping recorded in the manifest so restore never guesses.

8. **The frontend API is gone during shutdown.** `obs_frontend_get_app_config()` and
   `obs_frontend_get_current_profile_path()` both return null inside `obs_shutdown`, so the
   automatic backup cannot resolve anything from scratch. Locations are resolved and cached while
   OBS is alive (primed at `FINISHED_LOADING`) and the shutdown path uses that cache.

## How restore works

Restoring into a running OBS does not work, and fails in a way that looks like success until
the user quits. Two orderings in the frontend settle it.

Startup, `OBSBasic::OBSInit` in `widgets/OBSBasic.cpp`:

1. `InitBasicConfig()` reads the profile `basic.ini` into memory — line 994
2. `loadAppModules()` loads plugins, including us — line 1056
3. `OBSBasic::Load` reads the scene collection from disk — line ~1120

Shutdown, `closeEvent` → `closeWindow()` → `SaveProjectNow()` at line 1893 writes the scene
collection and configs from memory, and only then does `obs_shutdown()` unload modules.

So a scene collection restored mid-session is overwritten from memory on exit, and a restored
`basic.ini` is never read and then overwritten with the old values. Both silently discard the
restore at quit.

### Stage, apply, verify

**Stage** (OBS running). Unpack the archive to a staging folder, checksum every file, diff the
plugin inventory, and show the user exactly what will change. All slow or fallible work happens
here, where cancelling is free. Writes a journal describing the pending restore.

**Apply** (`obs_module_unload`). This runs inside `obs_shutdown()`, after OBS has written its
final state, so nothing clobbers what we write. This is also the only correct window for
profiles, whose next read is the following launch.

**Verify** (`obs_module_load`, next launch). Runs before the scene collection is read. Compares
against the manifest, finishes anything incomplete, then clears the journal.

Edge cases this covers by construction:

- OBS killed rather than closed: the journal survives, the next load applies it.
- Another plugin rewriting its own `plugin_config` during unload after ours: the next-start
  verify re-applies it.
- Crash part-way through applying: checksums plus the journal make re-running idempotent.

A helper executable that waits for OBS to exit and applies files externally is more robust in
principle, but means shipping and signing a binary per platform. Held in reserve.

### Restart

OBS's own relaunch is a global `restart` flag in `obs-main.cpp:81`, set by the frontend's
plugin manager. It is not exposed to plugins, so we cannot cleanly auto-restart. v1 flow ends
with OBS closing and the user reopening it, and the verify step confirms the restore landed.

## Selective restore

Restoring is filtered at STAGING time, not at apply time. Anything the user unticks is never
unpacked, so an unselected area cannot be touched by a later bug in the apply step. The journal
records the selection, so a partial restore is identifiable afterwards.

Scene collections can additionally be picked individually, which is the case people actually hit:
one collection lost, no reason to roll back everything else. Ticking all of them clears the filter
rather than listing every name, which keeps "was this partial?" honest.

Verified end to end: with only scene collections selected and one collection ticked, a deliberately
deleted collection came back byte-identical while a marker left in a plugin settings file survived,
proving the unselected areas were untouched. The apply reported "1 restored, 23 already correct,
0 failed", because the hash check means an unchanged file is never rewritten.

## Portable and installed

Both tested against a live config, back to back on the same build:

| | Portable | Installed |
|---|---|---|
| Files expected vs found | 1100, 0 missing | 71, 0 missing |
| Themes source | `<install>/data/obs-studio/themes` | `%APPDATA%/obs-studio/themes` |
| Plugin data | 123 files, 23 plugins | 61 files, 23 plugins |

Testing installed mode found the bug that mattered most in this feature, recorded as point 9
above. Portable had been passing only because portable ignores the `[Locations]` overrides.

## WebSocket

`CreateBackup` and `GetBackupInfo` are registered on the streamup vendor, documented in
WEBSOCKET_API_DOCUMENTATION.md. There is deliberately no restore request: a restore replaces the
user's setup and has to be applied during shutdown, so it stays a decision made in the UI where
the contents can be reviewed first.

## Restoring into a different OBS version

Tested: a backup taken on OBS 32.2.1 (plugin 2.2.5) restored into a clean portable OBS 32.2.0
running plugin 2.3.0.

The restore dialog read the source correctly across the version gap ("From OBS 32.2.1, portable")
and flagged the 71 plugins the backup used that the clean install does not have, which is exactly
the warning someone moving between machines needs. Staging wrote 1100 files, the apply reported
910 restored, 190 already correct, 0 failed, and the next launch came up on the source machine's
profile and scene collection with all 33 collections, 3 profiles and 24 plugin_config folders in
place.

Hashing every restored file against the archive afterwards gave 1096 of 1100 identical, 0 missing.
The four that differed were all rewritten by OBS or the plugin *after* the apply finished, which
their timestamps confirm: the active profile's `basic.ini`, `plugin_manager/modules.json`
(reconciled against the plugins this install does not have), the plugin's own `configs.json`, and
`restore-result.json`, which the restore writes itself. Nothing the restore put down was wrong.

**The real cross-version limit is not the archive, it is the plugin binary.** OBS refuses to load
a plugin built against a newer libobs than itself: dropping this 32.2-built plugin into OBS 32.1.2
logs `Module 'streamup.dll' compiled with newer libobs 32.2` and the module never loads, so there
is no StreamUP menu to restore from. Restoring *forward* into a newer OBS is fine. Restoring
*backward* into an older OBS needs a plugin build for that OBS, which is an OBS packaging rule
rather than anything the backup format can solve. Worth knowing before telling anyone to use a
backup to downgrade.

## Still to do

Nothing outstanding on backup and restore.
