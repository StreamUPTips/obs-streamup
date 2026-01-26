# StreamUP Font Checker - Roadmap

## Current Milestone: v1.0 Font Validation

### Phase 01: Font Extraction from StreamUP Files

**Goal:** Parse `.streamup` JSON files and extract all unique font family names.

**Depends on:** None

**Plans:** 1 plan

Plans:
- [ ] 01-01-PLAN.md - Implement ExtractFontsFromStreamupData function with case-insensitive deduplication

**Details:**
- Create function to iterate sources and extract `settings.font.face`
- Handle all text source types (`text_gdiplus`, potentially `text_ft2_source`)
- Return deduplicated list of font names

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

### Phase 04: API Integration for Font Links

**Goal:** Fetch font download URLs from StreamUP API.

**Depends on:** Phase 03

**Plans:** 0 plans

Plans:
- [ ] TBD (run /gsd:plan-phase 4 to break down)

**Details:**
- API endpoint to return font metadata by name
- Clickable links in warning dialog
- Graceful fallback when API unavailable
- Cache font info to reduce API calls

---

## Future Milestones

- Auto-download fonts (with user permission)
- Font license display in dialog
- Font preview before installation
