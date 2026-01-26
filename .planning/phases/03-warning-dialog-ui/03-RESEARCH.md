# Phase 03: Warning Dialog UI - Research

**Researched:** 2026-01-26
**Domain:** Qt 6 Widgets, OBS Plugin UI Patterns
**Confidence:** HIGH

## Summary

Phase 03 requires implementing a warning dialog to display missing fonts before .StreamUP file installation proceeds. The codebase already has a well-established dialog pattern in `PluginManager::PluginsHaveIssue()` that displays plugin issues with tables, warning messages, and a "Continue Anyway" callback mechanism. This exact pattern can be adapted for missing fonts.

The standard approach uses Qt 6 Widgets with QDialog, QTableWidget for tabular data, and the existing StreamUP UI styling system. The plugin has a mature UI infrastructure with `UIHelpers::ShowDialogOnUIThread()` for thread-safe dialog creation, `UIStyles::CreateStyledDialog()` for consistent theming, and `UIStyles::CreateStyledTable()` for tables with clickable links.

**Primary recommendation:** Follow the `PluginManager::PluginsHaveIssue()` pattern exactly - create a new function `FileManager::ShowMissingFontsDialog()` that displays a table of missing fonts with download links (from FontInfo.url field), a warning message, and two buttons: "Cancel" and "Continue Anyway". Integrate into `LoadStreamupFileWithWarning()` flow.

## Standard Stack

The established libraries/tools for dialog UI in this OBS plugin:

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt 6 Widgets | 6.x | Dialog creation, UI widgets | OBS Studio's official UI framework |
| QDialog | Qt 6 | Modal/modeless dialogs | Standard Qt dialog base class |
| QTableWidget | Qt 6 | Tabular data display | Used throughout plugin for lists with links |
| QVBoxLayout/QHBoxLayout | Qt 6 | Layout management | Qt's standard layout system |
| QPushButton | Qt 6 | Button widgets | Standard for dialog actions |
| QGroupBox | Qt 6 | Content grouping | Used for styled sections in dialogs |

### Supporting
| Library | Purpose | When to Use |
|---------|---------|-------------|
| StreamUP::UIHelpers | Thread-safe dialog helpers | For all dialog creation (required) |
| StreamUP::UIStyles | Consistent theming | For all UI elements (required) |
| obs-frontend-api | OBS main window access | For parent widget, theme detection |
| obs-module | Text localization | For all user-facing strings |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| QTableWidget | QListWidget | Less structured, no columns for name/url |
| Custom dialog | QMessageBox | Cannot display tables or custom layouts |
| exec() blocking | show() non-blocking | exec() discouraged by Qt, but codebase uses show() |

**Installation:**
Already included in project CMakeLists.txt via Qt::Core and Qt::Widgets.

## Architecture Patterns

### Recommended Project Structure
Files already exist, no new files needed:
```
core/
├── file-manager.hpp     # Add ShowMissingFontsDialog() declaration
└── file-manager.cpp     # Add ShowMissingFontsDialog() implementation
```

### Pattern 1: Thread-Safe Dialog Creation
**What:** All dialogs MUST be created on the UI thread using `ShowDialogOnUIThread()`
**When to use:** Every dialog creation (non-negotiable in this codebase)
**Example:**
```cpp
// Source: core/plugin-manager.cpp lines 358-360
void PluginsHaveIssue(..., std::function<void()> continueCallback, bool isStartupCheck)
{
	StreamUP::UIHelpers::ShowDialogOnUIThread([missing_modules, ..., continueCallback, isStartupCheck]() {
		// Create dialog here on UI thread
		QDialog *dialog = StreamUP::UIStyles::CreateStyledDialog(titleText);
		// ... rest of dialog setup
	});
}
```

### Pattern 2: Styled Dialog with Table
**What:** Header section + table in QGroupBox + warning label + button layout
**When to use:** Displaying lists of items with actions
**Example:**
```cpp
// Source: core/plugin-manager.cpp lines 385-425
QDialog *dialog = StreamUP::UIStyles::CreateStyledDialog(titleText);
QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);

// Header widget
QWidget* headerWidget = new QWidget();
headerWidget->setStyleSheet(QString("QWidget#headerWidget { background: %1; }")
	.arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD));
QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle(titleText);
QLabel* subtitleLabel = StreamUP::UIStyles::CreateStyledDescription(descText);

// Content with table
QGroupBox *group = StreamUP::UIStyles::CreateStyledGroupBox(groupTitle, "error");
QTableWidget *table = CreateStyledTable(missing_items);
```

