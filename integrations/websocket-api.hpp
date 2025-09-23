#ifndef STREAMUP_WEBSOCKET_API_HPP
#define STREAMUP_WEBSOCKET_API_HPP

#include <obs-data.h>

namespace StreamUP {
namespace WebSocketAPI {

//-------------------UTILITY FUNCTIONS-------------------
/**
 * Get the current bitrate and related streaming information
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestBitrate(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Get plugin version information
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestVersion(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

//-------------------PLUGIN MANAGEMENT-------------------
/**
 * Check required plugins and return status
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestCheckPlugins(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

//-------------------SOURCE MANAGEMENT-------------------
/**
 * Lock/unlock all sources in all scenes
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestLockAllSources(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Lock/unlock sources in current scene
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestLockCurrentSources(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Refresh audio monitoring for all sources
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestRefreshAudioMonitoring(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Refresh browser sources
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestRefreshBrowserSources(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Get currently selected source name
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestGetCurrentSelectedSource(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

//-------------------TRANSITION MANAGEMENT-------------------
/**
 * Get show transition settings
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data
 */
void WebsocketRequestGetShowTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Get hide transition settings
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data
 */
void WebsocketRequestGetHideTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Set show transition settings
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data
 */
void WebsocketRequestSetShowTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Set hide transition settings
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data
 */
void WebsocketRequestSetHideTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

//-------------------OUTPUT AND FILE MANAGEMENT-------------------
/**
 * Get current recording output file path
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestGetOutputFilePath(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Get current VLC source file
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestVLCGetCurrentFile(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Load StreamUP file
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketLoadStreamupFile(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

//-------------------SOURCE PROPERTIES-------------------
/**
 * Get source blending method (scene item property)
 * @param request_data Request data from WebSocket (requires sourceName and optional sceneName)
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestGetBlendingMethod(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Set source blending method (scene item property)
 * @param request_data Request data from WebSocket (requires sourceName, method, and optional sceneName)
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestSetBlendingMethod(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Get source deinterlacing settings
 * @param request_data Request data from WebSocket (requires sourceName)
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestGetDeinterlacing(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Set source deinterlacing settings
 * @param request_data Request data from WebSocket (requires sourceName, mode, and optional fieldOrder)
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestSetDeinterlacing(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Get source scale filtering (scene item property)
 * @param request_data Request data from WebSocket (requires sourceName and optional sceneName)
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestGetScaleFiltering(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Set source scale filtering (scene item property)
 * @param request_data Request data from WebSocket (requires sourceName, filter, and optional sceneName)
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestSetScaleFiltering(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Get source downmix to mono setting
 * @param request_data Request data from WebSocket (requires sourceName)
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestGetDownmixMono(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Set source downmix to mono setting
 * @param request_data Request data from WebSocket (requires sourceName, enabled)
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRequestSetDownmixMono(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

//-------------------UI INTERACTION-------------------
/**
 * Open source properties dialog
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketOpenSourceProperties(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Open source filters dialog
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketOpenSourceFilters(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Open source interact dialog
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketOpenSourceInteract(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Open scene filters dialog
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketOpenSceneFilters(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

//-------------------VIDEO CAPTURE DEVICE MANAGEMENT-------------------
/**
 * Activate all video capture devices
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketActivateAllVideoCaptureDevices(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Deactivate all video capture devices
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketDeactivateAllVideoCaptureDevices(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

/**
 * Refresh all video capture devices
 * @param request_data Request data from WebSocket
 * @param response_data Response data to populate
 * @param private_data Private data (unused)
 */
void WebsocketRefreshAllVideoCaptureDevices(obs_data_t *request_data, obs_data_t *response_data, void *private_data);

} // namespace WebSocketAPI
} // namespace StreamUP

#endif // STREAMUP_WEBSOCKET_API_HPP
