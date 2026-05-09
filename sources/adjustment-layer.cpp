#include "adjustment-layer.hpp"
#include "version.h"

#include <obs.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/threading.h>
#include <graphics/vec4.h>
#include <graphics/matrix4.h>
#include <graphics/srgb.h>

#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#define SOURCE_ID "streamup_adjustment_layer"

// Compatibility: TransitionIsActive() was added after OBS 31.1.0.
// Approximate it by checking whether the transition time is mid-progress.
static inline bool TransitionIsActive(obs_source_t *transition)
{
	float t = obs_transition_get_time(transition);
	return t > 0.0f && t < 1.0f;
}

// Scene-item visibility mutations (set_visible) are global and fire instantly,
// which causes originals to pop on/off mid-transition. Defer mutations while
// the program transition is animating.
static bool IsProgramTransitionActive()
{
	obs_source_t *tr = obs_frontend_get_current_transition();
	if (!tr)
		return false;
	bool active = TransitionIsActive(tr);
	obs_source_release(tr);
	return active;
}

// Inline effect shader for opacity - multiplies all 4 channels (premultiplied-safe)
static const char *OPACITY_EFFECT_STR =
	"uniform float4x4 ViewProj;\n"
	"uniform texture2d image;\n"
	"uniform float4 color;\n"
	"sampler_state texSampler {\n"
	"    Filter = Linear;\n"
	"    AddressU = Clamp;\n"
	"    AddressV = Clamp;\n"
	"};\n"
	"struct VertData {\n"
	"    float4 pos : POSITION;\n"
	"    float2 uv : TEXCOORD0;\n"
	"};\n"
	"VertData VS(VertData v) {\n"
	"    VertData o;\n"
	"    o.pos = mul(float4(v.pos.xyz, 1.0), ViewProj);\n"
	"    o.uv = v.uv;\n"
	"    return o;\n"
	"}\n"
	"float4 PS(VertData v) : TARGET {\n"
	"    return image.Sample(texSampler, v.uv) * color;\n"
	"}\n"
	"technique Draw {\n"
	"    pass {\n"
	"        vertex_shader = VS(v);\n"
	"        pixel_shader = PS(v);\n"
	"    }\n"
	"}\n";

// Blend mode parameters - replicated from obs-scene.c
static const struct {
	enum gs_blend_type src_color;
	enum gs_blend_type src_alpha;
	enum gs_blend_type dst_color;
	enum gs_blend_type dst_alpha;
	enum gs_blend_op_type op;
} blend_mode_params[] = {
	// OBS_BLEND_NORMAL
	{GS_BLEND_ONE, GS_BLEND_ONE, GS_BLEND_INVSRCALPHA,
	 GS_BLEND_INVSRCALPHA, GS_BLEND_OP_ADD},
	// OBS_BLEND_ADDITIVE
	{GS_BLEND_ONE, GS_BLEND_ONE, GS_BLEND_ONE, GS_BLEND_ONE,
	 GS_BLEND_OP_ADD},
	// OBS_BLEND_SUBTRACT
	{GS_BLEND_ONE, GS_BLEND_ONE, GS_BLEND_ONE, GS_BLEND_ONE,
	 GS_BLEND_OP_REVERSE_SUBTRACT},
	// OBS_BLEND_SCREEN
	{GS_BLEND_ONE, GS_BLEND_ONE, GS_BLEND_INVSRCCOLOR,
	 GS_BLEND_INVSRCALPHA, GS_BLEND_OP_ADD},
	// OBS_BLEND_MULTIPLY
	{GS_BLEND_DSTCOLOR, GS_BLEND_DSTALPHA, GS_BLEND_INVSRCALPHA,
	 GS_BLEND_INVSRCALPHA, GS_BLEND_OP_ADD},
	// OBS_BLEND_LIGHTEN
	{GS_BLEND_ONE, GS_BLEND_ONE, GS_BLEND_ONE, GS_BLEND_ONE,
	 GS_BLEND_OP_MAX},
	// OBS_BLEND_DARKEN
	{GS_BLEND_ONE, GS_BLEND_ONE, GS_BLEND_ONE, GS_BLEND_ONE,
	 GS_BLEND_OP_MIN},
};

namespace StreamUP {
namespace AdjustmentLayer {

// ---- Forward declarations ----
static const char *GetName(void *type_data);
static void *Create(obs_data_t *settings, obs_source_t *source);
static void Destroy(void *data);
static void Activate(void *data);
static void Deactivate(void *data);
static uint32_t GetWidth(void *data);
static uint32_t GetHeight(void *data);
static void VideoTick(void *data, float seconds);
static void VideoRender(void *data, gs_effect_t *effect);
static obs_properties_t *GetProperties(void *data);
static void GetDefaults(obs_data_t *settings);
static void Update(void *data, obs_data_t *settings);

// ---- Helper: check if crop is non-zero ----
static inline bool HasCrop(const struct obs_sceneitem_crop &crop)
{
	return crop.left > 0 || crop.top > 0 || crop.right > 0 ||
	       crop.bottom > 0;
}

// ---- Helper: compute bounds_crop from public API ----
// Replicates the bounds-crop logic from obs-scene.c calculate_bounds_data().
// The internal item->bounds_crop has no public getter; obs_sceneitem_get_bounds_crop()
// only returns the boolean flag. This helper recomputes the pixel crop values.
static struct obs_sceneitem_crop ComputeBoundsCrop(obs_sceneitem_t *item)
{
	struct obs_sceneitem_crop bc = {};

	struct obs_transform_info info;
	obs_sceneitem_get_info2(item, &info);

	if (!info.crop_to_bounds)
		return bc;
	if (info.bounds_type != OBS_BOUNDS_SCALE_OUTER &&
	    info.bounds_type != OBS_BOUNDS_SCALE_TO_WIDTH &&
	    info.bounds_type != OBS_BOUNDS_SCALE_TO_HEIGHT)
		return bc;

