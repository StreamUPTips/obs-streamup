#include "source-manager.hpp"
#include "streamup-common.hpp"
#include "error-handler.hpp"
#include <obs-module.h>
// QSystemTrayIcon now handled by NotificationManager
#include <vector>

// Forward declarations for functions from main streamup.cpp
extern void CreateToolDialog(const char *infoText1, const char *infoText2, const char *infoText3, const QString &titleText,
                            const std::function<void()> &buttonCallback, const QString &jsonString, const char *how1, const char *how2,
                            const char *how3, const char *how4, const char *notificationMessage);
#include "notification-manager.hpp"

namespace StreamUP {
namespace SourceManager {

// Helper function to convert monitoring type to string
const char *MonitoringTypeToString(obs_monitoring_type type)
{
	switch (type) {
	case OBS_MONITORING_TYPE_NONE:
		return "None";
	case OBS_MONITORING_TYPE_MONITOR_ONLY:
		return "Monitor Only";
	case OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT:
		return "Monitor and Output";
	default:
		return "Unknown";
	}
}

// Structure to hold data for finding selected items
struct SceneFindBoxData {
	std::vector<obs_sceneitem_t *> sceneItems;

	// Constructor
	SceneFindBoxData() = default;

	// Deleted copy/move constructors and assignment operators to avoid accidental copying
	SceneFindBoxData(const SceneFindBoxData &) = delete;
	SceneFindBoxData(SceneFindBoxData &&) = delete;
	SceneFindBoxData &operator=(const SceneFindBoxData &) = delete;
	SceneFindBoxData &operator=(SceneFindBoxData &&) = delete;
};

//-------------------AUDIO MONITORING FUNCTIONS-------------------
bool RefreshAudioMonitoring(void *data, obs_source_t *source)
{
	UNUSED_PARAMETER(data);

	if (!StreamUP::ErrorHandler::ValidateSource(source, "RefreshAudioMonitoring")) {
		return false;
	}

	const char *source_name = obs_source_get_name(source);

	obs_monitoring_type original_monitoring_type = obs_source_get_monitoring_type(source);

	if (original_monitoring_type != OBS_MONITORING_TYPE_NONE) {
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_NONE);
		obs_source_set_monitoring_type(source, original_monitoring_type);

		blog(LOG_INFO, "[StreamUP] '%s' has refreshed audio monitoring type: '%s'", source_name,
		     MonitoringTypeToString(original_monitoring_type));
	}

	return true;
}

void RefreshAudioMonitoringDialog()
{
	CreateToolDialog(
		"RefreshAudioMonitoringInfo1", "RefreshAudioMonitoringInfo2", "RefreshAudioMonitoringInfo3",
		"RefreshAudioMonitoring", []() { obs_enum_sources(RefreshAudioMonitoring, nullptr); },
		R"(
                    {
                        "requestType": "CallVendorRequest",
                        "requestData": {
                            "vendorName": "streamup",
                            "requestType": "refresh_audio_monitoring",
                            "requestData": null
                        }
                    })",
		"RefreshAudioMonitoringHowTo1", "RefreshAudioMonitoringHowTo2", "RefreshAudioMonitoringHowTo3",
		"RefreshAudioMonitoringHowTo4", "RefreshAudioMonitoringNotification");
}

//-------------------BROWSER SOURCE FUNCTIONS-------------------
bool RefreshBrowserSources(void *data, obs_source_t *source)
{
	UNUSED_PARAMETER(data);

	const char *source_id = obs_source_get_id(source);
	const char *source_name = obs_source_get_name(source);

	if (strcmp(source_id, "browser_source") == 0) {
		obs_data_t *settings = obs_source_get_settings(source);
		int fps = obs_data_get_int(settings, "fps");

		if (fps % 2 == 0) {
			obs_data_set_int(settings, "fps", fps + 1);
		} else {
			obs_data_set_int(settings, "fps", fps - 1);
		}
		obs_source_update(source, settings);
		blog(LOG_INFO, "[StreamUP] Refreshed '%s' browser source", source_name);

		obs_data_release(settings);
	}

	return true;
}

void RefreshBrowserSourcesDialog()
{
	CreateToolDialog(
		"RefreshBrowserSourcesInfo1", "RefreshBrowserSourcesInfo2", "RefreshBrowserSourcesInfo3", "RefreshBrowserSources",
		[]() { obs_enum_sources(RefreshBrowserSources, nullptr); },
		R"(
                    {
                        "requestType": "CallVendorRequest",
                        "requestData": {
                            "vendorName": "streamup",
                            "requestType": "refresh_browser_sources",
                            "requestData": null
                        }
                    })",
		"RefreshBrowserSourcesHowTo1", "RefreshBrowserSourcesHowTo2", "RefreshBrowserSourcesHowTo3",
		"RefreshBrowserSourcesHowTo4", "RefreshBrowserSourcesNotification");
}

