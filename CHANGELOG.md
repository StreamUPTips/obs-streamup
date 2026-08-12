# StreamUP - Changelog

---

## v2.3.0 (11 Aug '26)
**Patch Focus:** Backup and restore, vertical canvas Scene Organiser, plugin check improvements

### New Features
- Toolbar buttons that fire an obs-websocket request. Choose from the standard obs-websocket requests, StreamUP's own, or name another plugin's vendor and request by hand. Common requests give you real fields with your scenes, sources and transitions as dropdowns, and there is a raw JSON box for the rest
- Requests that return something show it. Anything that answers back, like GetStats or GetRecordStatus, pops a small readout next to the button, and the full result goes on your clipboard at the same time ready to paste. Failures show why. Requests that just do a thing and return nothing stay silent
- Toolbar buttons with no icon show their name instead of an empty square, so you can always find them on the bar and while editing
- Transition Override in the Scene Organiser right click menu, the same as the OBS scene list. Pick a transition and a duration for a scene, or None to clear it. It is stored where OBS stores its own, so an override set here shows up in the OBS scene list and the other way round. Main canvas only, since the vertical canvas switches scenes through Aitum's own transition and never reads it
- The toolbar editor is rebuilt. Right click the toolbar, pick Edit Toolbar, and the bar itself is what you edit: drag items onto it from the panel alongside, drag them along to reorder, drag them off to remove, and click anything to change its settings. No separate window guessing at the result, because you are working on the real toolbar throughout. Existing layouts are carried across, with a one-time note explaining the change
- Back up and restore your whole OBS setup. One file holds your scene collections, profiles, plugin settings, themes and OBS settings. Browser caches and logs are skipped, so a setup that fills 563MB on disk backs up to about 500KB. Backups happen on their own as OBS closes, once a day, keeping the last 10. Restoring shows you what is in the file first, warns about any plugins you no longer have, saves a safety copy of your current setup, then puts everything back as OBS shuts down and tells you how it went when you reopen. Your stream key is left out of manual backups by default so they are safe to share, and kept in the automatic ones since those stay on your machine. Works the same on a normal install or a portable one, and can be switched off in Settings > Plugins
- Restore just the bit you lost. Pick which parts come back, scene collections, profiles, plugin settings, themes or OBS settings, and within scene collections pick exactly which ones, with Select all and Select none. Anything you leave unticked is never even unpacked. Files that already match are left alone, so restoring one lost collection out of thirty writes one file
- A second Scene Organiser for Aitum's Vertical Canvas plugin. Same folders, colours, search and drag and drop as the main one, just pointed at your vertical scenes. The dock turns up on its own once the vertical canvas is running and keeps its own folder layout, so your vertical scenes are not mixed in with your main ones. Clicking a scene switches it through Aitum's own transition
- Open Folder on failed plugins. When a plugin fails to load, the check dialog gets an Open Folder button that opens the folder with the module file highlighted, so you can remove or replace it. Hover the plugin's status to see why it would not load, pulled from your OBS log
- The plugin check now spots plugins that are installed but switched off, either turned off in OBS's own Plugin Manager or blocked because OBS is in Safe Mode. They used to show up as missing, sending you off to download something you already had. They now get their own section telling you where to switch them back on. A required plugin being off always shows, even if you have skipped other reminders
- Custom source icon for the Adjustment Layer. On OBS 32.2 and newer it shows its own icon in the Add Source list and the audio mixer instead of the generic colour icon. Older OBS versions keep the default icon
- Two new WebSocket requests for backups. CreateBackup takes a backup with no setup needed and reports how many files went in, whether the stream key was included and how many files your scenes are missing. GetBackupInfo reports your backup settings and lists the backups you already have. There is no restore request on purpose, since restoring replaces your setup and deserves a look at what is in the file first
- Backing up checks every file your scenes point at and tells you which ones are no longer on disk. Those sources are already broken in OBS, nothing has ever told you which. The list shows the path and the scene collection that wants it, you can right-click any row to copy the name or path, and you can export the lot to a text file or spreadsheet
- Toolbar alignment. New Start / Centre / End dropdown in Settings, Toolbar tab. Works the same way when the toolbar is docked left or right, so Start becomes top and End becomes bottom. The StreamUP settings button stays pinned at the far end either way
- New WebSocket requests to read back whether sources are locked, either every source or just the current scene, and whether the selected source is visible, so a Stream Deck or Streamer.bot button can stay in sync with what is actually happening in OBS

