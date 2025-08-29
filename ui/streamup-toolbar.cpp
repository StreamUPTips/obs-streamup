#include "streamup-toolbar.hpp"
#include "streamup-toolbar-configurator.hpp"
#include "dock/streamup-dock.hpp"
#include "../video-capture-popup.hpp"
#include "ui-styles.hpp"
#include "ui-helpers.hpp"
#include "settings-manager.hpp"
#include <QIcon>
#include <QHBoxLayout>
#include <QWidget>
#include <QSizePolicy>
#include <QMainWindow>
#include <QAction>
#include <QFrame>
#include <QPushButton>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMessageBox>
#include <util/config-file.h>

StreamUPToolbar::StreamUPToolbar(QWidget *parent) : QToolBar(parent), 
	streamButton(nullptr), recordButton(nullptr), pauseButton(nullptr), 
	replayBufferButton(nullptr), saveReplayButton(nullptr), virtualCameraButton(nullptr), 
	virtualCameraConfigButton(nullptr), studioModeButton(nullptr), settingsButton(nullptr),
	streamUPSettingsButton(nullptr), centralWidget(nullptr), mainLayout(nullptr), 
	contextMenu(nullptr), configureAction(nullptr)
{
	setObjectName("StreamUPToolbar");
	setWindowTitle("StreamUP Controls");

	// Setup context menu
	contextMenu = new QMenu(this);
	configureAction = contextMenu->addAction("Configure Toolbar...");
	connect(configureAction, &QAction::triggered, this, &StreamUPToolbar::onConfigureToolbarClicked);
	
	toolbarSettingsAction = contextMenu->addAction("Toolbar Settings");
	connect(toolbarSettingsAction, &QAction::triggered, this, &StreamUPToolbar::onToolbarSettingsClicked);

	// Load configuration and setup UI
	toolbarConfig.loadFromSettings();
	setupDynamicUI();
	updateAllButtons();
	
	// Ensure icons are properly themed on startup
	updateIconsForTheme();
	
	// Set initial position-aware theming (will be updated when actually added to main window)
	updatePositionAwareTheme();
	
	// Register for OBS frontend events to update button states
	obs_frontend_add_event_callback(OnFrontendEvent, this);
}

StreamUPToolbar::~StreamUPToolbar()
{
	// Remove event callback
	obs_frontend_remove_event_callback(OnFrontendEvent, this);
}

QFrame* StreamUPToolbar::createSeparator()
{
	QFrame* separator = new QFrame();
	separator->setFrameShape(QFrame::VLine);
	separator->setFrameShadow(QFrame::Plain);
	separator->setFixedSize(1, 16);
	separator->setLineWidth(1);
	return separator;
}

QFrame* StreamUPToolbar::createHorizontalSeparator()
{
	QFrame* separator = new QFrame();
	separator->setFrameShape(QFrame::HLine);
	separator->setFrameShadow(QFrame::Plain);
	separator->setFixedSize(16, 1);
	separator->setLineWidth(1);
	return separator;
} 

