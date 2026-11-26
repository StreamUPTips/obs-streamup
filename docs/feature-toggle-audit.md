# Feature toggle audit

What each switchable part of StreamUP does when it is on and when it is off, where an off part
still costs something, and what that costs OBS. Written for the Features page work
(`feature/features-page`). Phase 1 is the audit as found; Phase 2 (below) records what changed.

Line references are to `feature/features-page` before the Phase 2 changes.

## Summary

- **Six things run with no switch at all:** the source-tree filter (app-wide, every paint of the
  Sources dock), the "Switch to Scene" menu filter (app-wide, every event), the studio-mode
  callback, the selection watcher (walks every scene on each scene change, with or without a
  websocket client), Overlay Controls, and the start-up plugin check (network call plus four
  full reads of the OBS log).
- **Four switches do less than they say:**
  - Toolbar off at runtime only hides it, and its 1 s status timer keeps running.
  - Hotkeys off at runtime leaves a save callback with stale IDs, which can wipe the saved
    bindings.
  - `websocket_api` is saved but never read: the 71 requests are always registered.
  - Scene Organiser, StreamUP Dock, Mixer Dock and Adjustment Layer are restart-only, so they
    keep everything running until a restart.
- **Theme tweaks** (mixer, studio mode, theme, vertical canvas, multistream, source tree) gate on
  the StreamUP theme being active, not on a StreamUP switch. Once installed, most of their event
  filters stay for the session even when the theme or tweak is switched off. Only Multistream
  cleans up after itself.
- **Two bugs found on the way:** the vertical-canvas button stylesheets grow on every refresh,
  and the studio-mode slider filter is added again on every Studio Mode toggle.

## Per part

Legend: **ON** what it does when switched on; **OFF** what still happens when switched off;
⚠ marks work done by an off part.

### Scene Organiser (restart)
- **ON:**
  - Normal dock (`obs_frontend_add_dock_by_id`) and a Vertical dock for Aitum's canvas.
  - Collection save callback that re-reads the per-canvas JSON on every save.
  - Per dock: a frontend event callback and a global `source_rename` handler.
  - Single-shot timers (save, batch, initial load twice).
  - Vertical dock: four canvas signals plus a **repeating 1 s poll** for its lifetime.
- **OFF at start-up:** nothing is created.
- **OFF:**
  - ⚠ Switched off at runtime, it all keeps running until restart.
  - ⚠ `CreateVerticalSceneOrganiserDock` reads the current setting, not the one applied at
    start-up. After a runtime flip, a canvas added later can get a Vertical dock with no Normal
    dock and no collection data.
- **Always:** `OnCanvasChanged` is registered with no gate (it only calls the gated create).

### StreamUP Dock (restart)
- **ON:**
  - The dock and a frontend event callback.
  - Six signals on the current scene, re-hooked on each scene change.
  - Walks every scene to update its lock icons, while the dock is closed too.
- **OFF at start-up:** nothing. ⚠ Switched off at runtime, it runs until restart.

### Mixer Dock (restart)
- **ON:**
  - The dock, a save callback and a frontend event callback.
  - Main-window signals, global `source_create`/`source_remove`, and an `item_visible` handler.
  - Per strip: an `obs_fader` and an `obs_volmeter` on the audio thread, eight source signals and
    two widget filters.
  - A shared **repeating 16 ms meter timer** on `qApp`, never stopped once a meter exists. It
    keeps ticking with the dock closed; each meter returns early when hidden.
  - A filter on its own floating frame (placement).
- **OFF at start-up:** nothing. ⚠ Switched off at runtime, it runs until restart.

### MultiDock (live)
- **ON:** a frontend callback, its saved docks read from disk and added, retry single-shots
  after loading, and filters on its own widgets.
- **OFF:** torn down properly (`SetGlobalEnabled(false)` saves, removes the docks and deletes the
  manager). Clean.

### Toolbar (live, but see ⚠)
- **ON:**
  - `addToolBar` and a frontend event callback.
  - A 50 ms batch timer.
  - Status items share a monitor with a **repeating 1 s timer** (CPU, fps, frames, bytes).