### Improvements
- Toolbar spacers resize while you edit them. Select one and set its size, or grab its edge on the toolbar and drag. It is drawn at its real size throughout and the toolbar updates live. Cancel puts everything back
- MultiDocks go narrower. The minimum width was 400px, which at 125% display scaling was really 500px. The floor is down to 120px, and docks living inside a MultiDock no longer force their own width onto it, so a single wide dock like Twitch chat cannot hold the whole thing open. Docks pulled back out keep their original sizing
- Windows fit their contents. Small StreamUP windows were being inflated to a fixed 420x360 and then spreading their contents down the middle to fill the space. That size is now only what a window opens at when it has no size of its own. The Plugin Updates window sizes to its list instead of always opening 620px tall with one plugin in it
- The mixer mute button shows a muted speaker glyph when a source is muted, so you can tell muted from live at a glance
- StreamUP windows resize. Grab any edge or corner and drag. The smaller windows also open a bit taller by default
- Grouping works with a single source selected. It used to need at least two, though OBS is happy with one

### Bug Fixes
- Toolbar spacers no longer collapse to nothing when the toolbar is docked left or right. They were built before OBS had told the toolbar where it was going to sit, so they were sized for a horizontal run and then rebuilt from whatever they happened to measure on screen, which was nothing. Opening the toolbar settings and pressing Save is no longer part of starting OBS
- The StreamUP theme puts the toolbar's spacing on the side facing the rest of OBS instead of the side against the edge of the window. Docked right it was padding the right hand side, which is the edge you are throwing the mouse at, so the buttons were held away from it for no reason. Each dock position now spaces itself the right way round
- Coloured scenes in the Scene Organiser are painted once. The colour was going on up to 3 times over: once as the row background, once as the theme's selection highlight, and once as StreamUP's own rounded fill. The preset colours are partly see through, so every extra layer stacked into a dark band around a brighter middle. Selecting a coloured scene now keeps that scene's colour, just brighter, instead of blending it with the theme's blue
- Fixed a crash when adding a WebSocket button for a request that takes a scene name. Building the scene dropdown freed the list OBS handed back the wrong way and corrupted memory, so OBS would go down there and then or a minute later with no crash report
- WebSocket buttons no longer disappear when you finish editing. They saved correctly, then a stale check threw them away the moment the toolbar reloaded
- The hotkey picker names your scenes. Every scene switch hotkey was called "Switch to scene" with nothing to tell them apart, because OBS registers them on the scene itself and StreamUP was reading the hotkey's name instead of asking which scene it belonged to. The same fault put every source's Mute under one source, and meant a mute button could fire at the wrong source. Hotkey buttons made before this update still work, but any that mute or unmute a specific source are worth adding again
- The hotkey picker shows the keys a hotkey is actually bound to. Every row used to read "Not bound" whether it was or not
- Toolbar separators are visible on a side-docked toolbar. They were given a thickness but nothing across, so they have been rendering as nothing wide for as long as side docking has existed
- Dragging a StreamUP window between monitors could leave it blank until you let go of the mouse. It now repaints as it crosses
- Pressing Enter in a StreamUP confirmation box used to trigger Cancel, because that is where the keyboard focus landed
- Mixer strips relabelled to the wrong source name when two sources shared the same prefix. Full names now only apply on a unique match
- OBS 32.2 was flashing raw black icons on the mixer buttons. They now re-tint to the theme colour whenever the icon changes
- The Audio Monitoring header was clipping its text. It now holds open to fit the full label
- If a docked panel loaded late, like the Twitch info dock, StreamUP could drop it from your MultiDock on the next restart. It now holds onto docks it has not seen yet and keeps trying to restore them
- Fixed a crash on OBS shutdown, caused by StreamUP letting go of a scene it was watching a little too late in the process
- The wizard's Save & Continue button was showing a double ampersand. The button paints its own text, so it never needed the ampersand doubling
- In the Scene Organiser's double-click switch mode, a click could accidentally start renaming a scene. Renaming is now F2 or right-click, Rename. Never a click
- Plugins that never print their version to the OBS log used to vanish from the Installed Plugins list, which made it look like you had not installed them. They now show as "Not checked", with a hover telling you why and pointing you at the plugin's own page

---

## v2.2.4 (6 Jul '26)
**Patch Focus:** Shared StreamUP UI design system

