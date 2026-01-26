# Phase 1: Font Extraction from StreamUP Files - Research

**Researched:** 2026-01-26
**Domain:** OBS plugin development (C++/Qt), JSON parsing with obs_data_t
**Confidence:** HIGH

## Summary

This research covers font extraction from StreamUP `.streamup` JSON files using OBS's native `obs_data_t` API. The codebase already has a well-established pattern for parsing `.streamup` files through `OBSWrappers::MakeOBSDataFromJsonFile()` and iterating through source arrays. Font information is located at `sources[].settings.font.face` for text sources with IDs `text_gdiplus` (Windows GDI+ text) and `text_ft2_source` (deprecated FreeType2 text). The standard approach is to add this functionality to the existing `FileManager` namespace in `core/file-manager.cpp`, leveraging existing utility infrastructure for error handling and RAII-based resource management.

**Primary recommendation:** Create a new function `ExtractFontsFromStreamupData()` in the `FileManager` namespace that accepts `obs_data_t*` and returns `std::vector<std::string>` of unique font names using case-insensitive deduplication.

## Standard Stack

The established libraries/tools for this domain:

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| obs-data API | OBS 31.x+ | JSON parsing and data manipulation | Native OBS API, zero dependencies, already integrated |
| Qt 6 Core | 6.x | String handling and utilities | Project dependency, QString comparison utilities |
| C++ STL | C++17+ | Containers and algorithms | std::vector, std::set for deduplication |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| StreamUP::OBSWrappers | Current | RAII wrappers for obs_data_t | All obs_data operations to prevent leaks |
| StreamUP::ErrorHandler | Current | Logging and validation | Error reporting, null checks |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| obs_data_t | Qt's QJsonDocument | obs_data_t already in use, no benefit to mixing APIs |
| std::vector | QStringList | std::string matches existing codebase patterns |

**Installation:**
No additional dependencies needed. All required libraries are already in the project.

## Architecture Patterns

### Recommended Project Structure
```
core/
├── file-manager.cpp     # Add ExtractFontsFromStreamupData() here
├── file-manager.hpp     # Add function declaration
└── ...
utilities/
├── obs-wrappers.hpp     # Use existing RAII wrappers
├── error-handler.hpp    # Use for validation/logging
└── ...
```

### Pattern 1: obs_data_t Iteration with RAII Wrappers
**What:** Use OBSWrappers smart pointers to automatically manage obs_data_t reference counting
**When to use:** All obs_data operations to prevent memory leaks
**Example:**
```cpp
// Source: Existing codebase pattern in file-manager.cpp lines 403-406
auto data = OBSWrappers::MakeOBSDataFromJsonFile(QT_TO_UTF8(file_path));
if (data) {
    // Use data.get() to access raw pointer
    obs_data_array_t *sources = obs_data_get_array(data.get(), "sources");
    // RAII wrapper automatically releases when out of scope
}
```

### Pattern 2: Array Iteration
**What:** Standard pattern for iterating obs_data_array_t
**When to use:** When processing sources, filters, or any array in obs_data
**Example:**
```cpp
// Source: Existing codebase in file-manager.cpp lines 310-320
obs_data_array_t *sources = obs_data_get_array(data, "sources");
const size_t count = obs_data_array_count(sources);

for (size_t i = 0; i < count; i++) {
    obs_data_t *source_data = obs_data_array_item(sources, i);
    const char *id = obs_data_get_string(source_data, "id");

    // Process source_data...

    obs_data_release(source_data); // MUST release each item
}
obs_data_array_release(sources); // MUST release array
```

### Pattern 3: Nested Object Access
**What:** Accessing nested JSON objects like `settings.font.face`
**When to use:** When extracting deeply nested values
**Example:**
```cpp
// Source: Existing codebase in file-manager.cpp lines 221-223
obs_data_t *settings = obs_data_get_obj(source_data, "settings");
if (settings) {
    obs_data_t *font = obs_data_get_obj(settings, "font");
    if (font) {
        const char *face = obs_data_get_string(font, "face");
        // Use face...
        obs_data_release(font);
    }
    obs_data_release(settings);
}
```

### Pattern 4: Source Type Filtering
**What:** Check source ID to determine if it has text/font capabilities
**When to use:** When processing only specific source types
**Example:**
```cpp
// Source: Existing codebase in file-manager.cpp lines 219-220, 234-242
const char *id = obs_data_get_string(source_data, "id");
if (strcmp(id, "text_gdiplus") == 0 || strcmp(id, "text_ft2_source") == 0) {
    // This is a text source, process font settings
}
```

