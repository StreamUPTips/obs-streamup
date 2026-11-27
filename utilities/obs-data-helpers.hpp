#ifndef STREAMUP_OBS_DATA_HELPERS_HPP
#define STREAMUP_OBS_DATA_HELPERS_HPP

#include <obs.h>
#include <obs-data.h>
#include <functional>

namespace StreamUP {
namespace OBSDataHelpers {

//-------------------SETTINGS FILE I/O-------------------

/**
 * Load settings from a file with automatic default creation
 * If file doesn't exist or is invalid, creates new obs_data and applies defaults
 * @param filename Path to settings file
 * @param setDefaults Function to set default values (called if file not found)
 * @return obs_data_t* Settings data (caller must call obs_data_release)
 */
obs_data_t* LoadSettingsWithDefaults(const char* filename, const std::function<void(obs_data_t*)>& setDefaults);

/**
 * Save settings to a file with error handling
 * @param settings The settings data to save
 * @param filename Path to save settings to
 * @return bool True on success, false on error
 */
bool SaveSettingsToFile(obs_data_t* settings, const char* filename);

//-------------------SAFE GETTERS WITH DEFAULTS-------------------

/**
 * Get boolean value with fallback default
 * @param data The obs_data object
 * @param key The key to look up
 * @param defaultValue Value to return if key not found or data is null
 * @return bool The value or default
 */
bool GetBoolWithDefault(obs_data_t* data, const char* key, bool defaultValue);

/**
 * Get string value with fallback default  
 * @param data The obs_data object
 * @param key The key to look up
 * @param defaultValue Value to return if key not found, empty, or data is null
 * @return const char* The value or default (do not free)
 */
const char* GetStringWithDefault(obs_data_t* data, const char* key, const char* defaultValue);

/**
 * Get integer value with fallback default
 * @param data The obs_data object
 * @param key The key to look up
 * @param defaultValue Value to return if key not found or data is null
 * @return int The value or default
 */
int GetIntWithDefault(obs_data_t* data, const char* key, int defaultValue);

//-------------------SAFE SETTERS WITH VALIDATION-------------------

//-------------------HOTKEY HELPERS-------------------

/**
 * Save a hotkey to obs_data
 * @param data The obs_data object to save to
 * @param key The key name to use
 * @param hotkeyId The hotkey ID to save
 * @return bool True on success
 */
bool SaveHotkeyToData(obs_data_t* data, const char* key, obs_hotkey_id hotkeyId);

/**
 * Load a hotkey from obs_data
 * @param data The obs_data object to load from
 * @param key The key name to use
 * @param hotkeyId The hotkey ID to load into
 * @return bool True on success
 */
bool LoadHotkeyFromData(obs_data_t* data, const char* key, obs_hotkey_id hotkeyId);

//-------------------WEBSOCKET RESPONSE HELPERS-------------------

//-------------------SOURCE VALIDATION HELPERS-------------------

} // namespace OBSDataHelpers
} // namespace StreamUP

#endif // STREAMUP_OBS_DATA_HELPERS_HPP
