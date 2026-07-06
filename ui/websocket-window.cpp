#include "websocket-window.hpp"
#include "ui-helpers.hpp"
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

		// Modern header inside scrollable area
		QWidget *headerSection = new QWidget();
		QVBoxLayout *headerLayout = new QVBoxLayout(headerSection);
		headerLayout->setContentsMargins(0, 0, 0, 0);
		headerLayout->setSpacing(StreamUP::UIStyles::S(8));

		// Title with modern styling
		QLabel *titleLabel = StreamUP::UIStyles::makeLabel(obs_module_text("WebSocket.Window.Header"), 22, 700,
								   StreamUP::UIStyles::Colors::TEXT_PRIMARY);
		titleLabel->setAlignment(Qt::AlignCenter);

		// Description with modern styling
		QLabel *subtitleLabel = StreamUP::UIStyles::makeLabel(obs_module_text("WebSocket.Window.Description"), 13, 500,
								      StreamUP::UIStyles::Colors::TEXT_SECONDARY);
		subtitleLabel->setWordWrap(true);
		subtitleLabel->setAlignment(Qt::AlignCenter);

		headerLayout->addWidget(titleLabel);
		headerLayout->addWidget(subtitleLabel);

		contentLayout->addWidget(headerSection);
		contentLayout->addSpacing(StreamUP::UIStyles::S(16));

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
			contentLayout->addWidget(StreamUP::UIStyles::sectionHeader(category));

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
				}
			}

			contentLayout->addWidget(categoryCard);
		}

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
	layout->setContentsMargins(0, StreamUP::UIStyles::S(8) + 3, 0,
				   StreamUP::UIStyles::S(8) + 3); // Add margin to layout instead
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
					 "font-weight: bold;"
					 "background: transparent;"
					 "border: none;"
					 "margin: 0px;"
					 "padding: 0px;"
					 "}")
					 .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
					 .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL)));

	// Command description
	QLabel *descLabel = new QLabel(description);
	descLabel->setStyleSheet(StreamUP::UIStyles::scale_qss(QString("QLabel {"
					 "color: %1;"
					 "font-size: %2px;"
					 "background: transparent;"
					 "border: none;"
					 "margin: 0px;"
					 "padding: 0px;"
					 "}")
					 .arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
					 .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL)));
	descLabel->setWordWrap(true);

	infoLayout->addWidget(nameLabel);
	infoLayout->addWidget(descLabel);

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

	// OBS Raw copy button
	QString obsRawJson =
		QString(R"({"requestType":"CallVendorRequest","requestData":{"vendorName":"streamup","requestType":"%1","requestData":{}}})")
			.arg(command);
	QPushButton *obsRawBtn = new QPushButton("Raw");
	obsRawBtn->setCursor(Qt::PointingHandCursor);
	obsRawBtn->setStyleSheet(StreamUP::UIStyles::buttonStyle("primary"));
	obsRawBtn->setFixedSize(StreamUP::UIStyles::S(72), StreamUP::UIStyles::S(24));
	obsRawBtn->setToolTip(obs_module_text("WebSocket.Button.OBSRaw.Tooltip"));
	QObject::connect(obsRawBtn, &QPushButton::clicked, [obsRawBtn, obsRawJson]() {
		QApplication::clipboard()->setText(obsRawJson);

		// Visual feedback: briefly change button text and style
		QString originalText = obsRawBtn->text();
		obsRawBtn->setText(obs_module_text("WebSocket.Button.Copied"));
		obsRawBtn->setEnabled(false);

		// Change to success style temporarily
		obsRawBtn->setStyleSheet(StreamUP::UIStyles::buttonStyle("success"));

		// Restore button after 1 second
		QTimer::singleShot(1000, [obsRawBtn, originalText]() {
			obsRawBtn->setText(originalText);
			obsRawBtn->setEnabled(true);
			// Restore original blue style
			obsRawBtn->setStyleSheet(StreamUP::UIStyles::buttonStyle("primary"));
		});
	});

	// CPH copy button
	QString cphCommand =
		QString(R"(CPH.ObsSendRaw("CallVendorRequest", "{\"vendorName\":\"streamup\",\"requestType\":\"%1\",\"requestData\":{}}", 0);)")
			.arg(command);
	QPushButton *cphBtn = new QPushButton("CPH");
	cphBtn->setCursor(Qt::PointingHandCursor);
	cphBtn->setStyleSheet(StreamUP::UIStyles::buttonStyle("primary"));
	cphBtn->setFixedSize(StreamUP::UIStyles::S(72), StreamUP::UIStyles::S(24));
	cphBtn->setToolTip(obs_module_text("WebSocket.Button.CPH.Tooltip"));
	QObject::connect(cphBtn, &QPushButton::clicked, [cphBtn, cphCommand]() {
		QApplication::clipboard()->setText(cphCommand);

		// Visual feedback: briefly change button text and style
		QString originalText = cphBtn->text();
		cphBtn->setText(obs_module_text("WebSocket.Button.Copied"));
		cphBtn->setEnabled(false);

		// Change to success style temporarily
		cphBtn->setStyleSheet(StreamUP::UIStyles::buttonStyle("success"));

		// Restore button after 1 second
		QTimer::singleShot(1000, [cphBtn, originalText]() {
			cphBtn->setText(originalText);
			cphBtn->setEnabled(true);
			// Restore original blue style
			cphBtn->setStyleSheet(StreamUP::UIStyles::buttonStyle("primary"));
		});
	});

	buttonLayout->addWidget(obsRawBtn);

	// Only add CPH button if CPH integration is enabled
	if (StreamUP::SettingsManager::IsCPHIntegrationEnabled()) {
		buttonLayout->addWidget(cphBtn);
	}

	buttonWrapperLayout->addLayout(buttonLayout);
	buttonWrapperLayout->addStretch(); // Add stretch below

	// Add layouts to main layout
	layout->addWidget(infoWrapper, 1);         // Give info section more space
	layout->addLayout(buttonWrapperLayout, 0); // Buttons take minimal space

	return widget;
}

} // namespace WebSocketWindow
} // namespace StreamUP

