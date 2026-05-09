#pragma once

#include <obs.h>
#include <graphics/graphics.h>
#include <util/threading.h>

#include <string>
#include <vector>

namespace StreamUP {
namespace AdjustmentLayer {

enum FilterMode {
	FILTER_MODE_ALL_BELOW = 0,
	FILTER_MODE_INCLUDE_LIST = 1,
	FILTER_MODE_EXCLUDE_LIST = 2,
};

struct AdjustmentLayerData {
	obs_source_t *source;

	gs_texrender_t *composite_render;
	gs_texrender_t *scratch_render;
	gs_effect_t *opacity_effect;

	uint32_t canvas_width;
	uint32_t canvas_height;

	// Mutex protecting filter_sources writes from Update() (UI thread)
	// against reads in VideoTick/VideoRender (render thread)
	pthread_mutex_t settings_mutex;

	// Properties (simple types are safe for unsynchronized cross-thread
	// reads on x86 - worst case is a one-frame stale value)
	int opacity;         // 0-100, default 100
	bool group_only;     // When in a group, only affect group siblings
	bool hide_originals; // Replace originals with filtered output
	int filter_mode;     // FilterMode enum
	bool auto_snap_zorder; // In include/exclude mode, snap our scene-item z-position
	                       // to one slot above the lowest passing item so the
	                       // filtered composite renders at the correct z-order
	                       // and items above stay visible.

	// filter_sources is written by Update() under settings_mutex.
	// render_filter_sources is swapped in VideoTick() and read by render.
	std::vector<std::string> filter_sources;
	std::vector<std::string> render_filter_sources;
	bool settings_dirty;

	// Parent scene cache (refreshed each tick)
	obs_scene_t *cached_parent_scene;
	obs_sceneitem_t *cached_parent_group; // Non-null if inside a group
	bool parent_cache_valid;

	// Hide-originals tracking: IDs of items we programmatically hid
	std::vector<int64_t> actively_hidden_ids;
	bool items_collected_in_tick;

	// Reusable collection buffer (avoids per-frame heap allocation)
	std::vector<obs_sceneitem_t *> collected_items;
};

void Register();

} // namespace AdjustmentLayer
} // namespace StreamUP
