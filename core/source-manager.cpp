#include "source-manager.hpp"
#include <streamup/debug-logger.hpp>
#include "streamup-common.hpp"
#include "error-handler.hpp"
#include <obs-module.h>
#include <callback/proc.h>
#include <callback/calldata.h>
// QSystemTrayIcon now handled by NotificationManager
#include <vector>
#include <QTimer>

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

	obs_monitoring_type original_monitoring_type = obs_source_get_monitoring_type(source);

	if (original_monitoring_type != OBS_MONITORING_TYPE_NONE) {
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_NONE);
		obs_source_set_monitoring_type(source, original_monitoring_type);
	}

	return true;
}

void RefreshAudioMonitoringDialog()
{
	CreateToolDialog(
		"Feature.AudioMonitoring.Info1", "Feature.AudioMonitoring.Info2", "Feature.AudioMonitoring.Info3",
		"Feature.AudioMonitoring.Title", []() { obs_enum_sources(RefreshAudioMonitoring, nullptr); },
		R"(
                    {
                        "requestType": "CallVendorRequest",
                        "requestData": {
                            "vendorName": "streamup",
                            "requestType": "refresh_audio_monitoring",
                            "requestData": null
                        }
                    })",
		"Feature.AudioMonitoring.HowTo1", "Feature.AudioMonitoring.HowTo2", "Feature.AudioMonitoring.HowTo3",
		"Feature.AudioMonitoring.HowTo4", "Feature.AudioMonitoring.Notification");
}

//-------------------BROWSER SOURCE FUNCTIONS-------------------
bool RefreshBrowserSources(void *data, obs_source_t *source)
{
	UNUSED_PARAMETER(data);

	const char *source_id = obs_source_get_id(source);
	if (!source_id)
		return true;

	if (strcmp(source_id, "browser_source") == 0) {
		obs_data_t *settings = obs_source_get_settings(source);
		int fps = obs_data_get_int(settings, "fps");

		if (fps % 2 == 0) {
			obs_data_set_int(settings, "fps", fps + 1);
		} else {
			obs_data_set_int(settings, "fps", fps - 1);
		}
		obs_source_update(source, settings);

		obs_data_release(settings);
	}

	return true;
}

void RefreshBrowserSourcesDialog()
{
	CreateToolDialog(
		"Feature.BrowserSources.Info1", "Feature.BrowserSources.Info2", "Feature.BrowserSources.Info3",
		"Feature.BrowserSources.Title",
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
		"Feature.BrowserSources.HowTo1", "Feature.BrowserSources.HowTo2", "Feature.BrowserSources.HowTo3",
		"Feature.BrowserSources.HowTo4", "Feature.BrowserSources.Notification");
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
	if (!group_source) return false;
	
	obs_scene_t *group_scene = obs_group_from_source(group_source);
	return group_scene ? CheckIfAnyUnlocked(group_scene) : false;
}

bool CheckIfAnyUnlocked(obs_scene_t *scene)
{
	if (!scene) return false;
	
	bool any_unlocked = false;
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) -> bool {
			bool *any_unlocked = static_cast<bool *>(param);
			
			// Early termination if already found unlocked item
			if (*any_unlocked) return false;
			
			obs_source_t *source = obs_sceneitem_get_source(item);
			if (!source) return true;

			// Check groups recursively with early termination
			if (obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE) {
				if (CheckGroupItemsIfAnyUnlocked(source)) {
					*any_unlocked = true;
					return false;
				}
			}

			// Check if current item is unlocked
			if (!obs_sceneitem_locked(item)) {
				*any_unlocked = true;
				return false;
			}
			return true;
		},
		&any_unlocked);
	return any_unlocked;
}

bool ToggleLockSceneItemCallback(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	bool *lock = static_cast<bool *>(param);
	obs_source_t *source = obs_sceneitem_get_source(item);
	
	if (!source) return true;

	// Handle groups first, then set item lock state
	if (obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE) {
		ToggleLockGroupItems(source, *lock);
	}

	obs_sceneitem_set_locked(item, *lock);
	return true;
}

