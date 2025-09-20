#include "streamup-toolbar.hpp"
#include "../utilities/debug-logger.hpp"
#include "streamup-toolbar-configurator.hpp"
#include "dock/streamup-dock.hpp"
#include "../video-capture-popup.hpp"
#include "ui-styles.hpp"
#include "ui-helpers.hpp"
#include "settings-manager.hpp"
#include "obs-hotkey-manager.hpp"
#include <obs-module.h>
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
#include <QFile>
#include <QTimer>
#include <util/config-file.h>

StreamUPToolbar::StreamUPToolbar(QWidget *parent) : QToolBar(parent),
	streamButton(nullptr), recordButton(nullptr), pauseButton(nullptr),
	replayBufferButton(nullptr), saveReplayButton(nullptr), virtualCameraButton(nullptr),
	virtualCameraConfigButton(nullptr), studioModeButton(nullptr), settingsButton(nullptr),
	streamUPSettingsButton(nullptr), centralWidget(nullptr), mainLayout(nullptr),
	contextMenu(nullptr), configureAction(nullptr), iconUpdateTimer(nullptr), m_updateBatchTimer(nullptr)
{
	setObjectName("StreamUPToolbar");
	setWindowTitle(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Title")));

	// Initialize optimized update system
	m_updateBatchTimer = new QTimer(this);
	m_updateBatchTimer->setSingleShot(true);
	m_updateBatchTimer->setInterval(50); // 50ms batching delay
	connect(m_updateBatchTimer, &QTimer::timeout, this, &StreamUPToolbar::processBatchedUpdates);

	// Setup context menu
	contextMenu = new QMenu(this);
	configureAction = contextMenu->addAction(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.Title")));
	connect(configureAction, &QAction::triggered, this, &StreamUPToolbar::onConfigureToolbarClicked);

	toolbarSettingsAction = contextMenu->addAction(QString::fromUtf8(obs_module_text("StreamUP.Settings.ToolbarSettings")));
	connect(toolbarSettingsAction, &QAction::triggered, this, &StreamUPToolbar::onToolbarSettingsClicked);

	// Load configuration and setup UI
	toolbarConfig.loadFromSettings();
	setupDynamicUI();

	// Preload commonly used icons for better performance
	preloadCommonIcons();

	// Initial update using optimized system
	scheduleUpdate();

	// Set initial position-aware theming (will be updated when actually added to main window)
	updatePositionAwareTheme();

	// Register for OBS frontend events to update button states
	obs_frontend_add_event_callback(OnFrontendEvent, this);
}

StreamUPToolbar::~StreamUPToolbar()
{
	// Remove event callback
	obs_frontend_remove_event_callback(OnFrontendEvent, this);
	
	// Clear caches on destruction
	clearIconCache();
	clearStyleSheetCache();
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

// Old setupUI function removed - now using setupDynamicUI() for configuration-based toolbar

void StreamUPToolbar::updateToolbarStyling()
{
	// Check if cached stylesheet is valid (prevents redundant generation)
	if (styleSheetCacheValid && !cachedStyleSheet.isEmpty()) {
		setStyleSheet(cachedStyleSheet);
		return;
	}
	
	// Generate new stylesheet with theme-aware styling using StreamUP UI constants
	cachedStyleSheet = QString(R"(
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
		/* Spacer widgets styling */
		QWidget[objectName*="spacer"] {
			background: transparent;
		}
	)").arg(StreamUP::UIStyles::Sizes::SPACE_4)                    // border-radius
	   .arg(StreamUP::UIStyles::Sizes::SPACE_2)                    // padding
	   .arg(StreamUP::UIStyles::Colors::HOVER_OVERLAY)             // hover background
	   .arg(StreamUP::UIStyles::Colors::PRIMARY_ALPHA_30)          // pressed background
	   .arg(StreamUP::UIStyles::Colors::PRIMARY_COLOR)             // checked background
	   .arg(StreamUP::UIStyles::Colors::PRIMARY_INACTIVE)          // checked border
	   .arg(StreamUP::UIStyles::Colors::PRIMARY_HOVER)             // checked hover background
	   .arg(StreamUP::UIStyles::Colors::BORDER_SUBTLE);            // frame background
	
	// Mark cache as valid and apply stylesheet
	styleSheetCacheValid = true;
	setStyleSheet(cachedStyleSheet);
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
		streamButton->setIcon(getCachedIcon(iconName));
		StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Update Stream Button", "Updated stream button with cached icon: %s", iconName.toUtf8().constData());
		streamButton->setToolTip(streaming ? "Stop Streaming" : "Start Streaming");
	}
}

void StreamUPToolbar::updateRecordButton()
{
	if (recordButton) {
		bool recording = obs_frontend_recording_active();
		recordButton->setChecked(recording);
		QString iconName = recording ? "record-on" : "record-off";
		recordButton->setIcon(getCachedIcon(iconName));
		recordButton->setToolTip(recording ? "Stop Recording" : "Start Recording");
		
		// Control pause button visibility based on recording state and compatibility
		if (pauseButton) {
			bool canPause = recording && isRecordingPausable();
			// Show pause button only when recording is active and pausable
			pauseButton->setVisible(canPause);
			pauseButton->setEnabled(canPause);
		}
	}
}

void StreamUPToolbar::updatePauseButton()
{
	if (isReconstructingUI) {
		return;
	}
	if (pauseButton) {
		bool recording = obs_frontend_recording_active();
		bool paused = obs_frontend_recording_paused();
		pauseButton->setEnabled(recording);
		pauseButton->setChecked(paused);
		pauseButton->setIcon(getCachedIcon("pause"));
		pauseButton->setToolTip(paused ? "Resume Recording" : "Pause Recording");
	}
}

void StreamUPToolbar::updateReplayBufferButton()
{
	if (replayBufferButton) {
		bool active = obs_frontend_replay_buffer_active();
		replayBufferButton->setChecked(active);
		QString iconName = active ? "replay-buffer-on" : "replay-buffer-off";
		replayBufferButton->setIcon(getCachedIcon(iconName));
		replayBufferButton->setToolTip(active ? "Stop Replay Buffer" : "Start Replay Buffer");
		
		// Control save replay button visibility based on replay buffer state
		if (saveReplayButton) {
			// Show save replay button only when replay buffer is active
			saveReplayButton->setVisible(active);
			saveReplayButton->setEnabled(active);
		}
	}
}

void StreamUPToolbar::updateSaveReplayButton()
{
	if (isReconstructingUI) {
		return;
	}
	if (saveReplayButton) {
		bool replayActive = obs_frontend_replay_buffer_active();
		
		// Ensure icon is set correctly
		saveReplayButton->setIcon(getCachedIcon("save-replay"));
		
		// Show only when replay buffer is active, enable/disable based on recording pause state
		saveReplayButton->setVisible(replayActive);
		bool recordingPaused = obs_frontend_recording_paused();
		saveReplayButton->setEnabled(replayActive && !recordingPaused);
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
		virtualCameraConfigButton->setIcon(getCachedIcon("virtual-camera-settings"));
	}
}

void StreamUPToolbar::updateSettingsButton()
{
	if (settingsButton) {
		settingsButton->setIcon(getCachedIcon("settings"));
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
				button->setIcon(getCachedIcon(iconName));
			}
		} else if (actionType == "lock_current_sources") {
			// Check if current scene sources are locked
			QWidget* mainWindow = static_cast<QWidget*>(obs_frontend_get_main_window());
			StreamUPDock* dock = mainWindow->findChild<StreamUPDock*>();
			if (dock) {
				bool currentLocked = dock->AreAllSourcesLockedInCurrentScene();
				QString iconName = currentLocked ? "current-scene-source-locked" : "current-scene-source-unlocked";
				button->setIcon(getCachedIcon(iconName));
			}
		}
	}
}