void StreamUPToolbar::setupUI()
{
	// Set basic toolbar properties like obs-toolbar
	setMovable(false);
	setFloatable(false);
	setOrientation(Qt::Horizontal);
	
	// Create a central widget with horizontal layout (will be changed to vertical if needed)
	centralWidget = new QWidget(this);
	mainLayout = new QHBoxLayout(centralWidget);
	mainLayout->setContentsMargins(8, 0, 8, 0);
	mainLayout->setSpacing(4);
	
	// === STREAMING SECTION ===
	// Create streaming button
	streamButton = new QToolButton(centralWidget);
	streamButton->setObjectName("streamButton");
	streamButton->setProperty("buttonType", "streamup-button");
	streamButton->setFixedSize(28, 28);
	streamButton->setIcon(QIcon(getThemedIconPath("streaming-inactive")));
	streamButton->setIconSize(QSize(20, 20));
	streamButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	streamButton->setToolTip("Start/Stop Streaming");
	streamButton->setCheckable(true);
	connect(streamButton, &QToolButton::clicked, this, &StreamUPToolbar::onStreamButtonClicked);
	mainLayout->addWidget(streamButton);
	
	// Separator
	QFrame* streamingSeparator = createSeparator();
	streamingSeparator->setObjectName("streamingSeparator");
	streamingSeparator->setProperty("separatorType", "streamup-separator");
	mainLayout->addWidget(streamingSeparator);
	
	// === RECORDING SECTION ===
	// Create recording button
	recordButton = new QToolButton(centralWidget);
	recordButton->setObjectName("recordButton");
	recordButton->setProperty("buttonType", "streamup-button");
	recordButton->setFixedSize(28, 28);
	recordButton->setIcon(QIcon(getThemedIconPath("record-off")));
	recordButton->setIconSize(QSize(20, 20));
	recordButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	recordButton->setToolTip("Start/Stop Recording");
	recordButton->setCheckable(true);
	connect(recordButton, &QToolButton::clicked, this, &StreamUPToolbar::onRecordButtonClicked);
	mainLayout->addWidget(recordButton);
	
	// Create pause recording button (initially hidden like OBS)
	pauseButton = new QToolButton(centralWidget);
	pauseButton->setObjectName("pauseButton");
	pauseButton->setProperty("buttonType", "streamup-button");
	pauseButton->setFixedSize(28, 28);
	pauseButton->setIcon(QIcon(getThemedIconPath("pause")));
	pauseButton->setIconSize(QSize(20, 20));
	pauseButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	pauseButton->setToolTip("Pause/Resume Recording");
	pauseButton->setCheckable(true);
	pauseButton->setVisible(false); // Hidden by default like OBS controls dock
	connect(pauseButton, &QToolButton::clicked, this, &StreamUPToolbar::onPauseButtonClicked);
	mainLayout->addWidget(pauseButton);
	
	// Separator
	QFrame* recordingSeparator = createSeparator();
	recordingSeparator->setObjectName("recordingSeparator");
	recordingSeparator->setProperty("separatorType", "streamup-separator");
	mainLayout->addWidget(recordingSeparator);
	
	// === REPLAY BUFFER SECTION ===
	// Create replay buffer button
	replayBufferButton = new QToolButton(centralWidget);
	replayBufferButton->setObjectName("replayBufferButton");
	replayBufferButton->setProperty("buttonType", "streamup-button");
	replayBufferButton->setFixedSize(28, 28);
	replayBufferButton->setIcon(QIcon(getThemedIconPath("replay-buffer-off")));
	replayBufferButton->setIconSize(QSize(20, 20));
	replayBufferButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	replayBufferButton->setToolTip("Start/Stop Replay Buffer");
	replayBufferButton->setCheckable(true);
	connect(replayBufferButton, &QToolButton::clicked, this, &StreamUPToolbar::onReplayBufferButtonClicked);
	mainLayout->addWidget(replayBufferButton);
	
	// Create save replay button (initially hidden like OBS)
	saveReplayButton = new QToolButton(centralWidget);
	saveReplayButton->setObjectName("saveReplayButton");
	saveReplayButton->setProperty("buttonType", "streamup-button");
	saveReplayButton->setFixedSize(28, 28);
	saveReplayButton->setIcon(QIcon(getThemedIconPath("save-replay")));
	saveReplayButton->setIconSize(QSize(20, 20)); // Slightly smaller
	saveReplayButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	saveReplayButton->setToolTip("Save Replay");
	saveReplayButton->setCheckable(false); // Save replay is not a toggle
	saveReplayButton->setVisible(false); // Hidden by default like OBS controls dock
	connect(saveReplayButton, &QToolButton::clicked, this, &StreamUPToolbar::onSaveReplayButtonClicked);
	mainLayout->addWidget(saveReplayButton);
	
	// Separator
	QFrame* replayBufferSeparator = createSeparator();
	replayBufferSeparator->setObjectName("replayBufferSeparator");
	replayBufferSeparator->setProperty("separatorType", "streamup-separator");
	mainLayout->addWidget(replayBufferSeparator);
	
	// === VIRTUAL CAMERA SECTION ===
	// Create virtual camera button
	virtualCameraButton = new QToolButton(centralWidget);
	virtualCameraButton->setObjectName("virtualCameraButton");
	virtualCameraButton->setProperty("buttonType", "streamup-button");
	virtualCameraButton->setFixedSize(28, 28);
	virtualCameraButton->setIcon(QIcon(getThemedIconPath("virtual-camera")));
	virtualCameraButton->setIconSize(QSize(20, 20));
	virtualCameraButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	virtualCameraButton->setToolTip("Start/Stop Virtual Camera");
	virtualCameraButton->setCheckable(true);
	connect(virtualCameraButton, &QToolButton::clicked, this, &StreamUPToolbar::onVirtualCameraButtonClicked);
	mainLayout->addWidget(virtualCameraButton);
	
	// Create virtual camera config button (like OBS controls dock)
	virtualCameraConfigButton = new QToolButton(centralWidget);
	virtualCameraConfigButton->setObjectName("virtualCameraConfigButton");
	virtualCameraConfigButton->setProperty("buttonType", "streamup-button");
	virtualCameraConfigButton->setFixedSize(28, 28);
	virtualCameraConfigButton->setIcon(QIcon(getThemedIconPath("virtual-camera-settings")));
	virtualCameraConfigButton->setIconSize(QSize(20, 20)); // Slightly smaller icon
	virtualCameraConfigButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	virtualCameraConfigButton->setToolTip("Virtual Camera Configuration");
	virtualCameraConfigButton->setCheckable(false);
	connect(virtualCameraConfigButton, &QToolButton::clicked, this, &StreamUPToolbar::onVirtualCameraConfigButtonClicked);
	mainLayout->addWidget(virtualCameraConfigButton);
	
	// Separator
	QFrame* virtualCameraSeparator = createSeparator();
	virtualCameraSeparator->setObjectName("virtualCameraSeparator");
	virtualCameraSeparator->setProperty("separatorType", "streamup-separator");
	mainLayout->addWidget(virtualCameraSeparator);
	
	// === STUDIO MODE SECTION ===
	// Create studio mode button
	studioModeButton = new QToolButton(centralWidget);
	studioModeButton->setObjectName("studioModeButton");
	studioModeButton->setProperty("buttonType", "streamup-button");
	studioModeButton->setFixedSize(28, 28);
	studioModeButton->setIcon(QIcon(getThemedIconPath("studio-mode")));
	studioModeButton->setIconSize(QSize(20, 20));
	studioModeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	studioModeButton->setToolTip("Toggle Studio Mode");
	studioModeButton->setCheckable(true);
	connect(studioModeButton, &QToolButton::clicked, this, &StreamUPToolbar::onStudioModeButtonClicked);
	mainLayout->addWidget(studioModeButton);
	
	// Separator
	QFrame* studioModeSeparator = createSeparator();
	studioModeSeparator->setObjectName("studioModeSeparator");
	studioModeSeparator->setProperty("separatorType", "streamup-separator");
	mainLayout->addWidget(studioModeSeparator);
	
	// === SETTINGS SECTION ===
	// Create settings button
	settingsButton = new QToolButton(centralWidget);
	settingsButton->setObjectName("settingsButton");
	settingsButton->setProperty("buttonType", "streamup-button");
	settingsButton->setFixedSize(28, 28);
	settingsButton->setIcon(QIcon(getThemedIconPath("settings")));
	settingsButton->setIconSize(QSize(20, 20));
	settingsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	settingsButton->setToolTip("Open Settings");
	settingsButton->setCheckable(false); // Settings button is not checkable
	connect(settingsButton, &QToolButton::clicked, this, &StreamUPToolbar::onSettingsButtonClicked);
	mainLayout->addWidget(settingsButton);
	
	// Add spacer to push StreamUP settings button to the right
	mainLayout->addStretch();
	
	// === STREAMUP SETTINGS SECTION (RIGHT SIDE) ===
	// Create StreamUP settings button
	streamUPSettingsButton = new QToolButton(centralWidget);
	streamUPSettingsButton->setObjectName("streamUPSettingsButton");
	streamUPSettingsButton->setProperty("buttonType", "streamup-button");
	streamUPSettingsButton->setFixedSize(28, 28);
	streamUPSettingsButton->setIcon(QIcon(":images/icons/social/streamup-logo-button.svg"));
	streamUPSettingsButton->setIconSize(QSize(20, 20));
	streamUPSettingsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	streamUPSettingsButton->setToolTip("Open StreamUP Settings");
	streamUPSettingsButton->setCheckable(false);
	connect(streamUPSettingsButton, &QToolButton::clicked, this, &StreamUPToolbar::onStreamUPSettingsButtonClicked);
	mainLayout->addWidget(streamUPSettingsButton);
	
	// Apply CSS styling for active states using theme constants
	updateToolbarStyling();
	
	// Add the central widget to toolbar
	addWidget(centralWidget);
	
	// Don't update button visibility immediately as OBS may not be fully initialized
	// It will be updated when OBS_FRONTEND_EVENT_FINISHED_LOADING is received
}

