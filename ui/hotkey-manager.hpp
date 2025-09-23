#ifndef STREAMUP_HOTKEY_MANAGER_HPP
#define STREAMUP_HOTKEY_MANAGER_HPP

#include <obs.h>
#include <obs-data.h>

namespace StreamUP {
namespace HotkeyManager {

//-------------------HOTKEY HANDLERS-------------------
/**
 * Hotkey handler for refreshing browser sources
 * @param data User data (unused)
 * @param id Hotkey ID
 * @param hotkey Hotkey object
 * @param pressed Whether the key was pressed (true) or released (false)
 */
void HotkeyRefreshBrowserSources(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

/**
 * Hotkey handler for locking/unlocking all sources
 * @param data User data (unused)
 * @param id Hotkey ID
 * @param hotkey Hotkey object
 * @param pressed Whether the key was pressed (true) or released (false)
 */
void HotkeyLockAllSources(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

/**
 * Hotkey handler for refreshing audio monitoring
 * @param data User data (unused)
 * @param id Hotkey ID
 * @param hotkey Hotkey object
 * @param pressed Whether the key was pressed (true) or released (false)
 */
void HotkeyRefreshAudioMonitoring(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

/**
 * Hotkey handler for locking/unlocking sources in current scene
 * @param data User data (unused)
 * @param id Hotkey ID
 * @param hotkey Hotkey object
 * @param pressed Whether the key was pressed (true) or released (false)
 */
void HotkeyLockCurrentSources(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

/**
 * Hotkey handler for opening source properties
 * @param data User data (unused)
 * @param id Hotkey ID
 * @param hotkey Hotkey object
 * @param pressed Whether the key was pressed (true) or released (false)
 */
void HotkeyOpenSourceProperties(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

/**
 * Hotkey handler for opening source filters
 * @param data User data (unused)
 * @param id Hotkey ID
 * @param hotkey Hotkey object
 * @param pressed Whether the key was pressed (true) or released (false)
 */
void HotkeyOpenSourceFilters(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

/**
 * Hotkey handler for opening source interact
 * @param data User data (unused)
 * @param id Hotkey ID
 * @param hotkey Hotkey object
 * @param pressed Whether the key was pressed (true) or released (false)
 */
void HotkeyOpenSourceInteract(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

/**
 * Hotkey handler for opening scene filters
 * @param data User data (unused)
 * @param id Hotkey ID
 * @param hotkey Hotkey object
 * @param pressed Whether the key was pressed (true) or released (false)
 */
void HotkeyOpenSceneFilters(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

/**
 * Hotkey handler for activating all video capture devices
 * @param data User data (unused)
 * @param id Hotkey ID
 * @param hotkey Hotkey object
 * @param pressed Whether the key was pressed (true) or released (false)
 */
void HotkeyActivateAllVideoCaptureDevices(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

/**
 * Hotkey handler for deactivating all video capture devices
 * @param data User data (unused)
 * @param id Hotkey ID
 * @param hotkey Hotkey object
 * @param pressed Whether the key was pressed (true) or released (false)
 */
void HotkeyDeactivateAllVideoCaptureDevices(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

/**
 * Hotkey handler for refreshing all video capture devices
 * @param data User data (unused)
 * @param id Hotkey ID
 * @param hotkey Hotkey object
 * @param pressed Whether the key was pressed (true) or released (false)
 */
void HotkeyRefreshAllVideoCaptureDevices(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

//-------------------HOTKEY MANAGEMENT-------------------
/**
 * Save and load hotkey settings
 * @param save_data Data to save/load
 * @param saving Whether we're saving (true) or loading (false)
 * @param param User parameter (unused)
 */
void SaveLoadHotkeys(obs_data_t *save_data, bool saving, void *param);

//-------------------HOTKEY REGISTRATION-------------------
/**
 * Register all hotkeys with OBS
 */
void RegisterHotkeys();

/**
 * Unregister all hotkeys from OBS
 */
void UnregisterHotkeys();

/**
 * Reset all StreamUP hotkeys to have no key combinations assigned
 */
void ResetAllHotkeys();

/**
 * Get the current hotkey combination for a specific StreamUP hotkey
 * @param hotkeyName The OBS hotkey name (e.g., "streamup_refresh_browser_sources")
 * @return obs_data_array_t* Array containing the current key combination (caller must release)
 */
obs_data_array_t* GetHotkeyBinding(const char* hotkeyName);

/**
 * Set a hotkey combination for a specific StreamUP hotkey
 * @param hotkeyName The OBS hotkey name (e.g., "streamup_refresh_browser_sources") 
 * @param keyData Array containing the key combination to set
 */
void SetHotkeyBinding(const char* hotkeyName, obs_data_array_t* keyData);

/**
 * Get the hotkey ID for a given hotkey name
 * @param hotkeyName The OBS hotkey name (e.g., "streamup_refresh_browser_sources")
 * @return obs_hotkey_id The corresponding hotkey ID, or OBS_INVALID_HOTKEY_ID if not found
 */
obs_hotkey_id GetHotkeyId(const char* hotkeyName);

} // namespace HotkeyManager
} // namespace StreamUP

#endif // STREAMUP_HOTKEY_MANAGER_HPP
