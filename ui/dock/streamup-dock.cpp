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
#include <QList>
#include <QMetaObject>
#include <QDialog>
#include <QGroupBox>
#include <QFrame>
#include <QPushButton>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QList>
#include <vector>
#include <functional>

// Static list to track all dock instances
static QList<StreamUPDock*> dockInstances;

void ShowDockConfigDialog()
{
	StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
		QDialog* dialog = StreamUP::UIStyles::CreateStyledDialog(obs_module_text("Settings.Dock.Title"));
		dialog->resize(600, 450);
		
		QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
		mainLayout->setContentsMargins(0, 0, 0, 0);
		mainLayout->setSpacing(0);

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
			{obs_module_text("Dock.Tool.VideoCaptureOptions.Title"), obs_module_text("Dock.Tool.VideoCaptureOptions.Description"), 4}
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
				
				QVBoxLayout* layout = new QVBoxLayout(confirmDialog);
				
				QLabel* warningLabel = new QLabel(obs_module_text("Settings.Dock.ResetWarning"));
				warningLabel->setStyleSheet(QString("color: %1; font-size: %2px; padding: %3px;")
					.arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
					.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL)
					.arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM));
				warningLabel->setWordWrap(true);
				warningLabel->setAlignment(Qt::AlignCenter);
				
				layout->addWidget(warningLabel);
				
				QHBoxLayout* buttonLayout = new QHBoxLayout();
				
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
				
				buttonLayout->addStretch();
				buttonLayout->addWidget(cancelBtn);
				buttonLayout->addWidget(resetBtn);
				
				layout->addLayout(buttonLayout);
				
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

		// Bottom button area
		QWidget* buttonWidget = new QWidget();
		buttonWidget->setStyleSheet("background: transparent;");
		QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
		buttonLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, 
			StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
			StreamUP::UIStyles::Sizes::PADDING_XL, 
			StreamUP::UIStyles::Sizes::PADDING_MEDIUM);

		QPushButton* closeButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("UI.Button.Close"), "neutral");
		QObject::connect(closeButton, &QPushButton::clicked, [=]() {
			dialog->close();
		});

		buttonLayout->addStretch();
		buttonLayout->addWidget(closeButton);
		buttonLayout->addStretch();
		
		mainLayout->addWidget(buttonWidget);

		dialog->setLayout(mainLayout);
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

	// Create buttons with more appropriate dock size
	button1 = StreamUP::UIStyles::CreateStyledSquircleButton("", "neutral", 28);
	button2 = StreamUP::UIStyles::CreateStyledSquircleButton("", "neutral", 28);
	button3 = StreamUP::UIStyles::CreateStyledSquircleButton("", "neutral", 28);
	button4 = StreamUP::UIStyles::CreateStyledSquircleButton("", "neutral", 28);
	videoCaptureButton = StreamUP::UIStyles::CreateStyledSquircleButton("", "neutral", 28);

	// Apply initial icons to buttons
	applyFileIconToButton(button1, StreamUP::UIHelpers::GetThemedIconPath("all-scene-source-locked"));
	applyFileIconToButton(button2, StreamUP::UIHelpers::GetThemedIconPath("current-scene-source-locked"));
	applyFileIconToButton(button3, StreamUP::UIHelpers::GetThemedIconPath("refresh-browser-sources"));
	applyFileIconToButton(button4, StreamUP::UIHelpers::GetThemedIconPath("refresh-audio-monitoring"));
	applyFileIconToButton(videoCaptureButton, StreamUP::UIHelpers::GetThemedIconPath("camera"));

	auto setButtonProperties = [](QPushButton *button) {
		button->setIconSize(QSize(16, 16));  // Smaller icon for smaller button
		// Force square dimensions aggressively
		button->setFixedSize(28, 28);
		button->setMinimumSize(28, 28);
		button->setMaximumSize(28, 28);
		button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		// Remove any padding/margins that might stretch the button
		button->setContentsMargins(0, 0, 0, 0);
	};

	// Set properties for each button
	setButtonProperties(button1);
	setButtonProperties(button2);
	setButtonProperties(button3);
	setButtonProperties(button4);
	setButtonProperties(videoCaptureButton);

	// Set tooltips for buttons
	button1->setToolTip(obs_module_text("Feature.SourceLock.All.Title"));
	button2->setToolTip(obs_module_text("Feature.SourceLock.Current.Title"));
	button3->setToolTip(obs_module_text("Feature.BrowserSources.Title"));
	button4->setToolTip(obs_module_text("Feature.AudioMonitoring.Title"));
	videoCaptureButton->setToolTip(obs_module_text("Feature.VideoCapture.Title"));

	// Create a flow layout to hold the buttons
	mainDockLayout = new FlowLayout(this, 5, 5, 5);

	mainDockLayout->addWidget(button1);
	mainDockLayout->addWidget(button2);
	mainDockLayout->addWidget(button3);
	mainDockLayout->addWidget(button4);
	mainDockLayout->addWidget(videoCaptureButton);

	// Set the layout to the StreamupDock
	this->setLayout(mainDockLayout);

	// Connect buttons to their respective slots
	connect(button1, &QPushButton::clicked, this, &StreamUPDock::ButtonToggleLockAllSources);
	connect(button2, &QPushButton::clicked, this, &StreamUPDock::ButtonToggleLockSourcesInCurrentScene);
	connect(button3, &QPushButton::clicked, this, &StreamUPDock::ButtonRefreshBrowserSources);
	connect(button4, &QPushButton::clicked, this, &StreamUPDock::ButtonRefreshAudioMonitoring);
	connect(videoCaptureButton, &QPushButton::clicked, this, &StreamUPDock::ButtonShowVideoCapturePopup);

	// Setup OBS signals
	setupObsSignals();

	updateButtonIcons();
	updateToolVisibility();
	
	// Add this dock to the static list for notifications
	dockInstances.append(this);
}

