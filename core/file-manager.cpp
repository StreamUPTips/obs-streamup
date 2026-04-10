#include "file-manager.hpp"
#include "streamup-common.hpp"
#include "plugin-manager.hpp"
#include "plugin-state.hpp"
#include "obs-wrappers.hpp"
#include "error-handler.hpp"
#include "string-utils.hpp"
#include "version-utils.hpp"
#include "path-utils.hpp"
#include "../ui/ui-styles.hpp"
#include "../ui/ui-helpers.hpp"
#include <obs-module.h>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QApplication>
#include <QString>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <list>
#include <map>

namespace StreamUP {
namespace FileManager {

namespace {
// Case-insensitive check if font already exists in vector (by face name)
bool FontExistsInVector(const std::vector<FontInfo>& fonts, const std::string& font_name) {
    QString target = QString::fromStdString(font_name);
    for (const auto& existing : fonts) {
        if (QString::compare(target, QString::fromStdString(existing.face), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

// Create styled table for missing fonts display
QTableWidget* CreateMissingFontsTable(const std::vector<FontInfo>& missingFonts) {
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
            noLinkItem->setForeground(QColor("#9ca3af")); // Gray for unavailable
            table->setItem(row, 1, noLinkItem);
        }

        row++;
    }

    // Connect click handler for download links
    QObject::connect(table, &QTableWidget::cellClicked,
        [table](int r, int c) {
            StreamUP::UIStyles::HandleTableCellClick(table, r, c);
        });

    StreamUP::UIStyles::AutoResizeTableColumns(table);
    return table;
}
} // anonymous namespace

//-------------------FONT EXTRACTION FUNCTIONS-------------------
std::vector<FontInfo> ExtractFontsFromStreamupData(obs_data_t *data)
{
	std::vector<FontInfo> fonts;

	if (!data) {
		StreamUP::ErrorHandler::LogWarning("Null data passed to ExtractFontsFromStreamupData",
			StreamUP::ErrorHandler::Category::FileSystem);
		return fonts;
	}

	// Get sources array
	obs_data_array_t *sources = obs_data_get_array(data, "sources");
	if (!sources) {
		return fonts;
	}

	const size_t count = obs_data_array_count(sources);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *source_data = obs_data_array_item(sources, i);
		if (!source_data) {
			continue;
		}

		// Check if this is a text source
		const char *source_id = obs_data_get_string(source_data, "id");
		bool is_text_source = (source_id &&
			(strcmp(source_id, "text_gdiplus") == 0 || strcmp(source_id, "text_ft2_source") == 0));

		if (is_text_source) {
			// Get settings object
			obs_data_t *settings = obs_data_get_obj(source_data, "settings");
			if (settings) {
				// Get font object
				obs_data_t *font_obj = obs_data_get_obj(settings, "font");
				if (font_obj) {
					// Get face string
					const char *face = obs_data_get_string(font_obj, "face");
					if (face && strlen(face) > 0) {
						// Add if not already present (case-insensitive)
						if (!FontExistsInVector(fonts, face)) {
							FontInfo info;
							info.face = face;
							// Get optional url field
							const char *url = obs_data_get_string(font_obj, "url");
							if (url && strlen(url) > 0) {
								info.url = url;
							}
							fonts.push_back(info);
						}
					}
					obs_data_release(font_obj);
				}
				obs_data_release(settings);
			}
		}

		obs_data_release(source_data);
	}

	obs_data_array_release(sources);
	return fonts;
}

std::vector<FontInfo> ExtractFontsFromStreamupFile(const QString &file_path)
{
	// Validate file exists
	if (!StreamUP::ErrorHandler::ValidateFile(file_path)) {
		return {};
	}

	// Load the .streamup file
	auto data = OBSWrappers::MakeOBSDataFromJsonFile(QT_TO_UTF8(file_path));
	if (!data) {
		StreamUP::ErrorHandler::LogWarning(
			"Failed to parse StreamUP file for font extraction: " + file_path.toStdString(),
			StreamUP::ErrorHandler::Category::FileSystem);
		return {};
	}

	return ExtractFontsFromStreamupData(data.get());
}

std::vector<FontInfo> CheckFontAvailability(const std::vector<FontInfo>& fonts)
{
	std::vector<FontInfo> missingFonts;

	// Get all installed fonts from system (Qt 6 static API)
	QStringList installedFonts = QFontDatabase::families();

	for (const FontInfo& fontInfo : fonts) {
		// Skip empty font names
		if (fontInfo.face.empty()) {
			continue;
		}

		QString qFontName = QString::fromStdString(fontInfo.face);
		bool found = false;

		// Case-insensitive comparison against installed fonts
		for (const QString& installed : installedFonts) {
			if (QString::compare(qFontName, installed, Qt::CaseInsensitive) == 0) {
				found = true;
				break;
			}
		}

		if (!found) {
			// Return full FontInfo to preserve url for Phase 04
			missingFonts.push_back(fontInfo);
		}
	}

	return missingFonts;
}

void ShowMissingFontsDialog(const std::vector<FontInfo>& missingFonts,
                            std::function<void()> continueCallback)
{
    StreamUP::UIHelpers::ShowDialogOnUIThread([missingFonts, continueCallback]() {
        QString titleText = obs_module_text("Font.Status.MissingFonts");
        QDialog *dialog = StreamUP::UIStyles::CreateStyledDialog(titleText);

        QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
        dialogLayout->setContentsMargins(0, 0, 0, 0);
        dialogLayout->setSpacing(0);

        // Header section (same pattern as PluginsHaveIssue)
        QWidget* headerWidget = new QWidget();
        headerWidget->setObjectName("headerWidget");
        headerWidget->setStyleSheet(QString("QWidget#headerWidget { background: %1; padding: %2px %3px %4px %3px; }")
            .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
            .arg(StreamUP::UIStyles::Sizes::PADDING_XL + StreamUP::UIStyles::Sizes::PADDING_MEDIUM)
            .arg(StreamUP::UIStyles::Sizes::PADDING_XL)
            .arg(StreamUP::UIStyles::Sizes::PADDING_XL));

        QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
        headerLayout->setContentsMargins(0, 0, 0, 0);

        QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle(titleText);
        titleLabel->setAlignment(Qt::AlignCenter);
        headerLayout->addWidget(titleLabel);

        headerLayout->addSpacing(-StreamUP::UIStyles::Sizes::SPACING_SMALL);

        QString descText = obs_module_text("Font.Message.RequiredNotInstalled");
        QLabel* subtitleLabel = StreamUP::UIStyles::CreateStyledDescription(descText);
        headerLayout->addWidget(subtitleLabel);

        dialogLayout->addWidget(headerWidget);

        // Content area with fonts table
        QVBoxLayout *contentLayout = new QVBoxLayout();
        contentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5,
            StreamUP::UIStyles::Sizes::PADDING_XL,
            StreamUP::UIStyles::Sizes::PADDING_XL + 5,
            StreamUP::UIStyles::Sizes::PADDING_XL);
        contentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);

        dialogLayout->addLayout(contentLayout);

        // Create styled GroupBox with table
        QGroupBox *fontGroup = StreamUP::UIStyles::CreateStyledGroupBox(
            obs_module_text("Font.Dialog.MissingGroup"), "error");
        fontGroup->setMinimumWidth(400);
        fontGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

        QVBoxLayout *fontLayout = new QVBoxLayout(fontGroup);
        fontLayout->setContentsMargins(8, 8, 8, 8);
        fontLayout->setSpacing(0);

        QTableWidget *fontTable = CreateMissingFontsTable(missingFonts);

        // Dynamic height calculation: max 10 rows
        int rowCount = fontTable->rowCount();
        int maxVisibleRows = std::min(rowCount, 10);
        int headerHeight = 35;
        int rowHeight = 30;
        int tableHeight = headerHeight + (rowHeight * maxVisibleRows) + 6;

        fontTable->setFixedHeight(tableHeight);
        if (rowCount > 10) {
            fontTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        } else {
            fontTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }

        // Remove table border to blend with group box
        fontTable->setStyleSheet(
            fontTable->styleSheet() + StreamUP::UIStyles::TABLE_INLINE_STYLESHEET);

        fontLayout->addWidget(fontTable);
        contentLayout->addWidget(fontGroup);

        // Warning message (same pattern as PluginsHaveIssue)
        dialogLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

        QLabel *warningLabel = new QLabel(QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f ") +
            QString(obs_module_text("Font.Dialog.WarningContinue")));
        warningLabel->setWordWrap(true);
        warningLabel->setStyleSheet(QString(
            "QLabel {"
            "background: rgba(45, 55, 72, 0.8);"
            "color: #fbbf24;"
            "border: 1px solid #f59e0b;"
            "border-radius: %1px;"
            "padding: %2px;"
            "margin: %3px %4px;"
            "font-size: %5px;"
            "line-height: 1.4;"
            "}")
            .arg(StreamUP::UIStyles::Sizes::BORDER_RADIUS)
            .arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM)
            .arg(StreamUP::UIStyles::Sizes::SPACING_SMALL)
            .arg(StreamUP::UIStyles::Sizes::PADDING_XL + 5)
            .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL));
        dialogLayout->addWidget(warningLabel);

