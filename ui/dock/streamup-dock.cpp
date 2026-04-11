#include "../../core/source-manager.hpp"
#include "../../utilities/debug-logger.hpp"
#include "streamup-dock.hpp"
#include "../../ui_StreamUPDock.h"
#include "../../flow-layout.hpp"
#include "../../video-capture-popup.hpp"
#include "../ui-styles.hpp"
#include "../settings-manager.hpp"
#include "../ui-helpers.hpp"
#include "../switch-button.hpp"
#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QDockWidget>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QList>
#include <QMetaObject>
#include <QDialog>
#include <QGroupBox>
#include <QFrame>
#include <QPushButton>
#include <QScrollArea>
#include <QHBoxLayout>
#include <vector>
#include <functional>

// Static list to track all dock instances
static QList<StreamUPDock*> dockInstances;

void ShowDockConfigDialog()
{
	StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
		QDialog* dialog = StreamUP::UIStyles::CreateStyledDialog(obs_module_text("Settings.Dock.Title"));
		dialog->resize(600, 450);
		
		QVBoxLayout* mainLayout = StreamUP::UIStyles::GetDialogContentLayout(dialog);

		// Header section
		QWidget* headerWidget = new QWidget();
		headerWidget->setObjectName("headerWidget");
		headerWidget->setStyleSheet(QString("QWidget#headerWidget { background: %1; padding: %2px %3px; }")
			.arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
			.arg(StreamUP::UIStyles::Sizes::PADDING_SMALL)
			.arg(StreamUP::UIStyles::Sizes::PADDING_XL));
		
		QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
		headerLayout->setContentsMargins(0, 0, 0, 0);
		
		QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle(obs_module_text("Settings.Dock.Title"));
		titleLabel->setAlignment(Qt::AlignCenter);
		headerLayout->addWidget(titleLabel);
		
		headerLayout->addSpacing(-StreamUP::UIStyles::Sizes::SPACING_SMALL);
		
		QLabel* subtitleLabel = StreamUP::UIStyles::CreateStyledDescription(obs_module_text("Settings.Dock.Description"));
		headerLayout->addWidget(subtitleLabel);
		
		mainLayout->addWidget(headerWidget);

		// Content area
		QWidget* contentWidget = new QWidget();
		contentWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
		QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
		contentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
			StreamUP::UIStyles::Sizes::PADDING_XL, 
			StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
			StreamUP::UIStyles::Sizes::PADDING_XL);
		contentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);

		// Info section
		QLabel* infoLabel = new QLabel(obs_module_text("Settings.Dock.Info"));
		infoLabel->setStyleSheet(QString(
			"QLabel {"
			"color: %1;"
			"font-size: %2px;"
			"line-height: 1.3;"
			"padding: %3px;"
			"background: %4;"
			"border: 1px solid %5;"
			"border-radius: %6px;"
			"}")
			.arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)
			.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_TINY)
			.arg(StreamUP::UIStyles::Sizes::PADDING_SMALL + 2)
			.arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
			.arg(StreamUP::UIStyles::Colors::BACKGROUND_HOVER)
			.arg(StreamUP::UIStyles::Sizes::BORDER_RADIUS));
		infoLabel->setWordWrap(true);
		contentLayout->addWidget(infoLabel);

		// Create GroupBox for dock tools configuration
		QGroupBox* toolsGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("Settings.Dock.ToolsGroupTitle"), "info");
		
		QVBoxLayout* toolsGroupLayout = new QVBoxLayout(toolsGroup);
		toolsGroupLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 0, StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 0);
		toolsGroupLayout->setSpacing(0);
		
		// Get current dock settings
		StreamUP::SettingsManager::DockToolSettings currentSettings = StreamUP::SettingsManager::GetDockToolSettings();
		(void)currentSettings; // Suppress unused variable warning

		// Define tool information structure
		struct ToolInfo {
			QString name;
			QString description;
			int toolIndex;
		};
		
		// List of all dock tools
		std::vector<ToolInfo> dockTools = {
			{obs_module_text("Dock.Tool.LockAllSources.Title"), obs_module_text("Dock.Tool.LockAllSources.Description"), 0},
			{obs_module_text("Dock.Tool.LockCurrentSources.Title"), obs_module_text("Dock.Tool.LockCurrentSources.Description"), 1},
			{obs_module_text("Dock.Tool.RefreshBrowserSources.Title"), obs_module_text("Dock.Tool.RefreshBrowserSources.Description"), 2},
			{obs_module_text("Dock.Tool.RefreshAudioMonitoring.Title"), obs_module_text("Dock.Tool.RefreshAudioMonitoring.Description"), 3},
			{obs_module_text("Dock.Tool.VideoCaptureOptions.Title"), obs_module_text("Dock.Tool.VideoCaptureOptions.Description"), 4},
			{obs_module_text("Dock.Tool.GroupSelectedSources.Title"), obs_module_text("Dock.Tool.GroupSelectedSources.Description"), 5},
			{obs_module_text("Dock.Tool.ToggleVisibilitySelectedSources.Title"), obs_module_text("Dock.Tool.ToggleVisibilitySelectedSources.Description"), 6}
		};
		
		// Create tool rows and collect switches
		for (int i = 0; i < static_cast<int>(dockTools.size()); ++i) {
			const auto& tool = dockTools[i];
			
			QWidget* toolRow = new QWidget();
			toolRow->setStyleSheet("QWidget { background: transparent; border: none; padding: 0px; }");
			
			QHBoxLayout* toolRowLayout = new QHBoxLayout(toolRow);
			toolRowLayout->setContentsMargins(0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3, 0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3);
			toolRowLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
			
			// Text section
			QVBoxLayout* textLayout = new QVBoxLayout();
			textLayout->setSpacing(2);
			textLayout->setContentsMargins(0, 0, 0, 0);
			
			QLabel* nameLabel = new QLabel(tool.name);
			nameLabel->setStyleSheet(QString(
				"QLabel {"
				"color: %1;"
				"font-size: %2px;"
				"font-weight: bold;"
				"background: transparent;"
				"border: none;"
				"margin: 0px;"
				"padding: 0px;"
				"}")
				.arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
				.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
			
			QLabel* descLabel = new QLabel(tool.description);
			descLabel->setStyleSheet(QString(
				"QLabel {"
				"color: %1;"
				"font-size: %2px;"
				"background: transparent;"
				"border: none;"
				"margin: 0px;"
				"padding: 0px;"
				"}")
				.arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
				.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL));
			descLabel->setWordWrap(true);
			
			textLayout->addWidget(nameLabel);
			textLayout->addWidget(descLabel);
			
			QWidget* textWrapper = new QWidget();
			QVBoxLayout* wrapperLayout = new QVBoxLayout(textWrapper);
			wrapperLayout->setContentsMargins(0, 0, 0, 0);
			wrapperLayout->addStretch();
			wrapperLayout->addLayout(textLayout);
			wrapperLayout->addStretch();
			
			toolRowLayout->addWidget(textWrapper, 1);
			
			// Switch section
			QVBoxLayout* switchWrapperLayout = new QVBoxLayout();
			switchWrapperLayout->setContentsMargins(0, 0, 0, 0);
			switchWrapperLayout->addStretch();
			
			StreamUP::SettingsManager::DockToolSettings freshSettings = StreamUP::SettingsManager::GetDockToolSettings();
			bool currentValue = false;
			
			switch (tool.toolIndex) {
				case 0: currentValue = freshSettings.showLockAllSources; break;
				case 1: currentValue = freshSettings.showLockCurrentSources; break;
				case 2: currentValue = freshSettings.showRefreshBrowserSources; break;
				case 3: currentValue = freshSettings.showRefreshAudioMonitoring; break;
				case 4: currentValue = freshSettings.showVideoCaptureOptions; break;
				case 5: currentValue = freshSettings.showGroupSelectedSources; break;
				case 6: currentValue = freshSettings.showToggleVisibilitySelectedSources; break;
			}
			
			StreamUP::UIStyles::SwitchButton* toolSwitch = StreamUP::UIStyles::CreateStyledSwitch("", currentValue);
			
			QTimer::singleShot(0, [toolSwitch, currentValue]() {
				toolSwitch->setChecked(currentValue);
			});
			
			QObject::connect(toolSwitch, &StreamUP::UIStyles::SwitchButton::toggled, [tool](bool checked) {
				StreamUP::SettingsManager::DockToolSettings settings = StreamUP::SettingsManager::GetDockToolSettings();

				switch (tool.toolIndex) {
					case 0: settings.showLockAllSources = checked; break;
					case 1: settings.showLockCurrentSources = checked; break;
					case 2: settings.showRefreshBrowserSources = checked; break;
					case 3: settings.showRefreshAudioMonitoring = checked; break;
					case 4: settings.showVideoCaptureOptions = checked; break;
					case 5: settings.showGroupSelectedSources = checked; break;
					case 6: settings.showToggleVisibilitySelectedSources = checked; break;
				}

				StreamUP::SettingsManager::UpdateDockToolSettings(settings);
			});
			
			switchWrapperLayout->addWidget(toolSwitch);
			switchWrapperLayout->addStretch();
			
			toolRowLayout->addLayout(switchWrapperLayout);
			toolsGroupLayout->addWidget(toolRow);
			
			// Add separator line between tools (but not after the last one)
			if (i < static_cast<int>(dockTools.size()) - 1) {
				QFrame* separator = new QFrame();
				separator->setFrameShape(QFrame::HLine);
				separator->setFrameShadow(QFrame::Plain);
				separator->setStyleSheet(QString(
					"QFrame {"
					"color: rgba(113, 128, 150, 0.3);"
					"background-color: rgba(113, 128, 150, 0.3);"
					"border: none;"
					"margin: 0px;"
					"max-height: 1px;"
					"}"));
				toolsGroupLayout->addWidget(separator);
			}
		}
		
		// Action buttons section
		QHBoxLayout* actionLayout = new QHBoxLayout();
		actionLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
		actionLayout->setContentsMargins(0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3, 0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3);
		
		QPushButton* resetButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("Settings.Dock.ResetConfig"), "error");
		
		QObject::connect(resetButton, &QPushButton::clicked, [toolsGroup]() {
			StreamUP::UIHelpers::ShowDialogOnUIThread([toolsGroup]() {
				QDialog* confirmDialog = StreamUP::UIStyles::CreateStyledDialog(obs_module_text("Settings.Dock.ResetTitle"));
				confirmDialog->resize(400, 200);
				
				QVBoxLayout* layout = StreamUP::UIStyles::GetDialogContentLayout(confirmDialog);
				
				QLabel* warningLabel = new QLabel(obs_module_text("Settings.Dock.ResetWarning"));
				warningLabel->setStyleSheet(QString("color: %1; font-size: %2px; padding: %3px;")
					.arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
					.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL)
					.arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM));
				warningLabel->setWordWrap(true);
				warningLabel->setAlignment(Qt::AlignCenter);
				
				layout->addWidget(warningLabel);
				
				QPushButton* cancelBtn = StreamUP::UIStyles::CreateStyledButton(obs_module_text("UI.Button.Cancel"), "neutral");
				QPushButton* resetBtn = StreamUP::UIStyles::CreateStyledButton(obs_module_text("Settings.Dock.ResetButton"), "error");

				QObject::connect(cancelBtn, &QPushButton::clicked, confirmDialog, &QDialog::close);
				QObject::connect(resetBtn, &QPushButton::clicked, [confirmDialog, toolsGroup]() {
					StreamUP::SettingsManager::DockToolSettings defaultSettings;
					StreamUP::SettingsManager::UpdateDockToolSettings(defaultSettings);

					// Find all switch buttons in the toolsGroup
					QList<StreamUP::UIStyles::SwitchButton*> switches = toolsGroup->findChildren<StreamUP::UIStyles::SwitchButton*>();
					for (int j = 0; j < switches.size(); ++j) {
						StreamUP::UIStyles::SwitchButton* switchButton = switches.at(j);
						if (switchButton) {
							switchButton->setChecked(true);
						}
					}

					confirmDialog->close();
				});

				QVBoxLayout* footerVLay = StreamUP::UIStyles::GetDialogFooterLayout(confirmDialog);
				QHBoxLayout* footerBtnLay = new QHBoxLayout();
				footerBtnLay->addStretch();
				footerBtnLay->addWidget(cancelBtn);
				footerBtnLay->addWidget(resetBtn);
				footerVLay->addLayout(footerBtnLay);
				
				confirmDialog->show();
				StreamUP::UIHelpers::CenterDialog(confirmDialog);
			});
		});
		
		actionLayout->addStretch();
		actionLayout->addWidget(resetButton);
		
		toolsGroupLayout->addLayout(actionLayout);
		contentLayout->addWidget(toolsGroup);
		contentLayout->addStretch();
		
		mainLayout->addWidget(contentWidget);

		// Close button in footer
		QVBoxLayout* footerLayout = StreamUP::UIStyles::GetDialogFooterLayout(dialog);
		QHBoxLayout* footerBtnLay = new QHBoxLayout();
		QPushButton* closeButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("UI.Button.Close"), "neutral");
		QObject::connect(closeButton, &QPushButton::clicked, [=]() {
			dialog->close();
		});
		footerBtnLay->addStretch();
		footerBtnLay->addWidget(closeButton);
		footerBtnLay->addStretch();
		footerLayout->addLayout(footerBtnLay);
		StreamUP::UIStyles::ApplyConsistentSizing(dialog, 600, 900, 450, 700);
		dialog->show();
	});
}