StreamUPDock::~StreamUPDock()
{
	// Remove this dock from the static list
	dockInstances.removeAll(this);
	
	signal_handler_t *sh = obs_get_signal_handler();
	signal_handler_disconnect(sh, "item_add", onSceneItemAdded, this);
	signal_handler_disconnect(sh, "item_remove", onSceneItemRemoved, this);
	signal_handler_disconnect(sh, "item_locked", onItemLockChanged, this);
	
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
	
	// Connect popup signals to our methods
	connect(videoCapturePopup, &VideoCapturePopup::activateAllVideoCaptureDevices,
		this, &StreamUPDock::ButtonActivateAllVideoCaptureDevices);
	connect(videoCapturePopup, &VideoCapturePopup::deactivateAllVideoCaptureDevices,
		this, &StreamUPDock::ButtonDeactivateAllVideoCaptureDevices);
	connect(videoCapturePopup, &VideoCapturePopup::refreshAllVideoCaptureDevices,
		this, &StreamUPDock::ButtonRefreshAllVideoCaptureDevices);
	
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

void StreamUPDock::updateButtonIcons()
{
	// Update button1 icon based on whether all sources are locked in all scenes
	if (StreamUP::SourceManager::AreAllSourcesLockedInAllScenes()) {
		applyFileIconToButton(button1, StreamUP::UIHelpers::GetThemedIconPath("all-scene-source-locked"));
	} else {
		applyFileIconToButton(button1, StreamUP::UIHelpers::GetThemedIconPath("all-scene-source-unlocked"));
	}

	// Update button2 icon based on whether all sources are locked in the current scene
	if (StreamUP::SourceManager::AreAllSourcesLockedInCurrentScene()) {
		applyFileIconToButton(button2, StreamUP::UIHelpers::GetThemedIconPath("current-scene-source-locked"));
	} else {
		applyFileIconToButton(button2, StreamUP::UIHelpers::GetThemedIconPath("current-scene-source-unlocked"));
	}

	// Update static button icons that don't change based on state
	applyFileIconToButton(button3, StreamUP::UIHelpers::GetThemedIconPath("refresh-browser-sources"));
	applyFileIconToButton(button4, StreamUP::UIHelpers::GetThemedIconPath("refresh-audio-monitoring"));
	applyFileIconToButton(videoCaptureButton, StreamUP::UIHelpers::GetThemedIconPath("camera"));

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
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (current_scene) {
		signal_handler_t *scene_handler = obs_source_get_signal_handler(current_scene);
		if (scene_handler) {
			signal_handler_connect(scene_handler, "item_add", StreamUPDock::onSceneItemAdded, this);
			signal_handler_connect(scene_handler, "item_remove", StreamUPDock::onSceneItemRemoved, this);
			signal_handler_connect(scene_handler, "item_locked", StreamUPDock::onItemLockChanged, this);
		}
		obs_source_release(current_scene);
	}
}

void StreamUPDock::disconnectSceneSignals()
{
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (current_scene) {
		signal_handler_t *scene_handler = obs_source_get_signal_handler(current_scene);
		if (scene_handler) {
			signal_handler_disconnect(scene_handler, "item_add", StreamUPDock::onSceneItemAdded, this);
			signal_handler_disconnect(scene_handler, "item_remove", StreamUPDock::onSceneItemRemoved, this);
			signal_handler_disconnect(scene_handler, "item_locked", StreamUPDock::onItemLockChanged, this);
		}
		obs_source_release(current_scene);
	}
}

void StreamUPDock::onFrontendEvent(enum obs_frontend_event event, void *private_data)
{
	StreamUPDock *dock = static_cast<StreamUPDock *>(private_data);
	if (dock->isProcessing)
		return;

	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
		dock->disconnectSceneSignals();
		dock->connectSceneSignals();
		dock->updateButtonIcons();
	}
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
	else if (event == OBS_FRONTEND_EVENT_THEME_CHANGED) {
		// Theme changed, update button icons for new theme
		StreamUP::DebugLogger::LogDebug("UI", "Theme", "Dock received OBS_FRONTEND_EVENT_THEME_CHANGED event");
		dock->updateButtonIcons();
	}
#endif
}

