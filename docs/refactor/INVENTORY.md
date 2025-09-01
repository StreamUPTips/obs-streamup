# StreamUP Plugin - Inventory Report

Generated: August 30, 2025
Branch: streamup-optimisations

## Project Overview
- **Name**: StreamUP
- **Version**: 2.0.0
- **Type**: OBS Studio Plugin
- **Build System**: CMake
- **Framework**: Qt6 (Core, Widgets, Gui, Svg)
- **Language**: C++

## Module Structure

### Root Files
- `streamup.cpp/hpp` - Main plugin entry point
- `video-capture-popup.cpp/hpp` - Video capture device popup dialog
- `flow-layout.cpp/hpp` - Custom flow layout widget
- `StreamUPDock.ui` - UI form file for main dock
- `obs-websocket-api.h` - OBS WebSocket API header
- `resources.qrc` - Qt resource file
- `version.h` - Version information (auto-generated)

### Core Module (/core)
**Purpose**: Core plugin functionality and state management
- `streamup-common.cpp/hpp` - Common utilities and shared functionality
- `plugin-state.cpp/hpp` - Plugin state management
- `source-manager.cpp/hpp` - OBS source management
- `file-manager.cpp/hpp` - File operations and management
- `plugin-manager.cpp/hpp` - Plugin lifecycle management

### Utilities Module (/utilities)
**Purpose**: Reusable utility functions
- `path-utils.cpp/hpp` - Path manipulation utilities
- `string-utils.cpp/hpp` - String processing utilities
- `version-utils.cpp/hpp` - Version handling utilities
- `http-client.cpp/hpp` - HTTP client functionality
- `obs-wrappers.hpp` - OBS API wrapper functions
- `error-handler.cpp/hpp` - Error handling utilities

### UI Module (/ui)
**Purpose**: User interface components and management

#### Main UI Components
- `ui-helpers.cpp/hpp` - UI utility functions
- `ui-styles.cpp/hpp` - Styling and theme management
- `switch-button.cpp/hpp` - Custom switch button widget
- `hotkey-manager.cpp/hpp` - Hotkey functionality
- `hotkey-widget.cpp/hpp` - Hotkey configuration widget

#### Managers
- `menu-manager.cpp/hpp` - Menu system management
- `settings-manager.cpp/hpp` - Settings dialog and persistence
- `notification-manager.cpp/hpp` - Notification system

#### Windows/Dialogs
- `splash-screen.cpp/hpp` - Plugin splash screen
- `patch-notes-window.cpp/hpp` - Patch notes display
- `theme-window.cpp/hpp` - Theme selection dialog
- `websocket-window.cpp/hpp` - WebSocket configuration

#### Dock
- `dock/streamup-dock.cpp/hpp` - Main plugin dock widget

#### Toolbar
- `streamup-toolbar.cpp/hpp` - Main toolbar component
- `streamup-toolbar-config.cpp/hpp` - Toolbar configuration
- `streamup-toolbar-configurator.cpp/hpp` - Toolbar configuration dialog

### Integrations Module (/integrations)
**Purpose**: External service integrations
- `websocket-api.cpp/hpp` - WebSocket API integration

### MultiDock Module (/multidock)
**Purpose**: Multi-dock functionality for custom layouts
- `multidock_manager.cpp/hpp` - Main manager for multi-dock system
- `multidock_dock.cpp/hpp` - Individual dock implementation
- `multidock_utils.cpp/hpp` - Utility functions for multi-dock
- `multidock_dialogs.cpp/hpp` - Dialog implementations
- `persistence.cpp/hpp` - Layout persistence functionality
- `inner_dock_host.cpp/hpp` - Host for nested docks
- `add_dock_dialog.cpp/hpp` - Dialog for adding new docks

## Assets and Resources

### Icons (/images/icons)
#### Social Icons
- beer.svg, bluesky.svg, discord.svg, doras.svg
- github.svg, kofi.svg, patreon.svg, twitter.svg
- streamup-logo-*.svg (button, stacked, text variants)

#### UI Icons (Dark/Light variants)
- Scene source lock/unlock states
- Camera, pause, record, streaming states
- Audio monitoring, browser refresh
- Replay buffer, settings, studio mode
- Video capture device controls
- Virtual camera controls

### Miscellaneous Assets (/images/misc)
- obs-theme-*.png (4 theme preview images)

### Media
- icon.ico - Windows application icon
- logo.png - Plugin logo

### Localization (/data/locale)
- en-US.ini - English translations

## Configuration Files

### Build System
- `CMakeLists.txt` - Main CMake configuration
- `cmake/ObsPluginHelpers.cmake` - OBS plugin build helpers
- `cmake/Bundle/macos/` - macOS bundle configuration

### Packaging
- `installer.iss.in` - Windows installer script template
- `resource.rc.in` - Windows resource template
- `buildspec.json` - Build specification

### Documentation
- `README.md` - Project readme
- `patch-notes-summary.md` - Patch notes summary
- `plugin-info.md` - Plugin information
- `support-info.md` - Support information

## Dependencies

### External Libraries
- **OBS Studio**: libobs, obs-frontend-api
- **Qt6**: Core, Widgets, Gui, Svg
- **CURL**: HTTP client functionality
- **Standard C++17**: STL containers, filesystem, regex

### Platform-Specific
- **Windows**: w32-pthreads
- **Cross-platform**: Standard library threading

## Potential Issues Identified

### Code Organization
1. **Mixed include paths**: Some includes use relative paths, others use module names
2. **Large main file**: streamup.cpp appears to contain multiple responsibilities
3. **Potential duplication**: Multiple manager classes with similar patterns

### Resource Management
1. **Manual memory management**: Some raw pointer usage in path handling
2. **Qt object ownership**: Need to verify parent-child relationships

### Build System
1. **Include directory complexity**: Multiple conditional include paths
2. **Source group maintenance**: Manual maintenance of Visual Studio folders

## Recommendations for Optimization

### High Priority
1. **Consolidate common patterns** in manager classes
2. **Standardize include paths** and header organization
3. **Review memory management** for RAII compliance
4. **Centralize theme and styling** logic

### Medium Priority
1. **Simplify CMake configuration** 
2. **Reduce code duplication** in UI components
3. **Standardize error handling** patterns

### Low Priority
1. **Update documentation** to reflect actual structure
2. **Consider header-only utilities** where appropriate