void StreamUPToolbar::updateToolbarStyling()
{
	// Apply theme-aware styling using StreamUP UI constants
	QString styleSheet = QString(R"(
		/* Base styling for all StreamUP toolbar buttons */
		QToolButton[buttonType="streamup-button"] {
			background: transparent;
			border: none;
			border-radius: %1px;
			padding: %2px;
		}
		QToolButton[buttonType="streamup-button"]:hover {
			background-color: %3;
		}
		QToolButton[buttonType="streamup-button"]:pressed {
			background-color: %4;
		}
		QToolButton[buttonType="streamup-button"]:checked {
			background: transparent;
			border: none;
		}
		QToolButton[buttonType="streamup-button"]:checked:hover {
			background-color: %3;
		}
		/* Special styling for virtual camera and studio mode buttons when active */
		QToolButton[objectName^="virtualCameraButton"]:checked {
			background-color: %5;
			border: 1px solid %6;
		}
		QToolButton[objectName^="virtualCameraButton"]:checked:hover {
			background-color: %7;
		}
		QToolButton[objectName^="studioModeButton"]:checked {
			background-color: %5;
			border: 1px solid %6;
		}
		QToolButton[objectName^="studioModeButton"]:checked:hover {
			background-color: %7;
		}
		/* Base styling for all StreamUP toolbar separators */
		QFrame[separatorType="streamup-separator"] {
			background-color: %8;
			border: none;
		}
	)").arg(StreamUP::UIStyles::Sizes::SPACE_4)                    // border-radius
	   .arg(StreamUP::UIStyles::Sizes::SPACE_2)                    // padding
	   .arg(StreamUP::UIStyles::Colors::HOVER_OVERLAY)             // hover background
	   .arg(StreamUP::UIStyles::Colors::PRIMARY_ALPHA_30)          // pressed background
	   .arg(StreamUP::UIStyles::Colors::PRIMARY_COLOR)             // checked background
	   .arg(StreamUP::UIStyles::Colors::PRIMARY_INACTIVE)          // checked border
	   .arg(StreamUP::UIStyles::Colors::PRIMARY_HOVER)             // checked hover background
	   .arg(StreamUP::UIStyles::Colors::BORDER_SUBTLE);            // frame background
	   
	setStyleSheet(styleSheet);
}

bool StreamUPToolbar::isReplayBufferAvailable()
{
	// Check configuration to see if replay buffer is enabled
	config_t* config = obs_frontend_get_profile_config();
	if (!config) return false;
	
	// Check if using simple or advanced output mode
	bool advancedMode = config_get_bool(config, "Output", "Mode");
	
	if (!advancedMode) {
		// Simple output mode - check if replay buffer is enabled
		return config_get_bool(config, "SimpleOutput", "RecRB");
	} else {
		// Advanced output mode - check if replay buffer is enabled
		return config_get_bool(config, "AdvOut", "RecRB");
	}
}

bool StreamUPToolbar::isRecordingPausable()
{
	// Check configuration to determine if recording is pausable
	config_t* config = obs_frontend_get_profile_config();
	if (!config) return false;
	
	// Check if using simple or advanced output mode
	bool advancedMode = config_get_bool(config, "Output", "Mode");
	
	if (!advancedMode) {
		// Simple output mode - check if quality is not "Stream" (shared encoder)
		const char* quality = config_get_string(config, "SimpleOutput", "RecQuality");
		return quality && strcmp(quality, "Stream") != 0;
	} else {
		// Advanced output mode - more complex logic, but generally pausable unless using stream encoder
		// For now, assume advanced mode recordings are generally pausable
		return true;
	}
}

void StreamUPToolbar::updateButtonVisibility()
{
	if (replayBufferButton) {
		replayBufferButton->setVisible(isReplayBufferAvailable());
	}
	
	// Pause button visibility is handled in updateRecordButton based on recording state and pausability
}

void StreamUPToolbar::onStreamButtonClicked()
{
	if (obs_frontend_streaming_active()) {
		obs_frontend_streaming_stop();
	} else {
		obs_frontend_streaming_start();
	}
	updateStreamButton();
}

void StreamUPToolbar::onRecordButtonClicked()
{
	if (obs_frontend_recording_active()) {
		obs_frontend_recording_stop();
	} else {
		obs_frontend_recording_start();
	}
	updateRecordButton();
}

void StreamUPToolbar::onPauseButtonClicked()
{
	if (obs_frontend_recording_paused()) {
		obs_frontend_recording_pause(false);
	} else {
		obs_frontend_recording_pause(true);
	}
	updatePauseButton();
}

void StreamUPToolbar::onReplayBufferButtonClicked()
{
	if (obs_frontend_replay_buffer_active()) {
		obs_frontend_replay_buffer_stop();
	} else {
		obs_frontend_replay_buffer_start();
	}
	updateReplayBufferButton();
	updateSaveReplayButton();
}

void StreamUPToolbar::onSaveReplayButtonClicked()
{
	// Save the current replay buffer
	obs_frontend_replay_buffer_save();
}

void StreamUPToolbar::onVirtualCameraButtonClicked()
{
	if (obs_frontend_virtualcam_active()) {
		obs_frontend_stop_virtualcam();
	} else {
		obs_frontend_start_virtualcam();
	}
	updateVirtualCameraButton();
}

void StreamUPToolbar::onStudioModeButtonClicked()
{
	bool studioMode = obs_frontend_preview_program_mode_active();
	obs_frontend_set_preview_program_mode(!studioMode);
	updateStudioModeButton();
}

void StreamUPToolbar::onVirtualCameraConfigButtonClicked()
{
	// Find and trigger the virtual camera config action like OBS controls dock
	QMainWindow* mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
	if (mainWindow) {
		// Try multiple possible action names for virtual camera config
		QAction* vcamConfigAction = mainWindow->findChild<QAction*>("actionVirtualCamConfig");
		if (!vcamConfigAction) {
			vcamConfigAction = mainWindow->findChild<QAction*>("action_VirtualCamConfig");
		}
		if (!vcamConfigAction) {
			vcamConfigAction = mainWindow->findChild<QAction*>("virtualCamConfigAction");
		}
		
		if (vcamConfigAction) {
			vcamConfigAction->trigger();
		} else {
			// If no action found, try to find the virtual camera config button and trigger it
			QPushButton* vcamConfigButton = mainWindow->findChild<QPushButton*>("virtualCamConfigButton");
			if (vcamConfigButton) {
				vcamConfigButton->click();
			}
		}
	}
}

void StreamUPToolbar::onSettingsButtonClicked()
{
	// Get the main OBS window and find the settings action
	QMainWindow* mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
	if (mainWindow) {
		// Look for the action_Settings (based on the connection in OBSBasic.cpp)
		QAction* settingsAction = mainWindow->findChild<QAction*>("action_Settings");
		if (settingsAction) {
			settingsAction->trigger();
		}
	}
}

void StreamUPToolbar::onStreamUPSettingsButtonClicked()
{
	// Open StreamUP settings dialog
	StreamUP::SettingsManager::ShowSettingsDialog();
}

void StreamUPToolbar::updateStreamButton()
{
	if (streamButton) {
		bool streaming = obs_frontend_streaming_active();
		streamButton->setChecked(streaming);
		QString iconName = streaming ? "streaming" : "streaming-inactive";
		QString iconPath = getThemedIconPath(iconName);
		blog(LOG_DEBUG, "[StreamUP] Updating stream button icon: %s", iconPath.toUtf8().constData());
		streamButton->setIcon(QIcon(iconPath));
		streamButton->setToolTip(streaming ? "Stop Streaming" : "Start Streaming");
	}
}

