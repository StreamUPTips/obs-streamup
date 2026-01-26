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

### Phase 02: Font Availability Check

**Goal:** Check if extracted fonts are installed on the user's system.

**Depends on:** Phase 01

**Plans:** 0 plans

Plans:
- [ ] TBD (run /gsd:plan-phase 2 to break down)

**Details:**
- Use `QFontDatabase::families()` to get installed fonts
- Case-insensitive comparison
- Return list of missing fonts

---

### Phase 03: Warning Dialog UI

**Goal:** Display a dialog listing missing fonts before installation proceeds.

**Depends on:** Phase 02

**Plans:** 0 plans

Plans:
- [ ] TBD (run /gsd:plan-phase 3 to break down)

**Details:**
- Follow existing plugin warning dialog pattern
- Show font name and placeholder for download link
- "Continue Anyway" and "Cancel" buttons
- Integrate into `LoadStreamupFileWithWarning()` flow

---

### Phase 04: Font URL Display in Dialog

**Goal:** Display font download URLs from embedded `font.url` field in warning dialog.

**Depends on:** Phase 03

**Plans:** 0 plans

Plans:
- [ ] TBD (run /gsd:plan-phase 4 to break down)

**Details:**
- Read `url` field from `FontInfo` (already extracted in Phase 01)
- Display clickable links in warning dialog for missing fonts
- Graceful handling when URL not provided (just show font name)

---

## Future Milestones

- Auto-download fonts (with user permission)
- Font license display in dialog
- Font preview before installation