void ToggleLockGroupItems(obs_source_t *group, bool lock)
{
	if (!group) return;
	
	obs_scene_t *group_scene = obs_group_from_source(group);
	if (group_scene) {
		obs_scene_enum_items(group_scene, ToggleLockSceneItemCallback, &lock);
	}
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
		if (sendNotification) {
			StreamUP::ErrorHandler::LogError("No current scene found", 
				StreamUP::ErrorHandler::Category::Source);
		}
		return false;
	}

	obs_scene_t *scene = obs_scene_from_source(current_scene);
	if (!scene) {
		obs_source_release(current_scene);
		if (sendNotification) {
			StreamUP::ErrorHandler::LogError("Invalid scene source", 
				StreamUP::ErrorHandler::Category::Source);
		}
		return false;
	}

	// Single-pass optimized check and toggle
	bool any_unlocked = CheckIfAnyUnlocked(scene);
	bool lock_state = any_unlocked; // Lock if any unlocked, unlock if all locked
	
	ToggleLockSceneItems(scene, lock_state);
	obs_source_release(current_scene);

	if (sendNotification) {
		if (any_unlocked) {
			StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Feature.SourceLock.Title"),
					     obs_module_text("Feature.SourceLock.Current.NotifyLocked"));
		} else {
			StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Feature.SourceLock.Title"),
					     obs_module_text("Feature.SourceLock.Current.NotifyUnlocked"));
		}
	}

	return any_unlocked;
}

// Optimized structure for combined operations
struct LockOperationData {
	bool any_unlocked = false;
	bool lock_determined = false;
	bool lock_state = false;
	bool check_only = false;
};

// Optimized single-pass callback for all scenes operations
static bool OptimizedLockCallback(void *param, obs_source_t *source)
{
	LockOperationData *data = static_cast<LockOperationData *>(param);
	obs_scene_t *scene = obs_scene_from_source(source);
	if (!scene) return true;

	// First pass: determine lock state if not already determined
	if (!data->lock_determined && !data->check_only) {
		if (CheckIfAnyUnlocked(scene)) {
			data->any_unlocked = true;
			data->lock_state = true; // Lock because unlocked items found
			data->lock_determined = true;
		}
	}
	
	// For check-only operations
	if (data->check_only) {
		if (CheckIfAnyUnlocked(scene)) {
			data->any_unlocked = true;
			return false; // Early termination for check-only
		}
		return true;
	}

	// Apply lock operation if state is determined
	if (data->lock_determined) {
		ToggleLockSceneItems(scene, data->lock_state);
	}

	return true;
}

bool CheckIfAnyUnlockedInAllScenes()
{
	LockOperationData data;
	data.check_only = true;
	obs_enum_scenes(OptimizedLockCallback, &data);
	return data.any_unlocked;
}

static void ToggleLockSourcesInAllScenes(bool lock)
{
	LockOperationData data;
	data.lock_determined = true;
	data.lock_state = lock;
	obs_enum_scenes(OptimizedLockCallback, &data);
}

bool ToggleLockAllSources(bool sendNotification)
{
	// Optimized single-pass operation
	LockOperationData data;
	
	// First pass: determine lock state
	obs_enum_scenes(OptimizedLockCallback, &data);
	
	// If no unlocked items found, unlock all (toggle behavior)
	if (!data.any_unlocked) {
		data.lock_state = false; // Unlock all
		data.lock_determined = true;
	} else {
		data.lock_state = true; // Lock all (already determined)
		data.lock_determined = true;
	}
	
	// Second pass: apply the determined lock state
	data.check_only = false;
	obs_enum_scenes(OptimizedLockCallback, &data);

	if (sendNotification) {
		if (data.any_unlocked) {
			StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Feature.SourceLock.Title"),
					     obs_module_text("Feature.SourceLock.All.NotifyLocked"));
		} else {
			StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Feature.SourceLock.Title"),
					     obs_module_text("Feature.SourceLock.All.NotifyUnlocked"));
		}
	}

	return data.any_unlocked;
}

