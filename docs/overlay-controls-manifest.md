# Overlay Controls manifest: product additions

A `.StreamUP` file can carry a `streamup_overlay` block that gives the product
its own control panel (see `core/overlay-controls.hpp` for the full format:
controls, bindings, outputs, layout parts, presets). This page covers what
StreamUP 2.5.0 adds for product authors:

- an `image` control,
- bindings that drive a scene transition's settings,
- `actions`, starting with `use_as_transition`,
- a `product` object that stamps every installed scene,
- a `requires` list for plugins the product needs that the file cannot show,
- My Products, which lists them.

All of it is optional. A manifest without these keys reads exactly as before.

## Compatibility

- Keep `"format": 2`. None of the additions needs a newer format number,
  because older plugins skip what they do not know:
  - an unknown control `type` is skipped, with a warning in the OBS log,
  - a binding with no `source` (a transition binding) is skipped the same way,
  - unknown top-level keys (`actions`, `product`) are ignored.
- From 2.5.0, a manifest with a higher `format` than the plugin knows is no
  longer refused. It loads with a warning, and anything the plugin does not
  understand is skipped. Format 1 and 2 files read as before.
- Unknown action types are skipped with a warning. A manifest whose every
  control and action was skipped still fails ("no usable controls").

## `image` control

A file path, picked with a file browser that only shows images (png, jpg,
jpeg, gif, bmp, tga, webp, psd, jxr), with a small preview. Bind it to an
`image_source`'s `file` setting.

```json
{
  "id": "logo",
  "type": "image",
  "label": "Your logo",
  "help": "A square PNG with a transparent background works best.",
  "bindings": [{ "source": "Logo Card • Logo", "key": "file" }]
}
```

- `default` is optional. Without one, the control shows the file the product
  installed (read from its first binding), and Reset puts that file back.
- The value is a path string. Backslashes are stored as forward slashes.
- Over WebSocket, `SetOverlayControl` takes the path as `value`.

## Transition bindings

A binding can name a scene transition instead of a source. It writes that
transition's own settings, so a slider can set a transition product's timing.

```json
{ "transition": "Logo Card", "key": "duration" }
```

- `transition` is the transition's name in OBS's Transitions dock. Use the
  same name as the `use_as_transition` action's `name`.
- `key` is any settings key of the transition. For Scene as Transition
  (`scene_as_transition`): `duration` (ms), `transition_point` (percent, used
  while `tp_type` is 0), `transition_point_ms`, `audio_volume`.
- `filter`, `scene`, `@enabled` and `@visible` are not allowed on a transition
  binding.
- `mode`, `map`, `clamp`, `integer` and the other binding options work as on
  any other binding.
- Until the transition exists, the binding waits quietly. It is not counted as
  a missing link. When the user runs `use_as_transition`, any value they have
  already set in the panel is applied to the new transition.

## `actions`

A list of one-click jobs, shown as buttons in the product's panel. If the
product has actions but no controls, the panel opens on a **Setup** page
holding them.

```json
"actions": [
  {
    "type": "use_as_transition",
    "scene": "StreamUP • Logo Card",
    "name": "Logo Card",
    "duration": 2000,
    "transition_point": 50,
    "make_current": true,
    "label": "Use as my transition",
    "help": "Optional text under the title."
  }
]
```

### `use_as_transition`

Creates a Scene as Transition in OBS's transition list that plays `scene`, or
updates the one already there.

| Key | Default | Meaning |
| --- | --- | --- |
| `scene` | first entry of `scenes` | The product scene the transition plays. Must be a main canvas scene. |
| `name` | the manifest `name` | The transition's name in OBS. |
| `duration` | 2000 | Milliseconds, 50 to 20000. Only used when the transition is created. |
| `transition_point` | 50 | Percent where the scenes swap. Only used when the transition is created. |
| `make_current` | true | Pick the transition in the Transitions dock afterwards. With false, the transition the user had picked stays picked. |
| `id` | `use_as_transition-<index>` | For `RunOverlayAction`. |
| `label`, `help` | built in | Button row title and help text. |

- When the transition already exists, only its `scene` is set again. Timings
  the user changed since are kept. The button then reads **Update My
  Transition**.
- The row shows whether the transition is in the list and whether it is the
  picked one, and keeps up if the user renames, removes or picks another one.
- Needs the StreamUP Scene as Transition plugin. Without it the user gets a
  message saying so, and nothing changes.
- A transition of another kind with the same name is never touched. The user
  is asked to rename it.
- How it works: OBS has no API to add a transition, so StreamUP drives OBS's
  own Add Transition path at runtime, answers its name prompt and closes the
  properties window it opens. OBS itself is not changed.

## `product`

Says which product this is. Every scene the `.StreamUP` file installs gets
it in its private settings, which travel inside the scene collection:

