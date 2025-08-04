# StreamUP v2.0.0 - Complete Redesign

## Version 2.0.0 (August 2025)

### 🚀 Major Architectural Overhaul

**Complete Modular Redesign**
- **Core Architecture**: Complete restructuring into specialized modules with clear separation of concerns
  - `core/` - Business logic modules (SourceManager, PluginManager, FileManager)
  - `utilities/` - Shared utility classes (ErrorHandler, HTTPClient, PathUtils, StringUtils, VersionUtils)  
  - `ui/` - User interface components (MenuManager, SettingsManager, NotificationManager, HotkeyManager)
  - `integrations/` - External integrations (WebSocket API)

**New Manager System**
- **SourceManager** (`core/source-manager.cpp/.hpp`): Centralized source operations and management
  - Advanced source locking with improved reliability and performance
  - Bulk source operations for enhanced workflow efficiency
  - Better handling of source states across scene transitions
- **PluginManager** (`core/plugin-manager.cpp/.hpp`): Plugin detection, updates, and dependency management
  - Enhanced plugin version checking and compatibility validation
  - Automatic plugin update checking with user preferences
  - Better handling of required OBS plugin dependencies
- **FileManager** (`core/file-manager.cpp/.hpp`): File operations and .streamup file handling
  - Enhanced .streamup file loading with force options and validation
  - Improved path resolution and cross-platform file handling
  - Better recording output path detection and management
- **SettingsManager** (`ui/settings-manager.cpp/.hpp`): Unified settings system with UI integration
  - Centralized configuration management across all modules
  - Plugin update preferences and notification settings
  - Startup behavior controls and user customization options
- **NotificationManager** (`ui/notification-manager.cpp/.hpp`): Enhanced notification system
  - Improved user feedback and error reporting
  - Categorized notification types with appropriate styling
  - Better integration with OBS notification system
- **HotkeyManager** (`ui/hotkey-manager.cpp/.hpp`): Hotkey registration and management
  - Updated hotkey naming conventions for clarity
  - Enhanced hotkey registration and conflict resolution
  - Better integration with OBS hotkey system
- **MenuManager** (`ui/menu-manager.cpp/.hpp`): Menu system organization and management
  - Streamlined menu structure and organization
  - Removal of deprecated menu items and cleanup
  - Enhanced menu integration with new manager system

### 🌐 Enhanced WebSocket API

**Complete WebSocket Overhaul**
- **New PascalCase Commands**: Modern WebSocket API following OBS WebSocket conventions
  - `GetStreamBitrate` - Retrieve current streaming bitrate information
  - `GetPluginVersion` - Get StreamUP plugin version details
  - `CheckRequiredPlugins` - Validate required plugin dependencies
  - `ToggleLockAllSources` - Lock/unlock all sources in current scene
  - `ToggleLockCurrentSceneSources` - Lock/unlock sources in active scene
  - `RefreshAudioMonitoring` - Refresh audio monitoring configurations
  - `RefreshBrowserSources` - Reload all browser sources
  - `GetSelectedSource` - Get currently selected source information
  - `GetRecordingOutputPath` - Retrieve recording output directory
  - `LoadStreamUpFile` - Load .streamup configuration files
  - `OpenSourceProperties` - Open source properties dialog
  - `OpenSourceFilters` - Open source filters dialog
  - `OpenSourceInteraction` - Open source interaction dialog
  - `OpenSceneFilters` - Open scene filters dialog

**Video Capture Device Management**
- **Bulk Device Operations**: New WebSocket commands for comprehensive camera management
  - `ActivateAllVideoCaptureDevices` - Enable all video capture devices
  - `DeactivateAllVideoCaptureDevices` - Disable all video capture devices
  - `RefreshAllVideoCaptureDevices` - Refresh all video capture device connections

**Advanced Source Properties API**
- **Detailed Source Control**: New WebSocket API for granular source management
  - `GetBlendingMethod` / `SetBlendingMethod` - Control source blending modes
  - `GetDeinterlacing` / `SetDeinterlacing` - Manage deinterlacing settings
  - `GetScaleFiltering` / `SetScaleFiltering` - Control scale filtering options
  - `GetDownmixMono` / `SetDownmixMono` - Audio downmix controls

**Transition Management**
- **Enhanced Transitions**: Advanced show/hide transition controls
  - `GetShowTransition` / `SetShowTransition` - Manage source show transitions
  - `GetHideTransition` / `SetHideTransition` - Manage source hide transitions

**Legacy Support & Migration**
- **Backward Compatibility**: All legacy WebSocket command names remain functional
- **Deprecation System**: Proper deprecation warnings for old commands with migration guidance
- **Enhanced Error Handling**: Improved error responses with detailed debugging information
- **Parameter Validation**: Enhanced input validation for all WebSocket commands

### 🎨 Revolutionary User Interface