void StreamUPToolbar::updateAllButtons()
{
	// Use the new efficient batched update system
	updateButtonStatesEfficiently();

	// Update non-state dependent buttons individually
	updateVirtualCameraConfigButton();
	updateSettingsButton();
	updateStreamUPSettingsButton();
}

void StreamUPToolbar::OnFrontendEvent(enum obs_frontend_event event, void *data)
{
	StreamUPToolbar* toolbar = static_cast<StreamUPToolbar*>(data);
	if (!toolbar) return;

	// Use optimized batched update system for all state-changing events
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED:
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		// Schedule efficient batched update instead of individual updates
		toolbar->scheduleUpdate();
		break;

	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		// Settings may have changed, force immediate update with button visibility
		toolbar->updateButtonVisibility();
		toolbar->scheduleUpdate();
		break;
		
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
	case OBS_FRONTEND_EVENT_THEME_CHANGED:
		// Theme changed, update icons and styling for new theme
		toolbar->updateIconsForTheme();
		toolbar->updateToolbarStyling();
		break;
#endif
		
	default:
		break;
	}
}

QString StreamUPToolbar::getThemedIconPath(const QString& iconName)
{
	// Use the centralized theme-aware icon helper
	return StreamUP::UIHelpers::GetThemedIconPath(iconName);
}