	obs_source_t *source = obs_sceneitem_get_source(item);
	uint32_t width = obs_source_get_width(source);
	uint32_t height = obs_source_get_height(source);
	if (width == 0 || height == 0)
		return bc;

	struct obs_sceneitem_crop crop;
	obs_sceneitem_get_crop(item, &crop);

	uint32_t crop_cx = crop.left + crop.right;
	uint32_t crop_cy = crop.top + crop.bottom;
	uint32_t cx = (crop_cx > width) ? 2 : (width - crop_cx);
	uint32_t cy = (crop_cy > height) ? 2 : (height - crop_cy);

	// info.scale and info.bounds are already in absolute coordinates
	struct vec2 scale = info.scale;
	struct vec2 bounds = info.bounds;

	float fwidth = (float)cx * fabsf(scale.x);
	float fheight = (float)cy * fabsf(scale.y);
	if (fwidth == 0.0f || fheight == 0.0f)
		return bc;
	float item_aspect = fwidth / fheight;
	float bounds_aspect = bounds.x / bounds.y;

	if (info.bounds_type == OBS_BOUNDS_SCALE_OUTER) {
		bool use_width = !(bounds_aspect < item_aspect);
		float mul = use_width ? bounds.x / fwidth
				      : bounds.y / fheight;
		vec2_mulf(&scale, &scale, mul);
	} else if (info.bounds_type == OBS_BOUNDS_SCALE_TO_WIDTH) {
		vec2_mulf(&scale, &scale, bounds.x / fwidth);
	} else if (info.bounds_type == OBS_BOUNDS_SCALE_TO_HEIGHT) {
		vec2_mulf(&scale, &scale, bounds.y / fheight);
	}

	float scaled_w = (float)cx * scale.x;
	float scaled_h = (float)cy * scale.y;

	float width_diff = bounds.x - fabsf(scaled_w);
	float height_diff = bounds.y - fabsf(scaled_h);

	if (width_diff >= -0.1f && height_diff >= -0.1f)
		return bc;

	bool crop_width = width_diff < -0.1f;
	bool crop_flipped = crop_width ? scaled_w < 0.0f : scaled_h < 0.0f;

	float crop_diff = crop_width ? width_diff : height_diff;
	float crop_scale = crop_width ? scale.x : scale.y;

	uint32_t crop_align_mask =
		crop_width ? (OBS_ALIGN_LEFT | OBS_ALIGN_RIGHT)
			   : (OBS_ALIGN_TOP | OBS_ALIGN_BOTTOM);
	uint32_t crop_align = info.bounds_alignment & crop_align_mask;

	float overdraw = fabsf(crop_diff / crop_scale);

	float overdraw_tl;
	if (crop_align & (OBS_ALIGN_TOP | OBS_ALIGN_LEFT))
		overdraw_tl = 0;
	else if (crop_align & (OBS_ALIGN_BOTTOM | OBS_ALIGN_RIGHT))
		overdraw_tl = overdraw;
	else
		overdraw_tl = overdraw / 2;

	float overdraw_br = overdraw - overdraw_tl;

	int crop_br, crop_tl;
	if (crop_flipped) {
		crop_br = (int)roundf(overdraw_tl);
		crop_tl = (int)roundf(overdraw_br);
	} else {
		crop_br = (int)roundf(overdraw_br);
		crop_tl = (int)roundf(overdraw_tl);
	}

	if (crop_width) {
		bc.right = crop_br;
		bc.left = crop_tl;
	} else {
		bc.bottom = crop_br;
		bc.top = crop_tl;
	}