void StreamUPDock::applyFileIconToButton(QPushButton *button, const QString &filePath)
{
	button->setIcon(QIcon(filePath));
}

StreamUPDock::StreamUPDock(QWidget *parent) : QFrame(parent), ui(new Ui::StreamUPDock), videoCapturePopup(nullptr), isProcessing(false)
{
	ui->setupUi(this);

	// Create buttons with standard QPushButton to allow theme customization
	lockAllSourcesButton = new QPushButton("", this);
	lockCurrentSourcesButton = new QPushButton("", this);
	refreshBrowserButton = new QPushButton("", this);
	refreshAudioButton = new QPushButton("", this);
	videoCaptureButton = new QPushButton("", this);
	groupSelectedSourcesButton = new QPushButton("", this);
	toggleVisibilityButton = new QPushButton("", this);

	// Apply initial icons to buttons
	applyFileIconToButton(lockAllSourcesButton, StreamUP::UIHelpers::GetThemedIconPath("all-scene-source-locked"));
	applyFileIconToButton(lockCurrentSourcesButton, StreamUP::UIHelpers::GetThemedIconPath("current-scene-source-locked"));
	applyFileIconToButton(refreshBrowserButton, StreamUP::UIHelpers::GetThemedIconPath("refresh-browser-sources"));
	applyFileIconToButton(refreshAudioButton, StreamUP::UIHelpers::GetThemedIconPath("refresh-audio-monitoring"));
	applyFileIconToButton(videoCaptureButton, StreamUP::UIHelpers::GetThemedIconPath("camera"));
	applyFileIconToButton(groupSelectedSourcesButton, StreamUP::UIHelpers::GetThemedIconPath("add-sources-to-group"));
	applyFileIconToButton(toggleVisibilityButton, StreamUP::UIHelpers::GetThemedIconPath("visible"));

	auto setButtonProperties = [](QPushButton *button) {
		button->setIconSize(QSize(16, 16));  // Smaller icon for smaller button
		// Force square dimensions aggressively
		button->setFixedSize(28, 28);
		button->setMinimumSize(28, 28);
		button->setMaximumSize(28, 28);
		button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		// Remove any padding/margins that might stretch the button
		button->setContentsMargins(0, 0, 0, 0);

		// Apply minimal structural styling - only size and spacing properties, no colors
		// This ensures the FlowLayout calculates positions correctly
		button->setStyleSheet(
			"QPushButton {"
			"    min-width: 28px;"
			"    max-width: 28px;"
			"    min-height: 28px;"
			"    max-height: 28px;"
			"    width: 28px;"
			"    height: 28px;"
			"    padding: 0px;"
			"    margin: 2px;"
			"}"
		);
	};

	// Set properties for each button
	setButtonProperties(lockAllSourcesButton);
	setButtonProperties(lockCurrentSourcesButton);
	setButtonProperties(refreshBrowserButton);
	setButtonProperties(refreshAudioButton);
	setButtonProperties(videoCaptureButton);
	setButtonProperties(groupSelectedSourcesButton);
	setButtonProperties(toggleVisibilityButton);

	// Set tooltips for buttons
	lockAllSourcesButton->setToolTip(obs_module_text("Feature.SourceLock.All.Title"));
	lockCurrentSourcesButton->setToolTip(obs_module_text("Feature.SourceLock.Current.Title"));
	refreshBrowserButton->setToolTip(obs_module_text("Feature.BrowserSources.Title"));
	refreshAudioButton->setToolTip(obs_module_text("Feature.AudioMonitoring.Title"));
	videoCaptureButton->setToolTip(obs_module_text("Feature.VideoCapture.Title"));
	groupSelectedSourcesButton->setToolTip(obs_module_text("Dock.Tool.GroupSelectedSources.Title"));
	toggleVisibilityButton->setToolTip(obs_module_text("Dock.Tool.ToggleVisibilitySelectedSources.Title"));

	// Create a flow layout to hold the buttons
	mainDockLayout = new FlowLayout(this, 5, 5, 5);

	mainDockLayout->addWidget(lockAllSourcesButton);
	mainDockLayout->addWidget(lockCurrentSourcesButton);
	mainDockLayout->addWidget(refreshBrowserButton);
	mainDockLayout->addWidget(refreshAudioButton);
	mainDockLayout->addWidget(videoCaptureButton);
	mainDockLayout->addWidget(groupSelectedSourcesButton);
	mainDockLayout->addWidget(toggleVisibilityButton);

	// Set the layout to the StreamupDock
	this->setLayout(mainDockLayout);

	// Connect buttons to their respective slots
	connect(lockAllSourcesButton, &QPushButton::clicked, this, &StreamUPDock::ButtonToggleLockAllSources);
	connect(lockCurrentSourcesButton, &QPushButton::clicked, this, &StreamUPDock::ButtonToggleLockSourcesInCurrentScene);
	connect(refreshBrowserButton, &QPushButton::clicked, this, &StreamUPDock::ButtonRefreshBrowserSources);
	connect(refreshAudioButton, &QPushButton::clicked, this, &StreamUPDock::ButtonRefreshAudioMonitoring);
	connect(videoCaptureButton, &QPushButton::clicked, this, &StreamUPDock::ButtonShowVideoCapturePopup);
	connect(groupSelectedSourcesButton, &QPushButton::clicked, this, &StreamUPDock::ButtonGroupSelectedSources);
	connect(toggleVisibilityButton, &QPushButton::clicked, this, &StreamUPDock::ButtonToggleVisibilitySelectedSources);

	// Setup OBS signals
	setupObsSignals();

	updateButtonIcons();
	updateToolVisibility();

	// Force layout recalculation after everything is set up
	QTimer::singleShot(0, this, [this]() {
		if (mainDockLayout) {
			mainDockLayout->invalidate();
			mainDockLayout->activate();
		}
		this->updateGeometry();
		this->update();
	});
	
	// Add this dock to the static list for notifications
	dockInstances.append(this);
}