### Pattern 5: Case-Insensitive Deduplication
**What:** Store unique font names with case-insensitive comparison
**When to use:** When building the unique font list
**Example:**
```cpp
// Source: Qt documentation and C++ best practices
// Option 1: Using std::set with custom comparator
struct CaseInsensitiveCompare {
    bool operator()(const std::string& a, const std::string& b) const {
        return QString::compare(QString::fromStdString(a),
                                QString::fromStdString(b),
                                Qt::CaseInsensitive) < 0;
    }
};
std::set<std::string, CaseInsensitiveCompare> unique_fonts;

// Option 2: Using std::vector and manual deduplication
std::vector<std::string> fonts;
for (const auto& font : extracted_fonts) {
    bool found = false;
    for (const auto& existing : fonts) {
        if (QString::compare(QString::fromStdString(font),
                             QString::fromStdString(existing),
                             Qt::CaseInsensitive) == 0) {
            found = true;
            break;
        }
    }
    if (!found) fonts.push_back(font);
}
```

### Anti-Patterns to Avoid
- **Manual obs_data_t management:** Always use OBSWrappers RAII types for top-level objects
- **Forgetting to release array items:** Each `obs_data_array_item()` must be released
- **Not checking for null:** Always validate obs_data_get_obj/get_array results before use
- **Case-sensitive font comparison:** Font names are case-insensitive in practice

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| JSON parsing | Custom JSON parser | obs_data_create_from_json_file() | Already integrated, handles OBS format |
| Memory management | Manual addref/release | OBSWrappers RAII types | Prevents leaks, exception-safe |
| String conversions | Custom Qt/std::string conversion | QT_TO_UTF8/QT_UTF8 macros | Standard macros in codebase |
| Error logging | printf/cout | StreamUP::ErrorHandler | Consistent formatting, categorization |
| Null validation | Manual if checks | ErrorHandler::ValidatePointer | Standardized error messages |

**Key insight:** The obs_data_t API is reference-counted and requires careful release management. Using RAII wrappers eliminates manual reference counting bugs. For nested objects (settings, font), raw pointers are acceptable since their lifetime is short and explicitly managed.

## Common Pitfalls

### Pitfall 1: Memory Leaks from Array Items
**What goes wrong:** Forgetting to release each `obs_data_array_item()` causes memory leaks
**Why it happens:** Array iteration requires releasing both the array AND each item
**How to avoid:** Always pair `obs_data_array_item()` with `obs_data_release()` at loop end
**Warning signs:** Memory usage grows when repeatedly loading .streamup files

### Pitfall 2: Null Pointer Dereferencing
**What goes wrong:** Accessing nested objects without null checks crashes the plugin
**Why it happens:** Not all sources have `settings.font`, some sources lack settings entirely
**How to avoid:** Check every `obs_data_get_obj()` result before using it
**Warning signs:** Crashes when loading .streamup files with non-text sources

### Pitfall 3: Case-Sensitive Font Matching
**What goes wrong:** Treating "Arial" and "arial" as different fonts
**Why it happens:** Default string comparison is case-sensitive
**How to avoid:** Use Qt::CaseInsensitive flag with QString::compare()
**Warning signs:** Duplicate fonts in output with different casing

### Pitfall 4: Wrong Source Type IDs
**What goes wrong:** Missing fonts because source ID is versioned (e.g., "text_gdiplus_v2")
**Why it happens:** OBS can version source IDs, `obs_source_get_id()` returns versioned ID
**How to avoid:** Use `obs_source_get_unversioned_id()` or check for string prefix
**Warning signs:** Fonts not extracted from valid text sources

### Pitfall 5: Mixing RAII and Manual Management
**What goes wrong:** Double-release or missed release when mixing wrapper types
**Why it happens:** OBSWrappers auto-release, manual obs_data_release also called
**How to avoid:** Use RAII wrappers for file-level data, manual release only for nested objects
**Warning signs:** Crashes on scope exit or use-after-free errors

## Code Examples

Verified patterns from official sources and existing codebase:

### Loading .streamup File
```cpp
// Source: core/file-manager.cpp lines 403-406
auto data = OBSWrappers::MakeOBSDataFromJsonFile(QT_TO_UTF8(file_path));
if (!data) {
    StreamUP::ErrorHandler::LogError("Failed to parse StreamUP file: " + file_path.toStdString(),
                                      StreamUP::ErrorHandler::Category::FileSystem);
    return {};
}
```

### Iterating Sources Array
```cpp
// Source: core/file-manager.cpp lines 381-385
obs_data_array_t *sources = obs_data_get_array(data.get(), "sources");
if (!sources) {
    return {};
}

const size_t count = obs_data_array_count(sources);
std::vector<std::string> fonts;

for (size_t i = 0; i < count; i++) {
    obs_data_t *source_data = obs_data_array_item(sources, i);

    // Process source...

    obs_data_release(source_data); // Critical: release each item
}

obs_data_array_release(sources); // Critical: release array
```

