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
#include "websocket-api.hpp"
#include "hotkey-manager.hpp"

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

// WebSocket API function wrappers
inline void WebsocketRequestBitrate(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketRequestBitrate(request_data, response_data, private_data);
}

inline void WebsocketRequestVersion(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketRequestVersion(request_data, response_data, private_data);
}

inline void WebsocketRequestCheckPlugins(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketRequestCheckPlugins(request_data, response_data, private_data);
}

inline void WebsocketRequestLockAllSources(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketRequestLockAllSources(request_data, response_data, private_data);
}

inline void WebsocketRequestLockCurrentSources(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketRequestLockCurrentSources(request_data, response_data, private_data);
}

inline void WebsocketRequestRefreshAudioMonitoring(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketRequestRefreshAudioMonitoring(request_data, response_data, private_data);
}

inline void WebsocketRequestRefreshBrowserSources(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketRequestRefreshBrowserSources(request_data, response_data, private_data);
}

inline void WebsocketRequestGetCurrentSelectedSource(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketRequestGetCurrentSelectedSource(request_data, response_data, private_data);
}

inline void WebsocketRequestGetShowTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketRequestGetShowTransition(request_data, response_data, private_data);
}

inline void WebsocketRequestGetHideTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketRequestGetHideTransition(request_data, response_data, private_data);
}

inline void WebsocketRequestSetShowTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketRequestSetShowTransition(request_data, response_data, private_data);
}

inline void WebsocketRequestSetHideTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketRequestSetHideTransition(request_data, response_data, private_data);
}

inline void WebsocketRequestGetOutputFilePath(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketRequestGetOutputFilePath(request_data, response_data, private_data);
}

inline void WebsocketRequestVLCGetCurrentFile(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketRequestVLCGetCurrentFile(request_data, response_data, private_data);
}

inline void WebsocketLoadStreamupFile(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketLoadStreamupFile(request_data, response_data, private_data);
}

inline void WebsocketOpenSourceProperties(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketOpenSourceProperties(request_data, response_data, private_data);
}

inline void WebsocketOpenSourceFilters(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketOpenSourceFilters(request_data, response_data, private_data);
}

inline void WebsocketOpenSourceInteract(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketOpenSourceInteract(request_data, response_data, private_data);
}

inline void WebsocketOpenSceneFilters(obs_data_t *request_data, obs_data_t *response_data, void *private_data) {
    StreamUP::WebSocketAPI::WebsocketOpenSceneFilters(request_data, response_data, private_data);
}

// Hotkey Manager function wrappers
inline void HotkeyRefreshBrowserSources(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
    StreamUP::HotkeyManager::HotkeyRefreshBrowserSources(data, id, hotkey, pressed);
}

inline void HotkeyLockAllSources(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
    StreamUP::HotkeyManager::HotkeyLockAllSources(data, id, hotkey, pressed);
}

inline void HotkeyRefreshAudioMonitoring(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
    StreamUP::HotkeyManager::HotkeyRefreshAudioMonitoring(data, id, hotkey, pressed);
}

inline void HotkeyLockCurrentSources(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
    StreamUP::HotkeyManager::HotkeyLockCurrentSources(data, id, hotkey, pressed);
}

inline void HotkeyOpenSourceProperties(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
    StreamUP::HotkeyManager::HotkeyOpenSourceProperties(data, id, hotkey, pressed);
}

inline void HotkeyOpenSourceFilters(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
    StreamUP::HotkeyManager::HotkeyOpenSourceFilters(data, id, hotkey, pressed);
}

inline void HotkeyOpenSourceInteract(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
    StreamUP::HotkeyManager::HotkeyOpenSourceInteract(data, id, hotkey, pressed);
}

inline void HotkeyOpenSceneFilters(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
    StreamUP::HotkeyManager::HotkeyOpenSceneFilters(data, id, hotkey, pressed);
}

inline void SaveLoadHotkeys(obs_data_t *save_data, bool saving, void *param) {
    StreamUP::HotkeyManager::SaveLoadHotkeys(save_data, saving, param);
}

inline void RegisterHotkeys() {
    StreamUP::HotkeyManager::RegisterHotkeys();
}

inline void UnregisterHotkeys() {
    StreamUP::HotkeyManager::UnregisterHotkeys();
}

#endif
