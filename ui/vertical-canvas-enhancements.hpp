#pragma once

/**
 * Theme enhancements for Aitum's Vertical Canvas dock.
 *
 * The dock puts its stream, record, backtrack and virtual camera buttons in a
 * bare layout rather than a toolbar, so there is no widget behind them for a
 * theme to paint. Qt cannot style a layout, which means the row cannot be given
 * the background every other toolbar in the StreamUP theme has.
 *
 * This wraps that row in a named widget so the theme can reach it. Nothing is
 * moved, replaced or reordered: the same layout, with the same buttons in it,
 * simply gains a parent that can be painted.
 *
 * Only runs under the StreamUP theme, like the mixer enhancements, and is
 * written to no-op quietly if Aitum's layout is not the shape it expects, since
 * that shape is theirs to change.
 */

namespace StreamUP {
namespace VerticalCanvasEnhancements {

/**
 * Wraps the control row of every Vertical Canvas dock currently open. Safe to
 * call repeatedly: a dock that has already been wrapped is left alone.
 */
void ApplyVerticalCanvasEnhancements();

/**
 * Puts the docks back as they were, for a theme change away from StreamUP.
 */
void CleanupVerticalCanvasEnhancements();

/**
 * Flips whether the partner blocks Aitum fetches into the dock are shown, writes
 * the answer to OBS' user config so it survives a restart, and applies it to
 * every open dock straight away.
 *
 * Bound to a hotkey that ships unassigned. Hiding another plugin's promotion of
 * its own product is the user's call, not something StreamUP does by default or
 * puts a switch for in its settings window.
 */
void TogglePartnerBlocks();

} // namespace VerticalCanvasEnhancements
} // namespace StreamUP
