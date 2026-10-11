#include "canvas-lookup.hpp"

#include <obs-frontend-api.h>

#include <cstring>

namespace StreamUP {
namespace CanvasLookup {

namespace {

struct FindByName {
	const char *name;
	obs_canvas_t *main;
	obs_source_t *found;
};

bool FindOnCanvas(void *param, obs_canvas_t *canvas)
{
	auto *f = static_cast<FindByName *>(param);
	if (canvas == f->main || obs_canvas_removed(canvas))
		return true;
	// Owned reference, or nullptr.
	f->found = obs_canvas_get_source_by_name(canvas, f->name);
	return f->found == nullptr;
}

} // namespace

obs_canvas_t *AcquireVertical()
{
	// Aitum's module name (its file name without the extension) and the
	// canvas name it registers (CANVAS_NAME in vertical-canvas.cpp).
	if (!obs_get_module("vertical-canvas"))
		return nullptr;
	obs_canvas_t *found = nullptr;
	struct obs_frontend_canvas_list list = {0};
	obs_frontend_get_canvases(&list);
	for (size_t i = 0; i < list.canvases.num; i++) {
		obs_canvas_t *c = list.canvases.array[i];
		const char *name = obs_canvas_get_name(c);
		if (name && strcmp(name, "Aitum Vertical") == 0 && !obs_canvas_removed(c)) {
			found = obs_canvas_get_ref(c);
			break;
		}
	}
	obs_frontend_canvas_list_free(&list);
	return found;
}

obs_source_t *GetSourceByName(const char *name)
{
	if (!name || !*name)
		return nullptr;
	if (obs_source_t *src = obs_get_source_by_name(name))
		return src;

	obs_canvas_t *main = obs_get_main_canvas();
	FindByName f{name, main, nullptr};
	obs_enum_canvases(FindOnCanvas, &f);
	obs_canvas_release(main);
	return f.found;
}

bool GetCanvasSize(obs_source_t *scene, uint32_t &width, uint32_t &height)
{
	struct obs_video_info ovi = {};
	bool ok = false;
	if (scene) {
		if (obs_canvas_t *canvas = obs_source_get_canvas(scene)) {
			ok = obs_canvas_get_video_info(canvas, &ovi);
			obs_canvas_release(canvas);
		}
	}
	if (!ok)
		ok = obs_get_video_info(&ovi);
	if (!ok || !ovi.base_width || !ovi.base_height)
		return false;
	width = ovi.base_width;
	height = ovi.base_height;
	return true;
}

} // namespace CanvasLookup
} // namespace StreamUP