### Improvements
- Every StreamUP window and dialog now shares one look. Rounded cards, soft shadows, matching pill buttons and dividers
- Windows scale with your text size. Turn up the Windows "Make text bigger" or display scaling setting and StreamUP windows grow to match instead of staying tiny. No change at 100%
- Confirmations, rename boxes and the colour picker are styled in StreamUP instead of the plain OS dialogs
- Built-in colour picker. Picking a folder or scene colour in the Scene Organiser uses a proper StreamUP picker with a colour square, hue strip, hex field and saved swatches
- Buttons line up Cancel on the left, confirm on the right. Anything destructive, like removing a scene or resetting the toolbar, gets a red button
- Theme preview images slide between shots instead of snapping, with rounded corners, arrows inside the image and dot markers along the bottom
- The Scene Organiser always highlights whatever scene is live on stream. In Studio Mode your selected preview scene shows in a different colour, and you can move up and down with the arrow keys then press Enter to cut to it
- The scene and source icons in the Scene Organiser now match the ones in the normal OBS list

### Bug Fixes
- The patch notes window stopped opening after the UI rework
- The show and hide transition tools, and a few source settings, could not find a source if it lived inside a group

---

## v2.2.3 (10 May '26)
**Patch Focus:** Toolbar sizing, Plugins picker and setup wizard

### New Features
- Toolbar Size. New Small / Medium / Large dropdown in Settings, Toolbar tab. Scales icon size and button padding together so buttons stay balanced at every tier
- Plugins picker. One place to turn each piece of StreamUP on or off, new tab in Settings. Soft toggles flip instantly, the rest show a "Restart required" badge. The first-launch wizard runs on fresh installs and after version bumps
- Adjustment Layer auto-position, on by default. The layer slots itself just above the lowest included source so the composite paints in the right z-band, leaving un-included sources untouched
- Switch to New Scene on Create, a new Scene Organiser setting, off by default. In Studio Mode it only sets the preview, never goes to program

### Bug Fixes
- Adjustment Layer originals stayed invisible after restarting OBS. The layer now adopts already-hidden included items on first tick
- Scene Organiser item height defaulted to 100% on fresh installs even though the in-code defaults and the upgrader used 50%
- The Windows installer was shipping with a blank AppId
- Rows in the plugin updates table were getting clipped flat on high-DPI displays
- The wizard's Save & Continue button rendered as "Save  continue" because Qt was eating the ampersand as a mnemonic prefix

---

## v2.2.2 (14 Apr '26)
**Patch Focus:** Toolbar theming

### Improvements
- Stripped custom sizing so the toolbar follows OBS theme standards
- Table row heights and theme styling now re-apply correctly when switching themes, without a restart

### Bug Fixes
- Fixed leaked source references when duplicating scenes from the Scene Organiser

---

## v2.2.1 (12 Apr '26)
**Patch Focus:** Adjustment Layer fix

### Bug Fixes
- Fixed sources flickering on and off during scene transitions when using the Adjustment Layer with "Hide Originals" enabled. Visibility changes are now deferred until the transition completes

---

## v2.2.0 (11 Apr '26)
**Patch Focus:** Adjustment Layer source and UI refresh

### New Features
- Adjustment Layer source. A new source type that applies filters to everything beneath it in your scene, without duplicating filters across multiple sources
- The visibility button in the StreamUP dock now reflects the actual state of your selected sources

### Improvements
- UI refresh across the whole plugin to match the OBS theme palette properly. Dialogs, buttons, dropdowns and input fields all sit better inside OBS
- Redesigned About window with a condensed layout and a cleaner header
- Redesigned patch notes. Each version lives in its own collapsible card
- The dock config dialog was reworked into a cleaner section and card layout
- Only one hotkey can be recorded at a time, so you cannot accidentally capture input into multiple slots
- Better buttons in the MultiDock interface and cleaner section dividers in the Hotkeys menu

### Bug Fixes
- Fixed a crash when opening the Toolbar Configurator
- Fixed several crashes caused by frameless dialogs being closed in the wrong order
- Added thread safety and null checks across the plugin to prevent rare crashes
- Filled in missing translations and fixed a bullet point encoding issue in some languages

---

## v2.1.8
**Patch Focus:** Crash fix

### Bug Fixes
- Fixed a crash when changing scene collections

---

## v2.1.7 (20 Mar '26)
**Patch Focus:** Memory and stability

