#include "module-setup-wizard.hpp"
#include "settings-manager.hpp"
#include "ui-helpers.hpp"
#include <streamup/debug-logger.hpp>
#include "../version.h"

#include <streamup/ui/window-chrome.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
#include <streamup/ui/switch-button.hpp>

#include <obs-module.h>

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QWidget>
#include <QPointer>

#include <functional>

using namespace StreamUP::UIStyles;

namespace StreamUP {
namespace ModuleSetupWizard {

namespace {

// Each row knows how to read its current state from the draft and how to
// write a new state back. Function-based instead of a raw bool* so the
// merged "StreamUP OBS Theme Enhancements" row can update three fields at
// once and so future combined toggles need no new plumbing.
struct PluginRow {
	const char *titleKey;
	const char *descKey;
	std::function<bool(const StreamUP::SettingsManager::ModuleSettings &)> getter;
	std::function<void(StreamUP::SettingsManager::ModuleSettings &, bool)> setter;
	StreamUP::UIStyles::SwitchButton *toggle; // Filled in during build
};

void AddRow(QVBoxLayout *parent,
            PluginRow &row,
            std::shared_ptr<StreamUP::SettingsManager::ModuleSettings> draft,
            bool addDivider)
{
	QWidget *rowWidget = new QWidget();
	QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
	// Generous vertical padding per row so descriptions don't run together.
	rowLayout->setContentsMargins(0, S(12), 0, S(12));
	rowLayout->setSpacing(S(16));

	QWidget *textBlock = new QWidget();
	QVBoxLayout *textLayout = new QVBoxLayout(textBlock);
	textLayout->setContentsMargins(0, 0, 0, 0);
	textLayout->setSpacing(S(8));

	QLabel *titleLabel = new QLabel(obs_module_text(row.titleKey));
	titleLabel->setStyleSheet(StreamUP::UIStyles::scale_qss(QString("color: %1; font-size: 15px; font-weight: 600; background: transparent;")
		.arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)));

	QLabel *descLabel = new QLabel(obs_module_text(row.descKey));
	descLabel->setWordWrap(true);
	descLabel->setStyleSheet(StreamUP::UIStyles::scale_qss(QString("color: %1; font-size: %2px; background: transparent; line-height: 1.4;")
		.arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)
		.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL)));

	textLayout->addWidget(titleLabel);
	textLayout->addWidget(descLabel);
	rowLayout->addWidget(textBlock, 1);

	row.toggle = StreamUP::UIStyles::CreateStyledSwitch("", row.getter(*draft));
	QObject::connect(row.toggle, &StreamUP::UIStyles::SwitchButton::toggled,
		[setter = row.setter, draft](bool checked) { setter(*draft, checked); });
	// Vertically centre the switch against the title/description block.
	rowLayout->addWidget(row.toggle, 0, Qt::AlignVCenter);

	parent->addWidget(rowWidget);

	// Thin divider between rows for visual separation. Skipped on the last
	// row of a section so the group box's bottom edge does double duty.
	if (addDivider) {
		QFrame *separator = new QFrame();
		separator->setFrameShape(QFrame::HLine);
		separator->setFrameShadow(QFrame::Plain);
		separator->setStyleSheet(StreamUP::UIStyles::scale_qss("QFrame { background-color: rgba(127, 132, 156, 0.2); "
		                         "border: none; max-height: 1px; }"));
		parent->addWidget(separator);
	}
}

} // namespace

