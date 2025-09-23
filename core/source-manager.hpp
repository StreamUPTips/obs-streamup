#ifndef STREAMUP_SOURCE_MANAGER_HPP
#define STREAMUP_SOURCE_MANAGER_HPP

#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-source.h>

namespace StreamUP {
namespace SourceManager {

// Audio monitoring functions
/**
 * Refresh audio monitoring for a source (callback for obs_enum_sources)
 * @param data Unused parameter
 * @param source The source to refresh audio monitoring for
 * @return bool Always returns true to continue enumeration
 */
bool RefreshAudioMonitoring(void *data, obs_source_t *source);

/**
 * Show dialog and refresh audio monitoring for all sources
 */
void RefreshAudioMonitoringDialog();

// Browser source functions
/**
 * Refresh browser sources (callback for obs_enum_sources)
 * @param data Unused parameter
 * @param source The source to refresh if it's a browser source
 * @return bool Always returns true to continue enumeration
 */
bool RefreshBrowserSources(void *data, obs_source_t *source);

/**
 * Show dialog and refresh all browser sources
 */
void RefreshBrowserSourcesDialog();

// Source locking functions
/**
 * Find if any source is selected in a scene
 * @param scene The scene to search
 * @param item The scene item to check
 * @param param Pointer to SceneItemEnumData struct
 * @return bool True to continue enumeration
 */
bool FindSelected(obs_scene_t *scene, obs_sceneitem_t *item, void *param);

/**
 * Check if any sources are unlocked in a scene
 * @param scene The scene to check
 * @return bool True if any sources are unlocked
 */
bool CheckIfAnyUnlocked(obs_scene_t *scene);

/**
 * Check if any group items are unlocked
 * @param group_source The group source to check
 * @return bool True if any items in the group are unlocked
 */
bool CheckGroupItemsIfAnyUnlocked(obs_source_t *group_source);

/**
 * Toggle lock state of all sources in current scene
 * @param sendNotification Whether to send a notification
 * @return bool True if operation was successful
 */
bool ToggleLockSourcesInCurrentScene(bool sendNotification = true);

/**
 * Toggle lock state of all sources in all scenes
 * @param sendNotification Whether to send a notification
 * @return bool True if operation was successful
 */
bool ToggleLockAllSources(bool sendNotification = true);

/**
 * Check if any sources are unlocked in all scenes
 * @return bool True if any sources are unlocked across all scenes
 */
bool CheckIfAnyUnlockedInAllScenes();

/**
 * Show dialog for locking all sources
 */
void LockAllSourcesDialog();

/**
 * Show dialog for locking current scene sources
 */
void LockAllCurrentSourcesDialog();

// Helper functions for scene item manipulation
/**
 * Toggle lock state of scene items
 * @param scene The scene containing the items
 * @param lock True to lock, false to unlock
 * @return bool True if operation was successful
 */
bool ToggleLockSceneItems(obs_scene_t *scene, bool lock);

/**
 * Toggle lock state of group items
 * @param group The group source
 * @param lock True to lock, false to unlock
 */
void ToggleLockGroupItems(obs_source_t *group, bool lock);

/**
 * Callback function for toggling lock state of scene items
 * @param scene The scene containing the item
 * @param item The scene item to toggle
 * @param param Pointer to bool indicating lock state
 * @return bool True to continue enumeration
 */
bool ToggleLockSceneItemCallback(obs_scene_t *scene, obs_sceneitem_t *item, void *param);

/**
 * Get the currently selected source name from the current scene
 * @return const char* The name of the selected source, or nullptr if none/multiple selected
 */
const char *GetSelectedSourceFromCurrentScene();

/**
 * Check if all sources are locked in the current scene (convenience function for dock widget)
 * @return bool True if all sources in current scene are locked
 */
bool AreAllSourcesLockedInCurrentScene();

/**
 * Check if all sources are locked in all scenes (convenience function for dock widget)
 * @return bool True if all sources in all scenes are locked
 */
bool AreAllSourcesLockedInAllScenes();

// Video capture device management functions
/**
 * Activate all video capture device sources
 * @param sendNotification Whether to send a notification
 * @return bool True if operation was successful
 */
bool ActivateAllVideoCaptureDevices(bool sendNotification = true);

/**
 * Deactivate all video capture device sources
 * @param sendNotification Whether to send a notification
 * @return bool True if operation was successful
 */
bool DeactivateAllVideoCaptureDevices(bool sendNotification = true);

/**
 * Refresh all video capture device sources (deactivate then reactivate)
 * @param sendNotification Whether to send a notification
 * @return bool True if operation was successful
 */
bool RefreshAllVideoCaptureDevices(bool sendNotification = true);

/**
 * Show dialog for activating all video capture devices
 */
void ActivateAllVideoCaptureDevicesDialog();

/**
 * Show dialog for deactivating all video capture devices
 */
void DeactivateAllVideoCaptureDevicesDialog();

/**
 * Show dialog for refreshing all video capture devices
 */
void RefreshAllVideoCaptureDevicesDialog();

/**
 * Check if a source is a video capture device
 * @param source The source to check
 * @return bool True if the source is a video capture device
 */
bool IsVideoCaptureDevice(obs_source_t *source);

/**
 * Callback function for video capture device operations
 * @param data Pointer to operation parameters
 * @param source The source to process
 * @return bool True to continue enumeration
 */
bool VideoCaptureDeviceCallback(void *data, obs_source_t *source);

} // namespace SourceManager
} // namespace StreamUP

#endif // STREAMUP_SOURCE_MANAGER_HPP