### Improvements & Bug Fixes
- Fixed several memory leaks, including when loading StreamUP products and using WebSocket bitrate requests. This should noticeably reduce memory usage over long sessions
- Improved dock restoration reliability and fixed MultiDocks losing their layout after restarting OBS
- Fixed show/hide transitions failing on certain error paths when set via WebSocket
- Theme changes are now detected automatically without restarting OBS
- Improved plugin state handling to prevent issues when multiple parts of the plugin access shared data at the same time
- Removed unused code, consolidated duplicate styling, and improved the shutdown sequence to clean up all UI enhancements

---

## v2.1.6 (26 Jan '26)
**Patch Focus:** OBS 32.1 support and mixer enhancements

### New Features
- Full compatibility with OBS Studio 32.1
- Font checker. StreamUP checks for missing fonts when installing products and shows a warning dialog with download links if any are not installed
- Advanced Audio Properties shows dynamic icons for each source's current monitoring state
- Rounded hover styling for source name labels in the audio mixer when using StreamUP themes
- Colour preview pill styling for StreamUP themes

### Improvements & Bug Fixes
- Scene Organiser item height range expanded from 50-200% to 10-200%, with a new default of 50%
- Mixer styling enhancements now only apply on OBS 32.1+ to avoid issues on older versions
- Fixed the MultiDock lock state resetting to unlocked after restarting OBS
- Split "Disable Scene Switching in Studio Mode" into two settings, one for preview switching (single-click) and one for transitions (double-click)
- Fixed Scene Organiser folders and order being lost on restart, particularly on scene collections with special characters
- Fixed rounded corners on the Studio Mode program display so it matches the preview
- Improved theme preview and context bar layout, with better main window padding and spacing
- Fixed centring of status dots in the Advanced Audio Properties grid
- Added proper StreamUP theme checks to all UI enhancement functions so they do not interfere with other themes

---

## v2.1.5 (28 Oct '25)
**Patch Focus:** Scene Organiser fixes

### Improvements & Bug Fixes
- Fixed saving and colour issues in the Scene Organiser
- Removed the temporary Group Toolbar functionality
- Fixed a separator bug

---

## v2.1.4 (24 Oct '25)
**Patch Focus:** Sizing controls and settings persistence

### New Features
- Scene Organiser item height slider (50-200%). Both icons and text scale together
- Toolbar icon size control between 10-24px
- Optional setting to prevent accidental scene changes when clicking in the Scene Organiser during Studio Mode
- New hotkey and WebSocket commands to add selected sources to groups
- Studio Mode mid-point transition UI

### Improvements & Bug Fixes
- Fixed settings, especially toolbar configuration, not saving properly when OBS closed
- The Scene Organiser height slider is now right-aligned in settings
- Improved MultiDock theme consistency
- Added active backgrounds to toolbar buttons
- Removed borders from native OBS docks and general interface cleanup
- Fixed glitches when loading docks
- Fixed image and group source handling
- Plugin manager and inner dock host improvements

---

## v2.1.3 (11 Oct '25)
**Patch Focus:** Transitions and Scene Organiser controls

### New Features
- Adjustable Scene Organiser dock height
- Copy and paste show/hide transitions between sources, available as a hotkey or WebSocket command
- "Don't remind" checkbox to stop plugin update notifications
- Early Access banner showing new features and supporter perks
- Scene Organiser expand/collapse all button that updates based on folder states

### Improvements & Bug Fixes
- Resolved memory leaks
- Fixed video capture device control buttons not working properly in multi-device setups
- Various hotkey and WebSocket additions and improvements
- Fixed the display of supporter names in the credits
- Fixed clearing the Scene Organiser search bar collapsing all folders instead of restoring their previous state

---

## v2.1.2 (5 Oct '25)
**Patch Focus:** Scene Organiser sorting

### New Features
- Automatic sorting of scenes and folders, alphabetically (A-Z or Z-A), newest first, or oldest first
- Right-click sorting in the Scene Organiser, without going into settings
- The Scene Organiser remembers which folders were expanded when you restart OBS

### Improvements & Bug Fixes
- Fixed the plugin update checker not working correctly
- Scene Organiser stability and performance improvements
- Fixed missing command descriptions in the WebSocket Commands window

---

## v2.1.1 (1 Oct '25)
**Patch Focus:** SceneTree import and localisation

### New Features
- Scene Tree importer. Import your existing SceneTree plugin configuration into the Scene Organiser
- Hide scenes in the Scene Organiser
- Chinese (zh-CN) localisation, thanks to ZRdRy

### Improvements & Bug Fixes
- Fixed scene collection save issues
- Fixed the plugin-not-loaded check
- Fixed the product installation process
- Added the libsimde-dev dependency for Ubuntu builds
- Scene Organiser reset improvements