//-------------------SELECTED SOURCE FUNCTIONS-------------------
bool FindSelected(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);

	SceneFindBoxData *data = reinterpret_cast<SceneFindBoxData *>(param);

	obs_source_t *source = obs_sceneitem_get_source(item);
	if (source) {
		// If the item is selected, add it to the list
		if (obs_sceneitem_selected(item)) {
			data->sceneItems.push_back(item);
		}

		// If the source is a group, recursively enumerate its items
		obs_scene_t *group_scene = obs_group_from_source(source);
		if (group_scene) {
			obs_scene_enum_items(group_scene, FindSelected, param); // Enumerate items inside the group
		}
	}

	return true; // Continue enumerating items
}

//-------------------SOURCE LOCKING FUNCTIONS-------------------
bool CheckGroupItemsIfAnyUnlocked(obs_source_t *group_source)
{
	obs_scene_t *group_scene = obs_group_from_source(group_source);
	if (!group_scene) {
		return false;
	}

	return CheckIfAnyUnlocked(group_scene);
}

bool CheckIfAnyUnlocked(obs_scene_t *scene)
{
	bool any_unlocked = false;
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
			bool *any_unlocked = static_cast<bool *>(param);
			obs_source_t *source = obs_sceneitem_get_source(item);

			UNUSED_PARAMETER(scene);

			if (obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE) {
				if (CheckGroupItemsIfAnyUnlocked(source)) {
					*any_unlocked = true;
					return false; // Stop enumeration
				}
			}

			if (!obs_sceneitem_locked(item)) {
				*any_unlocked = true;
				return false; // Stop enumeration
			}
			return true; // Continue enumeration
		},
		&any_unlocked);
	return any_unlocked;
}

bool ToggleLockSceneItemCallback(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);

	bool *lock = static_cast<bool *>(param);
	obs_source_t *source = obs_sceneitem_get_source(item);

	if (obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE) {
		ToggleLockGroupItems(source, *lock);
	}

	obs_sceneitem_set_locked(item, *lock);

	return true; // Return true to continue enumeration
}

void ToggleLockGroupItems(obs_source_t *group, bool lock)
{
	obs_scene_t *group_scene = obs_group_from_source(group);
	if (!group_scene) {
		return;
	}

	obs_scene_enum_items(group_scene, ToggleLockSceneItemCallback, &lock);
}

bool ToggleLockSceneItems(obs_scene_t *scene, bool lock)
{
	obs_scene_enum_items(scene, ToggleLockSceneItemCallback, &lock);
	return lock;
}

bool ToggleLockSourcesInCurrentScene(bool sendNotification)
{
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		blog(LOG_WARNING, "[StreamUP] No current scene found.");
		return false;
	}

	obs_scene_t *scene = obs_scene_from_source(current_scene);
	if (!scene) {
		obs_source_release(current_scene);
		return false;
	}

	bool any_unlocked = CheckIfAnyUnlocked(scene);
	ToggleLockSceneItems(scene, any_unlocked);
	obs_source_release(current_scene);

	if (sendNotification) {
		if (any_unlocked) {
			blog(LOG_INFO, "[StreamUP] All sources in the current scene have been locked.");
			StreamUP::NotificationManager::SendInfoNotification(obs_module_text("SourceLockSystem"),
					     obs_module_text("LockedCurrentSources"));
		} else {
			blog(LOG_INFO, "[StreamUP] All sources in the current scene have been unlocked.");
			StreamUP::NotificationManager::SendInfoNotification(obs_module_text("SourceLockSystem"),
					     obs_module_text("UnlockedCurrentSources"));
		}
	}

	return any_unlocked;
}

// Helper callbacks for all scenes operations
static bool CheckIfAnyUnlockedCallback(void *param, obs_source_t *source)
{
	bool *any_unlocked = static_cast<bool *>(param);
	obs_scene_t *scene = obs_scene_from_source(source);

	if (CheckIfAnyUnlocked(scene)) {
		*any_unlocked = true;
		return false; // Stop enumeration
	}

	return true; // Continue enumeration
}

static bool ToggleLockSceneItemsCallback(void *param, obs_source_t *source)
{
	bool *lock = static_cast<bool *>(param);
	obs_scene_t *scene = obs_scene_from_source(source);
	ToggleLockSceneItems(scene, *lock);
	return true; // Continue enumeration
}