QIcon StreamUPToolbar::getCachedIcon(const QString& iconName)
{
	// Check if theme has changed and invalidate caches if needed
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
	bool isDark = obs_frontend_is_theme_dark();
#else
	bool isDark = false; // Fallback for older OBS versions
#endif
	if (currentThemeIsDark != isDark) {
		clearIconCache();
		clearStyleSheetCache();  // Stylesheet also depends on theme
		currentThemeIsDark = isDark;
	}
	
	// Check cache first
	auto it = iconCache.find(iconName);
	if (it != iconCache.end()) {
		return it.value();
	}
	
	// Create and cache new icon
	QString iconPath = getThemedIconPath(iconName);
	QIcon icon(iconPath);
	iconCache.insert(iconName, icon);
	
	return icon;
}

void StreamUPToolbar::clearIconCache()
{
	iconCache.clear();
}

void StreamUPToolbar::clearStyleSheetCache()
{
	cachedStyleSheet.clear();
	styleSheetCacheValid = false;
}

void StreamUPToolbar::preloadCommonIcons()
{
	// Preload frequently used icons to reduce load time during updates
	const QStringList commonIcons = {
		"streaming", "streaming-inactive",
		"record-on", "record-off",
		"pause", "save-replay",
		"replay-buffer-on", "replay-buffer-off",
		"virtual-camera-on", "virtual-camera-off",
		"virtual-camera-settings", "settings"
	};

	for (const QString& iconName : commonIcons) {
		getCachedIcon(iconName); // This will cache the icon
	}

	StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Icon Preload", "Preloaded %d common icons", commonIcons.size());
}

void StreamUPToolbar::scheduleUpdate()
{
	if (!m_updatesPending) {
		m_updatesPending = true;
		m_updateBatchTimer->start();
	}
}

void StreamUPToolbar::processBatchedUpdates()
{
	if (!m_updatesPending) {
		return;
	}

	m_updatesPending = false;
	updateButtonStatesEfficiently();
}

void StreamUPToolbar::updateButtonStatesEfficiently()
{
	if (isReconstructingUI) {
		return;
	}

	// Batch all state checks to minimize OBS API calls
	bool streaming = obs_frontend_streaming_active();
	bool recording = obs_frontend_recording_active();
	bool paused = obs_frontend_recording_paused();
	bool replayActive = obs_frontend_replay_buffer_active();
	bool vcamActive = obs_frontend_virtualcam_active();
	bool studioMode = obs_frontend_preview_program_mode_active();

	// Update all buttons efficiently with batched state
	if (streamButton) {
		streamButton->setChecked(streaming);
		QString iconName = streaming ? "streaming" : "streaming-inactive";
		streamButton->setIcon(getCachedIcon(iconName));
		streamButton->setToolTip(streaming ? "Stop Streaming" : "Start Streaming");
	}

	if (recordButton) {
		recordButton->setChecked(recording);
		QString iconName = recording ? "record-on" : "record-off";
		recordButton->setIcon(getCachedIcon(iconName));
		recordButton->setToolTip(recording ? "Stop Recording" : "Start Recording");
	}

	if (pauseButton) {
		bool canPause = recording && isRecordingPausable();
		pauseButton->setVisible(canPause);
		pauseButton->setEnabled(canPause);
		pauseButton->setChecked(paused);
		pauseButton->setIcon(getCachedIcon("pause"));
		pauseButton->setToolTip(paused ? "Resume Recording" : "Pause Recording");
	}

	if (replayBufferButton) {
		replayBufferButton->setChecked(replayActive);
		QString iconName = replayActive ? "replay-buffer-on" : "replay-buffer-off";
		replayBufferButton->setIcon(getCachedIcon(iconName));
		replayBufferButton->setToolTip(replayActive ? "Stop Replay Buffer" : "Start Replay Buffer");
	}

	if (saveReplayButton) {
		saveReplayButton->setVisible(replayActive);
		saveReplayButton->setEnabled(replayActive && !paused);
		saveReplayButton->setIcon(getCachedIcon("save-replay"));
	}

	if (virtualCameraButton) {
		virtualCameraButton->setChecked(vcamActive);
		QString iconName = vcamActive ? "virtual-camera-on" : "virtual-camera-off";
		virtualCameraButton->setIcon(getCachedIcon(iconName));
		virtualCameraButton->setToolTip(vcamActive ? "Stop Virtual Camera" : "Start Virtual Camera");
	}

	if (studioModeButton) {
		studioModeButton->setChecked(studioMode);
		studioModeButton->setToolTip(studioMode ? "Disable Studio Mode" : "Enable Studio Mode");
	}

	StreamUP::DebugLogger::LogDebug("Toolbar", "Batch Update", "Completed efficient button state update");
}