void LockAllSourcesDialog()
{
	CreateToolDialog(
		"Feature.SourceLock.All.Info1", "Feature.SourceLock.All.Info2", "Feature.SourceLock.All.Info3",
		"Feature.SourceLock.All.Title",
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
		"Feature.SourceLock.All.HowTo1", "Feature.SourceLock.All.HowTo2", "Feature.SourceLock.All.HowTo3",
		"Feature.SourceLock.All.HowTo4", NULL);
}

void LockAllCurrentSourcesDialog()
{
	CreateToolDialog(
		"Feature.SourceLock.Current.Info1", "Feature.SourceLock.Current.Info2", "Feature.SourceLock.Current.Info3",
		"Feature.SourceLock.Current.Title",
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
		"Feature.SourceLock.Current.HowTo1", "Feature.SourceLock.Current.HowTo2", "Feature.SourceLock.Current.HowTo3",
		"Feature.SourceLock.Current.HowTo4", NULL);
}

//-------------------ADDITIONAL HELPER FUNCTIONS-------------------
// Function to get the currently selected source name
const char *GetSelectedSourceFromCurrentScene()
{
	// Get the current scene source from the frontend
	obs_source_t *current_scene_source = obs_frontend_get_current_scene();
	if (!current_scene_source) {
		return nullptr;
	}

	// Get the scene from the source
	obs_scene_t *scene = obs_scene_from_source(current_scene_source);
	obs_source_release(current_scene_source); // Always release the source when done

	if (!scene) {
		return nullptr;
	}

	// Data structure to hold the selected items found
	SceneFindBoxData data;

	// Enumerate through the scene items and find the selected ones, including groups
	obs_scene_enum_items(scene, FindSelected, &data);

	// If there is exactly one selected item, return its source name
	if (data.sceneItems.size() == 1) {
		obs_source_t *selected_source = obs_sceneitem_get_source(data.sceneItems[0]);
		if (!selected_source)
			return nullptr;
		return obs_source_get_name(selected_source);
	}

	return nullptr; // No source or multiple sources selected
}

// Count the selected scene items in the current scene (0, 1, or many)
int GetSelectedSourceCount()
{
	obs_source_t *current_scene_source = obs_frontend_get_current_scene();
	if (!current_scene_source) {
		return 0;
	}

	obs_scene_t *scene = obs_scene_from_source(current_scene_source);
	obs_source_release(current_scene_source);
	if (!scene) {
		return 0;
	}

	SceneFindBoxData data;
	obs_scene_enum_items(scene, FindSelected, &data);
	return static_cast<int>(data.sceneItems.size());
}

// Convenience functions for checking lock status (used by dock widget)
bool AreAllSourcesLockedInCurrentScene()
{
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) return false;

	obs_scene_t *scene = obs_scene_from_source(current_scene);
	bool result = false;
	
	if (scene) {
		result = !CheckIfAnyUnlocked(scene);
	}
	
	obs_source_release(current_scene);
	return result;
}

bool AreAllSourcesLockedInAllScenes()
{
	return !CheckIfAnyUnlockedInAllScenes();
}

//-------------------VIDEO CAPTURE DEVICE FUNCTIONS-------------------


bool IsVideoCaptureDevice(obs_source_t *source)
{
	if (!source) return false;
	
	const char *source_id = obs_source_get_id(source);
	if (!source_id) return false;
	
	// Check for common video capture device source IDs across platforms
	return (strcmp(source_id, "dshow_input") == 0 ||      // Windows DirectShow
		strcmp(source_id, "av_capture_input") == 0 ||  // macOS AVCapture  
		strcmp(source_id, "v4l2_input") == 0);         // Linux V4L2
}