void StreamUPToolbar::updateRecordButton()
{
	if (recordButton) {
		bool recording = obs_frontend_recording_active();
		recordButton->setChecked(recording);
		QString iconName = recording ? "record-on" : "record-off";
		recordButton->setIcon(QIcon(getThemedIconPath(iconName)));
		recordButton->setToolTip(recording ? "Stop Recording" : "Start Recording");
		
		// Show/hide pause button based on recording state AND pausability (like OBS controls dock)
		if (pauseButton) {
			bool showPause = recording && isRecordingPausable();
			pauseButton->setVisible(showPause);
		}
	}
}

void StreamUPToolbar::updatePauseButton()
{
	if (pauseButton) {
		bool recording = obs_frontend_recording_active();
		bool paused = obs_frontend_recording_paused();
		pauseButton->setEnabled(recording);
		pauseButton->setChecked(paused);
		pauseButton->setIcon(QIcon(getThemedIconPath("pause")));
		pauseButton->setToolTip(paused ? "Resume Recording" : "Pause Recording");
	}
}

void StreamUPToolbar::updateReplayBufferButton()
{
	if (replayBufferButton) {
		bool active = obs_frontend_replay_buffer_active();
		replayBufferButton->setChecked(active);
		QString iconName = active ? "replay-buffer-on" : "replay-buffer-off";
		replayBufferButton->setIcon(QIcon(getThemedIconPath(iconName)));
		replayBufferButton->setToolTip(active ? "Stop Replay Buffer" : "Start Replay Buffer");
		
		// Show/hide save replay button based on replay buffer state (like OBS controls dock)
		if (saveReplayButton) {
			saveReplayButton->setVisible(active);
		}
	}
}

void StreamUPToolbar::updateSaveReplayButton()
{
	if (saveReplayButton) {
		bool replayActive = obs_frontend_replay_buffer_active();
		saveReplayButton->setVisible(replayActive);
		
		// Enable/disable based on recording state (can't save during recording pause in some cases)
		bool recordingPaused = obs_frontend_recording_paused();
		saveReplayButton->setEnabled(!recordingPaused);
	}
}

void StreamUPToolbar::updateVirtualCameraButton()
{
	if (virtualCameraButton) {
		bool active = obs_frontend_virtualcam_active();
		virtualCameraButton->setChecked(active);
		virtualCameraButton->setToolTip(active ? "Stop Virtual Camera" : "Start Virtual Camera");
	}
}

void StreamUPToolbar::updateStudioModeButton()
{
	if (studioModeButton) {
		bool active = obs_frontend_preview_program_mode_active();
		studioModeButton->setChecked(active);
		studioModeButton->setToolTip(active ? "Exit Studio Mode" : "Enter Studio Mode");
	}
}

void StreamUPToolbar::updateVirtualCameraConfigButton()
{
	if (virtualCameraConfigButton) {
		virtualCameraConfigButton->setIcon(QIcon(getThemedIconPath("virtual-camera-settings")));
	}
}

void StreamUPToolbar::updateSettingsButton()
{
	if (settingsButton) {
		settingsButton->setIcon(QIcon(getThemedIconPath("settings")));
	}
}

void StreamUPToolbar::updateStreamUPSettingsButton()
{
	if (streamUPSettingsButton) {
		// StreamUP logo button stays the same (social icon, not UI icon)
		streamUPSettingsButton->setIcon(QIcon(":images/icons/social/streamup-logo-button.svg"));
	}
}

void StreamUPToolbar::updateDockButtonIcons()
{
	// Update all dynamic dock buttons that may have state-dependent icons
	for (auto it = dynamicButtons.begin(); it != dynamicButtons.end(); ++it) {
		QToolButton* button = it.value();
		if (!button) continue;
		
		// Get the dock button type from the button's property
		QString actionType = button->property("dockActionType").toString();
		
		// Update icons based on current state for lock buttons
		if (actionType == "lock_all_sources") {
			// Check if all sources are currently locked
			QWidget* mainWindow = static_cast<QWidget*>(obs_frontend_get_main_window());
			StreamUPDock* dock = mainWindow->findChild<StreamUPDock*>();
			if (dock) {
				bool allLocked = dock->AreAllSourcesLockedInAllScenes();
				QString iconName = allLocked ? "all-scene-source-locked" : "all-scene-source-unlocked";
				button->setIcon(QIcon(getThemedIconPath(iconName)));
			}
		} else if (actionType == "lock_current_sources") {
			// Check if current scene sources are locked
			QWidget* mainWindow = static_cast<QWidget*>(obs_frontend_get_main_window());
			StreamUPDock* dock = mainWindow->findChild<StreamUPDock*>();
			if (dock) {
				bool currentLocked = dock->AreAllSourcesLockedInCurrentScene();
				QString iconName = currentLocked ? "current-scene-source-locked" : "current-scene-source-unlocked";
				button->setIcon(QIcon(getThemedIconPath(iconName)));
			}
		}
	}
}

void StreamUPToolbar::updateAllButtons()
{
	updateStreamButton();
	updateRecordButton();
	updatePauseButton();
	updateReplayBufferButton();
	updateSaveReplayButton();
	updateVirtualCameraButton();
	updateVirtualCameraConfigButton();
	updateStudioModeButton();
	updateSettingsButton();
	updateStreamUPSettingsButton();
}

void StreamUPToolbar::OnFrontendEvent(enum obs_frontend_event event, void *data)
{
	StreamUPToolbar* toolbar = static_cast<StreamUPToolbar*>(data);
	if (!toolbar) return;
	
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		toolbar->updateStreamButton();
		break;
		
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		toolbar->updateRecordButton();
		toolbar->updatePauseButton();
		toolbar->updateSaveReplayButton(); // Save replay can be disabled during recording pause
		break;
		
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		toolbar->updateReplayBufferButton();
		toolbar->updateSaveReplayButton();
		break;
		
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED:
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED:
		toolbar->updateVirtualCameraButton();
		break;
		
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		toolbar->updateStudioModeButton();
		break;
		
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		// Settings may have changed, update button visibility
		toolbar->updateButtonVisibility();
		toolbar->updateAllButtons();
		break;
		
	case OBS_FRONTEND_EVENT_THEME_CHANGED:
		// Theme changed, update icons and styling for new theme
		blog(LOG_INFO, "[StreamUP] Received OBS_FRONTEND_EVENT_THEME_CHANGED event");
		toolbar->updateIconsForTheme();
		toolbar->updateToolbarStyling();
		break;
		
	default:
		break;
	}
}

QString StreamUPToolbar::getThemedIconPath(const QString& iconName)
{
	// Use the centralized theme-aware icon helper
	return StreamUP::UIHelpers::GetThemedIconPath(iconName);
}

