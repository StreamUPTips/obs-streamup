# Phase 2: Font Availability Check - Research

**Researched:** 2026-01-26
**Domain:** Qt 6 QFontDatabase API, cross-platform font detection
**Confidence:** HIGH

## Summary

This research covers checking if extracted fonts are installed on the user's system using Qt 6's `QFontDatabase::families()` API. Qt 6 made QFontDatabase fully static (no instance required), making it thread-safe and trivial to call. The `families()` method returns all installed font family names as a QStringList, which can be compared against extracted font names using case-insensitive matching. The primary challenge is handling font names with foundry information (e.g., "Helvetica [Adobe]") and ensuring cross-platform compatibility. The standard approach is to create a function `CheckFontAvailability()` in the FileManager namespace that takes a vector of FontInfo and returns a vector of missing font names.

**Key findings:**
- Qt 6 QFontDatabase uses only static methods (no instance needed)
- `QFontDatabase::families()` returns all installed fonts including private system fonts
- Font names with multiple foundries include bracketed notation: "Helvetica [Adobe]"
- Case-insensitive comparison is standard practice for font matching
- QString::compare() with Qt::CaseInsensitive is the recommended comparison method

**Primary recommendation:** Use `QFontDatabase::families()` to get installed fonts, perform case-insensitive comparison with extracted FontInfo list, optionally handling foundry brackets by comparing base family names.

## Standard Stack

The established libraries/tools for this domain:

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt 6 GUI | 6.x | QFontDatabase for font detection | Native Qt API, cross-platform, thread-safe |
| Qt 6 Core | 6.x | QString, QStringList for string handling | Project dependency, provides case-insensitive compare |
| C++ STL | C++17+ | std::vector, std::string for containers | Matches existing codebase patterns |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| StreamUP::FileManager::FontInfo | Current | Font information struct | Already defined in Phase 01 |
| QString::compare() | Qt 6.x | Case-insensitive string comparison | All font name matching operations |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| QFontDatabase | Platform-specific APIs (Win32, CoreText, fontconfig) | QFontDatabase abstracts all platforms, no benefit to custom implementation |
| QString::compare() | toLower() + == | Qt::CaseInsensitive handles Unicode properly, more robust |
| QStringList | std::vector<std::string> | QStringList is native return type from families(), less conversion overhead |

**Installation:**
No additional dependencies needed. QFontDatabase is part of Qt6::Gui which is already linked in the project.

## Architecture Patterns

### Recommended Project Structure
```
core/
├── file-manager.cpp     # Add CheckFontAvailability() here
├── file-manager.hpp     # Add function declaration
└── ...
```

### Pattern 1: Get Installed Fonts (Qt 6 Static API)
**What:** Call QFontDatabase::families() as a static method to retrieve all installed fonts
**When to use:** When you need the list of available system fonts
**Example:**
```cpp
// Source: Qt 6 official documentation https://doc.qt.io/qt-6/qfontdatabase.html
// Qt 6: Static method call (no instance needed)
QStringList installedFonts = QFontDatabase::families();

// Qt 5 deprecated pattern (DO NOT USE):
// QFontDatabase db;
// QStringList installedFonts = db.families();
```

### Pattern 2: Case-Insensitive Font Name Comparison
**What:** Use QString::compare() with Qt::CaseInsensitive flag for robust font matching
**When to use:** All font name comparisons, as font names are case-insensitive in practice
**Example:**
```cpp
// Source: Qt 6 QString documentation and existing codebase patterns
bool IsFontInstalled(const std::string& fontName, const QStringList& installedFonts) {
    QString qFontName = QString::fromStdString(fontName);

    for (const QString& installed : installedFonts) {
        if (QString::compare(qFontName, installed, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}
```

### Pattern 3: Handling Foundry Brackets
**What:** Handle font names with foundry information like "Helvetica [Adobe]"
**When to use:** When matching fonts that might be available from multiple foundries
**Example:**
```cpp
// Source: Qt documentation on QFontDatabase foundry handling
// Option 1: Exact match (foundry-specific)
bool exactMatch = (QString::compare(fontName, installed, Qt::CaseInsensitive) == 0);

// Option 2: Base name match (foundry-agnostic)
QString baseName = fontName;
QString installedBase = installed;

// Remove foundry brackets if present: "Helvetica [Adobe]" -> "Helvetica"
int bracketPos = installedBase.indexOf(" [");
if (bracketPos != -1) {
    installedBase = installedBase.left(bracketPos);
}

bool baseMatch = (QString::compare(baseName, installedBase, Qt::CaseInsensitive) == 0);
```

