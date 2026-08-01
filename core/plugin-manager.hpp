#ifndef STREAMUP_PLUGIN_MANAGER_HPP
#define STREAMUP_PLUGIN_MANAGER_HPP

#include <QString>
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace StreamUP {
namespace PluginManager {

//-------------------ERROR HANDLING FUNCTIONS-------------------
/**
 * Display an error dialog with a given message
 * @param errorMessage The error message to display
 */
void ErrorDialog(const QString &errorMessage);

/**
 * Show dialog indicating plugins are up to date
 * @param manuallyTriggered Whether the check was manually triggered by user
 */
void PluginsUpToDateOutput(bool manuallyTriggered);

/**
 * Show dialog indicating there are plugin issues
 * @param missing_modules Map of missing plugins (name -> required version)
 * @param version_mismatch_modules Map of outdated plugins (name -> installed version)
 * @param failed_to_load_modules Vector of module names that failed to load
 * @param continueCallback Optional callback to execute when "Continue Anyway" is pressed
 * @param isStartupCheck Whether this is being shown during startup check (shows skip checkbox)
 * @param disabled_modules Map of plugins switched off in OBS's own plugin manager (name -> true if blocked by safe mode)
 */
void PluginsHaveIssue(const std::map<std::string, std::string>& missing_modules, const std::map<std::string, std::string>& version_mismatch_modules, const std::vector<std::string>& failed_to_load_modules, std::function<void()> continueCallback = nullptr, bool isStartupCheck = false, const std::map<std::string, bool>& disabled_modules = std::map<std::string, bool>());

//-------------------PLUGIN UPDATE FUNCTIONS-------------------
/**
 * Check all plugins for updates and display results
 * @param manuallyTriggered Whether the check was manually triggered by user
 */
void CheckAllPluginsForUpdates(bool manuallyTriggered);

// HTTP functionality moved to StreamUP::HttpClient module

//-------------------PLUGIN INITIALIZATION FUNCTIONS-------------------
/**
 * Initialize required modules list from remote API
 */
void InitialiseRequiredModules();

/**
 * Check if all required OBS plugins are installed and up to date (without showing UI)
 * @param isLoadStreamUpFile Whether this check is for loading a .StreamUP file
 * @return bool True if all required plugins are present and up to date
 */
bool CheckrequiredOBSPluginsWithoutUI(bool isLoadStreamUpFile = false);

/**
 * Check if all required OBS plugins are installed and up to date
 * @param isLoadStreamUpFile Whether this check is for loading a .StreamUP file
 * @return bool True if all required plugins are present and up to date
 */
bool CheckrequiredOBSPlugins(bool isLoadStreamUpFile = false);

//-------------------UTILITY FUNCTIONS-------------------
/**
 * Search for a version string in a log file (plugin-specific version extraction)
 * @param path Path to the log file directory  
 * @param search The search string to look for in the log
 * @return std::string The version number found, or empty string if not found
 */
std::string SearchStringInFileForVersion(const char *path, const char *search);

/**
 * Search for StreamUP theme version in theme files
 * @param search The search string to look for in theme files
 * @return std::string The version number found, or empty string if not found
 */
std::string SearchThemeFileForVersion(const char *search);

/**
 * Get installed plugins list with versions
 * @return std::vector containing pairs of plugin names and versions
 */
std::vector<std::pair<std::string, std::string>> GetInstalledPlugins();

//-------------------UPDATE-CHECK ELIGIBILITY-------------------
/**
 * Whether a plugin's search string opts it out of version checking.
 *
 * True for the explicit "[ignore]" marker, and also for an empty search
 * string. Empty must never reach the log scan: std::string::find("") matches
 * at every byte of the log, so a blank field would report whatever number the
 * version regex hit first rather than finding nothing.
 *
 * @param searchString The plugin's search string from the database
 * @return bool True if the plugin cannot or should not be version checked
 */
bool IsUpdateCheckSkipped(const std::string &searchString);

/**
 * Human-readable reason a plugin is skipped, for display in the UI.
 * Returns the text following "[ignore]" in the search string, or an empty
 * string if no reason was given.
 * @param searchString The plugin's search string from the database
 * @return std::string The reason text, trimmed
 */
std::string GetUpdateCheckSkipReason(const std::string &searchString);

/**
 * Plugins that are in our database and loaded in OBS, but that cannot have
 * their version checked (see IsUpdateCheckSkipped). Without this they show in
 * neither the compatible nor the incompatible list, so they'd silently vanish
 * from the Installed Plugins table.
 * @return std::vector of (plugin name, reason) pairs
 */
std::vector<std::pair<std::string, std::string>> GetUncheckablePlugins();

//-------------------EFFICIENT CACHING FUNCTIONS-------------------
/**
 * Perform plugin check once and cache results for future use
 * This should be called after OBS is fully loaded
 * @param checkAllPlugins If true, check all plugins; if false, only check required plugins
 */
void PerformPluginCheckAndCache(bool checkAllPlugins = true);

/**
 * Check if all plugins are up to date using cached results
 * @return bool True if cached results show all plugins are up to date
 */
bool IsAllPluginsUpToDateCached();

/**
 * Show plugin issues dialog using cached results
 * @param continueCallback Optional callback to execute when "Continue Anyway" is pressed
 */
void ShowCachedPluginIssuesDialog(std::function<void()> continueCallback = nullptr);

/**
 * Show plugin updates dialog using cached results (for manual update checks)
 */
void ShowCachedPluginUpdatesDialog();

/**
 * Show plugin updates dialog only if there are updates (silent if up to date)
 * Used for startup checks to avoid showing "everything is fine" notifications
 */
void ShowCachedPluginUpdatesDialogSilent();

/**
 * Invalidate cached plugin status (call when plugins might have changed)
 */
void InvalidatePluginCache();

/**
 * Get installed plugins from cached results (much faster than scanning files)
 * @return std::vector containing pairs of plugin names and versions from cache
 */
std::vector<std::pair<std::string, std::string>> GetInstalledPluginsCached();

//-------------------UI HELPER FUNCTIONS-------------------
/**
 * Get forum/general URL for a specific plugin
 * @param pluginName The name of the plugin to get URL for
 * @return QString The forum/general URL for the plugin
 */
QString GetPluginForumLink(const std::string &pluginName);

/**
 * Get platform-specific download URL for a plugin
 * @param pluginName The name of the plugin to get URL for  
 * @return QString The platform-appropriate download URL
 */
QString GetPluginPlatformURL(const std::string &pluginName);

/**
 * Every third-party module in the log's "Loaded Modules:" section, whether or
 * not we have a database entry for it. Excludes modules that ship with OBS.
 * @param logPath Path to the OBS log directory
 * @return std::vector List of loaded module names
 */
std::vector<std::string> GetLoadedModuleNamesFromLog(const char *logPath);

/**
 * Search for loaded modules in OBS log files (excludes default OBS modules)
 * Narrows GetLoadedModuleNamesFromLog to modules with no database entry.
 * @param logPath Path to the OBS log directory
 * @return std::vector List of non-standard loaded module names
 */
std::vector<std::string> SearchLoadedModulesInLogFile(const char *logPath);

/**
 * Search for modules that failed to load in OBS log files
 * @param logPath Path to the OBS log directory
 * @return std::vector List of module names (DLL/SO names) that failed to load
 */
std::vector<std::string> SearchFailedToLoadModulesInLogFile(const char *logPath);

/**
 * Details about why a module failed to load, harvested from the OBS log.
 * The OBS log already records both the module's on-disk path and (usually) the
 * reason it wouldn't load on the line right before "Module '...' not loaded" -
 * this struct just surfaces what would otherwise be discarded.
 */
struct ModuleLoadFailure {
	std::string modulePath;      // path as written in the log (may be relative)
	std::string absoluteFolder;  // resolved folder containing the module (for "Open Folder")
	std::string absoluteModulePath; // resolved absolute path to the module file (for reveal/select)
	std::string reason;          // best-effort human-readable reason (empty if none found)
};

/**
 * Parse the most recent OBS log for modules that failed to load, capturing the
 * module's folder path and the failure reason for each. Keyed by the module's
 * base name (no extension) so it lines up with SearchFailedToLoadModulesInLogFile.
 * @param logPath Path to the OBS log directory
 * @return std::map moduleName -> ModuleLoadFailure
 */
std::map<std::string, ModuleLoadFailure> GetModuleLoadFailures(const char *logPath);

/**
 * Search the most recent OBS log for modules OBS refused to load because they
 * are switched off - either toggled off by the user in OBS's own plugin manager
 * or held back by safe mode. Such a module never logs its version line, so
 * without this it would be misreported as "missing".
 * @param logPath Path to the OBS log directory
 * @return std::map moduleName (base name, no extension) -> true if blocked by safe mode
 */
std::map<std::string, bool> SearchDisabledModulesInLogFile(const char *logPath);

} // namespace PluginManager
} // namespace StreamUP

#endif // STREAMUP_PLUGIN_MANAGER_HPP