void StreamUPToolbar::updateIconsForTheme()
{
	// Update all button icons for the current theme
	bool isDark = obs_frontend_is_theme_dark();
	blog(LOG_INFO, "[StreamUP] Theme changed, updating all toolbar icons (isDark: %s)", isDark ? "true" : "false");
	
	// Force icon cache clearing by creating new QIcon objects
	if (streamButton) {
		bool streaming = obs_frontend_streaming_active();
		QString iconName = streaming ? "streaming" : "streaming-inactive";
		QString iconPath = getThemedIconPath(iconName);
		streamButton->setIcon(QIcon());  // Clear first
		streamButton->setIcon(QIcon(iconPath));  // Set new icon
		blog(LOG_DEBUG, "[StreamUP] Force updated stream button: %s", iconPath.toUtf8().constData());
	}
	
	if (recordButton) {
		bool recording = obs_frontend_recording_active();
		QString iconName = recording ? "record-on" : "record-off";
		QString iconPath = getThemedIconPath(iconName);
		recordButton->setIcon(QIcon());
		recordButton->setIcon(QIcon(iconPath));
		blog(LOG_DEBUG, "[StreamUP] Force updated record button: %s", iconPath.toUtf8().constData());
	}
	
	if (pauseButton) {
		QString iconPath = getThemedIconPath("pause");
		pauseButton->setIcon(QIcon());
		pauseButton->setIcon(QIcon(iconPath));
		blog(LOG_DEBUG, "[StreamUP] Force updated pause button: %s", iconPath.toUtf8().constData());
	}
	
	if (replayBufferButton) {
		bool active = obs_frontend_replay_buffer_active();
		QString iconName = active ? "replay-buffer-on" : "replay-buffer-off";
		QString iconPath = getThemedIconPath(iconName);
		replayBufferButton->setIcon(QIcon());
		replayBufferButton->setIcon(QIcon(iconPath));
		blog(LOG_DEBUG, "[StreamUP] Force updated replay buffer button: %s", iconPath.toUtf8().constData());
	}
	
	if (saveReplayButton && saveReplayButton->isVisible()) {
		QString iconPath = getThemedIconPath("save-replay");
		saveReplayButton->setIcon(QIcon());
		saveReplayButton->setIcon(QIcon(iconPath));
		blog(LOG_DEBUG, "[StreamUP] Force updated save replay button: %s", iconPath.toUtf8().constData());
	}
	
	if (virtualCameraButton) {
		QString iconPath = getThemedIconPath("virtual-camera");
		virtualCameraButton->setIcon(QIcon());
		virtualCameraButton->setIcon(QIcon(iconPath));
		blog(LOG_DEBUG, "[StreamUP] Force updated virtual camera button: %s", iconPath.toUtf8().constData());
	}
	
	if (virtualCameraConfigButton) {
		QString iconPath = getThemedIconPath("virtual-camera-settings");
		virtualCameraConfigButton->setIcon(QIcon());
		virtualCameraConfigButton->setIcon(QIcon(iconPath));
		blog(LOG_DEBUG, "[StreamUP] Force updated virtual camera config button: %s", iconPath.toUtf8().constData());
	}
	
	if (studioModeButton) {
		QString iconPath = getThemedIconPath("studio-mode");
		studioModeButton->setIcon(QIcon());
		studioModeButton->setIcon(QIcon(iconPath));
		blog(LOG_DEBUG, "[StreamUP] Force updated studio mode button: %s", iconPath.toUtf8().constData());
	}
	
	if (settingsButton) {
		QString iconPath = getThemedIconPath("settings");
		settingsButton->setIcon(QIcon());
		settingsButton->setIcon(QIcon(iconPath));
		blog(LOG_DEBUG, "[StreamUP] Force updated settings button: %s", iconPath.toUtf8().constData());
	}
	
	// StreamUP settings button keeps its original icon (social icon)
	blog(LOG_INFO, "[StreamUP] All toolbar icons force-updated for theme change");
}

void StreamUPToolbar::updatePositionAwareTheme()
{
	// Get current toolbar position from the main window
	QMainWindow* mainWindow = qobject_cast<QMainWindow*>(parent());
	if (!mainWindow) {
		blog(LOG_WARNING, "[StreamUP] Unable to get main window for position-aware theming");
		return;
	}
	
	Qt::ToolBarArea currentArea = mainWindow->toolBarArea(this);
	QString positionSuffix;
	QString positionProperty;
	
	// Set object names and properties based on position for theme creators
	if (currentArea == Qt::TopToolBarArea) {
		setObjectName("StreamUPToolbar-Top");
		positionSuffix = "-Top";
		positionProperty = "top";
		blog(LOG_DEBUG, "[StreamUP] Updated toolbar theme for top position");
	} else if (currentArea == Qt::BottomToolBarArea) {
		setObjectName("StreamUPToolbar-Bottom"); 
		positionSuffix = "-Bottom";
		positionProperty = "bottom";
		blog(LOG_DEBUG, "[StreamUP] Updated toolbar theme for bottom position");
	} else if (currentArea == Qt::LeftToolBarArea) {
		setObjectName("StreamUPToolbar-Left");
		positionSuffix = "-Left";
		positionProperty = "left";
		blog(LOG_DEBUG, "[StreamUP] Updated toolbar theme for left position");
	} else if (currentArea == Qt::RightToolBarArea) {
		setObjectName("StreamUPToolbar-Right");
		positionSuffix = "-Right";
		positionProperty = "right";
		blog(LOG_DEBUG, "[StreamUP] Updated toolbar theme for right position");
	} else {
		// Fallback for floating or other positions
		setObjectName("StreamUPToolbar");
		positionSuffix = "";
		positionProperty = "floating";
		blog(LOG_DEBUG, "[StreamUP] Updated toolbar theme for floating position");
	}
	
	setProperty("toolbarPosition", positionProperty);
	
	// Update button object names with position suffix for theme targeting
	if (streamButton) {
		streamButton->setObjectName("streamButton" + positionSuffix);
		streamButton->setProperty("toolbarPosition", positionProperty);
		streamButton->setProperty("buttonType", "streamup-button"); // Ensure common property is maintained
	}
	if (recordButton) {
		recordButton->setObjectName("recordButton" + positionSuffix);
		recordButton->setProperty("toolbarPosition", positionProperty);
		recordButton->setProperty("buttonType", "streamup-button");
	}
	if (pauseButton) {
		pauseButton->setObjectName("pauseButton" + positionSuffix);
		pauseButton->setProperty("toolbarPosition", positionProperty);
		pauseButton->setProperty("buttonType", "streamup-button");
	}
	if (replayBufferButton) {
		replayBufferButton->setObjectName("replayBufferButton" + positionSuffix);
		replayBufferButton->setProperty("toolbarPosition", positionProperty);
		replayBufferButton->setProperty("buttonType", "streamup-button");
	}
	if (saveReplayButton) {
		saveReplayButton->setObjectName("saveReplayButton" + positionSuffix);
		saveReplayButton->setProperty("toolbarPosition", positionProperty);
		saveReplayButton->setProperty("buttonType", "streamup-button");
	}
	if (virtualCameraButton) {
		virtualCameraButton->setObjectName("virtualCameraButton" + positionSuffix);
		virtualCameraButton->setProperty("toolbarPosition", positionProperty);
		virtualCameraButton->setProperty("buttonType", "streamup-button");
	}
	if (virtualCameraConfigButton) {
		virtualCameraConfigButton->setObjectName("virtualCameraConfigButton" + positionSuffix);
		virtualCameraConfigButton->setProperty("toolbarPosition", positionProperty);
		virtualCameraConfigButton->setProperty("buttonType", "streamup-button");
	}
	if (studioModeButton) {
		studioModeButton->setObjectName("studioModeButton" + positionSuffix);
		studioModeButton->setProperty("toolbarPosition", positionProperty);
		studioModeButton->setProperty("buttonType", "streamup-button");
	}
	if (settingsButton) {
		settingsButton->setObjectName("settingsButton" + positionSuffix);
		settingsButton->setProperty("toolbarPosition", positionProperty);
		settingsButton->setProperty("buttonType", "streamup-button");
	}
	if (streamUPSettingsButton) {
		streamUPSettingsButton->setObjectName("streamUPSettingsButton" + positionSuffix);
		streamUPSettingsButton->setProperty("toolbarPosition", positionProperty);
		streamUPSettingsButton->setProperty("buttonType", "streamup-button");
	}
	
	// Update separator object names with position suffix
	QList<QFrame*> separators = centralWidget->findChildren<QFrame*>();
	for (QFrame* separator : separators) {
		QString currentName = separator->objectName();
		if (currentName.contains("Separator")) {
			// Remove any existing position suffix first
			currentName = currentName.split("-").first();
			separator->setObjectName(currentName + positionSuffix);
			separator->setProperty("toolbarPosition", positionProperty);
			separator->setProperty("separatorType", "streamup-separator"); // Ensure common property is maintained
		}
	}
	
	// Update layout orientation before applying theme
	updateLayoutOrientation();
	
	// Force style sheet refresh to apply position-based styling
	style()->unpolish(this);
	style()->polish(this);
	
	blog(LOG_INFO, "[StreamUP] Toolbar position-aware theme updated");
}

