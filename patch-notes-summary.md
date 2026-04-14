# StreamUP v2.2.2 - Patch Update

## Improvements
- **Toolbar Theming** - Stripped custom sizing so the toolbar now follows OBS theme standards for a more consistent look
- **Theme Switching** - Table row heights and theme styling now re-apply correctly when switching between themes without needing to restart

## Bug Fixes
- **Scene Organiser Memory Leak** - Fixed leaked source references when duplicating scenes from the Scene Organiser

---

# StreamUP v2.2.1 - Patch Update

## Bug Fixes
- **Adjustment Layer Freeze** - Fixed sources flickering on and off during scene transitions when using the Adjustment Layer with "Hide Originals" enabled. Visibility mutations are now deferred until the transition completes

---

# StreamUP v2.2.0 - Feature Update

## New Features
- **Adjustment Layer Source** - A brand new source type that lets you apply filters to everything beneath it in your scene, without having to duplicate filters across multiple sources
- **Dock Visibility Button** - The visibility button in the StreamUP dock now reflects the actual state of your selected sources, so you can see at a glance what's visible and what's not

## Improvements
- **UI Refresh** - The entire plugin has been given a fresh coat of paint to match the OBS theme palette properly. Dialogs, buttons, dropdowns, and input fields all look cleaner and feel more at home inside OBS
- **Redesigned About Window** - Condensed layout with a cleaner header so it takes up less space and is easier to read
- **Redesigned Patch Notes** - Each version now lives in its own collapsible card so you can quickly find the release you're looking for
- **Dock Config Dialog** - Reworked to use a cleaner section and card layout, making it much easier to navigate
- **Hotkey Recording** - Only one hotkey can be recorded at a time now, so you won't accidentally capture input into multiple slots
- **Multi-Dock & Hotkeys Polish** - Better buttons in the Multi-Dock interface and cleaner section dividers in the Hotkeys menu

## Bug Fixes
- **Toolbar Configurator Crash** - Fixed a crash that could happen when opening the Toolbar Configurator
- **Dialog Crashes** - Fixed several crashes caused by frameless dialogs being closed in the wrong order
- **Thread Safety & Null Checks** - Added extra safety checks across the plugin to prevent rare crashes
- **Locale Fixes** - Filled in missing translations and fixed a bullet point encoding issue in certain languages

---

# StreamUP v2.1.8 - Patch Update

## Bug Fixes
- **Scene Collection Crash Fix** - Fixed a crash that occurred when changing scene collections

---

# StreamUP v2.1.7 - Patch Update

## Improvements & Bug Fixes
- **Memory & Stability Fixes** - Fixed several memory leaks throughout the plugin, including when loading StreamUP products and using WebSocket bitrate requests. This should noticeably reduce memory usage over long sessions
- **Multi-Dock Improvements** - Improved dock restoration reliability and fixed issues where docks could lose their layout after restarting OBS
- **Show/Hide Transitions** - Fixed an issue where setting show/hide transitions via WebSocket could fail on certain error paths
- **Theme Enhancements** - Theme changes are now detected automatically without needing to restart OBS
- **Thread Safety** - Improved plugin state handling to prevent potential issues when multiple parts of the plugin access shared data at the same time
- **General Cleanup** - Removed unused code, consolidated duplicate styling, and improved the shutdown sequence to properly clean up all UI enhancements

---

# StreamUP v2.1.6 - Patch Update

## New Features
- **OBS 32.1 Support** - Full compatibility with OBS Studio 32.1
- **Font Checker** - StreamUP now checks for missing fonts when installing products and shows a warning dialog with download links if any fonts are not installed on your system
- **Dynamic Audio Monitoring Icons** - Advanced Audio Properties now displays dynamic icons showing the current monitoring state for each source
- **Mixer Enhancements** - Added rounded hover styling for source name labels in the audio mixer when using StreamUP themes
- **Color Preview Pills** - New color preview pill styling for StreamUP themes

