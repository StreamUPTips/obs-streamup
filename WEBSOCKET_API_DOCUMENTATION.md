# StreamUP WebSocket API Documentation

This document provides comprehensive documentation for all WebSocket commands available in the StreamUP plugin for OBS Studio.

## Table of Contents

- [Overview](#overview)
- [Connection Information](#connection-information)
- [Command Format](#command-format)
- [Vendor Commands](#vendor-commands)
  - [Utility Commands](#utility-commands)
  - [Plugin Management](#plugin-management)
  - [Source Management](#source-management)
  - [Transition Management](#transition-management)
  - [File and Output Management](#file-and-output-management)
  - [Source Properties](#source-properties)
  - [UI Interaction](#ui-interaction)
  - [Video Capture Device Management](#video-capture-device-management)
  - [Transition Copy/Paste](#transition-copypaste)
- [Deprecated Commands](#deprecated-commands)

---

## Overview

The StreamUP plugin extends OBS Studio's WebSocket API with custom vendor commands under the `streamup` vendor namespace. These commands provide enhanced functionality for source management, transitions, and plugin-specific features. You can use the Request Parameters and Response Fields with the StreamUP OBS Plugin Commands in Streamer.Bot. 

## Connection Information

- **Vendor Name**: `streamup`
- **Protocol**: OBS WebSocket 5.x
- **Default Port**: 4455 (standard OBS WebSocket port)

## Command Format

All StreamUP vendor commands follow the OBS WebSocket 5.x vendor request format:

```json
{
  "requestType": "CallVendorRequest",
  "requestId": "unique-id",
  "requestData": {
    "vendorName": "streamup",
    "requestType": "CommandName",
    "requestData": {
      // Command-specific parameters
    }
  }
}
```

---

## Vendor Commands

### Utility Commands

#### GetStreamBitrate

Get the current streaming bitrate and related information.

**Request Parameters**: None

**Response Data**:
```json
{
  "kbits-per-sec": 5000
}
```

**Response Fields**:
- `kbits-per-sec` (integer): Current bitrate in kilobits per second
- `error` (string, optional): Error message if streaming is not active

**Notes**:
- Returns 0 on first call to initialize tracking
- Returns error if streaming is not active
- Calculates bitrate based on bytes sent since last request

---

### Source Management

#### ToggleLockAllSources

Toggle the lock state of all sources in all scenes.

**Request Parameters**: None

**Response Data**:
```json
{
  "lockState": true
}
```

**Response Fields**:
- `lockState` (boolean): The new lock state after toggling

**Notes**:
- Affects all sources across all scenes
- Returns the new state after toggling

---

#### ToggleLockCurrentSceneSources

Toggle the lock state of all sources in the current scene only.

**Request Parameters**: None

**Response Data**:
```json
{
  "lockState": true
}
```

**Response Fields**:
- `lockState` (boolean): The new lock state after toggling

**Notes**:
- Only affects sources in the currently active scene

---

#### RefreshAudioMonitoring

Refresh audio monitoring for all audio sources.

**Request Parameters**: None

**Response Data**:
```json
{
  "success": true
}
```

**Response Fields**:
- `success` (boolean): Always true

**Notes**:
- Executed on graphics thread for thread safety
- Enumerates all sources and refreshes their audio monitoring state

---

#### RefreshBrowserSources

Refresh all browser sources in the current OBS session.

**Request Parameters**: None

**Response Data**:
```json
{
  "success": true
}
```

**Response Fields**:
- `success` (boolean): Always true

**Notes**:
- Executed on graphics thread for thread safety
- Triggers a refresh on all browser source instances

---

#### GetSelectedSource

Get the name of the currently selected source in the current scene.

**Request Parameters**: None

**Response Data**:
```json
{
  "selectedSource": "Camera"
}
```

**Response Fields**:
- `selectedSource` (string): Name of the selected source, or "None" if no source is selected

---

### Transition Management

#### GetShowTransition

Get the show transition settings for a specific source in a scene.

**Request Parameters**:
```json
{
  "sceneName": "Scene",
  "sourceName": "Source"
}
```

**Request Fields**:
- `sceneName` (string, required): Name of the scene
- `sourceName` (string, required): Name of the source

**Response Data**:
```json
{
  "transitionType": "Fade",
  "transitionSettings": {
    // Transition-specific settings
  },
  "transitionDuration": 300,
  "success": true
}
```

**Response Fields**:
- `transitionType` (string): Display name of the transition (e.g., "Fade", "Cut")
- `transitionSettings` (object): Transition-specific settings
- `transitionDuration` (integer): Duration in milliseconds
- `success` (boolean): True if successful
- `error` (string, optional): Error message if failed

---

#### GetHideTransition

Get the hide transition settings for a specific source in a scene.

**Request Parameters**: Same as GetShowTransition

**Response Data**: Same as GetShowTransition

---

#### SetShowTransition

Set the show transition for a specific source in a scene.

**Request Parameters**:
```json
{
  "sceneName": "Scene",
  "sourceName": "Source",
  "transitionType": "Fade",
  "transitionSettings": {
    // Transition-specific settings
  },
  "transitionDuration": 300
}
```

**Request Fields**:
- `sceneName` (string, required): Name of the scene
- `sourceName` (string, required): Name of the source
- `transitionType` (string, required): Display name of the transition
- `transitionSettings` (object, optional): Transition-specific settings
- `transitionDuration` (integer, required): Duration in milliseconds

**Response Data**:
```json
{
  "success": true
}
```

**Response Fields**:
- `success` (boolean): True if successful
- `error` (string, optional): Error message if failed

**Valid Transition Types** (Display Names):
- "Cut"
- "Fade"
- "Swipe"
- "Slide"
- "Stinger"
- "Fade to Color"
- "Wipe"
- "Scene as Transition" (if available)
- "Move" (if Move Transition plugin is installed)
- "Shader" (if Shader Transition plugin is installed)

---

#### SetHideTransition

Set the hide transition for a specific source in a scene.

**Request Parameters**: Same as SetShowTransition

**Response Data**: Same as SetShowTransition

---

### File and Output Management

#### GetRecordingOutputPath

Get the current recording output file path.

**Request Parameters**: None

**Response Data**:
```json
{
  "outputFilePath": "C:/Videos/Recording.mp4"
}
```

**Response Fields**:
- `outputFilePath` (string): Path to the current or next recording file

**Notes**:
- DEPRECATED: Use official OBS WebSocket 5.0+ `GetRecordDirectory` instead
- Maintained for backward compatibility only

---

#### GetVLCCurrentFile

Get the currently playing file from a VLC source.

**Request Parameters**:
```json
{
  "sourceName": "VLC Source"
}
```

**Request Fields**:
- `sourceName` (string, required): Name of the VLC source

**Response Data**:
```json
{
  "title": "Current Video Title"
}
```

**Response Fields**:
- `title` (string): Title metadata of the currently playing file
- `error` (string, optional): Error message if failed

**Possible Errors**:
- "No source name provided"
- "Source not found"
- "Source is not a VLC source"
- "No proc handler available"
- "Failed to call proc handler"
- "No title metadata found"

---

### Source Properties

#### GetBlendingMethod

Get the blending method for a source in a scene.

**Request Parameters**:
```json
{
  "sourceName": "Source",
  "sceneName": "Scene"
}
```

**Request Fields**:
- `sourceName` (string, required): Name of the source
- `sceneName` (string, optional): Name of the scene (uses current scene if omitted)

**Response Data**:
```json
{
  "blendingMethod": "default",
  "success": true
}
```

**Response Fields**:
- `blendingMethod` (string): Either "default" or "srgb_off"
- `success` (boolean): True if successful
- `error` (string, optional): Error message if failed

---

#### SetBlendingMethod

Set the blending method for a source in a scene.

**Request Parameters**:
```json
{
  "sourceName": "Source",
  "sceneName": "Scene",
  "method": "default"
}
```

**Request Fields**:
- `sourceName` (string, required): Name of the source
- `sceneName` (string, optional): Name of the scene (uses current scene if omitted)
- `method` (string, required): Either "default" or "srgb_off"

**Response Data**:
```json
{
  "status": "success"
}
```

**Response Fields**:
- `status` (string): "success" if successful
- `error` (string, optional): Error message if failed

---

#### GetDeinterlacing

Get the deinterlacing settings for a source.

**Request Parameters**:
```json
{
  "sourceName": "Source"
}
```

**Request Fields**:
- `sourceName` (string, required): Name of the source

**Response Data**:
```json
{
  "mode": "disable",
  "fieldOrder": "top",
  "success": true
}
```

**Response Fields**:
- `mode` (string): Deinterlacing mode
- `fieldOrder` (string): Field order ("top" or "bottom")
- `success` (boolean): True if successful
- `error` (string, optional): Error message if failed

**Valid Modes**:
- "disable"
- "discard"
- "retro"
- "blend"
- "blend_2x"
- "linear"
- "linear_2x"
- "yadif"
- "yadif_2x"

---

#### SetDeinterlacing

Set the deinterlacing settings for a source.

**Request Parameters**:
```json
{
  "sourceName": "Source",
  "mode": "yadif",
  "fieldOrder": "top"
}
```

**Request Fields**:
- `sourceName` (string, required): Name of the source
- `mode` (string, required): Deinterlacing mode (see valid modes above)
- `fieldOrder` (string, optional): Field order ("top" or "bottom", defaults to "top")

**Response Data**:
```json
{
  "status": "success"
}
```

**Response Fields**:
- `status` (string): "success" if successful
- `error` (string, optional): Error message if failed

---

#### GetScaleFiltering

Get the scale filtering method for a source in a scene.

**Request Parameters**:
```json
{
  "sourceName": "Source",
  "sceneName": "Scene"
}
```

**Request Fields**:
- `sourceName` (string, required): Name of the source
- `sceneName` (string, optional): Name of the scene (uses current scene if omitted)

**Response Data**:
```json
{
  "scaleFilter": "bilinear",
  "success": true
}
```

**Response Fields**:
- `scaleFilter` (string): The current scale filtering method
- `success` (boolean): True if successful
- `error` (string, optional): Error message if failed

**Valid Scale Filters**:
- "disable"
- "point"
- "bicubic"
- "bilinear"
- "lanczos"
- "area"

---

#### SetScaleFiltering

Set the scale filtering method for a source in a scene.

**Request Parameters**:
```json
{
  "sourceName": "Source",
  "sceneName": "Scene",
  "filter": "lanczos"
}
```

**Request Fields**:
- `sourceName` (string, required): Name of the source
- `sceneName` (string, optional): Name of the scene (uses current scene if omitted)
- `filter` (string, required): Scale filtering method (see valid filters above)

**Response Data**:
```json
{
  "status": "success"
}
```

**Response Fields**:
- `status` (string): "success" if successful
- `error` (string, optional): Error message if failed

---

#### GetDownmixMono

Get the downmix to mono setting for a source.

**Request Parameters**:
```json
{
  "sourceName": "Source"
}
```

**Request Fields**:
- `sourceName` (string, required): Name of the source

**Response Data**:
```json
{
  "downmixMono": false,
  "success": true
}
```

**Response Fields**:
- `downmixMono` (boolean): True if downmix to mono is enabled
- `success` (boolean): True if successful
- `error` (string, optional): Error message if failed

---

#### SetDownmixMono

Set the downmix to mono setting for a source.

**Request Parameters**:
```json
{
  "sourceName": "Source",
  "enabled": true
}
```

**Request Fields**:
- `sourceName` (string, required): Name of the source
- `enabled` (boolean, required): Enable or disable downmix to mono

**Response Data**:
```json
{
  "status": "success"
}
```

**Response Fields**:
- `status` (string): "success" if successful
- `error` (string, optional): Error message if failed

---

### UI Interaction

#### OpenSourceProperties

Open the properties dialog for the currently selected source.

**Request Parameters**: None

**Response Data**:
```json
{
  "status": "Properties opened."
}
```

**Response Fields**:
- `status` (string): Success message
- `error` (string, optional): Error message if no source is selected

**Notes**:
- Requires a source to be selected in the current scene
- Opens the standard OBS source properties dialog

---

#### OpenSourceFilters

Open the filters dialog for the currently selected source.

**Request Parameters**: None

**Response Data**:
```json
{
  "status": "Filters opened."
}
```

**Response Fields**:
- `status` (string): Success message
- `error` (string, optional): Error message if no source is selected

**Notes**:
- Requires a source to be selected in the current scene
- Opens the standard OBS source filters dialog

---

#### OpenSourceInteraction

Open the interaction window for the currently selected source.

**Request Parameters**: None

**Response Data**:
```json
{
  "status": "Interact window opened."
}
```

**Response Fields**:
- `status` (string): Success message
- `error` (string, optional): Error message if no source is selected

**Notes**:
- Requires a source to be selected in the current scene
- Only works for sources that support interaction (e.g., Browser sources)

---

#### OpenSceneFilters

Open the filters dialog for the current scene.

**Request Parameters**: None

**Response Data**:
```json
{
  "status": "Scene filters opened."
}
```

**Response Fields**:
- `status` (string): Success message
- `error` (string, optional): Error message if no current scene

---

### Video Capture Device Management

#### ActivateAllVideoCaptureDevices

Activate all video capture devices in the current OBS session.

**Request Parameters**: None

**Response Data**:
```json
{
  "status": "All video capture devices activated successfully",
  "success": true
}
```

**Response Fields**:
- `status` (string): Success message
- `success` (boolean): True if successful
- `error` (string, optional): Error message if failed

---

#### DeactivateAllVideoCaptureDevices

Deactivate all video capture devices in the current OBS session.

**Request Parameters**: None

**Response Data**:
```json
{
  "status": "All video capture devices deactivated successfully",
  "success": true
}
```

**Response Fields**:
- `status` (string): Success message
- `success` (boolean): True if successful
- `error` (string, optional): Error message if failed

**Notes**:
- Useful for temporarily disabling all cameras without removing them from scenes

---

#### RefreshAllVideoCaptureDevices

Refresh all video capture devices in the current OBS session.

**Request Parameters**: None

**Response Data**:
```json
{
  "status": "All video capture devices refresh initiated successfully",
  "success": true
}
```

**Response Fields**:
- `status` (string): Success message
- `success` (boolean): True if successful
- `error` (string, optional): Error message if failed

**Notes**:
- Forces all video capture devices to reinitialize
- Useful when a camera has been disconnected and reconnected

---

### Transition Copy/Paste

#### CopyShowTransition

Copy the show transition from the currently selected source.

**Request Parameters**: None

**Response Data**:
```json
{
  "success": true
}
```

**Response Fields**:
- `success` (boolean): Always true

**Notes**:
- Requires a source to be selected in the current scene
- Stores transition settings in plugin clipboard for pasting

---

#### CopyHideTransition

Copy the hide transition from the currently selected source.

**Request Parameters**: None

**Response Data**:
```json
{
  "success": true
}
```

**Response Fields**:
- `success` (boolean): Always true

**Notes**:
- Requires a source to be selected in the current scene
- Stores transition settings in plugin clipboard for pasting

---

#### PasteShowTransition

Paste the previously copied show transition to the currently selected source.

**Request Parameters**: None

**Response Data**:
```json
{
  "success": true
}
```

**Response Fields**:
- `success` (boolean): Always true

**Notes**:
- Requires a source to be selected in the current scene
- Requires a show transition to have been previously copied

---

#### PasteHideTransition

Paste the previously copied hide transition to the currently selected source.

**Request Parameters**: None

**Response Data**:
```json
{
  "success": true
}
```

**Response Fields**:
- `success` (boolean): Always true

**Notes**:
- Requires a source to be selected in the current scene
- Requires a hide transition to have been previously copied

---

## Deprecated Commands

The following command names are deprecated but still supported for backward compatibility. Use the PascalCase versions instead.

| Deprecated Name | Current Name |
|----------------|--------------|
| `getBitrate` | `GetStreamBitrate` |
| `version` | `GetPluginVersion` |
| `check_plugins` | `CheckRequiredPlugins` |
| `toggleLockAllSources` | `ToggleLockAllSources` |
| `toggleLockCurrentSources` | `ToggleLockCurrentSceneSources` |
| `refresh_audio_monitoring` | `RefreshAudioMonitoring` |
| `refresh_browser_sources` | `RefreshBrowserSources` |
| `getCurrentSource` | `GetSelectedSource` |
| `getShowTransition` | `GetShowTransition` |
| `getHideTransition` | `GetHideTransition` |
| `setShowTransition` | `SetShowTransition` |
| `setHideTransition` | `SetHideTransition` |
| `getOutputFilePath` | `GetRecordingOutputPath` |
| `vlcGetCurrentFile` | `GetVLCCurrentFile` |
| `loadStreamupFile` | `LoadStreamUpFile` |
| `openSourceProperties` | `OpenSourceProperties` |
| `openSourceFilters` | `OpenSourceFilters` |
| `openSourceInteract` | `OpenSourceInteraction` |
| `openSceneFilters` | `OpenSceneFilters` |

---

## Error Handling

All commands follow consistent error handling patterns:

**Success Response**:
- Contains relevant data fields
- May include `success: true` boolean

**Error Response**:
- Contains `error` field with descriptive message
- May include `success: false` boolean

**Common Errors**:
- "Scene not found"
- "Source not found"
- "Source not found in scene"
- "No source selected"
- "Invalid parameters"
- Missing required fields

---

## Notes

1. All commands are case-sensitive
2. Scene item IDs are unique per scene
3. Source names are case-sensitive and must match exactly
4. Transition display names are localized based on OBS language settings
5. Commands that operate on "selected source" require a source to be selected in OBS
6. File paths should use forward slashes (/) or escaped backslashes (\\\\)
7. Thread-safe operations are executed on appropriate OBS threads automatically

---

## Version Information

- **Document Version**: 1.0
- **Plugin Version**: 2.1.3+
- **OBS WebSocket Version**: 5.x
- **Last Updated**: 2025

---

## Support and Resources

- **GitHub Repository**: [StreamUP Plugin Repository]
- **Issue Tracker**: [GitHub Issues]
- **OBS WebSocket Protocol**: https://github.com/obsproject/obs-websocket/blob/master/docs/generated/protocol.md

---

*This documentation is maintained by the StreamUP development team. For questions or issues, please file a GitHub issue.*
