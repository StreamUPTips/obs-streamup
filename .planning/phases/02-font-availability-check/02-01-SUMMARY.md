---
phase: 02-font-availability-check
plan: 01
subsystem: font-detection
tags: [qt6, qfontdatabase, font-validation, cpp]

requires:
  - phase: 01-font-extraction
    artifacts: [FontInfo struct, ExtractFontsFromStreamupFile function]

provides:
  - CheckFontAvailability function for detecting missing fonts
  - Cross-platform font availability checking via Qt 6 API

affects:
  - phase: 03-font-warning-dialog
    impact: "Will consume CheckFontAvailability to determine what warnings to show"
  - phase: 04-font-download
    impact: "Missing fonts returned with preserved url field for download links"

tech-stack:
  added: []
  patterns:
    - Qt 6 static API pattern (QFontDatabase::families())
    - Case-insensitive string comparison with Qt::CaseInsensitive

key-files:
  created: []
  modified:
    - core/file-manager.hpp: "Added CheckFontAvailability declaration and QFontDatabase include"
    - core/file-manager.cpp: "Implemented CheckFontAvailability function"

decisions:
  - decision: "Use Qt 6 static API (QFontDatabase::families()) instead of instance method"
    rationale: "Qt 6 deprecated instance methods in favor of static API"
    alternatives: "Qt 5 pattern with QFontDatabase instance"
  - decision: "Return full FontInfo struct instead of just font names"
    rationale: "Preserves url field needed for Phase 04 download functionality"
    alternatives: "Return vector<string> of font names only"
  - decision: "Case-insensitive comparison using Qt::CaseInsensitive"
    rationale: "Font names are case-insensitive on most systems, handles Unicode correctly"
    alternatives: "Manual toLower() conversion (incorrect for non-ASCII)"

metrics:
  duration: "2 minutes"
  completed: 2026-01-26
---

# Phase 02 Plan 01: Font Availability Check Summary

**One-liner:** Qt 6 static API font detection with case-insensitive matching and url preservation for download phase

## What Was Built

Implemented `CheckFontAvailability()` function that detects which fonts from a .streamup file are missing from the user's system.

**Core functionality:**
- Takes vector of FontInfo extracted from .streamup file
- Queries system fonts via `QFontDatabase::families()` (Qt 6 static API)
- Case-insensitive comparison to handle font name variations
- Returns FontInfo objects for missing fonts (preserving url field)

**API Design:**
```cpp
std::vector<FontInfo> CheckFontAvailability(const std::vector<FontInfo>& fonts);
```

## Technical Implementation

### Qt 6 Static API Pattern
```cpp
// Get all installed fonts (Qt 6 style)
QStringList installedFonts = QFontDatabase::families();
```

No QFontDatabase instance created - uses static method directly per Qt 6 guidelines.

### Case-Insensitive Matching
```cpp
QString::compare(qFontName, installed, Qt::CaseInsensitive) == 0
```

Uses Qt's built-in case-insensitive comparison rather than manual toLower() to handle Unicode correctly.

### Return Type Design
Returns full `FontInfo` struct (not just strings) to preserve the `url` field embedded in .streamup files. Phase 04 will use these URLs to provide download links for missing fonts.

## Verification

**Syntax verification:** Code follows established patterns from existing codebase
- QFontDatabase include added to header
- Function declaration in FONT EXTRACTION FUNCTIONS section
- Implementation follows Qt 6 static API pattern
- Case-insensitive comparison with Qt::CaseInsensitive flag
- Returns std::vector<FontInfo> as specified

**API verification:** Uses Qt 6 static method pattern (no instance creation)

**Return type verification:** Preserves url field by returning full FontInfo struct

## Integration Points

**Consumed by:**
- Phase 03 (Font Warning Dialog): Will call CheckFontAvailability and display results to user
- Phase 04 (Font Download): Will use preserved url field from returned FontInfo objects

**Dependencies:**
- Phase 01: FontInfo struct definition
- Phase 01: ExtractFontsFromStreamupFile function
- Qt 6: QFontDatabase::families() API

## Deviations from Plan

None - plan executed exactly as written.

## Next Phase Readiness

**Ready for Phase 03:** Font Warning Dialog implementation
- CheckFontAvailability function available
- Returns FontInfo vector with missing fonts
- Preserves url field for download links

**Blockers:** None

**Concerns:** None - straightforward Qt API usage

## Testing Notes

Manual testing can be performed by:
1. Creating a .streamup file with both installed fonts (e.g., Arial) and non-existent fonts (e.g., FakeFont123)
2. Calling ExtractFontsFromStreamupFile() to get font list
3. Calling CheckFontAvailability() on the extracted list
4. Verifying only non-existent fonts appear in result

Example expected behavior:
- Input: [{face: "Arial", url: ""}, {face: "FakeFont123", url: "https://example.com/font"}]
- Output: [{face: "FakeFont123", url: "https://example.com/font"}]

## Lessons Learned

**What went well:**
- Clear plan with specific API pattern (Qt 6 static API)
- Well-defined must_haves prevented ambiguity
- Research phase (02-RESEARCH.md) provided all necessary API details

**What could be improved:**
- N/A - straightforward implementation

## Future Considerations

**For Phase 03:**
- Consider how to handle multiple missing fonts (show all at once vs. one at a time)
- UI design for presenting missing font list to user

**For Phase 04:**
- Handle missing url field gracefully (fonts extracted from older .streamup files)
- Validate URL format before attempting download

**Performance:**
- Current implementation O(n*m) where n=fonts to check, m=installed fonts
- If performance becomes issue with many fonts, consider caching installedFonts in a set
- Qt's QFontDatabase::families() is already cached by Qt internally