	return bc;
}

// ---- Helper: calculate cropped width ----
static inline uint32_t CalcCroppedWidth(const struct obs_sceneitem_crop &crop,
					const struct obs_sceneitem_crop &bcrop,
					uint32_t width)
{
	uint32_t crop_cx =
		crop.left + crop.right + bcrop.left + bcrop.right;
	return (crop_cx > width) ? 2 : (width - crop_cx);
}

// ---- Helper: calculate cropped height ----
static inline uint32_t
CalcCroppedHeight(const struct obs_sceneitem_crop &crop,
		  const struct obs_sceneitem_crop &bcrop, uint32_t height)
{
	uint32_t crop_cy =
		crop.top + crop.bottom + bcrop.top + bcrop.bottom;
	return (crop_cy > height) ? 2 : (height - crop_cy);
}

// ---- Helper: check if a source name is in the filter list ----
static bool IsSourceInList(const std::vector<std::string> &list,
			   const char *name)
{
	if (!name)
		return false;
	for (const auto &entry : list) {
		if (entry == name)
			return true;
	}
	return false;
}

// ---- Helper: check if item passes the filter mode ----
static bool PassesFilter(AdjustmentLayerData *d, obs_sceneitem_t *item)
{
	if (d->filter_mode == FILTER_MODE_ALL_BELOW)
		return true;

	const char *name =
		obs_source_get_name(obs_sceneitem_get_source(item));
	bool in_list = IsSourceInList(d->render_filter_sources, name);

	if (d->filter_mode == FILTER_MODE_INCLUDE_LIST)
		return in_list;
	else // FILTER_MODE_EXCLUDE_LIST
		return !in_list;
}

// ---- Helper: hide an item without triggering its hide transition ----
// Temporarily removes the hide transition, sets invisible, then restores it.
static void HideItemSilently(obs_sceneitem_t *item)
{
	obs_source_t *tr = obs_sceneitem_get_transition(item, false);
	if (tr)
		tr = obs_source_get_ref(tr);

	obs_sceneitem_set_transition(item, false, nullptr);
	obs_sceneitem_set_visible(item, false);
	obs_sceneitem_set_transition(item, false, tr);

	if (tr)
		obs_source_release(tr);
}

// ---- Helper: show an item without triggering its show transition ----
static void ShowItemSilently(obs_sceneitem_t *item)
{
	obs_source_t *tr = obs_sceneitem_get_transition(item, true);
	if (tr)
		tr = obs_source_get_ref(tr);

	obs_sceneitem_set_transition(item, true, nullptr);
	obs_sceneitem_set_visible(item, true);
	obs_sceneitem_set_transition(item, true, tr);

	if (tr)
		obs_source_release(tr);
}

// ---- Helper: check if item is visible or was hidden by us ----
static inline bool IsItemVisible(obs_sceneitem_t *item,
				 const std::vector<int64_t> &hidden_ids)
{
	if (obs_sceneitem_visible(item))
		return true;

	// Item is not visible, but if a hide transition is still playing
	// we need to keep rendering it (so it fades out instead of popping)
	obs_source_t *hide_tr = obs_sceneitem_get_transition(item, false);
	if (hide_tr && TransitionIsActive(hide_tr))
		return true;

	// Check if WE hid this item (still needs to be in our capture)
	if (hidden_ids.empty())
		return false;
	int64_t id = obs_sceneitem_get_id(item);
	for (int64_t hid : hidden_ids)
		if (hid == id)
			return true;
	return false;
}

// ---- Parent scene discovery ----
struct ParentSearchData {
	obs_source_t *target_source;
	obs_scene_t *found_scene;
	obs_sceneitem_t *found_group;
};

static bool FindInGroupCallback(obs_scene_t *scene, obs_sceneitem_t *item,
				void *data)
{
	auto *search = static_cast<ParentSearchData *>(data);
	if (obs_sceneitem_get_source(item) == search->target_source) {
		search->found_scene = scene;
		return false;
	}
	return true;
}

static bool FindInSceneCallback(obs_scene_t *scene, obs_sceneitem_t *item,
				void *data)
{
	auto *search = static_cast<ParentSearchData *>(data);
	obs_source_t *item_source = obs_sceneitem_get_source(item);

	if (item_source == search->target_source) {
		search->found_scene = scene;
		return false;
	}

	if (obs_sceneitem_is_group(item) && item_source) {
		ParentSearchData gsearch = {};
		gsearch.target_source = search->target_source;

		obs_sceneitem_group_enum_items(item, FindInGroupCallback,
					       &gsearch);
		if (gsearch.found_scene) {
			search->found_scene = gsearch.found_scene;
			search->found_group = item;
			return false;
		}
	}

	return true;
}

static bool SceneEnumCallback(void *data, obs_source_t *source)
{
	auto *search = static_cast<ParentSearchData *>(data);
	obs_scene_t *scene = obs_scene_from_source(source);
	if (!scene)
		return true;

	obs_scene_enum_items(scene, FindInSceneCallback, search);

	if (search->found_scene)
		return false;

	return true;
}

// ---- Render a single scene item onto the composite texture ----
static void RenderSceneItem(AdjustmentLayerData *data, obs_sceneitem_t *item)
{
	obs_source_t *item_source = obs_sceneitem_get_source(item);
	uint32_t source_width = obs_source_get_width(item_source);
	uint32_t source_height = obs_source_get_height(item_source);

	if (source_width == 0 || source_height == 0)
		return;

	// Determine what to render: show/hide transition or raw source
	obs_source_t *render_source = item_source;
	bool is_visible = obs_sceneitem_visible(item);

	obs_source_t *show_tr = obs_sceneitem_get_transition(item, true);
	obs_source_t *hide_tr = obs_sceneitem_get_transition(item, false);

	if (is_visible && show_tr && TransitionIsActive(show_tr)) {
		obs_transition_set_size(show_tr, source_width, source_height);
		render_source = show_tr;
	} else if (!is_visible && hide_tr &&
		   TransitionIsActive(hide_tr)) {
		obs_transition_set_size(hide_tr, source_width, source_height);
		render_source = hide_tr;
	}

	struct obs_sceneitem_crop crop;
	obs_sceneitem_get_crop(item, &crop);

	struct obs_sceneitem_crop bounds_crop = ComputeBoundsCrop(item);

	enum obs_blending_type blend_type =
		obs_sceneitem_get_blending_mode(item);
	enum obs_blending_method blend_method =
		obs_sceneitem_get_blending_method(item);

	struct matrix4 draw_transform;
	obs_sceneitem_get_draw_transform(item, &draw_transform);

	bool needs_scratch = (blend_type != OBS_BLEND_NORMAL) ||
			     HasCrop(crop) || HasCrop(bounds_crop);

	if (needs_scratch) {
		uint32_t cx =
			CalcCroppedWidth(crop, bounds_crop, source_width);
		uint32_t cy =
			CalcCroppedHeight(crop, bounds_crop, source_height);
		if (cx == 0 || cy == 0)
			return;

		gs_texrender_reset(data->scratch_render);
		if (!gs_texrender_begin(data->scratch_render, cx, cy))
			return;

		struct vec4 clear_color;
		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_ortho(0.0f, (float)source_width, 0.0f,
			 (float)source_height, -100.0f, 100.0f);

		float cx_scale = (float)source_width / (float)cx;
		float cy_scale = (float)source_height / (float)cy;
		gs_matrix_scale3f(cx_scale, cy_scale, 1.0f);
		gs_matrix_translate3f(
			-(float)(crop.left + bounds_crop.left),
			-(float)(crop.top + bounds_crop.top), 0.0f);

		const bool prev_srgb_src = gs_set_linear_srgb(true);
		obs_source_video_render(render_source);
		gs_set_linear_srgb(prev_srgb_src);

		gs_texrender_end(data->scratch_render);

		gs_texture_t *scratch_tex =
			gs_texrender_get_texture(data->scratch_render);
		if (!scratch_tex)
			return;

		const bool linear_srgb =
			(blend_method != OBS_BLEND_METHOD_SRGB_OFF);
		const bool prev_srgb = gs_set_linear_srgb(linear_srgb);

		gs_blend_state_push();
		gs_blend_function_separate(
			blend_mode_params[blend_type].src_color,
			blend_mode_params[blend_type].dst_color,
			blend_mode_params[blend_type].src_alpha,
			blend_mode_params[blend_type].dst_alpha);
		gs_blend_op(blend_mode_params[blend_type].op);

		gs_matrix_push();
		gs_matrix_mul(&draw_transform);

		gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		gs_eparam_t *image =
			gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(image, scratch_tex);

		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(scratch_tex, 0, 0, 0);

		gs_matrix_pop();
		gs_blend_state_pop();
		gs_set_linear_srgb(prev_srgb);
	} else {
		const bool prev_srgb = gs_set_linear_srgb(true);

		gs_matrix_push();
		gs_matrix_mul(&draw_transform);

		obs_source_video_render(render_source);

		gs_matrix_pop();
		gs_set_linear_srgb(prev_srgb);
	}
}

// ---- Collection callback data and callbacks ----

struct CollectBelowData {
	AdjustmentLayerData *d;
	obs_source_t *self_source;
	std::vector<obs_sceneitem_t *> *items;
	const std::vector<int64_t> *hidden_ids;
};

static bool CollectBelowCallback(obs_scene_t *scene, obs_sceneitem_t *item,
				 void *data)
{
	UNUSED_PARAMETER(scene);
	auto *ctx = static_cast<CollectBelowData *>(data);

	if (obs_sceneitem_get_source(item) == ctx->self_source)
		return false;

	if (IsItemVisible(item, *ctx->hidden_ids) &&
	    PassesFilter(ctx->d, item))
		ctx->items->push_back(item);

	return true;
}

struct CollectBelowGroupData {
	AdjustmentLayerData *d;
	obs_sceneitem_t *group_item;
	std::vector<obs_sceneitem_t *> *items;
	const std::vector<int64_t> *hidden_ids;
};

static bool CollectBelowGroupCallback(obs_scene_t *scene,
				      obs_sceneitem_t *item, void *data)
{
	UNUSED_PARAMETER(scene);
	auto *ctx = static_cast<CollectBelowGroupData *>(data);

	if (item == ctx->group_item)
		return false;

	if (IsItemVisible(item, *ctx->hidden_ids) &&
	    PassesFilter(ctx->d, item))
		ctx->items->push_back(item);

	return true;
}

// ---- Shared collection orchestration ----
static void CollectTargetItems(AdjustmentLayerData *d,
			       const std::vector<int64_t> &hidden_ids)
{
	d->collected_items.clear();
	auto &items = d->collected_items;

	if (!d->parent_cache_valid || !d->cached_parent_scene)
		return;

	if (d->cached_parent_group) {
		obs_source_t *group_source =
			obs_sceneitem_get_source(d->cached_parent_group);
		obs_scene_t *group_scene =
			obs_group_from_source(group_source);
		if (!group_scene)
			return;

		CollectBelowData ctx = {d, d->source, &items, &hidden_ids};
		obs_scene_enum_items(group_scene, CollectBelowCallback,
				     &ctx);

		if (!d->group_only) {
			obs_scene_t *parent_scene = obs_sceneitem_get_scene(
				d->cached_parent_group);
			if (parent_scene) {
				CollectBelowGroupData gctx = {
					d, d->cached_parent_group, &items,
					&hidden_ids};
				obs_scene_enum_items(
					parent_scene,
					CollectBelowGroupCallback, &gctx);
			}
		}
	} else {
		CollectBelowData ctx = {d, d->source, &items, &hidden_ids};
		obs_scene_enum_items(d->cached_parent_scene,
				     CollectBelowCallback, &ctx);
	}
}

// ---- Restore hidden items via callbacks (using silent show) ----
static bool RestoreHiddenCallback(obs_scene_t *scene, obs_sceneitem_t *item,
				  void *data)
{
	UNUSED_PARAMETER(scene);
	auto *ids = static_cast<std::vector<int64_t> *>(data);

	if (!obs_sceneitem_visible(item)) {
		int64_t id = obs_sceneitem_get_id(item);
		for (int64_t hid : *ids) {
			if (hid == id) {
				ShowItemSilently(item);
				break;
			}
		}
	}
	return true;
}

static bool RestoreWithGroupsCallback(obs_scene_t *scene,
				      obs_sceneitem_t *item, void *data)
{
	RestoreHiddenCallback(scene, item, data);

	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, RestoreHiddenCallback,
					       data);

