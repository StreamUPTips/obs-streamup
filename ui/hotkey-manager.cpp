#include "hotkey-manager.hpp"
#include "../utilities/debug-logger.hpp"
#include "streamup-common.hpp"
#include "source-manager.hpp"
#include "../utilities/obs-data-helpers.hpp"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs-hotkey.h>
// QSystemTrayIcon now handled by NotificationManager

// Forward declarations for functions from main streamup.cpp
#include "notification-manager.hpp"

namespace StreamUP {
namespace HotkeyManager {

//-------------------HOTKEY ID STORAGE-------------------
static obs_hotkey_id refresh_browser_sources_hotkey_id = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id refresh_audio_monitoring_hotkey_id = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id lock_all_sources_hotkey_id = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id lock_current_sources_hotkey_id = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id open_source_properties_hotkey_id = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id open_source_filters_hotkey_id = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id open_scene_filters_hotkey_id = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id open_source_interact_hotkey_id = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id activate_video_capture_devices_hotkey_id = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id deactivate_video_capture_devices_hotkey_id = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id refresh_video_capture_devices_hotkey_id = OBS_INVALID_HOTKEY_ID;

//-------------------HOTKEY HANDLERS-------------------
void HotkeyRefreshBrowserSources(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);
	if (!pressed)
		return;
	obs_enum_sources(StreamUP::SourceManager::RefreshBrowserSources, nullptr);
	StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Feature.BrowserSources.Title"),
			     obs_module_text("Hotkey.ActionCompleted"));
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
	StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Feature.AudioMonitoring.Title"),
			     obs_module_text("Hotkey.ActionCompleted"));
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
		return;
	}

	// Find the source by name
	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		// Open the properties of the selected source
		obs_frontend_open_source_properties(selected_source);
		obs_source_release(selected_source);
	} else {
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
		return;
	}

	// Find the source by name
	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		// Open the filters of the selected source
		obs_frontend_open_source_filters(selected_source);
		obs_source_release(selected_source);
	} else {
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
		return;
	}

	// Find the source by name
	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		// Open the interact window of the selected source
		obs_frontend_open_source_interaction(selected_source);
		obs_source_release(selected_source); // Release reference count
	} else {
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
		StreamUP::DebugLogger::LogDebug("Hotkeys", "Scene Filters", "No current scene found, cannot open filters.");
		return;
	}

	// Open the filters of the current scene
	obs_frontend_open_source_filters(current_scene);

	// Release reference count for the current scene
	obs_source_release(current_scene);
}

void HotkeyActivateAllVideoCaptureDevices(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);
	
	if (!pressed)
		return;
		
	StreamUP::SourceManager::ActivateAllVideoCaptureDevices();
}

void HotkeyDeactivateAllVideoCaptureDevices(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);
	
	if (!pressed)
		return;
		
	StreamUP::SourceManager::DeactivateAllVideoCaptureDevices();
}

void HotkeyRefreshAllVideoCaptureDevices(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);
	
	if (!pressed)
		return;
		
	StreamUP::SourceManager::RefreshAllVideoCaptureDevices();
}