        // Buttons
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        buttonLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5,
            StreamUP::UIStyles::Sizes::SPACING_MEDIUM,
            StreamUP::UIStyles::Sizes::PADDING_XL + 5,
            StreamUP::UIStyles::Sizes::PADDING_XL);
        buttonLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        buttonLayout->addStretch();

        // Continue Anyway button
        QPushButton *continueButton = StreamUP::UIStyles::CreateStyledButton(
            obs_module_text("UI.Message.ContinueAnyway"), "warning");
        QObject::connect(continueButton, &QPushButton::clicked, [dialog, continueCallback]() {
            dialog->close();
            continueCallback();
        });
        buttonLayout->addWidget(continueButton);

        // Cancel button
        QPushButton *cancelButton = StreamUP::UIStyles::CreateStyledButton(
            obs_module_text("UI.Button.Cancel"), "neutral", 30, 100);
        QObject::connect(cancelButton, &QPushButton::clicked, dialog, &QDialog::close);
        buttonLayout->addWidget(cancelButton);

        dialogLayout->addLayout(buttonLayout);
        dialog->setLayout(dialogLayout);

        // Apply auto-sizing
        StreamUP::UIStyles::ApplyAutoSizing(dialog, 500, 800, 200, 600);
    });
}

//-------------------FONT URL MANAGER FUNCTIONS-------------------
std::vector<TextSourceFontInfo> ScanCurrentSceneForTextSources()
{
	std::vector<TextSourceFontInfo> results;

	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		return results;
	}

	obs_scene_t *scene = obs_scene_from_source(current_scene);
	if (!scene) {
		obs_source_release(current_scene);
		return results;
	}

	obs_scene_enum_items(scene, [](obs_scene_t*, obs_sceneitem_t *item, void *data) {
		auto *results = static_cast<std::vector<TextSourceFontInfo>*>(data);
		obs_source_t *source = obs_sceneitem_get_source(item);
		const char *id = obs_source_get_unversioned_id(source);

		// Check for text source types (Windows and Linux/macOS)
		// Use unversioned ID to handle text_gdiplus_v3, etc.
		if (id && (strcmp(id, "text_gdiplus") == 0 || strcmp(id, "text_ft2_source") == 0)) {
			TextSourceFontInfo info;
			info.source = source;
			obs_source_get_ref(source);  // addref so pointer stays valid beyond enumeration
			info.sourceName = obs_source_get_name(source);

			obs_data_t *settings = obs_source_get_settings(source);
			if (settings) {
				obs_data_t *font = obs_data_get_obj(settings, "font");
				if (font) {
					const char *face = obs_data_get_string(font, "face");
					const char *url = obs_data_get_string(font, "url");
					info.fontFace = face ? face : "";
					info.currentUrl = url ? url : "";
					obs_data_release(font);
				}
				obs_data_release(settings);
			}

			results->push_back(info);
		}
		return true; // continue enumeration
	}, &results);

	obs_source_release(current_scene);
	return results;
}