void StreamUPToolbar::updateIconsForTheme()
{
	// Update all button icons for the current theme using cached icons
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
	bool isDark = obs_frontend_is_theme_dark();
#else
	bool isDark = false; // Fallback for older OBS versions
#endif
	
	// Update buttons with cached icons (eliminates redundant QIcon construction)
	if (streamButton) {
		bool streaming = obs_frontend_streaming_active();
		QString iconName = streaming ? "streaming" : "streaming-inactive";
		streamButton->setIcon(getCachedIcon(iconName));
	}
	
	if (recordButton) {
		bool recording = obs_frontend_recording_active();
		QString iconName = recording ? "record-on" : "record-off";
		recordButton->setIcon(getCachedIcon(iconName));
	}
	
	if (pauseButton) {
		pauseButton->setIcon(getCachedIcon("pause"));
	}
	
	if (replayBufferButton) {
		bool active = obs_frontend_replay_buffer_active();
		QString iconName = active ? "replay-buffer-on" : "replay-buffer-off";
		replayBufferButton->setIcon(getCachedIcon(iconName));
	}
	
	if (saveReplayButton && saveReplayButton->isVisible()) {
		saveReplayButton->setIcon(getCachedIcon("save-replay"));
	}
	
	if (virtualCameraButton) {
		virtualCameraButton->setIcon(getCachedIcon("virtual-camera"));
	}
	
	if (virtualCameraConfigButton) {
		virtualCameraConfigButton->setIcon(getCachedIcon("virtual-camera-settings"));
	}
	
	if (studioModeButton) {
		studioModeButton->setIcon(getCachedIcon("studio-mode"));
	}
	
	if (settingsButton) {
		settingsButton->setIcon(getCachedIcon("settings"));
	}
	
	// StreamUP settings button keeps its original icon (social icon, not cached)
}