bool ActivateAllVideoCaptureDevices(bool sendNotification)
{
	int counts[2] = {0, 0}; // [activated_count, total_count]
	
	obs_enum_sources([](void *data, obs_source_t *source) -> bool {
		int *counts = static_cast<int*>(data);
		int &activated = counts[0];
		int &total = counts[1];
		
		if (!IsVideoCaptureDevice(source)) {
			return true;
		}
		
		total++;
		
		// Get the source's procedure handler
		proc_handler_t *ph = obs_source_get_proc_handler(source);
		if (!ph) {
			// Fallback to old method if no procedure handler
			if (!obs_source_enabled(source)) {
				obs_source_set_enabled(source, true);
				activated++;
			}
			return true;
		}
		
		// Check if device is already active by reading the "active" setting
		obs_data_t *settings = obs_source_get_settings(source);
		bool isActive = obs_data_get_bool(settings, "active");
		obs_data_release(settings);

		if (isActive) {
			// Device is already active, skip it
			return true;
		}

		// Use proper activation via procedure handler
		calldata_t cd;
		calldata_init(&cd);
		calldata_set_bool(&cd, "active", true);

		if (proc_handler_call(ph, "activate", &cd)) {
			activated++;
			StreamUP::DebugLogger::LogDebugFormat("VideoCapture", "Activate Device", "Activated video capture device: %s", obs_source_get_name(source));
		} else {
			// Fallback to old method if procedure call fails
			if (!obs_source_enabled(source)) {
				obs_source_set_enabled(source, true);
				activated++;
			}
		}
		
		calldata_free(&cd);
		return true;
	}, (void*)counts);
	
	int activated_count = counts[0];
	int total_count = counts[1];

	if (sendNotification) {
		QString message;
		if (activated_count > 0) {
			message = QString(obs_module_text("Feature.VideoCapture.Activate.NotifyActivated")).arg(activated_count);
			StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Menu.VideoCapture.Root"), message);
		} else if (total_count > 0) {
			message = obs_module_text("Feature.VideoCapture.Activate.NotifyAlreadyActive");
			StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Menu.VideoCapture.Root"), message);
		} else {
			message = obs_module_text("Feature.VideoCapture.NotifyNoneFound");
			StreamUP::NotificationManager::SendWarningNotification(obs_module_text("Menu.VideoCapture.Root"), message);
		}
	}

	StreamUP::DebugLogger::LogInfoFormat("VideoCapture", "Activated %d video capture devices (total found: %d)",
		 activated_count, total_count);
	return true;
}

bool DeactivateAllVideoCaptureDevices(bool sendNotification)
{
	int counts[2] = {0, 0}; // [deactivated_count, total_count]

	obs_enum_sources([](void *data, obs_source_t *source) -> bool {
		int *counts = static_cast<int*>(data);
		int &deactivated = counts[0];
		int &total = counts[1];

		if (!IsVideoCaptureDevice(source)) {
			return true;
		}

		total++;

		// Get the source's procedure handler
		proc_handler_t *ph = obs_source_get_proc_handler(source);
		if (!ph) {
			// Fallback to old method if no procedure handler
			if (obs_source_enabled(source)) {
				obs_source_set_enabled(source, false);
				deactivated++;
			}
			return true;
		}

		// Check if device is already inactive by reading the "active" setting
		obs_data_t *settings = obs_source_get_settings(source);
		bool isActive = obs_data_get_bool(settings, "active");
		obs_data_release(settings);

		if (!isActive) {
			// Device is already inactive, skip it
			return true;
		}

		// Use proper deactivation via procedure handler
		calldata_t cd;
		calldata_init(&cd);
		calldata_set_bool(&cd, "active", false);

		if (proc_handler_call(ph, "activate", &cd)) {
			deactivated++;
			StreamUP::DebugLogger::LogDebugFormat("VideoCapture", "Deactivate Device", "Deactivated video capture device: %s", obs_source_get_name(source));
		} else {
			// Fallback to old method if procedure call fails
			if (obs_source_enabled(source)) {
				obs_source_set_enabled(source, false);
				deactivated++;
			}
		}

		calldata_free(&cd);
		return true;
	}, (void*)counts);

	int deactivated_count = counts[0];
	int total_count = counts[1];
	
	if (sendNotification) {
		QString message;
		if (deactivated_count > 0) {
			message = QString(obs_module_text("Feature.VideoCapture.Deactivate.NotifyDeactivated")).arg(deactivated_count);
			StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Menu.VideoCapture.Root"), message);
		} else if (total_count > 0) {
			message = obs_module_text("Feature.VideoCapture.Deactivate.NotifyAlreadyInactive");
			StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Menu.VideoCapture.Root"), message);
		} else {
			message = obs_module_text("Feature.VideoCapture.NotifyNoneFound");
			StreamUP::NotificationManager::SendWarningNotification(obs_module_text("Menu.VideoCapture.Root"), message);
		}
	}
	
	StreamUP::DebugLogger::LogInfoFormat("VideoCapture", "Deactivated %d video capture devices (total found: %d)", 
		 deactivated_count, total_count);
	return true;
}

