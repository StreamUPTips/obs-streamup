# Project State

## Current Position

**Milestone:** v1.0 Font Validation
**Phase:** 03 of 04 (Warning Dialog UI)
**Plan:** 02 of 02 complete
**Status:** Phase 03 complete
**Last activity:** 2026-01-26 - Completed 03-02-PLAN.md (Font Localization Keys)

**Progress:** ████████░░ 75% (3/4 phases complete)

## Research Findings

### File Format
- `.streamup` files are JSON
- Text sources identified by `id: "text_gdiplus"`
- Font info at `sources[].settings.font` with structure:
  ```json
  {
    "face": "SeeD Computer",
    "flags": 0,
    "size": 72,
    "style": "Regular"
  }
  ```

### Font Detection
- Qt's `QFontDatabase::families()` returns all installed font family names
- Works cross-platform (Windows, macOS, Linux)
- Need case-insensitive matching

### Existing Patterns
- Plugin warning dialog: `PluginManager::ShowCachedPluginIssuesDialog()`
- File loading entry: `FileManager::LoadStreamupFileFromPath()`
- Already has "Continue Anyway" callback pattern

## Accumulated Context

### Decisions

| Decision | Phase | Rationale | Impact |
|----------|-------|-----------|--------|
| Manual obs_data memory management for nested objects | 01-01 | Match existing ConvertSourcePaths pattern | Consistent codebase patterns |
| Support both text_gdiplus and text_ft2_source | 01-01 | Cross-platform compatibility (Windows + Linux/macOS) | Works on all platforms |
| Case-insensitive font deduplication | 01-01 | Font names case-insensitive in most systems | Avoids duplicate detection |
| FontInfo struct with url field | 01-01 | Embed download URLs in .streamup files | Eliminates API dependency, self-contained files |
| Use Qt 6 static API (QFontDatabase::families()) | 02-01 | Qt 6 deprecated instance methods | Modern Qt pattern, future-proof |
| Return full FontInfo struct from CheckFontAvailability | 02-01 | Preserves url field for Phase 04 download | Download links available without re-parsing |
| Case-insensitive font comparison with Qt::CaseInsensitive | 02-01 | Handles Unicode correctly, system font names vary in case | Cross-platform reliability |
| Follow PluginsHaveIssue dialog pattern | 03-01 | Consistent UI/UX with existing plugin warning | Familiar user experience |
| Thread-safe dialog with ShowDialogOnUIThread | 03-01 | Qt widgets must be created on main thread | Prevents race conditions |
| Place FONT SYSTEM after PLUGIN MANAGEMENT | 03-02 | Consistent with existing section ordering | Clean separation of concerns |

### Font URL Format
Font download URLs embedded directly in .streamup files:
```json
{
  "font": {
    "face": "SeeD Computer",
    "flags": 0,
    "size": 72,
    "style": "Regular",
    "url": "https://fonts.example.com/seed-computer"
  }
}
```

### Localization Keys
Font.* keys added in 03-02:
- Font.Label.FontName
- Font.Label.DownloadLink
- Font.Status.MissingFonts
- Font.Message.RequiredNotInstalled
- Font.Dialog.MissingGroup
- Font.Dialog.WarningContinue
- Font.Message.NoDownloadAvailable

### Roadmap Evolution
- Initial project setup: 2026-01-26
- Phase 01 Plan 01 complete: 2026-01-26
- Phase 02 Plan 01 complete: 2026-01-26
- Phase 03 Plan 01 complete: 2026-01-26
- Phase 03 Plan 02 complete: 2026-01-26 (gap closure)

## Session Continuity

**Last session:** 2026-01-26 09:53:46 UTC
**Stopped at:** Completed 03-02-PLAN.md
**Resume file:** None
