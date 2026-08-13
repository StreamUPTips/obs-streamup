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

## Icons, not words

Each readout is an icon and a value, the way the OBS status bar reads. Writing
the name out in front of every number (`CPU 11.4% FPS 60.00 REC 00:00`) filled
the bar with words and left no room for the numbers.

The icon is the only thing naming the readout, so every item carries a tooltip
with its full name.

## Settings per item

- **Show icon** drops the icon and leaves the bare value, for anyone who knows
  what their own bar says.
- **Always show hours** applies to the two durations, so a recording reads
  `00:04:12` from the start rather than switching from `04:12` to `01:00:00`
  when it passes an hour.

## Width and orientation

Each item's value is pinned to the width of a realistic reading, not the worst
case, and the pin only ever grows. Without a pin at all the whole run reflows
every second and the bar visibly jitters while you stream. Pinned to the worst
case instead, every item carried a hole that nobody's numbers ever filled: room
for 88888 dropped frames at 100%, or a five figure bitrate.

So a value that outgrows its slot widens it, once, and it stays widened. For a
clock that happens exactly once, when it passes an hour and `59:59` becomes
`01:00:00`. Turning on **Always show hours** pins it at the hours width from the
start, for anyone who would rather it never moved at all.

Along the bar, the value sits hard against its icon and any slack falls to the
right, so the number stays with the icon that names it. Centring it in the
reserved slot pushed the two apart and left a gap on both sides.

The pin is measured from the value label's own font once the widget has been
polished, with padding either side. Measuring the parent's font, or measuring
to the pixel, clips the edge of every readout the moment the theme applies a
margin or a heavier weight.

On a left or right docked toolbar each item stacks, icon above value, and
switches to a compact form. A side-docked bar is only as wide as a button, so a
row of icon-then-value would either force the bar wide or clip. `240.00` becomes
`240`, `6000 kb/s` becomes `6000k`, and each message has its own shorter wording
rather than being elided.

## Running out of room

A readout that grows has to take the space from somewhere. If the bar is full,
and particularly if a wide spacer has pushed a group of readouts to the far end,
the growth used to come out of the right-hand edge, which meant the last item
was clipped.

Spacers give ground instead. A spacer asks for its configured length and keeps
it while there is room, and shrinks toward zero when there is not, so a growing
value, a narrowed window or a bigger font eats into the empty gap rather than
into the items. A group pushed to the right by a spacer therefore grows
leftwards. The gap is the only thing on the bar that can afford to lose space,
which is what makes it the right thing to take it from.

Nothing else in the run can shrink: buttons hold their size hint, and a
readout's value is pinned. So the order is always spacers first, then nothing.

## In the editor

The editor builds the real readout rather than a button with the item's name on
it. Every other toolbar item is a button, so the editor's inert lookalike is a
button too, and a status item rendered that way came out as a row of elided
names: `Fr...te` where a frame rate was going to be. You cannot arrange a bar
you cannot read.

The preview is inert like everything else in the editor, and the message item
shows a sample rather than hiding itself, since an invisible item cannot be
selected, moved or removed.

## The message item takes no room when it is quiet

Every other readout always has a value. The message is empty most of the time,
and an icon sitting over nothing is dead space on a bar where space is the whole
problem, so the item takes its slot when there is something to say and gives it
back afterwards.

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
