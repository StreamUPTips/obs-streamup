#include "streamup-toolbar.hpp"
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
#include <util/config-file.h>

StreamUPToolbar::StreamUPToolbar(QWidget *parent) : QToolBar(parent), 
	streamButton(nullptr), recordButton(nullptr), pauseButton(nullptr), 
	replayBufferButton(nullptr), saveReplayButton(nullptr), virtualCameraButton(nullptr), 
	virtualCameraConfigButton(nullptr), studioModeButton(nullptr), settingsButton(nullptr),
	streamUPSettingsButton(nullptr)
{
	setObjectName("StreamUPToolbar");
	setWindowTitle("StreamUP Controls");

	setupUI();
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

void StreamUPToolbar::setupUI()
{
	// Set basic toolbar properties like obs-toolbar
	setMovable(false);
	setFloatable(false);
	setOrientation(Qt::Horizontal);
	
	// Create a central widget with horizontal layout
	QWidget* centralWidget = new QWidget(this);
	QHBoxLayout* layout = new QHBoxLayout(centralWidget);
	layout->setContentsMargins(8, 0, 8, 0);
	layout->setSpacing(4);
	
	// === STREAMING SECTION ===
	// Create streaming button
	streamButton = new QToolButton(centralWidget);
	streamButton->setFixedSize(28, 28);
	streamButton->setIcon(QIcon(getThemedIconPath("streaming-inactive")));
	streamButton->setIconSize(QSize(20, 20));
	streamButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	streamButton->setToolTip("Start/Stop Streaming");
	streamButton->setCheckable(true);
	connect(streamButton, &QToolButton::clicked, this, &StreamUPToolbar::onStreamButtonClicked);
	layout->addWidget(streamButton);
	
	// Separator
	layout->addWidget(createSeparator());
	
	// === RECORDING SECTION ===
	// Create recording button
	recordButton = new QToolButton(centralWidget);
	recordButton->setFixedSize(28, 28);
	recordButton->setIcon(QIcon(getThemedIconPath("record-off")));
	recordButton->setIconSize(QSize(20, 20));
	recordButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	recordButton->setToolTip("Start/Stop Recording");
	recordButton->setCheckable(true);
	connect(recordButton, &QToolButton::clicked, this, &StreamUPToolbar::onRecordButtonClicked);
	layout->addWidget(recordButton);
	
	// Create pause recording button (initially hidden like OBS)
	pauseButton = new QToolButton(centralWidget);
	pauseButton->setFixedSize(28, 28);
	pauseButton->setIcon(QIcon(getThemedIconPath("pause")));
	pauseButton->setIconSize(QSize(20, 20));
	pauseButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	pauseButton->setToolTip("Pause/Resume Recording");
	pauseButton->setCheckable(true);
	pauseButton->setVisible(false); // Hidden by default like OBS controls dock
	connect(pauseButton, &QToolButton::clicked, this, &StreamUPToolbar::onPauseButtonClicked);
	layout->addWidget(pauseButton);
	
	// Separator
	layout->addWidget(createSeparator());
	
	// === REPLAY BUFFER SECTION ===
	// Create replay buffer button
	replayBufferButton = new QToolButton(centralWidget);
	replayBufferButton->setFixedSize(28, 28);
	replayBufferButton->setIcon(QIcon(getThemedIconPath("replay-buffer-off")));
	replayBufferButton->setIconSize(QSize(20, 20));
	replayBufferButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	replayBufferButton->setToolTip("Start/Stop Replay Buffer");
	replayBufferButton->setCheckable(true);
	connect(replayBufferButton, &QToolButton::clicked, this, &StreamUPToolbar::onReplayBufferButtonClicked);
	layout->addWidget(replayBufferButton);
	
	// Create save replay button (initially hidden like OBS)
	saveReplayButton = new QToolButton(centralWidget);
	saveReplayButton->setFixedSize(28, 28);
	saveReplayButton->setIcon(QIcon(getThemedIconPath("save-replay")));
	saveReplayButton->setIconSize(QSize(20, 20)); // Slightly smaller
	saveReplayButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	saveReplayButton->setToolTip("Save Replay");
	saveReplayButton->setCheckable(false); // Save replay is not a toggle
	saveReplayButton->setVisible(false); // Hidden by default like OBS controls dock
	connect(saveReplayButton, &QToolButton::clicked, this, &StreamUPToolbar::onSaveReplayButtonClicked);
	layout->addWidget(saveReplayButton);
	
	// Separator
	layout->addWidget(createSeparator());
	
	// === VIRTUAL CAMERA SECTION ===
	// Create virtual camera button
	virtualCameraButton = new QToolButton(centralWidget);
	virtualCameraButton->setObjectName("virtualCameraButton");
	virtualCameraButton->setFixedSize(28, 28);
	virtualCameraButton->setIcon(QIcon(getThemedIconPath("virtual-camera")));
	virtualCameraButton->setIconSize(QSize(20, 20));
	virtualCameraButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	virtualCameraButton->setToolTip("Start/Stop Virtual Camera");
	virtualCameraButton->setCheckable(true);
	connect(virtualCameraButton, &QToolButton::clicked, this, &StreamUPToolbar::onVirtualCameraButtonClicked);
	layout->addWidget(virtualCameraButton);
	
	// Create virtual camera config button (like OBS controls dock)
	virtualCameraConfigButton = new QToolButton(centralWidget);
	virtualCameraConfigButton->setFixedSize(28, 28);
	virtualCameraConfigButton->setIcon(QIcon(getThemedIconPath("virtual-camera-settings")));
	virtualCameraConfigButton->setIconSize(QSize(20, 20)); // Slightly smaller icon
	virtualCameraConfigButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	virtualCameraConfigButton->setToolTip("Virtual Camera Configuration");
	virtualCameraConfigButton->setCheckable(false);
	connect(virtualCameraConfigButton, &QToolButton::clicked, this, &StreamUPToolbar::onVirtualCameraConfigButtonClicked);
	layout->addWidget(virtualCameraConfigButton);
	
	// Separator
	layout->addWidget(createSeparator());
	
	// === STUDIO MODE SECTION ===
	// Create studio mode button
	studioModeButton = new QToolButton(centralWidget);
	studioModeButton->setObjectName("studioModeButton");
	studioModeButton->setFixedSize(28, 28);
	studioModeButton->setIcon(QIcon(getThemedIconPath("studio-mode")));
	studioModeButton->setIconSize(QSize(20, 20));
	studioModeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	studioModeButton->setToolTip("Toggle Studio Mode");
	studioModeButton->setCheckable(true);
	connect(studioModeButton, &QToolButton::clicked, this, &StreamUPToolbar::onStudioModeButtonClicked);
	layout->addWidget(studioModeButton);
	
	// Separator
	layout->addWidget(createSeparator());
	
	// === SETTINGS SECTION ===
	// Create settings button
	settingsButton = new QToolButton(centralWidget);
	settingsButton->setFixedSize(28, 28);
	settingsButton->setIcon(QIcon(getThemedIconPath("settings")));
	settingsButton->setIconSize(QSize(20, 20));
	settingsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	settingsButton->setToolTip("Open Settings");
	settingsButton->setCheckable(false); // Settings button is not checkable
	connect(settingsButton, &QToolButton::clicked, this, &StreamUPToolbar::onSettingsButtonClicked);
	layout->addWidget(settingsButton);
	
	// Add spacer to push StreamUP settings button to the right
	layout->addStretch();
	
	// === STREAMUP SETTINGS SECTION (RIGHT SIDE) ===
	// Create StreamUP settings button
	streamUPSettingsButton = new QToolButton(centralWidget);
	streamUPSettingsButton->setFixedSize(28, 28);
	streamUPSettingsButton->setIcon(QIcon(":images/icons/social/streamup-logo-button.svg"));
	streamUPSettingsButton->setIconSize(QSize(20, 20));
	streamUPSettingsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	streamUPSettingsButton->setToolTip("Open StreamUP Settings");
	streamUPSettingsButton->setCheckable(false);
	connect(streamUPSettingsButton, &QToolButton::clicked, this, &StreamUPToolbar::onStreamUPSettingsButtonClicked);
	layout->addWidget(streamUPSettingsButton);
	
	// Apply CSS styling for active states
	setStyleSheet(R"(
		QToolButton {
			background: transparent;
			border: none;
			border-radius: 4px;
			padding: 2px;
		}
		QToolButton:hover {
			background-color: rgba(255, 255, 255, 0.1);
		}
		QToolButton:pressed {
			background-color: rgba(255, 255, 255, 0.2);
		}
		QToolButton:checked {
			background: transparent;
			border: none;
		}
		QToolButton:checked:hover {
			background-color: rgba(255, 255, 255, 0.1);
		}
		/* Special styling for virtual camera and studio mode buttons when active */
		QToolButton[objectName="virtualCameraButton"]:checked {
			background-color: #0f7bcf;
			border: 1px solid #0a5a9c;
		}
		QToolButton[objectName="virtualCameraButton"]:checked:hover {
			background-color: #1584d6;
		}
		QToolButton[objectName="studioModeButton"]:checked {
			background-color: #0f7bcf;
			border: 1px solid #0a5a9c;
		}
		QToolButton[objectName="studioModeButton"]:checked:hover {
			background-color: #1584d6;
		}
		QFrame {
			background-color: rgba(128, 128, 128, 0.3);
			border: none;
		}
	)");
	
	// Add the central widget to toolbar
	addWidget(centralWidget);
	
	// Don't update button visibility immediately as OBS may not be fully initialized
	// It will be updated when OBS_FRONTEND_EVENT_FINISHED_LOADING is received
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
		// Theme changed, update icons for new theme
		blog(LOG_INFO, "[StreamUP] Received OBS_FRONTEND_EVENT_THEME_CHANGED event");
		toolbar->updateIconsForTheme();
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
	
	// Set object names and properties based on position for theme creators
	if (currentArea == Qt::TopToolBarArea) {
		setObjectName("StreamUPToolbar-Top");
		setProperty("toolbarPosition", "top");
		blog(LOG_DEBUG, "[StreamUP] Updated toolbar theme for top position");
	} else if (currentArea == Qt::BottomToolBarArea) {
		setObjectName("StreamUPToolbar-Bottom"); 
		setProperty("toolbarPosition", "bottom");
		blog(LOG_DEBUG, "[StreamUP] Updated toolbar theme for bottom position");
	} else {
		// Fallback for floating or other positions
		setObjectName("StreamUPToolbar");
		setProperty("toolbarPosition", "floating");
		blog(LOG_DEBUG, "[StreamUP] Updated toolbar theme for floating position");
	}
	
	// Force style sheet refresh to apply position-based styling
	style()->unpolish(this);
	style()->polish(this);
	
	blog(LOG_INFO, "[StreamUP] Toolbar position-aware theme updated");
}

#include "streamup-toolbar.moc"