void SetFontUrlOnSource(obs_source_t *source, const std::string &url)
{
	if (!source) {
		StreamUP::ErrorHandler::LogWarning("Null source passed to SetFontUrlOnSource",
			StreamUP::ErrorHandler::Category::Source);
		return;
	}

	obs_data_t *settings = obs_source_get_settings(source);
	if (!settings) {
		StreamUP::ErrorHandler::LogWarning("Failed to get settings for source in SetFontUrlOnSource",
			StreamUP::ErrorHandler::Category::Source);
		return;
	}

	obs_data_t *font = obs_data_get_obj(settings, "font");
	if (font) {
		obs_data_set_string(font, "url", url.c_str());
		obs_data_release(font);
	} else {
		// Font object doesn't exist - create one with just the URL
		// This shouldn't happen for text sources, but handle gracefully
		obs_data_t *new_font = obs_data_create();
		obs_data_set_string(new_font, "url", url.c_str());
		obs_data_set_obj(settings, "font", new_font);
		obs_data_release(new_font);
	}

	// Apply the updated settings to the source
	obs_source_update(source, settings);
	obs_data_release(settings);
}

void ShowFontUrlManagerDialog()
{
	StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
		QString titleText = obs_module_text("FontUrlManager.Title");
		QDialog *dialog = StreamUP::UIStyles::CreateStyledDialog(titleText);

		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(0, 0, 0, 0);
		dialogLayout->setSpacing(0);

		// Header section
		QWidget* headerWidget = new QWidget();
		headerWidget->setObjectName("headerWidget");
		headerWidget->setStyleSheet(QString("QWidget#headerWidget { background: %1; padding: %2px %3px %4px %3px; }")
			.arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
			.arg(StreamUP::UIStyles::Sizes::PADDING_XL + StreamUP::UIStyles::Sizes::PADDING_MEDIUM)
			.arg(StreamUP::UIStyles::Sizes::PADDING_XL)
			.arg(StreamUP::UIStyles::Sizes::PADDING_XL));

		QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
		headerLayout->setContentsMargins(0, 0, 0, 0);

		QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle(titleText);
		titleLabel->setAlignment(Qt::AlignCenter);
		headerLayout->addWidget(titleLabel);

		headerLayout->addSpacing(-StreamUP::UIStyles::Sizes::SPACING_SMALL);

		QString descText = obs_module_text("FontUrlManager.Description");
		QLabel* subtitleLabel = StreamUP::UIStyles::CreateStyledDescription(descText);
		headerLayout->addWidget(subtitleLabel);

		dialogLayout->addWidget(headerWidget);

		// Content area
		QVBoxLayout *contentLayout = new QVBoxLayout();
		contentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5,
			StreamUP::UIStyles::Sizes::PADDING_XL,
			StreamUP::UIStyles::Sizes::PADDING_XL + 5,
			StreamUP::UIStyles::Sizes::PADDING_XL);
		contentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);

		dialogLayout->addLayout(contentLayout);

		// Scan current scene for text sources
		std::vector<TextSourceFontInfo> textSources = ScanCurrentSceneForTextSources();

		// Release addref'd source pointers when dialog is destroyed
		auto *textSourcesCopy = new std::vector<TextSourceFontInfo>(textSources);
		QObject::connect(dialog, &QDialog::destroyed, [textSourcesCopy]() {
			for (auto &info : *textSourcesCopy) {
				if (info.source) {
					obs_source_release(info.source);
					info.source = nullptr;
				}
			}
			delete textSourcesCopy;
		});

		if (textSources.empty()) {
			// No text sources found - show message
			QLabel *emptyLabel = new QLabel(obs_module_text("FontUrlManager.Status.NoTextSources"));
			emptyLabel->setAlignment(Qt::AlignCenter);
			emptyLabel->setStyleSheet(QString("color: %1; padding: 20px;")
				.arg(StreamUP::UIStyles::Colors::TEXT_MUTED));
			contentLayout->addWidget(emptyLabel);
		} else {
			// Create table with text sources
			QStringList headers = {
				obs_module_text("FontUrlManager.Label.SourceName"),
				obs_module_text("FontUrlManager.Label.FontName"),
				obs_module_text("FontUrlManager.Label.Url")
			};

			QTableWidget* table = StreamUP::UIStyles::CreateStyledTable(headers);
			table->setRowCount(static_cast<int>(textSources.size()));

			// Enable editing for URL column (double-click or select and click)
			table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);

			int row = 0;
			for (const auto& info : textSources) {
				// Source Name (read-only) - also store source pointer in UserRole
				QTableWidgetItem* sourceItem = new QTableWidgetItem(
					QString::fromStdString(info.sourceName));
				sourceItem->setFlags(sourceItem->flags() & ~Qt::ItemIsEditable);
				sourceItem->setData(Qt::UserRole, QVariant::fromValue(static_cast<void*>(info.source)));
				table->setItem(row, 0, sourceItem);

				// Font Name (read-only)
				QTableWidgetItem* fontItem = new QTableWidgetItem(
					QString::fromStdString(info.fontFace));
				fontItem->setFlags(fontItem->flags() & ~Qt::ItemIsEditable);
				table->setItem(row, 1, fontItem);

				// URL (editable)
				QTableWidgetItem* urlItem = new QTableWidgetItem(
					QString::fromStdString(info.currentUrl));
				// Keep default flags (editable)
				table->setItem(row, 2, urlItem);

				row++;
			}

			// Dynamic height calculation: max 10 rows
			int rowCount = table->rowCount();
			int maxVisibleRows = std::min(rowCount, 10);
			int headerHeight = 35;
			int rowHeight = 30;
			int tableHeight = headerHeight + (rowHeight * maxVisibleRows) + 6;

			table->setFixedHeight(tableHeight);
			if (rowCount > 10) {
				table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
			} else {
				table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			}

			// Style table to match dialog
			table->setStyleSheet(
				table->styleSheet() + StreamUP::UIStyles::TABLE_INLINE_STYLESHEET);

			StreamUP::UIStyles::AutoResizeTableColumns(table);
			contentLayout->addWidget(table);

			// Store table for button handlers (sources stored in row data)
			dialog->setProperty("table", QVariant::fromValue(static_cast<void*>(table)));
		}

		// Buttons
		QHBoxLayout *buttonLayout = new QHBoxLayout();
		buttonLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5,
			StreamUP::UIStyles::Sizes::SPACING_MEDIUM,
			StreamUP::UIStyles::Sizes::PADDING_XL + 5,
			StreamUP::UIStyles::Sizes::PADDING_XL);
		buttonLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
		buttonLayout->addStretch();

		if (!textSources.empty()) {
			// Save button
			QPushButton *saveButton = StreamUP::UIStyles::CreateStyledButton(
				obs_module_text("FontUrlManager.Button.Save"), "success");
			QObject::connect(saveButton, &QPushButton::clicked, [dialog]() {
				QTableWidget *table = static_cast<QTableWidget*>(
					dialog->property("table").value<void*>());

				if (table) {
					for (int row = 0; row < table->rowCount(); row++) {
						QTableWidgetItem *sourceItem = table->item(row, 0);
						QTableWidgetItem *urlItem = table->item(row, 2);
						if (sourceItem && urlItem) {
							obs_source_t* source = static_cast<obs_source_t*>(
								sourceItem->data(Qt::UserRole).value<void*>());
							if (source) {
								QString url = urlItem->text().trimmed();
								SetFontUrlOnSource(source, url.toStdString());
							}
						}
					}
				}
				dialog->close();
			});
			buttonLayout->addWidget(saveButton);
		}

		// Cancel/Close button
		QString closeText = textSources.empty()
			? obs_module_text("UI.Button.Close")
			: obs_module_text("FontUrlManager.Button.Cancel");
		QPushButton *cancelButton = StreamUP::UIStyles::CreateStyledButton(
			closeText, "neutral", 30, 100);
		QObject::connect(cancelButton, &QPushButton::clicked, dialog, &QDialog::close);
		buttonLayout->addWidget(cancelButton);

		dialogLayout->addLayout(buttonLayout);
		dialog->setLayout(dialogLayout);

		// Apply auto-sizing
		StreamUP::UIStyles::ApplyAutoSizing(dialog, 550, 850, 200, 500);
	});
}

