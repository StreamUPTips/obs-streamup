#pragma once

// "Switch to Scene 'Name'" in the Sources dock's right-click menu.
//
// Right-clicking a nested scene, or a Source Clone of a scene, in OBS's
// Sources dock or Aitum Vertical's Vertical Sources dock adds entries at the
// top of that menu to switch to the scene: the main canvas for a main scene,
// the vertical canvas (through Aitum's own switch) for a vertical one. In
// studio mode a main scene can go to preview, as picking it in the Scenes
// dock does, or straight to program.
//
// Neither menu has a hook for plugins: OBS and Aitum build it on the stack and
// run it with exec(). So an application event filter notes which row a
// SourceTree was right-clicked on, and when the next top-level menu is about
// to be laid out it adds the entries before everything else. That relies on
// nothing in the menu's own contents, so it keeps working when OBS changes
// what the menu holds.
//
// The entries only appear while the "Switch to a nested scene" module is on
// (StreamUP Settings > Plugins); the setting is read on every right-click, so
// switching it takes effect straight away.

namespace StreamUP {
namespace SourceMenuSceneSwitch {

// Call once OBS has finished loading (the main window must exist).
void Install();
// Call on exit.
void Uninstall();

} // namespace SourceMenuSceneSwitch
} // namespace StreamUP
