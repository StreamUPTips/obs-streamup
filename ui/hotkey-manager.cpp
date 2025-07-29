#include "hotkey-manager.hpp"
#include "streamup-common.hpp"
#include "source-manager.hpp"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs-hotkey.h>
#include <QSystemTrayIcon>

// Forward declarations for functions from main streamup.cpp
extern void SendTrayNotification(QSystemTrayIcon::MessageIcon icon, const QString &title, const QString &body);

namespace StreamUP {
namespace HotkeyManager {

//-------------------HOTKEY ID STORAGE-------------------
static obs_hotkey_id refreshBrowserSourcesHotkey = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id refreshAudioMonitoringHotkey = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id lockAllSourcesHotkey = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id lockCurrentSourcesHotkey = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id openSourcePropertiesHotkey = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id openSourceFiltersHotkey = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id openSceneFiltersHotkey = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id openSourceInteractHotkey = OBS_INVALID_HOTKEY_ID;

//-------------------HOTKEY HANDLERS-------------------
void HotkeyRefreshBrowserSources(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);
	if (!pressed)
		return;
	obs_enum_sources(StreamUP::SourceManager::RefreshBrowserSources, nullptr);
	SendTrayNotification(QSystemTrayIcon::Information, obs_module_text("RefreshBrowserSources"),
			     "Action completed successfully.");
}

void HotkeyLockAllSources(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);
	if (!pressed)
		return;
	StreamUP::SourceManager::ToggleLockAllSources();
}

void HotkeyRefreshAudioMonitoring(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);
	if (!pressed)
		return;
	obs_enum_sources(StreamUP::SourceManager::RefreshAudioMonitoring, nullptr);
	SendTrayNotification(QSystemTrayIcon::Information, obs_module_text("RefreshAudioMonitoring"),
			     "Action completed successfully.");
}

void HotkeyLockCurrentSources(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);
	if (!pressed)
		return;
	StreamUP::SourceManager::ToggleLockSourcesInCurrentScene();
}

void HotkeyOpenSourceProperties(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);

	if (!pressed)
		return;

	// Get the name of the currently selected source
	const char *selected_source_name = StreamUP::SourceManager::GetSelectedSourceFromCurrentScene();
	if (!selected_source_name) {
		blog(LOG_INFO, "[StreamUP] No source selected, cannot open properties.");
		return;
	}

	// Find the source by name
	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		// Open the properties of the selected source
		obs_frontend_open_source_properties(selected_source);
		obs_source_release(selected_source);
	} else {
		blog(LOG_INFO, "[StreamUP] Failed to find source: %s", selected_source_name);
	}
}

void HotkeyOpenSourceFilters(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);

	if (!pressed)
		return;

	// Get the name of the currently selected source
	const char *selected_source_name = StreamUP::SourceManager::GetSelectedSourceFromCurrentScene();
	if (!selected_source_name) {
		blog(LOG_INFO, "[StreamUP] No source selected, cannot open filters.");
		return;
	}

	// Find the source by name
	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		// Open the filters of the selected source
		obs_frontend_open_source_filters(selected_source);
		obs_source_release(selected_source);
	} else {
		blog(LOG_INFO, "[StreamUP] Failed to find source: %s", selected_source_name);
	}
}

void HotkeyOpenSourceInteract(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);

	if (!pressed)
		return;

	// Get the name of the currently selected source
	const char *selected_source_name = StreamUP::SourceManager::GetSelectedSourceFromCurrentScene();
	if (!selected_source_name) {
		blog(LOG_INFO, "[StreamUP] No source selected, cannot open interact window.");
		return;
	}

	// Find the source by name
	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		// Open the interact window of the selected source
		obs_frontend_open_source_interaction(selected_source);
		obs_source_release(selected_source); // Release reference count
	} else {
		blog(LOG_INFO, "[StreamUP] Failed to find source: %s", selected_source_name);
	}
}

void HotkeyOpenSceneFilters(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);

	if (!pressed)
		return;

	// Get the current scene
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		blog(LOG_INFO, "[StreamUP] No current scene found, cannot open filters.");
		return;
	}

	// Open the filters of the current scene
	obs_frontend_open_source_filters(current_scene);

	// Release reference count for the current scene
	obs_source_release(current_scene);
}

