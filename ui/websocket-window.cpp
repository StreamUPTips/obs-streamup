#include "websocket-window.hpp"
#include "ui-helpers.hpp"
#include "streamup-kit.hpp"
#include <streamup/ui/search-field.hpp>
#include <streamup/ui/segmented-control.hpp>
#include <streamup/ui/window-chrome.hpp> // ShadowDialog, makeWindow, WindowShell
#include <streamup/ui/pill-button.hpp>   // PillButton
#include <streamup/ui/labels.hpp>        // makeLabel, sectionHeader
#include <streamup/ui/ui-scrollbar.hpp>  // useScrollBars
#include "notification-manager.hpp"
#include "settings-manager.hpp"
#include "../version.h"
#include <obs-module.h>
#include <QApplication>
#include <QScrollArea>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QClipboard>
#include <QFrame>
#include <QTimer>
#include <QPointer>

namespace StreamUP {
namespace WebSocketWindow {

// 0: OBS Raw JSON, 1: Streamer.bot code. Picked at the top of the window.
static int &CopyKind()
{
	static int kind = 0;
	return kind;
}

// WebSocket dialogs are now managed by DialogManager in ui-helpers

struct WebSocketCommand {
	QString name;
	QString description;
	QString category;
	bool isInternalTool = false; // Mark commands that are internal tools
};

// Define all available WebSocket commands organized by category
static const QList<WebSocketCommand> websocketCommands = {
	// Utility Commands
	{"GetStreamBitrate", "WebSocket.Command.GetStreamBitrate.Description", "WebSocket.Category.Utility", false},
	{"GetPluginVersion", "WebSocket.Command.GetPluginVersion.Description", "WebSocket.Category.Utility", true}, // Internal tool

	// Plugin Management
	{"CheckRequiredPlugins", "WebSocket.Command.CheckRequiredPlugins.Description", "WebSocket.Category.PluginManagement",
	 true}, // Internal tool

	// Source Management
	{"ToggleLockAllSources", "WebSocket.Command.ToggleLockAllSources.Description", "WebSocket.Category.SourceManagement",
	 false},
	{"ToggleLockCurrentSceneSources", "WebSocket.Command.ToggleLockCurrentSceneSources.Description",
	 "WebSocket.Category.SourceManagement", false},
	{"RefreshAudioMonitoring", "WebSocket.Command.RefreshAudioMonitoring.Description", "WebSocket.Category.SourceManagement",
	 false},
	{"RefreshBrowserSources", "WebSocket.Command.RefreshBrowserSources.Description", "WebSocket.Category.SourceManagement",
	 false},
	{"GetSelectedSource", "WebSocket.Command.GetSelectedSource.Description", "WebSocket.Category.SourceManagement", false},

	// Transition Management
	{"GetShowTransition", "WebSocket.Command.GetShowTransition.Description", "WebSocket.Category.TransitionManagement", false},
	{"GetHideTransition", "WebSocket.Command.GetHideTransition.Description", "WebSocket.Category.TransitionManagement", false},
	{"SetShowTransition", "WebSocket.Command.SetShowTransition.Description", "WebSocket.Category.TransitionManagement", false},
	{"SetHideTransition", "WebSocket.Command.SetHideTransition.Description", "WebSocket.Category.TransitionManagement", false},
	{"CopyShowTransition", "WebSocket.Command.CopyShowTransition.Description", "WebSocket.Category.TransitionManagement", false},
	{"CopyHideTransition", "WebSocket.Command.CopyHideTransition.Description", "WebSocket.Category.TransitionManagement", false},
	{"PasteShowTransition", "WebSocket.Command.PasteShowTransition.Description", "WebSocket.Category.TransitionManagement", false},
	{"PasteHideTransition", "WebSocket.Command.PasteHideTransition.Description", "WebSocket.Category.TransitionManagement", false},

	// Source Properties
	{"GetBlendingMethod", "WebSocket.Command.GetBlendingMethod.Description", "WebSocket.Category.SourceProperties", false},
	{"SetBlendingMethod", "WebSocket.Command.SetBlendingMethod.Description", "WebSocket.Category.SourceProperties", false},
	{"GetDeinterlacing", "WebSocket.Command.GetDeinterlacing.Description", "WebSocket.Category.SourceProperties", false},
	{"SetDeinterlacing", "WebSocket.Command.SetDeinterlacing.Description", "WebSocket.Category.SourceProperties", false},
	{"GetScaleFiltering", "WebSocket.Command.GetScaleFiltering.Description", "WebSocket.Category.SourceProperties", false},
	{"SetScaleFiltering", "WebSocket.Command.SetScaleFiltering.Description", "WebSocket.Category.SourceProperties", false},
	{"GetDownmixMono", "WebSocket.Command.GetDownmixMono.Description", "WebSocket.Category.SourceProperties", false},
	{"SetDownmixMono", "WebSocket.Command.SetDownmixMono.Description", "WebSocket.Category.SourceProperties", false},

	// File Management
	{"GetRecordingOutputPath", "WebSocket.Command.GetRecordingOutputPath.Description", "WebSocket.Category.FileManagement",
	 true}, // Internal tool
	{"GetVLCCurrentFile", "WebSocket.Command.GetVLCCurrentFile.Description", "WebSocket.Category.FileManagement", false},
	{"LoadStreamUpFile", "WebSocket.Command.LoadStreamUpFile.Description", "WebSocket.Category.FileManagement",
	 true}, // Internal tool

	// UI Interaction
	{"OpenSourceProperties", "WebSocket.Command.OpenSourceProperties.Description", "WebSocket.Category.UIInteraction", false},
	{"OpenSourceFilters", "WebSocket.Command.OpenSourceFilters.Description", "WebSocket.Category.UIInteraction", false},
	{"OpenSourceInteraction", "WebSocket.Command.OpenSourceInteraction.Description", "WebSocket.Category.UIInteraction", false},
	{"OpenSceneFilters", "WebSocket.Command.OpenSceneFilters.Description", "WebSocket.Category.UIInteraction", false},

	// Video Capture Device Management
	{"ActivateAllVideoCaptureDevices", "WebSocket.Command.ActivateAllVideoCaptureDevices.Description",
	 "WebSocket.Category.VideoDeviceManagement", false},
	{"DeactivateAllVideoCaptureDevices", "WebSocket.Command.DeactivateAllVideoCaptureDevices.Description",
	 "WebSocket.Category.VideoDeviceManagement", false},
	{"RefreshAllVideoCaptureDevices", "WebSocket.Command.RefreshAllVideoCaptureDevices.Description",
	 "WebSocket.Category.VideoDeviceManagement", false},

	// Group and Visibility Management
	{"GroupSelectedSources", "WebSocket.Command.GroupSelectedSources.Description",
	 "WebSocket.Category.GroupManagement", false},
	{"ToggleVisibilitySelectedSources", "WebSocket.Command.ToggleVisibilitySelectedSources.Description",
	 "WebSocket.Category.GroupManagement", false}};

void ShowWebSocketWindow(bool showInternalTools)
{
	std::string dialogId = showInternalTools ? "websocket-internal" : "websocket-normal";
	StreamUP::UIHelpers::ShowSingletonDialogOnUIThread(dialogId, [showInternalTools]() -> QDialog* {
		StreamUP::UIStyles::WindowShell shell =
			StreamUP::UIStyles::makeWindow(obs_module_text("WebSocket.Window.Title"), "v" PROJECT_VERSION,
						       nullptr, /*brandFooter=*/true, "StreamUP");
		QDialog *dialog = shell.dialog;

		// Start with compact size - will expand based on content
		dialog->resize(StreamUP::UIStyles::S(700) + 2 * StreamUP::UIStyles::S(StreamUP::UIStyles::ShadowDialog::kShadowMargin),
			       StreamUP::UIStyles::S(700) + 2 * StreamUP::UIStyles::S(StreamUP::UIStyles::ShadowDialog::kShadowMargin));

		QVBoxLayout *mainLayout = shell.content;
		mainLayout->setContentsMargins(StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(16),
					       StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(16));

		// Modern unified content area with scroll - everything inside
		QScrollArea *scrollArea = new QScrollArea();
		scrollArea->setWidgetResizable(true);
		scrollArea->setFrameShape(QFrame::NoFrame);
		StreamUP::UIStyles::useScrollBars(scrollArea);

		QWidget *contentWidget = new QWidget();
		contentWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
		QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
		contentLayout->setContentsMargins(StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(20),
						  StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(20));
		contentLayout->setSpacing(StreamUP::UIStyles::S(20));

		// Search, and which kind of code the Copy buttons give you.
		auto *tools = new QHBoxLayout();
		tools->setSpacing(StreamUP::UIStyles::S(10));
		auto *search = new StreamUP::UIStyles::SearchField();
		search->setPlaceholderText(obs_module_text("WebSocket.Search"));
		search->setAccessibleName(obs_module_text("WebSocket.Search"));
		search->setFixedHeight(StreamUP::UIStyles::S(30));
		tools->addWidget(search, 1);
		if (StreamUP::SettingsManager::IsCPHIntegrationEnabled()) {
			auto *kind = new StreamUP::UIStyles::SegmentedControl(
				{obs_module_text("WebSocket.Kind.Raw"), obs_module_text("WebSocket.Kind.StreamerBot")});
			kind->setFixedWidth(StreamUP::UIStyles::S(220));
			kind->setCurrentIndex(0);
			kind->onChanged([](int i) { CopyKind() = i; });
			tools->addWidget(kind);
		}
		CopyKind() = 0;
		contentLayout->addLayout(tools);
		QList<QPair<QWidget *, QString>> rows;    // command row, lower-case name and description
		QList<QWidget *> separators;              // hairlines between rows; hidden while searching
		QList<QPair<QWidget *, QWidget *>> cards; // section header, card

		// Group commands by category, filtering out internal tools if Shift is not held
		QMap<QString, QList<WebSocketCommand>> commandsByCategory;
		for (const auto &cmd : websocketCommands) {
			// Skip internal tools unless Shift key was held
			if (cmd.isInternalTool && !showInternalTools) {
				continue;
			}
			QString translatedCategory = obs_module_text(cmd.category.toUtf8().constData());
			commandsByCategory[translatedCategory].append(cmd);
		}

		// Create sections for each category
		QStringList categoryOrder = {obs_module_text("WebSocket.Category.Utility"),
					     obs_module_text("WebSocket.Category.PluginManagement"),
					     obs_module_text("WebSocket.Category.SourceManagement"),
					     obs_module_text("WebSocket.Category.SourceProperties"),
					     obs_module_text("WebSocket.Category.TransitionManagement"),
					     obs_module_text("WebSocket.Category.FileManagement"),
					     obs_module_text("WebSocket.Category.UIInteraction"),
					     obs_module_text("WebSocket.Category.VideoDeviceManagement")};

		for (const QString &category : categoryOrder) {
			if (!commandsByCategory.contains(category) || commandsByCategory[category].isEmpty())
				continue;

			// Section header with bottom divider — cleaner than a groupbox and
			// reads as a proper section break
			QLabel *sectionLabel = StreamUP::UIStyles::sectionHeader(category);
			contentLayout->addWidget(sectionLabel);

			// Commands container — rounded card so the section reads as a distinct panel
			QFrame *categoryCard = new QFrame();
			categoryCard->setStyleSheet(StreamUP::UIStyles::scale_qss(QString(
				"QFrame { background: %1; border-radius: %2px; border: 1px solid %3; }")
				.arg(StreamUP::UIStyles::Colors::BG_PRIMARY)
				.arg(10)
				.arg(StreamUP::UIStyles::Colors::BORDER_SUBTLE)));
			QVBoxLayout *categoryLayout = new QVBoxLayout(categoryCard);
			categoryLayout->setContentsMargins(StreamUP::UIStyles::S(12), 0,
							   StreamUP::UIStyles::S(12), 0);
			categoryLayout->setSpacing(0);

			// Add commands for this category
			const auto &commands = commandsByCategory[category];
			for (int i = 0; i < commands.size(); ++i) {
				const auto &cmd = commands[i];
				QString translatedDescription = obs_module_text(cmd.description.toUtf8().constData());
				QWidget *commandWidget = CreateCommandWidget(cmd.name, translatedDescription);
				categoryLayout->addWidget(commandWidget);
				commandWidget->setProperty("su_card", QVariant::fromValue<QObject *>(categoryCard));
				// Rows keep their own height when a search hides their neighbours.
				commandWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
				rows.append({commandWidget, (cmd.name + " " + translatedDescription).toLower()});

				// Add separator line between commands (but not after the last one)
				if (i < commands.size() - 1) {
					QFrame *separator = new QFrame();
					separator->setFrameShape(QFrame::HLine);
					separator->setFrameShadow(QFrame::Plain);
					separator->setStyleSheet(StreamUP::UIStyles::scale_qss(QString("QFrame {"
									 "color: rgba(113, 128, 150, 0.3);"
									 "background-color: rgba(113, 128, 150, 0.3);"
									 "border: none;"
									 "margin: 0px;"
									 "max-height: 1px;"
									 "}")));
					categoryLayout->addWidget(separator);
					separators.append(separator);
				}
			}

			categoryCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
			contentLayout->addWidget(categoryCard);
			cards.append({sectionLabel, categoryCard});
		}

		// Filter rows as you type; a section with nothing left hides.
		QObject::connect(search, &QLineEdit::textChanged, dialog, [rows, cards, separators](const QString &text) {
			const QString needle = text.trimmed().toLower();
			for (QWidget *line : separators)
				line->setVisible(needle.isEmpty());
			for (const auto &r : rows)
				r.first->setVisible(needle.isEmpty() || r.second.contains(needle));
			for (const auto &c : cards) {
				bool any = false;
				for (const auto &r : rows)
					if (r.first->property("su_card").value<QObject *>() == c.second && r.first->isVisibleTo(c.second))
						any = true;
				c.first->setVisible(any);
				c.second->setVisible(any);
			}
		});

		// Add stretch to push content to top
		contentLayout->addStretch();

		scrollArea->setWidget(contentWidget);
		mainLayout->addWidget(scrollArea);

		// Close button in footer (right-anchored, inline with brand line)
		QPushButton *closeButton = new StreamUP::UIStyles::PillButton(obs_module_text("WebSocket.Button.Close"), "outline");
		QObject::connect(closeButton, &QPushButton::clicked, [dialog]() { dialog->close(); });
		shell.footerButtons->addWidget(closeButton);

		// Sizing for websocket window (preferred width + reasonable height)
		dialog->resize(StreamUP::UIStyles::S(700), StreamUP::UIStyles::S(700));
		dialog->show();
		return dialog;
	});
}

QWidget *CreateCommandWidget(const QString &command, const QString &description)
{
	QWidget *widget = new QWidget();
	widget->setStyleSheet(StreamUP::UIStyles::scale_qss(QString("QWidget {"
				      "background: transparent;"
				      "border: none;"
				      "padding: 0px;" // No padding on the widget itself
				      "}")));

	QHBoxLayout *layout = new QHBoxLayout(widget);
	layout->setContentsMargins(0, StreamUP::UIStyles::S(6), 0, StreamUP::UIStyles::S(6));
	layout->setSpacing(StreamUP::UIStyles::S(12));

	// Command info section - use vertical layout but center it
	QVBoxLayout *infoLayout = new QVBoxLayout();
	infoLayout->setSpacing(StreamUP::UIStyles::S(2)); // Tight spacing between name and description
	infoLayout->setContentsMargins(0, 0, 0, 0);

	// Command name
	QLabel *nameLabel = new QLabel(command);
	nameLabel->setStyleSheet(StreamUP::UIStyles::scale_qss(QString("QLabel {"
					 "color: %1;"
					 "font-size: %2px;"
					 "font-weight: 600;"
					 "background: transparent;"
					 "border: none;"
					 "margin: 0px;"
					 "padding: 0px;"
					 "}")
					 .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
					 .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL)));

