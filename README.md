# StreamUP Plugin for OBS Studio

The toolkit that makes OBS less annoying. Product installation, plugin update checking, scene organisation, custom docks, a toolbar, backup and restore, dedicated hotkeys, a source type, a full WebSocket API, and a pile of quality of life bits. All in one plugin.

Built for [StreamUP](https://streamup.tips) products but genuinely useful even if you've never touched one.

---

## Table of Contents

1. [The StreamUP Menu](#the-streamup-menu)
2. [StreamUP Settings](#streamup-settings)
3. [Scene Organiser](#scene-organiser)
4. [Multi-Dock System](#multi-dock-system)
5. [StreamUP Toolbar](#streamup-toolbar)
6. [StreamUP Dock](#streamup-dock)
7. [Backup and Restore](#backup-and-restore)
8. [Adjustment Layer Source](#adjustment-layer-source)
9. [Source and Scene Tools](#source-and-scene-tools)
10. [Hotkeys](#hotkeys)
11. [WebSocket API](#websocket-api)
12. [Mixer, Studio Mode, and Theme Enhancements](#mixer-studio-mode-and-theme-enhancements)
13. [Welcome Screen, About, Patch Notes](#welcome-screen-about-patch-notes)
14. [Build](#build)
15. [Support](#support)

---

## The StreamUP Menu

StreamUP gets its own menu in the OBS menu bar, next to Help. Everything hangs off it.

- **Install a Product.** Drop in a `.streamup` file and it handles the rest. Hold Shift to force install if something's already there.
- **Download Products.** Opens the StreamUP store.
- **Check Product Requirements.** Scans what you've got and tells you if a StreamUP product needs a plugin you're missing. Hold Shift to refresh the cache.
- **Check for OBS Plugin Updates.** Scans every OBS plugin you have installed and lists the out of date ones with direct download links. Also runs at launch so you don't get caught out.
- **Backup submenu:** Backup OBS, and Restore From Backup.
- **Tools submenu:**
  - Lock or Unlock All Sources (everywhere, or just the current scene)
  - Refresh Audio Monitoring for all sources
  - Refresh All Browser Sources
  - Video Capture Devices: Activate All, Deactivate All, or Refresh All
- **Multi-Dock submenu:** New Multi-Dock, Manage Multi-Docks, and a toggle for each Multi-Dock you've created.
- **Theme.** Opens the theme window.
- **WebSocket Commands.** Full browsable reference for every StreamUP WebSocket vendor command. Copy OBS Raw requests straight out of it, or copy Streamer.Bot CPH code if you've got that enabled.
- **Settings.** Opens the main settings dialog (see below).
- **Patch Notes.** Collapsible card view of every version's changes.
- **About.** Version info, credits, and an update check button.

---

## StreamUP Settings

One dialog, multiple tabs.

### General
- **Run plugin checker on OBS startup.** Scans for plugin updates when OBS launches.
- **Mute system tray notifications.** Silences StreamUP notifications. Can also auto-mute while you're live so nothing pops up mid-stream.
- **Show CPH references for Streamer.Bot.** Adds Streamer.Bot CPH code alongside the OBS Raw code in the WebSocket Commands window.
- **Show StreamUP Toolbar.** Master toggle for the toolbar.
- **Enable debug logging.** Extra detail in the OBS log file for troubleshooting.

### Plugins
Switch whole parts of StreamUP on or off: the Toolbar, Multi-Dock System, Scene Organiser, StreamUP Dock, StreamUP Hotkeys, Adjustment Layer Source and Backup. Handy if you only want one or two bits.

### Plugin Management
Table of every OBS plugin you have installed with its name, version, module, website, and a compatibility tick. Click a website to visit the plugin's forum thread.

### Backup
Automatic backups, how many to keep, and where they are saved. There is a Back Up Now button and a Restore From Backup button here too.

### Hotkeys
All StreamUP hotkeys in one place. Bind, rebind, or clear each one. Reset All button to wipe them back to defaults. Only one hotkey can be recorded at a time so you won't accidentally capture the same input into two slots.

### Dock Configuration
Pick which tool buttons appear on the StreamUP dock. Toggle any of the seven individually, or hit Reset to restore the defaults.

### Toolbar
- **Show StreamUP Toolbar** toggle
- **Position.** Top, Bottom, Left, or Right of the OBS window.
- **Alignment.** Start, Centre, or End. Docked left or right, Start becomes top and End becomes bottom. The StreamUP settings button stays pinned at the far end either way.
- **Icon Size.** Slider from 10 to 24 pixels.
- **Edit Toolbar.** Drops the toolbar itself into edit mode (see below).

### Scene Organiser
- **Enable Standard Canvas Organiser.** Master toggle for the dock.
- **Show scene and folder icons.** Toggle icons on or off for a cleaner look.
- **Show Favourites tab.** Adds a Favourites tab to the dock. Right-click a scene to add it.
- **Show Recent tab.** Adds a Recent tab listing the scenes you have been live with most recently.
- **Show folder guide lines.** Vertical lines down the tree so it is clear which folder each scene belongs to.
- **Item height.** 19 to 48 pixels, so your scene list can be as tight or chunky as you want. The default of 24px matches the normal OBS docks.
- **Scene switching mode.** Single click or double click to switch.
- **Automatic sorting.** None, Alphabetical A to Z, Alphabetical Z to A, Newest First, or Oldest First.
- **Keep folders grouped at top.** Folders stay above scenes when sorting.
- **Remember folder expansion state.** Open folders stay open across restarts.
- **Disable preview switching in Studio Mode.** Stops single click from changing the preview scene while you're in Studio Mode.
- **Disable transition in Studio Mode.** Stops double click from firing a transition while you're in Studio Mode.
- **Switch to new scene on create.** Switches OBS to a scene as soon as you make it in the organiser. In Studio Mode it only goes to preview, never straight to program.
- **Import current collection.** Pulls your existing scene list from the old SceneTree plugin straight into the organiser.

---

## Scene Organiser

A proper replacement for the OBS scene list. Drag and drop scene management with folders, colour coding, icons, tabs, search, sorting, and adjustable item sizing. Remembers your layout between sessions, even with special characters in scene collection names.

### Tabs
Favourites, Recent, and as many of your own as you like. Each tab holds its own set of scenes and folders, saved per scene collection. Favourites and Recent can be switched off in Settings if you would rather not have them. Search works on every tab, and Enter sends the top result straight to program.

### Undo and redo
Ctrl+Z and Ctrl+Y cover moves, folders, renames and colours, so a drag that went somewhere odd is one keypress away from being put back.

### Icons
- Custom icons on scenes and folders, taken from your OBS theme or an image off your drive
- Emoji and symbol icons, with a searchable picker, or paste in any character you like
- Icon colours, including on the default icon
- Folders show an open icon or a closed one depending on their state
- Folder guide lines down the tree, with a switch in Settings, Scene Organiser

### Dock toolbar
- Add Folder
- Remove selected item
- Open Filters for selected scene
- Move Up and Move Down
- Expand All and Collapse All (button updates dynamically based on current state)
- Lock the organiser layout
- Search box that filters by scene or folder name without collapsing the tree

### Right-click on a scene
- Rename (F2) and Duplicate
- Copy Filters and Paste Filters between scenes
- Remove (Del)
- Add to Favourites, or Add to Tab for one of your own tabs
- Hide Scene
- Order: move up, down, to the top or to the bottom
- Open Scene Projector
- Transition Override. Pick a transition and a duration for a scene, or None to clear it. Stored where OBS stores its own, so an override set here turns up in the OBS scene list and the other way round. Main canvas only
- Screenshot (Scene)
- Filters
- Show in Multiview
- Set Color, and Set Icon (an OBS source icon, a custom image, or an emoji or symbol, plus icon colour)
- Toggle Icons
- Lock Scene Organiser
- Sort: A to Z, Z to A, newest first, or oldest first

Renaming is F2 or right-click, Rename. Never a click. Multi select works too, so you can drag a run of scenes at once.

### Right-click on a folder
- Rename
- Delete
- Add a folder
- Create a scene inside it
- Move Up, Down, To Top, or To Bottom

### Live scene and Studio Mode
- Always highlights the live scene so you can see what's on air at a glance.
- In Studio Mode the preview scene shows in a different colour. Move it up and down with the arrow keys and press Enter to cut it to program, so you can drive a show from the keyboard.

### Extras
- Built-in colour picker for both folder and scene colours, so you can pick any shade without leaving OBS.
- Hide scenes you don't need to see to keep your workspace tidy.
- SceneTree importer. If you're coming from the old DigitOtter SceneTree plugin, one click brings your layout over.
- Your folder layout is backed up every time it saves, so a bad scene collection switch cannot take it with it.

### Vertical Canvas Organiser
A second Scene Organiser pointed at Aitum's Vertical Canvas plugin. Same folders, colours, icons, search and drag and drop as the main one, with its own folder layout so your vertical scenes are not mixed in with your main ones. The dock turns up on its own once the vertical canvas is running, and clicking a scene switches it through Aitum's own transition. Linked Scenes are shared with Aitum's own list. On the StreamUP theme the Vertical Canvas control row is a proper toolbar, and the virtual camera button goes green while the camera is running.

---

## Multi-Dock System

Combine multiple OBS docks into one container so your layout doesn't look like confetti.

- **New Multi-Dock.** Create a new container and give it a name.
- **Manage Multi-Docks.** Edit, rename, delete, or add docks to any of your Multi-Docks from one dialog.
- **Lock.** Lock a Multi-Dock's layout when you're happy so you don't nudge things by accident. The lock state persists across OBS restarts.
- **Visibility toggle.** Each Multi-Dock gets a menu item you can tick on or off.
- **Persistence.** Layout and contents save across sessions.

Build themed setups like a "Vertical Canvas" dock with preview, scene list, and source list all in one place.

---

## StreamUP Toolbar

Slim replacement for the bulky OBS Controls dock. Position it top, bottom, left, or right, align it Start, Centre or End, and size the icons however you like.

### Editing it
Right-click the toolbar and pick **Edit Toolbar**. The bar itself is what you edit. Drag items onto it from the panel alongside, drag them along to reorder, drag them off to remove, and click anything to change its settings. There is no separate window guessing at the result, because you are working on the real toolbar throughout.

### What you can put on it
- **StreamUP tools.** A button for every StreamUP tool.
- **Dock buttons.** Show or hide any OBS or StreamUP dock.
- **Hotkey buttons.** Fire any OBS hotkey, with an icon and label you choose.
- **WebSocket buttons.** Fire a standard obs-websocket request, one of StreamUP's own, or another plugin's vendor request named by hand. Common requests give you real fields with your scenes, sources and transitions as dropdowns, and there is a raw JSON box for the rest. Anything that answers back, like GetStats or GetRecordStatus, pops a small readout next to the button and puts the full result on your clipboard.
- **Status readouts.** CPU, frame rate, missed frames, recording time, stream time, stream bitrate, recording bitrate and a status message, the same numbers as the OBS status bar. Each one is an icon and a value, the icon can be turned off, and the two clocks can hold hours from the start so they do not change shape when they pass an hour. Right-click a readout to reset it. Your OBS status bar is untouched.
- **Spacers.** Set a width, or tick Fill available space and it takes whatever the bar has spare, so buttons can sit on the left and stats on the right. Grab a spacer's edge on the toolbar to drag it to size.
- **Separators.**

### Built in button types
- Lock or Unlock All Sources
- Lock or Unlock Current Scene Sources
- Refresh Browser Sources
- Refresh Audio Monitoring
- Activate, Deactivate, or Refresh Video Capture Devices
- Group Selected Sources
- Toggle Visibility of Selected Sources
- Show or Hide Selected Sources

Buttons with no icon show their name instead of an empty square, so you can always find them on the bar.

Docked down the side, every item stacks icon above value and switches to a shorter form, so the bar stays as narrow as it was.

---

## StreamUP Dock

A dedicated dock that puts the most common StreamUP tools one click away. Every button is optional. Pick what you want in Settings > Dock Configuration.

**Available buttons:**
- Lock or Unlock All Sources
- Lock or Unlock Current Scene Sources
- Refresh Browser Sources
- Refresh Audio Monitoring
- Video Capture Devices (activate, deactivate, or refresh)
- Group Selected Sources
- Toggle Visibility of Selected Sources. Reflects the actual visibility state of your selected sources so you can see at a glance what's on and what's off.

Right-click the dock for quick access to the config dialog.

---

## Backup and Restore

One file holds your whole OBS setup. Scene collections, profiles, plugin settings, themes and OBS settings. Browser caches, logs and crash reports are skipped, so a setup that fills 563MB on disk backs up to about 500KB.

- **Automatic.** A backup runs when OBS starts, at most once a day, keeping the last 10. Switch it off in Settings, Backup.
- **Manual.** Back Up Now from the settings, or StreamUP menu > Backup. Your stream key is left out by default so the file is safe to share, and kept in the automatic ones since those stay on your machine.
- **Restore just the bit you lost.** Pick which parts come back, and within scene collections pick exactly which ones. Anything you leave unticked is never unpacked, and files that already match are left alone.
- **Before it restores** it shows you what is in the file, warns about plugins you no longer have, and saves a safety copy of your current setup. Everything goes back as OBS shuts down, and it tells you how it went when you reopen.
- **Missing media check.** Backing up checks every file your scenes point at and lists the ones that are no longer on disk, with the scene collection that wants them. Right-click a row to copy the name or path, or export the lot to a text file or spreadsheet.

Works the same on a normal install or a portable one.

---

## Adjustment Layer Source

A new source type that applies filters to everything beneath it in your scene. No more duplicating the same filter across ten sources. Works a bit like a Photoshop adjustment layer.

**Properties:**
- **Opacity.** Blend the filter effect from 0 to 100%.
- **Group Only.** Affect only sources in the same group, or everything in the scene.
- **Hide Originals.** Hide the unfiltered versions so you only see the filtered output.
- **Filter Mode.** All Sources, Include List Only, or Exclude List.
- **Source Picker.** Add or remove sources from the include or exclude list.

---

## Source and Scene Tools

Fire these from the menu, the dock, the toolbar, a hotkey, or the WebSocket API. Same functions, whichever way you prefer.

- **Lock or Unlock All Sources.** Across every scene, or just the current one.
- **Refresh Audio Monitoring.** Resets monitoring for every source, useful when audio routing gets stuck.
- **Refresh All Browser Sources.** Reloads every browser source in one go.
- **Video Capture Devices.** Activate, deactivate, or refresh all of them. Fixes cameras that didn't wake up when OBS started.
- **Group Selected Sources.** Wraps whatever you've got selected into a new group.
- **Toggle Visibility of Selected Sources.** Flip selected sources on or off together.
- **Copy and Paste Show or Hide Transitions.** Copy a source's show or hide transition, paste it onto another source. Huge time saver for matching effects across a scene.

---

## Hotkeys

Every StreamUP function has a hotkey. Bound through OBS Settings > Hotkeys or through StreamUP's own hotkey menu.

- Lock or Unlock All Sources
- Lock or Unlock Current Scene Sources
- Refresh Browser Sources
- Refresh Audio Monitoring
- Open Selected Source Properties
- Open Selected Source Filters
- Open Selected Source Interact
- Open Current Scene Filters
- Activate All Video Capture Devices
- Deactivate All Video Capture Devices
- Refresh All Video Capture Devices
- Copy Show Transition
- Copy Hide Transition
- Paste Show Transition
- Paste Hide Transition
- Group Selected Sources
- Toggle Visibility of Selected Sources

---

## WebSocket API

Over 30 vendor commands under the `streamup` namespace. Use them from Streamer.Bot, Touch Portal, Stream Deck, or any tool that talks OBS WebSocket. Full protocol docs, with every command's parameters and response fields, at [docs.streamup.tips](https://docs.streamup.tips/obs/streamup_plugin_websocket_api.html).

### Utility
- `GetStreamBitrate`. Current stream bitrate in kbps.
- `GetPluginVersion`. StreamUP version number.
- `CheckRequiredPlugins`. Returns which required plugins are missing.
- `GetRecordingOutputPath`. The file OBS is recording to.

### Source management
- `ToggleLockAllSources`
- `ToggleLockCurrentSceneSources`
- `RefreshAudioMonitoring`
- `RefreshBrowserSources`
- `GetSelectedSource`
- `GroupSelectedSources`
- `ToggleVisibilitySelectedSources`
- `GetAllSourcesLocked` and `GetCurrentSceneSourcesLocked`. Read back whether sources are locked, so a Stream Deck or Streamer.bot button can stay in sync.
- `GetSelectedVisibility`. Whether the selected source is visible.

### Transitions
- `GetShowTransition` and `SetShowTransition`
- `GetHideTransition` and `SetHideTransition`
- `CopyShowTransition` and `PasteShowTransition`
- `CopyHideTransition` and `PasteHideTransition`

### Source properties
- `GetBlendingMethod` and `SetBlendingMethod`
- `GetDeinterlacing` and `SetDeinterlacing` (disable, discard, retro, blend, blend 2x, linear, linear 2x, yadif, yadif 2x)
- `GetScaleFiltering` and `SetScaleFiltering` (disable, point, bicubic, bilinear, lanczos, area)
- `GetDownmixMono` and `SetDownmixMono`

### UI interaction
- `OpenSourceProperties`
- `OpenSourceFilters`
- `OpenSourceInteraction`
- `OpenSceneFilters`

### File management
- `LoadStreamUpFile`. Load a `.streamup` product file by path.
- `GetVLCCurrentFile`. Current file playing in a VLC source.

### Backup
- `CreateBackup`. Takes a backup with no setup needed and reports how many files went in, whether the stream key was included and how many files your scenes are missing.
- `GetBackupInfo`. Your backup settings and the backups you already have.

There is no restore request on purpose. Restoring replaces your setup and deserves a look at what is in the file first.

### Video capture devices
- `ActivateAllVideoCaptureDevices`
- `DeactivateAllVideoCaptureDevices`
- `RefreshAllVideoCaptureDevices`

---

## Mixer, Studio Mode, and Theme Enhancements

### Mixer
Rounded hover styling for source name labels in the audio mixer and dynamic icons in Advanced Audio Properties that show each source's monitoring state at a glance. Only kicks in on OBS 32.1+ and only when a StreamUP theme is active, so it doesn't mess with other themes.

### Studio Mode
Rounded corners on the program display so it matches the preview. Mid-point transition UI improvements. Optional "disable scene switching" settings (covered under Scene Organiser) so you can't accidentally fire a transition mid-show.

### Theme
Theme-aware styling that follows the OBS palette and updates without restarting OBS. An optional StreamUP OBS theme is available to monthly supporters.

### Font Checker
When you install a StreamUP product, the plugin checks whether any text sources reference fonts you don't have installed. If it finds missing ones it shows a dialog with download links so your text doesn't break.

---

## Welcome Screen, About, Patch Notes

- **Welcome Screen.** Shows what's new, supporter shoutouts, and useful links.
- **About.** Condensed dialog with version info, credits, and an update check button.
- **Patch Notes.** Collapsible per-version cards so you can find the release you're looking for.
- **Notifications.** System tray notifications for completed actions. Mutable in settings and can auto-mute while you're live.

---

## Build

Requires OBS Studio 31.1 or newer.

**In-tree build:**
1. Set up OBS Studio: https://obsproject.com/wiki/Install-Instructions
2. Check out this repo to `frontend/plugins/obs-streamup`
3. Add `add_subdirectory(obs-streamup)` to `frontend/plugins/CMakeLists.txt`
4. Rebuild OBS

**Stand-alone build (Linux only):**
```bash
cmake -S . -B build -DBUILD_OUT_OF_TREE=On
cmake --build build
```

---

## Support

Built and maintained by Andi. It's free and always will be. If it saves you time, consider chucking some support his way. Cheers.

- [**Memberships**](https://andilippi.co.uk/pages/memberships). Access all products and exclusive perks.
- [**PayPal**](https://www.paypal.me/andilippi). Buy me a beer.
- [**Twitch**](https://www.twitch.tv/andilippi). Come hang out and ask questions.
- [**YouTube**](https://www.youtube.com/andilippi). Tutorials on OBS and streaming.
- [**Discord**](https://discord.com/invite/RnDKRaVCEu). Community support and chat.
