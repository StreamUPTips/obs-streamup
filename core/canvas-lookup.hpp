#pragma once

// Finding scenes on every canvas, not just the main one.
//
// obs_get_source_by_name only sees inputs and the MAIN canvas's scenes. A scene
// on another canvas (Aitum Vertical's "V • ..." scenes) is invisible to it, so
// anything that resolves a scene by name has to ask the other canvases too.
// Lookups by UUID (obs_get_source_by_uuid) already cover every canvas.
//
// Everything returned is a NEW reference the caller must release.

#include <obs.h>

#include <cstdint>

namespace StreamUP {
namespace CanvasLookup {

// Aitum Vertical's canvas, only while the plugin is actually loaded (OBS 32.2
// restores a saved canvas from the scene collection even after the plugin has
// been removed, and nobody drives that orphan). NEW reference or nullptr.
obs_canvas_t *AcquireVertical();

// A source by name: inputs and main-canvas scenes first, then the scenes of
// every other canvas. NEW reference or nullptr.
obs_source_t *GetSourceByName(const char *name);

// The base size of the canvas a scene draws on: its own canvas when it has
// one, else the main canvas. False only when no video is set up at all.
bool GetCanvasSize(obs_source_t *scene, uint32_t &width, uint32_t &height);

} // namespace CanvasLookup
} // namespace StreamUP
