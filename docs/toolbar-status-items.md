# Toolbar status items

What the OBS status bar shows, as items you place on the StreamUP toolbar and
order like any other item. This adds a readout to the toolbar. It does not
replace or hide OBS's own status bar.

## What you can place

| Item | Where the value comes from |
| --- | --- |
| CPU usage | `os_cpu_usage_info_query`, the same call OBS's status bar uses. One query object for the whole plugin. |
| Frame rate | `obs_get_active_fps()`. |
| Missed frames | `obs_get_lagged_frames()` against `obs_get_total_frames()`, which is rendering lag. |
| Recording time | A timestamp taken on `RECORDING_STARTED`, with `RECORDING_PAUSED` / `UNPAUSED` accumulating so a paused recording holds its clock. |
| Stream time | The same, off `STREAMING_STARTED`. |
| Stream bitrate | `obs_output_get_total_bytes` on the streaming output, differenced against the previous sample. |
| Recording bitrate | The same, on the recording output. |
| Status message | Derived. See below. |

## The messages are ours, not OBS's

OBS's status bar messages are not exposed through the frontend API. The
overloaded warning is computed inside `OBSBasicStatusBar` from skipped frame
counts, and the lifecycle messages come from output stop codes handled in the
main window. Reading that widget directly would mean depending on private UI
that can be renamed or restructured in any OBS release, and it would break
silently when it was.

So these are derived from public frontend events and public counters instead.
The wording is StreamUP's own and will not be character-identical to what OBS
shows. Encoding overload is judged over a five second window rather than a
single tick, so one bad second during a scene change does not light it up.

## Settings per item

- **Show label** turns `CPU 11.4%` into `11.4%`.
- **Always show hours** applies to the two durations, so a recording reads
  `00:04:12` from the start rather than switching from `04:12` to `01:00:00`
  when it passes an hour.

## Width and orientation

Each item is pinned to the width of the widest value it can ever show. Without
that the whole run reflows every second and the bar visibly jitters while you
stream. The durations are always measured at the hours form for the same
reason: crossing an hour must not shove everything along the bar.

On a left or right docked toolbar every item switches to a compact form, since
`60.00 FPS` does not fit a bar the width of a button. `240.00 FPS` becomes
`240`, `6000 kb/s` becomes `6000k`, and each message has its own shorter
wording rather than being elided.

## Updating

One timer for every status item on the bar, at 1Hz, running only while at least
one item exists. Items are views onto a single `Monitor` that samples each
counter once per tick.

## Not in this pass

Status items are read-only. A recording time that starts and stops recording
when clicked would be natural, but it doubles the design, so the first pass
leaves them as readouts. Threshold colouring is signalled to the theme through
an `alerting` property rather than painted here, so the theme decides what a
CPU pegged at 95% looks like.