void StreamUPToolbar::updateLayoutOrientation()
{
	if (!centralWidget || !mainLayout) {
		blog(LOG_WARNING, "[StreamUP] Cannot update layout orientation - missing central widget or layout");
		return;
	}
	
	// Get current toolbar position from the main window
	QMainWindow* mainWindow = qobject_cast<QMainWindow*>(parent());
	if (!mainWindow) {
		blog(LOG_WARNING, "[StreamUP] Unable to get main window for layout orientation update");
		return;
	}
	
	Qt::ToolBarArea currentArea = mainWindow->toolBarArea(this);
	bool shouldBeVertical = (currentArea == Qt::LeftToolBarArea || currentArea == Qt::RightToolBarArea);
	bool currentlyVertical = (mainLayout->direction() == QBoxLayout::TopToBottom || mainLayout->direction() == QBoxLayout::BottomToTop);
	
	// Only rebuild layout if orientation needs to change
	if (shouldBeVertical != currentlyVertical) {
		blog(LOG_INFO, "[StreamUP] Changing toolbar layout orientation to %s", shouldBeVertical ? "vertical" : "horizontal");
		
		// Store all current widgets in order
		QList<QWidget*> widgets;
		QList<bool> isSeparator;
		
		// Extract widgets from current layout
		while (mainLayout->count() > 0) {
			QLayoutItem* item = mainLayout->takeAt(0);
			if (item->widget()) {
				widgets.append(item->widget());
				// Check if this is a separator to know which type to use
				QFrame* frame = qobject_cast<QFrame*>(item->widget());
				isSeparator.append(frame && (frame->frameShape() == QFrame::VLine || frame->frameShape() == QFrame::HLine));
			}
			delete item;
		}
		
		// Delete the old layout and create a new one with correct orientation
		delete mainLayout;
		if (shouldBeVertical) {
			mainLayout = new QVBoxLayout(centralWidget);
			setOrientation(Qt::Vertical);
			mainLayout->setContentsMargins(4, 8, 4, 8);
			mainLayout->setAlignment(Qt::AlignHCenter); // Center widgets horizontally
			
			// Set size policies for vertical layout to maintain button sizes
			centralWidget->setMinimumWidth(36); // Ensure minimum width for buttons (28px + padding)
			centralWidget->setMaximumWidth(48); // Prevent excessive width
		} else {
			mainLayout = new QHBoxLayout(centralWidget);
			setOrientation(Qt::Horizontal);
			mainLayout->setContentsMargins(8, 0, 8, 0);
			// No alignment needed for horizontal layout (default is fine)
			
			// Reset size constraints for horizontal layout
			centralWidget->setMinimumWidth(0);
			centralWidget->setMaximumWidth(16777215); // QWIDGETSIZE_MAX equivalent
		}
		mainLayout->setSpacing(4);
		
		// Re-add widgets with appropriate separators, handling StreamUP button positioning
		QWidget* streamupButton = nullptr;
		QList<QWidget*> mainButtons;
		QList<bool> mainSeparators;
		
		// Separate StreamUP button from main buttons
		for (int i = 0; i < widgets.size(); ++i) {
			QWidget* widget = widgets[i];
			
			// Check if this is the StreamUP settings button
			if (widget == streamUPSettingsButton) {
				streamupButton = widget;
			} else {
				mainButtons.append(widget);
				mainSeparators.append(isSeparator[i]);
			}
		}
		
		// Add main buttons first
		for (int i = 0; i < mainButtons.size(); ++i) {
			QWidget* widget = mainButtons[i];
			
			if (mainSeparators[i]) {
				// Replace separator with correct orientation
				QString separatorName = widget->objectName(); // Preserve the separator name
				widget->deleteLater(); // Delete the old separator
				
				// Add new separator with correct orientation and restore object name
				QFrame* separator;
				if (shouldBeVertical) {
					separator = createHorizontalSeparator();
					mainLayout->addWidget(separator, 0, Qt::AlignHCenter);
				} else {
					separator = createSeparator();
					mainLayout->addWidget(separator);
				}
				separator->setObjectName(separatorName); // Restore the name
				separator->setProperty("separatorType", "streamup-separator"); // Restore common property
				
				// Set the toolbarPosition property based on current position
				QMainWindow* mainWindow = qobject_cast<QMainWindow*>(parent());
				if (mainWindow) {
					Qt::ToolBarArea currentArea = mainWindow->toolBarArea(this);
					QString positionProperty;
					if (currentArea == Qt::TopToolBarArea) {
						positionProperty = "top";
					} else if (currentArea == Qt::BottomToolBarArea) {
						positionProperty = "bottom";
					} else if (currentArea == Qt::LeftToolBarArea) {
						positionProperty = "left";
					} else if (currentArea == Qt::RightToolBarArea) {
						positionProperty = "right";
					} else {
						positionProperty = "floating";
					}
					separator->setProperty("toolbarPosition", positionProperty);
				}
			} else {
				// Re-add the widget
				if (shouldBeVertical) {
					mainLayout->addWidget(widget, 0, Qt::AlignHCenter);
				} else {
					mainLayout->addWidget(widget);
				}
			}
		}
		
		// Add spacer (stretch) to push StreamUP button to the end
		mainLayout->addStretch();
		
		// Add StreamUP button at the end (right side for horizontal, bottom for vertical)
		if (streamupButton) {
			if (shouldBeVertical) {
				mainLayout->addWidget(streamupButton, 0, Qt::AlignHCenter);
			} else {
				mainLayout->addWidget(streamupButton);
			}
		}
		
		blog(LOG_INFO, "[StreamUP] Toolbar layout orientation updated successfully");
	}
}