void StreamUPToolbar::updatePositionAwareTheme()
{
	// Get current toolbar position from the main window
	QMainWindow* mainWindow = qobject_cast<QMainWindow*>(parent());
	if (!mainWindow) {
		StreamUP::DebugLogger::LogWarning("Toolbar", "Theming: Unable to get main window for position-aware theming");
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
	} else if (currentArea == Qt::BottomToolBarArea) {
		setObjectName("StreamUPToolbar-Bottom"); 
		positionSuffix = "-Bottom";
		positionProperty = "bottom";
	} else if (currentArea == Qt::LeftToolBarArea) {
		setObjectName("StreamUPToolbar-Left");
		positionSuffix = "-Left";
		positionProperty = "left";
	} else if (currentArea == Qt::RightToolBarArea) {
		setObjectName("StreamUPToolbar-Right");
		positionSuffix = "-Right";
		positionProperty = "right";
	} else {
		// Fallback for floating or other positions
		setObjectName("StreamUPToolbar");
		positionSuffix = "";
		positionProperty = "floating";
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
	
}

void StreamUPToolbar::updateLayoutOrientation()
{
	if (!centralWidget || !mainLayout) {
		StreamUP::DebugLogger::LogWarning("Toolbar", "Layout: Cannot update layout orientation - missing central widget or layout");
		return;
	}
	
	// Get current toolbar position from the main window
	QMainWindow* mainWindow = qobject_cast<QMainWindow*>(parent());
	if (!mainWindow) {
		StreamUP::DebugLogger::LogWarning("Toolbar", "Layout: Unable to get main window for layout orientation update");
		return;
	}
	
	Qt::ToolBarArea currentArea = mainWindow->toolBarArea(this);
	bool shouldBeVertical = (currentArea == Qt::LeftToolBarArea || currentArea == Qt::RightToolBarArea);
	bool currentlyVertical = (mainLayout->direction() == QBoxLayout::TopToBottom || mainLayout->direction() == QBoxLayout::BottomToTop);
	
	// Only rebuild layout if orientation needs to change
	if (shouldBeVertical != currentlyVertical) {
		
		// Store all current widgets in order with their types
		QList<QWidget*> widgets;
		QList<QString> widgetTypes; // "separator", "spacer", "button", or "other"
		
		// Extract widgets from current layout
		while (mainLayout->count() > 0) {
			QLayoutItem* item = mainLayout->takeAt(0);
			if (item->widget()) {
				QWidget* widget = item->widget();
				widgets.append(widget);
				
				// Determine widget type for proper handling during rebuild
				QFrame* frame = qobject_cast<QFrame*>(widget);
				if (frame && (frame->frameShape() == QFrame::VLine || frame->frameShape() == QFrame::HLine)) {
					widgetTypes.append("separator");
				} else if (widget->objectName().contains("spacer")) {
					widgetTypes.append("spacer");
				} else if (qobject_cast<QToolButton*>(widget)) {
					widgetTypes.append("button");
				} else {
					widgetTypes.append("other");
				}
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
		
		// Re-add widgets with proper orientation handling, including StreamUP button positioning
		QWidget* streamupButton = nullptr;
		QList<QWidget*> mainWidgets;
		QList<QString> mainWidgetTypes;
		
		// Separate StreamUP button from main widgets
		for (int i = 0; i < widgets.size(); ++i) {
			QWidget* widget = widgets[i];
			
			// Check if this is the StreamUP settings button
			if (widget == streamUPSettingsButton) {
				streamupButton = widget;
			} else {
				mainWidgets.append(widget);
				mainWidgetTypes.append(widgetTypes[i]);
			}
		}
		
		// Add main widgets first
		for (int i = 0; i < mainWidgets.size(); ++i) {
			QWidget* widget = mainWidgets[i];
			QString widgetType = mainWidgetTypes[i];
			
			if (widgetType == "separator") {
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
			} else if (widgetType == "spacer") {
				// Update spacer dimensions for new orientation
				// Extract the spacer size from the old dimensions and apply to new orientation
				QSize oldSize = widget->size();
				int spacerSize;
				if (currentlyVertical) {
					// Was vertical, spacer size was the height
					spacerSize = oldSize.height();
				} else {
					// Was horizontal, spacer size was the width  
					spacerSize = oldSize.width();
				}
				
				// Apply new dimensions based on new orientation
				if (shouldBeVertical) {
					// Now vertical, spacer size becomes the height
					widget->setFixedSize(28, spacerSize);
				} else {
					// Now horizontal, spacer size becomes the width
					widget->setFixedSize(spacerSize, 28);
				}
				
				// Re-add the spacer widget
				if (shouldBeVertical) {
					mainLayout->addWidget(widget, 0, Qt::AlignHCenter);
				} else {
					mainLayout->addWidget(widget);
				}
			} else {
				// Re-add other widgets (buttons, etc.)
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
		
	}
}

void StreamUPToolbar::setupDynamicUI()
{
	// Set flag to prevent updates during reconstruction
	isReconstructingUI = true;
	
	// Clear stylesheet cache since UI is being reconstructed
	clearStyleSheetCache();
	
	// Set basic toolbar properties like obs-toolbar
	setMovable(false);
	setFloatable(false);
	setOrientation(Qt::Horizontal);
	
	// Clear existing toolbar contents
	clear();
	
	// Delete old central widget if it exists to ensure complete cleanup
	if (centralWidget) {
		centralWidget->setParent(nullptr);
		centralWidget->deleteLater();
		centralWidget = nullptr;
		mainLayout = nullptr;
	}
	
	// Create a new central widget with horizontal layout (will be changed to vertical if needed)
	centralWidget = new QWidget(this);
	mainLayout = new QHBoxLayout(centralWidget);
	mainLayout->setContentsMargins(8, 0, 8, 0);
	mainLayout->setSpacing(4);
	
	// Clear existing buttons and properly clean up old references
	dynamicButtons.clear();
	
	// Delete old button instances if they exist (more aggressive cleanup)
	if (pauseButton) {
		pauseButton->setParent(nullptr);
		pauseButton->deleteLater();
	}
	if (saveReplayButton) {
		saveReplayButton->setParent(nullptr);
		saveReplayButton->deleteLater();
	}
	
	// Also delete all other button references to ensure clean slate
	if (streamButton) {
		streamButton->setParent(nullptr);
		streamButton->deleteLater();
	}
	if (recordButton) {
		recordButton->setParent(nullptr);
		recordButton->deleteLater();
	}
	if (replayBufferButton) {
		replayBufferButton->setParent(nullptr);
		replayBufferButton->deleteLater();
	}
	if (virtualCameraButton) {
		virtualCameraButton->setParent(nullptr);
		virtualCameraButton->deleteLater();
	}
	if (virtualCameraConfigButton) {
		virtualCameraConfigButton->setParent(nullptr);
		virtualCameraConfigButton->deleteLater();
	}
	if (studioModeButton) {
		studioModeButton->setParent(nullptr);
		studioModeButton->deleteLater();
	}
	if (settingsButton) {
		settingsButton->setParent(nullptr);
		settingsButton->deleteLater();
	}
	if (streamUPSettingsButton) {
		streamUPSettingsButton->setParent(nullptr);
		streamUPSettingsButton->deleteLater();
	}
	
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
	
	// Create widgets from configuration in two passes to ensure proper positioning
	// Get flattened items (ignoring groups since they're just for UI organization)
	auto flattenedItems = toolbarConfig.getFlattenedItems();
	
	// First pass: Add all items that are NOT StreamUP settings buttons
	for (const auto& item : flattenedItems) {
		if (!item->visible) continue;
		
		// Skip StreamUP settings buttons in first pass
		bool isStreamUPSettings = false;
		if (item->type == StreamUP::ToolbarConfig::ItemType::Button) {
			if (auto buttonItem = std::dynamic_pointer_cast<StreamUP::ToolbarConfig::ButtonItem>(item)) {
				isStreamUPSettings = (buttonItem->buttonType == "streamup_settings");
			}
		}
		if (isStreamUPSettings) continue;
		
		if (item->type == StreamUP::ToolbarConfig::ItemType::Separator) {
			QFrame* separator = createSeparatorFromConfig(false);
			separator->setObjectName(item->id);
			separator->setProperty("separatorType", "streamup-separator");
			mainLayout->addWidget(separator);
		} else if (item->type == StreamUP::ToolbarConfig::ItemType::CustomSpacer) {
			auto spacerItem = std::static_pointer_cast<StreamUP::ToolbarConfig::CustomSpacerItem>(item);
			// Create a fixed-size spacer widget with orientation-aware dimensions
			QWidget* spacerWidget = new QWidget(centralWidget);
			// Ensure objectName contains "spacer" for CSS selector matching
			spacerWidget->setObjectName(QString("spacer_%1").arg(item->id));
			
			// Set dimensions based on current toolbar orientation
			QMainWindow* mainWindow = qobject_cast<QMainWindow*>(parent());
			if (mainWindow) {
				Qt::ToolBarArea currentArea = mainWindow->toolBarArea(this);
				bool isVertical = (currentArea == Qt::LeftToolBarArea || currentArea == Qt::RightToolBarArea);
				
				if (isVertical) {
					// For vertical toolbar, spacer height should be the configured size
					spacerWidget->setFixedSize(28, spacerItem->size); // Match button width, use configured size for height
				} else {
					// For horizontal toolbar, spacer width should be the configured size
					spacerWidget->setFixedSize(spacerItem->size, 28); // Use configured size for width, match button height
				}
			} else {
				// Fallback to horizontal orientation if can't determine position
				spacerWidget->setFixedSize(spacerItem->size, 28);
			}
			
			spacerWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
			// Styling handled by parent container CSS (eliminates individual setStyleSheet call)
			mainLayout->addWidget(spacerWidget);
		} else {
			QToolButton* button = createButtonFromConfig(item);
			if (button) {
				button->setObjectName(item->id);
				dynamicButtons[item->id] = button;
				mainLayout->addWidget(button);
				
				// Check if this button needs companion buttons (pause/save_replay)
				if (item->type == StreamUP::ToolbarConfig::ItemType::Button) {
					auto buttonItem = std::dynamic_pointer_cast<StreamUP::ToolbarConfig::ButtonItem>(item);
					if (buttonItem) {
						// Add pause button immediately after record button
						if (buttonItem->buttonType == "record") {
							
							// Create new pause button for this position
							QToolButton* newPauseButton = new QToolButton(centralWidget);
											newPauseButton->setProperty("buttonType", "streamup-button");
							newPauseButton->setFixedSize(28, 28);
							newPauseButton->setIconSize(QSize(20, 20));
							newPauseButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
							newPauseButton->setIcon(getCachedIcon("pause"));
											newPauseButton->setToolTip("Pause Recording");
							newPauseButton->setCheckable(true);
							newPauseButton->setObjectName("pause_dynamic");
							// Start hidden - will be shown when recording is active and pausable
							newPauseButton->setVisible(false);
							connect(newPauseButton, &QToolButton::clicked, this, &StreamUPToolbar::onPauseButtonClicked);
							
							// Replace the old pause button reference
							pauseButton = newPauseButton;
							recordButton = button;
							mainLayout->addWidget(pauseButton);
						}
						// Add save_replay button immediately after replay_buffer button
						else if (buttonItem->buttonType == "replay_buffer") {
											// Create new save_replay button for this position
							QToolButton* newSaveReplayButton = new QToolButton(centralWidget);
							newSaveReplayButton->setProperty("buttonType", "streamup-button");
							newSaveReplayButton->setFixedSize(28, 28);
							newSaveReplayButton->setIconSize(QSize(20, 20));
							newSaveReplayButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
							newSaveReplayButton->setIcon(getCachedIcon("save-replay"));
											newSaveReplayButton->setToolTip("Save Replay");
							newSaveReplayButton->setCheckable(false);
							newSaveReplayButton->setObjectName("save_replay_dynamic");
							// Start hidden - will be shown when replay buffer is active
							newSaveReplayButton->setVisible(false);
							connect(newSaveReplayButton, &QToolButton::clicked, this, &StreamUPToolbar::onSaveReplayButtonClicked);
							
							// Replace the old save replay button reference
											saveReplayButton = newSaveReplayButton;
							replayBufferButton = button;
							mainLayout->addWidget(saveReplayButton);
										}
					}
				}
			}
		}
	}
	
	// Add stretch to push StreamUP settings buttons to the right
	bool hasStreamUPSettings = false;
	for (const auto& item : flattenedItems) {
		if (!item->visible) continue;
		if (item->type == StreamUP::ToolbarConfig::ItemType::Button) {
			if (auto buttonItem = std::dynamic_pointer_cast<StreamUP::ToolbarConfig::ButtonItem>(item)) {
				if (buttonItem->buttonType == "streamup_settings") {
					hasStreamUPSettings = true;
					break;
				}
			}
		}
	}
	if (hasStreamUPSettings) {
		mainLayout->addStretch();
	}
	
	// Second pass: Add StreamUP settings buttons (they go on the right)
	for (const auto& item : flattenedItems) {
		if (!item->visible) continue;
		
		// Only process StreamUP settings buttons in second pass
		bool isStreamUPSettings = false;
		if (item->type == StreamUP::ToolbarConfig::ItemType::Button) {
			if (auto buttonItem = std::dynamic_pointer_cast<StreamUP::ToolbarConfig::ButtonItem>(item)) {
				isStreamUPSettings = (buttonItem->buttonType == "streamup_settings");
			}
		}
		if (!isStreamUPSettings) continue;
		
		QToolButton* button = createButtonFromConfig(item);
		if (button) {
			button->setObjectName(item->id);
			dynamicButtons[item->id] = button;
			mainLayout->addWidget(button);
		}
	}
	
	// Apply CSS styling
	updateToolbarStyling();
	
	// Add the central widget to toolbar
	addWidget(centralWidget);
	
	// Update layout orientation based on current position
	updateLayoutOrientation();
	
	// Clear flag and update buttons now that reconstruction is complete
	isReconstructingUI = false;
	updateAllButtons();
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
		
		// Set up built-in button - ensure we have a valid icon path
		QString iconPath = buttonItem->iconPath;
		if (iconPath.isEmpty()) {
			iconPath = StreamUP::ToolbarConfig::ButtonRegistry::getButtonInfo(buttonItem->buttonType).defaultIcon;
		}
		// If still empty, use a default icon to prevent empty path errors
		if (iconPath.isEmpty()) {
			iconPath = "settings"; // Use settings icon as fallback
		}
		button->setIcon(getCachedIcon(iconPath));
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
			button->setIcon(getCachedIcon(dockItem->iconPath));
		} else {
			// Use a default dock icon
			button->setIcon(getCachedIcon("settings"));
		}
		button->setToolTip(dockItem->tooltip);
		button->setCheckable(false);
		
		// Store dock action type in button's property
		button->setProperty("dockActionType", dockItem->dockButtonType);
		connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onDockButtonClicked);
	} else if (item->type == StreamUP::ToolbarConfig::ItemType::HotkeyButton) {
		auto hotkeyItem = std::static_pointer_cast<StreamUP::ToolbarConfig::HotkeyButtonItem>(item);
		
		// Set up hotkey button
		if (hotkeyItem->useCustomIcon && !hotkeyItem->customIconPath.isEmpty()) {
			// Use custom icon
			button->setIcon(QIcon(hotkeyItem->customIconPath));
		} else if (!hotkeyItem->iconPath.isEmpty()) {
			// Check if iconPath is a full file path (from OBS icons) or just a name (from StreamUP icons)
			if (QFile::exists(hotkeyItem->iconPath)) {
				// Full file path - use directly
				button->setIcon(QIcon(hotkeyItem->iconPath));
			} else {
				// Icon name - use themed icon system
				button->setIcon(getCachedIcon(hotkeyItem->iconPath));
			}
		} else {
			// Use default icon for this hotkey
			QString defaultIcon = StreamUP::OBSHotkeyManager::getDefaultHotkeyIcon(hotkeyItem->hotkeyName);
			button->setIcon(getCachedIcon(defaultIcon));
		}
		
		button->setToolTip(hotkeyItem->tooltip.isEmpty() ? hotkeyItem->displayName : hotkeyItem->tooltip);
		button->setCheckable(false); // Hotkey buttons are typically not checkable
		
		// Store hotkey name in button's property
		button->setProperty("hotkeyName", hotkeyItem->hotkeyName);
		connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onHotkeyButtonClicked);
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
	
	// Clear caches since UI is being reconstructed
	clearIconCache();
	clearStyleSheetCache();
	
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

void StreamUPToolbar::onHotkeyButtonClicked()
{
	QToolButton* button = qobject_cast<QToolButton*>(sender());
	if (!button) return;
	
	QString hotkeyName = button->property("hotkeyName").toString();
	if (hotkeyName.isEmpty()) {
		qWarning() << "[StreamUP] Hotkey button has no associated hotkey name";
		return;
	}
	
	// Trigger the OBS hotkey
	bool success = StreamUP::OBSHotkeyManager::triggerHotkey(hotkeyName);
	if (!success) {
		qWarning() << "[StreamUP] Failed to trigger hotkey:" << hotkeyName;
	}
}


void StreamUPToolbar::executeDockAction(const QString& actionType)
{
	// Find the StreamUP dock to call its functions
	QWidget* mainWindow = static_cast<QWidget*>(obs_frontend_get_main_window());
	if (!mainWindow) return;
	
	StreamUPDock* dock = mainWindow->findChild<StreamUPDock*>();
	if (!dock) {
		QMessageBox::warning(this, QString::fromUtf8(obs_module_text("Dock.Title")),
			QString::fromUtf8(obs_module_text("StreamUP.Toolbar.DockNotAvailable")));
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
		QMessageBox::warning(this, QString::fromUtf8(obs_module_text("StreamUP.Toolbar.UnknownAction")),
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
		QMessageBox::warning(this, QString::fromUtf8(obs_module_text("Dock.Title")),
			QString::fromUtf8(obs_module_text("StreamUP.Toolbar.DockNotAvailable")));
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
		
		// Ensure popup icons are themed correctly for current theme
		videoCapturePopup->updateIconsForTheme();
		
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