//-------------------HOTKEY MANAGEMENT-------------------
void SaveLoadHotkeys(obs_data_t *save_data, bool saving, void *param)
{
	UNUSED_PARAMETER(param);
	
	if (saving) {
		// save hotkeys
		obs_data_array_t *hotkeySaveArray;

		hotkeySaveArray = obs_hotkey_save(refreshBrowserSourcesHotkey);
		obs_data_set_array(save_data, "refreshBrowserSourcesHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

		hotkeySaveArray = obs_hotkey_save(lockAllSourcesHotkey);
		obs_data_set_array(save_data, "lockAllSourcesHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

		hotkeySaveArray = obs_hotkey_save(refreshAudioMonitoringHotkey);
		obs_data_set_array(save_data, "refreshAudioMonitoringHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

		hotkeySaveArray = obs_hotkey_save(lockCurrentSourcesHotkey);
		obs_data_set_array(save_data, "lockCurrentSourcesHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

		hotkeySaveArray = obs_hotkey_save(openSourceInteractHotkey);
		obs_data_set_array(save_data, "openSourceInteractHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

		hotkeySaveArray = obs_hotkey_save(openSourcePropertiesHotkey);
		obs_data_set_array(save_data, "openSourcePropertiesHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

		hotkeySaveArray = obs_hotkey_save(openSourceFiltersHotkey);
		obs_data_set_array(save_data, "openSourceFiltersHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

		hotkeySaveArray = obs_hotkey_save(openSceneFiltersHotkey);
		obs_data_set_array(save_data, "openSceneFiltersHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);
	} else {
		// load hotkeys
		obs_data_array_t *hotkeyLoadArray;

		hotkeyLoadArray = obs_data_get_array(save_data, "refreshBrowserSourcesHotkey");
		obs_hotkey_load(refreshBrowserSourcesHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);

		hotkeyLoadArray = obs_data_get_array(save_data, "lockAllSourcesHotkey");
		obs_hotkey_load(lockAllSourcesHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);

		hotkeyLoadArray = obs_data_get_array(save_data, "refreshAudioMonitoringHotkey");
		obs_hotkey_load(refreshAudioMonitoringHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);

		hotkeyLoadArray = obs_data_get_array(save_data, "lockCurrentSourcesHotkey");
		obs_hotkey_load(lockCurrentSourcesHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);

		hotkeyLoadArray = obs_data_get_array(save_data, "openSourceInteractHotkey");
		obs_hotkey_load(openSourceInteractHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);

		hotkeyLoadArray = obs_data_get_array(save_data, "openSourcePropertiesHotkey");
		obs_hotkey_load(openSourcePropertiesHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);

		hotkeyLoadArray = obs_data_get_array(save_data, "openSourceFiltersHotkey");
		obs_hotkey_load(openSourceFiltersHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);

		hotkeyLoadArray = obs_data_get_array(save_data, "openSceneFiltersHotkey");
		obs_hotkey_load(openSceneFiltersHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);
	}
}

//-------------------HOTKEY REGISTRATION-------------------
void RegisterHotkeys()
{
	refreshBrowserSourcesHotkey = obs_hotkey_register_frontend("refresh_browser_sources", obs_module_text("RefreshBrowserSources"),
								HotkeyRefreshBrowserSources, nullptr);
	lockAllSourcesHotkey = obs_hotkey_register_frontend("lock_all_sources", obs_module_text("LockAllSources"),
							    HotkeyLockAllSources, nullptr);
	refreshAudioMonitoringHotkey = obs_hotkey_register_frontend("refresh_audio_monitoring", obs_module_text("RefreshAudioMonitoring"),
								    HotkeyRefreshAudioMonitoring, nullptr);
	lockCurrentSourcesHotkey = obs_hotkey_register_frontend("lock_current_sources", obs_module_text("LockCurrentSources"),
								HotkeyLockCurrentSources, nullptr);
	openSourcePropertiesHotkey = obs_hotkey_register_frontend("open_source_properties", obs_module_text("OpenSourceProperties"),
								  HotkeyOpenSourceProperties, nullptr);
	openSourceFiltersHotkey = obs_hotkey_register_frontend("open_source_filters", obs_module_text("OpenSourceFilters"),
							       HotkeyOpenSourceFilters, nullptr);
	openSourceInteractHotkey = obs_hotkey_register_frontend("open_source_interact", obs_module_text("OpenSourceInteract"),
								HotkeyOpenSourceInteract, nullptr);
	openSceneFiltersHotkey = obs_hotkey_register_frontend("open_scene_filters", obs_module_text("OpenSceneFilters"),
							      HotkeyOpenSceneFilters, nullptr);
}

void UnregisterHotkeys()
{
	obs_hotkey_unregister(refreshBrowserSourcesHotkey);
	obs_hotkey_unregister(lockAllSourcesHotkey);
	obs_hotkey_unregister(refreshAudioMonitoringHotkey);
	obs_hotkey_unregister(lockCurrentSourcesHotkey);
	obs_hotkey_unregister(openSourcePropertiesHotkey);
	obs_hotkey_unregister(openSourceFiltersHotkey);
	obs_hotkey_unregister(openSourceInteractHotkey);
	obs_hotkey_unregister(openSceneFiltersHotkey);
}

} // namespace HotkeyManager
} // namespace StreamUP