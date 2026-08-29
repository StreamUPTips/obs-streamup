# StreamUP v2.4.0 - Feature Update

## New Features
- **Tabs In The Scene Organiser** - Favourites, Recent, and as many of your own as you want, along the bottom of the dock. Each one is a proper tree with its own folders, arranged however you like, so the same scene can sit in a different folder on every tab. Right click the tab bar to make a tab, rename it, delete it or hide the built in ones, and drag the tabs into whatever order suits you. Favourites fills from the right click menu on a scene, Recent fills itself from whatever you go live with, and the rest is yours to fill from the add button. All of it is saved per scene collection.
- **Undo And Redo** - Ctrl+Z in the Scene Organiser now covers moving scenes and folders, adding a folder, renaming one, deleting one, and colour changes. Deleting a scene is the one thing it will not bring back, because that removes the scene from OBS itself and no amount of putting our tree back changes that.
- **Multi Select** - Ctrl click and shift click to pick up a run of scenes and drag the lot in one go, instead of one at a time.
- **Custom Icons On Scenes And Folders** - Right click, Set Icon, and take any of the 16 icons your OBS theme provides or an image off your drive. Theme icons stay themed, so they follow when you change theme. There is an Icon Colour submenu with the same 8 swatches the row colours use, and it tints whichever icon is in use including the default one, so you can colour code a whole collection without picking a single custom icon.
- **Folders Open And Close** - A folder shows an open or a closed folder icon depending on whether it is open, so you can tell at a glance without reading the arrow. A folder you have given an icon of your own keeps that icon in both states, since picking one on purpose and then having it change on you would be daft.
- **Folder Guide Lines** - Lines down the tree so you can see which folder a scene belongs to, with a switch for them in Settings, Scene Organiser.
- **Search On Every Tab** - The search box works on all of them now, not just the main one, and Enter takes the first scene still showing straight to program. There is a hotkey for jumping to the search box too, in Settings, Hotkeys.
- **A Real Toolbar For The Vertical Canvas** - On the StreamUP theme, the Vertical Canvas control row is a toolbar like every other dock's. Aitum put those buttons in a plain row with nothing behind them, so a theme had nothing to paint. StreamUP wraps the row so the theme can reach it, and adds dividers so the outputs, the clip controls and settings read as groups rather than one long line of icons. Nothing is moved or replaced, and on any other theme the dock is left exactly as Aitum built it.
- **The Backtrack Switch Is Back** - The backtrack toggle in the Vertical Canvas dock is one of the StreamUP theme's switches now. It was there all along, it just had nowhere to be drawn: the button it lives in is 32px wide and the toggle is wider than that, so it was cut away to nothing and looked like a missing icon. The button gives way instead.
- **Rounded MultiDock** - On the StreamUP theme, the MultiDock body has rounded corners, so the panel hugs the docks inside it instead of boxing them in.
- **Virtual Camera Button Goes Green** - The virtual camera button changes its icon while the camera is running, the way the record and stream buttons do, so a glance at the bar tells you rather than having to spot the dot on the taskbar.
- **Linked Scenes In The Vertical Scene Organiser** - Right click a vertical scene and tick the main scenes that should bring it up with them, the same as Aitum's own vertical scene list. When one of them goes live the vertical canvas follows. The link is stored where Aitum stores it, so one set here is ticked in their dock and the other way round.