//-------------------HOTKEY MANAGEMENT-------------------
void SaveLoadHotkeys(obs_data_t *save_data, bool saving, void *param)
{
	UNUSED_PARAMETER(param);
	
	if (saving) {
		// save hotkeys
		StreamUP::OBSDataHelpers::SaveHotkeyToData(save_data, "refresh_browser_sources_hotkey", refresh_browser_sources_hotkey_id);
		StreamUP::OBSDataHelpers::SaveHotkeyToData(save_data, "refresh_audio_monitoring_hotkey", refresh_audio_monitoring_hotkey_id);
		StreamUP::OBSDataHelpers::SaveHotkeyToData(save_data, "lock_all_sources_hotkey", lock_all_sources_hotkey_id);
		StreamUP::OBSDataHelpers::SaveHotkeyToData(save_data, "lock_current_sources_hotkey", lock_current_sources_hotkey_id);
		StreamUP::OBSDataHelpers::SaveHotkeyToData(save_data, "open_source_properties_hotkey", open_source_properties_hotkey_id);
		StreamUP::OBSDataHelpers::SaveHotkeyToData(save_data, "open_source_filters_hotkey", open_source_filters_hotkey_id);
		StreamUP::OBSDataHelpers::SaveHotkeyToData(save_data, "open_source_interact_hotkey", open_source_interact_hotkey_id);
		StreamUP::OBSDataHelpers::SaveHotkeyToData(save_data, "open_scene_filters_hotkey", open_scene_filters_hotkey_id);
		StreamUP::OBSDataHelpers::SaveHotkeyToData(save_data, "activate_video_capture_devices_hotkey", activate_video_capture_devices_hotkey_id);
		StreamUP::OBSDataHelpers::SaveHotkeyToData(save_data, "deactivate_video_capture_devices_hotkey", deactivate_video_capture_devices_hotkey_id);
		StreamUP::OBSDataHelpers::SaveHotkeyToData(save_data, "refresh_video_capture_devices_hotkey", refresh_video_capture_devices_hotkey_id);
	} else {
		// load hotkeys
		StreamUP::OBSDataHelpers::LoadHotkeyFromData(save_data, "refresh_browser_sources_hotkey", refresh_browser_sources_hotkey_id);
		StreamUP::OBSDataHelpers::LoadHotkeyFromData(save_data, "refresh_audio_monitoring_hotkey", refresh_audio_monitoring_hotkey_id);
		StreamUP::OBSDataHelpers::LoadHotkeyFromData(save_data, "lock_all_sources_hotkey", lock_all_sources_hotkey_id);
		StreamUP::OBSDataHelpers::LoadHotkeyFromData(save_data, "lock_current_sources_hotkey", lock_current_sources_hotkey_id);
		StreamUP::OBSDataHelpers::LoadHotkeyFromData(save_data, "open_source_properties_hotkey", open_source_properties_hotkey_id);
		StreamUP::OBSDataHelpers::LoadHotkeyFromData(save_data, "open_source_filters_hotkey", open_source_filters_hotkey_id);
		StreamUP::OBSDataHelpers::LoadHotkeyFromData(save_data, "open_source_interact_hotkey", open_source_interact_hotkey_id);
		StreamUP::OBSDataHelpers::LoadHotkeyFromData(save_data, "open_scene_filters_hotkey", open_scene_filters_hotkey_id);
		StreamUP::OBSDataHelpers::LoadHotkeyFromData(save_data, "activate_video_capture_devices_hotkey", activate_video_capture_devices_hotkey_id);
		StreamUP::OBSDataHelpers::LoadHotkeyFromData(save_data, "deactivate_video_capture_devices_hotkey", deactivate_video_capture_devices_hotkey_id);
		StreamUP::OBSDataHelpers::LoadHotkeyFromData(save_data, "refresh_video_capture_devices_hotkey", refresh_video_capture_devices_hotkey_id);
	}
}

//-------------------HOTKEY REGISTRATION-------------------
void RegisterHotkeys()
{
	refresh_browser_sources_hotkey_id = obs_hotkey_register_frontend("streamup_refresh_browser_sources", "StreamUP: Refresh Browser Sources",
											 HotkeyRefreshBrowserSources, nullptr);
	refresh_audio_monitoring_hotkey_id = obs_hotkey_register_frontend("streamup_refresh_audio_monitoring", "StreamUP: Refresh Audio Monitoring",
											  HotkeyRefreshAudioMonitoring, nullptr);
	lock_all_sources_hotkey_id = obs_hotkey_register_frontend("streamup_lock_all_sources", "StreamUP: Lock/Unlock All Sources",
										  HotkeyLockAllSources, nullptr);
	lock_current_sources_hotkey_id = obs_hotkey_register_frontend("streamup_lock_current_sources", "StreamUP: Lock/Unlock Current Scene Sources",
										      HotkeyLockCurrentSources, nullptr);
	open_source_properties_hotkey_id = obs_hotkey_register_frontend("streamup_open_source_properties", "StreamUP: Open Selected Source Properties",
											    HotkeyOpenSourceProperties, nullptr);
	open_source_filters_hotkey_id = obs_hotkey_register_frontend("streamup_open_source_filters", "StreamUP: Open Selected Source Filters",
											 HotkeyOpenSourceFilters, nullptr);
	open_source_interact_hotkey_id = obs_hotkey_register_frontend("streamup_open_source_interact", "StreamUP: Open Selected Source Interact",
											  HotkeyOpenSourceInteract, nullptr);
	open_scene_filters_hotkey_id = obs_hotkey_register_frontend("streamup_open_scene_filters", "StreamUP: Open Current Scene Filters",
											 HotkeyOpenSceneFilters, nullptr);
	activate_video_capture_devices_hotkey_id = obs_hotkey_register_frontend("streamup_activate_video_capture_devices", "StreamUP: Activate All Video Capture Devices",
												HotkeyActivateAllVideoCaptureDevices, nullptr);
	deactivate_video_capture_devices_hotkey_id = obs_hotkey_register_frontend("streamup_deactivate_video_capture_devices", "StreamUP: Deactivate All Video Capture Devices",
												  HotkeyDeactivateAllVideoCaptureDevices, nullptr);
	refresh_video_capture_devices_hotkey_id = obs_hotkey_register_frontend("streamup_refresh_video_capture_devices", "StreamUP: Refresh All Video Capture Devices",
											       HotkeyRefreshAllVideoCaptureDevices, nullptr);
}