	return true;
}

static bool RestoreEnumCallback(void *param, obs_source_t *source)
{
	obs_scene_t *scene = obs_scene_from_source(source);
	if (!scene)
		return true;

	obs_scene_enum_items(scene, RestoreWithGroupsCallback, param);
	return true;
}

static void RestoreAllHiddenItems(AdjustmentLayerData *d)
{
	if (d->actively_hidden_ids.empty())
		return;

	obs_enum_scenes(RestoreEnumCallback, &d->actively_hidden_ids);
	d->actively_hidden_ids.clear();
}

// ---- Manage hide-originals: restore non-targets callback ----
struct RestoreNonTargetsData {
	const std::vector<int64_t> *actively_hidden_ids;
	const std::vector<int64_t> *new_ids;
};

static bool RestoreNonTargetsCallback(obs_scene_t *scene,
				      obs_sceneitem_t *item, void *data)
{
	UNUSED_PARAMETER(scene);
	auto *ctx = static_cast<RestoreNonTargetsData *>(data);

	if (!obs_sceneitem_visible(item)) {
		int64_t id = obs_sceneitem_get_id(item);
		for (int64_t hid : *ctx->actively_hidden_ids) {
			if (hid == id) {
				bool in_new = false;
				for (int64_t nid : *ctx->new_ids) {
					if (nid == id) {
						in_new = true;
						break;
					}
				}
				if (!in_new)
					ShowItemSilently(item);
				break;
			}
		}
	}
	return true;
}

// ---- Manage hide-originals visibility per tick ----
static void ManageHiddenItems(AdjustmentLayerData *d)
{
	// Collect targets (including items we previously hid)
	CollectTargetItems(d, d->actively_hidden_ids);

	// Build set of new target IDs
	std::vector<int64_t> new_ids;
	new_ids.reserve(d->collected_items.size());
	for (obs_sceneitem_t *item : d->collected_items)
		new_ids.push_back(obs_sceneitem_get_id(item));

	RestoreNonTargetsData rctx = {&d->actively_hidden_ids, &new_ids};

	// Restore in group / scene as needed
	if (d->cached_parent_group) {
		obs_source_t *gs =
			obs_sceneitem_get_source(d->cached_parent_group);
		obs_scene_t *group_scene = obs_group_from_source(gs);
		if (group_scene)
			obs_scene_enum_items(group_scene,
					     RestoreNonTargetsCallback,
					     &rctx);
		if (!d->group_only) {
			obs_scene_t *parent =
				obs_sceneitem_get_scene(
					d->cached_parent_group);
			if (parent)
				obs_scene_enum_items(
					parent, RestoreNonTargetsCallback,
					&rctx);
		}
	} else if (d->cached_parent_scene) {
		obs_scene_enum_items(d->cached_parent_scene,
				     RestoreNonTargetsCallback, &rctx);
	}

	// Hide newly targeted items (silently, without triggering transitions)
	for (obs_sceneitem_t *item : d->collected_items) {
		int64_t id = obs_sceneitem_get_id(item);
		bool in_old = false;
		for (int64_t hid : d->actively_hidden_ids) {
			if (hid == id) {
				in_old = true;
				break;
			}
		}
		if (!in_old && obs_sceneitem_visible(item))
			HideItemSilently(item);
	}

	d->actively_hidden_ids = std::move(new_ids);
}

// ---- Z-order snap (include/exclude modes) ----
//
// In include/exclude mode the user's mental model is "filter only these
// sources at their z-position, leave everything else alone". The AL's natural
// rendering draws its full-canvas composite at the AL's own scene z-order,
// which means a full-canvas included source covers any "not included" source
// that sits above it. The fix is to move the AL itself to one slot above the
// lowest passing item every tick — that way the composite occupies the right
// z-band and items above the AL render on top, undisturbed.

struct SnapScanCtx {
	AdjustmentLayerData *d;
	int pos;
	int self_pos;
	int lowest_passing;
};

static bool SnapScanCallback(obs_scene_t *scene, obs_sceneitem_t *item,
			     void *param)
{
	UNUSED_PARAMETER(scene);
	auto *c = static_cast<SnapScanCtx *>(param);
	if (obs_sceneitem_get_source(item) == c->d->source) {
		c->self_pos = c->pos;
	} else if (PassesFilter(c->d, item)) {
		if (c->lowest_passing == -1)
			c->lowest_passing = c->pos;
	}
	c->pos++;
	return true;
}

static obs_sceneitem_t *FindSelfItem(obs_scene_t *scene, obs_source_t *self)
{
	struct FindCtx {
		obs_source_t *self;
		obs_sceneitem_t *result;
	} ctx = {self, nullptr};
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *p) -> bool {
			auto *c = static_cast<FindCtx *>(p);
			if (obs_sceneitem_get_source(item) == c->self) {
				obs_sceneitem_addref(item);
				c->result = item;
				return false;
			}
			return true;
		},
		&ctx);
	return ctx.result;
}