## Bug Fixes
- **The Live Scene Was Almost Invisible** - On some themes the current scene in the Scene Organiser came out near enough black, and it was the one row that would not light up when you moved the mouse over it. Rows the dock does not colour itself were left to the theme, and a theme that styles the OBS scene list by name rather than styling tree views in general had nothing to say about ours. The dock paints its own highlight now, on every row, and checks the colour it lands on against the background so a theme with a dark selection colour still reads. Thanks to Mapsking for the report and the screenshots.
- **A Move Could Leave A Copy Of The Scene Behind** - Move a folder with scenes selected inside it and the same scene could turn up in the tree more than once, and stay there. A move puts the scenes in their new home and leaves it to OBS to take the originals away, which is fine for one row but not for a folder that has already rebuilt its own contents on the way past. The dock now checks the tree against its own record of which scene is which, after every change and before anything is saved, so a copy cannot survive. It tidies up a tree that already has some, so if you have seen this, one start sorts it.
- **Renaming A Scene Emptied Your Lists** - Rename a scene anywhere outside the Scene Organiser and it dropped out of your Favourites and your tabs, and a hidden scene quietly stopped being hidden. All 3 of those remember a scene by name, so a rename left them pointing at a name that matched nothing. The dock listens for renames now, wherever they happen, whether that is the scene list, a hotkey or a websocket call.
- **Dragging While Searching Put Scenes In The Wrong Place** - A drop landed against the filtered list rather than the real one, so the neighbours it appeared to land between were not its actual neighbours. Dragging is switched off while a search is active.
- **Stream And Record Timers Were Cut Off** - In the Vertical Canvas dock the timers were clipped part way through, so a recording that had been going for hours showed the first few digits and nothing else. Those 2 buttons are no longer given a width to stay inside, so they take whatever the timer needs.
- **The MultiDock Toolbar Was Too Tall** - It sat taller than every other toolbar in the dock stack. It takes its height from OBS' own toolbar now, the same as the Scene Organiser toolbar does, so the 3 of them line up.
- **The Toolbar Edit Panel Opened Off Screen** - It was anchored to the leading edge of the bar, and a bar docked along the bottom of the window is the full width of it, so below the bottom edge and in from the left was off screen on both axes and got clamped into the corner. It centres on the bar now and opens on whichever side has room, falling back to the middle of the OBS window when neither side does.
- **Plugins You Switched Off No Longer Nag You** - The startup check treated a required plugin being off as something that always had to be shouted about, so it skipped the do not remind me logic entirely, and the tickbox was only ever drawn for updates and load failures, so there was nothing to tick. The dialog now offers to stop reminding you when that is all it is reporting, and honours it. Turning the plugin back on clears it, and a manual plugin check, or installing a product, still reports it every time.
- **Some Tidying Under The Surface** - 3 of the Scene Organiser toolbar buttons were never initialised properly, which meant every check for whether they existed was reading whatever happened to be in memory. Nothing had gone wrong with it yet, and now it cannot.

---

# StreamUP v2.3.2 - Patch Update

## Bug Fixes
- **Switching Scene Collections Could Take OBS Down** - Change to another scene collection and OBS would complain that not all sources could be cleared, then hang or close on you. StreamUP watches the scene you are working in so it can tell Streamer.bot and your Stream Deck what you have selected, and it was still holding on to that scene while OBS was trying to pack the old collection away. It lets go before the switch starts now, and picks the new scene up once the new collection has loaded. Same on the way out, so closing OBS is clean too.

---

# StreamUP v2.3.1 - Patch Update

## Bug Fixes
- **A New Vertical Scene Would Not Switch** - Make a scene in the Vertical Scene Organiser, click it, and nothing happened. We were asking Aitum to do the switch and taking its reply as a job done, but that reply only says the request arrived. Aitum checks the name against a list it builds from its own add button, so a scene added any other way is a stranger to it and the switch quietly goes nowhere. We check whether the scene actually changed now, and do it ourselves when it has not. The scene was being created properly all along, and still shows up in Aitum's own dock after a restart.
- **The Green Live Marker Did Not Follow The Vertical Canvas** - Switch scenes on the vertical canvas and the Scene Organiser kept the on air marker on whatever was live when it loaded. Aitum runs its switching through its own transition, which sits on the canvas and never moves, so nothing is ever raised to say the live scene changed. The dock now checks for itself once a second, and only redraws when the answer is different.

---

# StreamUP v2.3.0 - Feature Update

