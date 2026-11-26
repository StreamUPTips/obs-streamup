# Changelog

## v1.0.0

The StreamUP 3.0 look as a library of its own.

- Theme tokens (`theme.hpp`): colours read at runtime from the active StreamUP
  theme variant, The Andi's gold and charcoal under any other theme; the theme's
  radii, spacing and control heights.
- The kit (`kit.hpp`): status pills, notes, row lists, setting and field rows,
  the switched-off banner, empty states, toasts, picture slots, the lightbox,
  the button preview, the sidebar list style and the icon resolver.
- A Tabler outline icon set for toolbars and docks, centred on each glyph's own
  bounding box, with the generator and an offline centring check.
- Quiet fields: drop-downs and number fields with a hairline and one muted
  chevron; drop-downs open as one rounded panel; soft grey row highlights in
  menus and lists; a calm grey scrollbar; flat switches.
- `cmake/streamup-ui.cmake`: `streamup_ui_add(<target>)` adds the library to a
  plugin in one call.
- Earlier history (window chrome, buttons, inputs, cards, dialogs, feedback)
  comes from the vendored copies in the StreamUP plugins.