void StreamUPToolbar::setupDynamicUI()
{
	// Set basic toolbar properties like obs-toolbar
	setMovable(false);
	setFloatable(false);
	setOrientation(Qt::Horizontal);
	
	// Clear existing toolbar contents
	clear();
	
	// Create a central widget with horizontal layout (will be changed to vertical if needed)
	centralWidget = new QWidget(this);
	mainLayout = new QHBoxLayout(centralWidget);
	mainLayout->setContentsMargins(8, 0, 8, 0);
	mainLayout->setSpacing(4);
	
	// Clear existing buttons
	dynamicButtons.clear();
	streamButton = nullptr;
	recordButton = nullptr;
	pauseButton = nullptr;
	replayBufferButton = nullptr;
	saveReplayButton = nullptr;
	virtualCameraButton = nullptr;
	virtualCameraConfigButton = nullptr;
	studioModeButton = nullptr;
	settingsButton = nullptr;
	streamUPSettingsButton = nullptr;
	
	// Create widgets from configuration
	bool needsStretch = false;
	for (const auto& item : toolbarConfig.items) {
		if (!item->visible) continue;
		
		if (item->type == StreamUP::ToolbarConfig::ItemType::Separator) {
			QFrame* separator = createSeparatorFromConfig(false);
			separator->setObjectName(item->id);
			separator->setProperty("separatorType", "streamup-separator");
			mainLayout->addWidget(separator);
		} else if (item->type == StreamUP::ToolbarConfig::ItemType::CustomSpacer) {
			auto spacerItem = std::static_pointer_cast<StreamUP::ToolbarConfig::CustomSpacerItem>(item);
			// Create a fixed-size spacer widget
			QWidget* spacerWidget = new QWidget(centralWidget);
			spacerWidget->setObjectName(item->id);
			spacerWidget->setFixedSize(spacerItem->size, 28); // Match button height
			spacerWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
			spacerWidget->setStyleSheet("background: transparent;");
			mainLayout->addWidget(spacerWidget);
		} else {
			QToolButton* button = createButtonFromConfig(item);
			if (button) {
				button->setObjectName(item->id);
				dynamicButtons[item->id] = button;
				
				// Check if this is the StreamUP settings button (should be on the right)
				if (auto buttonItem = std::dynamic_pointer_cast<StreamUP::ToolbarConfig::ButtonItem>(item)) {
					if (buttonItem->buttonType == "streamup_settings") {
						if (!needsStretch) {
							mainLayout->addStretch();
							needsStretch = true;
						}
					}
				}
				
				mainLayout->addWidget(button);
			}
		}
	}
	
	// Apply CSS styling
	updateToolbarStyling();
	
	// Add the central widget to toolbar
	addWidget(centralWidget);
	
	// Update layout orientation based on current position
	updateLayoutOrientation();
}

QToolButton* StreamUPToolbar::createButtonFromConfig(std::shared_ptr<StreamUP::ToolbarConfig::ToolbarItem> item)
{
	QToolButton* button = new QToolButton(centralWidget);
	button->setProperty("buttonType", "streamup-button");
	button->setFixedSize(28, 28);
	button->setIconSize(QSize(20, 20));
	button->setToolButtonStyle(Qt::ToolButtonIconOnly);
	
	if (item->type == StreamUP::ToolbarConfig::ItemType::Button) {
		auto buttonItem = std::static_pointer_cast<StreamUP::ToolbarConfig::ButtonItem>(item);
		
		// Set up built-in button
		button->setIcon(QIcon(getThemedIconPath(buttonItem->iconPath.isEmpty() ? 
			StreamUP::ToolbarConfig::ButtonRegistry::getButtonInfo(buttonItem->buttonType).defaultIcon : 
			buttonItem->iconPath)));
		button->setToolTip(buttonItem->tooltip.isEmpty() ? 
			StreamUP::ToolbarConfig::ButtonRegistry::getButtonInfo(buttonItem->buttonType).defaultTooltip : 
			buttonItem->tooltip);
		button->setCheckable(buttonItem->checkable);
		
		// Connect to appropriate slot based on button type
		if (buttonItem->buttonType == "stream") {
			streamButton = button;
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onStreamButtonClicked);
		} else if (buttonItem->buttonType == "record") {
			recordButton = button;
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onRecordButtonClicked);
		} else if (buttonItem->buttonType == "pause") {
			pauseButton = button;
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onPauseButtonClicked);
		} else if (buttonItem->buttonType == "replay_buffer") {
			replayBufferButton = button;
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onReplayBufferButtonClicked);
		} else if (buttonItem->buttonType == "save_replay") {
			saveReplayButton = button;
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onSaveReplayButtonClicked);
		} else if (buttonItem->buttonType == "virtual_camera") {
			virtualCameraButton = button;
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onVirtualCameraButtonClicked);
		} else if (buttonItem->buttonType == "virtual_camera_config") {
			virtualCameraConfigButton = button;
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onVirtualCameraConfigButtonClicked);
		} else if (buttonItem->buttonType == "studio_mode") {
			studioModeButton = button;
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onStudioModeButtonClicked);
		} else if (buttonItem->buttonType == "settings") {
			settingsButton = button;
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onSettingsButtonClicked);
		} else if (buttonItem->buttonType == "streamup_settings") {
			streamUPSettingsButton = button;
			// Use the special StreamUP logo icon
			button->setIcon(QIcon(":images/icons/social/streamup-logo-button.svg"));
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onStreamUPSettingsButtonClicked);
		}
		
	} else if (item->type == StreamUP::ToolbarConfig::ItemType::DockButton) {
		auto dockItem = std::static_pointer_cast<StreamUP::ToolbarConfig::DockButtonItem>(item);
		
		// Set up dock button
		if (!dockItem->iconPath.isEmpty()) {
			// Use themed icon system for dock button icons
			button->setIcon(QIcon(getThemedIconPath(dockItem->iconPath)));
		} else {
			// Use a default dock icon
			button->setIcon(QIcon(getThemedIconPath("settings")));
		}
		button->setToolTip(dockItem->tooltip);
		button->setCheckable(false);
		
		// Store dock action type in button's property
		button->setProperty("dockActionType", dockItem->dockButtonType);
		connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onDockButtonClicked);
	}
	
	return button;
}