## New Features
- **Spacers That Fill What Is Left** - Tick Fill available space on a spacer and it stops being a set width and starts taking whatever the bar has spare. That is how you get your buttons on the left and your stats hard against the right, and drop in a second one and you have a group in the middle as well. The toolbar alignment setting shifts the whole bar in one go and was never able to say that, so a flexible spacer quietly takes over from it wherever you use one. It is also the first thing to hand room back when the bar gets tight, so every spacer you sized by hand keeps the size you gave it.
- **Reset A Stat From The Bar** - Right click a readout and you can send it back to zero. Missed frames counts up the whole time OBS is open, so a rough morning followed you into the evening's stream and there was nothing you could do about it. Resetting does not touch OBS's own counter, it just starts counting from the moment you asked, so the number means "since I last looked". The status message can be cleared the same way instead of waiting for it to fade. The clocks and the live readings deliberately do not offer it, because a recording time you can zero is a recording time that lies to you.
- **The Status Bar, On Your Toolbar** - Everything the OBS status bar shows is now something you can put on the StreamUP toolbar: CPU, frame rate, missed frames, recording time, stream time, stream and recording bitrate, and a status message. Drag them on and order them how you like. Each one is an icon and a value, the way the status bar itself reads, with the full name on hover, and you can drop the icon if you would rather have just the number. The two clocks can show hours from the start if you would rather they did not change shape when you pass an hour. They read out, they do not click, and the whole set runs off one timer at the same 1Hz OBS uses. Each value holds the width of the widest reading it could ever show, so the bar does not shuffle along every second while you are live. Dock the toolbar down the side and every item stacks, icon above value, in a shorter form, so a side bar stays exactly as narrow as it was before you added them. The message item takes up no room at all until it has something to tell you. The messages are worked out from OBS's own events rather than read off its status bar, which is private and would break the day OBS moved it, so the wording is ours rather than a copy. None of this touches your OBS status bar. It just puts the same numbers where you actually look.
- **Toolbar Buttons That Fire WebSocket Requests** - A new button type that runs an obs-websocket request when you press it. Pick from the standard obs-websocket requests, from StreamUP's own, or name another plugin's vendor and request yourself. The common ones give you proper fields to fill in rather than making you write JSON, with your scenes, sources and transitions offered as dropdowns, and there is a raw JSON box for anything else. Anything obs-websocket can do from a Stream Deck, your toolbar can now do with one button.
- **Transition Override In The Scene Organiser** - Right click a scene and set its transition and duration, the same as you can in the OBS scene list. None clears it. It writes to the same place OBS does, so an override set in the Scene Organiser shows up ticked in the OBS scene list, and one set there shows up here. Main canvas only, because the vertical canvas runs its switches through Aitum's own transition and would ignore it.
- **See What A Request Sends Back** - Requests that return something, like GetStats or GetRecordStatus, show it in a small readout next to the button you pressed, and put the full result on your clipboard at the same time so you can paste it wherever you need it. Failures tell you why rather than disappearing into the log. Requests that just do a thing and return nothing stay out of your way.
- **Edit Your Toolbar On Your Toolbar** - The toolbar editor has been rebuilt from the ground up. Right click the toolbar and pick Edit Toolbar, and the bar itself becomes the thing you edit. Drag items straight onto it from the panel that opens alongside, drag them along the bar to reorder, and drag them off to remove. Click anything on the bar to change its settings, so a spacer is resized by dragging its edge or by typing a number, whichever you prefer. There is no separate configuration window guessing at what the result will look like, because you are working on the real toolbar the whole time. Your existing layout is carried across and you get a note the first time explaining what has changed.
- **Open Folder On Failed Plugins** - When a plugin fails to load, the check dialog gets an Open Folder button that opens the folder with the module file highlighted, so you can remove or replace it. Hover the plugin's status to see why it wouldn't load, pulled from your OBS log.
- **Custom Source Icon For Adjustment Layer** - On OBS 32.2 and newer, the Adjustment Layer shows its own icon in the Add Source list and the audio mixer instead of the generic colour icon. Older OBS versions keep the default icon.
- **Spot Switched-Off Plugins** - The plugin check now notices when a plugin is installed but switched off, either turned off in OBS's own Plugin Manager or blocked because OBS is in Safe Mode. They used to show up as missing, sending you off to download something you already had. They now get their own section telling you where to switch them back on. A required plugin being off always shows, even if you have skipped other reminders.
- **Scene Organiser For Your Vertical Canvas** - If you run Aitum's Vertical Canvas plugin, you get a second Scene Organiser for it. Same folders, colours, search and drag and drop as the main one, just pointed at your vertical scenes. The dock turns up on its own once the vertical canvas is running, and it keeps its own folder layout, so your vertical scenes are not mixed in with your main ones. Clicking a scene switches it through Aitum's own transition, the same as clicking it in their dock.
- **Back Up And Restore Your Whole OBS** - New Backup menu, and a Backup tab in StreamUP Settings. One file holds your scene collections, profiles, plugin settings, themes and OBS settings. Browser caches and logs are skipped, so a setup that fills 563MB on disk backs up to about 500KB. Backups happen on their own as OBS closes, once a day, keeping the last 10, so there is always a recent one without you remembering. Restoring shows you what is in the file first, warns about any plugins you no longer have, saves a safety copy of your current setup, then puts everything back as OBS shuts down and tells you how it went when you reopen. Your stream key is left out of manual backups by default so they are safe to share, and kept in the automatic ones since those stay on your machine. Works the same on a normal install or a portable one, and you can switch the whole thing off in Settings > Plugins if you would rather handle backups yourself.
- **Restore Just The Bit You Lost** - Restoring does not have to be all or nothing. Pick which parts come back, scene collections, profiles, plugin settings, themes or OBS settings, and within scene collections pick exactly which ones, with Select all and Select none for when there are a lot. Anything you leave unticked is never even unpacked, so it cannot be touched. Files that already match are left alone, so restoring one lost collection out of thirty writes one file.
- **Trigger A Backup From Streamer.Bot Or A Stream Deck** - Two new WebSocket requests. CreateBackup takes a backup with no setup needed, and reports how many files went in, whether the stream key was included and how many files your scenes are missing. GetBackupInfo reports your backup settings and lists the backups you already have. There is no restore request on purpose, since restoring replaces your setup and deserves a look at what is in the file first.
- **Find The Files Your Scenes Have Lost** - Backing up checks every file your scenes point at and tells you which ones are no longer on disk. Those sources are already broken in OBS, nothing has ever told you which. The list shows the path and the scene collection that wants it, you can right-click any row to copy the name or path, and you can export the lot to a text file or spreadsheet to work through later.
- **Toolbar Alignment** - New Start / Centre / End dropdown in StreamUP Settings, Toolbar tab. Centre your buttons instead of leaving them stuck to one edge. Works the same way when the toolbar is docked left or right, so Start becomes top and End becomes bottom. The StreamUP settings button stays pinned at the far end either way.
- **Read Source State Over WebSocket** - New WebSocket requests to read back whether sources are locked, either every source or just the current scene, and whether the selected source is visible. Handy for Stream Deck and Streamer.bot setups so a button can stay in sync with what is actually happening in OBS.