bool RefreshAllVideoCaptureDevices(bool sendNotification)
{
	std::vector<obs_source_t*> activeSources;
	int total_count = 0;
	
	// Step 1: Collect all currently active video capture devices
	std::pair<std::vector<obs_source_t*>*, int*> context = {&activeSources, &total_count};
	obs_enum_sources([](void *data, obs_source_t *source) -> bool {
		auto *context = static_cast<std::pair<std::vector<obs_source_t*>*, int*>*>(data);
		std::vector<obs_source_t*> *activeSources = context->first;
		int *total_count = context->second;

		if (!IsVideoCaptureDevice(source)) {
			return true;
		}

		(*total_count)++;

		// Check if device is active by reading the "active" setting
		obs_data_t *settings = obs_source_get_settings(source);
		bool isActive = obs_data_get_bool(settings, "active");
		obs_data_release(settings);

		// Only collect devices that are currently active
		if (isActive) {
			obs_source_get_ref(source); // Add reference since we're storing it
			activeSources->push_back(source);
		}

		return true;
	}, &context);
	
	if (activeSources.empty()) {
		if (sendNotification) {
			QString message = total_count > 0 ? 
				obs_module_text("Feature.VideoCapture.Refresh.NotifyNoneActive") : 
				obs_module_text("Feature.VideoCapture.NotifyNoneFound");
			StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Menu.VideoCapture.Root"), message);
		}
		StreamUP::DebugLogger::LogDebugFormat("VideoCapture", "Refresh", "No active video capture devices to refresh (total found: %d)", total_count);
		return true;
	}
	
	// Step 2: Deactivate all active devices using proper procedure handler
	for (obs_source_t *source : activeSources) {
		proc_handler_t *ph = obs_source_get_proc_handler(source);
		if (ph) {
			calldata_t cd;
			calldata_init(&cd);
			calldata_set_bool(&cd, "active", false);
			proc_handler_call(ph, "activate", &cd);
			calldata_free(&cd);
		} else {
			// Fallback to old method
			obs_source_set_enabled(source, false);
		}
	}
	
	// Step 3: Wait a short time for devices to properly deactivate
	// Use Qt's event processing to avoid blocking the UI completely
	QTimer::singleShot(500, [activeSources, sendNotification]() {
		// Step 4: Reactivate all previously active devices
		int reactivated = 0;
		for (obs_source_t *source : activeSources) {
			if (source) { // Check if source pointer is valid
				proc_handler_t *ph = obs_source_get_proc_handler(source);
				if (ph) {
					calldata_t cd;
					calldata_init(&cd);
					calldata_set_bool(&cd, "active", true);
					if (proc_handler_call(ph, "activate", &cd)) {
						reactivated++;
					}
					calldata_free(&cd);
				} else {
					// Fallback to old method
					obs_source_set_enabled(source, true);
					reactivated++;
				}
			}
			obs_source_release(source); // Release our reference
		}
		
		if (sendNotification) {
			QString message = QString(obs_module_text("Feature.VideoCapture.Refresh.NotifyRefreshed")).arg(reactivated);
			StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Menu.VideoCapture.Root"), message);
		}
		
		StreamUP::DebugLogger::LogInfoFormat("VideoCapture", "Refreshed %d video capture devices", reactivated);
	});
	
	StreamUP::DebugLogger::LogInfoFormat("VideoCapture", "Started refresh process for %d video capture devices", 
		 static_cast<int>(activeSources.size()));
	return true;
}