void Show(std::function<void()> onFinished)
{
	StreamUP::UIHelpers::ShowSingletonDialogOnUIThread("module-setup-wizard", [onFinished]() -> QDialog * {
		// Working copy of the module settings that the wizard mutates. Copied
		// from the live settings so existing values (e.g. from an upgrade
		// path) are preserved as defaults.
		auto draft = std::make_shared<StreamUP::SettingsManager::ModuleSettings>(
			StreamUP::SettingsManager::GetCurrentSettings().modules);

		// brandFooter=true → primary window with the StreamUP brand line.
		WindowShell shell = makeWindow(obs_module_text("PluginWizard.Window.Title"), "v" PROJECT_VERSION,
		                               nullptr, /*brandFooter=*/true, "StreamUP");
		QDialog *dialog = shell.dialog;
		dialog->resize(S(720) + 2 * S(ShadowDialog::kShadowMargin), S(760) + 2 * S(ShadowDialog::kShadowMargin));

		QVBoxLayout *mainLayout = shell.content;
		mainLayout->setContentsMargins(S(20), S(20), S(20), S(20));
		mainLayout->setSpacing(S(16));

		// Headline.
		QLabel *headline = new QLabel(obs_module_text("PluginWizard.Headline"));
		headline->setStyleSheet(StreamUP::UIStyles::scale_qss(QString("color: %1; font-size: 22px; font-weight: 700; background: transparent;")
			.arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)));
		mainLayout->addWidget(headline);

		// Welcome line — short, sets the frame.
		QLabel *welcome = new QLabel(obs_module_text("PluginWizard.Welcome"));
		welcome->setWordWrap(true);
		welcome->setStyleSheet(StreamUP::UIStyles::scale_qss(QString("color: %1; font-size: %2px; background: transparent;")
			.arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)
			.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL)));
		mainLayout->addWidget(welcome);

		// Resource reassurance — visually distinct, sits in a tinted callout
		// box so people who skim still notice it.
		QLabel *resourceNote = new QLabel(obs_module_text("PluginWizard.ResourceNote"));
		resourceNote->setWordWrap(true);
		resourceNote->setStyleSheet(StreamUP::UIStyles::scale_qss(QString(
			"QLabel { color: %1; font-size: %2px; background-color: rgba(137, 220, 235, 0.08); "
			"border-left: 3px solid rgba(137, 220, 235, 0.6); border-radius: 4px; "
			"padding: 10px 12px; }"
		).arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
		 .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL)));
		mainLayout->addWidget(resourceNote);

		// Usage hint — small italic line.
		QLabel *usageHint = new QLabel(obs_module_text("PluginWizard.UsageHint"));
		usageHint->setWordWrap(true);
		usageHint->setStyleSheet(StreamUP::UIStyles::scale_qss(QString("color: %1; font-size: %2px; background: transparent; font-style: italic;")
			.arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
			.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL)));
		mainLayout->addWidget(usageHint);

		// Scrollable area for the plugin list (the dialog can be small on
		// laptops, so don't trust everything fits without a scroll).
		QScrollArea *scrollArea = new QScrollArea();
		scrollArea->setWidgetResizable(true);
		scrollArea->setFrameShape(QFrame::NoFrame);
		useScrollBars(scrollArea);
		scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

		QWidget *contentContainer = new QWidget();
		contentContainer->setStyleSheet("QWidget { background: transparent; }");
		QVBoxLayout *contentLayout = new QVBoxLayout(contentContainer);
		contentLayout->setContentsMargins(0, 0, 0, 0);
		contentLayout->setSpacing(S(16));

		using Mod = StreamUP::SettingsManager::ModuleSettings;

		// The wizard groups modules exactly like Settings > Plugins does, and
		// carries the same rows. Someone who picks here and later revisits the
		// settings page should recognise the same three headings in the same
		// order, with nothing appearing in one place but not the other.
		auto rows = std::make_shared<std::vector<PluginRow>>();

		// Each section is a header plus a plain widget — no group frame, the
		// header does the separating.
		struct Section {
			const char *headerKey;
			QVBoxLayout *layout;
			size_t endRow; // one past the last row belonging to this section
		};
		std::vector<Section> sections;

		auto beginSection = [&](const char *headerKey) {
			QWidget *widget = new QWidget();
			QVBoxLayout *layout = new QVBoxLayout(widget);
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setSpacing(0);
			contentLayout->addWidget(sectionHeader(obs_module_text(headerKey)));
			contentLayout->addWidget(widget);
			sections.push_back({headerKey, layout, 0});
		};
		auto endSection = [&]() { sections.back().endRow = rows->size(); };

		// ── Docks and panels ─────────────────────────────────────────────
		beginSection("Plugins.Section.Docks");
		rows->push_back({"Plugins.SceneOrganiser.Title","Plugins.SceneOrganiser.Description",
		                 [](const Mod &m){ return m.sceneOrganiser; },
		                 [](Mod &m, bool v){ m.sceneOrganiser = v; }, nullptr});
		rows->push_back({"Plugins.StreamupDock.Title",  "Plugins.StreamupDock.Description",
		                 [](const Mod &m){ return m.streamupDock; },
		                 [](Mod &m, bool v){ m.streamupDock = v; }, nullptr});
		rows->push_back({"Plugins.MultiDock.Title",     "Plugins.MultiDock.Description",
		                 [](const Mod &m){ return m.multiDock; },
		                 [](Mod &m, bool v){ m.multiDock = v; }, nullptr});
		endSection();

		// ── Ways to control OBS ──────────────────────────────────────────
		// No theme enhancements row: those always run and self-gate on
		// whether a StreamUP theme is active. No WebSocket row either, the
		// vendor is always registered.
		beginSection("Plugins.Section.Controls");
		rows->push_back({"Plugins.Toolbar.Title",       "Plugins.Toolbar.Description",
		                 [](const Mod &m){ return m.toolbar; },
		                 [](Mod &m, bool v){ m.toolbar = v; }, nullptr});
		rows->push_back({"Plugins.Hotkeys.Title",       "Plugins.Hotkeys.Description",
		                 [](const Mod &m){ return m.hotkeys; },
		                 [](Mod &m, bool v){ m.hotkeys = v; }, nullptr});
		endSection();

		// ── Tools and sources ────────────────────────────────────────────
		beginSection("Plugins.Section.Tools");
		rows->push_back({"Plugins.Backup.Title",        "Plugins.Backup.Description",
		                 [](const Mod &m){ return m.backup; },
		                 [](Mod &m, bool v){ m.backup = v; }, nullptr});
		rows->push_back({"Plugins.AdjustmentLayer.Title", "Plugins.AdjustmentLayer.Description",
		                 [](const Mod &m){ return m.adjustmentLayerSource; },
		                 [](Mod &m, bool v){ m.adjustmentLayerSource = v; }, nullptr});
		endSection();

		size_t rowIndex = 0;
		for (const Section &section : sections) {
			for (; rowIndex < section.endRow; ++rowIndex) {
				// Divider after every row except the last in the section,
				// where the next header does the separating.
				bool isLastInSection = (rowIndex + 1 == section.endRow);
				AddRow(section.layout, (*rows)[rowIndex], draft, !isLastInSection);
			}
		}

		contentLayout->addStretch();

		scrollArea->setWidget(contentContainer);
		mainLayout->addWidget(scrollArea, 1);

		// Footer buttons — right-anchored, inline with the brand line.
		QPushButton *skipButton = new PillButton(obs_module_text("PluginWizard.SkipForNow"), "outline");
		QPushButton *resetButton = new PillButton(obs_module_text("PluginWizard.RecommendedDefaults"), "neutral");
		QPushButton *saveButton = new PillButton(obs_module_text("PluginWizard.SaveAndContinue"), "primary");

		// "Recommended defaults" = everything on, which mirrors the struct default ctor.
		QObject::connect(resetButton, &QPushButton::clicked, [rows, draft]() {
			for (PluginRow &row : *rows) {
				if (row.toggle) {
					row.toggle->setChecked(true);
				}
				row.setter(*draft, true);
			}
		});

		// "Skip for now" leaves the draft as the user has it (or untouched at all-on
		// defaults) and saves it. Same persistence behaviour as Save — the only
		// difference is intent. We still mark setup complete so the wizard does not
		// reappear next launch.
		QObject::connect(skipButton, &QPushButton::clicked, [dialog]() { dialog->accept(); });
		QObject::connect(saveButton, &QPushButton::clicked, [dialog]() { dialog->accept(); });

		shell.footerButtons->addWidget(skipButton);
		shell.footerButtons->addWidget(resetButton);
		shell.footerButtons->addWidget(saveButton);

		// Persist on any close path (Save, Skip, or user closing the window).
		// Stamps the current PROJECT_VERSION so this exact build's wizard does
		// not show again. A future build that bumps PROJECT_VERSION will fire
		// the wizard once for everyone, fresh installs and upgraders alike.
		QObject::connect(dialog, &QDialog::finished, [draft, onFinished](int) {
			StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
			settings.modules = *draft;
			settings.moduleSetupComplete = true;
			settings.wizardVersionShown = PROJECT_VERSION;
			StreamUP::SettingsManager::UpdateSettings(settings);
			StreamUP::DebugLogger::LogInfo("ModuleWizard", "Plugin picker finished, settings saved (version stamped)");
			if (onFinished) {
				onFinished();
			}
		});

		dialog->show();
		return dialog;
	});
}

} // namespace ModuleSetupWizard
} // namespace StreamUP