## Improvements
- **Resize Spacers While You Edit Them** - Toolbar spacers used to be set once on creation. If you wanted a different size you had to delete it and add a new one. Now you select a spacer and set its size, or just grab its edge on the toolbar and drag. It is drawn at its real size the whole time and the toolbar updates live, so you can dial in the gap without guessing. Cancel puts everything back.
- **Narrower MultiDocks** - The MultiDock refused to be dragged below 400px wide, which at 125% display scaling was really 500px, far too wide for a sidebar. The floor is down to 120px, and docks living inside a MultiDock no longer force their own width onto it, so a single wide dock like Twitch chat cannot hold the whole thing open. Docks pulled back out keep their original sizing.
- **Windows That Fit Their Contents** - Small StreamUP windows were being inflated to a fixed 420x360 and then spreading their contents down the middle to fill the space. That size is now what a window opens at only when it has no size of its own, and the minimum is just a collapse guard, so a dialog with one field opens as a dialog with one field. The Plugin Updates window also sizes to its list now instead of always opening 620px tall with one plugin in it.
- **Mixer Mute Button** - The mute button now shows a muted speaker glyph when a source is muted, so you can tell muted from live at a glance.
- **Resizable Windows** - StreamUP windows now resize. Grab any edge or corner and drag. The smaller windows also open a bit taller by default, so there is less squinting to read through them.
- **Group A Single Source** - Grouping used to need at least two sources selected. OBS is happy to group just one, so now StreamUP is too.