**Modern Splash Screen System**
- **Dynamic Welcome Screen** (`ui/splash-screen.cpp/.hpp`): Complete redesign with rich content
  - Dynamic patch notes loading from markdown files with real-time rendering
  - Comprehensive support information with donation links and community resources
  - Social media integration (Twitter, Bluesky, Doras) with live links
  - Supporter acknowledgments section with community recognition
  - Responsive design with smooth scrolling and modern styling
  - Force show capability for development testing and user preference

**Enhanced Dock System**
- **Redesigned StreamUP Dock** (`ui/dock/streamup-dock.cpp/.hpp`): Complete dock overhaul
  - Comprehensive dock customization options for personalized workflow
  - Resizable dock components for optimal screen space utilization
  - Customizable quick action buttons with drag-and-drop layout
  - Enhanced dock widget appearance with modern styling
  - Direct video capture device controls integrated into dock interface
  - Quick activate/deactivate all video capture devices from dock
  - One-click refresh all video capture devices functionality
  - Streamlined camera management workflow

**WebSocket Configuration Interface**
- **Dedicated WebSocket Window**: New comprehensive WebSocket management interface
  - Real-time WebSocket connection status monitoring with visual indicators
  - Interactive WebSocket command testing and validation environment
  - Easy copying of WebSocket command examples with syntax highlighting
  - Comprehensive WebSocket API documentation integrated into interface
  - Command history and favorites for frequently used commands

**Internationalization Framework**
- **Localization Support** (`data/locale/en-US.ini`): Foundation for multi-language support
  - Complete English (US) locale implementation
  - Structured framework for additional language support
  - Localized UI strings and messages throughout the interface
  - Support for RTL languages and cultural formatting preferences

### 🔧 Advanced Technical Improvements

**Enhanced Error Handling System**
- **Centralized ErrorHandler** (`utilities/error-handler.cpp/.hpp`): Comprehensive error management
  - Categorized error logging with severity levels and context
  - Improved error reporting with actionable user feedback
  - Better integration with OBS logging system
  - Enhanced debugging capabilities for development

**Utility Library Expansion**
- **String Utilities** (`utilities/string-utils.cpp/.hpp`): Advanced string manipulation functions
- **Path Utilities** (`utilities/path-utils.cpp/.hpp`): Cross-platform path handling and resolution
- **Version Utilities** (`utilities/version-utils.cpp/.hpp`): Version comparison and management
- **HTTP Client** (`utilities/http-client.cpp/.hpp`): Network operations for plugin updates and communication
- **OBS Wrappers** (`utilities/obs-wrappers.hpp`): Simplified OBS API interaction layer

**UI Helper Framework**
- **UIHelpers** (`ui/ui-helpers.cpp/.hpp`): Reusable UI components and dialog systems
  - Thread-safe UI operations for better stability
  - Standardized dialog appearance across the plugin
  - Enhanced icon integration throughout the interface
  - Responsive design supporting different screen sizes and DPI settings

**Build System Enhancements**
- **Modular CMake Structure**: Organized build system with Visual Studio source groups
- **Resource Management**: Enhanced resource handling and embedding
- **Cross-Platform Compatibility**: Improved support for Windows, macOS, and Linux builds

### 🚀 Performance & Stability

**Memory Management**
- **Resource Optimization**: Improved memory usage and cleanup processes
- **Thread Safety**: Enhanced thread safety for all UI operations
- **Startup Performance**: Optimized plugin initialization and loading sequence

**Robust Error Recovery**
- **Graceful Degradation**: Better handling of error conditions without plugin crashes
- **State Management**: Improved state consistency across module boundaries
- **Recovery Mechanisms**: Automatic recovery from common error scenarios

### 🔌 Enhanced Integration

**OBS Studio Integration**
- **Frontend API**: Deeper integration with OBS Studio's frontend API
- **Plugin Ecosystem**: Better compatibility with other OBS plugins
- **Event Handling**: Enhanced event system integration for real-time updates

**External Compatibility**
- **obs-websocket**: Enhanced compatibility with obs-websocket plugin
- **Third-Party Tools**: Improved integration points for external applications
- **API Standardization**: Consistent API patterns following OBS conventions

### 🛠️ Developer Experience

**Development Tools**
- **Enhanced Debugging**: Comprehensive logging with categorization and filtering
- **Development Mode**: Special development features for testing and validation
- **Code Organization**: Better namespace organization and header management
- **Documentation**: Enhanced inline documentation and code comments

**Maintenance Improvements**
- **Modular Testing**: Individual module testing capabilities
- **Code Quality**: Improved code structure and maintainability
- **Dependency Management**: Cleaner dependency relationships between modules

---

*StreamUP v2.0.0 represents a complete ground-up redesign of the plugin architecture, delivering enhanced performance, better user experience, and a solid foundation for future development. This major version introduces breaking changes in the internal architecture while maintaining backward compatibility for user workflows and WebSocket API consumers.*