### Pattern 3: Continue Anyway Callback
**What:** Pass callback function to dialog for "Continue Anyway" button
**When to use:** Warning dialogs that allow proceeding despite issues
**Example:**
```cpp
// Source: core/file-manager.cpp lines 573-577
StreamUP::PluginManager::ShowCachedPluginIssuesDialog([]() {
	// Continue anyway callback - load with force
	LoadStreamupFile(true);
});

// In dialog button setup (plugin-manager.cpp lines 663-669)
QPushButton *continueButton = StreamUP::UIStyles::CreateStyledButton(
	obs_module_text("UI.Message.ContinueAnyway"), "warning");
QObject::connect(continueButton, &QPushButton::clicked, [dialog, continueCallback]() {
	dialog->close();
	continueCallback();
});
```

### Pattern 4: Table with Clickable Links
**What:** QTableWidget with UserRole data containing URLs, click handler opens links
**When to use:** Displaying download links, documentation URLs
**Example:**
```cpp
// Source: core/plugin-manager.cpp lines 128-131
QTableWidgetItem* downloadItem = new QTableWidgetItem(obs_module_text("UI.Button.Download"));
downloadItem->setForeground(QColor("#3b82f6"));
downloadItem->setData(Qt::UserRole, QString::fromStdString(direct_download_link));
table->setItem(row, 3, downloadItem);

// Click handler (lines 488-491)
QObject::connect(missingTable, &QTableWidget::cellClicked,
	[missingTable](int row, int column) {
		StreamUP::UIStyles::HandleTableCellClick(missingTable, row, column);
	});
```

### Pattern 5: Auto-Sizing Dialogs
**What:** Use `ApplyAutoSizing()` to set responsive min/max dimensions
**When to use:** All dialogs with dynamic content
**Example:**
```cpp
// Source: core/plugin-manager.cpp line 699
StreamUP::UIStyles::ApplyAutoSizing(dialog, 700, 1000, 150, 800);
// Parameters: minWidth, maxWidth, minHeight, maxHeight
```

### Anti-Patterns to Avoid
- **DON'T use QDialog::exec()**: Qt documentation discourages exec() due to nested event loops. This codebase uses show() with callbacks instead
- **DON'T create dialogs on non-UI threads**: Always use ShowDialogOnUIThread() wrapper
- **DON'T hardcode colors**: Use StreamUP::UIStyles::Colors constants
- **DON'T hardcode strings**: Use obs_module_text() for all user-facing text
- **DON'T manually manage dialog memory**: Use Qt::WA_DeleteOnClose attribute

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Dialog creation | Raw QDialog constructor | StreamUP::UIStyles::CreateStyledDialog() | Handles theming, flags, attributes |
| Thread-safe UI updates | Manual QMetaObject::invokeMethod | StreamUP::UIHelpers::ShowDialogOnUIThread() | Proven pattern, already tested |
| Table styling | Custom QTableWidget subclass | StreamUP::UIStyles::CreateStyledTable() | Matches existing plugin aesthetics |
| Button creation | Raw QPushButton | StreamUP::UIStyles::CreateStyledButton() | Consistent colors, hover states |
| URL opening from table | Custom click handler | StreamUP::UIStyles::HandleTableCellClick() | Already handles UserRole data |
| Dialog sizing | Fixed setFixedSize() | StreamUP::UIStyles::ApplyAutoSizing() | Responsive to content, screen size |
| GroupBox styling | Custom stylesheets | StreamUP::UIStyles::CreateStyledGroupBox() | Error/warning/info variants |

**Key insight:** The codebase has mature UI infrastructure. Every "simple" UI task has a helper function that handles edge cases, theming, and platform differences.

## Common Pitfalls