void StreamUPDock::onSceneItemAdded(void *param, calldata_t *data)
{
	UNUSED_PARAMETER(data);

	StreamUPDock *self = static_cast<StreamUPDock *>(param);
	if (self->isProcessing)
		return;
	self->updateButtonIcons();
}

void StreamUPDock::onSceneItemRemoved(void *param, calldata_t *data)
{
	UNUSED_PARAMETER(data);

	StreamUPDock *self = static_cast<StreamUPDock *>(param);
	if (self->isProcessing)
		return;
	self->updateButtonIcons();
}

void StreamUPDock::onItemLockChanged(void *param, calldata_t *data)
{
	UNUSED_PARAMETER(data);

	StreamUPDock *self = static_cast<StreamUPDock *>(param);
	if (self->isProcessing)
		return;
	self->updateButtonIcons();
}

void StreamUPDock::updateToolVisibility()
{
	StreamUP::SettingsManager::DockToolSettings settings = StreamUP::SettingsManager::GetDockToolSettings();
	
	button1->setVisible(settings.showLockAllSources);
	button2->setVisible(settings.showLockCurrentSources);
	button3->setVisible(settings.showRefreshBrowserSources);
	button4->setVisible(settings.showRefreshAudioMonitoring);
	videoCaptureButton->setVisible(settings.showVideoCaptureOptions);
	
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
		ShowDockConfigDialog();
	});
	
	contextMenu.exec(mapToGlobal(position));
}

void StreamUPDock::contextMenuEvent(QContextMenuEvent* event)
{
	showContextMenu(event->pos());
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