static void MaybeSnapZOrder(AdjustmentLayerData *d)
{
	if (!d->auto_snap_zorder)
		return;
	if (d->filter_mode == FILTER_MODE_ALL_BELOW)
		return;
	if (!d->parent_cache_valid)
		return;

	// Decide which scene we're in (group's inner scene if the AL sits in
	// a group, otherwise the parent scene).
	obs_scene_t *scene = nullptr;
	if (d->cached_parent_group) {
		obs_source_t *gs =
			obs_sceneitem_get_source(d->cached_parent_group);
		scene = obs_group_from_source(gs);
	} else {
		scene = d->cached_parent_scene;
	}
	if (!scene)
		return;

	SnapScanCtx ctx = {d, 0, -1, -1};
	obs_scene_enum_items(scene, SnapScanCallback, &ctx);

	if (ctx.lowest_passing < 0 || ctx.self_pos < 0)
		return; // Nothing to snap to, or the AL is not in this scene.

	// Target: one slot above the lowest passing item. set_order_position
	// removes-and-reinserts, so we account for the AL's own current
	// position when computing the target index.
	int target = ctx.lowest_passing + 1;
	if (ctx.self_pos < ctx.lowest_passing)
		target = ctx.lowest_passing; // AL was below the passing item; account for the shift on removal.

	if (ctx.self_pos == target)
		return; // Already where we want to be.

	obs_sceneitem_t *self_item = FindSelfItem(scene, d->source);
	if (!self_item)
		return;
	obs_sceneitem_set_order_position(self_item, target);
	obs_sceneitem_release(self_item);
}

// ---- OBS Source Callbacks ----

static const char *GetName(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("AdjustmentLayer.Name");
}