```json
"product": { "id": "logo-card-sup076", "name": "Logo Card", "version": "1.0.0" }
```

Two optional keys next to it (top level of the manifest, or inside `product`):

```json
"icon": "Resources/Icons/Logo Card - Icon.png",
"category": "transitions"
```

- `icon` is either an image embedded in the file or a path:
  - **Embedded** (what the packager writes, and the preferred form):
    `"icon": "data:image/png;base64,iVBORw0..."`. PNG, JPEG (`image/jpeg`) or
    WebP (`image/webp`). PNG is the safe choice: every OBS platform can read
    it. Anything over 256 KB decoded, anything that is not one of those three,
    or anything that does not decode as an image is skipped with a line in the
    OBS log, and the product installs as normal with its initials instead.
  - **Path**: relative to the `.StreamUP` file. Note the packager only ships
    `Resources/Image Sources`, `Media Sources` and `Shader Filters`, so a path
    elsewhere never reaches customers; the packager replaces it with the
    embedded form anyway.
  - Icons are **16:9**: 320x180 is right, 640x360 for extra sharpness. My
    Products shows them at 64x36 (more on HiDPI screens) with rounded corners.
    An image of another shape, such as an old square icon, is centre-cropped
    to 16:9, never stretched. At install, when controls are attached, and on
    `RegisterOverlay`, the icon is written to the plugin's config folder
    (`product-icons/`), so it survives the user deleting or moving the
    download.
  - The StreamUP-Products packager embeds an icon in every `.StreamUP` it puts
    in a ZIP (320x180, from `dev/marketing/icon.png`, or made from the cover with
    a warning), and gives a product with no manifest a minimal identity block:

    ```json
    "streamup_overlay": {
      "format": 2, "id": "minim-alerts-sup003", "name": "Minim Alerts",
      "version": "3.0.3", "category": "alerts",
      "product": { "id": "minim-alerts-sup003", "name": "Minim Alerts", "version": "3.0.3" },
      "icon": "data:image/png;base64,..."
    }
    ```

    A block with no `groups` and no `actions` registers no controls and logs
    nothing in 2.5; 2.4.1 logs "no usable controls" and installs as normal;
    2.4.0 and older ignore the key.
- `category` groups the product in My Products: `overlays`, `alerts`,
  `transitions`, `effects`, `widgets`, `games` or `tools`. Without it the
  category is read from the file name (`StreamUP Alerts - ...`,
  `StreamUP FX - ...`), then from the product (a transition action means
  Transitions, controls mean Overlays), else Other.

Stored on each scene as:

```json
"streamup_product": {
  "id": "logo-card-sup076", "name": "Logo Card", "version": "1.0.0",
  "overlay": "logo-card", "category": "transitions", "installed": "2026-11-21T21:37:28Z"
}
```

- Any key left out falls back to the manifest's own `id`, `name` and
  `version`. A manifest with no `product` object still stamps its scenes.
- A file with no `streamup_overlay` block is stamped too, with a name and
  category from its file name ("StreamUP FX - GTA Wasted" is GTA Wasted in
  Effects), so older products show in My Products once installed again.
- Only scenes listed in the file's `sources` (type `scene`) are stamped, on
  any canvas, including Aitum Vertical.
- Reinstalling restamps the scenes with the new version.

## Plugin requirements and `requires`

Install a Product picks the file first, then checks only what that file uses.
Every source, filter, group and transition in the file names its type; a type
OBS has not registered means its plugin is missing. A built-in table in
`core/product-requirements.cpp` turns type ids into plugin names and links
(taken from the StreamUP plugin list where it knows the plugin). A type the
table does not know still blocks, shown as "Unknown plugin (type ...)" with a
link to the StreamUP Pluginstaller. OBS's own types never block. Plugins the
file uses that are older than the StreamUP plugin list wants show as updates.
Holding Shift skips every check. `docs/plugin-requirements-scan.md` has the
scan of every existing product behind the table.

A product with a `use_as_transition` action needs Scene as Transition even
though no transition is in the file, so the check adds it automatically.

For anything else that adds no source types (a dock, a frontend tool) or an
exact minimum version, list it in `requires`:

```json
"requires": [
  {
    "name": "Advanced Scene Switcher",
    "url": "https://obsproject.com/forum/resources/advanced-scene-switcher.395/",
    "min_version": "1.30.0",
    "search_string": "[adv-ss] version:"
  }
]
```

| Key | Meaning |
| --- | --- |
| `name` | Shown in the window. When it matches a plugin in the StreamUP plugin list, that entry fills in anything left out. |
| `url` | Where to get it. |
| `min_version` | Older than this shows as an update. Defaults to the plugin list's version. |
| `search_string` | The text the plugin writes to the OBS log just before its version, which is how StreamUP finds the installed version. Defaults to the plugin list's. Without one from either place the entry cannot be checked and is skipped with a log warning. |