//-------------------RESIZE AND SCALING FUNCTIONS-------------------
void ResizeAdvancedMaskFilter(obs_source_t *filter, float factor)
{
	if (!StreamUP::ErrorHandler::ValidateSource(filter, "ResizeAdvancedMaskFilter")) {
		return;
	}

	auto settings = OBSWrappers::OBSDataPtr(obs_source_get_settings(filter));
	if (!settings) {
		StreamUP::ErrorHandler::LogError("Failed to get settings for advanced mask filter", StreamUP::ErrorHandler::Category::Source);
		return;
	}

	for (size_t i = 0; i < ADVANCED_MASKS_SETTINGS_SIZE; i++) {
		if (obs_data_has_user_value(settings.get(), advanced_mask_settings[i]))
			obs_data_set_double(settings.get(), advanced_mask_settings[i],
					    obs_data_get_double(settings.get(), advanced_mask_settings[i]) * factor);
	}
	obs_source_update(filter, settings.get());
}

void ResizeMoveSetting(obs_data_t *obs_data, float factor)
{
	if (!obs_data) return;
	
	double x = obs_data_get_double(obs_data, "x");
	obs_data_set_double(obs_data, "x", x * factor);
	double y = obs_data_get_double(obs_data, "y");
	obs_data_set_double(obs_data, "y", y * factor);
	// Note: obs_data passed in is managed externally, don't release here
}