- **OFF at start-up:** nothing created.
- **OFF:**
  - ⚠ Switched off at runtime, it is only hidden: the callback and the 1 s timer keep running.
  - ⚠ Off at start-up and switched on at runtime, nothing appears until restart.

### Hotkeys (live, but see ⚠)
- **ON:** 19 frontend hotkeys and a collection save callback.
- **OFF at start-up:** nothing.
- **OFF:**
  - ⚠ Switched off at runtime, the hotkeys are unregistered but their IDs and the save callback
    stay. The next save can drop every StreamUP binding from the collection.
  - ⚠ Off at start-up then switched on, there's no save callback, so new bindings aren't saved.

### Source Menu Scene Switch (live)
- **ON:** adds "Switch to Scene" to the Sources docks' right-click menu.
- **OFF:**
  - ⚠ The app-wide event filter is installed regardless (`streamup.cpp:1384`) and inspects every
    event in OBS.
  - ⚠ The setting is only read after a right-click.

### Backup (live)
- **ON:** the daily automatic backup on the start-up worker thread (zips the config), the
  menu and the settings page.
- **OFF:**
  - ⚠ `ResolveLocations` still runs at every launch (about eight directory checks and eight log
    lines).
  - ⚠ The websocket `CreateBackup`/`GetBackupInfo` requests still work.
  - Restore journal checks at load and unload stay; they are needed and cheap.

### Adjustment Layer (restart; a source type)
- **ON:** registers `streamup_adjustment_layer`.
  - No instances: free.
  - Each instance: two texrenders and an effect compile at create.
  - Showing: walks every scene each frame.
- **OFF:** not registered. ⚠ Existing instances in a collection then fail to load and stop
  showing, and nothing warned about it.

### WebSocket API
- **ON:** one vendor (`streamup`) with 71 requests: 52 current plus 19 deprecated aliases.
- **OFF:**
  - ⚠ The `websocket_api` switch is dead: the requests are registered and answered regardless.
  - ⚠ No request checks its feature's switch (Backup requests answer with Backup off).

### Theme tweaks (mixer, studio mode, theme, vertical canvas, multistream, source tree)
Gate: `ThemeEnhancements::IsTweakActive`, which means the StreamUP theme is in use and that
tweak's `theme_*` setting is on. The `mixer_enhancements`, `studio_mode_enhancements` and
`theme_enhancements` module keys are saved but no longer read.
- **Mixer:**
  - Filters on the mixer dock and every strip; 50 ms single-shots per layout event, not coalesced.
  - ⚠ After the theme or tweak goes off, the filters stay and the per-button filters keep
    re-tinting on paint.
- **Studio mode:**
  - ⚠ The event callback is always registered.
  - ⚠ Each Studio Mode enable scans every slider, logs one line each, and adds **another** slider
    filter, so they pile up. The filter checks the tweak on every paint.
- **Theme:**
  - ⚠ `ApplyDockSpacing` always runs.
  - With the StreamUP theme: an app-wide `AppWidgetFilter` on `qApp`, never removed, checked on
    every Show in OBS, plus colour-preview and status-bar filters.
  - Layout changes aren't reverted when switched off.
- **Vertical canvas:**
  - A dock watcher on Aitum's dock and every child.
  - ⚠ It keeps refreshing (re-styling buttons, reading config) after the tweak goes off.
  - ⚠ Bug: the button stylesheets grow by one rule on every refresh.
- **Multistream:** an app-wide dialog filter and a dock watcher. Switching it off cleans
  everything up. Clean.
- **Source tree:**
  - ⚠ An app-wide `SourceTreeFilter` is installed whatever the theme or setting.
  - ⚠ On every paint of a Sources dock it re-sends the paint and walks every row, even when off.

### No switch at all
- **Selection watcher** (`streamup.cpp:504-558`):
  - Four signals on the current scene.
  - ⚠ On every scene change and every lock or visibility toggle it walks every scene to emit
    `SourceStateChanged`, with or without a websocket client.
