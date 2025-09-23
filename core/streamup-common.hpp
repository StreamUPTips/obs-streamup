#ifndef STREAMUP_COMMON_HPP
#define STREAMUP_COMMON_HPP

#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-source.h>
#include <QString>
// QSystemTrayIcon now handled by NotificationManager module
#include <map>
#include <string>

// Forward declarations
class QMainWindow;
class QWidget;
class QLayout;

namespace StreamUP {

// Common structures used across modules
struct PluginInfo {
	std::string name;
	std::string version;
	std::string searchString;
	std::string windowsURL;
	std::string macURL;
	std::string linuxURL;
	std::string generalURL;
	std::string moduleName;
	bool required;
};

// RequestData moved to StreamUP::HttpClient module

// SystemTrayNotification struct moved to StreamUP::NotificationManager module

struct SceneItemEnumData {
	bool isAnySourceSelected = false;
	const char *selectedSourceName = nullptr;
};

// Global plugin state moved to StreamUP::PluginState class

// Platform detection
#if defined(_WIN32)
#define STREAMUP_PLATFORM_NAME "windows"
#elif defined(__APPLE__)
#define STREAMUP_PLATFORM_NAME "macos"  
#elif defined(__linux__)
#define STREAMUP_PLATFORM_NAME "linux"
#else
#define STREAMUP_PLATFORM_NAME "unknown"
#endif

// Common Qt utility macros
#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

// Advanced mask filter settings
#define ADVANCED_MASKS_SETTINGS_SIZE 15
extern const char *advanced_mask_settings[ADVANCED_MASKS_SETTINGS_SIZE];

} // namespace StreamUP

#endif // STREAMUP_COMMON_HPP
