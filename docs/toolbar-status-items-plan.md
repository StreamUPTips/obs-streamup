# Toolbar status items: plan for its own pass

Put what the OBS status bar shows onto the StreamUP toolbar, as items you place
and order like any other toolbar item.

## What goes on it

| Item | Where the value comes from |
| --- | --- |
| CPU percentage | `os_cpu_usage_info_query`, the same call OBS's status bar uses. One shared query object for the plugin, not one per item. |
| FPS | `obs_get_active_fps()` for the target, plus dropped/skipped frame counts from `obs_get_frames_output` / `obs_get_lagged_frames` if we want the "x / y FPS" pairing OBS shows. |
| Recording time | `obs_output_get_active_delay` is not it. Use the record output's total frames over the FPS, or track a start timestamp on `OBS_FRONTEND_EVENT_RECORDING_STARTED` and format elapsed. The timestamp route is simpler and survives pause if we also handle `RECORDING_PAUSED` / `UNPAUSED`. |
| Stream time | Same shape, off `OBS_FRONTEND_EVENT_STREAMING_STARTED`. |
| Bitrate | `obs_output_get_total_bytes` sampled on an interval, differenced against the previous sample. Needs its own small history per output so the number is a rate, not a total. Stream and record bitrates are separate outputs and probably separate items. |
| Messages | "Recording stopped", "Encoding overloaded" and friends. See below, this is the awkward one. |

## The messages are the hard part

OBS's status bar messages are not exposed through the frontend API. The
overloaded warning in particular is computed inside `OBSBasicStatusBar` from
skipped frame counts, and the "recording stopped" style messages come from
output stop codes handled in the main window.

Two honest options:

1. **Derive them ourselves.** Watch `OBS_FRONTEND_EVENT_RECORDING_STOPPED` and
   friends for the lifecycle messages, and compute the encoding overload
   warning the same way OBS does, from the ratio of skipped to total frames over
   a window. This means our wording and thresholds are ours, and may drift from
   what OBS shows. Acceptable if we word them as our own.
2. **Read the OBS status bar widget directly.** Find `OBSBasicStatusBar` under
   the main window and mirror its labels. Fragile: it is private UI that can be
   renamed or restructured in any OBS release, and it would break silently.

Recommend option 1. Deriving from public events and frame counters is stable
across OBS versions, and the plugin controls the wording. Say so in the docs so
nobody expects character-identical text to OBS.

## Shape in the toolbar

A new `ItemType::StatusItem` in `streamup-toolbar-config.hpp`, with a sub-type
enum for which readout it is. That keeps it in the existing item system, so it
places, reorders and deletes in the editor for free, and serialises with
everything else.

Remember: the load-time range check in `streamup-toolbar-config.cpp` is bounded
by `kLastItemType`. Adding a type without moving that constant makes every
status item vanish on the next reload, which is exactly the bug the WebSocket
buttons hit.

Per-item settings worth having:

- Label on/off, so you can have "CPU: 11%" or just "11%"
- For times, whether to show hours when under an hour
- For bitrate, which output (stream or record)

## Updating

One timer for the whole toolbar, not one per item. OBS's status bar runs at 1Hz
and that is plenty for all of these. Everything already goes quiet in edit mode
through the `editModeActive` guards, and status items must respect that too or
they will fight the editor for the layout.

Text items change width as their value changes, which will make the whole run
reflow every second if the labels are not width-stable. Use a fixed width per
item computed from the widest plausible value, or the bar will visibly jitter
while streaming. This is the detail most likely to make it feel cheap.

## Open questions before starting

1. Does a status item need to be clickable? A recording time that starts and
   stops recording when clicked would be natural, but it doubles the design.
   Suggest read-only for the first pass.
2. Colour on threshold. OBS turns things red when they matter. Worth having, but
   it needs theme hooks rather than hardcoded colours, matching how the rest of
   the toolbar leaves colour to the theme.
3. What happens on a vertical toolbar. "60.00 / 60.00 FPS" is wide, and a
   side-docked bar is narrow. Either these items are horizontal-only, or they
   need a compact form. Needs deciding before the widgets are written, not after.

## Not in this pass

Anything that changes what the OBS status bar itself does. This adds a readout
to the toolbar, it does not replace or hide OBS's own bar.
