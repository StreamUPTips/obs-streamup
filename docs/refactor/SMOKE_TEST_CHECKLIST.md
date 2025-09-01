# StreamUP Plugin - Smoke Test Checklist

**Run this checklist after every phase to ensure no regressions.**

## Pre-Test Setup
- [ ] Build completed successfully with no new errors
- [ ] OBS launches without crashes
- [ ] Plugin loads (check OBS log for "StreamUP" entries)

## Basic Plugin Loading
- [ ] Plugin loads without errors in OBS log
- [ ] Version string logs correctly on startup
- [ ] No immediate crashes or exceptions

## UI Components
### Main Dock
- [ ] StreamUP dock appears in View → Docks menu
- [ ] StreamUP dock opens and displays correctly
- [ ] Dock can be closed and reopened
- [ ] Dock remembers position/size across OBS restarts

### MultiDock System
- [ ] Can create a new MultiDock
- [ ] Can add nested docks to MultiDock
- [ ] Can remove nested docks from MultiDock
- [ ] MultiDock layout persists after OBS restart
- [ ] No crashes when manipulating MultiDock structure

### Toolbar
- [ ] Toolbar appears and functions correctly
- [ ] All toolbar buttons are visible and clickable
- [ ] Toolbar configuration opens without errors
- [ ] Custom toolbar settings persist

### Tools Window
- [ ] Tools window opens from dock or menu
- [ ] All tool buttons are present and functional
- [ ] No broken icons or missing graphics
- [ ] Settings dialogs open correctly

## Source Management
- [ ] Source Info panel updates when selecting sources
- [ ] Source filters display correctly
- [ ] No lag or performance issues when switching sources
- [ ] Source operations don't cause crashes

## Theme and Styling  
- [ ] Dark mode styling applied correctly throughout
- [ ] No unreadable text (light text on light background, etc.)
- [ ] High-DPI displays render correctly
- [ ] Icons display at correct sizes
- [ ] No missing stylesheets or broken CSS

## Settings and Persistence
- [ ] Plugin settings save correctly
- [ ] Settings persist across OBS sessions
- [ ] No corruption of existing settings
- [ ] Default settings work for new installations

## Integration Features
- [ ] WebSocket integration functions (if applicable)
- [ ] Hotkey assignments work correctly
- [ ] Menu items appear in correct locations

## Performance and Stability
- [ ] No memory leaks during normal operation
- [ ] UI remains responsive during operations
- [ ] No excessive CPU usage when idle
- [ ] No new warnings/errors in OBS log compared to baseline

## Cross-Session Behavior
- [ ] Plugin state restores correctly after OBS restart
- [ ] Window geometry restoration works
- [ ] No settings reset unexpectedly

---

**Test Result**: ✅ PASS / ❌ FAIL  
**Date**: ___________  
**Phase**: ___________  
**Notes**: 
_Record any issues or observations here_

---

## Quick Test Commands
For faster testing, focus on these critical paths:
1. Load OBS → Check StreamUP dock appears → Open/close dock
2. Create MultiDock → Add nested dock → Restart OBS → Verify layout restored  
3. Open Tools window → Click 3-4 different buttons → Verify no crashes
4. Change OBS theme → Verify StreamUP styling updates correctly