void UnregisterHotkeys()
{
	obs_hotkey_unregister(refresh_browser_sources_hotkey_id);
	obs_hotkey_unregister(refresh_audio_monitoring_hotkey_id);
	obs_hotkey_unregister(lock_all_sources_hotkey_id);
	obs_hotkey_unregister(lock_current_sources_hotkey_id);
	obs_hotkey_unregister(open_source_properties_hotkey_id);
	obs_hotkey_unregister(open_source_filters_hotkey_id);
	obs_hotkey_unregister(open_source_interact_hotkey_id);
	obs_hotkey_unregister(open_scene_filters_hotkey_id);
	obs_hotkey_unregister(activate_video_capture_devices_hotkey_id);
	obs_hotkey_unregister(deactivate_video_capture_devices_hotkey_id);
	obs_hotkey_unregister(refresh_video_capture_devices_hotkey_id);
}

void ResetAllHotkeys()
{
	// Create empty data arrays to clear hotkey assignments
	obs_data_array_t *emptyArray = obs_data_array_create();
	
	// Reset all StreamUP hotkeys to empty assignments
	obs_hotkey_load(refresh_browser_sources_hotkey_id, emptyArray);
	obs_hotkey_load(refresh_audio_monitoring_hotkey_id, emptyArray);
	obs_hotkey_load(lock_all_sources_hotkey_id, emptyArray);
	obs_hotkey_load(lock_current_sources_hotkey_id, emptyArray);
	obs_hotkey_load(open_source_properties_hotkey_id, emptyArray);
	obs_hotkey_load(open_source_filters_hotkey_id, emptyArray);
	obs_hotkey_load(open_source_interact_hotkey_id, emptyArray);
	obs_hotkey_load(open_scene_filters_hotkey_id, emptyArray);
	obs_hotkey_load(activate_video_capture_devices_hotkey_id, emptyArray);
	obs_hotkey_load(deactivate_video_capture_devices_hotkey_id, emptyArray);
	obs_hotkey_load(refresh_video_capture_devices_hotkey_id, emptyArray);
	
	obs_data_array_release(emptyArray);
	
	StreamUP::DebugLogger::LogDebug("Hotkeys", "Reset", "All hotkeys have been reset to no key assignments");
}

obs_hotkey_id GetHotkeyId(const char* hotkeyName)
{
	if (strcmp(hotkeyName, "streamup_refresh_browser_sources") == 0)
		return refresh_browser_sources_hotkey_id;
	else if (strcmp(hotkeyName, "streamup_refresh_audio_monitoring") == 0)
		return refresh_audio_monitoring_hotkey_id;
	else if (strcmp(hotkeyName, "streamup_lock_all_sources") == 0)
		return lock_all_sources_hotkey_id;
	else if (strcmp(hotkeyName, "streamup_lock_current_sources") == 0)
		return lock_current_sources_hotkey_id;
	else if (strcmp(hotkeyName, "streamup_open_source_properties") == 0)
		return open_source_properties_hotkey_id;
	else if (strcmp(hotkeyName, "streamup_open_source_filters") == 0)
		return open_source_filters_hotkey_id;
	else if (strcmp(hotkeyName, "streamup_open_source_interact") == 0)
		return open_source_interact_hotkey_id;
	else if (strcmp(hotkeyName, "streamup_open_scene_filters") == 0)
		return open_scene_filters_hotkey_id;
	else if (strcmp(hotkeyName, "streamup_activate_video_capture_devices") == 0)
		return activate_video_capture_devices_hotkey_id;
	else if (strcmp(hotkeyName, "streamup_deactivate_video_capture_devices") == 0)
		return deactivate_video_capture_devices_hotkey_id;
	else if (strcmp(hotkeyName, "streamup_refresh_video_capture_devices") == 0)
		return refresh_video_capture_devices_hotkey_id;
	
	return OBS_INVALID_HOTKEY_ID;
}

obs_data_array_t* GetHotkeyBinding(const char* hotkeyName)
{
	obs_hotkey_id hotkeyId = GetHotkeyId(hotkeyName);
	if (hotkeyId == OBS_INVALID_HOTKEY_ID) {
		StreamUP::DebugLogger::LogWarningFormat("Hotkeys", "Get Binding", "Invalid hotkey name: %s", hotkeyName);
		return nullptr;
	}
	
	return obs_hotkey_save(hotkeyId);
}

void SetHotkeyBinding(const char* hotkeyName, obs_data_array_t* keyData) 
{
	obs_hotkey_id hotkeyId = GetHotkeyId(hotkeyName);
	if (hotkeyId == OBS_INVALID_HOTKEY_ID) {
		StreamUP::DebugLogger::LogWarningFormat("Hotkeys", "Set Binding", "Invalid hotkey name: %s", hotkeyName);
		return;
	}
	
	obs_hotkey_load(hotkeyId, keyData);
	StreamUP::DebugLogger::LogDebugFormat("Hotkeys", "Set Binding", "Updated hotkey binding for: %s", hotkeyName);
	
	// Force OBS to save the scene collection to persist hotkey changes
	obs_frontend_save();
}

} // namespace HotkeyManager
} // namespace StreamUP