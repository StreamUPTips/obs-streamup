#ifndef STREAMUP_HPP
#define STREAMUP_HPP

#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-source.h>

// StreamUP Plugin Main Header
// All functionality is now provided by modular components.
// To use StreamUP functionality, include the specific module headers:
//   #include "source-manager.hpp"     - Source locking and management
//   #include "file-manager.hpp"       - StreamUP file operations  
//   #include "plugin-manager.hpp"     - Plugin validation and updates
//   #include "websocket-api.hpp"      - WebSocket API endpoints
//   #include "hotkey-manager.hpp"     - Hotkey registration and handling
//   #include "ui-helpers.hpp"         - UI dialog and widget utilities

// Use the following namespace pattern to access functionality:
//   StreamUP::SourceManager::ToggleLockAllSources()
//   StreamUP::FileManager::LoadStreamupFile()
//   StreamUP::PluginManager::CheckAllPluginsForUpdates()
//   StreamUP::WebSocketAPI::WebsocketRequestVersion()
//   StreamUP::HotkeyManager::RegisterHotkeys()
//   StreamUP::UIHelpers::ShowDialogOnUIThread()

namespace StreamUP {

// True once OBS has emitted FINISHED_LOADING.
//
// Modules are loaded from inside OBSBasic::OBSInit, so anything constructed at
// load time runs before the frontend API exists. Calling obs_frontend_* from
// there is an access violation, not a null return. Anything built during module
// load that wants to talk to the frontend must check this first, and anything
// built after that point has already missed the event and can only find out by
// asking.
bool ObsFinishedLoading();

} // namespace StreamUP

#endif
