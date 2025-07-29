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

} // namespace HotkeyManager
} // namespace StreamUP

#endif // STREAMUP_HOTKEY_MANAGER_HPP