void ActivateAllVideoCaptureDevicesDialog()
{
	CreateToolDialog("Feature.VideoCapture.Activate.Info1", "Feature.VideoCapture.Activate.Info2",
			 "Feature.VideoCapture.Activate.Info3",
			 QString("Feature.VideoCapture.Activate.Title"),
			 []() { ActivateAllVideoCaptureDevices(true); },
			 QString(), // No JSON needed for this action
			 "Feature.VideoCapture.Activate.HowTo1", "Feature.VideoCapture.Activate.HowTo2",
			 "Feature.VideoCapture.Activate.HowTo3", "Feature.VideoCapture.Activate.HowTo4",
			 "Feature.VideoCapture.Activate.Notification");
}

void DeactivateAllVideoCaptureDevicesDialog()
{
	CreateToolDialog("Feature.VideoCapture.Deactivate.Info1", "Feature.VideoCapture.Deactivate.Info2",
			 "Feature.VideoCapture.Deactivate.Info3",
			 QString("Feature.VideoCapture.Deactivate.Title"),
			 []() { DeactivateAllVideoCaptureDevices(true); },
			 QString(), // No JSON needed for this action
			 "Feature.VideoCapture.Deactivate.HowTo1", "Feature.VideoCapture.Deactivate.HowTo2",
			 "Feature.VideoCapture.Deactivate.HowTo3", "Feature.VideoCapture.Deactivate.HowTo4",
			 "Feature.VideoCapture.Deactivate.Notification");
}

void RefreshAllVideoCaptureDevicesDialog()
{
	CreateToolDialog("Feature.VideoCapture.Refresh.Info1", "Feature.VideoCapture.Refresh.Info2",
			 "Feature.VideoCapture.Refresh.Info3",
			 QString("Feature.VideoCapture.Refresh.Title"),
			 []() { RefreshAllVideoCaptureDevices(true); },
			 QString(), // No JSON needed for this action
			 "Feature.VideoCapture.Refresh.HowTo1", "Feature.VideoCapture.Refresh.HowTo2",
			 "Feature.VideoCapture.Refresh.HowTo3", "Feature.VideoCapture.Refresh.HowTo4",
			 "Feature.VideoCapture.Refresh.Notification");
}

//-------------------GROUP MANAGEMENT FUNCTIONS-------------------
bool GroupSelectedSources(bool sendNotification)
{
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		if (sendNotification) {
			StreamUP::NotificationManager::SendWarningNotification(obs_module_text("Feature.GroupSources.Title"),
							      obs_module_text("Feature.GroupSources.NotifyNoScene"));
		}
		return false;
	}

	obs_scene_t *scene = obs_scene_from_source(current_scene);
	if (!scene) {
		obs_source_release(current_scene);
		if (sendNotification) {
			StreamUP::NotificationManager::SendWarningNotification(obs_module_text("Feature.GroupSources.Title"),
							      obs_module_text("Feature.GroupSources.NotifyInvalidScene"));
		}
		return false;
	}

	// Find all selected items
	SceneFindBoxData data;
	obs_scene_enum_items(scene, FindSelected, &data);

	if (data.sceneItems.empty()) {
		obs_source_release(current_scene);
		if (sendNotification) {
			StreamUP::NotificationManager::SendWarningNotification(obs_module_text("Feature.GroupSources.Title"),
							      obs_module_text("Feature.GroupSources.NotifyNoSelection"));
		}
		return false;
	}

	// OBS itself allows grouping a single selected source, so we do too (empty is
	// already handled above).

	// Create a unique group name
	QString baseName = "Group";
	QString groupName = baseName;
	int counter = 1;
	obs_source_t *existing = nullptr;
	while ((existing = obs_get_source_by_name(groupName.toUtf8().constData())) != nullptr) {
		obs_source_release(existing);
		groupName = QString("%1 %2").arg(baseName).arg(counter++);
	}

	// Add the group to the scene at the position of the first selected item
	obs_sceneitem_t *group_item = obs_scene_insert_group(scene, groupName.toUtf8().constData(), &data.sceneItems[0], data.sceneItems.size());

	if (!group_item) {
		obs_source_release(current_scene);
		if (sendNotification) {
			StreamUP::NotificationManager::SendWarningNotification(obs_module_text("Feature.GroupSources.Title"),
							      obs_module_text("Feature.GroupSources.NotifyFailed"));
		}
		return false;
	}

	obs_source_release(current_scene);

	if (sendNotification) {
		QString message = QString(obs_module_text("Feature.GroupSources.NotifyGrouped")).arg(data.sceneItems.size());
		StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Feature.GroupSources.Title"), message);
	}

	StreamUP::DebugLogger::LogInfoFormat("GroupSources", "Grouped %d sources into '%s'",
		static_cast<int>(data.sceneItems.size()), groupName.toUtf8().constData());

	return true;
}

