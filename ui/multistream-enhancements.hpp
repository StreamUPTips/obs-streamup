#pragma once

/**
 * Theme enhancements for Aitum Multistream's dock.
 *
 * Aitum builds the dock with stylesheets of its own on the list, on each canvas
 * group and on every output card. Those carry no selector, so they reach every
 * widget inside and beat the theme: padding and margins are zeroed throughout
 * and the cards take a colour picked from the palette once, at load. The theme
 * cannot get past either, so this clears those sheets and names each part for
 * the theme to style instead.
 *
 * It also gives the settings, heart and logo row a widget the theme can paint as
 * a toolbar, the same treatment as the Vertical Canvas control row, lets long
 * output names shorten with an ellipsis rather than push the stream buttons out
 * of the dock, and hides Aitum's partner blocks when the same setting that hides
 * them on the Vertical Canvas dock is on.
 *
 * Only runs under the StreamUP theme, and no-ops quietly if the dock is not the
 * shape it expects, since that shape is Aitum's to change.
 */

namespace StreamUP {
namespace MultistreamEnhancements {

/**
 * Applies the enhancements to the Multistream dock if it exists. Safe to call
 * repeatedly: a dock that has already been done is only refreshed.
 */
void ApplyMultistreamEnhancements();

/**
 * Puts the dock back as Aitum built it, for a theme change away from StreamUP.
 */
void CleanupMultistreamEnhancements();

/**
 * Re-reads whether partner blocks should be hidden and applies it to the dock.
 */
void RefreshPartnerBlocks();

} // namespace MultistreamEnhancements
} // namespace StreamUP