StreamUPDock::~StreamUPDock()
{
	// Remove this dock from the static list
	dockInstances.removeAll(this);

	// Remove frontend event callback
	obs_frontend_remove_event_callback(onFrontendEvent, this);

	// Disconnect from the stored scene signals
	disconnectSceneSignals();

	if (videoCapturePopup) {
		videoCapturePopup->deleteLater();
		videoCapturePopup = nullptr;
	}

	delete ui;
}

void StreamUPDock::ButtonToggleLockAllSources()
{
	if (isProcessing)
		return;
	isProcessing = true;

	StreamUP::SourceManager::ToggleLockAllSources(false);
	updateButtonIcons();

	isProcessing = false;
}

void StreamUPDock::ButtonToggleLockSourcesInCurrentScene()
{
	if (isProcessing)
		return;
	isProcessing = true;

	StreamUP::SourceManager::ToggleLockSourcesInCurrentScene(false);
	updateButtonIcons();

	isProcessing = false;
}

void StreamUPDock::ButtonRefreshAudioMonitoring()
{
	if (isProcessing)
		return;
	isProcessing = true;

	obs_enum_sources(StreamUP::SourceManager::RefreshAudioMonitoring, nullptr);

	isProcessing = false;
}

void StreamUPDock::ButtonRefreshBrowserSources()
{
	if (isProcessing)
		return;
	isProcessing = true;

	obs_enum_sources(StreamUP::SourceManager::RefreshBrowserSources, nullptr);

	isProcessing = false;
}

