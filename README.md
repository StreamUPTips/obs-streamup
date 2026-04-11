# StreamUP Plugin for OBS Studio

The toolkit that makes OBS less annoying. Product installation, plugin update checking, scene organisation, custom docks, a toolbar, dedicated hotkeys, a source type, a full WebSocket API, and a pile of quality of life bits. All in one plugin.

Built for [StreamUP](https://streamup.tips) products but genuinely useful even if you've never touched one.

---

## Table of Contents

1. [Tools > StreamUP Menu](#tools--streamup-menu)
2. [StreamUP Settings](#streamup-settings)
3. [Scene Organiser](#scene-organiser)
4. [Multi-Dock System](#multi-dock-system)
5. [StreamUP Toolbar](#streamup-toolbar)
6. [StreamUP Dock](#streamup-dock)
7. [Adjustment Layer Source](#adjustment-layer-source)
8. [Source and Scene Tools](#source-and-scene-tools)
9. [Hotkeys](#hotkeys)
10. [WebSocket API](#websocket-api)
11. [Mixer, Studio Mode, and Theme Enhancements](#mixer-studio-mode-and-theme-enhancements)
12. [Welcome Screen, About, Patch Notes](#welcome-screen-about-patch-notes)
13. [Build](#build)
14. [Support](#support)

---

## Tools > StreamUP Menu

Everything hangs off the **Tools > StreamUP** menu in OBS.

- **Install a Product.** Drop in a `.streamup` file and it handles the rest. Hold Shift to force install if something's already there.
- **Download Products.** Opens the StreamUP store.
- **Check Product Requirements.** Scans what you've got and tells you if a StreamUP product needs a plugin you're missing. Hold Shift to refresh the cache.
- **Check for OBS Plugin Updates.** Scans every OBS plugin you have installed and lists the out of date ones with direct download links. Also runs at launch so you don't get caught out.
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

### Plugin Management
Table of every OBS plugin you have installed with its name, version, module, website, and a compatibility tick. Click a website to visit the plugin's forum thread.

### Hotkeys
All StreamUP hotkeys in one place. Bind, rebind, or clear each one. Reset All button to wipe them back to defaults. Only one hotkey can be recorded at a time so you won't accidentally capture the same input into two slots.

### Dock Configuration
Pick which tool buttons appear on the StreamUP dock. Toggle any of the seven individually, or hit Reset to restore the defaults.

### Toolbar
- **Show StreamUP Toolbar** toggle
- **Position.** Top, Bottom, Left, or Right of the OBS window.
- **Icon Size.** Slider from 10 to 24 pixels.
- **Configure Toolbar.** Opens the configurator (see below).

### Scene Organiser
- **Enable Standard Canvas Organiser.** Master toggle for the dock.
- **Show scene and folder icons.** Toggle icons on or off for a cleaner look.
- **Item height.** Slider from 10% to 200% so your scene list can be as tight or chunky as you want.
- **Scene switching mode.** Single click or double click to switch.
- **Automatic sorting.** None, Alphabetical A to Z, Alphabetical Z to A, Newest First, or Oldest First.
- **Keep folders grouped at top.** Folders stay above scenes when sorting.
- **Remember folder expansion state.** Open folders stay open across restarts.
- **Disable preview switching in Studio Mode.** Stops single click from changing the preview scene while you're in Studio Mode.
- **Disable transition in Studio Mode.** Stops double click from firing a transition while you're in Studio Mode.
- **Import current collection.** Pulls your existing scene list from the old SceneTree plugin straight into the organiser.

---

## Scene Organiser

A proper replacement for the OBS scene list. Drag and drop scene management with folders, colour coding, search, sorting, and adjustable item sizing. Remembers your layout between sessions, even with special characters in scene collection names.

### Dock toolbar
- Add Folder
- Remove selected item
- Open Filters for selected scene
- Move Up and Move Down
- Expand All and Collapse All (button updates dynamically based on current state)
- Lock the organiser layout
- Search box that filters by scene or folder name without collapsing the tree

### Right-click on a scene
- Rename
- Switch to scene
- Duplicate scene
- Copy and paste filters between scenes
- Remove
- Open scene filters
- Take a screenshot
- Show in multiview
- Open projector on any monitor, or fullscreen projector
- Move Up, Down, To Top, or To Bottom
- Sort (alphabetical A to Z or Z to A, newest first, oldest first)
- Set a colour or clear it
- Toggle icons

### Right-click on a folder
- Rename
- Delete
- Add a folder
- Create a scene inside it
- Move Up, Down, To Top, or To Bottom

### Extras
- Hide scenes you don't need to see to keep your workspace tidy.
- SceneTree importer. If you're coming from the old DigitOtter SceneTree plugin, one click brings your layout over.

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

Slim replacement for the bulky OBS Controls dock. Position it top, bottom, left, or right. Size the icons however you like.

### Configurator
Left panel shows available items grouped into tabs. Right panel shows your current toolbar. Drag to reorder or use the Move Up and Move Down buttons.

**Available tabs:**
- **StreamUP tab.** Built in buttons for every StreamUP tool.
- **Dock tab.** Show or hide any OBS or StreamUP dock as a toolbar button.
- **Hotkey tab.** Add your own custom hotkey button with a chosen icon and label.

**Actions:**
- Add Selected Button, Add Dock Button, Add Hotkey Button
- Add Custom Spacer (adjustable width in pixels)
- Add Separator
- Remove, Move Up, Move Down
- Reset to Default

**Built in button types:**
- Lock or Unlock All Sources
- Lock or Unlock Current Scene Sources
- Refresh Browser Sources
- Refresh Audio Monitoring
- Activate, Deactivate, or Refresh Video Capture Devices
- Group Selected Sources
- Toggle Visibility of Selected Sources
- Show or Hide Selected Sources
- Custom hotkey buttons

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

Over 30 vendor commands under the `streamup` namespace. Use them from Streamer.Bot, Touch Portal, Stream Deck, or any tool that talks OBS WebSocket. Full protocol docs in [`WEBSOCKET_API_DOCUMENTATION.md`](WEBSOCKET_API_DOCUMENTATION.md).

### Utility
- `GetStreamBitrate`. Current stream bitrate in kbps.
- `GetPluginVersion`. StreamUP version number.
- `CheckRequiredPlugins`. Returns which required plugins are missing.

### Source management
- `ToggleLockAllSources`
- `ToggleLockCurrentSceneSources`
- `RefreshAudioMonitoring`
- `RefreshBrowserSources`
- `GetSelectedSource`
- `GroupSelectedSources`
- `ToggleVisibilitySelectedSources`

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
- `LoadStreamupFile`. Load a `.streamup` product file by path.
- `GetVLCCurrentFile`. Current file playing in a VLC source.

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
