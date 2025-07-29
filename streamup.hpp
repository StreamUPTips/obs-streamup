#ifndef STREAMUP_HPP
#define STREAMUP_HPP

#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-source.h>

// These functions are now provided by the SourceManager module
// Include source-manager.hpp to access them
#include "source-manager.hpp"

// Wrapper functions for backward compatibility
inline bool ToggleLockAllSources(bool sendNotification = true) {
    return StreamUP::SourceManager::ToggleLockAllSources(sendNotification);
}

inline bool ToggleLockSourcesInCurrentScene(bool sendNotification = true) {
    return StreamUP::SourceManager::ToggleLockSourcesInCurrentScene(sendNotification);
}

inline bool RefreshBrowserSources(void *data, obs_source_t *source) {
    return StreamUP::SourceManager::RefreshBrowserSources(data, source);
}

inline bool RefreshAudioMonitoring(void *data, obs_source_t *source) {
    return StreamUP::SourceManager::RefreshAudioMonitoring(data, source);
}

inline bool CheckIfAnyUnlocked(obs_scene_t *scene) {
    return StreamUP::SourceManager::CheckIfAnyUnlocked(scene);
}

inline bool CheckIfAnyUnlockedInAllScenes() {
    return StreamUP::SourceManager::CheckIfAnyUnlockedInAllScenes();
}

// Convenience functions for dock widget (now use SourceManager)
inline bool AreAllSourcesLockedInCurrentScene() {
    return StreamUP::SourceManager::AreAllSourcesLockedInCurrentScene();
}

inline bool AreAllSourcesLockedInAllScenes() {
    return StreamUP::SourceManager::AreAllSourcesLockedInAllScenes();
}

// Additional wrapper for GetSelectedSourceFromCurrentScene
inline const char *GetSelectedSourceFromCurrentScene() {
    return StreamUP::SourceManager::GetSelectedSourceFromCurrentScene();
}

// Dialog function wrappers
inline void LockAllSourcesDialog() {
    StreamUP::SourceManager::LockAllSourcesDialog();
}

inline void LockAllCurrentSourcesDialog() {
    StreamUP::SourceManager::LockAllCurrentSourcesDialog();
}

inline void RefreshAudioMonitoringDialog() {
    StreamUP::SourceManager::RefreshAudioMonitoringDialog();
}

inline void RefreshBrowserSourcesDialog() {
    StreamUP::SourceManager::RefreshBrowserSourcesDialog();
}

#endif