void StreamUPDock::ButtonActivateAllVideoCaptureDevices()
{
	if (isProcessing)
		return;
	isProcessing = true;

	StreamUP::SourceManager::ActivateAllVideoCaptureDevices(true);

	isProcessing = false;
}

void StreamUPDock::ButtonDeactivateAllVideoCaptureDevices()
{
	if (isProcessing)
		return;
	isProcessing = true;

	StreamUP::SourceManager::DeactivateAllVideoCaptureDevices(true);

	isProcessing = false;
}

void StreamUPDock::ButtonShowVideoCapturePopup()
{
	if (isProcessing)
		return;

	// Toggle functionality: if popup is already open, close it
	if (videoCapturePopup && videoCapturePopup->isVisible()) {
		videoCapturePopup->deleteLater();
		videoCapturePopup = nullptr;
		return;
	}

	// Close existing popup if open but not visible
	if (videoCapturePopup) {
		videoCapturePopup->deleteLater();
		videoCapturePopup = nullptr;
	}

	// Create new popup
	videoCapturePopup = new VideoCapturePopup(this);

	// Connect to handle cleanup when popup is closed
	connect(videoCapturePopup, &QWidget::destroyed, this, [this]() {
		videoCapturePopup = nullptr;
	});

	// Ensure popup icons are themed correctly for current theme
	videoCapturePopup->updateIconsForTheme();

	// Show popup next to button
	QPoint buttonPos = videoCaptureButton->mapToGlobal(QPoint(0, 0));
	videoCapturePopup->showNearButton(buttonPos, videoCaptureButton->size());
}

