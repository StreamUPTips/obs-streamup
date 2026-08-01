#pragma once

// Canvas plumbing for the Scene Organiser.
//
// OBS 32.2 gained real multi-canvas support, and Aitum's Vertical Canvas plugin
// registers its vertical canvas through it (obs_frontend_add_canvas, named
// "Aitum Vertical"). That means we can list, create, rename and switch its
// scenes with the stock libobs canvas API instead of reaching into the plugin.
//
// Everything here takes a CanvasType so the dock can stay canvas-agnostic: the
// Normal dock talks to the main canvas, the Vertical dock talks to Aitum's, and
// the call sites read the same either way.
//
// Reference counting follows the usual libobs rules. Anything returned from a
// Get/Create/Find function here is a NEW reference the caller must release.

#include <obs.h>

#include <string>
#include <vector>

#include "scene-organiser-dock.hpp"

namespace StreamUP {
namespace SceneOrganiser {
namespace Canvas {

// The canvas name Aitum registers. Matches CANVAS_NAME in vertical-canvas.cpp.
// If they ever rename it, the vertical dock simply stops finding a canvas and
// hides itself, which is the same as the plugin not being installed.
extern const char *const kVerticalCanvasName;

// Acquire the canvas backing a dock. Returns a NEW reference, or nullptr when
// the vertical canvas is not present (plugin not installed, or removed).
// Callers must obs_canvas_release() a non-null result.
obs_canvas_t *Acquire(CanvasType type);

// Whether Aitum's vertical canvas currently exists. Drives whether the vertical
// dock is created at all.
bool VerticalAvailable();

// Every scene on the canvas, in canvas order. Each source is a NEW reference;
// release them all when done. Mirrors obs_frontend_get_scenes for the main
// canvas so the organiser's tree code reads the same for both.
std::vector<obs_source_t *> GetScenes(CanvasType type);

// Release a list handed back by GetScenes.
void ReleaseScenes(std::vector<obs_source_t *> &scenes);

// The scene currently live on the canvas, or nullptr. NEW reference.
obs_source_t *GetCurrentScene(CanvasType type);

// Make a scene live. The vertical path asks Aitum to do the switch (its
// proc handler runs the switch through its own transition and keeps its dock in
// step); if the proc handler is missing we fall back to setting the canvas
// channel ourselves, which is a hard cut but still works.
void SetCurrentScene(CanvasType type, obs_source_t *scene);

// Create a scene on the canvas. NEW reference to the scene, or nullptr.
obs_scene_t *CreateScene(CanvasType type, const char *name);

// Find a scene on the canvas by name. NEW reference, or nullptr.
obs_source_t *FindScene(CanvasType type, const char *name);

// Whether a scene source lives on this dock's canvas. Used to keep vertical
// scenes out of the Normal dock and vice versa.
bool SceneBelongsTo(CanvasType type, obs_source_t *source);

} // namespace Canvas
} // namespace SceneOrganiser
} // namespace StreamUP