---

## v2.1.0 (23 Sep '25)
**Patch Focus:** Complete rebuild

The plugin was rebuilt from scratch after months of work. It remains completely free.

### New Features
- Complete interface redesign, with a matching StreamUP OBS theme available to monthly supporters
- New welcome screen showing what's new, how to support the project, the supporter list and the important links
- StreamUP Toolbar. The OBS Controls dock is bulky and wastes space, so this gives you the controls, hotkeys and StreamUP settings in a slimmer strip. Position it at the top, left, right or bottom of OBS. Enable it in StreamUP > Settings
- MultiDock system. Combine multiple docks into one, so you can build themed setups like a single Vertical Canvas dock holding everything for vertical streaming
- Scene Organiser. Organise scenes into folders and colour code them, based on the SceneTree plugin by DigitOtter, with a scene search added
- WebSocket Commands window at StreamUP > WebSocket Commands. Copy OBSRaw requests directly, or CPH (custom C#) code for Streamer.Bot when enabled in settings
- Dedicated hotkeys menu that integrates with OBS, so it is easier to tell which hotkeys belong to StreamUP. They still appear in the main OBS hotkeys menu
- Input capture device management. Enable all devices, disable all devices, or refresh them. Available from StreamUP > Tools, WebSocket, a hotkey, or the StreamUP dock

### Improvements
- OBS starts faster because the plugin update check now runs after startup instead of during it
- Options in StreamUP > Tools trigger their function directly instead of opening a separate window

---

## v1.7.1 (9 Jun '25)
- Fixed the menu on Mac and Linux

---

## v1.7.0 (29 May '25)
- Added hotkeys and WebSocket shortcuts
- New toolbar menu
- Load a StreamUP file over WebSocket
- Fixed source clone for the previous or current scene, and for groups and scenes
- Fixed moving a source filter when cloning a group or scene
- Improved Resize Advanced Masks
- Get and Set Show/Hide now use the transition display name as the type
- Fixed GetCurrentSelectedSource
- Fixed the build against the latest OBS, and the Mac build

---

## v1.6.0 (20 Jun '24)
- Added the StreamUP dock
- Added Resize Advanced Mask
- Added vlcGetCurrentFile
- Made the installed plugins list scrollable
- Added a tooltip for muting notifications
- Fixed hotkey save and load
- Fixed the Mac and Linux builds

---

## v1.5.0 (7 Jun '24)
- Added the lock sources system, with dialog options
- Added system tray notifications for refreshes
- Added a mute notifications toggle
- Added new vendor requests
- Fixed saving and loading hotkeys
- Major code cleanup, and a Mac build fix

---

## v1.4.0 (7 Jan '24)
- Added support for the StreamUP Pluginstaller
- Changed "Recommended" plugins to "Required"
- Fixed version numbers in x.x format not being processed
- Fixed the Mac and Linux builds

---

## v1.3.2 (23 Oct '23)
- Added links to the installed plugins dialog
- Moved the dialog onto the UI thread

---

## v1.3.0 (9 Oct '23)
- Added Mac and Linux default plugins to the ignore list
- Fixed Linux module extension name removal
- Code cleanup, and a Mac build fix

---

## v1.2.0 (3 Oct '23)
- Added new tools
- Started a general code improvement pass
- Fixed the in-tree build for OBS CMake version 3.0.0, and the Mac build

---

## v1.1.5 (17 Sep '23)
- Fixed the Mac build

---

## v1.1.4 (13 Aug '23)
- Fixed a WebSocket issue

---

## v1.1.3 (12 Aug '23)
- Added WebSocket support
- Added a list of installed plugins
- The version check now tells you when the installed version is higher than the current one
- Fixed settings saving and loading
- Fixed the plugins message popping up incorrectly
- Fixed the WebSocket send when all plugins are correct
- Fixed the Mac and Linux builds

---

## v1.0.0 (4 Jul '23)
**Initial Release**
- Plugin checker for the OBS plugins StreamUP products depend on
- Added error checking
- Fixed a memory leak and thread congestion
- Minor code optimisations

---

## v0.0.8 (13 Mar '23)
- Fixed reference leaks

---

## v0.0.7 (11 Mar '23)
- Merge filters
- Fixed merge scenes

---

## v0.0.6 (27 Aug '22)
- OBS 28 support

---

## v0.0.5 (2 Oct '21)
- Early builds. Converted paths and stopped nested scenes being scaled
