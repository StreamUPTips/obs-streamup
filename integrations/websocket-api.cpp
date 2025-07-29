#include "websocket-api.hpp"
#include "streamup-common.hpp"
#include "version.h"
#include "plugin-manager.hpp"
#include "source-manager.hpp"
#include "file-manager.hpp"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>
#include <QString>

// Forward declarations for functions from main streamup.cpp
extern void GetShowHideTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data, bool transition_type);
extern void SetShowHideTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data, bool show_transition);

namespace StreamUP {
namespace WebSocketAPI {

//-------------------UTILITY FUNCTIONS-------------------
void WebsocketRequestBitrate(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	obs_output_t *streamOutput = obs_frontend_get_streaming_output();
	if (!streamOutput || !obs_frontend_streaming_active()) {
		obs_data_set_string(response_data, "error", "Streaming is not active.");
		return;
	}

	uint64_t bytesSent = obs_output_get_total_bytes(streamOutput);
	uint64_t currentTime = os_gettime_ns();
	static uint64_t lastBytesSent = 0;
	static uint64_t lastTime = 0;
	static bool initialized = false;

	if (!initialized) {
		lastBytesSent = bytesSent;
		lastTime = currentTime;
		initialized = true;
		obs_data_set_int(response_data, "kbits-per-sec", 0);
		return;
	}

	if (bytesSent < lastBytesSent) {
		bytesSent = 0;
	}

	uint64_t bytesBetween = bytesSent - lastBytesSent;
	double timePassed = (currentTime - lastTime) / 1000000000.0;

	// Ensure timePassed is greater than zero to avoid division by zero
	uint64_t bytesPerSec = 0;
	if (timePassed > 0)
		bytesPerSec = bytesBetween / timePassed;

	uint64_t kbitsPerSec = (bytesPerSec * 8) / 1024;

	lastBytesSent = bytesSent;
	lastTime = currentTime;

	obs_data_set_int(response_data, "kbits-per-sec", kbitsPerSec);
}

void WebsocketRequestVersion(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);
	obs_data_set_string(response_data, "version", PROJECT_VERSION);
	obs_data_set_bool(response_data, "success", true);
}

//-------------------PLUGIN MANAGEMENT-------------------
void WebsocketRequestCheckPlugins(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);
	bool pluginsUpToDate = StreamUP::PluginManager::CheckrequiredOBSPlugins(true);
	obs_data_set_bool(response_data, "success", pluginsUpToDate);
}

//-------------------SOURCE MANAGEMENT-------------------
void WebsocketRequestLockAllSources(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);
	bool lockState = StreamUP::SourceManager::ToggleLockAllSources();
	obs_data_set_bool(response_data, "lockState", lockState);
}

void WebsocketRequestLockCurrentSources(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);
	bool lockState = StreamUP::SourceManager::ToggleLockSourcesInCurrentScene();
	obs_data_set_bool(response_data, "lockState", lockState);
}

void WebsocketRequestRefreshAudioMonitoring(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);
	obs_enum_sources(StreamUP::SourceManager::RefreshAudioMonitoring, nullptr);
	obs_data_set_bool(response_data, "Audio monitoring refreshed", true);
}

void WebsocketRequestRefreshBrowserSources(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);
	obs_enum_sources(StreamUP::SourceManager::RefreshBrowserSources, nullptr);
	obs_data_set_bool(response_data, "Browser sources refreshed", true);
}

void WebsocketRequestGetCurrentSelectedSource(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	const char *selected_source_name = StreamUP::SourceManager::GetSelectedSourceFromCurrentScene();
	if (selected_source_name) {
		obs_data_set_string(response_data, "selectedSource", selected_source_name);
	} else {
		blog(LOG_INFO, "[StreamUP] No selected source.");
		obs_data_set_string(response_data, "selectedSource", "None");
	}
}

//-------------------TRANSITION MANAGEMENT-------------------
void WebsocketRequestGetShowTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	GetShowHideTransition(request_data, response_data, private_data, true);
}

void WebsocketRequestGetHideTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	GetShowHideTransition(request_data, response_data, private_data, false);
}

void WebsocketRequestSetShowTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	SetShowHideTransition(request_data, response_data, private_data, true);
}

void WebsocketRequestSetHideTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	SetShowHideTransition(request_data, response_data, private_data, false);
}

//-------------------OUTPUT AND FILE MANAGEMENT-------------------
void WebsocketRequestGetOutputFilePath(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	char *path = obs_frontend_get_current_record_output_path();
	obs_data_set_string(response_data, "outputFilePath", path);
}