- **Overlay Controls / Products:** a notifier, a save callback and an event callback. Core.
- **Start-up check** (worker thread):
  - A blocking curl GET to `api.streamup.tips/plugins`.
  - Four full reads of the newest OBS log with per-plugin regexes.
  - Theme file probes.
  - It always writes `%LOCALAPPDATA%/StreamUP-OutdatedPluginsList.txt`.
  - Core.
- **Menu:** rebuilt on every open (cheap).

## Measurements

### Method (repeatable)

`measure.ps1` (kept in the PR notes; any copy works) does this for each run:
1. Starts the dev build portable, `--disable-updater --disable-shutdown-check --collection <name>`.
   The name is the collection's saved name, not its file name.
2. Reads **start-up time** from OBS's own log: first line to `==== Startup complete`, which OBS
   writes once the main window is shown.
3. Waits 20 s to settle.
4. Samples **idle CPU** as the change in the process's total processor time over 60 s, as a
   percentage of one core.
5. Reads **memory** (private bytes and working set).
6. Closes OBS normally.

Notes:
- **Collections.**
  - "Main Stream" is Andi's real collection, saved in the dev build as `Untitled` (174 sources,
    46 scenes). Some of its third-party plugins aren't installed there, which is the same for
    every run.
  - "Test Blank" (2 sources) shows StreamUP's own cost without the collection's rendering on top.
- **Two runs to compare.**
  - *Before* is the build at `c592159`, the start of this branch.
  - *After* is this branch.
  - The same DLL swap and the same `configs.json` were used for both.
- **"All OFF"** means every `modules` key false and every `theme_*` tweak false. The dev build
  runs the StreamUP theme (Silverlink), so the tweaks were really active in the ON runs.
- **"No StreamUP"** is the same OBS with `streamup.dll` moved out: the floor.
- **Idle CPU is mostly OBS rendering** the preview and outputs. On Main Stream it moves by about
  five points between identical runs, so only large differences mean anything there.
- **Correction.** A first pass of these numbers (sent in the Phase 1 summary) loaded an empty
  collection by mistake, because `--collection "Main Stream"` doesn't match the file's saved
  name. Those runs are discarded; everything below is re-measured.

### Main Stream (174 sources)

Medians of three runs:

| Build, set | Start-up | Idle CPU (% of one core) | Private | Working set |
|---|---|---|---|---|
| Before, all ON | 2958 ms | 66.5 | 2176 MB | 1395 MB |
| Before, all OFF | 2790 ms | 67.2 | 2166 MB | 1379 MB |
| After, all ON | 2882 ms | 68.6 | 2177 MB | 1393 MB |
| After, all OFF | 2786 ms | 70.9 | 2162 MB | 1378 MB |

Raw runs (start-up ms / CPU % / private MB / working set MB):
- Before ON: 2966/65.9/2176/1399, 2958/66.5/2174/1391, 2857/73.9/2179/1395
- Before OFF: 2792/67.2/2164/1374, 2790/65.6/2166/1380, 2768/70.9/2168/1379
- After ON: 3044/68.4/2180/1395, 2861/68.6/2177/1393, 2882/68.6/2174/1391
- After OFF: 2734/71.9/2162/1376, 2816/65.1/2170/1384, 2786/70.9/2160/1378

Reading it:
- **Start-up:** everything off is about 100 to 170 ms faster.
- **Memory:** everything off saves about 10 to 15 MB of private memory and 15 MB of working set.
- **Idle CPU:** on a collection this heavy, StreamUP's idle CPU is lost in the noise (65 to 74%
  in identical runs). The blank collection below shows it.

### Test Blank (2 sources)

Medians of three runs:

| Build, set | Start-up | Idle CPU | Private | Working set |
|---|---|---|---|---|
| No StreamUP (floor) | 2792 ms | 11.2 | 704.6 MB | 495.2 MB |
| Before, all ON | 2895 ms | 8.0 | 714.0 MB | 505.6 MB |
| Before, all OFF | 2829 ms | 8.0 | 707.9 MB | 500.1 MB |
| After, all ON | 2895 ms | 9.0 | 713.2 MB | 507.0 MB |
| After, all OFF | see note | 12.1 | 707.0 MB | 499.0 MB |