### Pattern 4: Filtering Private System Fonts
**What:** Optionally filter out private system fonts that users shouldn't select
**When to use:** If building a font selector UI (not needed for availability check)
**Example:**
```cpp
// Source: Qt 6 QFontDatabase documentation
QStringList publicFonts;
QStringList allFonts = QFontDatabase::families();

for (const QString& family : allFonts) {
    if (!QFontDatabase::isPrivateFamily(family)) {
        publicFonts.append(family);
    }
}
// Note: For availability checking, include private fonts
```

### Pattern 5: Return Missing Fonts List
**What:** Compare extracted fonts against installed fonts and return missing ones
**When to use:** Primary pattern for Phase 02 implementation
**Example:**
```cpp
// Source: Derived from existing codebase pattern (plugin-manager.cpp)
std::vector<std::string> CheckFontAvailability(const std::vector<FontInfo>& fonts) {
    QStringList installedFonts = QFontDatabase::families();
    std::vector<std::string> missingFonts;

    for (const FontInfo& fontInfo : fonts) {
        bool found = false;
        QString qFontName = QString::fromStdString(fontInfo.face);

        for (const QString& installed : installedFonts) {
            if (QString::compare(qFontName, installed, Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            missingFonts.push_back(fontInfo.face);
        }
    }

    return missingFonts;
}
```

### Anti-Patterns to Avoid
- **Creating QFontDatabase instance:** In Qt 6, the constructor is deprecated - use static methods
- **Case-sensitive comparison:** Use QString::compare() with Qt::CaseInsensitive, not ==
- **Using toLower() for comparison:** Qt::CaseInsensitive handles Unicode correctly, toLower() may not
- **Ignoring foundry brackets:** "Helvetica" might be installed as "Helvetica [Adobe]" - handle both forms
- **Not checking for empty font names:** Always validate fontInfo.face is non-empty before checking

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Font enumeration | Platform-specific font APIs | QFontDatabase::families() | Cross-platform, handles all edge cases |
| Case-insensitive comparison | Custom toLower() implementation | QString::compare() with Qt::CaseInsensitive | Unicode-aware, locale-correct |
| Foundry name parsing | Custom regex/parsing | QString::indexOf() + left() | Simple and reliable |
| Thread-safe font queries | Mutex-protected custom cache | QFontDatabase (already thread-safe) | All Qt 6 QFontDatabase methods are thread-safe |
| Font existence check | System-specific queries | Compare against QFontDatabase::families() | Consistent across Windows/macOS/Linux |

**Key insight:** Font detection seems simple but has many platform-specific edge cases (foundries, case sensitivity, private fonts, Unicode normalization). QFontDatabase abstracts all of this complexity and provides a clean, thread-safe, cross-platform API.

## Common Pitfalls

### Pitfall 1: Using Qt 5 Instance-Based API in Qt 6
**What goes wrong:** Creating QFontDatabase instance fails or triggers deprecation warnings
**Why it happens:** Qt 6 changed QFontDatabase to be fully static, constructor is deprecated
**How to avoid:** Use `QFontDatabase::families()` directly, never create an instance
**Warning signs:** Compiler warnings about deprecated constructor, unnecessary object allocation

### Pitfall 2: Case-Sensitive Font Name Matching
**What goes wrong:** "Arial" not matched with "arial", fonts reported as missing when they're installed
**Why it happens:** Default string comparison (==) is case-sensitive
**How to avoid:** Always use `QString::compare(a, b, Qt::CaseInsensitive)` for font name comparison
**Warning signs:** Users report fonts as missing that they know are installed, duplicate reports with different casing

### Pitfall 3: Foundry Bracket Mismatch
**What goes wrong:** "Helvetica" not matched when system has "Helvetica [Adobe]"
**Why it happens:** `families()` returns foundry names in brackets when multiple foundries provide the same font
**How to avoid:** Either do exact match OR strip foundry brackets for base name comparison
**Warning signs:** Common fonts like Times, Helvetica reported as missing on some systems