### Pitfall 1: Creating Dialogs on Wrong Thread
**What goes wrong:** Crashes or undefined behavior when creating Qt widgets from non-UI threads
**Why it happens:** OBS plugins may be called from various threads (frontend events, callbacks)
**How to avoid:** ALWAYS wrap dialog creation in `ShowDialogOnUIThread()`
**Warning signs:** Random crashes, "QWidget: Must construct a QApplication before a QWidget" errors

### Pitfall 2: Not Using show() Pattern
**What goes wrong:** Using exec() blocks the event loop and is discouraged by Qt
**Why it happens:** exec() seems simpler for modal dialogs
**How to avoid:** Use show() with callbacks, as done throughout this codebase
**Warning signs:** Nested event loops, dialog lifecycle issues

### Pitfall 3: Hardcoded UI Strings
**What goes wrong:** No localization support, inconsistent with rest of plugin
**Why it happens:** Forgetting to use obs_module_text() wrapper
**How to avoid:** ALL user-facing text must use obs_module_text("UI.Key")
**Warning signs:** English-only text in a plugin that supports multiple languages

### Pitfall 4: Incorrect Font Data Type
**What goes wrong:** Passing vector<string> instead of vector<FontInfo> to dialog
**Why it happens:** Phase 02 returns full FontInfo structs (with url field)
**How to avoid:** Accept vector<FontInfo> parameter, extract face and url in dialog
**Warning signs:** Cannot access download URLs in dialog

### Pitfall 5: Memory Management
**What goes wrong:** Memory leaks or double-free crashes
**Why it happens:** Qt parent-child ownership not understood
**How to avoid:**
- Set Qt::WA_DeleteOnClose on dialogs
- Add widgets to layouts (automatic parenting)
- Don't manually delete dialog-owned widgets
**Warning signs:** Increasing memory usage, crashes on dialog close

### Pitfall 6: Missing OBS Parent Window
**What goes wrong:** Dialog appears behind OBS, not centered correctly
**Why it happens:** Not setting parent widget
**How to avoid:** Pass obs_frontend_get_main_window() as parent
**Warning signs:** Dialogs appear in wrong location, taskbar issues

## Code Examples

Verified patterns from official sources:

### Creating Font Warning Dialog (Adapted from PluginManager)
```cpp
// Source: Adapted from core/plugin-manager.cpp PluginsHaveIssue()
void ShowMissingFontsDialog(const std::vector<FontInfo>& missingFonts,
                           std::function<void()> continueCallback)
{
	StreamUP::UIHelpers::ShowDialogOnUIThread([missingFonts, continueCallback]() {
		QString titleText = obs_module_text("Font.Status.MissingFonts");
		QDialog *dialog = StreamUP::UIStyles::CreateStyledDialog(titleText);

		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(0, 0, 0, 0);

		// Header section
		QWidget* headerWidget = new QWidget();
		headerWidget->setObjectName("headerWidget");
		headerWidget->setStyleSheet(QString("QWidget#headerWidget { background: %1; padding: %2px %3px; }")
			.arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
			.arg(StreamUP::UIStyles::Sizes::PADDING_XL)
			.arg(StreamUP::UIStyles::Sizes::PADDING_XL));

		QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
		QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle(titleText);
		titleLabel->setAlignment(Qt::AlignCenter);
		headerLayout->addWidget(titleLabel);

		QString descText = obs_module_text("Font.Message.RequiredNotInstalled");
		QLabel* subtitleLabel = StreamUP::UIStyles::CreateStyledDescription(descText);
		headerLayout->addWidget(subtitleLabel);

		dialogLayout->addWidget(headerWidget);

		// Content with table
		QVBoxLayout *contentLayout = new QVBoxLayout();
		contentLayout->setContentsMargins(
			StreamUP::UIStyles::Sizes::PADDING_XL,
			StreamUP::UIStyles::Sizes::PADDING_XL,
			StreamUP::UIStyles::Sizes::PADDING_XL,
			StreamUP::UIStyles::Sizes::PADDING_XL);

		QGroupBox *fontGroup = StreamUP::UIStyles::CreateStyledGroupBox(
			obs_module_text("Font.Dialog.MissingGroup"), "error");

		QVBoxLayout *fontLayout = new QVBoxLayout(fontGroup);
		QTableWidget *fontTable = CreateMissingFontsTable(missingFonts);
		fontLayout->addWidget(fontTable);

		contentLayout->addWidget(fontGroup);
		dialogLayout->addLayout(contentLayout);

		// Warning message
		QLabel *warningLabel = new QLabel("⚠️ " + QString(obs_module_text("Font.Dialog.WarningContinue")));
		warningLabel->setStyleSheet(QString(
			"QLabel {"
			"background: rgba(45, 55, 72, 0.8);"
			"color: #fbbf24;"
			"border: 1px solid #f59e0b;"
			"border-radius: %1px;"
			"padding: %2px;"
			"}")
			.arg(StreamUP::UIStyles::Sizes::BORDER_RADIUS)
			.arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM));
		dialogLayout->addWidget(warningLabel);

		// Buttons
		QHBoxLayout *buttonLayout = new QHBoxLayout();
		buttonLayout->addStretch();

		QPushButton *continueButton = StreamUP::UIStyles::CreateStyledButton(
			obs_module_text("UI.Message.ContinueAnyway"), "warning");
		QObject::connect(continueButton, &QPushButton::clicked, [dialog, continueCallback]() {
			dialog->close();
			continueCallback();
		});
		buttonLayout->addWidget(continueButton);

		QPushButton *cancelButton = StreamUP::UIStyles::CreateStyledButton(
			obs_module_text("UI.Button.Cancel"), "neutral");
		QObject::connect(cancelButton, &QPushButton::clicked, dialog, &QDialog::close);
		buttonLayout->addWidget(cancelButton);

		dialogLayout->addLayout(buttonLayout);

		StreamUP::UIStyles::ApplyAutoSizing(dialog, 600, 900, 200, 600);
		dialog->show();
	});
}
```

