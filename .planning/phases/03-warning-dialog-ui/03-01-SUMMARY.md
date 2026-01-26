# Phase 03 Plan 01: Warning Dialog UI Summary

**One-liner:** Missing fonts warning dialog with styled table displaying font names and clickable download links, following PluginsHaveIssue pattern

## Metadata

| Field | Value |
|-------|-------|
| Phase | 03-warning-dialog-ui |
| Plan | 01 |
| Status | Complete |
| Duration | ~4 minutes |
| Completed | 2026-01-26 |

## What Was Built

### ShowMissingFontsDialog Function
A warning dialog that displays missing fonts before .streamup file loading proceeds:

- **Thread-safe dialog creation** using `ShowDialogOnUIThread` wrapper
- **Styled header section** with title and description labels
- **Fonts table** in a GroupBox with error styling:
  - Column 1: Font name
  - Column 2: Download link (clickable) or "No download available" (grayed)
- **Dynamic height calculation**: max 10 rows visible, scrollable if more
- **Warning message** with amber styling warning about continuing without fonts
- **Two buttons**:
  - "Continue Anyway" (warning style) - invokes callback to load file
  - "Cancel" (neutral style) - closes dialog

### CreateMissingFontsTable Helper
Anonymous namespace helper function that creates the styled two-column table:
- Creates QTableWidget with headers using `UIStyles::CreateStyledTable`
- Populates rows with font names and download links
- Download links stored in `Qt::UserRole` for click handling
- Connects `HandleTableCellClick` for URL opening
- Auto-resizes columns

### LoadStreamupFileWithWarning Integration
Font checking integrated into the file loading flow:

**Flow when plugins are OK:**
1. Show file selector dialog
2. Extract fonts from selected file
3. Check font availability
4. If fonts missing: show `ShowMissingFontsDialog`
5. If fonts OK: load file directly

**Flow when plugins have issues:**
1. Show plugin warning dialog
2. On "Continue Anyway": show file selector
3. Extract and check fonts
4. If fonts missing: show font warning
5. On "Continue Anyway": load with `forceLoad=true`

## Commits

| Commit | Description | Files |
|--------|-------------|-------|
| `59dd14b` | Add ShowMissingFontsDialog declaration and helper function | file-manager.hpp, file-manager.cpp |
| `118cb05` | Implement ShowMissingFontsDialog following PluginsHaveIssue pattern | file-manager.cpp |
| `c9a212f` | Integrate font checking into LoadStreamupFileWithWarning flow | file-manager.cpp |

## Files Modified

| File | Changes |
|------|---------|
| `core/file-manager.hpp` | Added `<functional>` include, `ShowMissingFontsDialog` declaration |
| `core/file-manager.cpp` | Added includes (ui-styles, ui-helpers, Qt widgets), `CreateMissingFontsTable` helper, `ShowMissingFontsDialog` implementation, updated `LoadStreamupFileWithWarning` |

## Key Implementation Details

### Pattern Followed: PluginsHaveIssue
The dialog follows the exact same pattern as `PluginsHaveIssue` in `plugin-manager.cpp`:
- Same header widget with background styling
- Same GroupBox with table inside
- Same table styling (no borders, transparent background)
- Same warning label styling
- Same button layout

### Localization Keys Used
Existing keys:
- `UI.Message.ContinueAnyway`
- `UI.Button.Cancel`
- `UI.Button.Download`
- `UI.Button.Load`

New keys (to be added to locale files):
- `Font.Status.MissingFonts`
- `Font.Message.RequiredNotInstalled`
- `Font.Dialog.MissingGroup`
- `Font.Dialog.WarningContinue`
- `Font.Label.FontName`
- `Font.Label.DownloadLink`
- `Font.Message.NoDownloadAvailable`

### Thread Safety
All dialog creation is wrapped in `ShowDialogOnUIThread` to ensure Qt widgets are created on the main thread regardless of which thread calls the function.

## Verification Results

- [x] ShowMissingFontsDialog declared in file-manager.hpp
- [x] ShowMissingFontsDialog implemented following PluginsHaveIssue pattern
- [x] CreateMissingFontsTable helper creates styled two-column table
- [x] Table shows font name and download link (or "No download available")
- [x] Download links are clickable (HandleTableCellClick)
- [x] Dialog has Continue Anyway and Cancel buttons
- [x] Continue Anyway invokes callback, Cancel closes dialog
- [x] LoadStreamupFileWithWarning integrates font checking
- [x] Code compiles without errors

## Deviations from Plan

None - plan executed exactly as written.

## Next Phase Readiness

**Phase 04 Prerequisites Met:**
- Font checking infrastructure complete
- Missing fonts returned with URL field preserved
- Dialog displays download links
- Foundation ready for actual font download implementation