## Bug Fixes
- **Toolbar Sits Where You Put It** - The toolbar floated 9px off the edge of the window it was docked to, so it never quite looked docked. That gap came from the padding StreamUP puts around the main window to keep your docks off the corners. It is now dropped on whichever edge the toolbar is on, and comes back when you move it somewhere else.
- **Toolbar Size Actually Changes Size** - Small, Medium and Large barely looked different, because 16px of chrome sat around the buttons no matter which you picked, and the whole range only moved the bar from 35px to 45px. That chrome is down to 6px, so the bar hugs the buttons and the three sizes look like three sizes. The bar is also the same thickness wherever you dock it now.
- **Toolbar Background Matches OBS** - The bar was a lighter panel colour sat on top of the window. It now uses the same colour as the OBS window behind it.
- **Coloured Scenes Painted Once** - Give a scene a colour and it came out with a dark band round the outside and a brighter middle, like it had been printed twice. It had. The colour was being drawn up to 3 times over: the row background, the theme's selection highlight, and StreamUP's own rounded fill. The preset colours are partly see through, so every extra layer stacked up. It is drawn once now, and selecting a coloured scene keeps that scene's colour instead of blending it with the theme's blue.
- **Crash Adding A Scene-Based WebSocket Button** - Picking a request that takes a scene name, such as GetSceneItemList, could take OBS down with it. Building the scene dropdown freed the list OBS handed back the wrong way, damaging memory, so OBS either went straight down or fell over a minute later doing something unrelated with no crash report to show for it.
- **WebSocket Buttons Vanishing** - A WebSocket button disappeared the moment you pressed Done. It was being saved correctly, then thrown away by a stale check the next time the toolbar loaded its configuration.
- **Scene Hotkeys All Looked The Same** - The hotkey picker listed every scene as "Switch to scene" with nothing to tell them apart, and every source's Mute stacked up under one source. OBS registers those hotkeys on the scene or source itself, and StreamUP was going by the hotkey's name, which they all share. It now asks which scene or source a hotkey belongs to. The same fault meant a mute button could fire at the wrong source, so if you made one before this update it is worth adding again.
- **Hotkey Keys Column** - The picker said "Not bound" against every hotkey whether it was bound or not. It now shows the keys you have actually assigned.
- **Buttons With No Icon** - A toolbar button without an icon was an invisible square, which made it near impossible to find or drag while editing. It now shows its name instead.
- **Spacers On A Side Toolbar** - If you dock the toolbar to the left or right, your spacers came back collapsed to nothing every time OBS started, and the only way to get them back was to open the toolbar settings and press Save. The toolbar was building its spacers before OBS had told it which edge it was going to sit on, so they were sized for a left to right run, then rebuilt from whatever they happened to measure on screen, which was nothing at all. It now knows where it is headed before it builds, and it remembers the size you set rather than measuring it back off the screen.
- **Windows Going Blank Between Monitors** - Dragging a StreamUP window from one screen to another could leave it blank until you let go of the mouse. It now repaints as it crosses, so it stays visible the whole way.
- **Enter In Pop-Ups** - Pressing Enter in a StreamUP confirmation box used to trigger Cancel, because that is where the keyboard focus landed. Enter now does the thing you actually asked for.
- **Mixer Strip Names** - Fixed strips relabelling to the wrong source name when two sources shared the same prefix. Full names now only apply on a unique match, otherwise OBS keeps its usual shortened name.
- **Mixer Button Icons On OBS 32.2** - OBS 32.2 was flashing raw black icons on the mixer buttons. They now re-tint to the theme colour whenever the icon changes.
- **Audio Monitoring Header** - The Audio Monitoring header was clipping its text. It now holds open to fit the full label.
- **Twitch Dock Leaving The MultiDock** - If a docked panel loaded late, like the Twitch info dock, StreamUP could drop it from your MultiDock on the next restart. It now holds onto docks it has not seen yet and keeps trying to restore them, so they stop disappearing.
- **Crash When Closing OBS** - Fixed a crash that could happen as OBS shut down, caused by StreamUP letting go of a scene it was watching a little too late in the process.
- **Wizard Save & Continue Button** - The Save & Continue button was showing a double ampersand, "Save && Continue". Sorted properly this time. The button paints its own text, so it never needed the ampersand doubling from before.
- **Scene Organiser Rename On Click** - In double-click switch mode a click could accidentally start renaming a scene. Renaming is now F2 or right-click, Rename. Never a click.
- **Plugins That Never Report A Version** - Some plugins never print their version to the OBS log, so there is nothing for us to compare against. They used to just vanish from the Installed Plugins list, which made it look like you had not installed them. They now show as "Not checked", with a hover telling you why and pointing you at the plugin's own page. The download link still works, you just will not get an update prompt for those ones.

---

# StreamUP v2.2.4 - Patch Update

## Improvements
- **Fresh Look Across The Whole Plugin** - Every StreamUP window and dialog now shares one look. Rounded cards, soft shadows, matching pill buttons and dividers, consistent wherever you are in StreamUP.
- **Windows Scale With Your Text Size** - Turn up the Windows "Make text bigger" or display scaling setting and StreamUP windows now grow to match instead of staying tiny. Much easier to read on high-DPI displays. No change at 100%.
- **In-App Pop-Ups** - Confirmations, rename boxes and the colour picker are now styled in StreamUP instead of the plain OS dialogs, so they fit the rest of the UI.
- **Built-In Colour Picker** - Picking a folder or scene colour in the Scene Organiser now uses a proper StreamUP picker with a colour square, hue strip, hex field and saved swatches.
- **Clearer Buttons** - Buttons line up Cancel on the left, confirm on the right. Anything destructive, like removing a scene or resetting the toolbar, gets a red button so there are no surprises.
- **Theme Preview Carousel** - Theme preview images now slide between shots instead of snapping, with rounded corners, arrows inside the image and dot markers along the bottom.
- **Live Scene Highlight In The Scene Organiser** - The Scene Organiser now always highlights whatever scene is live on stream, so you can tell at a glance what is actually going out. In Studio Mode your selected preview scene shows in a different colour, and you can move up and down with the arrow keys then press Enter to cut to it.
- **Smaller Scene Organiser Icons** - The scene and source icons in the Scene Organiser were bigger than the ones in the normal OBS list. They match now, so the panel sits alongside OBS a bit more naturally.

## Bug Fixes
- **Patch Notes Window** - The patch notes window stopped opening after the UI rework. Fixed, it shows and jumps to the front again.
- **Transitions Find Sources Inside Groups** - The show and hide transition tools, and a few source settings, could not find a source if it lived inside a group. They look inside groups properly now.

---

# StreamUP v2.2.3 - Patch Update

