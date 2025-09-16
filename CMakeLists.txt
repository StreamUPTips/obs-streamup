# --- Detect if the plugin is built out of tree or not ---
if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  set(BUILD_OUT_OF_TREE OFF)
else()
  set(BUILD_OUT_OF_TREE ON)
  cmake_minimum_required(VERSION 3.18)
endif()

project(streamup VERSION 2.0.0)
set(PROJECT_FULL_NAME "StreamUP")

# Set new UUIDs when you start to create a new plugin.
set(MACOS_PACKAGE_UUID "526F4EB1-783F-466E-AA20-AE4F7461CE7E")
set(MACOS_INSTALLER_UUID "C18E7256-5BEF-496C-A390-B71FC4935209")

add_library(${PROJECT_NAME} MODULE)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)

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

find_package(CURL REQUIRED)
if(BUILD_OUT_OF_TREE)
    find_package(libobs REQUIRED)
    find_package(obs-frontend-api REQUIRED)
    include(cmake/ObsPluginHelpers.cmake)
    set(OBS_FRONTEND_API_NAME "obs-frontend-api")
else()
    set(OBS_FRONTEND_API_NAME "frontend-api")
endif()

# Set common include directories for both build modes
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/utilities"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/integrations"
    "${CMAKE_CURRENT_SOURCE_DIR}/multidock")

if(OS_WINDOWS)
	get_filename_component(ISS_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_FILES_DIR}" ISS_FILES_DIR)
	get_filename_component(ISS_PACKAGE_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_PACKAGE_DIR}" ISS_PACKAGE_DIR)
	get_filename_component(ISS_SOURCE_DIR "${PROJECT_SOURCE_DIR}" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_SOURCE_DIR}" ISS_SOURCE_DIR)
	configure_file("installer.iss.in"
			"${PROJECT_BINARY_DIR}/installer.iss"
		)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resource.rc.in ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.rc)
	target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.rc)
  target_link_libraries(${PROJECT_NAME} w32-pthreads)
	
elseif(OS_MACOS)
	set(MACOS_BUNDLEID "com.andilippi.${PROJECT_NAME}")
	set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
	set(MACOSX_PLUGIN_BUNDLE_VERSION "${PROJECT_VERSION}")
	set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")
	configure_file(cmake/bundle/macos/installer-macos.pkgproj.in ${CMAKE_BINARY_DIR}/installer-macos.generated.pkgproj)
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
endif()

# Find Qt components (order matters for dependencies)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg)

# Enable Qt Meta-Object Compiler, UI Compiler, and Resource Compiler
set_target_properties(${PROJECT_NAME} PROPERTIES 
    AUTOMOC ON 
    AUTOUIC ON 
    AUTORCC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)

# Link required libraries (grouped by dependency order)
target_link_libraries(${PROJECT_NAME}
    # OBS Studio libraries
    OBS::libobs
    OBS::${OBS_FRONTEND_API_NAME}
    # Qt6 libraries (Core must come before others)
    Qt6::Core
    Qt6::Gui  
    Qt6::Widgets
    Qt6::Svg
    # External libraries
    CURL::libcurl)

if(BUILD_OUT_OF_TREE)
	if(NOT LIB_OUT_DIR)
		set(LIB_OUT_DIR "/lib/obs-plugins")
	endif()
	if(NOT DATA_OUT_DIR)
		set(DATA_OUT_DIR "/share/obs/obs-plugins/${PROJECT_NAME}")
	endif()
	set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
	install(TARGETS ${PROJECT_NAME}
		LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
	install(DIRECTORY data/locale
		DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
	setup_plugin_target(${PROJECT_NAME})
else()
	# Add OBS frontend API include directory for in-tree builds
	target_include_directories(${PROJECT_NAME} PRIVATE
		"${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
	if(OBS_CMAKE_VERSION VERSION_GREATER_EQUAL 3.0.0)
		set_target_properties_obs(${PROJECT_NAME} PROPERTIES FOLDER "plugins/andilippi" PREFIX "")
	else()
		set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "plugins/andilippi")
		setup_plugin_target(${PROJECT_NAME})
	endif()
endif()
