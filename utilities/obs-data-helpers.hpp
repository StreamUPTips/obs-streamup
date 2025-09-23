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

/**
 * Get the standard StreamUP config file path
 * @param filename The config filename (e.g., "configs.json")
 * @return char* Full path (caller must call bfree)
 */
char* GetConfigFilePath(const char* filename);

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

/**
 * Get double value with fallback default
 * @param data The obs_data object
 * @param key The key to look up
 * @param defaultValue Value to return if key not found or data is null
 * @return double The value or default
 */
double GetDoubleWithDefault(obs_data_t* data, const char* key, double defaultValue);

//-------------------SAFE SETTERS WITH VALIDATION-------------------

/**
 * Set string value with null/empty checking
 * @param data The obs_data object
 * @param key The key to set
 * @param value The string value (can be null)
 * @param allowEmpty If false, null/empty strings won't be set
 * @return bool True if value was set
 */
bool SetStringIfValid(obs_data_t* data, const char* key, const char* value, bool allowEmpty = true);

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

/**
 * Set a standardized error response
 * @param response The response data object
 * @param errorMessage The error message to include
 */
void SetErrorResponse(obs_data_t* response, const char* errorMessage);

/**
 * Set a standardized success response
 * @param response The response data object
 * @param statusMessage Optional status message (can be null)
 */
void SetSuccessResponse(obs_data_t* response, const char* statusMessage = nullptr);

/**
 * Set a boolean response with optional success flag
 * @param response The response data object
 * @param key The key to set
 * @param value The boolean value
 * @param includeSuccess Whether to include success=true field
 */
void SetBoolResponse(obs_data_t* response, const char* key, bool value, bool includeSuccess = true);

//-------------------SOURCE VALIDATION HELPERS-------------------

/**
 * Validate and get a source from WebSocket request data
 * Automatically sets error response if validation fails
 * @param request The request data object
 * @param response The response data object (for error messages)
 * @param paramName The parameter name to check (default: "sourceName")
 * @return obs_source_t* Valid source or nullptr (caller must release if non-null)
 */
obs_source_t* ValidateAndGetSource(obs_data_t* request, obs_data_t* response, const char* paramName = "sourceName");

/**
 * Validate and get a scene from WebSocket request data
 * Automatically sets error response if validation fails
 * @param request The request data object
 * @param response The response data object (for error messages)  
 * @param paramName The parameter name to check (default: "sceneName")
 * @return obs_scene_t* Valid scene or nullptr (caller must release if non-null)
 */
obs_scene_t* ValidateAndGetScene(obs_data_t* request, obs_data_t* response, const char* paramName = "sceneName");

} // namespace OBSDataHelpers
} // namespace StreamUP

#endif // STREAMUP_OBS_DATA_HELPERS_HPP