## New Features
- **Toolbar Size** - New Small / Medium / Large dropdown in StreamUP Settings, Toolbar tab. Scales icon size and button padding together so buttons stay balanced at every tier.
- **Plugins Picker** - One place to turn each piece of StreamUP on or off, new tab in Settings. Soft toggles flip instantly, the rest show a "Restart required" badge. First-launch wizard runs on fresh installs and after version bumps.
- **Adjustment Layer Auto-Position** - New property, on by default. The AL slots itself just above the lowest included source so the composite paints in the right z-band, leaving un-included sources untouched.
- **Switch to New Scene on Create** - New Scene Organiser setting, off by default. Switch it on and creating a scene auto-switches OBS to it. In Studio Mode it only sets the preview, never goes to program.

## Bug Fixes
- **Adjustment Layer Hide Originals on Startup** - Originals stayed invisible after restarting OBS because the saved scene state remembered the hidden flag but our runtime tracker did not. The AL now adopts already-hidden included items on first tick.
- **Scene Organiser Item Height** - Fresh installs defaulted to 100% even though the in-code defaults and upgrader path used 50%. Lined them all up on 50%.
- **Installer App ID** - Windows installer was shipping with a blank AppId. Fixed by reading UUID_APP from the buildspec.
- **Plugin Updates Dialog** - Rows in the plugin updates table were getting clipped flat on high-DPI displays. Fixed.
- **Wizard Save & Continue Button** - Qt was eating the ampersand as a mnemonic prefix so it rendered "Save  continue". Doubled the & for a literal ampersand.

---

# StreamUP v2.2.2 - Patch Update

## Improvements
- **Toolbar Theming** - Stripped custom sizing so the toolbar now follows OBS theme standards for a more consistent look
- **Theme Switching** - Table row heights and theme styling now re-apply correctly when switching between themes without needing to restart

## Bug Fixes
- **Scene Organiser Memory Leak** - Fixed leaked source references when duplicating scenes from the Scene Organiser

---

# StreamUP v2.2.1 - Patch Update

## Bug Fixes
- **Adjustment Layer Freeze** - Fixed sources flickering on and off during scene transitions when using the Adjustment Layer with "Hide Originals" enabled. Visibility mutations are now deferred until the transition completes

---

# StreamUP v2.2.0 - Feature Update

## New Features
- **Adjustment Layer Source** - A brand new source type that lets you apply filters to everything beneath it in your scene, without having to duplicate filters across multiple sources
- **Dock Visibility Button** - The visibility button in the StreamUP dock now reflects the actual state of your selected sources, so you can see at a glance what's visible and what's not

## Improvements
- **UI Refresh** - The entire plugin has been given a fresh coat of paint to match the OBS theme palette properly. Dialogs, buttons, dropdowns, and input fields all look cleaner and feel more at home inside OBS
- **Redesigned About Window** - Condensed layout with a cleaner header so it takes up less space and is easier to read
- **Redesigned Patch Notes** - Each version now lives in its own collapsible card so you can quickly find the release you're looking for
- **Dock Config Dialog** - Reworked to use a cleaner section and card layout, making it much easier to navigate
- **Hotkey Recording** - Only one hotkey can be recorded at a time now, so you won't accidentally capture input into multiple slots
- **Multi-Dock & Hotkeys Polish** - Better buttons in the Multi-Dock interface and cleaner section dividers in the Hotkeys menu

## Bug Fixes
- **Toolbar Configurator Crash** - Fixed a crash that could happen when opening the Toolbar Configurator
- **Dialog Crashes** - Fixed several crashes caused by frameless dialogs being closed in the wrong order
- **Thread Safety & Null Checks** - Added extra safety checks across the plugin to prevent rare crashes
- **Locale Fixes** - Filled in missing translations and fixed a bullet point encoding issue in certain languages

---

# StreamUP v2.1.8 - Patch Update

## Bug Fixes
- **Scene Collection Crash Fix** - Fixed a crash that occurred when changing scene collections

---

# StreamUP v2.1.7 - Patch Update

## Improvements & Bug Fixes
- **Memory & Stability Fixes** - Fixed several memory leaks throughout the plugin, including when loading StreamUP products and using WebSocket bitrate requests. This should noticeably reduce memory usage over long sessions
- **Multi-Dock Improvements** - Improved dock restoration reliability and fixed issues where docks could lose their layout after restarting OBS
- **Show/Hide Transitions** - Fixed an issue where setting show/hide transitions via WebSocket could fail on certain error paths
- **Theme Enhancements** - Theme changes are now detected automatically without needing to restart OBS
- **Thread Safety** - Improved plugin state handling to prevent potential issues when multiple parts of the plugin access shared data at the same time
- **General Cleanup** - Removed unused code, consolidated duplicate styling, and improved the shutdown sequence to properly clean up all UI enhancements

---

# StreamUP v2.1.6 - Patch Update