void StreamUPDock::ButtonRefreshAllVideoCaptureDevices()
{
	if (isProcessing)
		return;
	isProcessing = true;

	StreamUP::SourceManager::RefreshAllVideoCaptureDevices(true);

	isProcessing = false;
}

void StreamUPDock::ButtonGroupSelectedSources()
{
	if (isProcessing)
		return;
	isProcessing = true;

	StreamUP::SourceManager::GroupSelectedSources(true);

	isProcessing = false;
}

void StreamUPDock::ButtonToggleVisibilitySelectedSources()
{
	if (isProcessing)
		return;
	isProcessing = true;

	StreamUP::SourceManager::ToggleVisibilitySelectedSources(true);
	updateButtonIcons();

	isProcessing = false;
}

void StreamUPDock::updateButtonIcons()
{
	// Update lockAllSourcesButton icon based on whether all sources are locked in all scenes
	if (StreamUP::SourceManager::AreAllSourcesLockedInAllScenes()) {
		applyFileIconToButton(lockAllSourcesButton, StreamUP::UIHelpers::GetThemedIconPath("all-scene-source-locked"));
	} else {
		applyFileIconToButton(lockAllSourcesButton, StreamUP::UIHelpers::GetThemedIconPath("all-scene-source-unlocked"));
	}

	// Update lockCurrentSourcesButton icon based on whether all sources are locked in the current scene
	if (StreamUP::SourceManager::AreAllSourcesLockedInCurrentScene()) {
		applyFileIconToButton(lockCurrentSourcesButton, StreamUP::UIHelpers::GetThemedIconPath("current-scene-source-locked"));
	} else {
		applyFileIconToButton(lockCurrentSourcesButton, StreamUP::UIHelpers::GetThemedIconPath("current-scene-source-unlocked"));
	}

	// Update static button icons that don't change based on state
	applyFileIconToButton(refreshBrowserButton, StreamUP::UIHelpers::GetThemedIconPath("refresh-browser-sources"));
	applyFileIconToButton(refreshAudioButton, StreamUP::UIHelpers::GetThemedIconPath("refresh-audio-monitoring"));
	applyFileIconToButton(videoCaptureButton, StreamUP::UIHelpers::GetThemedIconPath("camera"));
	applyFileIconToButton(groupSelectedSourcesButton, StreamUP::UIHelpers::GetThemedIconPath("add-sources-to-group"));
	applyFileIconToButton(toggleVisibilityButton, StreamUP::UIHelpers::GetThemedIconPath("visible"));

}