	// One line: the name, with what it does behind "?".
	auto *nameRow = new QHBoxLayout();
	nameRow->setSpacing(StreamUP::UIStyles::S(6));
	nameRow->addWidget(nameLabel);
	if (QLabel *dot = StreamUP::Kit::HelpDot(description))
		nameRow->addWidget(dot);
	nameRow->addStretch(1);
	infoLayout->addLayout(nameRow);

	// Create a wrapper widget for info that centers the content vertically
	QWidget *infoWrapper = new QWidget();
	QVBoxLayout *wrapperLayout = new QVBoxLayout(infoWrapper);
	wrapperLayout->setContentsMargins(0, 0, 0, 0);
	wrapperLayout->addStretch(); // Add stretch above
	wrapperLayout->addLayout(infoLayout);
	wrapperLayout->addStretch(); // Add stretch below

	// Button section - also center vertically
	QVBoxLayout *buttonWrapperLayout = new QVBoxLayout();
	buttonWrapperLayout->setContentsMargins(0, 0, 0, 0);
	buttonWrapperLayout->addStretch(); // Add stretch above

	QHBoxLayout *buttonLayout = new QHBoxLayout();
	buttonLayout->setSpacing(StreamUP::UIStyles::S(8));
	buttonLayout->setContentsMargins(0, 0, 0, 0);

