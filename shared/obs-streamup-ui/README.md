# obs-streamup-ui

The StreamUP UI library: the look every StreamUP OBS plugin shares. Window
chrome, buttons, fields, cards, rows, toasts, a lightbox, the theme tokens read
from the StreamUP OBS theme, and a Tabler outline icon set. Qt 6, C++17, built
straight into each plugin.

Version: see `CHANGELOG.md`. Tags are `vMAJOR.MINOR.PATCH`.

## Visibility

The UI library is public (it already was, inside obs-streamup). Plugins may be
private; nothing private goes in this repo.

Rule: never put member-only code, keys or product logic in the UI library. It
holds the look (styles, widgets, tokens, icons) and nothing a plugin sells or
protects. Anything specific to one plugin stays in that plugin.

## What is in it

All headers live under `include/streamup/ui/`. Include them as
`#include <streamup/ui/<name>.hpp>`.

### Theme and tokens

| Header | What it gives you |
|---|---|
| `theme.hpp` | `Kit::Theme::LoadTokens()` reads the active StreamUP theme variant's palette (The StreamUP, The Andi, Silverlink, ...) from the theme files and points `UIStyles::Colors` at it. Under any other theme the colours are The Andi's gold and charcoal. `TokenSource()` says where they came from. Radii and spacing: `RadiusMenuItem` 6, `RadiusControl` 8, `RadiusCard` 12, `RadiusPill` 14, `RadiusLarge` 16, `RadiusRail` 22, `MenuPad` 8, `MenuItemPadX` 16, `MenuItemHeight` 28, `ControlHeight` 28, `CardPadding` 20, `CardSpacing` 16. |
| `gallery-style.hpp` | `UIStyles::Colors` (surfaces, accent, text, borders, popups), `Type` (the type scale: 18 / 14 / 13 / 12, weights 400 / 500 / 600), `Sizes`, `S()` / `scale_qss()` for OS text scaling, and style strings: `buttonStyle`, `lineEditStyle`, `plainTextStyle`, `comboStyle`, `menuStyle`, `listStyle`, `treeStyle`, `cardStyle`. |

Call `StreamUP::Kit::Theme::LoadTokens()` once in `obs_module_load()`, and again
on `OBS_FRONTEND_EVENT_THEME_CHANGED`. Windows opened afterwards follow the theme.

### The kit (`kit.hpp`, namespace `StreamUP::Kit`)

| API | Use |
|---|---|
| `Space::Page / PageTop / Block / CardX / CardY / Row / Gap` | One spacing scale. |
| `StatusPill(status, text)` | A coloured pill: Ok, Warn, Bad, Info, Neutral. |
| `Note(status, text)` | A short note in a tinted box with an icon. |
| `RowList` | A rounded card of rows: leading widget, title, detail line, trailing widgets. |
| `SettingRow(label, control, help, icon)` | One setting on one line, with a "?" tooltip. |
| `FieldRow(label, placeholder, &edit)` | A labelled full-width text field for a form card. |
| `HelpDot(text)` | The small "?" with its text as a tooltip. |
| `ScrollingBody(layout)` / `FitToContent(window, width)` | A window as tall as its contents, scrolling only when the screen runs out. |
| `SwitchedOffBanner(name)` / `DimForSwitchedOff(widget, off)` / `SetOpenFeaturesHook(fn)` | A settings page whose feature is switched off: one line, a button to where it is switched on, the settings greyed out. |
| `MoreOptions` | A folded section (avoid in Settings: StreamUP Settings use plain boxed sections). |
| `EmptyState(icon, line, button, onClick)` | An empty list: a large icon, one line, the next step. |
| `Toast(text, status, icon)` | A small pill in the bottom corner of OBS for a few seconds. Safe from any thread. |
| `PreviewSlot(folder, id, height)` | A picture slot: animated WebP or GIF, else PNG, from the plugin's `data/<folder>/<id>.*`. `setZoom()` opens it big. |
| `Lightbox(window, folder, id, title, line, buttons)` | A picture or clip big over a window. |
| `ButtonPreview` | How a toolbar button will look: icon tile, caption, tooltip. |
| `NavListQss()` | The sidebar list style (soft raised row for the current page). |
| `IconPath(name)` / `SetIconResolver(fn)` | Theme-aware icon files: the kit's own set first, then the plugin's `:images/icons/ui/<name>-dark|light.svg`. |

The rounded drop-down popup is automatic for every `MacComboBox`
(`mac-inputs.hpp`) and anything passed to `makeComboAnimated()` (`glass.hpp`).

### Widgets and helpers