bool StreamUPDock::AreAllSourcesLockedInAllScenes()
{
	return !StreamUP::SourceManager::CheckIfAnyUnlockedInAllScenes();
}

bool StreamUPDock::AreAllSourcesLockedInCurrentScene()
{
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		return false;
	}

	obs_scene_t *scene = obs_scene_from_source(current_scene);
	bool result = !StreamUP::SourceManager::CheckIfAnyUnlocked(scene);
	obs_source_release(current_scene);
	return result;
}

void StreamUPDock::setupObsSignals()
{
	obs_frontend_add_event_callback(StreamUPDock::onFrontendEvent, this);
	connectSceneSignals();
}

void StreamUPDock::connectSceneSignals()
{
	// Disconnect from previous scene first
	disconnectSceneSignals();

	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (current_scene) {
		signal_handler_t *scene_handler = obs_source_get_signal_handler(current_scene);
		if (scene_handler) {
			signal_handler_connect(scene_handler, "item_add", StreamUPDock::onSceneItemAdded, this);
			signal_handler_connect(scene_handler, "item_remove", StreamUPDock::onSceneItemRemoved, this);
			signal_handler_connect(scene_handler, "item_locked", StreamUPDock::onItemLockChanged, this);
		}
		// Store the connected scene (addref for our stored reference)
		m_connectedScene = current_scene; // already has a ref from obs_frontend_get_current_scene
	}
}