	// One Copy button. It copies OBS Raw JSON, or Streamer.bot code when that
	// is picked at the top of the window.
	const QString obsRawJson =
		QString(R"({"requestType":"CallVendorRequest","requestData":{"vendorName":"streamup","requestType":"%1","requestData":{}}})")
			.arg(command);
	const QString cphCommand =
		QString(R"(CPH.ObsSendRaw("CallVendorRequest", "{\"vendorName\":\"streamup\",\"requestType\":\"%1\",\"requestData\":{}}", 0);)")
			.arg(command);
	auto *copyBtn = new StreamUP::UIStyles::PillButton(obs_module_text("WebSocket.Button.Copy"), "neutral");
	copyBtn->setAccessibleName(QString(obs_module_text("WebSocket.Button.CopyAccessible")).arg(command));
	QPointer<QPushButton> guard(copyBtn);
	QObject::connect(copyBtn, &QPushButton::clicked, [guard, obsRawJson, cphCommand]() {
		QApplication::clipboard()->setText(CopyKind() == 1 ? cphCommand : obsRawJson);
		if (!guard)
			return;
		guard->setText(obs_module_text("WebSocket.Button.Copied"));
		QTimer::singleShot(1000, guard, [guard]() {
			if (guard)
				guard->setText(obs_module_text("WebSocket.Button.Copy"));
		});
	});
	buttonLayout->addWidget(copyBtn);

	buttonWrapperLayout->addLayout(buttonLayout);
	buttonWrapperLayout->addStretch(); // Add stretch below

	// Add layouts to main layout
	layout->addWidget(infoWrapper, 1);         // Give info section more space
	layout->addLayout(buttonWrapperLayout, 0); // Buttons take minimal space

	return widget;
}

} // namespace WebSocketWindow
} // namespace StreamUP

