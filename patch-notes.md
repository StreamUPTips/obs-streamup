# StreamUP Patch Notes

## Version 1.7.2+ (Development Version)

### 🚀 Major New Features

#### Enhanced WebSocket API
- **New PascalCase WebSocket Commands**: Added properly named WebSocket API commands following OBS WebSocket conventions
  - `GetStreamBitrate`, `GetPluginVersion`, `CheckRequiredPlugins`
  - `ToggleLockAllSources`, `ToggleLockCurrentSceneSources`
  - `RefreshAudioMonitoring`, `RefreshBrowserSources`
  - `GetSelectedSource`, `GetRecordingOutputPath`
  - `LoadStreamUpFile`, `OpenSourceProperties`, `OpenSourceFilters`
  - `OpenSourceInteraction`, `OpenSceneFilters`
- **Video Capture Device Management**: New WebSocket commands for bulk video device operations
  - `ActivateAllVideoCaptureDevices`
  - `DeactivateAllVideoCaptureDevices` 
  - `RefreshAllVideoCaptureDevices`
- **Advanced Source Properties**: New WebSocket API for detailed source control
  - `GetBlendingMethod` / `SetBlendingMethod` - Control source blending modes
  - `GetDeinterlacing` / `SetDeinterlacing` - Manage deinterlacing settings
  - `GetScaleFiltering` / `SetScaleFiltering` - Control scale filtering options
  - `GetDownmixMono` / `SetDownmixMono` - Audio downmix controls
- **Transition Management**: Enhanced show/hide transition controls
  - `GetShowTransition` / `SetShowTransition`
  - `GetHideTransition` / `SetHideTransition`
- **Backward Compatibility**: All old WebSocket command names still supported for existing integrations

#### Modular Architecture Redesign
- **Core Managers**: Refactored into specialized manager classes
  - `SourceManager` - Centralized source operations and management
  - `PluginManager` - Plugin detection, updates, and dependency management
  - `SettingsManager` - Unified settings system with UI integration
  - `NotificationManager` - Enhanced notification system
  - `HotkeyManager` - Hotkey registration and management
  - `MenuManager` - Menu system organization
  - `FileManager` - File operations and .streamup file handling
- **Error Handling**: New centralized `ErrorHandler` with categorized logging
- **Utility Modules**: Enhanced utility classes for common operations
  - `UIHelpers` - Reusable UI components and dialogs
  - `StringUtils`, `PathUtils`, `VersionUtils` - Common utility functions
  - `HTTPClient` - Network operations for plugin updates

#### Enhanced User Interface
- **Redesigned Splash Screen**: Modern welcome screen with rich content
  - Dynamic patch notes loading from markdown files
  - Support information with donation links
  - Social media integration (Twitter, Bluesky, Doras)
  - Supporter acknowledgments section
  - Responsive design with smooth scrolling
- **Settings Dialog Improvements**: Centralized settings management
  - Plugin update preferences
  - Notification settings
  - Startup behavior controls
- **Rich Text Support**: Enhanced markdown processing for documentation
  - Inline formatting (**bold**, *italic*, `code`)
  - Hyperlink support with styling
  - Horizontal rules and structured content

### 🔧 Technical Improvements

#### Plugin Management
- **Enhanced Plugin Detection**: Improved plugin version checking and compatibility
- **Automatic Updates**: Background plugin update checking with user preferences
- **Dependency Management**: Better handling of required OBS plugins
- **Module Initialization**: Improved startup sequence and plugin loading

#### Source Management
- **Video Capture Device Operations**: Bulk operations for camera management
- **Audio Monitoring**: Enhanced audio monitoring refresh capabilities
- **Browser Source Management**: Improved browser source refresh functionality
- **Source Locking**: Enhanced source locking for all sources or current scene

#### File Operations
- **StreamUP File Loading**: Enhanced .streamup file loading with force options
- **Path Management**: Improved path resolution and file handling
- **Output Path Detection**: Better recording output path detection

### 🐛 Bug Fixes and Improvements

#### Stability Enhancements
- **Memory Management**: Improved resource cleanup and memory handling
- **Thread Safety**: Enhanced thread safety for UI operations
- **Error Handling**: Better error reporting and user feedback

#### UI/UX Improvements
- **Dialog Consistency**: Standardized dialog appearance across the plugin
- **Icon Integration**: Enhanced icon usage throughout the interface
- **Responsive Design**: Better handling of different screen sizes and DPI settings

#### WebSocket API Fixes
- **Parameter Validation**: Enhanced input validation for WebSocket commands
- **Error Responses**: Improved error message clarity and debugging information
- **Response Consistency**: Standardized response formats across all commands

### 🏗️ Development Features

#### Testing and Debugging
- **Development Mode**: Enhanced development features for testing
- **Splash Screen Testing**: Force show splash screen for development testing
- **Detailed Logging**: Improved logging with categorization and severity levels

#### Code Organization
- **Namespace Organization**: Better code organization with StreamUP namespace
- **Header Management**: Cleaner header includes and dependency management
- **Documentation**: Enhanced inline documentation and code comments

### 📝 Documentation Updates
- **Markdown Support**: Enhanced markdown processing for documentation files
- **API Documentation**: Improved WebSocket API documentation
- **User Guides**: Updated user guides and help content

### 🔗 Integration Improvements
- **OBS Frontend API**: Better integration with OBS Studio's frontend API
- **WebSocket Compatibility**: Enhanced compatibility with obs-websocket plugin
- **Cross-Platform Support**: Improved support across Windows, macOS, and Linux

---

## Previous Versions

### Version 1.7.1 (Last Stable Release)
- Base functionality and core features
- Original WebSocket API implementation
- Basic plugin management
- Initial UI components

---

*Note: This changelog represents the current development state. Features may be subject to change before final release.*