### Extracting Font from Text Source
```cpp
// Source: Derived from file-manager.cpp patterns
const char *id = obs_data_get_string(source_data, "id");

// Check if this is a text source type
if (strcmp(id, "text_gdiplus") == 0 || strcmp(id, "text_ft2_source") == 0) {
    obs_data_t *settings = obs_data_get_obj(source_data, "settings");
    if (settings) {
        obs_data_t *font_obj = obs_data_get_obj(settings, "font");
        if (font_obj) {
            const char *face = obs_data_get_string(font_obj, "face");
            if (face && strlen(face) > 0) {
                // Add font to collection
                fonts.push_back(std::string(face));
            }
            obs_data_release(font_obj);
        }
        obs_data_release(settings);
    }
}
```

### Case-Insensitive Deduplication
```cpp
// Source: Qt documentation https://doc.qt.io/qt-6/qstring.html
std::vector<std::string> unique_fonts;

for (const auto& font : all_fonts) {
    bool exists = false;
    for (const auto& existing : unique_fonts) {
        if (QString::compare(QString::fromStdString(font),
                             QString::fromStdString(existing),
                             Qt::CaseInsensitive) == 0) {
            exists = true;
            break;
        }
    }
    if (!exists) {
        unique_fonts.push_back(font);
    }
}
```

### Using ErrorHandler for Validation
```cpp
// Source: utilities/error-handler.hpp and existing usage
if (!StreamUP::ErrorHandler::ValidatePointer(settings, "settings")) {
    StreamUP::ErrorHandler::LogWarning("Source missing settings object",
                                        StreamUP::ErrorHandler::Category::FileSystem);
    continue; // Skip this source
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Qt QJsonDocument | obs_data_t API | OBS 0.x+ | Native OBS integration, no JSON library dependency |
| Manual new/delete | RAII smart pointers | C++11+ | Automatic memory management, exception-safe |
| text_ft2_source | text_gdiplus (Windows) | OBS 27.x | FreeType2 marked deprecated, GDI+ preferred on Windows |

**Deprecated/outdated:**
- **text_ft2_source**: Still functional but marked deprecated in favor of platform-native text sources
- **Manual obs_data_t release**: Replaced by OBSWrappers RAII types in this codebase
- **QJson library**: Superseded by Qt's built-in JSON support and OBS's obs_data_t

## Open Questions

Things that couldn't be fully resolved:

1. **Are there other text source types besides text_gdiplus and text_ft2_source?**
   - What we know: Official OBS documentation only lists these two
   - What's unclear: Third-party plugins may add custom text sources with different IDs
   - Recommendation: Start with known types, add logging to detect unknown types with "text" in ID

2. **Should we use versioned or unversioned source IDs?**
   - What we know: `obs_source_get_id()` returns versioned (e.g., "text_gdiplus_v2"), `obs_source_get_unversioned_id()` returns base
   - What's unclear: Which form is stored in .streamup JSON files
   - Recommendation: Test with actual .streamup file, use strcmp or strstr for matching

3. **Do all .streamup files use the sources[] array structure?**
   - What we know: LoadScene() function expects this structure (line 381-385)
   - What's unclear: Whether legacy or malformed files might differ
   - Recommendation: Validate array exists, return empty list if not found (graceful degradation)

## Sources

### Primary (HIGH confidence)
- [OBS Studio Source API Documentation](https://docs.obsproject.com/reference-sources) - Source ID functions, settings access
- [OBS Studio Data Settings API Reference](https://docs.obsproject.com/reference-settings) - obs_data_t functions
- Existing codebase patterns in `core/file-manager.cpp` - Proven iteration and parsing patterns
- Existing codebase patterns in `utilities/obs-wrappers.hpp` - RAII wrapper implementations

### Secondary (MEDIUM confidence)
- [Qt 6 QString Documentation](https://doc.qt.io/qt-6/qstring.html) - QString::compare() with Qt::CaseInsensitive
- [OBS Forum: Text Source Types Discussion](https://obsproject.com/forum/threads/why-two-types-text-sources.66406/) - text_gdiplus vs text_ft2_source
- [OBS GitHub: obs-text plugin source](https://github.com/obsproject/obs-studio/blob/master/plugins/obs-text/gdiplus/obs-text.cpp) - text_gdiplus implementation
- [OBS Text Sources KB Article](https://obsproject.com/kb/text-sources) - Official documentation on text source types

### Tertiary (LOW confidence)
- Web search results mentioning text_gdiplus_v2 variant - Not verified in official docs
- Community discussions about font handling - Anecdotal, not authoritative

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All libraries verified in CMakeLists.txt and existing code
- Architecture: HIGH - Patterns directly from existing codebase file-manager.cpp
- Pitfalls: HIGH - Based on obs_data_t API documentation and common C++ memory management issues
- Text source types: MEDIUM - Only two types confirmed in official docs, possible third-party extensions
- Font structure location: HIGH - Confirmed in PROJECT.md and consistent with OBS text source design

**Research date:** 2026-01-26
**Valid until:** ~2026-03-26 (60 days - OBS plugin API is stable, Qt 6 API is stable)