static void *Create(obs_data_t *settings, obs_source_t *source)
{
	auto *data = new AdjustmentLayerData();
	data->source = source;
	data->canvas_width = 0;
	data->canvas_height = 0;
	data->opacity = 100;
	data->group_only = true;
	data->hide_originals = false;
	data->filter_mode = FILTER_MODE_ALL_BELOW;
	data->auto_snap_zorder = true;
	data->settings_dirty = false;
	data->cached_parent_scene = nullptr;
	data->cached_parent_group = nullptr;
	data->parent_cache_valid = false;
	data->items_collected_in_tick = false;
	data->collected_items.reserve(16);

	pthread_mutex_init_value(&data->settings_mutex);
	pthread_mutex_init(&data->settings_mutex, NULL);

	obs_enter_graphics();
	data->composite_render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	data->scratch_render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	data->opacity_effect = gs_effect_create(OPACITY_EFFECT_STR,
						"adjustment_layer_opacity",
						NULL);
	obs_leave_graphics();

	Update(data, settings);
	return data;
}

static void Destroy(void *data)
{
	auto *d = static_cast<AdjustmentLayerData *>(data);

	RestoreAllHiddenItems(d);

	obs_enter_graphics();
	gs_texrender_destroy(d->composite_render);
	gs_texrender_destroy(d->scratch_render);
	gs_effect_destroy(d->opacity_effect);
	obs_leave_graphics();

	pthread_mutex_destroy(&d->settings_mutex);
	delete d;
}

static void Activate(void *data)
{
	UNUSED_PARAMETER(data);
	// Nothing needed - items will be hidden on next VideoTick
}

static void Deactivate(void *data)
{
	auto *d = static_cast<AdjustmentLayerData *>(data);
	RestoreAllHiddenItems(d);
}

static uint32_t GetWidth(void *data)
{
	auto *d = static_cast<AdjustmentLayerData *>(data);
	return d->canvas_width;
}

static uint32_t GetHeight(void *data)
{
	auto *d = static_cast<AdjustmentLayerData *>(data);
	return d->canvas_height;
}

static void VideoTick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	auto *d = static_cast<AdjustmentLayerData *>(data);

	// Skip all work when the source isn't visible in any output
	if (!obs_source_showing(d->source)) {
		// Restore any items we hid before going idle, but not during
		// a program transition - the outgoing scene is still being
		// rendered and mutating visibility would pop originals back on
		// mid-fade.
		if (!d->actively_hidden_ids.empty() &&
		    !IsProgramTransitionActive())
			RestoreAllHiddenItems(d);
		d->parent_cache_valid = false;
		return;
	}

	// Update canvas dimensions
	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		d->canvas_width = ovi.base_width;
		d->canvas_height = ovi.base_height;
	}

	// Reset texrenders for this frame
	gs_texrender_reset(d->composite_render);
	gs_texrender_reset(d->scratch_render);

	// Sync filter sources from UI thread (lock-protected swap)
	pthread_mutex_lock(&d->settings_mutex);
	if (d->settings_dirty) {
		std::swap(d->render_filter_sources, d->filter_sources);
		d->settings_dirty = false;
	}
	pthread_mutex_unlock(&d->settings_mutex);

	// Refresh parent scene cache
	d->cached_parent_scene = nullptr;
	d->cached_parent_group = nullptr;
	d->parent_cache_valid = false;

	ParentSearchData search = {};
	search.target_source = d->source;
	search.found_scene = nullptr;
	search.found_group = nullptr;

	obs_enum_scenes(SceneEnumCallback, &search);

	if (search.found_scene) {
		d->cached_parent_scene = search.found_scene;
		d->cached_parent_group = search.found_group;
		d->parent_cache_valid = true;
	}

	// Manage hide-originals visibility. Visibility mutations are global
	// scene state and will pop mid-transition, so freeze them while the
	// program transition is animating. We still need collected_items
	// populated for VideoRender, so collect without mutating.
	d->items_collected_in_tick = false;
	bool transition_active = IsProgramTransitionActive();
	if (d->hide_originals && d->parent_cache_valid) {
		if (transition_active) {
			CollectTargetItems(d, d->actively_hidden_ids);
		} else {
			ManageHiddenItems(d);
		}
		d->items_collected_in_tick = true;
	} else if (!d->hide_originals && !d->actively_hidden_ids.empty() &&
		   !transition_active) {
		// Feature was just turned off - restore all hidden items
		// (but wait until any transition finishes)
		RestoreAllHiddenItems(d);
	}

	// Snap z-position so include/exclude rendering happens at the
	// correct depth. Skipped during program transitions for the same
	// reason hide-originals is — mutating the source list mid-fade pops
	// items around visually.
	if (!transition_active)
		MaybeSnapZOrder(d);
}

static void VideoRender(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	auto *d = static_cast<AdjustmentLayerData *>(data);

	if (d->canvas_width == 0 || d->canvas_height == 0)
		return;
	if (d->opacity == 0)
		return;
	if (!d->parent_cache_valid || !d->cached_parent_scene)
		return;

	// Use canvas dimensions for rendering (even when in a group, filters
	// apply at canvas resolution)
	uint32_t render_width = d->canvas_width;
	uint32_t render_height = d->canvas_height;

	// When hide_originals is on, items were already collected in VideoTick.
	// Otherwise, collect them now.
	if (!d->items_collected_in_tick) {
		static const std::vector<int64_t> empty_ids;
		CollectTargetItems(d, empty_ids);
	}

	auto &items = d->collected_items;
	if (items.empty())
		return;

	// Begin composite render
	gs_texrender_reset(d->composite_render);
	if (!gs_texrender_begin(d->composite_render, render_width,
				render_height))
		return;

	struct vec4 clear_color;
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
	gs_ortho(0.0f, (float)render_width, 0.0f, (float)render_height,
		 -100.0f, 100.0f);

	// Render each collected item
	gs_blend_state_push();
	gs_reset_blend_state();

	for (obs_sceneitem_t *render_item : items) {
		RenderSceneItem(d, render_item);
	}

	gs_blend_state_pop();
	gs_texrender_end(d->composite_render);

	// Get the composited texture
	gs_texture_t *tex = gs_texrender_get_texture(d->composite_render);
	if (!tex)
		return;

	// Draw output with opacity using custom effect
	// The composite texture is premultiplied alpha, so we scale ALL 4
	// channels by opacity to maintain correct premultiplied blending.
	gs_effect_t *draw_effect = d->opacity_effect;
	if (!draw_effect)
		draw_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	gs_eparam_t *image_param =
		gs_effect_get_param_by_name(draw_effect, "image");
	gs_effect_set_texture(image_param, tex);

	float alpha = (float)d->opacity / 100.0f;
	gs_eparam_t *color_param =
		gs_effect_get_param_by_name(draw_effect, "color");
	if (color_param) {
		struct vec4 color_val;
		vec4_set(&color_val, alpha, alpha, alpha, alpha);
		gs_effect_set_vec4(color_param, &color_val);
	}

	const bool prev_srgb = gs_set_linear_srgb(true);

	gs_blend_state_push();
	// Premultiplied alpha blend onto scene
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	while (gs_effect_loop(draw_effect, "Draw"))
		gs_draw_sprite(tex, 0, render_width, render_height);

	gs_blend_state_pop();
	gs_set_linear_srgb(prev_srgb);
}

