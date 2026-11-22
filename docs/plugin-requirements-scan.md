# Plugin requirements: product scan

Read-only scan of `D:\GitHub\streamup\StreamUP-Products\products` on 22 Nov 2026, for the
per-product requirement check (`core/product-requirements.cpp`). Every `.StreamUP` file under a product
was walked for source, filter, group and transition records (anything with a type `id`), and each id was
run through the table in `product-requirements.cpp`.

## Summary

- 95 product folders, 33 with `.StreamUP` files. The other 62 are OBS plugins, shaders, Streamer.bot-only tools, stubs, or member
  products whose OBS files are not in the repo, so there is nothing for the check to read.
- Unmapped ids after this work: none.
- Ids identified and added to the table during the scan: `ouput_source` (Downstream Keyer, the typo is the
  plugin's own), `obs_ghostscript` (PDF Source, Ghostscript; not in the StreamUP plugin list, so its link is the
  project page), `pixel_art_plugin`, `phandasm_waveform_source`, `dynamic_delay_filter`, `obs_retro_effects_filter`.
  All but `obs_ghostscript` were confirmed against the plugin's registered types in a dev OBS.
- Hidden dependencies (needed, but no type id in the file):
  - Logo Card needs Scene as Transition, which the product creates through its `use_as_transition` action.
    The check adds `scene_as_transition` for any manifest with that action, so this is covered.
  - No other product declares a plugin dependency besides the StreamUP plugin itself (checked against each
    product's `dev/dependencies` folder and README). Fonts are listed there too; they are handled by the existing
    missing-fonts warning, not by this check.
- Plugins from the old global "required" list that no scanned product uses at all: 3D Effect.
  The old check made every user install these before any product would install.
- OBS built-in types (text, colour, image, media, browser, scene, group, the stock transitions and filters) never
  block: on another platform they are OBS's to report, and blocking would stop every Mac install over GDI+ text.

## Per product

| Product | Plugins the check will ask for | Unmapped ids | Type ids in the file |
| --- | --- | --- | --- |
| alerts/final-fantasy-vii-alerts-sup001 | Gradient Source, Move, OBS-Shaderfilter | none | `color_source`, `ffmpeg_source`, `gradient_source`, `group`, `image_source`, `move_action_filter`, `move_source_filter`, `move_transition`, `move_value_filter`, `scene`, `shader_filter`, `text_gdiplus` |
| alerts/final-fantasy-viii-alerts-sup002 | Gradient Source, Move, OBS-Shaderfilter | none | `color_source`, `ffmpeg_source`, `gradient_source`, `group`, `image_source`, `move_action_filter`, `move_source_filter`, `move_transition`, `move_value_filter`, `scene`, `shader_filter`, `text_gdiplus` |
| alerts/minim-alerts-sup003 | Move | none | `color_filter`, `color_source`, `ffmpeg_source`, `group`, `image_source`, `move_action_filter`, `move_source_filter`, `move_value_filter`, `scene`, `text_gdiplus`, `wipe_transition` |
| alerts/sonic-3-stage-card-alerts-sup059 | Composite Blur, Stroke Glow Shadow | none | `color_filter`, `color_source`, `group`, `obs_composite_blur`, `obs_shadow_filter`, `scene`, `slide_transition`, `text_gdiplus` |
| alerts/spin-pop-alerts-sup049 | Advanced Masks, Move, OBS-Shaderfilter, Stroke Glow Shadow | none | `advanced_masks_filter`, `browser_source`, `color_source`, `fade_transition`, `group`, `move_source_filter`, `obs_shadow_filter`, `scene`, `shader_filter`, `swipe_transition`, `text_gdiplus` |
| alerts/xbox-one-achievement-alerts-sup004 | Advanced Masks, Move, Source Clone | none | `advanced_masks_filter`, `browser_source`, `color_filter`, `color_source`, `fade_transition`, `ffmpeg_source`, `group`, `move_action_filter`, `move_source_filter`, `move_transition`, `move_value_filter`, `scene`, `scroll_filter`, `source-clone`, `swipe_transition`, `text_gdiplus` |
| effects/codec-moments-sup023 | Advanced Masks, Composite Blur, Freeze Filter, Move, OBS-Shaderfilter, Pixel Art, Retro Effects, Source Clone, Stroke Glow Shadow, Waveform | none | `advanced_masks_filter`, `browser_source`, `color_filter`, `color_key_filter`, `color_source`, `crop_filter`, `freeze_filter`, `group`, `image_source`, `move_action_filter`, `move_source_filter`, `move_value_filter`, `obs_composite_blur`, `obs_glow_filter`, `obs_retro_effects_filter`, `obs_shadow_filter`, `phandasm_waveform_source`, `pixel_art_plugin`, `scene`, `shader_filter`, `source-clone`, `text_gdiplus`, `wipe_transition` |
| effects/gamelad-sup022 | Composite Blur, Freeze Filter, Move, OBS-Shaderfilter, Source Clone | none | `color_filter`, `color_source`, `fade_transition`, `freeze_filter`, `group`, `image_source`, `move_action_filter`, `move_source_filter`, `move_value_filter`, `obs_composite_blur`, `scene`, `shader_filter`, `source-clone` |
| effects/gta-wasted-effect-sup025 | Composite Blur, Dynamic Delay, Move, OBS-Shaderfilter, Source Clone | none | `color_filter`, `dynamic_delay_filter`, `ffmpeg_source`, `image_source`, `move_action_filter`, `move_source_filter`, `move_value_filter`, `obs_composite_blur`, `scene`, `shader_filter`, `source-clone`, `text_gdiplus` |
| effects/polaroid-picture-sup020 | Freeze Filter, Move, OBS-Shaderfilter, Source Clone | none | `color_filter`, `color_source`, `crop_filter`, `ffmpeg_source`, `freeze_filter`, `group`, `image_source`, `move_action_filter`, `move_source_filter`, `move_value_filter`, `scene`, `shader_filter`, `source-clone`, `text_gdiplus` |
| effects/twitch-disconnect-sup019 | Freeze Filter, Source Clone | none | `color_filter`, `ffmpeg_source`, `freeze_filter`, `scene`, `source-clone` |
| games/pokemon-and-friends-sup028 | Move | none | `browser_source`, `fade_transition`, `move_source_filter`, `scene` |
| tools/dsk-blur-sup006 | Composite Blur, Downstream Keyer | none | `obs_composite_blur`, `ouput_source`, `scene` |
| tools/lego-instruction-manual-sup061 | PDF Source (Ghostscript) | none | `obs_ghostscript`, `scene` |
| transitions/logo-card-sup076 | Move, StreamUP Scene as Transition (from the use_as_transition action) | none | `color_filter`, `color_source`, `image_source`, `move_source_filter`, `move_value_filter`, `scene`, `text_ft2_source` |
| transitions/pixelation-transition-sup026 | Composite Blur, Move, Source Clone | none | `move_action_filter`, `move_value_filter`, `obs_composite_blur`, `scene`, `source-clone` |
| transitions/sonic-2-zone-card-sup021 | Move, OBS-Shaderfilter | none | `color_filter`, `color_source`, `group`, `image_source`, `move_source_filter`, `move_transition`, `scene`, `shader_filter`, `text_gdiplus` |
| widgets/cards-against-humanity-sup034 | Move, OBS-Shaderfilter | none | `color_source`, `group`, `image_source`, `move_source_filter`, `scene`, `shader_filter`, `slide_transition`, `swipe_transition`, `text_gdiplus` |
| widgets/digital-ticker-sup042 | Move, OBS-Shaderfilter, Stroke Glow Shadow | none | `color_source`, `fade_transition`, `group`, `move_value_filter`, `obs_shadow_filter`, `scene`, `shader_filter`, `slide_transition`, `text_gdiplus` |
| widgets/ffvii-highlight-message-sup009 | Gradient Source, Move, OBS-Shaderfilter | none | `cut_transition`, `gradient_source`, `move_transition`, `move_value_filter`, `scene`, `shader_filter`, `text_gdiplus` |
| widgets/ffviii-highlight-message-sup010 | Gradient Source, Move, OBS-Shaderfilter | none | `gradient_source`, `move_transition`, `move_value_filter`, `scene`, `shader_filter`, `text_gdiplus` |
| widgets/ios-26-notification-sup066 | Advanced Masks, Composite Blur, Downstream Keyer, Move, OBS-Shaderfilter, Source Clone | none | `advanced_masks_filter`, `browser_source`, `color_filter`, `color_source`, `group`, `move_source_filter`, `move_value_filter`, `obs_composite_blur`, `ouput_source`, `scene`, `shader_filter`, `source-clone`, `text_gdiplus` |
| widgets/ios-notification-classic-sup008 | Move, OBS-Shaderfilter | none | `color_filter`, `color_source`, `ffmpeg_source`, `group`, `image_source`, `move_source_filter`, `scene`, `shader_filter`, `text_gdiplus` |
| widgets/ios-notification-sup007 | Advanced Masks, Move, OBS-Shaderfilter | none | `advanced_masks_filter`, `browser_source`, `color_filter`, `color_source`, `ffmpeg_source`, `group`, `move_source_filter`, `scene`, `shader_filter`, `text_gdiplus` |
| widgets/minim-ad-tracker-sup040 | Advanced Masks, Move | none | `advanced_masks_filter`, `color_source`, `crop_filter`, `fade_transition`, `group`, `move_source_filter`, `move_value_filter`, `scene`, `swipe_transition`, `text_gdiplus` |
| widgets/minim-first-words-message-sup005 | Move | none | `browser_source`, `move_action_filter`, `move_source_filter`, `scene`, `slide_transition`, `text_gdiplus` |
| widgets/minim-goal-bar-sup014 | Move, OBS-Shaderfilter | none | `color_source`, `crop_filter`, `fade_transition`, `move_action_filter`, `move_source_filter`, `move_value_filter`, `scene`, `shader_filter`, `text_gdiplus` |
| widgets/minim-hype-train-sup024 | Advanced Masks, Composite Blur, Move, OBS-Shaderfilter | none | `advanced_masks_filter`, `color_filter`, `color_source`, `crop_filter`, `fade_transition`, `group`, `move_source_filter`, `move_transition`, `move_value_filter`, `obs_composite_blur`, `scene`, `shader_filter`, `slide_transition`, `text_gdiplus` |
| widgets/minim-poll-sup015 | Advanced Masks, Move | none | `advanced_masks_filter`, `color_source`, `fade_transition`, `group`, `move_source_filter`, `move_value_filter`, `scene`, `scroll_filter`, `slide_transition`, `text_gdiplus` |
| widgets/minim-prediction-sup016 | Advanced Masks, Move | none | `advanced_masks_filter`, `color_filter`, `color_source`, `fade_transition`, `group`, `move_action_filter`, `move_source_filter`, `move_value_filter`, `scene`, `scroll_filter`, `swipe_transition`, `text_gdiplus` |
| widgets/minim-raid-out-sup017 | Move, OBS-Shaderfilter | none | `browser_source`, `color_source`, `crop_filter`, `fade_transition`, `group`, `move_action_filter`, `move_source_filter`, `move_transition`, `move_value_filter`, `scene`, `shader_filter`, `slide_transition`, `swipe_transition`, `text_gdiplus` |
| widgets/minim-stream-goals-sup018 | Move | none | `color_filter`, `color_source`, `crop_filter`, `fade_transition`, `group`, `image_source`, `mask_filter`, `move_action_filter`, `move_source_filter`, `move_value_filter`, `scene`, `text_gdiplus` |
| widgets/on-screen-message-1-sup013 | Move, OBS-Shaderfilter | none | `browser_source`, `color_source`, `fade_transition`, `group`, `move_source_filter`, `move_transition`, `scene`, `shader_filter`, `slide_transition`, `text_gdiplus` |

## How the table was checked

The third-party plugins from the everyday OBS install were loaded into a dev build, and obs-websocket
`GetInputKindList`, `GetSourceFilterKindList` and `GetTransitionKindList` gave the exact registered ids. Each id
was tied to its plugin by finding the id string in that plugin's DLL. Prefix rules (`move_*`, `obs_stroke_*`,
`obs_glow_*`, `obs_shadow_*`, `obs_noise_*`, `advanced_masks_filter*`, `obs_composite_blur*`, `streamfx-*`)
cover versioned and future types from the same plugin.