## New Features
- **OBS 32.1 Support** - Full compatibility with OBS Studio 32.1
- **Font Checker** - StreamUP now checks for missing fonts when installing products and shows a warning dialog with download links if any fonts are not installed on your system
- **Dynamic Audio Monitoring Icons** - Advanced Audio Properties now displays dynamic icons showing the current monitoring state for each source
- **Mixer Enhancements** - Added rounded hover styling for source name labels in the audio mixer when using StreamUP themes
- **Color Preview Pills** - New color preview pill styling for StreamUP themes

## Improvements & Bug Fixes
- **Scene Organiser Item Height Range** - Expanded height adjustment range from 50-200% to 10-200% with a new default of 50%, allowing for more compact scene lists
- **Mixer Enhancements Compatibility** - Mixer styling enhancements now only apply on OBS 32.1+ to prevent issues on older versions
- **Multi-Dock Lock Persistence** - Fixed issue where the Multi-Dock lock state would reset to unlocked after restarting OBS
- **Scene Organiser Studio Mode Controls** - Split "Disable Scene Switching in Studio Mode" into two separate settings: one for preview switching (single-click) and one for transitions (double-click), allowing users to disable accidental transitions while keeping preview selection functional
- **Scene Organiser Persistence** - Fixed issue where Scene Organiser folders and order would be lost on OBS restart, particularly affecting scene collections with special characters
- **Studio Mode Program Display** - Fixed rounded corners on the program display in Studio Mode to match the preview display styling
- **Theme Enhancements** - Improved preview and context bar layout with better main window padding and spacing
- **Advanced Audio Properties** - Fixed centering of status dots in the grid layout
- **Theme Detection** - Added proper StreamUP theme checks to all UI enhancement functions to prevent styling issues with other themes

---

# StreamUP v2.1.5 - Patch Update

## Improvements & Bug Fixes
- **Scene Organiser** - Fixed saving and colour issues in Scene Organiser
- **Toolbar** - Removed temporary Group Toolbar functionality
- **UI** - Fixed separator bug

---

# StreamUP v2.1.4 - Patch Update

## New Features
- **Scene Organiser Item Height Adjustment** - Control the size of scene items in Scene Organiser with a slider (50-200%). Both icons and text scale automatically
- **Toolbar Icon Size Control** - Adjust toolbar icon size between 10-24px to fit your workspace
- **Disable Scene Switching in Studio Mode** - Optional setting to prevent accidental scene changes when clicking in Scene Organiser during Studio Mode
- **Add Sources to Group Shortcuts** - New hotkey and WebSocket commands to quickly add selected sources to groups, streamlining your workflow
- **Studio Mode Mid-Point Transition UI** - Enhanced studio mode interface for better transition control

## Improvements & Bug Fixes
- **Settings Persistence** - Fixed critical issue where settings (especially toolbar configuration) weren't saving properly on OBS close
- **Scene Organiser Height Slider** - Slider now properly right-aligned in settings
- **MultiDock Theming** - Improved theme consistency across docks
- **Toolbar Active States** - Added active backgrounds to toolbar buttons for better visual feedback
- **UI Polish** - Various interface improvements and cleanup, including removed borders from native OBS docks for a cleaner look
- **Dock Loading** - Fixed glitches that could occur when loading docks
- **Image and Group Sources** - Fixed issues with image and group source handling
- **Plugin Manager Updates** - Improved plugin manager functionality
- **Dock Host Improvements** - Enhanced inner dock host stability

---

# StreamUP v2.1.3 - Patch Update

## New Features
- **Adjustable Scene Organiser Height** - Customise the height of your Scene Organiser dock to fit your workspace
- **Transition Copy/Paste** - Copy and paste show/hide transitions between sources to speed up your scene setup. This can be set as a HotKey or WebSocket command
- **"Don't Remind" Option** - Added checkbox to stop plugin update notifications if you prefer not to be reminded
- **Early Access Showcase** - Add a banner to show the new features in Early Access and all the perks for supporters
- **Scene Organiser Expand/Collapse Button** - Added dynamic expand/collapse all button that updates based on folder states

## Improvements & Bug Fixes
- **Memory Leak Fixes** - Resolved memory leaks
- **Video Capture Controls** - Fixed issues with video capture device control buttons not working properly in multi-device setups
- **Hotkeys & WebSocket** - Various improvements and additions to the hotkey and WebSocket systems
- **Supporter Names** - Fixed display of supporter names in the credits
- **Scene Organiser Search** - Fixed issue where clearing the search bar would collapse all folders instead of remembering their previous state

---

# StreamUP v2.1.2 - Patch Update

