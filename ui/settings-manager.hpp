#pragma once

#include <obs-data.h>
#include <map>
#include <string>
#include <vector>

// Forward declarations
class QScrollArea;
class QWidget;
class QDialog;
class QTableWidget;

namespace StreamUP {
namespace UIStyles {
    struct StandardDialogComponents;
}
}

namespace StreamUP {
namespace SettingsManager {

/**
 * @brief Dock tool visibility settings
 */
struct DockToolSettings {
    bool showLockAllSources;
    bool showLockCurrentSources;
    bool showRefreshBrowserSources;
    bool showRefreshAudioMonitoring;
    bool showVideoCaptureOptions;
    bool showGroupSelectedSources;
    bool showToggleVisibilitySelectedSources;

    DockToolSettings() :
        showLockAllSources(true),
        showLockCurrentSources(true),
        showRefreshBrowserSources(true),
        showRefreshAudioMonitoring(true),
        showVideoCaptureOptions(true),
        showGroupSelectedSources(true),
        showToggleVisibilitySelectedSources(true) {}
};

/**
 * @brief Toolbar position options
 */
enum class ToolbarPosition {
    Top,
    Bottom,
    Left,
    Right
};

/**
 * @brief Scene switching click mode options
 */
enum class SceneSwitchMode {
    SingleClick,
    DoubleClick
};

/**
 * @brief Scene organizer sort method options
 */
enum class SceneSortMethod {
    None,
    AlphabeticalAZ,
    AlphabeticalZA,
    NewestFirst,
    OldestFirst
};

/**
 * @brief Settings structure to hold plugin configuration
 */
struct PluginSettings {
    bool runAtStartup;
    bool notificationsMute;
    bool showCPHIntegration;
    bool showToolbar;
    bool debugLoggingEnabled;
    bool sceneOrganiserShowIcons;
    bool sceneOrganiserGroupFolders;
    bool sceneOrganiserRememberFolderState;
    bool sceneOrganiserDisablePreviewSwitchingInStudioMode; // Disable preview switching (single-click) when in studio mode
    bool sceneOrganiserDisableTransitionInStudioMode; // Disable transition trigger (double-click) when in studio mode
    int sceneOrganiserItemHeight; // Height multiplier percentage (50-200, default 100)
    SceneSwitchMode sceneOrganiserSwitchMode;
    SceneSortMethod sceneOrganiserSortMethod;
    ToolbarPosition toolbarPosition;
    DockToolSettings dockTools;
    int toolbarIconSize;            // Icon size (10-24 pixels, default 16)

    PluginSettings() : runAtStartup(true), notificationsMute(false), showCPHIntegration(true), showToolbar(true), debugLoggingEnabled(false), sceneOrganiserShowIcons(true), sceneOrganiserGroupFolders(true), sceneOrganiserRememberFolderState(true), sceneOrganiserDisablePreviewSwitchingInStudioMode(false), sceneOrganiserDisableTransitionInStudioMode(false), sceneOrganiserItemHeight(100), sceneOrganiserSwitchMode(SceneSwitchMode::SingleClick), sceneOrganiserSortMethod(SceneSortMethod::None), toolbarPosition(ToolbarPosition::Top), toolbarIconSize(16) {}
};

/**
 * @brief Load settings from configuration file
 * @return obs_data_t* Pointer to loaded settings data (caller must release)
 */
obs_data_t* LoadSettings();

/**
 * @brief Save settings to configuration file
 * @param settings The settings data to save
 * @return bool True if save was successful, false otherwise
 */
bool SaveSettings(obs_data_t* settings);

/**
 * @brief Get current plugin settings as a structure
 * @return PluginSettings Current settings values
 */
PluginSettings GetCurrentSettings();

/**
 * @brief Update plugin settings from structure
 * @param settings The new settings to apply
 */
void UpdateSettings(const PluginSettings& settings);

/**
 * @brief Initialize settings system (creates default config if needed)
 */
void InitializeSettingsSystem();

/**
 * @brief Show the settings dialog window
 */
void ShowSettingsDialog();

/**
 * @brief Show the settings dialog window with a specific tab selected
 * @param tabIndex The index of the tab to open (0=General, 1=Toolbar Settings, etc.)
 */
void ShowSettingsDialog(int tabIndex);

/**
 * @brief Check if notifications are currently muted
 * @return bool True if notifications are muted
 */
bool AreNotificationsMuted();

/**
 * @brief Set notification mute state
 * @param muted True to mute notifications, false to enable them
 */
void SetNotificationsMuted(bool muted);

/**
 * @brief Check if CPH integration is enabled
 * @return bool True if CPH integration should be shown
 */
bool IsCPHIntegrationEnabled();

/**
 * @brief Check if debug logging is currently enabled
 * @return bool True if debug logging is enabled
 */
bool IsDebugLoggingEnabled();

/**
 * @brief Set debug logging state
 * @param enabled True to enable debug logging, false to disable it
 */
void SetDebugLoggingEnabled(bool enabled);

/**
 * @brief Show the installed plugins page inline within the same window
 * @param components The dialog components from the template system
 */
void ShowInstalledPluginsInline(const StreamUP::UIStyles::StandardDialogComponents& components);

/**
 * @brief Show the installed plugins page in a separate window
 * @param parentWidget Optional parent widget for proper positioning
 */
void ShowInstalledPluginsPage(QWidget* parentWidget = nullptr);

/**
 * @brief Show the hotkeys management page inline within the same window
 * @param components The dialog components from the template system
 */
void ShowHotkeysInline(const StreamUP::UIStyles::StandardDialogComponents& components);

/**
 * @brief Get current dock tool settings
 * @return DockToolSettings Current dock tool visibility configuration
 */
DockToolSettings GetDockToolSettings();

/**
 * @brief Update dock tool settings
 * @param dockSettings The new dock tool settings to apply
 */
void UpdateDockToolSettings(const DockToolSettings& dockSettings);

/**
 * @brief Show dock configuration settings inline within the same window
 * @param components The dialog components from the template system
 */
void ShowDockConfigInline(const StreamUP::UIStyles::StandardDialogComponents& components);

/**
 * Invalidate settings cache (forces reload on next access)
 */
void InvalidateSettingsCache();

/**
 * Clean up cached settings on plugin shutdown
 */
void CleanupSettingsCache();

/**
 * @brief Save skipped plugin updates to settings
 * @param outdatedPlugins Map of outdated plugins (name -> required/available version that was skipped)
 * @param failedPlugins Vector of failed to load plugin module names
 */
void SaveSkippedUpdates(const std::map<std::string, std::string>& outdatedPlugins, const std::vector<std::string>& failedPlugins);

/**
 * @brief Get skipped plugin updates from settings
 * @param outdatedPlugins Output map of outdated plugins that were skipped (name -> required version)
 * @param failedPlugins Output vector of failed plugins that were skipped
 */
void GetSkippedUpdates(std::map<std::string, std::string>& outdatedPlugins, std::vector<std::string>& failedPlugins);

/**
 * @brief Clear skipped plugin updates from settings
 */
void ClearSkippedUpdates();

/**
 * @brief Check if current updates match the skipped updates
 * @param currentOutdated Current outdated plugins map (name -> required version)
 * @param currentFailed Current failed to load plugins vector
 * @return bool True if current updates exactly match skipped updates
 */
bool AreUpdatesSkipped(const std::map<std::string, std::string>& currentOutdated, const std::vector<std::string>& currentFailed);

} // namespace SettingsManager
} // namespace StreamUP

// Global functions for toolbar management (defined in streamup.cpp)
extern void ApplyToolbarVisibility();
extern void ApplyToolbarPosition();