void ResizeMoveValueFilter(obs_source_t *filter, float factor)
{
	if (!StreamUP::ErrorHandler::ValidateSource(filter, "ResizeMoveValueFilter")) {
		return;
	}

	auto settings = OBSWrappers::OBSDataPtr(obs_source_get_settings(filter));
	if (!settings) {
		StreamUP::ErrorHandler::LogError("Failed to get settings for move value filter", StreamUP::ErrorHandler::Category::Source);
		return;
	}
	if (obs_data_get_int(settings.get(), "move_value_type") == 1) {
		for (size_t i = 0; i < ADVANCED_MASKS_SETTINGS_SIZE; i++) {
			if (obs_data_has_user_value(settings.get(), advanced_mask_settings[i]))
				obs_data_set_double(settings.get(), advanced_mask_settings[i],
						    obs_data_get_double(settings.get(), advanced_mask_settings[i]) * factor);
		}
	} else {
		const char *setting_name = obs_data_get_string(settings.get(), "setting_name");
		for (size_t i = 0; i < ADVANCED_MASKS_SETTINGS_SIZE; i++) {
			if (strcmp(setting_name, advanced_mask_settings[i]) == 0) {
				if (obs_data_has_user_value(settings.get(), "setting_float"))
					obs_data_set_double(settings.get(), "setting_float",
							    obs_data_get_double(settings.get(), "setting_float") * factor);
				if (obs_data_has_user_value(settings.get(), "setting_float_min"))
					obs_data_set_double(settings.get(), "setting_float_min",
							    obs_data_get_double(settings.get(), "setting_float_min") * factor);
				if (obs_data_has_user_value(settings.get(), "setting_float_max"))
					obs_data_set_double(settings.get(), "setting_float_max",
							    obs_data_get_double(settings.get(), "setting_float_max") * factor);
			}
		}
	}

	obs_source_update(filter, settings.get());
}

