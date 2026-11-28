#include "obs-data-helpers.hpp"
#include "path-utils.hpp"
#include "error-handler.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <string>
#include <cstring>

namespace StreamUP {
namespace OBSDataHelpers {

//-------------------SETTINGS FILE I/O-------------------

obs_data_t* LoadSettingsWithDefaults(const char* filename, const std::function<void(obs_data_t*)>& setDefaults)
{
    if (!filename) {
        ErrorHandler::LogError("LoadSettingsWithDefaults: filename is null", ErrorHandler::Category::FileSystem);
        return nullptr;
    }

    char* configPath = StreamUP::PathUtils::GetOBSConfigPath(filename);
    if (!configPath) {
        ErrorHandler::LogError("LoadSettingsWithDefaults: Failed to get config path", ErrorHandler::Category::FileSystem);
        return nullptr;
    }

    obs_data_t* data = obs_data_create_from_json_file(configPath);
    if (!data) {
        ErrorHandler::LogInfo("Settings file not found, creating defaults: " + std::string(filename), ErrorHandler::Category::FileSystem);
        data = obs_data_create();
        if (data && setDefaults) {
            setDefaults(data);
        }
    }

    bfree(configPath);
    return data;
}

bool SaveSettingsToFile(obs_data_t* settings, const char* filename)
{
    if (!settings || !filename) {
        ErrorHandler::LogError("SaveSettingsToFile: Invalid parameters", ErrorHandler::Category::General);
        return false;
    }

    char* configPath = StreamUP::PathUtils::GetOBSConfigPath(filename);
    if (!configPath) {
        ErrorHandler::LogError("SaveSettingsToFile: Failed to get config path", ErrorHandler::Category::FileSystem);
        return false;
    }

    bool success = obs_data_save_json_safe(settings, configPath, "tmp", "bak");
    if (!success) {
        ErrorHandler::LogError("SaveSettingsToFile: Failed to save to " + std::string(configPath), ErrorHandler::Category::FileSystem);
    }

    bfree(configPath);
    return success;
}

//-------------------SAFE GETTERS WITH DEFAULTS-------------------

bool GetBoolWithDefault(obs_data_t* data, const char* key, bool defaultValue)
{
    if (!data || !key) {
        return defaultValue;
    }
    if (!obs_data_has_user_value(data, key)) {
        return defaultValue;
    }
    return obs_data_get_bool(data, key);
}

const char* GetStringWithDefault(obs_data_t* data, const char* key, const char* defaultValue)
{
    if (!data || !key) {
        return defaultValue ? defaultValue : "";
    }

    const char* value = obs_data_get_string(data, key);
    if (!value || strlen(value) == 0) {
        return defaultValue ? defaultValue : "";
    }
    
    return value;
}

int GetIntWithDefault(obs_data_t* data, const char* key, int defaultValue)
{
    if (!data || !key) {
        return defaultValue;
    }
    if (!obs_data_has_user_value(data, key)) {
        return defaultValue;
    }
    return static_cast<int>(obs_data_get_int(data, key));
}

//-------------------SAFE SETTERS WITH VALIDATION-------------------

//-------------------HOTKEY HELPERS-------------------

bool SaveHotkeyToData(obs_data_t* data, const char* key, obs_hotkey_id hotkeyId)
{
    if (!data || !key || hotkeyId == OBS_INVALID_HOTKEY_ID) {
        return false;
    }

    obs_data_array_t* hotkeyArray = obs_hotkey_save(hotkeyId);
    if (!hotkeyArray) {
        return false;
    }

    obs_data_set_array(data, key, hotkeyArray);
    obs_data_array_release(hotkeyArray);
    return true;
}

bool LoadHotkeyFromData(obs_data_t* data, const char* key, obs_hotkey_id hotkeyId)
{
    if (!data || !key || hotkeyId == OBS_INVALID_HOTKEY_ID) {
        return false;
    }

    obs_data_array_t* hotkeyArray = obs_data_get_array(data, key);
    if (!hotkeyArray) {
        return false;
    }

    obs_hotkey_load(hotkeyId, hotkeyArray);
    obs_data_array_release(hotkeyArray);
    return true;
}

//-------------------WEBSOCKET RESPONSE HELPERS-------------------

//-------------------SOURCE VALIDATION HELPERS-------------------

} // namespace OBSDataHelpers
} // namespace StreamUP