//-------------------VISIBILITY MANAGEMENT FUNCTIONS-------------------
bool CheckIfAnySelectedVisible()
{
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) return false;

	obs_scene_t *scene = obs_scene_from_source(current_scene);
	if (!scene) {
		obs_source_release(current_scene);
		return false;
	}

	SceneFindBoxData data;
	obs_scene_enum_items(scene, FindSelected, &data);

	bool any_visible = false;
	for (obs_sceneitem_t *item : data.sceneItems) {
		if (obs_sceneitem_visible(item)) {
			any_visible = true;
			break;
		}
	}

	obs_source_release(current_scene);
	return any_visible;
}

bool HasAnySelectedSceneItem()
{
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) return false;

	obs_scene_t *scene = obs_scene_from_source(current_scene);
	if (!scene) {
		obs_source_release(current_scene);
		return false;
	}

	SceneFindBoxData data;
	obs_scene_enum_items(scene, FindSelected, &data);

	const bool any = !data.sceneItems.empty();
	obs_source_release(current_scene);
	return any;
}

bool ToggleVisibilitySelectedSources(bool sendNotification)
{
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		if (sendNotification) {
			StreamUP::NotificationManager::SendWarningNotification(obs_module_text("Feature.ToggleVisibility.Title"),
							      obs_module_text("Feature.ToggleVisibility.NotifyNoScene"));
		}
		return false;
	}

	obs_scene_t *scene = obs_scene_from_source(current_scene);
	if (!scene) {
		obs_source_release(current_scene);
		if (sendNotification) {
			StreamUP::NotificationManager::SendWarningNotification(obs_module_text("Feature.ToggleVisibility.Title"),
							      obs_module_text("Feature.ToggleVisibility.NotifyInvalidScene"));
		}
		return false;
	}

	// Find all selected items
	SceneFindBoxData data;
	obs_scene_enum_items(scene, FindSelected, &data);

	if (data.sceneItems.empty()) {
		obs_source_release(current_scene);
		if (sendNotification) {
			StreamUP::NotificationManager::SendWarningNotification(obs_module_text("Feature.ToggleVisibility.Title"),
							      obs_module_text("Feature.ToggleVisibility.NotifyNoSelection"));
		}
		return false;
	}

	// Check if any selected items are visible
	bool any_visible = false;
	for (obs_sceneitem_t *item : data.sceneItems) {
		if (obs_sceneitem_visible(item)) {
			any_visible = true;
			break;
		}
	}

	// Toggle visibility: if any are visible, hide all; if all are hidden, show all
	bool new_visibility = !any_visible;
	for (obs_sceneitem_t *item : data.sceneItems) {
		obs_sceneitem_set_visible(item, new_visibility);
	}

	obs_source_release(current_scene);

	if (sendNotification) {
		QString message = QString(obs_module_text(new_visibility ? "Feature.ToggleVisibility.NotifyShown"
								    : "Feature.ToggleVisibility.NotifyHidden"))
			.arg(data.sceneItems.size());
		StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Feature.ToggleVisibility.Title"), message);
	}

	StreamUP::DebugLogger::LogInfoFormat("ToggleVisibility", "%s %d selected sources",
		new_visibility ? "Shown" : "Hidden", static_cast<int>(data.sceneItems.size()));

	return true;
}

} // namespace SourceManager
} // namespace StreamUP
