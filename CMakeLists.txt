project(streamup VERSION 0.0.3)
set(PROJECT_FULL_NAME "StreamUP")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)

set(streamup_HEADERS
	streamup.hpp
	version.h)
set(streamup_SOURCES
	streamup.cpp)

if(WIN32)
	get_filename_component(ISS_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_FILES_DIR}" ISS_FILES_DIR)
	get_filename_component(ISS_PACKAGE_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_PACKAGE_DIR}" ISS_PACKAGE_DIR)
	get_filename_component(ISS_SOURCE_DIR "${PROJECT_SOURCE_DIR}" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_SOURCE_DIR}" ISS_SOURCE_DIR)
	configure_file("installer.iss.in"
			"${PROJECT_BINARY_DIR}/installer.iss"
		)
	configure_file(resource.rc.in streamup.rc)
	list(APPEND streamup_SOURCES
		streamup.rc)
endif()

add_library(streamup MODULE
	${streamup_HEADERS}
	${streamup_SOURCES})

target_link_libraries(streamup
		obs-frontend-api
		Qt5::Widgets
		libobs)

set_target_properties(streamup PROPERTIES FOLDER "plugins/exeldro")
install_obs_plugin_with_data(streamup data)