// ---- Populate picker with sources from the current scene only ----
struct PickerCallbackData {
	obs_property_t *list;
	obs_source_t *self;
};

static bool AddPickerCallback(obs_scene_t *scene, obs_sceneitem_t *item,
			      void *data)
{
	UNUSED_PARAMETER(scene);
	auto *ctx = static_cast<PickerCallbackData *>(data);

	obs_source_t *source = obs_sceneitem_get_source(item);
	if (source && source != ctx->self) {
		const char *name = obs_source_get_name(source);
		if (name && *name) {
			size_t count =
				obs_property_list_item_count(ctx->list);
			bool exists = false;
			for (size_t i = 0; i < count; i++) {
				const char *val =
					obs_property_list_item_string(ctx->list,
								      i);
				if (val && strcmp(val, name) == 0) {
					exists = true;
					break;
				}
			}
			if (!exists)
				obs_property_list_add_string(ctx->list, name,
							     name);
		}

		if (obs_sceneitem_is_group(item))
			obs_sceneitem_group_enum_items(
				item, AddPickerCallback, data);
	}
	return true;
}

// ---- Source picker modified callback ----
static bool SourcePickerChanged(obs_properties_t *props, obs_property_t *prop,
				obs_data_t *settings)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(prop);

	const char *selected =
		obs_data_get_string(settings, "source_picker");
	if (!selected || !*selected)
		return false;

	// Get or create the filter sources array
	obs_data_array_t *array =
		obs_data_get_array(settings, "filter_sources");
	if (!array)
		array = obs_data_array_create();

	// Check for duplicates
	bool exists = false;
	size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		const char *val = obs_data_get_string(item, "value");
		if (val && strcmp(val, selected) == 0)
			exists = true;
		obs_data_release(item);
		if (exists)
			break;
	}

	if (!exists) {
		obs_data_t *new_item = obs_data_create();
		obs_data_set_string(new_item, "value", selected);
		obs_data_array_push_back(array, new_item);
		obs_data_release(new_item);
		obs_data_set_array(settings, "filter_sources", array);
	}

	obs_data_array_release(array);

	// Reset picker back to placeholder
	obs_data_set_string(settings, "source_picker", "");
	return true;
}

// ---- Property visibility callbacks ----
static bool FilterModeChanged(obs_properties_t *props, obs_property_t *prop,
			      obs_data_t *settings)
{
	UNUSED_PARAMETER(prop);
	int mode = (int)obs_data_get_int(settings, "filter_mode");
	bool show = (mode != FILTER_MODE_ALL_BELOW);

	obs_property_set_visible(obs_properties_get(props, "source_picker"),
				 show);
	obs_property_set_visible(
		obs_properties_get(props, "filter_sources"), show);
	obs_property_set_visible(
		obs_properties_get(props, "auto_snap_zorder"), show);
	return true;
}

