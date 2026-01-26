---
phase: "04"
plan: "02"
title: "Font URL Manager UI"
subsystem: "ui"
tags: ["qt", "dialog", "menu", "localization", "font-url"]
dependency-graph:
  requires: ["04-01"]
  provides: ["font-url-manager-dialog", "font-url-menu-integration"]
  affects: []
tech-stack:
  added: []
  patterns: ["ShowDialogOnUIThread", "CreateStyledDialog", "CreateStyledTable"]
key-files:
  created: []
  modified:
    - "core/file-manager.hpp"
    - "core/file-manager.cpp"
    - "ui/menu-manager.cpp"
    - "data/locale/en-US.ini"
    - "data/locale/en-GB.ini"
    - "data/locale/zh-CN.ini"
decisions:
  - id: "menu-always-visible"
    choice: "Font URL Manager menu entry is always visible (not Shift-activated)"
    rationale: "Legitimate dev tool for content creators, not a debug hack"
metrics:
  duration: "~5 minutes"
  completed: "2026-01-26"
---

# Phase 04 Plan 02: Font URL Manager UI Summary

Complete Font URL Manager dialog, menu integration, and localization enabling developers to add font download URLs to text sources in their scenes.

## What Was Built

### ShowFontUrlManagerDialog Implementation
- Full dialog implementation in `file-manager.cpp` (~160 lines)
- Thread-safe dialog creation using `ShowDialogOnUIThread`
- Scans current scene for text sources using `ScanCurrentSceneForTextSources()`
- Displays editable 3-column table: Source Name, Font Name, Download URL
- Save button writes URLs to sources using `SetFontUrlOnSource()`
- Cancel/Close button closes without saving changes
- Empty state handling with informative message when no text sources found
- Dynamic table height calculation (max 10 visible rows)
- Follows existing `ShowMissingFontsDialog` layout pattern for UI consistency

### Menu Integration
- Added Font URL Manager entry to Tools submenu in `menu-manager.cpp`
- Positioned after video device tools, before MultiDock submenu
- Entry triggers `ShowFontUrlManagerDialog()` on click
- Always visible (not Shift-activated) as legitimate dev tool

### Localization
- Added 9 FontUrlManager.* keys to all 3 locale files:
  - `FontUrlManager.Title` - Dialog title
  - `FontUrlManager.Description` - Dialog subtitle/description
  - `FontUrlManager.Label.SourceName` - Table column header
  - `FontUrlManager.Label.FontName` - Table column header
  - `FontUrlManager.Label.Url` - Table column header
  - `FontUrlManager.Button.Save` - Save button text
  - `FontUrlManager.Button.Cancel` - Cancel button text
  - `FontUrlManager.Status.NoTextSources` - Empty state message
  - `FontUrlManager.Status.Saved` - Success notification (reserved)

## Commits

| Hash | Description |
|------|-------------|
| bf6e7ff | feat(04-02): add ShowFontUrlManagerDialog declaration and implementation |
| 1d9efe3 | feat(04-02): add Font URL Manager menu entry |
| ea18294 | feat(04-02): add FontUrlManager.* localization keys |

## Decisions Made

1. **Menu Always Visible:** Font URL Manager entry is always visible in Tools submenu, not Shift-activated like some debug features. Rationale: It's a legitimate developer tool for content creators, not a debug hack.

2. **Follow Existing Dialog Pattern:** Used same layout structure as `ShowMissingFontsDialog` - header widget with title/description, content area with table, button row. Ensures UI consistency.

3. **Editable URL Column Only:** Source Name and Font Name columns are read-only to prevent accidental modifications. Only the URL column is editable.

## Deviations from Plan

None - plan executed exactly as written.

## Success Criteria Verification

- [x] ShowFontUrlManagerDialog declared in file-manager.hpp
- [x] ShowFontUrlManagerDialog implemented in file-manager.cpp
- [x] Menu entry added to Tools submenu in menu-manager.cpp
- [x] All 9 FontUrlManager.* keys added to en-US.ini
- [x] All 9 FontUrlManager.* keys added to en-GB.ini
- [x] All 9 FontUrlManager.* keys added to zh-CN.ini
- [x] Dialog uses ScanCurrentSceneForTextSources (key_link verified)
- [x] Save button uses SetFontUrlOnSource (key_link verified)

## Phase 04 Completion Status

Phase 04 (Font URL Manager) is now **complete**:
- Plan 01: Backend logic (ScanCurrentSceneForTextSources, SetFontUrlOnSource)
- Plan 02: UI dialog, menu integration, localization

The Font URL Manager is fully functional and accessible from the StreamUP Tools menu.

## Testing Notes

To test:
1. Open OBS with StreamUP plugin
2. Create a scene with text sources (GDI+ or FreeType2)
3. Navigate to StreamUP > Tools > Font URL Manager
4. Dialog should show text sources with their font names
5. Edit URL column for any source
6. Click Save - URLs should persist to source settings
7. Re-open dialog to verify URLs were saved
