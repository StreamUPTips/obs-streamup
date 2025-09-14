#include "websocket-api.hpp"
#include "../utilities/debug-logger.hpp"
#include "streamup-common.hpp"
#include "../version.h"
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

	StreamUP::DebugLogger::LogDebug("WebSocket", "GetBitrate", "WebSocket request received for stream bitrate");

	obs_output_t *streamOutput = obs_frontend_get_streaming_output();
	if (!streamOutput || !obs_frontend_streaming_active()) {
		StreamUP::DebugLogger::LogDebug("WebSocket", "GetBitrate", "Streaming is not active, returning error");
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
	
	bool pluginsUpToDate;
	
	// Check if OBS is currently recording or streaming
	bool isRecording = obs_frontend_recording_active();
	bool isStreaming = obs_frontend_streaming_active();
	
	if (isRecording || isStreaming) {
		// Don't show UI when recording/streaming, just check silently
		pluginsUpToDate = StreamUP::PluginManager::CheckrequiredOBSPluginsWithoutUI(true);
		StreamUP::DebugLogger::LogDebugFormat("WebSocket", "Plugin Check", "Plugin check via WebSocket completed without UI (recording: %s, streaming: %s)", 
			isRecording ? "active" : "inactive", isStreaming ? "active" : "inactive");
	} else {
		// Show UI normally when not recording/streaming
		pluginsUpToDate = StreamUP::PluginManager::CheckrequiredOBSPlugins(true);
	}
	
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
	
	// Queue source enumeration on graphics thread for thread safety
	obs_queue_task(OBS_TASK_GRAPHICS, [](void *param) {
		UNUSED_PARAMETER(param);
		obs_enum_sources(StreamUP::SourceManager::RefreshAudioMonitoring, nullptr);
	}, nullptr, false);
	
	obs_data_set_bool(response_data, "success", true);
}

void WebsocketRequestRefreshBrowserSources(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);
	
	// Queue source enumeration on graphics thread for thread safety
	obs_queue_task(OBS_TASK_GRAPHICS, [](void *param) {
		UNUSED_PARAMETER(param);
		obs_enum_sources(StreamUP::SourceManager::RefreshBrowserSources, nullptr);
	}, nullptr, false);
	
	obs_data_set_bool(response_data, "success", true);
}