## Improvements & Bug Fixes
- **Scene Organiser Item Height Range** - Expanded height adjustment range from 50-200% to 10-200% with a new default of 50%, allowing for more compact scene lists
- **Mixer Enhancements Compatibility** - Mixer styling enhancements now only apply on OBS 32.1+ to prevent issues on older versions
- **Multi-Dock Lock Persistence** - Fixed issue where the Multi-Dock lock state would reset to unlocked after restarting OBS
- **Scene Organiser Studio Mode Controls** - Split "Disable Scene Switching in Studio Mode" into two separate settings: one for preview switching (single-click) and one for transitions (double-click), allowing users to disable accidental transitions while keeping preview selection functional
- **Scene Organiser Persistence** - Fixed issue where Scene Organiser folders and order would be lost on OBS restart, particularly affecting scene collections with special characters
- **Studio Mode Program Display** - Fixed rounded corners on the program display in Studio Mode to match the preview display styling
- **Theme Enhancements** - Improved preview and context bar layout with better main window padding and spacing
- **Advanced Audio Properties** - Fixed centering of status dots in the grid layout
- **Theme Detection** - Added proper StreamUP theme checks to all UI enhancement functions to prevent styling issues with other themes

---

# StreamUP v2.1.5 - Patch Update

## Improvements & Bug Fixes
- **Scene Organiser** - Fixed saving and colour issues in Scene Organiser
- **Toolbar** - Removed temporary Group Toolbar functionality
- **UI** - Fixed separator bug

---

# StreamUP v2.1.4 - Patch Update

## New Features
- **Scene Organiser Item Height Adjustment** - Control the size of scene items in Scene Organiser with a slider (50-200%). Both icons and text scale automatically
- **Toolbar Icon Size Control** - Adjust toolbar icon size between 10-24px to fit your workspace
- **Disable Scene Switching in Studio Mode** - Optional setting to prevent accidental scene changes when clicking in Scene Organiser during Studio Mode
- **Add Sources to Group Shortcuts** - New hotkey and WebSocket commands to quickly add selected sources to groups, streamlining your workflow
- **Studio Mode Mid-Point Transition UI** - Enhanced studio mode interface for better transition control

## Improvements & Bug Fixes
- **Settings Persistence** - Fixed critical issue where settings (especially toolbar configuration) weren't saving properly on OBS close
- **Scene Organiser Height Slider** - Slider now properly right-aligned in settings
- **MultiDock Theming** - Improved theme consistency across docks
- **Toolbar Active States** - Added active backgrounds to toolbar buttons for better visual feedback
- **UI Polish** - Various interface improvements and cleanup, including removed borders from native OBS docks for a cleaner look
- **Dock Loading** - Fixed glitches that could occur when loading docks
- **Image and Group Sources** - Fixed issues with image and group source handling
- **Plugin Manager Updates** - Improved plugin manager functionality
- **Dock Host Improvements** - Enhanced inner dock host stability

---

# StreamUP v2.1.3 - Patch Update

## New Features
- **Adjustable Scene Organiser Height** - Customise the height of your Scene Organiser dock to fit your workspace
- **Transition Copy/Paste** - Copy and paste show/hide transitions between sources to speed up your scene setup. This can be set as a HotKey or WebSocket command
- **"Don't Remind" Option** - Added checkbox to stop plugin update notifications if you prefer not to be reminded
- **Early Access Showcase** - Add a banner to show the new features in Early Access and all the perks for supporters
- **Scene Organiser Expand/Collapse Button** - Added dynamic expand/collapse all button that updates based on folder states

## Improvements & Bug Fixes
- **Memory Leak Fixes** - Resolved memory leaks
- **Video Capture Controls** - Fixed issues with video capture device control buttons not working properly in multi-device setups
- **Hotkeys & WebSocket** - Various improvements and additions to the hotkey and WebSocket systems
- **Supporter Names** - Fixed display of supporter names in the credits
- **Scene Organiser Search** - Fixed issue where clearing the search bar would collapse all folders instead of remembering their previous state

---

# StreamUP v2.1.2 - Patch Update

## New Features
- **Scene Organiser Sorting** - Automatically sort your scenes and folders alphabetically (A-Z or Z-A), by newest first, or oldest first
- **Right-Click Scene Sorting** - Right-click in Scene Organiser to quickly sort your scenes without going into settings
- **Remember Folder State** - Scene Organiser now remembers which folders were expanded when you restart OBS

## Improvements & Bug Fixes
- **Plugin Version Checker** - Fixed issues with the plugin update checker not working correctly
- **Scene Organiser** - Various improvements to stability and performance
- **WebSocket Commands Window** - Fixed missing command descriptions that weren't displaying properly

