# --- Detect if the plugin is built out of tree or not ---
if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  set(BUILD_OUT_OF_TREE OFF)
else()
  set(BUILD_OUT_OF_TREE ON)
  cmake_minimum_required(VERSION 3.18)
endif()

project(streamup VERSION 1.7.1)
set(PROJECT_FULL_NAME "StreamUP")

# Set new UUIDs when you start to create a new plugin.
set(MACOS_PACKAGE_UUID "526F4EB1-783F-466E-AA20-AE4F7461CE7E")
set(MACOS_INSTALLER_UUID "C18E7256-5BEF-496C-A390-B71FC4935209")

add_library(${PROJECT_NAME} MODULE)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)

target_sources(${PROJECT_NAME} PRIVATE
	streamup.cpp
	streamup.hpp
  streamup-dock.cpp
  streamup-dock.hpp
  StreamUPDock.ui
  obs-websocket-api.h
  resources.qrc
	version.h)

find_package(CURL REQUIRED)
if(BUILD_OUT_OF_TREE)
    find_package(libobs REQUIRED)
    find_package(obs-frontend-api REQUIRED)
    include(cmake/ObsPluginHelpers.cmake)
    set(OBS_FRONTEND_API_NAME "obs-frontend-api")
else()
    set(OBS_FRONTEND_API_NAME "frontend-api")
endif()

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

# Find Qt components
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)

set_target_properties(${PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)
set(UI_FILES StreamUPDock.ui)

target_link_libraries(${PROJECT_NAME}
		OBS::${OBS_FRONTEND_API_NAME}
    CURL::libcurl
		Qt6::Widgets
    Qt6::Gui
		OBS::libobs)

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
	target_include_directories(${PROJECT_NAME} PRIVATE
		"${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
	if(OBS_CMAKE_VERSION VERSION_GREATER_EQUAL 3.0.0)
		set_target_properties_obs(${PROJECT_NAME} PROPERTIES FOLDER "plugins/andilippi" PREFIX "")
	else()
		set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "plugins/andilippi")
		setup_plugin_target(${PROJECT_NAME})
	endif()
endif()