bool IsCloningSceneOrGroup(obs_source_t *source)
{
	if (!source)
		return false;

	const char *input_kind = obs_source_get_id(source);
	if (strcmp(input_kind, "source-clone") != 0)
		return false; // Not a source-clone, no need to check further.

	obs_data_t *source_settings = obs_source_get_settings(source);
	if (!source_settings)
		return false;

	if (obs_data_get_int(source_settings, "clone_type")) {
		obs_data_release(source_settings);
		return true;
	}

	const char *cloned_source_name = obs_data_get_string(source_settings, "clone");
	obs_source_t *cloned_source = obs_get_source_by_name(cloned_source_name);
	if (!cloned_source) {
		obs_data_release(source_settings);
		return false;
	}

	const char *cloned_source_kind = obs_source_get_unversioned_id(cloned_source);
	bool is_scene_or_group = (strcmp(cloned_source_kind, "scene") == 0 || strcmp(cloned_source_kind, "group") == 0);

	obs_source_release(cloned_source);
	obs_data_release(source_settings);

	return is_scene_or_group;
}

void ResizeMoveFilters(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);
	float factor = *((float *)param);

	const char *filter_id = obs_source_get_unversioned_id(child);

	if (strcmp(filter_id, "move_source_filter") == 0) {
		obs_data_t *settings = obs_source_get_settings(child);
		obs_data_t *pos = obs_data_get_obj(settings, "pos");
		ResizeMoveSetting(pos, factor);
		obs_data_release(pos);
		obs_data_t *bounds = obs_data_get_obj(settings, "bounds");
		ResizeMoveSetting(bounds, factor);
		obs_data_release(bounds);
		const char *source_name = obs_data_get_string(settings, "source");
		obs_source_t *source = (source_name && strlen(source_name)) ? obs_get_source_by_name(source_name) : nullptr;
		// Skip resize if cloning a Scene or Group
		if (!obs_scene_from_source(source) && !obs_group_from_source(source) && !IsCloningSceneOrGroup(source)) {
			obs_data_t *scale = obs_data_get_obj(settings, "scale");
			ResizeMoveSetting(scale, factor);
			obs_data_release(scale);
		}
		obs_source_release(source);
		obs_data_set_string(settings, "transform_text", "");
		obs_data_release(settings);
	} else if (strcmp(filter_id, "advanced_masks_filter") == 0) {
		ResizeAdvancedMaskFilter(child, factor);
	} else if (strcmp(filter_id, "move_value_filter") == 0) {
		ResizeMoveValueFilter(child, factor);
	}
}

void ResizeSceneItems(obs_data_t *settings, float factor)
{
	obs_data_array_t *items = obs_data_get_array(settings, "items");
	size_t count = obs_data_array_count(items);

	if (obs_data_get_bool(settings, "custom_size")) {
		obs_data_set_int(settings, "cx", obs_data_get_int(settings, "cx") * factor);
		obs_data_set_int(settings, "cy", obs_data_get_int(settings, "cy") * factor);
	}

	for (size_t i = 0; i < count; i++) {
		obs_data_t *item_data = obs_data_array_item(items, i);
		const char *name = obs_data_get_string(item_data, "name");
		obs_source_t *item_source = obs_get_source_by_name(name);

		vec2 vec2;
		obs_data_get_vec2(item_data, "pos", &vec2);
		vec2.x *= factor;
		vec2.y *= factor;
		obs_data_set_vec2(item_data, "pos", &vec2);
		obs_data_get_vec2(item_data, "bounds", &vec2);
		vec2.x *= factor;
		vec2.y *= factor;
		obs_data_set_vec2(item_data, "bounds", &vec2);

		// Skip resizing if it's a source-clone and cloning a Scene or Group
		if (item_source && (obs_scene_from_source(item_source) || obs_group_from_source(item_source) ||
				    IsCloningSceneOrGroup(item_source))) {
			obs_data_get_vec2(item_data, "scale_ref", &vec2);
			vec2.x *= factor;
			vec2.y *= factor;
			obs_data_set_vec2(item_data, "scale_ref", &vec2);
		} else {
			obs_data_get_vec2(item_data, "scale", &vec2);
			vec2.x *= factor;
			vec2.y *= factor;
			obs_data_set_vec2(item_data, "scale", &vec2);
		}

		obs_source_release(item_source);
		obs_data_release(item_data);
	}

	obs_data_array_release(items);
}

//-------------------PATH CONVERSION FUNCTIONS-------------------
void ConvertSettingPath(obs_data_t *settings, const char *setting_name, const QString &path, const char *sub_folder)
{
	const char *file = obs_data_get_string(settings, setting_name);
	if (!file || !strlen(file))
		return;
	if (QFile::exists(file))
		return;
	const QString file_name = QFileInfo(QT_UTF8(file)).fileName();
	QString filePath = path + "/Resources/" + sub_folder + "/" + file_name;
	if (QFile::exists(filePath)) {
		obs_data_set_string(settings, setting_name, QT_TO_UTF8(filePath));
		return;
	}
	filePath = path + "/" + sub_folder + "/" + file_name;
	if (QFile::exists(filePath)) {
		obs_data_set_string(settings, setting_name, QT_TO_UTF8(filePath));
	}
}

