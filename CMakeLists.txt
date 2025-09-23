# --- Detect if the plugin is build out of tree or not ---
if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  set(BUILD_OUT_OF_TREE OFF)
else()
  set(BUILD_OUT_OF_TREE ON)
  cmake_minimum_required(VERSION 3.16...3.26)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${PROJECT_NAME} MODULE)

target_link_libraries(${PROJECT_NAME} PRIVATE OBS::libobs)

if(BUILD_OUT_OF_TREE)
  find_package(libobs REQUIRED)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE OBS::frontend-api)
endif()

find_package(Qt6 COMPONENTS Widgets Core)
if(BUILD_OUT_OF_TREE)
  if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
    find_package(Qt6 REQUIRED Gui)
  endif()
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets)

if((OS_LINUX OR OS_FREEBSD OR OS_OPENBSD) AND Qt_VERSION VERSION_LESS "6.9.0")
	target_link_libraries(${PROJECT_NAME} PRIVATE Qt::GuiPrivate)
endif()

target_compile_options(
${PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                -Wno-comma>)
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_target_properties(
${PROJECT_NAME}
PROPERTIES AUTOMOC ON
            AUTOUIC ON
            AUTORCC ON)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)

if(OS_WINDOWS)
  configure_file(cmake/windows/resources/installer-Windows.iss.in "${CMAKE_CURRENT_BINARY_DIR}/installer-Windows.generated.iss")
endif()

# Root files
target_sources(${PROJECT_NAME} PRIVATE
	streamup.cpp
	streamup.hpp
  video-capture-popup.cpp
  video-capture-popup.hpp
  flow-layout.cpp
  flow-layout.hpp
  StreamUPDock.ui
  obs-websocket-api.h
  resources.qrc
	version.h)

# Core module files
target_sources(${PROJECT_NAME} PRIVATE
  core/streamup-common.hpp
  core/streamup-common.cpp
  core/plugin-state.hpp
  core/plugin-state.cpp
  core/source-manager.hpp
  core/source-manager.cpp
  core/file-manager.hpp
  core/file-manager.cpp
  core/plugin-manager.hpp
  core/plugin-manager.cpp)

# Utilities module files
target_sources(${PROJECT_NAME} PRIVATE
  utilities/path-utils.hpp
  utilities/path-utils.cpp
  utilities/string-utils.hpp
  utilities/string-utils.cpp
  utilities/version-utils.hpp
  utilities/version-utils.cpp
  utilities/http-client.hpp
  utilities/http-client.cpp
  utilities/obs-wrappers.hpp
  utilities/error-handler.hpp
  utilities/error-handler.cpp
  utilities/obs-data-helpers.hpp
  utilities/obs-data-helpers.cpp
  utilities/debug-logger.hpp
  utilities/debug-logger.cpp)

# UI module files
target_sources(${PROJECT_NAME} PRIVATE
  ui/hotkey-manager.hpp
  ui/hotkey-manager.cpp
  ui/hotkey-widget.hpp
  ui/hotkey-widget.cpp
  ui/ui-helpers.hpp
  ui/ui-helpers.cpp
  ui/ui-styles.hpp
  ui/ui-styles.cpp
  ui/switch-button.hpp
  ui/switch-button.cpp
  ui/menu-manager.hpp
  ui/menu-manager.cpp
  ui/settings-manager.hpp
  ui/settings-manager.cpp
  ui/notification-manager.hpp
  ui/notification-manager.cpp
  ui/splash-screen.hpp
  ui/splash-screen.cpp
  ui/patch-notes-window.hpp
  ui/patch-notes-window.cpp
  ui/theme-window.hpp
  ui/theme-window.cpp
  ui/websocket-window.hpp
  ui/websocket-window.cpp
  ui/dock/streamup-dock.hpp
  ui/dock/streamup-dock.cpp
  ui/scene-organiser/scene-organiser-dock.hpp
  ui/scene-organiser/scene-organiser-dock.cpp
  ui/streamup-toolbar.hpp
  ui/streamup-toolbar.cpp
  ui/streamup-toolbar-config.hpp
  ui/streamup-toolbar-config.cpp
  ui/streamup-toolbar-configurator.hpp
  ui/streamup-toolbar-configurator.cpp
  ui/obs-hotkey-manager.hpp
  ui/obs-hotkey-manager.cpp
  ui/icon-selector-dialog.hpp
  ui/icon-selector-dialog.cpp
  ui/hotkey-selector-dialog.hpp
  ui/hotkey-selector-dialog.cpp
  ui/hotkey-button-config-dialog.hpp
  ui/hotkey-button-config-dialog.cpp)

# Integrations module files
target_sources(${PROJECT_NAME} PRIVATE
  integrations/websocket-api.hpp
  integrations/websocket-api.cpp)

