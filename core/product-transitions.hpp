#pragma once

// Transition products: a scene that plays as a scene transition through the
// StreamUP Scene as Transition plugin ("scene_as_transition").
//
// OBS has no frontend API that adds a transition to its list. The only way in
// is the frontend's own "add transition" path, which asks for a name and then
// opens the transition's properties. UseSceneAsTransition drives that path at
// runtime: it fills in the name, closes the properties window, then writes the
// settings itself. OBS itself is untouched.

#include "overlay-controls.hpp"

#include <obs.h>

#include <QString>

namespace StreamUP {
namespace ProductTransitions {

constexpr const char *kSceneAsTransitionId = "scene_as_transition";

// A transition in OBS's list, by UUID when one is given and still valid, else
// by name. NEW reference or nullptr. Safe from any thread.
obs_source_t *Find(const QString &name, const QString &uuidHint = QString());

// Whether the transition with this name is the one picked in the Transitions dock.
bool IsCurrent(const QString &name);

// Creates the transition when it is missing (or points the existing one at the
// scene), then picks it when the action asks to. UI thread only.
bool UseSceneAsTransition(const OverlayControls::ActionInfo &action, bool *created, QString *error);

} // namespace ProductTransitions
} // namespace StreamUP
