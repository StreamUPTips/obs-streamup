# StreamUP Font Checker - Roadmap

## Current Milestone: v1.0 Font Validation

### Phase 01: Font Extraction from StreamUP Files ✓

**Goal:** Parse `.streamup` JSON files and extract all unique font family names.

**Status:** Complete (2026-01-26)

**Depends on:** None

**Plans:** 1 plan

Plans:
- [x] 01-01-PLAN.md - Implement ExtractFontsFromStreamupData function with case-insensitive deduplication

**Delivered:**
- `FontInfo` struct with `face` and `url` fields
- `ExtractFontsFromStreamupData(obs_data_t*)` - extracts fonts from parsed data
- `ExtractFontsFromStreamupFile(QString&)` - convenience wrapper for file path
- Handles both `text_gdiplus` and `text_ft2_source` types
- Case-insensitive deduplication using Qt

---

### Phase 02: Font Availability Check ✓

**Goal:** Check if extracted fonts are installed on the user's system.

**Status:** Complete (2026-01-26)

**Depends on:** Phase 01

**Plans:** 1 plan

Plans:
- [x] 02-01-PLAN.md - Implement CheckFontAvailability function with Qt 6 QFontDatabase API

**Delivered:**
- `CheckFontAvailability(const std::vector<FontInfo>&)` - returns missing fonts
- Uses Qt 6 static API (`QFontDatabase::families()`)
- Case-insensitive font matching with `Qt::CaseInsensitive`
- Preserves `url` field in returned FontInfo for Phase 04

---

### Phase 03: Warning Dialog UI ✓

**Goal:** Display a dialog listing missing fonts before installation proceeds.

**Status:** Complete (2026-01-26)

**Depends on:** Phase 02

**Plans:** 2 plans

Plans:
- [x] 03-01-PLAN.md — Implement ShowMissingFontsDialog with styled table and Continue Anyway/Cancel flow
- [x] 03-02-PLAN.md — Add Font.* localization keys to all locale files (gap closure)

**Delivered:**
- `ShowMissingFontsDialog(vector<FontInfo>, callback)` - displays warning dialog
- Styled table showing font names and clickable download links
- Thread-safe dialog creation via `ShowDialogOnUIThread`
- Continue Anyway and Cancel buttons with proper callbacks
- Integration into `LoadStreamupFileWithWarning()` flow
- Localized strings for en-US, en-GB, zh-CN

---

### Phase 04: Font URL Manager (Dev Tool) ✓

**Goal:** Provide a dev tool to add font download URLs directly to text sources in OBS scenes.

**Status:** Complete (2026-01-26)

**Depends on:** Phase 03

**Plans:** 2 plans

Plans:
- [x] 04-01-PLAN.md — Backend logic (TextSourceFontInfo struct, ScanCurrentSceneForTextSources, SetFontUrlOnSource)
- [x] 04-02-PLAN.md — Dialog UI, menu integration, and localization keys

**Delivered:**
- `TextSourceFontInfo` struct for holding source/font/URL data
- `ScanCurrentSceneForTextSources()` — enumerates text sources in current scene
- `SetFontUrlOnSource()` — writes URL to source settings via OBS API
- `ShowFontUrlManagerDialog()` — styled dialog with editable URL column
- Menu entry in Tools submenu (always visible)
- All FontUrlManager.* localization keys (en-US, en-GB, zh-CN)

---

## Future Milestones

- Auto-download fonts (with user permission)
- Font license display in dialog
- Font preview before installation