void ConvertFilterPaths(obs_data_t *filter_data, const QString &path)
{
	const char *id = obs_data_get_string(filter_data, "id");
	if (strcmp(id, "shader_filter") == 0) {
		obs_data_t *settings = obs_data_get_obj(filter_data, "settings");
		ConvertSettingPath(settings, "shader_file_name", path, "Shader Filters");
		obs_data_release(settings);
	} else if (strcmp(id, "mask_filter") == 0) {
		obs_data_t *settings = obs_data_get_obj(filter_data, "settings");
		ConvertSettingPath(settings, "image_path", path, "Image Masks");
		obs_data_release(settings);
	}
}

void ConvertSourcePaths(obs_data_t *source_data, const QString &path)
{
	const char *id = obs_data_get_string(source_data, "id");
	if (strcmp(id, "image_source") == 0) {
		obs_data_t *settings = obs_data_get_obj(source_data, "settings");
		ConvertSettingPath(settings, "file", path, "Image Sources");
		obs_data_release(settings);
	} else if (strcmp(id, "ffmpeg_source") == 0) {
		obs_data_t *settings = obs_data_get_obj(source_data, "settings");
		ConvertSettingPath(settings, "local_file", path, "Media Sources");
		obs_data_release(settings);
	}
	obs_data_array_t *filters = obs_data_get_array(source_data, "filters");
	if (!filters)
		return;
	const size_t count = obs_data_array_count(filters);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *filter_data = obs_data_array_item(filters, i);
		ConvertFilterPaths(filter_data, path);
		obs_data_release(filter_data);
	}
	obs_data_array_release(filters);
}

//-------------------SCENE LOADING FUNCTIONS-------------------
void MergeScenes(obs_source_t *s, obs_data_t *scene_settings)
{
	obs_source_save(s);
	obs_data_array_t *items = obs_data_get_array(scene_settings, "items");
	if (!items)
		return;
	const size_t item_count = obs_data_array_count(items);
	obs_data_t *scene_settings_orig = obs_source_get_settings(s);
	if (!scene_settings_orig) {
		obs_data_array_release(items);
		return;
	}
	obs_data_array_t *items_orig = obs_data_get_array(scene_settings_orig, "items");
	if (!items_orig) {
		obs_data_release(scene_settings_orig);
		obs_data_array_release(items);
		return;
	}
	const size_t item_count_orig = obs_data_array_count(items_orig);
	for (size_t j = 0; j < item_count_orig; j++) {
		obs_data_t *item_data_orig = obs_data_array_item(items_orig, j);
		const char *name_orig = obs_data_get_string(item_data_orig, "name");
		bool found = false;
		for (size_t k = 0; k < item_count; k++) {
			obs_data_t *item_data = obs_data_array_item(items, k);
			const char *name = obs_data_get_string(item_data, "name");
			if (strcmp(name, name_orig) == 0) {
				found = true;
				obs_data_release(item_data);
				break;
			}
			obs_data_release(item_data);
		}
		if (!found) {
			obs_data_array_push_back(items, item_data_orig);
		}
		obs_data_release(item_data_orig);
	}
	obs_data_array_release(items_orig);
	obs_data_release(scene_settings_orig);
	obs_data_array_release(items);
}

void MergeFilters(obs_source_t *s, obs_data_array_t *filters)
{
	size_t count = obs_data_array_count(filters);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *filter_data = obs_data_array_item(filters, i);
		const char *filter_name = obs_data_get_string(filter_data, "name");
		obs_source_t *filter = obs_source_get_filter_by_name(s, filter_name);
		if (filter) {
			obs_data_release(filter_data);
			obs_source_release(filter);
			continue;
		}
		filter = obs_load_private_source(filter_data);
		if (filter) {
			obs_source_filter_add(s, filter);
			obs_source_release(filter);
		}
		obs_data_release(filter_data);
	}

	// Note: caller owns the filters array lifecycle - do not release here
}