# MultiDock module files
target_sources(${PROJECT_NAME} PRIVATE
  multidock/multidock_utils.hpp
  multidock/multidock_utils.cpp
  multidock/multidock_dialogs.hpp
  multidock/multidock_dialogs.cpp
  multidock/persistence.hpp
  multidock/persistence.cpp
  multidock/inner_dock_host.hpp
  multidock/inner_dock_host.cpp
  multidock/add_dock_dialog.hpp
  multidock/add_dock_dialog.cpp
  multidock/multidock_dock.hpp
  multidock/multidock_dock.cpp
  multidock/multidock_manager.hpp
  multidock/multidock_manager.cpp)

# Create source groups for Visual Studio folder structure
source_group("Root" FILES
	streamup.cpp
	streamup.hpp
  video-capture-popup.cpp
  video-capture-popup.hpp
  flow-layout.cpp
  flow-layout.hpp
  StreamUPDock.ui
  obs-websocket-api.h
  resources.qrc
	version.h)

source_group("Core" FILES
  core/streamup-common.hpp
  core/streamup-common.cpp
  core/plugin-state.hpp
  core/plugin-state.cpp
  core/source-manager.hpp
  core/source-manager.cpp
  core/file-manager.hpp
  core/file-manager.cpp
  core/plugin-manager.hpp
  core/plugin-manager.cpp)

source_group("Utilities" FILES
  utilities/path-utils.hpp
  utilities/path-utils.cpp
  utilities/string-utils.hpp
  utilities/string-utils.cpp
  utilities/version-utils.hpp
  utilities/version-utils.cpp
  utilities/http-client.hpp
  utilities/http-client.cpp
  utilities/obs-wrappers.hpp
  utilities/error-handler.hpp
  utilities/error-handler.cpp
  utilities/obs-data-helpers.hpp
  utilities/obs-data-helpers.cpp
  utilities/debug-logger.hpp
  utilities/debug-logger.cpp)

source_group("UI" FILES
  ui/hotkey-manager.hpp
  ui/hotkey-manager.cpp
  ui/hotkey-widget.hpp
  ui/hotkey-widget.cpp
  ui/ui-helpers.hpp
  ui/ui-helpers.cpp
  ui/ui-styles.hpp
  ui/ui-styles.cpp
  ui/switch-button.hpp
  ui/switch-button.cpp
  ui/menu-manager.hpp
  ui/menu-manager.cpp
  ui/settings-manager.hpp
  ui/settings-manager.cpp
  ui/notification-manager.hpp
  ui/notification-manager.cpp
  ui/splash-screen.hpp
  ui/splash-screen.cpp
  ui/patch-notes-window.hpp
  ui/patch-notes-window.cpp
  ui/websocket-window.hpp
  ui/websocket-window.cpp)

source_group("UI\\Dock" FILES
  ui/dock/streamup-dock.hpp
  ui/dock/streamup-dock.cpp)

source_group("UI\\Scene Organiser" FILES
  ui/scene-organiser/scene-organiser-dock.hpp
  ui/scene-organiser/scene-organiser-dock.cpp)

source_group("UI\\Toolbar" FILES
  ui/streamup-toolbar.hpp
  ui/streamup-toolbar.cpp
  ui/streamup-toolbar-config.hpp
  ui/streamup-toolbar-config.cpp
  ui/streamup-toolbar-configurator.hpp
  ui/streamup-toolbar-configurator.cpp
  ui/obs-hotkey-manager.hpp
  ui/obs-hotkey-manager.cpp
  ui/icon-selector-dialog.hpp
  ui/icon-selector-dialog.cpp
  ui/hotkey-selector-dialog.hpp
  ui/hotkey-selector-dialog.cpp
  ui/hotkey-button-config-dialog.hpp
  ui/hotkey-button-config-dialog.cpp)

source_group("Integrations" FILES
  integrations/websocket-api.hpp
  integrations/websocket-api.cpp)

source_group("MultiDock" FILES
  multidock/multidock_utils.hpp
  multidock/multidock_utils.cpp
  multidock/multidock_dialogs.hpp
  multidock/multidock_dialogs.cpp
  multidock/persistence.hpp
  multidock/persistence.cpp
  multidock/inner_dock_host.hpp
  multidock/inner_dock_host.cpp
  multidock/add_dock_dialog.hpp
  multidock/add_dock_dialog.cpp
  multidock/multidock_dock.hpp
  multidock/multidock_dock.cpp
  multidock/multidock_manager.hpp
  multidock/multidock_manager.cpp)

if(BUILD_OUT_OF_TREE)
	set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
else()
	set_target_properties_obs(${PROJECT_NAME} PROPERTIES FOLDER "plugins/exeldro" PREFIX "")
endif()