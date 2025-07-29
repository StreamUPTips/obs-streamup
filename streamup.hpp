#ifndef STREAMUP_HPP
#define STREAMUP_HPP

#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-source.h>

// These functions are now provided by modular components
// Include relevant module headers to access them
#include "source-manager.hpp"
#include "file-manager.hpp"
#include "plugin-manager.hpp"

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

// File management function wrappers
inline bool LoadStreamupFileFromPath(const QString &file_path, bool forceLoad = false) {
    return StreamUP::FileManager::LoadStreamupFileFromPath(file_path, forceLoad);
}

inline void LoadStreamupFile(bool forceLoad = false) {
    StreamUP::FileManager::LoadStreamupFile(forceLoad);
}

// Plugin management function wrappers
inline void CheckAllPluginsForUpdates(bool manuallyTriggered) {
    StreamUP::PluginManager::CheckAllPluginsForUpdates(manuallyTriggered);
}

inline void InitialiseRequiredModules() {
    StreamUP::PluginManager::InitialiseRequiredModules();
}

inline bool CheckrequiredOBSPlugins(bool isLoadStreamUpFile = false) {
    return StreamUP::PluginManager::CheckrequiredOBSPlugins(isLoadStreamUpFile);
}

#endif