void LoadSources(obs_data_array_t *data, const QString &path)
{
	const size_t count = obs_data_array_count(data);
	std::list<obs_source_t *> ref_sources;
	std::list<obs_source_t *> load_sources;
	obs_source_t *cs = obs_frontend_get_current_scene();
	uint32_t w = obs_source_get_width(cs);
	float factor = (float)w / 1920.0f;
	obs_source_release(cs);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *sourceData = obs_data_array_item(data, i);
		const char *name = obs_data_get_string(sourceData, "name");

		bool new_source = true;
		obs_source_t *s = obs_get_source_by_name(name);
		if (!s) {
			ConvertSourcePaths(sourceData, path);
			s = obs_load_source(sourceData);
			load_sources.push_back(s);
		} else {
			new_source = false;
			obs_data_array_t *filters = obs_data_get_array(sourceData, "filters");
			if (filters) {
				MergeFilters(s, filters);
				obs_data_array_release(filters);
			}
		}
		if (s)
			ref_sources.push_back(s);
		obs_scene_t *scene = obs_scene_from_source(s);
		if (!scene)
			scene = obs_group_from_source(s);
		if (scene) {
			obs_data_t *scene_settings = obs_data_get_obj(sourceData, "settings");
			if (w != 1920) {
				ResizeSceneItems(scene_settings, factor);
				if (new_source)
					obs_source_enum_filters(s, ResizeMoveFilters, &factor);
			}
			if (!new_source) {
				obs_scene_enum_items(
					scene,
					[](obs_scene_t *, obs_sceneitem_t *item, void *d) {
						std::list<obs_source_t *> *sources = (std::list<obs_source_t *> *)d;
						obs_source_t *si = obs_sceneitem_get_source(item);
						si = obs_source_get_ref(si);
						if (si)
							sources->push_back(si);
						return true;
					},
					&ref_sources);
				MergeScenes(s, scene_settings);
			}
			obs_source_update(s, scene_settings);
			obs_data_release(scene_settings);
			if (!new_source) {
				// Only push existing scenes; new sources were already added above
				load_sources.push_back(s);
			}
		}
		obs_data_release(sourceData);
	}

	for (obs_source_t *source : load_sources)
		obs_source_load(source);

	for (obs_source_t *source : ref_sources)
		obs_source_release(source);
}

void LoadScene(obs_data_t *data, const QString &path)
{
	if (!data)
		return;
	obs_data_array_t *sourcesData = obs_data_get_array(data, "sources");
	if (!sourcesData)
		return;
	LoadSources(sourcesData, path);
	obs_data_array_release(sourcesData);
}

//-------------------MAIN LOADING FUNCTIONS-------------------
bool LoadStreamupFileFromPath(const QString &file_path, bool forceLoad)
{
	if (!forceLoad) {
		if (!StreamUP::PluginManager::CheckrequiredOBSPlugins(true)) {
			return false;
		}
	}

	// Validate file path first
	if (!StreamUP::ErrorHandler::ValidateFile(file_path)) {
		return false;
	}

	// Load the .streamup file from the file path
	auto data = OBSWrappers::MakeOBSDataFromJsonFile(QT_TO_UTF8(file_path));
	if (data) {
		StreamUP::ErrorHandler::LogInfo("Successfully loaded StreamUP file: " + file_path.toStdString(), StreamUP::ErrorHandler::Category::FileSystem);
		LoadScene(data.get(), QFileInfo(file_path).absolutePath());
		return true;
	} else {
		StreamUP::ErrorHandler::LogError("Failed to parse StreamUP file: " + file_path.toStdString(), StreamUP::ErrorHandler::Category::FileSystem);
	}

	return false;
}

void LoadStreamupFile(bool forceLoad)
{
	if (!forceLoad) {
		if (!StreamUP::PluginManager::CheckrequiredOBSPlugins(true)) {
			return;
		}
	}

	QString fileName =
		QFileDialog::getOpenFileName(nullptr, QT_UTF8(obs_module_text("UI.Button.Load")), QString(), "StreamUP File (*.streamup)");
	if (!fileName.isEmpty()) {
		LoadStreamupFileFromPath(fileName, forceLoad);
	}
}

void LoadStreamupFileWithWarning()
{
	// Use cached plugin status for instant response
	if (StreamUP::PluginManager::IsAllPluginsUpToDateCached()) {
		// Plugins are OK, show file selector
		QString fileName = QFileDialog::getOpenFileName(
			nullptr, QT_UTF8(obs_module_text("UI.Button.Load")),
			QString(), "StreamUP File (*.streamup)");

		if (!fileName.isEmpty()) {
			// Check fonts in selected file
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

	// Plugins have issues, show cached plugin issues dialog first
	// On continue, allow loading with font check
	StreamUP::PluginManager::ShowCachedPluginIssuesDialog([]() {
		// After user clicks Continue Anyway for plugins, show file selector with font check
		QString fileName = QFileDialog::getOpenFileName(
			nullptr, QT_UTF8(obs_module_text("UI.Button.Load")),
			QString(), "StreamUP File (*.streamup)");

		if (!fileName.isEmpty()) {
			std::vector<FontInfo> fonts = ExtractFontsFromStreamupFile(fileName);
			std::vector<FontInfo> missingFonts = CheckFontAvailability(fonts);

			if (missingFonts.empty()) {
				LoadStreamupFileFromPath(fileName, true);
			} else {
				ShowMissingFontsDialog(missingFonts, [fileName]() {
					LoadStreamupFileFromPath(fileName, true);
				});
			}
		}
	});
}

} // namespace FileManager
} // namespace StreamUP