void WebsocketRequestGetCurrentSelectedSource(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	const char *selected_source_name = StreamUP::SourceManager::GetSelectedSourceFromCurrentScene();
	if (selected_source_name) {
		obs_data_set_string(response_data, "selectedSource", selected_source_name);
	} else {
		StreamUP::DebugLogger::LogDebug("WebSocket", "Source Selection", "No selected source");
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

	// NOTE: This custom command is DEPRECATED as of OBS WebSocket 5.0+
	// The official OBS WebSocket API now provides "GetRecordDirectory" which serves the same purpose.
	// This command is maintained for backward compatibility only.
	// New implementations should use the official "GetRecordDirectory" command instead.
	//
	// Differences:
	// - Our custom command: Returns "outputFilePath" field
	// - Official command: Returns recording directory path
	// - Functionality: Both retrieve the current recording output directory

	char *path = obs_frontend_get_current_record_output_path();
	obs_data_set_string(response_data, "outputFilePath", path);
	bfree(path);
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
	StreamUP::DebugLogger::LogDebugFormat("WebSocket", "Request Data", "Websocket request data: %s", request_data_json);

	// Extract the "file" parameter as a string (file path)
	const char *file_path = obs_data_get_string(request_data, "file");
	bool force_load = obs_data_get_bool(request_data, "force_load");

	if (!file_path || !strlen(file_path)) {
		// If the "file" path is missing, return an error response and log it
		StreamUP::DebugLogger::LogError("WebSocket", "WebsocketLoadStreamupFile: 'file' parameter is missing or invalid");
		obs_data_set_string(response_data, "error", "'file' path is missing or invalid");
		return;
	}

	// Log the extracted file path for debugging
	StreamUP::DebugLogger::LogDebugFormat("WebSocket", "File Path", "Extracted 'file' path: %s", file_path);

	// Call the function to load the .streamup file from the path
	if (!StreamUP::FileManager::LoadStreamupFileFromPath(QString::fromUtf8(file_path), force_load)) {
		obs_data_set_string(response_data, "error", "Failed to load streamup file");
		return;
	}

	// Return success response
	obs_data_set_string(response_data, "status", "success");
}

//-------------------SOURCE PROPERTIES-------------------
void WebsocketRequestGetBlendingMethod(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	const char *source_name = obs_data_get_string(request_data, "sourceName");
	const char *scene_name = obs_data_get_string(request_data, "sceneName");

	if (!source_name || !strlen(source_name)) {
		obs_data_set_string(response_data, "error", "sourceName parameter is required");
		return;
	}

	obs_source_t *scene_source = nullptr;
	if (scene_name && strlen(scene_name)) {
		scene_source = obs_get_source_by_name(scene_name);
	} else {
		scene_source = obs_frontend_get_current_scene();
	}

	if (!scene_source) {
		obs_data_set_string(response_data, "error", "Scene not found");
		return;
	}

	obs_scene_t *scene = obs_scene_from_source(scene_source);
	if (!scene) {
		obs_source_release(scene_source);
		obs_data_set_string(response_data, "error", "Invalid scene");
		return;
	}

	obs_sceneitem_t *sceneitem = obs_scene_find_source(scene, source_name);
	if (!sceneitem) {
		obs_source_release(scene_source);
		obs_data_set_string(response_data, "error", "Source not found in scene");
		return;
	}

	enum obs_blending_method method = obs_sceneitem_get_blending_method(sceneitem);
	const char *method_name = (method == OBS_BLEND_METHOD_SRGB_OFF) ? "srgb_off" : "default";
	
	obs_data_set_string(response_data, "blendingMethod", method_name);
	obs_data_set_bool(response_data, "success", true);
	
	obs_source_release(scene_source);
}

void WebsocketRequestSetBlendingMethod(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	const char *source_name = obs_data_get_string(request_data, "sourceName");
	const char *scene_name = obs_data_get_string(request_data, "sceneName");
	const char *method_str = obs_data_get_string(request_data, "method");

	if (!source_name || !strlen(source_name)) {
		obs_data_set_string(response_data, "error", "sourceName parameter is required");
		return;
	}

	if (!method_str || !strlen(method_str)) {
		obs_data_set_string(response_data, "error", "method parameter is required");
		return;
	}

	enum obs_blending_method method;
	if (strcmp(method_str, "srgb_off") == 0) {
		method = OBS_BLEND_METHOD_SRGB_OFF;
	} else if (strcmp(method_str, "default") == 0) {
		method = OBS_BLEND_METHOD_DEFAULT;
	} else {
		obs_data_set_string(response_data, "error", "Invalid method. Valid values: 'default', 'srgb_off'");
		return;
	}

	obs_source_t *scene_source = nullptr;
	if (scene_name && strlen(scene_name)) {
		scene_source = obs_get_source_by_name(scene_name);
	} else {
		scene_source = obs_frontend_get_current_scene();
	}

	if (!scene_source) {
		obs_data_set_string(response_data, "error", "Scene not found");
		return;
	}

	obs_scene_t *scene = obs_scene_from_source(scene_source);
	if (!scene) {
		obs_source_release(scene_source);
		obs_data_set_string(response_data, "error", "Invalid scene");
		return;
	}

	obs_sceneitem_t *sceneitem = obs_scene_find_source(scene, source_name);
	if (!sceneitem) {
		obs_source_release(scene_source);
		obs_data_set_string(response_data, "error", "Source not found in scene");
		return;
	}

	obs_sceneitem_set_blending_method(sceneitem, method);
	obs_data_set_string(response_data, "status", "success");
	
	obs_source_release(scene_source);
}

void WebsocketRequestGetDeinterlacing(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	const char *source_name = obs_data_get_string(request_data, "sourceName");

	if (!source_name || !strlen(source_name)) {
		obs_data_set_string(response_data, "error", "sourceName parameter is required");
		return;
	}

	obs_source_t *source = obs_get_source_by_name(source_name);
	if (!source) {
		obs_data_set_string(response_data, "error", "Source not found");
		return;
	}

	enum obs_deinterlace_mode mode = obs_source_get_deinterlace_mode(source);
	enum obs_deinterlace_field_order field_order = obs_source_get_deinterlace_field_order(source);

	const char *mode_name;
	switch (mode) {
		case OBS_DEINTERLACE_MODE_DISABLE: mode_name = "disable"; break;
		case OBS_DEINTERLACE_MODE_DISCARD: mode_name = "discard"; break;
		case OBS_DEINTERLACE_MODE_RETRO: mode_name = "retro"; break;
		case OBS_DEINTERLACE_MODE_BLEND: mode_name = "blend"; break;
		case OBS_DEINTERLACE_MODE_BLEND_2X: mode_name = "blend_2x"; break;
		case OBS_DEINTERLACE_MODE_LINEAR: mode_name = "linear"; break;
		case OBS_DEINTERLACE_MODE_LINEAR_2X: mode_name = "linear_2x"; break;
		case OBS_DEINTERLACE_MODE_YADIF: mode_name = "yadif"; break;
		case OBS_DEINTERLACE_MODE_YADIF_2X: mode_name = "yadif_2x"; break;
		default: mode_name = "unknown"; break;
	}

	const char *field_order_name = (field_order == OBS_DEINTERLACE_FIELD_ORDER_TOP) ? "top" : "bottom";

	obs_data_set_string(response_data, "mode", mode_name);
	obs_data_set_string(response_data, "fieldOrder", field_order_name);
	obs_data_set_bool(response_data, "success", true);
	
	obs_source_release(source);
}

void WebsocketRequestSetDeinterlacing(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	const char *source_name = obs_data_get_string(request_data, "sourceName");
	const char *mode_str = obs_data_get_string(request_data, "mode");
	const char *field_order_str = obs_data_get_string(request_data, "fieldOrder");

	if (!source_name || !strlen(source_name)) {
		obs_data_set_string(response_data, "error", "sourceName parameter is required");
		return;
	}

	if (!mode_str || !strlen(mode_str)) {
		obs_data_set_string(response_data, "error", "mode parameter is required");
		return;
	}

	enum obs_deinterlace_mode mode;
	if (strcmp(mode_str, "disable") == 0) {
		mode = OBS_DEINTERLACE_MODE_DISABLE;
	} else if (strcmp(mode_str, "discard") == 0) {
		mode = OBS_DEINTERLACE_MODE_DISCARD;
	} else if (strcmp(mode_str, "retro") == 0) {
		mode = OBS_DEINTERLACE_MODE_RETRO;
	} else if (strcmp(mode_str, "blend") == 0) {
		mode = OBS_DEINTERLACE_MODE_BLEND;
	} else if (strcmp(mode_str, "blend_2x") == 0) {
		mode = OBS_DEINTERLACE_MODE_BLEND_2X;
	} else if (strcmp(mode_str, "linear") == 0) {
		mode = OBS_DEINTERLACE_MODE_LINEAR;
	} else if (strcmp(mode_str, "linear_2x") == 0) {
		mode = OBS_DEINTERLACE_MODE_LINEAR_2X;
	} else if (strcmp(mode_str, "yadif") == 0) {
		mode = OBS_DEINTERLACE_MODE_YADIF;
	} else if (strcmp(mode_str, "yadif_2x") == 0) {
		mode = OBS_DEINTERLACE_MODE_YADIF_2X;
	} else {
		obs_data_set_string(response_data, "error", "Invalid mode. Valid values: disable, discard, retro, blend, blend_2x, linear, linear_2x, yadif, yadif_2x");
		return;
	}

	enum obs_deinterlace_field_order field_order = OBS_DEINTERLACE_FIELD_ORDER_TOP;
	if (field_order_str && strlen(field_order_str)) {
		if (strcmp(field_order_str, "bottom") == 0) {
			field_order = OBS_DEINTERLACE_FIELD_ORDER_BOTTOM;
		} else if (strcmp(field_order_str, "top") != 0) {
			obs_data_set_string(response_data, "error", "Invalid fieldOrder. Valid values: 'top', 'bottom'");
			return;
		}
	}

	obs_source_t *source = obs_get_source_by_name(source_name);
	if (!source) {
		obs_data_set_string(response_data, "error", "Source not found");
		return;
	}

	obs_source_set_deinterlace_mode(source, mode);
	obs_source_set_deinterlace_field_order(source, field_order);
	obs_data_set_string(response_data, "status", "success");
	
	obs_source_release(source);
}

void WebsocketRequestGetScaleFiltering(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	const char *source_name = obs_data_get_string(request_data, "sourceName");
	const char *scene_name = obs_data_get_string(request_data, "sceneName");

	if (!source_name || !strlen(source_name)) {
		obs_data_set_string(response_data, "error", "sourceName parameter is required");
		return;
	}

	obs_source_t *scene_source = nullptr;
	if (scene_name && strlen(scene_name)) {
		scene_source = obs_get_source_by_name(scene_name);
	} else {
		scene_source = obs_frontend_get_current_scene();
	}

	if (!scene_source) {
		obs_data_set_string(response_data, "error", "Scene not found");
		return;
	}

	obs_scene_t *scene = obs_scene_from_source(scene_source);
	if (!scene) {
		obs_source_release(scene_source);
		obs_data_set_string(response_data, "error", "Invalid scene");
		return;
	}

	obs_sceneitem_t *sceneitem = obs_scene_find_source(scene, source_name);
	if (!sceneitem) {
		obs_source_release(scene_source);
		obs_data_set_string(response_data, "error", "Source not found in scene");
		return;
	}

	enum obs_scale_type filter = obs_sceneitem_get_scale_filter(sceneitem);
	
	const char *filter_name;
	switch (filter) {
		case OBS_SCALE_DISABLE: filter_name = "disable"; break;
		case OBS_SCALE_POINT: filter_name = "point"; break;
		case OBS_SCALE_BICUBIC: filter_name = "bicubic"; break;
		case OBS_SCALE_BILINEAR: filter_name = "bilinear"; break;
		case OBS_SCALE_LANCZOS: filter_name = "lanczos"; break;
		case OBS_SCALE_AREA: filter_name = "area"; break;
		default: filter_name = "unknown"; break;
	}
	
	obs_data_set_string(response_data, "scaleFilter", filter_name);
	obs_data_set_bool(response_data, "success", true);
	
	obs_source_release(scene_source);
}

void WebsocketRequestSetScaleFiltering(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	const char *source_name = obs_data_get_string(request_data, "sourceName");
	const char *scene_name = obs_data_get_string(request_data, "sceneName");
	const char *filter_str = obs_data_get_string(request_data, "filter");

	if (!source_name || !strlen(source_name)) {
		obs_data_set_string(response_data, "error", "sourceName parameter is required");
		return;
	}

	if (!filter_str || !strlen(filter_str)) {
		obs_data_set_string(response_data, "error", "filter parameter is required");
		return;
	}

	enum obs_scale_type filter;
	if (strcmp(filter_str, "disable") == 0) {
		filter = OBS_SCALE_DISABLE;
	} else if (strcmp(filter_str, "point") == 0) {
		filter = OBS_SCALE_POINT;
	} else if (strcmp(filter_str, "bicubic") == 0) {
		filter = OBS_SCALE_BICUBIC;
	} else if (strcmp(filter_str, "bilinear") == 0) {
		filter = OBS_SCALE_BILINEAR;
	} else if (strcmp(filter_str, "lanczos") == 0) {
		filter = OBS_SCALE_LANCZOS;
	} else if (strcmp(filter_str, "area") == 0) {
		filter = OBS_SCALE_AREA;
	} else {
		obs_data_set_string(response_data, "error", "Invalid filter. Valid values: disable, point, bicubic, bilinear, lanczos, area");
		return;
	}

	obs_source_t *scene_source = nullptr;
	if (scene_name && strlen(scene_name)) {
		scene_source = obs_get_source_by_name(scene_name);
	} else {
		scene_source = obs_frontend_get_current_scene();
	}

	if (!scene_source) {
		obs_data_set_string(response_data, "error", "Scene not found");
		return;
	}

	obs_scene_t *scene = obs_scene_from_source(scene_source);
	if (!scene) {
		obs_source_release(scene_source);
		obs_data_set_string(response_data, "error", "Invalid scene");
		return;
	}

	obs_sceneitem_t *sceneitem = obs_scene_find_source(scene, source_name);
	if (!sceneitem) {
		obs_source_release(scene_source);
		obs_data_set_string(response_data, "error", "Source not found in scene");
		return;
	}

	obs_sceneitem_set_scale_filter(sceneitem, filter);
	obs_data_set_string(response_data, "status", "success");
	
	obs_source_release(scene_source);
}

void WebsocketRequestGetDownmixMono(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	const char *source_name = obs_data_get_string(request_data, "sourceName");

	if (!source_name || !strlen(source_name)) {
		obs_data_set_string(response_data, "error", "sourceName parameter is required");
		return;
	}

	obs_source_t *source = obs_get_source_by_name(source_name);
	if (!source) {
		obs_data_set_string(response_data, "error", "Source not found");
		return;
	}

	uint32_t flags = obs_source_get_flags(source);
	bool is_mono = (flags & OBS_SOURCE_FLAG_FORCE_MONO) != 0;
	
	obs_data_set_bool(response_data, "downmixMono", is_mono);
	obs_data_set_bool(response_data, "success", true);
	
	obs_source_release(source);
}

void WebsocketRequestSetDownmixMono(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	const char *source_name = obs_data_get_string(request_data, "sourceName");
	bool enabled = obs_data_get_bool(request_data, "enabled");

	if (!source_name || !strlen(source_name)) {
		obs_data_set_string(response_data, "error", "sourceName parameter is required");
		return;
	}

	obs_source_t *source = obs_get_source_by_name(source_name);
	if (!source) {
		obs_data_set_string(response_data, "error", "Source not found");
		return;
	}

	uint32_t flags = obs_source_get_flags(source);
	
	if (enabled) {
		flags |= OBS_SOURCE_FLAG_FORCE_MONO;
	} else {
		flags &= ~OBS_SOURCE_FLAG_FORCE_MONO;
	}
	
	obs_source_set_flags(source, flags);
	obs_data_set_string(response_data, "status", "success");
	
	obs_source_release(source);
}

//-------------------UI INTERACTION-------------------
void WebsocketOpenSourceProperties(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	const char *selected_source_name = StreamUP::SourceManager::GetSelectedSourceFromCurrentScene();
	if (!selected_source_name) {
		obs_data_set_string(response_data, "error", "No source selected.");
		StreamUP::DebugLogger::LogDebug("WebSocket", "Source Properties", "No source selected for properties");
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
		StreamUP::DebugLogger::LogDebug("WebSocket", "Source Filters", "No source selected for filters");
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
		StreamUP::DebugLogger::LogDebug("WebSocket", "Source Interaction", "No source selected for interaction");
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
		StreamUP::DebugLogger::LogDebug("WebSocket", "Scene Filters", "No current scene for filters");
		return;
	}

	obs_frontend_open_source_filters(current_scene);
	obs_source_release(current_scene);
	obs_data_set_string(response_data, "status", "Scene filters opened.");
}

//-------------------VIDEO CAPTURE DEVICE MANAGEMENT-------------------
void WebsocketActivateAllVideoCaptureDevices(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	bool success = StreamUP::SourceManager::ActivateAllVideoCaptureDevices(false);
	if (success) {
		obs_data_set_string(response_data, "status", "All video capture devices activated successfully");
		obs_data_set_bool(response_data, "success", true);
	} else {
		obs_data_set_string(response_data, "error", "Failed to activate video capture devices");
		obs_data_set_bool(response_data, "success", false);
	}
}

void WebsocketDeactivateAllVideoCaptureDevices(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	bool success = StreamUP::SourceManager::DeactivateAllVideoCaptureDevices(false);
	if (success) {
		obs_data_set_string(response_data, "status", "All video capture devices deactivated successfully");
		obs_data_set_bool(response_data, "success", true);
	} else {
		obs_data_set_string(response_data, "error", "Failed to deactivate video capture devices");
		obs_data_set_bool(response_data, "success", false);
	}
}

void WebsocketRefreshAllVideoCaptureDevices(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	bool success = StreamUP::SourceManager::RefreshAllVideoCaptureDevices(false);
	if (success) {
		obs_data_set_string(response_data, "status", "All video capture devices refresh initiated successfully");
		obs_data_set_bool(response_data, "success", true);
	} else {
		obs_data_set_string(response_data, "error", "Failed to refresh video capture devices");
		obs_data_set_bool(response_data, "success", false);
	}
}

} // namespace WebSocketAPI
} // namespace StreamUP