bool CheckIfAnyUnlockedInAllScenes()
{
	bool any_unlocked = false;
	obs_enum_scenes(CheckIfAnyUnlockedCallback, &any_unlocked);
	return any_unlocked;
}

static void ToggleLockSourcesInAllScenes(bool lock)
{
	obs_enum_scenes(ToggleLockSceneItemsCallback, &lock);
}

bool ToggleLockAllSources(bool sendNotification)
{
	bool any_unlocked = CheckIfAnyUnlockedInAllScenes();
	ToggleLockSourcesInAllScenes(any_unlocked);

	if (sendNotification) {
		if (any_unlocked) {
			blog(LOG_INFO, "[StreamUP] All sources in all scenes have been locked.");
			StreamUP::NotificationManager::SendInfoNotification(obs_module_text("SourceLockSystem"),
					     obs_module_text("LockedAllSources"));
		} else {
			blog(LOG_INFO, "[StreamUP] All sources in all scenes have been unlocked.");
			StreamUP::NotificationManager::SendInfoNotification(obs_module_text("SourceLockSystem"),
					     obs_module_text("UnlockedAllSources"));
		}
	}

	return any_unlocked;
}

void LockAllSourcesDialog()
{
	CreateToolDialog(
		"LockAllSourcesInfo1", "LockAllSourcesInfo2", "LockAllSourcesInfo3", "LockAllSources",
		[]() { ToggleLockAllSources(); },
		R"(
                    {
                        "requestType": "CallVendorRequest",
                        "requestData": {
                            "vendorName": "streamup",
                            "requestType": "toggleLockAllSources",
                            "requestData": null
                        }
                    })",
		"LockAllSourcesHowTo1", "LockAllSourcesHowTo2", "LockAllSourcesHowTo3", "LockAllSourcesHowTo4", NULL);
}

void LockAllCurrentSourcesDialog()
{
	CreateToolDialog(
		"LockAllCurrentSourcesInfo1", "LockAllCurrentSourcesInfo2", "LockAllCurrentSourcesInfo3", "LockAllCurrentSources",
		[]() { ToggleLockSourcesInCurrentScene(); },
		R"(
                    {
                        "requestType": "CallVendorRequest",
                        "requestData": {
                            "vendorName": "streamup",
                            "requestType": "toggleLockCurrentSources",
                            "requestData": null
                        }
                    })",
		"LockAllCurrentSourcesHowTo1", "LockAllCurrentSourcesHowTo2", "LockAllCurrentSourcesHowTo3",
		"LockAllCurrentSourcesHowTo4", NULL);
}

//-------------------ADDITIONAL HELPER FUNCTIONS-------------------
// Function to get the currently selected source name
const char *GetSelectedSourceFromCurrentScene()
{
	// Get the current scene source from the frontend
	obs_source_t *current_scene_source = obs_frontend_get_current_scene();
	if (!current_scene_source) {
		blog(LOG_INFO, "[StreamUP] No active scene.");
		return nullptr;
	}

	// Get the scene from the source
	obs_scene_t *scene = obs_scene_from_source(current_scene_source);
	obs_source_release(current_scene_source); // Always release the source when done

	if (!scene) {
		blog(LOG_INFO, "[StreamUP] No active scene found.");
		return nullptr;
	}

	// Data structure to hold the selected items found
	SceneFindBoxData data;

	// Enumerate through the scene items and find the selected ones, including groups
	obs_scene_enum_items(scene, FindSelected, &data);

	// If there is exactly one selected item, return its source name
	if (data.sceneItems.size() == 1) {
		obs_source_t *selected_source = obs_sceneitem_get_source(data.sceneItems[0]);
		return obs_source_get_name(selected_source); // Return the source name
	}

	blog(LOG_INFO, "[StreamUP] No selected source or multiple selected sources.");
	return nullptr; // No source or multiple sources selected
}

// Convenience functions for checking lock status (used by dock widget)
bool AreAllSourcesLockedInCurrentScene()
{
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		return false;
	}

	obs_scene_t *scene = obs_scene_from_source(current_scene);
	if (!scene) {
		obs_source_release(current_scene);
		return false;
	}

	bool any_unlocked = CheckIfAnyUnlocked(scene);
	obs_source_release(current_scene);

	return !any_unlocked;
}

bool AreAllSourcesLockedInAllScenes()
{
	return !CheckIfAnyUnlockedInAllScenes();
}

} // namespace SourceManager
} // namespace StreamUP