### Creating Missing Fonts Table
```cpp
// Source: Adapted from core/plugin-manager.cpp CreateMissingPluginsTable()
QTableWidget* CreateMissingFontsTable(const std::vector<FontInfo>& missingFonts)
{
	QStringList headers = {
		obs_module_text("Font.Label.FontName"),
		obs_module_text("Font.Label.DownloadLink")
	};

	QTableWidget* table = StreamUP::UIStyles::CreateStyledTable(headers);
	table->setRowCount(static_cast<int>(missingFonts.size()));

	int row = 0;
	for (const auto& fontInfo : missingFonts) {
		// Font Name column
		QTableWidgetItem* nameItem = new QTableWidgetItem(
			QString::fromStdString(fontInfo.face));
		table->setItem(row, 0, nameItem);

		// Download Link column
		if (!fontInfo.url.empty()) {
			QTableWidgetItem* downloadItem = new QTableWidgetItem(
				obs_module_text("UI.Button.Download"));
			downloadItem->setForeground(QColor("#3b82f6"));
			downloadItem->setData(Qt::UserRole, QString::fromStdString(fontInfo.url));
			table->setItem(row, 1, downloadItem);
		} else {
			QTableWidgetItem* noLinkItem = new QTableWidgetItem(
				obs_module_text("Font.Message.NoDownloadAvailable"));
			noLinkItem->setForeground(QColor("#9ca3af")); // Gray
			table->setItem(row, 1, noLinkItem);
		}

		row++;
	}

	// Connect click handler for links
	QObject::connect(table, &QTableWidget::cellClicked,
		[table](int row, int column) {
			StreamUP::UIStyles::HandleTableCellClick(table, row, column);
		});

	StreamUP::UIStyles::AutoResizeTableColumns(table);
	return table;
}
```