## New Features
- **Scene Organiser Sorting** - Automatically sort your scenes and folders alphabetically (A-Z or Z-A), by newest first, or oldest first
- **Right-Click Scene Sorting** - Right-click in Scene Organiser to quickly sort your scenes without going into settings
- **Remember Folder State** - Scene Organiser now remembers which folders were expanded when you restart OBS

## Improvements & Bug Fixes
- **Plugin Version Checker** - Fixed issues with the plugin update checker not working correctly
- **Scene Organiser** - Various improvements to stability and performance
- **WebSocket Commands Window** - Fixed missing command descriptions that weren't displaying properly

---

# StreamUP v2.1.1 - Patch Update

## New Features
- **Scene Tree Importer** - Import your existing SceneTree plugin configuration into Scene Organiser
- **Hide Scenes in Scene Organiser** - Keep your workspace tidy by hiding scenes you don't need to see
- **Chinese Localisation** - Added zh-CN translation support (thanks to ZRdRy)

## Improvements & Bug Fixes
- Fixed scene collection save issues
- Fixed plugin not loaded check
- Fixed product installation process
- Added libsimde-dev dependency for Ubuntu builds
- Scene Organiser reset functionality improvements

---

# StreamUP v2.1.0 - Major Update

We've listened to your feedback and completely rebuilt the StreamUP OBS plugin from scratch. This has been an enormous project that's taken months of work. The plugin remains completely free - if you find it useful, please share it with friends and consider supporting us!

# New Features & What They Do

## Complete Interface Redesign
Fresh, modern UI that makes all our features easily accessible. We've also created a matching StreamUP OBS theme available to monthly supporters.

## New Welcome Screen
Shows what's new, how to support the project, showcases our supporters, and provides all the important links you need.

## StreamUP Toolbar
The OBS Controls dock is bulky and wastes space. Our sleek toolbar gives you access to a lot of controls and hotkeys plus StreamUP settings. Position it at the top, left, right, or bottom of OBS. Enable it in StreamUP > Settings.

## Multi-Dock System  
Combine multiple functions into single, organised docks. Create themed setups like a 'Vertical Canvas' dock with everything for vertical streaming in one place. Perfect for keeping your workspace tidy and efficient.

## Scene Organiser
Organise your scenes into folders and even colour code them. This is based on the SceneTree plugin by DigitOtter with some extra StreamUP spice added such as a search function for your scenes!

## Enhanced WebSocket Commands
Previously available but poorly documented commands now have a proper interface at StreamUP > WebSocket Commands. Copy OBSRaw websocket requests directly for your workflow. Streamer.Bot users can copy CPH (custom C#) code when enabled in settings.

## Dedicated Hotkeys Menu
As StreamUP becomes more feature-rich, we've added our own hotkeys menu that integrates with OBS. Makes it easier to identify which hotkeys belong to StreamUP (though they still appear in the main OBS hotkeys menu).

## Input Capture Device Management
Tired of cameras not activating when OBS starts, or capture cards crashing? This tool lets you enable all devices, disable all devices, or refresh them (turn off and back on). Access via StreamUP > Tools menu, WebSocket, hotkey, or the StreamUP dock.

# Improvements & Bug Fixes

## Better Performance
Enhanced existing features like the OBS plugin update checker. OBS now starts faster because we check for updates after startup instead of during.

## Streamlined Tools Menu
Options in StreamUP > Tools now trigger their function directly instead of opening separate windows. For hotkey or WebSocket access, use the new WebSocket Commands menu and Settings.

---

# Support This Project

StreamUP is completely free and always will be. Your support helps us continue developing amazing features!

## Ways to Support:
- **[Patreon](https://www.patreon.com/streamup)** - Monthly memberships with exclusive benefits
- **[Ko-Fi](https://ko-fi.com/streamup)** - One-time donations and coffee fund
- **[Buy Me a Beer](https://paypal.me/andilippi)** - Because coding is thirsty work!

## Monthly Supporter Benefits:
- **Tier 1 (£5/month):** StreamUP Product Pass + Discord role + Priority support
- **Tier 2 (£10/month):** All Access Pass + Early releases + Budget-friendly access  
- **Tier 3 (£25/month):** Gold Supporter + Name in credits + Monthly giveaways + Exclusive Discord role

## Follow StreamUP's Development Journey:
- **[Andi's Streams](https://twitch.tv/andilippi)** - Watch development live and see what's coming next!
- **[Andi's Socials](https://doras.to/andi)** - Andi always posts about what he's working on!
- **[Discord Community](https://discord.com/invite/RnDKRaVCEu)** - Get support and chat with other users
- **[Twitter Updates](https://twitter.com/StreamUPTips)** - Latest news and announcements

---

*StreamUP v2.0.0 is our biggest update yet - a complete rebuild that makes everything more reliable whilst adding loads of new features. Thanks for being part of the StreamUP community.*