void StreamUPDock::disconnectSceneSignals()
{
	if (m_connectedScene) {
		signal_handler_t *scene_handler = obs_source_get_signal_handler(m_connectedScene);
		if (scene_handler) {
			signal_handler_disconnect(scene_handler, "item_add", StreamUPDock::onSceneItemAdded, this);
			signal_handler_disconnect(scene_handler, "item_remove", StreamUPDock::onSceneItemRemoved, this);
			signal_handler_disconnect(scene_handler, "item_locked", StreamUPDock::onItemLockChanged, this);
		}
		obs_source_release(m_connectedScene);
		m_connectedScene = nullptr;
	}
}

void StreamUPDock::onFrontendEvent(enum obs_frontend_event event, void *private_data)
{
	StreamUPDock *dock = static_cast<StreamUPDock *>(private_data);
	if (dock->isProcessing)
		return;

	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING) {
		// Disconnect scene signals immediately before collection teardown
		// to prevent item_remove signals from firing into stale state
		dock->disconnectSceneSignals();
		dock->m_sceneCollectionChanging = true;
	}
	else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		dock->m_sceneCollectionChanging = false;
		QMetaObject::invokeMethod(dock, [dock]() {
			dock->connectSceneSignals();
			dock->updateButtonIcons();
		}, Qt::QueuedConnection);
	}
	else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
		if (dock->m_sceneCollectionChanging)
			return;
		QMetaObject::invokeMethod(dock, [dock]() {
			dock->disconnectSceneSignals();
			dock->connectSceneSignals();
			dock->updateButtonIcons();
		}, Qt::QueuedConnection);
	}
	else if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING || event == OBS_FRONTEND_EVENT_PROFILE_CHANGED) {
		// OBS finished loading or profile changed, update icons with proper theme detection
		StreamUP::DebugLogger::LogDebugFormat("UI", "Event", "Dock received %s event",
			event == OBS_FRONTEND_EVENT_FINISHED_LOADING ? "FINISHED_LOADING" : "PROFILE_CHANGED");
		dock->m_sceneCollectionChanging = false;
		QMetaObject::invokeMethod(dock, "updateButtonIcons", Qt::QueuedConnection);
	}
	else if (event == OBS_FRONTEND_EVENT_THEME_CHANGED) {
		// Theme changed, update button icons for new theme
		StreamUP::DebugLogger::LogDebug("UI", "Theme", "Dock received OBS_FRONTEND_EVENT_THEME_CHANGED event");
		QMetaObject::invokeMethod(dock, "updateButtonIcons", Qt::QueuedConnection);
	}
}