---

# StreamUP v2.1.1 - Patch Update

## New Features
- **Scene Tree Importer** - Import your existing SceneTree plugin configuration into Scene Organiser
- **Hide Scenes in Scene Organiser** - Keep your workspace tidy by hiding scenes you don't need to see
- **Chinese Localisation** - Added zh-CN translation support (thanks to ZRdRy)

## Improvements & Bug Fixes
- Fixed scene collection save issues
- Fixed plugin not loaded check
- Fixed product installation process
- Added libsimde-dev dependency for Ubuntu builds
- Scene Organiser reset functionality improvements

---

# StreamUP v2.1.0 - Major Update

We've listened to your feedback and completely rebuilt the StreamUP OBS plugin from scratch. This has been an enormous project that's taken months of work. The plugin remains completely free - if you find it useful, please share it with friends and consider supporting us!

# New Features & What They Do

## Complete Interface Redesign
Fresh, modern UI that makes all our features easily accessible. We've also created a matching StreamUP OBS theme available to monthly supporters.

## New Welcome Screen
Shows what's new, how to support the project, showcases our supporters, and provides all the important links you need.

## StreamUP Toolbar
The OBS Controls dock is bulky and wastes space. Our sleek toolbar gives you access to a lot of controls and hotkeys plus StreamUP settings. Position it at the top, left, right, or bottom of OBS. Enable it in StreamUP > Settings.

## Multi-Dock System  
Combine multiple functions into single, organised docks. Create themed setups like a 'Vertical Canvas' dock with everything for vertical streaming in one place. Perfect for keeping your workspace tidy and efficient.

## Scene Organiser
Organise your scenes into folders and even colour code them. This is based on the SceneTree plugin by DigitOtter with some extra StreamUP spice added such as a search function for your scenes!

## Enhanced WebSocket Commands
Previously available but poorly documented commands now have a proper interface at StreamUP > WebSocket Commands. Copy OBSRaw websocket requests directly for your workflow. Streamer.Bot users can copy CPH (custom C#) code when enabled in settings.

## Dedicated Hotkeys Menu
As StreamUP becomes more feature-rich, we've added our own hotkeys menu that integrates with OBS. Makes it easier to identify which hotkeys belong to StreamUP (though they still appear in the main OBS hotkeys menu).

## Input Capture Device Management
Tired of cameras not activating when OBS starts, or capture cards crashing? This tool lets you enable all devices, disable all devices, or refresh them (turn off and back on). Access via StreamUP > Tools menu, WebSocket, hotkey, or the StreamUP dock.

# Improvements & Bug Fixes

## Better Performance
Enhanced existing features like the OBS plugin update checker. OBS now starts faster because we check for updates after startup instead of during.

## Streamlined Tools Menu
Options in StreamUP > Tools now trigger their function directly instead of opening separate windows. For hotkey or WebSocket access, use the new WebSocket Commands menu and Settings.

---

# Support This Project

StreamUP is completely free and always will be. Your support helps us continue developing amazing features!

## Ways to Support:
- **[Patreon](https://www.patreon.com/streamup)** - Monthly memberships with exclusive benefits
- **[Ko-Fi](https://ko-fi.com/streamup)** - One-time donations and coffee fund
- **[Buy Me a Beer](https://paypal.me/andilippi)** - Because coding is thirsty work!

## Monthly Supporter Benefits:
- **Tier 1 (£5/month):** StreamUP Product Pass + Discord role + Priority support
- **Tier 2 (£10/month):** All Access Pass + Early releases + Budget-friendly access  
- **Tier 3 (£25/month):** Gold Supporter + Name in credits + Monthly giveaways + Exclusive Discord role

## Follow StreamUP's Development Journey:
- **[Andi's Streams](https://twitch.tv/andilippi)** - Watch development live and see what's coming next!
- **[Andi's Socials](https://doras.to/andi)** - Andi always posts about what he's working on!
- **[Discord Community](https://discord.com/invite/RnDKRaVCEu)** - Get support and chat with other users
- **[Twitter Updates](https://twitter.com/StreamUPTips)** - Latest news and announcements

---

*StreamUP v2.0.0 is our biggest update yet - a complete rebuild that makes everything more reliable whilst adding loads of new features. Thanks for being part of the StreamUP community.*
