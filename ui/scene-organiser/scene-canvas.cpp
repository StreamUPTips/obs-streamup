#include "scene-canvas.hpp"

#include <obs-frontend-api.h>
#include <util/platform.h>

#include <cstring>

namespace StreamUP {
namespace SceneOrganiser {
namespace Canvas {

const char *const kVerticalCanvasName = "Aitum Vertical";

namespace {
// Aitum's OBS module name (its file name without the extension). Used to tell a
// live vertical canvas apart from one OBS restored out of the scene collection
// after the plugin was removed.
const char *const kVerticalModuleName = "vertical-canvas";

// Find Aitum's canvas among the frontend canvases. NEW reference or nullptr.
// A canvas that has been removed but not yet freed still shows up in the list,
// so skip those or the dock would keep talking to a dead canvas.
obs_canvas_t *AcquireVertical()
{
	// The canvas alone is not proof the plugin is there. OBS 32.2 saves canvases
	// into the scene collection and restores them on load, so once Aitum has run
	// even once, "Aitum Vertical" keeps coming back whether or not the plugin is
	// still installed. Driving a dock off that orphan gives people a Vertical
	// Scene Organiser they cannot use, and leaves us talking to a canvas nobody
	// owns. Require the module to actually be loaded.
	if (!obs_get_module(kVerticalModuleName))
		return nullptr;

	obs_canvas_t *found = nullptr;

	struct obs_frontend_canvas_list list = {0};
	obs_frontend_get_canvases(&list);
	for (size_t i = 0; i < list.canvases.num; i++) {
		obs_canvas_t *c = list.canvases.array[i];
		const char *name = obs_canvas_get_name(c);
		if (name && strcmp(name, kVerticalCanvasName) == 0 && !obs_canvas_removed(c)) {
			found = obs_canvas_get_ref(c);
			break;
		}
	}
	obs_frontend_canvas_list_free(&list);

	return found;
}

bool CollectScene(void *param, obs_source_t *scene)
{
	auto *out = static_cast<std::vector<obs_source_t *> *>(param);
	if (obs_source_t *ref = obs_source_get_ref(scene))
		out->push_back(ref);
	return true;
}

// Aitum keys its proc handlers on canvas dimensions rather than a name, so read
// them off the canvas we already resolved.
bool VerticalDimensions(int &width, int &height)
{
	obs_canvas_t *canvas = AcquireVertical();
	if (!canvas)
		return false;

	struct obs_video_info ovi = {};
	const bool ok = obs_canvas_get_video_info(canvas, &ovi);
	if (ok) {
		width = (int)ovi.base_width;
		height = (int)ovi.base_height;
	}
	obs_canvas_release(canvas);
	return ok;
}

} // namespace

obs_canvas_t *Acquire(CanvasType type)
{
	if (type == CanvasType::Vertical)
		return AcquireVertical();
	return obs_get_main_canvas();
}

bool VerticalAvailable()
{
	obs_canvas_t *canvas = AcquireVertical();
	if (!canvas)
		return false;
	obs_canvas_release(canvas);
	return true;
}

std::vector<obs_source_t *> GetScenes(CanvasType type)
{
	std::vector<obs_source_t *> scenes;

	obs_canvas_t *canvas = Acquire(type);
	if (!canvas)
		return scenes;

	obs_canvas_enum_scenes(canvas, CollectScene, &scenes);
	obs_canvas_release(canvas);

	return scenes;
}

void ReleaseScenes(std::vector<obs_source_t *> &scenes)
{
	for (obs_source_t *s : scenes)
		obs_source_release(s);
	scenes.clear();
}

obs_source_t *GetCurrentScene(CanvasType type)
{
	if (type != CanvasType::Vertical)
		return obs_frontend_get_current_scene();

	// Ask Aitum which scene it considers live. It tracks its own "current"
	// separately from the canvas channel while a transition is running, so its
	// answer is the accurate one.
	int width = 0, height = 0;
	if (VerticalDimensions(width, height)) {
		proc_handler_t *ph = obs_get_proc_handler();
		calldata_t cd = {0};
		calldata_set_int(&cd, "width", width);
		calldata_set_int(&cd, "height", height);
		if (proc_handler_call(ph, "aitum_vertical_get_scene", &cd)) {
			const char *name = calldata_string(&cd, "scene");
			if (name && *name) {
				obs_source_t *source = FindScene(type, name);
				calldata_free(&cd);
				if (source)
					return source;
			} else {
				calldata_free(&cd);
			}
		} else {
			calldata_free(&cd);
		}
	}

	// Fallback: whatever is on the canvas's program channel.
	obs_canvas_t *canvas = Acquire(type);
	if (!canvas)
		return nullptr;
	obs_source_t *source = obs_canvas_get_channel(canvas, 0);
	obs_canvas_release(canvas);
	return source;
}

void SetCurrentScene(CanvasType type, obs_source_t *scene)
{
	if (!scene)
		return;

	if (type != CanvasType::Vertical) {
		obs_frontend_set_current_scene(scene);
		return;
	}

	const char *name = obs_source_get_name(scene);
	if (!name)
		return;

	int width = 0, height = 0;
	if (VerticalDimensions(width, height)) {
		proc_handler_t *ph = obs_get_proc_handler();
		calldata_t cd = {0};
		calldata_set_int(&cd, "width", width);
		calldata_set_int(&cd, "height", height);
		calldata_set_string(&cd, "scene", name);
		const bool handled = proc_handler_call(ph, "aitum_vertical_switch_scene", &cd);
		calldata_free(&cd);
		if (handled)
			return; // Aitum ran the switch through its own transition
	}

	// No proc handler (older Aitum build, or the plugin went away mid-session).
	// Set the program channel directly. Hard cut, but the scene still goes live.
	obs_canvas_t *canvas = Acquire(type);
	if (!canvas)
		return;
	obs_canvas_set_channel(canvas, 0, scene);
	obs_canvas_release(canvas);
}

obs_scene_t *CreateScene(CanvasType type, const char *name)
{
	if (!name || !*name)
		return nullptr;

	if (type != CanvasType::Vertical)
		return obs_scene_create(name);

	obs_canvas_t *canvas = Acquire(type);
	if (!canvas)
		return nullptr;
	obs_scene_t *scene = obs_canvas_scene_create(canvas, name);
	obs_canvas_release(canvas);
	return scene;
}

obs_source_t *FindScene(CanvasType type, const char *name)
{
	if (!name || !*name)
		return nullptr;

	obs_canvas_t *canvas = Acquire(type);
	if (!canvas)
		return nullptr;

	obs_source_t *found = nullptr;
	obs_scene_t *scene = obs_canvas_get_scene_by_name(canvas, name);
	if (scene) {
		found = obs_source_get_ref(obs_scene_get_source(scene));
		obs_scene_release(scene);
	}
	obs_canvas_release(canvas);

	return found;
}

bool SceneBelongsTo(CanvasType type, obs_source_t *source)
{
	if (!source)
		return false;

	obs_canvas_t *want = Acquire(type);
	if (!want)
		return false;

	obs_canvas_t *has = obs_source_get_canvas(source);
	// A scene created before any canvas existed reports no canvas at all. Treat
	// that as belonging to the main canvas, which is where OBS puts it.
	const bool match = has ? (has == want) : (type == CanvasType::Normal);

	if (has)
		obs_canvas_release(has);
	obs_canvas_release(want);

	return match;
}

} // namespace Canvas
} // namespace SceneOrganiser
} // namespace StreamUP
