#pragma once

// StreamUP theme tokens: the StreamUP OBS theme's own sizes, and its colours
// read at runtime from the active StreamUP theme variant (The StreamUP, The
// Andi, Silverlink, ...). Under any other theme the tokens are The Andi's gold
// and charcoal. LoadTokens() points the shared UIStyles::Colors tokens at the
// result, so everything built from gallery-style follows the theme.

#include <QString>

namespace StreamUP {
namespace Kit {

// The StreamUP OBS theme's own tokens, so every StreamUP window looks like
// it belongs to the theme. Sizes are design px (pass through S()).
namespace Theme {
constexpr int RadiusMenuItem = 6;  // --menu_item_pill_radius: a highlighted row
constexpr int RadiusControl = 8;   // --radius_sm: fields and drop-downs
constexpr int RadiusCard = 12;     // --settings_group_radius / --radius_md_plus: cards, menus
constexpr int RadiusPill = 14;     // --radius_lg: buttons
constexpr int RadiusLarge = 16;    // --radius_xl: large panels
constexpr int RadiusRail = 22;     // --radius_dock: sidebars and docks
constexpr int MenuPad = 8;         // --menu_pad_x / --menu_pad_y
constexpr int MenuItemPadX = 16;   // --menu_item_pad_x
constexpr int MenuItemHeight = 28; // --menu_item_height
constexpr int ControlHeight = 28;  // one height for fields, drop-downs and buttons
constexpr int CardPadding = 20;    // --settings_group_padding
constexpr int CardSpacing = 16;    // --settings_group_content_spacing

// Points the shared colour tokens at the active StreamUP theme variant
// (The StreamUP, The Andi, Silverlink, ...), read from the theme's own
// variables. Any other theme gets The Andi's gold values. Call at load and
// whenever OBS's theme changes; windows opened afterwards follow it.
void LoadTokens();
// The theme the tokens came from, e.g. "com.streamup.andi", or "built-in".
QString TokenSource();
} // namespace Theme

} // namespace Kit
} // namespace StreamUP
