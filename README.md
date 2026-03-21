# StreamUP Plugin for OBS Studio

The toolkit that makes OBS less annoying. Plugin update checking, product installation, source management, scene organisation, and a load of quality of life tools all in one place.

[StreamUP](https://streamup.tips) products tap into a lot of different OBS plugins to pull off cool effects and widgets. This tool makes sure you've got everything installed and up to date so things actually work. There's also a full OBS plugin update checker built in so you don't have to trawl through the forums every time something gets updated. It checks at launch too.

## What's in the Menu

Under **Tools > StreamUP** you'll find:

- **Install a Product** - Drop in a .StreamUP file and it handles the rest. Hold Shift to force install.
- **Check Product Requirements** - Makes sure you've got all the plugins needed for StreamUP products. Hold Shift to refresh the cache.
- **Check for OBS Plugin Updates** - Scans your installed plugins and tells you what's out of date with download links. Hold Shift to skip the cache.
- **Download Products** - Quick link to the StreamUP store.

### Source and Scene Tools
- Lock or unlock all sources across every scene or just the current one
- Refresh audio monitoring for all sources
- Refresh all browser sources
- Activate, deactivate or refresh all video capture devices
- Group selected sources together
- Toggle visibility of selected sources

### Scene Organiser Dock
Drag and drop scene management with folders, colour coding, search, sorting and adjustable item sizing. Remembers your layout between sessions. You can set the item height anywhere from 10% to 200% so it fits however you like.

### Multi-Dock System
Combine multiple OBS docks into single containers. Create, manage and toggle them from the StreamUP menu. Saves your layout across sessions.

### StreamUP Toolbar
Customisable toolbar with hotkeyable buttons. Position it top, bottom, left or right.

### UI Bits
- Mixer enhancements with dynamic audio monitoring icons
- Studio mode improvements
- Theme-aware styling that updates without restarting OBS

### Font Checker
Detects missing fonts when installing products and gives you download links so your text sources don't break.

### WebSocket API
Over 30 vendor commands under the `streamup` namespace. Source management, transitions, device control, plugin checking, and more. Full docs in `WEBSOCKET_API_DOCUMENTATION.md`.

## Build

**In-tree build:**
1. Build OBS Studio: https://obsproject.com/wiki/Install-Instructions
2. Check out this repository to `frontend/plugins/obs-streamup`
3. Add `add_subdirectory(obs-streamup)` to `frontend/plugins/CMakeLists.txt`
4. Rebuild OBS Studio

**Stand-alone build (Linux only):**
1. Make sure you have the OBS development packages installed
2. Check out this repository and run `cmake -S . -B build -DBUILD_OUT_OF_TREE=On && cmake --build build`

## Support

Built and maintained by Andi. If you find this useful, consider chucking some support his way.

- [**Memberships**](https://andilippi.co.uk/pages/memberships) - Access all products and exclusive perks
- [**PayPal**](https://www.paypal.me/andilippi) - Buy me a beer
- [**Twitch**](https://www.twitch.tv/andilippi) - Come hang out and ask questions
- [**YouTube**](https://www.youtube.com/andilippi) - Tutorials on OBS and streaming