### Integration into LoadStreamupFileWithWarning
```cpp
// Source: core/file-manager.cpp lines 560-578 (modified)
void LoadStreamupFileWithWarning()
{
	// Use cached plugin status for instant response
	if (StreamUP::PluginManager::IsAllPluginsUpToDateCached()) {
		// Plugins are OK, check fonts before opening file selector
		QString fileName = QFileDialog::getOpenFileName(
			nullptr, QT_UTF8(obs_module_text("UI.Button.Load")),
			QString(), "StreamUP File (*.streamup)");

		if (!fileName.isEmpty()) {
			// Extract fonts from selected file
			std::vector<FontInfo> fonts = ExtractFontsFromStreamupFile(fileName);
			std::vector<FontInfo> missingFonts = CheckFontAvailability(fonts);

			if (missingFonts.empty()) {
				// All fonts available, load directly
				LoadStreamupFileFromPath(fileName, false);
			} else {
				// Fonts missing, show warning with continue callback
				ShowMissingFontsDialog(missingFonts, [fileName]() {
					LoadStreamupFileFromPath(fileName, true);
				});
			}
		}
		return;
	}

	// Plugins have issues, show plugin dialog first
	StreamUP::PluginManager::ShowCachedPluginIssuesDialog([]() {
		LoadStreamupFile(true);
	});
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| QDialog::exec() blocking | show() with callbacks | Qt 6 recommendation | Avoids nested event loops, safer lifecycle |
| Manual styling | UIStyles helper functions | Codebase maturity | Consistent theming, less code |
| Direct Qt thread access | ShowDialogOnUIThread() wrapper | OBS plugin requirements | Thread-safe, prevents crashes |
| QMessageBox for warnings | Custom QDialog with tables | Complex data display | More flexible, can show lists |
| QString for fonts | FontInfo struct with url | Phase 01 decision | Download links in Phase 04 |

**Deprecated/outdated:**
- QDialog::exec() - Qt recommends against due to nested event loops; use show() instead
- Direct QWidget creation - Must use UIHelpers::ShowDialogOnUIThread() in OBS plugins
- Custom table styling - Use UIStyles::CreateStyledTable() for consistency

## Open Questions

Things that couldn't be fully resolved:

1. **Localization Keys for New UI Text**
   - What we know: Plugin uses obs_module_text("UI.Key") for all strings
   - What's unclear: Where to add new keys (locale file location not found in codebase)
   - Recommendation: Add placeholder keys in code; localization files likely in data/ folder (not in plugin source)

2. **Empty URL Field Handling**
   - What we know: FontInfo.url may be empty if not provided in .streamup file
   - What's unclear: Should we show "No download available" or hide the column?
   - Recommendation: Show greyed-out text "No download available" for consistency with plugin dialog pattern (see failed_to_load handling)

3. **Dialog Ordering with Plugin Check**
   - What we know: LoadStreamupFileWithWarning() first checks plugins, then should check fonts
   - What's unclear: Should font check happen before or after file selection dialog?
   - Recommendation: Check fonts AFTER file selection (user picks file, then we analyze and warn) - more responsive UX

## Sources

### Primary (HIGH confidence)
- Existing codebase patterns in core/plugin-manager.cpp PluginsHaveIssue() (lines 358-701)
- Existing codebase patterns in core/file-manager.cpp LoadStreamupFileWithWarning() (lines 560-578)
- Existing codebase patterns in ui/ui-helpers.cpp ShowDialogOnUIThread() (lines 83-86)
- Existing codebase patterns in ui/ui-styles.hpp design system constants

### Secondary (MEDIUM confidence)
- [QDialog Class | Qt Widgets | Qt 6.10.1](https://doc.qt.io/qt-6/qdialog.html)
- [QDialogButtonBox Class | Qt Widgets | Qt 6.10.1](https://doc.qt.io/qt-6/qdialogbuttonbox.html)
- [Qt Style Sheets Examples | Qt Widgets | Qt 6.10.1](https://doc.qt.io/qt-6/stylesheet-examples.html)

### Tertiary (LOW confidence)
- [OBS Studio 32.1 Beta: Revamped Audio Mixer](https://www.webpronews.com/obs-studio-32-1-beta-revamped-audio-mixer-and-multi-platform-streaming/) - General UI trends
- Community forum discussions on Qt exec() vs show() patterns

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All libraries already in use, verified in CMakeLists.txt
- Architecture: HIGH - Direct patterns from existing working code
- Pitfalls: HIGH - Common issues documented in similar plugin dialog code

**Research date:** 2026-01-26
**Valid until:** 30 days (stable domain - Qt 6 and OBS plugin patterns change slowly)