`window-chrome.hpp` (frameless window shell: `makeWindow`, `applyChrome`,
`ShadowDialog`, `RoundedContainer`), `pill-button.hpp` (`PillButton`, `Badge`,
`IconButton`), `mac-inputs.hpp` (`MacComboBox`, `MacSpinBox`),
`switch-button.hpp`, `ios-checkbox.hpp`, `segmented-control.hpp`,
`search-field.hpp`, `section-card.hpp` (`sectionCard`, `sectionHeader`),
`labels.hpp` (`makeLabel`), `ui-scrollbar.hpp` (`useScrollBars`),
`tree-widget.hpp`, `draggable-list.hpp`, `flow-layout.hpp`,
`collapsible-section.hpp`, `image-carousel.hpp`, `progress-bar.hpp`,
`svg-icon.hpp` (`svgIcon`: a few Tabler glyphs drawn in any colour),
`color-picker.hpp`, `gradient-editor.hpp`, `hotkey-field.hpp`,
`line-numbered-editor.hpp`, `breadcrumb.hpp`, `item-delegate.hpp`,
`dialogs.hpp` (`confirm`, `info`), `feedback.hpp` (the in-app feedback dialog),
`glass.hpp` (popup animation, rounded combo popup).

### Icons

`icons/` holds the Tabler outline icons (MIT) the StreamUP toolbar and dock
use, each as `<name>-dark.svg` (light ink, for dark themes) and
`<name>-light.svg`, compiled into the plugin under `:/streamup-ui/icons/`.
`tools/make-icons.py` regenerates them and centres every glyph on its own
bounding box; `tools/check-icon-centring.py` rasterises them offline and
reports any glyph that lands off centre.

### Text

The kit looks its few strings up with `obs_module_text()` under `Kit.*` keys
(`Kit.SwitchedOff`, `Kit.SwitchedOff.Open`, `Kit.MoreOptions`,
`Kit.Preview.Soon`, `Kit.Preview.Zoom`, `Kit.Lightbox.Close`,
`Kit.ButtonPreview.NoCaption`, `Kit.ButtonPreview.Tooltip`), so a plugin can
word them in its own locale. A missing key falls back to English built into the
kit.

## Using it in a plugin

The library is a git submodule at `shared/obs-streamup-ui`, pinned to a tag.

```sh
git submodule add https://github.com/StreamUPTips/obs-streamup-ui.git shared/obs-streamup-ui
git -C shared/obs-streamup-ui checkout v1.0.0
git add .gitmodules shared/obs-streamup-ui
```

In the plugin's `CMakeLists.txt`:

```cmake
include("${CMAKE_CURRENT_SOURCE_DIR}/shared/obs-streamup-ui/cmake/streamup-ui.cmake")
streamup_ui_add(${PROJECT_NAME})
```

The target must link `libobs` and `obs-frontend-api` and have `AUTOMOC` and
`AUTORCC` on (the OBS plugin template does both).

Cloning a plugin: `git clone --recurse-submodules ...`, or in an existing
clone `git submodule update --init --recursive`.

### Building before the GitHub repo exists (local override)

Until `StreamUPTips/obs-streamup-ui` is on GitHub, each plugin clone points the
GitHub URL at the local repo:

```sh
git config url."C:/obs-dev-build/obs-streamup-ui".insteadOf https://github.com/StreamUPTips/obs-streamup-ui.git
git config protocol.file.allow always
```

Remove both once the GitHub repo is live:

```sh
git config --unset url."C:/obs-dev-build/obs-streamup-ui".insteadOf
git config --unset protocol.file.allow
```

### GitHub Actions

This repo is public, so no token is needed, even in a private plugin's
workflow. Every job that builds the plugin must check out submodules:

```yaml
- uses: actions/checkout@v4
  with:
    submodules: recursive
    fetch-depth: 0
```

The StreamUP plugin template's `build-project.yaml` already does this in the
macOS, Ubuntu and Windows build jobs. The first job (event data) and
`check-format.yaml` do not need the submodule. If the repo were ever made
private, add `token: ${{ secrets.STREAMUP_UI_TOKEN }}` (a read-only fine-grained
token with access to obs-streamup-ui) to those checkout steps.

## How to update the UI in every plugin

1. Change the library here (or in any plugin's `shared/obs-streamup-ui`, which
   is a checkout of this repo: commit there, then push from it).
2. Add a line to `CHANGELOG.md`, bump `STREAMUP_UI_VERSION` in
   `cmake/streamup-ui.cmake`, commit, and tag:
   `git tag -a v1.1.0 -m "v1.1.0" && git push origin main v1.1.0`.
   Patch for fixes, minor for new things, major when an API changes.
3. In each plugin:
   ```sh
   git -C shared/obs-streamup-ui fetch --tags
   git -C shared/obs-streamup-ui checkout v1.1.0
   git add shared/obs-streamup-ui
   git commit -m "obs-streamup-ui v1.1.0"
   ```
   Build, check the windows, push. Plugins can move at their own pace: each one
   is pinned to the tag it was built and checked against.

## Versioning

Semantic versioning on the C++ API: removing or changing a public function,
class or token is a major version; adding one is minor; a fix or a look change
that keeps the API is a patch.