QFrame* StreamUPToolbar::createSeparatorFromConfig(bool isVertical)
{
	QFrame* separator = new QFrame();
	if (isVertical) {
		separator->setFrameShape(QFrame::HLine);
		separator->setFixedSize(16, 1);
	} else {
		separator->setFrameShape(QFrame::VLine);
		separator->setFixedSize(1, 16);
	}
	separator->setFrameShadow(QFrame::Plain);
	separator->setLineWidth(1);
	return separator;
}

void StreamUPToolbar::refreshFromConfiguration()
{
	// Clear the current toolbar state
	clear();
	dynamicButtons.clear();
	
	// Reset button pointers
	streamButton = nullptr;
	recordButton = nullptr;
	pauseButton = nullptr;
	replayBufferButton = nullptr;
	saveReplayButton = nullptr;
	virtualCameraButton = nullptr;
	virtualCameraConfigButton = nullptr;
	studioModeButton = nullptr;
	settingsButton = nullptr;
	streamUPSettingsButton = nullptr;
	centralWidget = nullptr;
	mainLayout = nullptr;
	
	// Reload configuration and rebuild UI
	toolbarConfig.loadFromSettings();
	setupDynamicUI();
	updateAllButtons();
	updateIconsForTheme();
	updatePositionAwareTheme();
}

void StreamUPToolbar::clearLayout()
{
	if (mainLayout) {
		// Delete all widgets in the layout
		while (QLayoutItem* item = mainLayout->takeAt(0)) {
			if (QWidget* widget = item->widget()) {
				widget->deleteLater();
			}
			delete item;
		}
	}
	
	dynamicButtons.clear();
}

void StreamUPToolbar::onConfigureToolbarClicked()
{
	StreamUP::ToolbarConfigurator configurator(this);
	if (configurator.exec() == QDialog::Accepted) {
		// Refresh the toolbar with new configuration
		refreshFromConfiguration();
	}
}

void StreamUPToolbar::onToolbarSettingsClicked()
{
	// Open StreamUP Settings on the Toolbar Settings tab (index 1)
	StreamUP::SettingsManager::ShowSettingsDialog(1);
}

void StreamUPToolbar::onDockButtonClicked()
{
	QToolButton* button = qobject_cast<QToolButton*>(sender());
	if (!button) return;
	
	QString actionType = button->property("dockActionType").toString();
	
	// Special handling for video_capture to position popup correctly
	if (actionType == "video_capture") {
		executeDockActionWithButton(actionType, button);
	} else {
		executeDockAction(actionType);
	}
}


void StreamUPToolbar::executeDockAction(const QString& actionType)
{
	// Find the StreamUP dock to call its functions
	QWidget* mainWindow = static_cast<QWidget*>(obs_frontend_get_main_window());
	if (!mainWindow) return;
	
	StreamUPDock* dock = mainWindow->findChild<StreamUPDock*>();
	if (!dock) {
		QMessageBox::warning(this, "StreamUP Dock", 
			"StreamUP dock is not available. Please make sure it is loaded.");
		return;
	}
	
	// Call the appropriate dock function based on action type
	if (actionType == "lock_all_sources") {
		dock->ButtonToggleLockAllSources();
		// Update toolbar dock button icons after state change
		updateDockButtonIcons();
	} else if (actionType == "lock_current_sources") {
		dock->ButtonToggleLockSourcesInCurrentScene();
		// Update toolbar dock button icons after state change
		updateDockButtonIcons();
	} else if (actionType == "refresh_audio") {
		dock->ButtonRefreshAudioMonitoring();
	} else if (actionType == "refresh_browser") {
		dock->ButtonRefreshBrowserSources();
	} else if (actionType == "video_capture") {
		dock->ButtonShowVideoCapturePopup();
	} else if (actionType == "activate_video_devices") {
		dock->ButtonActivateAllVideoCaptureDevices();
	} else if (actionType == "deactivate_video_devices") {
		dock->ButtonDeactivateAllVideoCaptureDevices();
	} else if (actionType == "refresh_video_devices") {
		dock->ButtonRefreshAllVideoCaptureDevices();
	} else if (actionType == "streamup_settings") {
		// Open StreamUP settings dialog
		ShowDockConfigDialog();
	} else {
		QMessageBox::warning(this, "Unknown Action", 
			QString("Unknown dock action: %1").arg(actionType));
	}
}

void StreamUPToolbar::executeDockActionWithButton(const QString& actionType, QToolButton* button)
{
	// Find the StreamUP dock to call its functions
	QWidget* mainWindow = static_cast<QWidget*>(obs_frontend_get_main_window());
	if (!mainWindow) return;
	
	StreamUPDock* dock = mainWindow->findChild<StreamUPDock*>();
	if (!dock) {
		QMessageBox::warning(this, "StreamUP Dock", 
			"StreamUP dock is not available. Please make sure it is loaded.");
		return;
	}
	
	// Special handling for video_capture to position popup relative to toolbar button
	if (actionType == "video_capture") {
		// Check if there's already an open popup and close it
		static VideoCapturePopup* videoCapturePopup = nullptr;
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
		
		// Connect popup signals to dock methods
		connect(videoCapturePopup, &VideoCapturePopup::activateAllVideoCaptureDevices,
			dock, &StreamUPDock::ButtonActivateAllVideoCaptureDevices);
		connect(videoCapturePopup, &VideoCapturePopup::deactivateAllVideoCaptureDevices,
			dock, &StreamUPDock::ButtonDeactivateAllVideoCaptureDevices);
		connect(videoCapturePopup, &VideoCapturePopup::refreshAllVideoCaptureDevices,
			dock, &StreamUPDock::ButtonRefreshAllVideoCaptureDevices);
		
		// Connect to handle cleanup when popup is closed
		connect(videoCapturePopup, &QWidget::destroyed, [&]() {
			videoCapturePopup = nullptr;
		});
		
		// Show popup next to the toolbar button (not dock button)
		QPoint buttonPos = button->mapToGlobal(QPoint(0, 0));
		videoCapturePopup->showNearButton(buttonPos, button->size());
	} else {
		// For other actions, use regular dock action
		executeDockAction(actionType);
	}
}

void StreamUPToolbar::contextMenuEvent(QContextMenuEvent* event)
{
	if (contextMenu) {
		contextMenu->popup(event->globalPos());
	}
}

#include "streamup-toolbar.moc"