An older StreamUP plugin ignores `requires`.

## Folding rare controls

Since 3.0 a control can carry `"advanced": true`. It still works the same, but
the window folds it, with any other advanced controls on that tab, under
**More options** at the bottom of the tab. Use it for the settings most people
never touch (breathing speed, line thickness, exact offsets), so a tab opens on
the handful that matter.

```json
{ "id": "breathing_speed", "label": "Breathing speed", "type": "slider", "advanced": true }
```

A product's `description` shows as a **?** at the top of the first tab, not as
a paragraph, so keep it to two short sentences.

## My Products

**StreamUP > My Products…** lists every StreamUP product in the current scene
collection. It replaced the Overlays submenu. Products come from:

1. the Overlay Controls registry (products whose controls are saved in the
   collection),
2. scenes carrying a `streamup_product` stamp, and
3. scenes installed before stamps existed, recognised by StreamUP's scene
   naming (`StreamUP Alerts • Minim`).

Products are grouped by category, each card showing a count, with a search
box that matches names, categories and scene names. Each row shows the
product's icon (or its initials on its category's colour), its version and
scene count. **Adjust** opens its controls; the **⋯** menu also has Use as My
Transition (transition products) and Remove Controls. A product whose file
shipped controls that are not saved here says so and offers **Add Controls**.
**Add From File** attaches controls from a `.StreamUP` file without
installing it.

## WebSocket

| Request | Data | Does |
| --- | --- | --- |
| `RunOverlayAction` | `overlay`, optional `action` (id; the first action when left out) | Runs an action. Returns `message` or `error`. |
| `GetMyProducts` | | Returns `products`: `id`, `name`, `version`, `overlay`, `stamped`, `transition`, `scenes`. |
| `OpenMyProducts` | | Opens the My Products window. |
| `GetOverlayControls` | `overlay` | Now also returns `actions`, each with `exists` and `current`. |

## Worked example: a transition product (Logo Card)

The Logo Card scene is a card that sweeps across with the user's logo and a
line of text, driven by Move filters. The manifest gives it a logo picker, the
text, timing sliders and the one-click transition set-up.

```json
"streamup_overlay": {
  "format": 2,
  "id": "logo-card",
  "name": "Logo Card",
  "version": "1.0.0",
  "description": "Your logo on a card that sweeps across between scenes.",
  "product": { "id": "logo-card-sup076", "name": "Logo Card", "version": "1.0.0" },
  "scenes": ["StreamUP • Logo Card"],
  "tabs": [
    { "id": "look", "label": "Look" },
    { "id": "timing", "label": "Timing" }
  ],
  "groups": [
    {
      "label": "Card",
      "tab": "look",
      "controls": [
        {
          "id": "logo",
          "type": "image",
          "label": "Your logo",
          "help": "A square PNG with a transparent background works best.",
          "bindings": [{ "source": "Logo Card • Logo", "key": "file" }]
        },
        {
          "id": "line",
          "type": "text",
          "label": "Text line",
          "default": "BE RIGHT BACK",
          "bindings": [{ "source": "Logo Card • Text", "key": "text" }]
        }
      ]
    },
    {
      "label": "Timing",
      "tab": "timing",
      "controls": [
        {
          "id": "duration",
          "type": "slider",
          "label": "Duration",
          "min": 1000, "max": 4000, "step": 100, "decimals": 0, "unit": " ms",
          "default": 2000,
          "bindings": [{ "transition": "Logo Card", "key": "duration" }]
        },
        {
          "id": "swap",
          "type": "slider",
          "label": "Swap point",
          "help": "When the old scene is swapped for the new one, while the card covers the screen.",
          "min": 20, "max": 80, "step": 1, "decimals": 0, "unit": "%",
          "default": 50,
          "bindings": [{ "transition": "Logo Card", "key": "transition_point" }]
        }
      ]
    }
  ],
  "actions": [
    {
      "type": "use_as_transition",
      "scene": "StreamUP • Logo Card",
      "name": "Logo Card",
      "duration": 2000,
      "transition_point": 50
    }
  ]
}
```

What the user sees after installing it:

1. The Installed card offers Adjust (or StreamUP > My Products > Adjust later).
2. Look tab: a **Transition** card with **Use as My Transition**, then the logo
   picker and the text line.
3. Clicking the button adds "Logo Card" to the Transitions dock at 2000 ms /
   50% and picks it. The row turns green: "In your transitions as Logo Card,
   and picked."
4. Timing tab: the sliders change the transition's duration and swap point
   live. Each row's reset puts back the value the transition had when it was
   made.

Notes for the Move filters: Scene as Transition plays the scene for the whole
`duration`, so the Move filters' own durations should add up to it. If the
duration slider is offered, either keep the filters' timing short enough for
the shortest duration, or also bind the filters' `duration` settings to the
slider with a `map` binding.
