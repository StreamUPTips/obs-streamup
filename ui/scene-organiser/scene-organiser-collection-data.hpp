#pragma once

// Carries the Scene Organiser's per-collection layout inside the scene
// collection file itself, so it travels with the collection.
//
// The organiser keeps its folders, tabs, hidden scenes and lock state in its
// own config folder, keyed by collection name. A duplicated, renamed or
// imported collection arrives under a name that folder has never seen, and
// used to open with no folders at all. OBS copies the collection file in all
// three cases, so a copy kept in there comes along for free.
//
// The config folder stays the source of truth. On save we mirror it into the
// collection; on load we only write it back for a canvas that has nothing
// saved locally under this collection's name.
namespace StreamUP {
namespace SceneOrganiser {
namespace CollectionData {

void Register();
void Unregister();

} // namespace CollectionData
} // namespace SceneOrganiser
} // namespace StreamUP