Notes on these numbers:
- **The After, all OFF start-up has no reliable figure.** The machine slowed down from that set
  onwards: every run took about 4 s, whatever the settings. A re-check with everything ON at the
  same time also took 4.5 s. The extra time is spent before StreamUP loads (inside another
  plugin's load), so it is the machine, not this change.
- **Idle CPU here is 8 to 14% in every set, with or without StreamUP.** It is OBS's own rendering
  plus whatever else the machine was doing.

Reading it:
- **Everything on:** StreamUP adds about 9 to 10 MB of private memory and 11 MB of working set to
  OBS, and about 100 ms of start-up.
- **Everything off:** that drops to about 3 MB (the code itself), with no measurable start-up or
  idle CPU cost. That is the floor for a loaded plugin.
- **The Phase 2 gain doesn't show up as a number.** The work removed is event-driven: app-wide
  filters on every event, scene walks on every scene change, and paint work in the Sources dock.
  It costs while you use OBS, not while it idles. The audit tables above list what no longer runs.

## Phase 2: what changed

Every part on the Features page now does nothing while it is off. Parts marked *live* switch
straight away; *restart* parts say so on their card ("Turns off after an OBS restart").

| Part | Switch | Off now means |
|---|---|---|
| Scene Organiser | restart | Not created. The Vertical dock follows what the session started with, not a later flip. |
| StreamUP Dock | restart | Not created. |
| Mixer Dock | restart | Not created. When on, the 16 ms meter timer only runs while a meter is on screen. |
| Multi-Dock | live | Torn down (unchanged; it was already clean). |
| Toolbar | live | Removed from the window and deleted, with its callback and 1 s timer. On builds it live. |
| Hotkeys | live | Unregistered. The bindings are parked and written back on every save, so none are lost. They return when the hotkeys do. |
| Switch to Nested Scene | live | Its app-wide event filter comes off. |
| Backup | live | No automatic backup, no location scan at start-up (unless a restore is queued), no menu. Its websocket requests answer "switched off". |
| Adjustment Layer | restart | Not registered. Switching off first counts the scenes using it and warns. |
| WebSocket API | live | Every request answers `{success: false, error: "… is switched off …", feature: id}`. The selection and source-state events stop, along with their scene walks. |
| Theme tweaks | live | Each comes off fully when switched off or when the theme changes. Details below. |

How each theme tweak comes off:
- **Studio mode:** no event callback; the markers are removed. The filter now goes on once per
  slider, and the per-slider log lines are gone.
- **Source folders and guides:** the folder layout is taken back off and the app-wide filter is
  removed. It is only installed while Source Folders is active.
- **Mixer layout:** the watchers come off the mixer and every channel. The 50 ms refresh is
  coalesced. The layout settles back on the next OBS start.
- **Vertical canvas:** the watchers are removed and the row is unwrapped. The stylesheet growth
  bug is fixed.
- **Theme (window polish, stats):** the app-wide filter is removed away from the StreamUP theme.

Still running on purpose (Core):
- The start-up plugin check and the update notice.
- Overlay Controls and My Products.
- The restore journal checks.
- The menu itself.

The StreamUP menu lists Backup, Quick Actions, Multi-Dock and WebSocket Commands (under Help) only while their part
is on. Settings is always there, and opens on Tools and tweaks.

### Checked by hand (dev OBS, Main Stream)

- **Toolbar:** off removes it from the window; on builds it again, live.
- **Backup off:** `GetBackupInfo` and `CreateBackup` answer
  `{"success": false, "error": "Backup is switched off in StreamUP Settings > Tools and tweaks", "feature": "backup"}`.
- **WebSocket API off:** every request answers the same way with `"feature": "websocket"`.
- **Hotkeys off at start-up:** a binding in the collection survives a save. With hotkeys back
  on, it loads and survives a save again.
- **Adjustment Layer:**
  - Switching it off warns "Used in 3 scenes…". Those are the three `01 • CUT` scenes holding
    `[ADJ] Blur`.
  - Starting OBS with it off keeps the `[ADJ] Blur` source and its settings in the saved
    collection, so nothing is lost.
- **Restart parts** show "Turns off after an OBS restart" as soon as they are flipped.

## Tools and tweaks (was the Features page)

Replaces the first-run module wizard and Settings > Plugins. Since 3.0 it is the first tab of
StreamUP Settings (it keeps the old Plugins page slot, `SettingsTabTools`, so tab indices are
unchanged). The menu has no Features entry any more.

- **Where it opens:**
  - StreamUP > Settings (its first tab), and every "Open Tools and tweaks" link.
  - Welcome's "Pick your features" and What's new's "See every feature", which close the
    start-up window and open Settings on this tab.
  - On first run (no version stamp).
  - After an update that adds a part the user has not seen, with "New" on those cards. The stamp
    is `features_version_seen`; an upgrader from before the page falls back to
    `wizard_version_shown`.
- **Groups:** Products, Docks & Layout, Mixer & Audio, Sources, Hotkeys & Control, Safety.
- **Core cards** (no switch): Install a Product, My Products, Plugin Checker.
- **Demos:**
  - Each card looks for `data/demos/<id>.webp` or `.gif` (animated), then `<id>.png` (still).
  - OBS ships without Qt Multimedia, so WebM can't play. Animated WebP plays through Qt's image
    plugins, which OBS does ship.
  - Animations only run while their card is on screen.

### Demo clips to make

640x360, animated WebP, 4 to 8 s loop, ideally under 600 KB each. The ones marked *still* have a
placeholder PNG from dev OBS for now.

| id | What to show |
|---|---|
| install-product | Pick a .StreamUP file, the plugin check, the scenes landing in the list |
| my-products (still) | Search for a product, open its controls with Adjust |
| plugin-check | The update list flagging an outdated plugin |
| scene-organiser (still) | Open a folder, star a favourite, search to find a scene |
| streamup-dock (still) | Click lock all, the lock icons in the Sources dock follow |
| multidock | Drag two docks into one Multi-Dock as tabs |
| toolbar (still) | The toolbar's buttons, and the stats ticking while live |
| vertical-canvas | Aitum Vertical's row before and after |
| multistream | The Multistream dock before and after on the StreamUP theme |
| dock-spacing (still) | The whole window, even gaps between the docks |
| window-polish | A colour property as a pill, a roomy list dialog |
| mixer-dock (still) | Switching mixer tabs, dragging a source between tabs |
| mixer-layout (still) | OBS's mixer with full names wrapping |
| adjustment-layer | Add the layer, a blur that hits everything under it |
| scene-switch | Right-click a nested scene in Sources, Switch to Scene |
| source-folders (still) | A group opening and closing as a folder |
| source-guides | The guide lines down a folder's children |
| hotkeys | Press a hotkey, sources lock; the Hotkeys settings page |
| websocket | A Stream Deck press running a StreamUP request |
| studio-mode | The transition slider with its halfway mark, mid-transition |
| backup | Create Backup, then the restore preview |

### Defaults for new users (proposal, not applied)

Existing users keep what they have; the code default is still everything on. Proposed new-user
defaults, each with the reason:

**On**
- Scene Organiser: the main reason people install StreamUP.
- StreamUP Dock: small, and most product guides point at its buttons.
- Toolbar: the quickest route to the common tools.
- Hotkeys: costs nothing until a key is bound.
- WebSocket API: StreamUP products' Streamer.bot actions need it.
- Backup: one zip a day protects everyone, and most never think to turn it on.
- Switch to Nested Scene: only shows up on a right-click.
- Every StreamUP theme tweak: they only act on the StreamUP theme, which the user chose for
  exactly this.

**Off**
- StreamUP Mixer: a second mixer beside OBS's is confusing until someone wants it.
- Multi-Dock: power-user layout tool.
- Adjustment Layer: advanced source; off keeps the source list short. Turning it on is a
  restart, so it could be the exception that stays on if Andi prefers.