void WebsocketRequestVLCGetCurrentFile(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	const char *source_name = obs_data_get_string(request_data, "sourceName");
	if (!source_name) {
		obs_data_set_string(response_data, "error", "No source name provided");
		return;
	}

	obs_source_t *source = obs_get_source_by_name(source_name);
	if (!source) {
		obs_data_set_string(response_data, "error", "Source not found");
		return;
	}

	if (strcmp(obs_source_get_unversioned_id(source), "vlc_source") == 0) {
		proc_handler_t *ph = obs_source_get_proc_handler(source);
		if (ph) {
			calldata_t cd;
			calldata_init(&cd);
			calldata_set_string(&cd, "tag_id", "title");
			if (proc_handler_call(ph, "get_metadata", &cd)) {
				const char *title = calldata_string(&cd, "tag_data");
				if (title) {
					obs_data_set_string(response_data, "title", title);
				} else {
					obs_data_set_string(response_data, "error", "No title metadata found");
				}
			} else {
				obs_data_set_string(response_data, "error", "Failed to call proc handler");
			}
			calldata_free(&cd);
		} else {
			obs_data_set_string(response_data, "error", "No proc handler available");
		}
	} else {
		obs_data_set_string(response_data, "error", "Source is not a VLC source");
	}

	obs_source_release(source);
}

void WebsocketLoadStreamupFile(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	// Log the entire request for debugging
	const char *request_data_json = obs_data_get_json(request_data);
	blog(LOG_INFO, "Websocket request data: %s", request_data_json);

	// Extract the "file" parameter as a string (file path)
	const char *file_path = obs_data_get_string(request_data, "file");
	bool force_load = obs_data_get_bool(request_data, "force_load");

	if (!file_path || !strlen(file_path)) {
		// If the "file" path is missing, return an error response and log it
		blog(LOG_ERROR, "WebsocketLoadStreamupFile: 'file' parameter is missing or invalid");
		obs_data_set_string(response_data, "error", "'file' path is missing or invalid");
		return;
	}

	// Log the extracted file path for debugging
	blog(LOG_INFO, "Extracted 'file' path: %s", file_path);

	// Call the function to load the .streamup file from the path
	if (!StreamUP::FileManager::LoadStreamupFileFromPath(QString::fromUtf8(file_path), force_load)) {
		obs_data_set_string(response_data, "error", "Failed to load streamup file");
		return;
	}

	// Return success response
	obs_data_set_string(response_data, "status", "success");
}

//-------------------UI INTERACTION-------------------
void WebsocketOpenSourceProperties(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	const char *selected_source_name = StreamUP::SourceManager::GetSelectedSourceFromCurrentScene();
	if (!selected_source_name) {
		obs_data_set_string(response_data, "error", "No source selected.");
		blog(LOG_INFO, "[StreamUP] No source selected for properties.");
		return;
	}

	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		obs_frontend_open_source_properties(selected_source);
		obs_source_release(selected_source);
		obs_data_set_string(response_data, "status", "Properties opened.");
	} else {
		obs_data_set_string(response_data, "error", "Failed to find source.");
	}
}

void WebsocketOpenSourceFilters(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	const char *selected_source_name = StreamUP::SourceManager::GetSelectedSourceFromCurrentScene();
	if (!selected_source_name) {
		obs_data_set_string(response_data, "error", "No source selected.");
		blog(LOG_INFO, "[StreamUP] No source selected for filters.");
		return;
	}

	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		obs_frontend_open_source_filters(selected_source);
		obs_source_release(selected_source);
		obs_data_set_string(response_data, "status", "Filters opened.");
	} else {
		obs_data_set_string(response_data, "error", "Failed to find source.");
	}
}

void WebsocketOpenSourceInteract(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	const char *selected_source_name = StreamUP::SourceManager::GetSelectedSourceFromCurrentScene();
	if (!selected_source_name) {
		obs_data_set_string(response_data, "error", "No source selected.");
		blog(LOG_INFO, "[StreamUP] No source selected for interaction.");
		return;
	}

	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		obs_frontend_open_source_interaction(selected_source);
		obs_source_release(selected_source);
		obs_data_set_string(response_data, "status", "Interact window opened.");
	} else {
		obs_data_set_string(response_data, "error", "Failed to find source.");
	}
}

void WebsocketOpenSceneFilters(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		obs_data_set_string(response_data, "error", "No current scene.");
		blog(LOG_INFO, "[StreamUP] No current scene for filters.");
		return;
	}

	obs_frontend_open_source_filters(current_scene);
	obs_source_release(current_scene);
	obs_data_set_string(response_data, "status", "Scene filters opened.");
}

} // namespace WebSocketAPI
} // namespace StreamUP