### Pitfall 4: Including Private System Fonts in UI
**What goes wrong:** Font selector shows system-internal fonts that users can't/shouldn't use
**Why it happens:** `families()` returns ALL fonts including private ones (especially on macOS)
**How to avoid:** For availability checking, include private fonts; for UI display, filter with `isPrivateFamily()`
**Warning signs:** Weird font names in UI, fonts that don't work when selected
**Note:** This pitfall is NOT relevant for Phase 02 (we're just checking availability, not building UI)

### Pitfall 5: Empty or Whitespace-Only Font Names
**What goes wrong:** Crash or incorrect results when checking empty string against installed fonts
**Why it happens:** Malformed .streamup files or extraction bugs may produce empty font names
**How to avoid:** Validate `fontInfo.face` is non-empty before comparison
**Warning signs:** Crashes when processing certain .streamup files, all fonts reported as missing

### Pitfall 6: Not Handling Repeated Fonts in Input
**What goes wrong:** Same missing font appears multiple times in output
**Why it happens:** FontInfo list might have duplicates (though Phase 01 deduplicates)
**How to avoid:** Trust Phase 01 deduplication, but consider secondary deduplication in missing list if needed
**Warning signs:** Duplicate entries in missing fonts list shown to user

## Code Examples

Verified patterns from official sources:

### Get Installed Fonts (Qt 6)
```cpp
// Source: https://doc.qt.io/qt-6/qfontdatabase.html
// Qt 6: Static method call
QStringList fontFamilies = QFontDatabase::families();

// Optional: Filter by writing system (e.g., Latin, Cyrillic)
QStringList latinFonts = QFontDatabase::families(QFontDatabase::Latin);
```

### Case-Insensitive Font Matching
```cpp
// Source: Qt 6 QString documentation and community best practices
bool IsFontInstalled(const QString& fontName, const QStringList& installedFonts) {
    for (const QString& installed : installedFonts) {
        // Returns 0 if equal (case-insensitive)
        if (QString::compare(fontName, installed, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}
```

### Check Font Availability and Return Missing Fonts
```cpp
// Source: Derived from existing codebase patterns
std::vector<std::string> CheckFontAvailability(const std::vector<FontInfo>& fonts) {
    // Get all installed fonts from system
    QStringList installedFonts = QFontDatabase::families();
    std::vector<std::string> missingFonts;

    for (const FontInfo& fontInfo : fonts) {
        // Skip empty font names
        if (fontInfo.face.empty()) {
            continue;
        }

        QString qFontName = QString::fromStdString(fontInfo.face);
        bool found = false;

        for (const QString& installed : installedFonts) {
            if (QString::compare(qFontName, installed, Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            missingFonts.push_back(fontInfo.face);
        }
    }

    return missingFonts;
}
```

### Handle Foundry Brackets (Optional Enhancement)
```cpp
// Source: Qt documentation on foundry handling
bool IsFontInstalledWithFoundry(const QString& fontName, const QStringList& installedFonts) {
    for (const QString& installed : installedFonts) {
        // Try exact match first
        if (QString::compare(fontName, installed, Qt::CaseInsensitive) == 0) {
            return true;
        }

        // Try base name match (strip foundry)
        QString installedBase = installed;
        int bracketPos = installedBase.indexOf(" [");
        if (bracketPos != -1) {
            installedBase = installedBase.left(bracketPos);
            if (QString::compare(fontName, installedBase, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }
    return false;
}
```

### Efficient Lookup with QSet (Performance Optimization)
```cpp
// Source: Qt best practices for faster lookup in large lists
std::vector<std::string> CheckFontAvailabilityFast(const std::vector<FontInfo>& fonts) {
    QStringList installedList = QFontDatabase::families();

    // Convert to QSet for O(1) lookup instead of O(n)
    QSet<QString> installedSet;
    for (const QString& font : installedList) {
        installedSet.insert(font.toLower()); // Store lowercase for case-insensitive
    }

    std::vector<std::string> missingFonts;

    for (const FontInfo& fontInfo : fonts) {
        if (fontInfo.face.empty()) continue;

        QString qFontName = QString::fromStdString(fontInfo.face).toLower();
        if (!installedSet.contains(qFontName)) {
            missingFonts.push_back(fontInfo.face);
        }
    }

    return missingFonts;
}
// Note: QSet approach is faster but loses exact case preservation
// Use if performance is critical and you have many fonts to check
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| QFontDatabase instance | Static methods only | Qt 6.0 | Simpler API, no object creation needed |
| Platform-specific APIs | QFontDatabase cross-platform | Qt 4.x+ | Single API works on all platforms |
| Case-sensitive comparison | Qt::CaseInsensitive flag | Best practice | More accurate font matching |
| families(WritingSystem) | families() with optional parameter | Qt 6.x | Cleaner API, backwards compatible |

**Deprecated/outdated:**
- **QFontDatabase constructor**: Deprecated in Qt 6.0, use static methods
- **Non-static method calls**: All QFontDatabase methods are now static
- **Platform-specific font enumeration**: Win32 EnumFontFamilies, CoreText, fontconfig superseded by QFontDatabase

## Open Questions

Things that couldn't be fully resolved:

1. **Should we handle foundry brackets in Phase 02?**
   - What we know: Some fonts appear as "Helvetica [Adobe]" when multiple foundries provide them
   - What's unclear: How common this is in practice, whether .streamup files contain foundry info
   - Recommendation: Start with exact case-insensitive match, add foundry stripping if users report common fonts as missing

2. **Should we normalize Unicode font names?**
   - What we know: QString::compare() with Qt::CaseInsensitive handles Unicode
   - What's unclear: Whether font names in .streamup files use different Unicode normalization (NFC vs NFD)
   - Recommendation: Trust Qt's comparison for now, revisit if edge cases appear with non-Latin fonts

3. **Should we cache QFontDatabase::families() result?**
   - What we know: families() is thread-safe and likely internally cached by Qt
   - What's unclear: Performance impact of calling it multiple times
   - Recommendation: Call once per check, don't pre-cache unless performance profiling shows it's needed

4. **Should missing fonts be deduplicated?**
   - What we know: Phase 01 already deduplicates extracted fonts
   - What's unclear: Whether to double-check for duplicates in missing list
   - Recommendation: Trust Phase 01 deduplication, no need for secondary check

5. **Should we filter private fonts from availability check?**
   - What we know: families() includes private system fonts on macOS/iOS
   - What's unclear: Whether .streamup files might reference private fonts
   - Recommendation: Include all fonts in availability check (don't filter), only filter when building UI

## Sources

### Primary (HIGH confidence)
- [Qt 6 QFontDatabase Class Documentation](https://doc.qt.io/qt-6/qfontdatabase.html) - Official API reference, static methods, families() function
- [Qt 6 GUI Changes from Qt 5](https://doc.qt.io/qt-6/gui-changes-qt6.html) - QFontDatabase constructor deprecation
- [Qt 6 QFontDatabase Obsolete Members](https://doc.qt.io/qt-6/qfontdatabase-obsolete.html) - Deprecated constructor details
- [Qt 6 QString Class Documentation](https://doc.qt.io/qt-6/qstring.html) - compare() method with Qt::CaseInsensitive
- Existing codebase patterns in `core/file-manager.hpp` - FontInfo struct, function signature patterns

### Secondary (MEDIUM confidence)
- [QString Comparison Best Practices](https://runebook.dev/en/articles/qt/qstring/compare) - Case-insensitive comparison guidance
- [Qt Forum: Cross-platform font normalization](https://interest.qt-project.narkive.com/EYxESpwy/cross-platform-font-normalization-best-practice) - Community discussion on font matching
- [Qt Font Handling Guide](https://runebook.dev/en/docs/qt/qfont/setFamily) - Font family setting and fallback behavior

### Tertiary (LOW confidence)
- Web search results about font foundry handling - Anecdotal, should be verified with testing
- Community discussions about private fonts - Platform-specific behavior may vary

## Metadata

**Confidence breakdown:**
- QFontDatabase API: HIGH - Official Qt 6 documentation, confirmed Qt 6 usage in project
- Case-insensitive comparison: HIGH - Qt documentation and existing codebase patterns
- Foundry bracket handling: MEDIUM - Documented in Qt but unclear how often encountered
- Performance considerations: MEDIUM - QSet optimization is standard practice but not verified needed
- Unicode normalization: LOW - Qt handles it but edge cases with non-Latin fonts unclear

**Research date:** 2026-01-26
**Valid until:** ~2026-04-26 (90 days - Qt 6 API is stable, font handling is mature)