void StreamUPDock::onSceneItemAdded(void *param, calldata_t *data)
{
	Q_UNUSED(data);

	StreamUPDock *self = static_cast<StreamUPDock *>(param);
	if (self->isProcessing || self->m_sceneCollectionChanging)
		return;
	QMetaObject::invokeMethod(self, "updateButtonIcons", Qt::QueuedConnection);
}

void StreamUPDock::onSceneItemRemoved(void *param, calldata_t *data)
{
	Q_UNUSED(data);

	StreamUPDock *self = static_cast<StreamUPDock *>(param);
	if (self->isProcessing || self->m_sceneCollectionChanging)
		return;
	QMetaObject::invokeMethod(self, "updateButtonIcons", Qt::QueuedConnection);
}

void StreamUPDock::onItemLockChanged(void *param, calldata_t *data)
{
	Q_UNUSED(data);

	StreamUPDock *self = static_cast<StreamUPDock *>(param);
	if (self->isProcessing || self->m_sceneCollectionChanging)
		return;
	QMetaObject::invokeMethod(self, "updateButtonIcons", Qt::QueuedConnection);
}

void StreamUPDock::updateToolVisibility()
{
	StreamUP::SettingsManager::DockToolSettings settings = StreamUP::SettingsManager::GetDockToolSettings();

	lockAllSourcesButton->setVisible(settings.showLockAllSources);
	lockCurrentSourcesButton->setVisible(settings.showLockCurrentSources);
	refreshBrowserButton->setVisible(settings.showRefreshBrowserSources);
	refreshAudioButton->setVisible(settings.showRefreshAudioMonitoring);
	videoCaptureButton->setVisible(settings.showVideoCaptureOptions);
	groupSelectedSourcesButton->setVisible(settings.showGroupSelectedSources);
	toggleVisibilityButton->setVisible(settings.showToggleVisibilitySelectedSources);

	// Force layout update and repaint
	if (mainDockLayout) {
		mainDockLayout->update();
	}
	this->update();
	this->repaint();
}

void StreamUPDock::showContextMenu(const QPoint& position)
{
	QMenu contextMenu(this);

	QAction* configureAction = contextMenu.addAction(obs_module_text("Dock.ContextMenu.Configure"));
	connect(configureAction, &QAction::triggered, []() {
		// Open Settings window on Dock Configuration tab (index 5)
		StreamUP::SettingsManager::ShowSettingsDialog(5);
	});

	contextMenu.exec(mapToGlobal(position));
}

void StreamUPDock::contextMenuEvent(QContextMenuEvent* event)
{
	showContextMenu(event->pos());
}

void StreamUPDock::showEvent(QShowEvent* event)
{
	QFrame::showEvent(event);

	// Force layout recalculation when dock is first shown
	if (mainDockLayout) {
		mainDockLayout->invalidate();
		mainDockLayout->activate();
		mainDockLayout->update();
	}
	updateGeometry();
	update();
}

void StreamUPDock::resizeEvent(QResizeEvent* event)
{
	QFrame::resizeEvent(event);

	// Force layout recalculation when dock is resized
	if (mainDockLayout) {
		mainDockLayout->invalidate();
		mainDockLayout->activate();
	}
}

void StreamUPDock::onSettingsChanged()
{
	updateToolVisibility();
}

void StreamUPDock::NotifyAllDocksSettingsChanged()
{
	for (StreamUPDock* dock : dockInstances) {
		if (dock) {
			// Use Qt's queued connection to ensure we're on the correct thread
			QMetaObject::invokeMethod(dock, "onSettingsChanged", Qt::QueuedConnection);
		}
	}
}
