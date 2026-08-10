#pragma once

#include <obs.h>
#include <obs-frontend-api.h>
#include <string>

namespace StreamUP {
namespace UndoHelpers {

// ---- Transform Undo ----

// Capture current transform state for all items in a scene.
// Returns JSON string to be used as undo_data.
// Call this BEFORE modifying transforms.
inline std::string CaptureTransformState(obs_scene_t *scene)
{
	if (!scene)
		return {};
	obs_data_t *data = obs_scene_save_transform_states(scene, true);
	if (!data)
		return {};
	const char *json = obs_data_get_json(data);
	std::string result = json ? json : "";
	obs_data_release(data);
	return result;
}

// Push a transform undo/redo action.
// undoData = state captured BEFORE the change (from CaptureTransformState).
// Call this AFTER modifying transforms (captures current state as redo automatically).
inline void PushTransformUndo(const char *actionName, obs_scene_t *scene,
			      const std::string &undoData)
{
	if (!scene || undoData.empty())
		return;

	// Capture current (post-change) state as redo data
	std::string redoData = CaptureTransformState(scene);
	if (redoData.empty())
		return;

	// Don't push if nothing actually changed
	if (undoData == redoData)
		return;

	obs_frontend_add_undo_redo_action(
		actionName,
		[](const char *data) {
			obs_scene_load_transform_states(data);
		},
		[](const char *data) {
			obs_scene_load_transform_states(data);
		},
		undoData.c_str(), redoData.c_str(), false);
}

// Convenience: capture + push for transform operations on a scene item.
// Call BEFORE modifying, store the returned token, then call CommitTransformUndo.
inline std::string BeginTransformUndo(obs_sceneitem_t *sceneItem)
{
	if (!sceneItem)
		return {};
	obs_scene_t *scene = obs_sceneitem_get_scene(sceneItem);
	return CaptureTransformState(scene);
}

inline void CommitTransformUndo(const char *actionName,
				obs_sceneitem_t *sceneItem,
				const std::string &undoData)
{
	if (!sceneItem || undoData.empty())
		return;
	obs_scene_t *scene = obs_sceneitem_get_scene(sceneItem);
	PushTransformUndo(actionName, scene, undoData);
}

// ---- Audio Undo ----

// Capture audio state as JSON. Includes volume, mute, balance, mono,
// sync offset, monitoring type, and mixer tracks.
inline std::string CaptureAudioState(obs_source_t *source)
{
	if (!source)
		return {};

	obs_data_t *data = obs_data_create();

	// Store UUID for lookup during undo
	const char *uuid = obs_source_get_uuid(source);
	obs_data_set_string(data, "uuid", uuid ? uuid : "");

	obs_data_set_double(data, "volume", obs_source_get_volume(source));
	obs_data_set_bool(data, "muted", obs_source_muted(source));
	obs_data_set_double(data, "balance",
			    obs_source_get_balance_value(source));

	uint32_t flags = obs_source_get_flags(source);
	obs_data_set_bool(data, "mono",
			  (flags & OBS_SOURCE_FLAG_FORCE_MONO) != 0);

	obs_data_set_int(data, "sync_offset",
			 obs_source_get_sync_offset(source));
	obs_data_set_int(data, "monitoring_type",
			 (int)obs_source_get_monitoring_type(source));
	obs_data_set_int(data, "mixers", obs_source_get_audio_mixers(source));

	const char *json = obs_data_get_json(data);
	std::string result = json ? json : "";
	obs_data_release(data);
	return result;
}

// Restore audio state from JSON
inline void RestoreAudioState(const char *jsonData)
{
	if (!jsonData || !*jsonData)
		return;

	obs_data_t *data = obs_data_create_from_json(jsonData);
	if (!data)
		return;

	const char *uuid = obs_data_get_string(data, "uuid");
	obs_source_t *source = obs_get_source_by_uuid(uuid);
	if (!source) {
		obs_data_release(data);
		return;
	}

	obs_source_set_volume(source,
			      (float)obs_data_get_double(data, "volume"));
	obs_source_set_muted(source, obs_data_get_bool(data, "muted"));
	obs_source_set_balance_value(
		source, (float)obs_data_get_double(data, "balance"));

	uint32_t flags = obs_source_get_flags(source);
	if (obs_data_get_bool(data, "mono"))
		flags |= OBS_SOURCE_FLAG_FORCE_MONO;
	else
		flags &= ~OBS_SOURCE_FLAG_FORCE_MONO;
	obs_source_set_flags(source, flags);

	obs_source_set_sync_offset(source,
				   obs_data_get_int(data, "sync_offset"));
	obs_source_set_monitoring_type(
		source,
		(obs_monitoring_type)obs_data_get_int(data, "monitoring_type"));
	obs_source_set_audio_mixers(
		source, (uint32_t)obs_data_get_int(data, "mixers"));

	obs_source_release(source);
	obs_data_release(data);
}

// Push an audio undo/redo action.
// undoData = state captured BEFORE the change (from CaptureAudioState).
// repeatable = true for slider-driven changes (volume, balance) to merge
// consecutive adjustments within OBS's 3-second merge window.
inline void PushAudioUndo(const char *actionName, obs_source_t *source,
			  const std::string &undoData, bool repeatable = false)
{
	if (!source || undoData.empty())
		return;

	std::string redoData = CaptureAudioState(source);
	if (redoData.empty())
		return;

	if (undoData == redoData)
		return;

	obs_frontend_add_undo_redo_action(
		actionName,
		[](const char *data) { RestoreAudioState(data); },
		[](const char *data) { RestoreAudioState(data); },
		undoData.c_str(), redoData.c_str(), repeatable);
}

} // namespace UndoHelpers
} // namespace StreamUP