static obs_properties_t *GetProperties(void *data)
{
	auto *d = static_cast<AdjustmentLayerData *>(data);
	obs_properties_t *props = obs_properties_create();

	// ---- Header description ----
	obs_properties_add_text(
		props, "info_header",
		obs_module_text("AdjustmentLayer.Info.Header"),
		OBS_TEXT_INFO);

	// ---- Effect properties ----
	obs_property_t *opacity = obs_properties_add_int_slider(
		props, "opacity",
		obs_module_text("AdjustmentLayer.Property.Opacity"), 0, 100,
		1);
	obs_property_set_long_description(
		opacity, obs_module_text("AdjustmentLayer.Tooltip.Opacity"));

	obs_property_t *grp = obs_properties_add_bool(
		props, "group_only",
		obs_module_text("AdjustmentLayer.Property.GroupOnly"));
	obs_property_set_long_description(
		grp, obs_module_text("AdjustmentLayer.Tooltip.GroupOnly"));

	obs_property_t *hide_orig = obs_properties_add_bool(
		props, "hide_originals",
		obs_module_text("AdjustmentLayer.Property.HideOriginals"));
	obs_property_set_long_description(
		hide_orig,
		obs_module_text("AdjustmentLayer.Tooltip.HideOriginals"));

	// ---- Source filter properties ----
	obs_property_t *mode_prop = obs_properties_add_list(
		props, "filter_mode",
		obs_module_text("AdjustmentLayer.Property.FilterMode"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_set_long_description(
		mode_prop,
		obs_module_text("AdjustmentLayer.Tooltip.FilterMode"));

	obs_property_list_add_int(
		mode_prop,
		obs_module_text("AdjustmentLayer.Property.FilterMode.AllBelow"),
		FILTER_MODE_ALL_BELOW);
	obs_property_list_add_int(
		mode_prop,
		obs_module_text(
			"AdjustmentLayer.Property.FilterMode.IncludeList"),
		FILTER_MODE_INCLUDE_LIST);
	obs_property_list_add_int(
		mode_prop,
		obs_module_text(
			"AdjustmentLayer.Property.FilterMode.ExcludeList"),
		FILTER_MODE_EXCLUDE_LIST);

	obs_property_set_modified_callback(mode_prop, FilterModeChanged);

	// Source Picker (select to add)
	obs_property_t *picker = obs_properties_add_list(
		props, "source_picker",
		obs_module_text("AdjustmentLayer.Property.SourcePicker"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_set_long_description(
		picker,
		obs_module_text("AdjustmentLayer.Tooltip.SourcePicker"));

	obs_property_list_add_string(
		picker,
		obs_module_text(
			"AdjustmentLayer.Property.SourcePicker.Select"),
		"");

	// Populate dropdown with sources from the current scene only
	if (d) {
		ParentSearchData psearch = {};
		psearch.target_source = d->source;
		obs_enum_scenes(SceneEnumCallback, &psearch);

		obs_scene_t *scene_to_walk = nullptr;
		if (psearch.found_group) {
			scene_to_walk = obs_sceneitem_get_scene(
				psearch.found_group);
		} else if (psearch.found_scene) {
			scene_to_walk = psearch.found_scene;
		}

		if (scene_to_walk) {
			PickerCallbackData pctx = {picker, d->source};
			obs_scene_enum_items(scene_to_walk,
					     AddPickerCallback, &pctx);
		}
	}

	obs_property_set_modified_callback(picker, SourcePickerChanged);

	// Source list (editable list reads directly from settings)
	obs_property_t *filter_list = obs_properties_add_editable_list(
		props, "filter_sources",
		obs_module_text("AdjustmentLayer.Property.FilterSources"),
		OBS_EDITABLE_LIST_TYPE_STRINGS, NULL, NULL);
	obs_property_set_long_description(
		filter_list,
		obs_module_text("AdjustmentLayer.Tooltip.FilterSources"));

	// Auto z-order snap toggle. Only meaningful in include/exclude mode —
	// in "All Sources Below" mode the AL's z-position already matches the
	// user's intent (filter everything below where I am).
	obs_property_t *snap_prop = obs_properties_add_bool(
		props, "auto_snap_zorder",
		obs_module_text("AdjustmentLayer.Property.AutoSnapZ"));
	obs_property_set_long_description(
		snap_prop,
		obs_module_text("AdjustmentLayer.Tooltip.AutoSnapZ"));

	// Set initial visibility (hidden for "All Sources" mode)
	if (d) {
		bool show = (d->filter_mode != FILTER_MODE_ALL_BELOW);
		obs_property_set_visible(picker, show);
		obs_property_set_visible(filter_list, show);
		obs_property_set_visible(snap_prop, show);
	}

	// ---- Footer ----
	std::string footer =
		std::string("<br><b>Adjustment Layer</b>"
			    " &middot; <a href='https://streamup.tips'>StreamUP</a> v") +
		PROJECT_VERSION +
		"<br><br><a href='https://paypal.me/andilippi'>"
		"Send us a Beer!</a>";
	obs_properties_add_text(props, "info_footer", footer.c_str(),
				OBS_TEXT_INFO);

	return props;
}

static void GetDefaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "opacity", 100);
	obs_data_set_default_bool(settings, "group_only", true);
	obs_data_set_default_bool(settings, "hide_originals", false);
	obs_data_set_default_int(settings, "filter_mode",
				 FILTER_MODE_ALL_BELOW);
	// On by default: in include/exclude mode the user's mental model is
	// "filter these specific sources at their position". Without auto-snap
	// the composite ends up drawn at the AL's own z-position, covering
	// non-included sources above the included ones.
	obs_data_set_default_bool(settings, "auto_snap_zorder", true);
}

static void Update(void *data, obs_data_t *settings)
{
	auto *d = static_cast<AdjustmentLayerData *>(data);
	d->opacity = (int)obs_data_get_int(settings, "opacity");
	d->group_only = obs_data_get_bool(settings, "group_only");
	d->hide_originals = obs_data_get_bool(settings, "hide_originals");
	d->filter_mode = (int)obs_data_get_int(settings, "filter_mode");
	d->auto_snap_zorder = obs_data_get_bool(settings, "auto_snap_zorder");

	// Parse the source list into filter_sources (mutex-protected
	// to prevent data race with render thread reading via swap)
	pthread_mutex_lock(&d->settings_mutex);
	d->filter_sources.clear();
	obs_data_array_t *array =
		obs_data_get_array(settings, "filter_sources");
	if (array) {
		size_t count = obs_data_array_count(array);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *item = obs_data_array_item(array, i);
			const char *val =
				obs_data_get_string(item, "value");
			if (val && *val)
				d->filter_sources.push_back(val);
			obs_data_release(item);
		}
		obs_data_array_release(array);
	}
	d->settings_dirty = true;
	pthread_mutex_unlock(&d->settings_mutex);
}

// ---- Registration ----

void Register()
{
	obs_source_info info = {};
	info.id = SOURCE_ID;
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	info.icon_type = OBS_ICON_TYPE_COLOR;

	info.get_name = GetName;
	info.create = Create;
	info.destroy = Destroy;
	info.activate = Activate;
	info.deactivate = Deactivate;
	info.get_width = GetWidth;
	info.get_height = GetHeight;
	info.video_tick = VideoTick;
	info.video_render = VideoRender;
	info.get_properties = GetProperties;
	info.get_defaults = GetDefaults;
	info.update = Update;

	obs_register_source(&info);

	blog(LOG_INFO, "[StreamUP] Adjustment Layer source registered");
}

} // namespace AdjustmentLayer
} // namespace StreamUP
