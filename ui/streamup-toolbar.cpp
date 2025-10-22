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
	iconUpdateTimer(nullptr), m_updateBatchTimer(nullptr), streamButton(nullptr),
	recordButton(nullptr), pauseButton(nullptr), replayBufferButton(nullptr),
	saveReplayButton(nullptr), virtualCameraButton(nullptr), virtualCameraConfigButton(nullptr),
	studioModeButton(nullptr), settingsButton(nullptr), streamUPSettingsButton(nullptr),
	centralWidget(nullptr), mainLayout(nullptr), contextMenu(nullptr), configureAction(nullptr)
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
	contextMenu->setObjectName("StreamUPToolbarContextMenu");
	configureAction = contextMenu->addAction(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.Title")));
	configureAction->setObjectName("StreamUPToolbarConfigureAction");
	connect(configureAction, &QAction::triggered, this, &StreamUPToolbar::onConfigureToolbarClicked);

	toolbarSettingsAction = contextMenu->addAction(QString::fromUtf8(obs_module_text("StreamUP.Settings.ToolbarSettings")));
	toolbarSettingsAction->setObjectName("StreamUPToolbarSettingsAction");
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

	// Apply toolbar styling (including active button backgrounds if enabled)
	updateToolbarStyling();

	// Initialize current theme state
	currentThemeIsDark = StreamUP::UIHelpers::IsOBSThemeDark();


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
	separator->setProperty("class", "toolbar-separator");
	separator->setFrameShape(QFrame::NoFrame);
	separator->setFixedWidth(1);

	// Set height based on current icon size
	StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
	int separatorHeight = settings.toolbarIconSize;
	separator->setFixedHeight(separatorHeight);

	return separator;
}

QFrame* StreamUPToolbar::createHorizontalSeparator()
{
	QFrame* separator = new QFrame();
	separator->setProperty("class", "toolbar-separator");
	separator->setFrameShape(QFrame::NoFrame);
	separator->setFixedHeight(1);

	// Set width based on current icon size
	StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
	int separatorWidth = settings.toolbarIconSize;
	separator->setFixedWidth(separatorWidth);

	return separator;
} 

// Old setupUI function removed - now using setupDynamicUI() for configuration-based toolbar

void StreamUPToolbar::updateToolbarStyling()
{
	try {
		// Get current settings to check if button backgrounds should be shown
		StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();

		if (settings.toolbarButtonBackgrounds) {
			// Apply OBS theme's complete button styling to checked toolbar buttons
			// This uses all the standard button properties from the active theme
			QString activeButtonStyle = QString(
				"QToolButton[buttonType='streamup-button']:checked {"
				"    background: palette(button);"
				"    border: 1px solid palette(mid);"
				"    padding: 2px;"
				"}"
				"QToolButton[buttonType='streamup-button']:checked:hover {"
				"    background: palette(light);"
				"    border: 1px solid palette(mid);"
				"}"
				"QToolButton[buttonType='streamup-button']:checked:pressed {"
				"    background: palette(dark);"
				"    border: 1px solid palette(mid);"
				"}"
			);
			setStyleSheet(activeButtonStyle);
		} else {
			// Let OBS theme handle all toolbar styling - no custom stylesheets
			// Buttons and separators will inherit styling from the active OBS theme
			setStyleSheet("");
		}
	} catch (...) {
		// If settings aren't initialized yet, just use empty stylesheet
		StreamUP::DebugLogger::LogWarning("Toolbar", "Failed to apply toolbar styling - settings may not be initialized");
		setStyleSheet("");
	}
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
	StreamUP::DebugLogger::LogDebug("Toolbar", "Record Button Clicked", "Button click handler triggered");

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
		streamButton->setToolTip(streaming ? "Stop Streaming" : "Start Streaming");

		// Debug: Log the checked state
		StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Stream Button",
			"Streaming: %s, Checked: %s, Checkable: %s",
			streaming ? "true" : "false",
			streamButton->isChecked() ? "true" : "false",
			streamButton->isCheckable() ? "true" : "false");
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

		// Debug: Log the checked state
		StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Record Button",
			"Recording: %s, Checked: %s, Checkable: %s",
			recording ? "true" : "false",
			recordButton->isChecked() ? "true" : "false",
			recordButton->isCheckable() ? "true" : "false");

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
		studioModeButton->setToolTip(active ? "Disable Studio Mode" : "Enable Studio Mode");
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
		} else if (actionType == "group_selected_sources") {
			// Update icon for group selected sources button
			button->setIcon(getCachedIcon("add-sources-to-group"));
		} else if (actionType == "toggle_visibility_selected_sources") {
			// Update icon for toggle visibility button using custom StreamUP icon
			button->setIcon(getCachedIcon("visible"));
		} else if (actionType == "refresh_browser") {
			// Update icon for refresh browser sources button
			button->setIcon(getCachedIcon("refresh-browser-sources"));
		} else if (actionType == "refresh_audio") {
			// Update icon for refresh audio monitoring button
			button->setIcon(getCachedIcon("refresh-audio-monitoring"));
		} else if (actionType == "video_capture") {
			// Update icon for video capture button
			button->setIcon(getCachedIcon("camera"));
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
		// Settings may have changed, force immediate update with button visibility and theme icons
		toolbar->updateButtonVisibility();
		toolbar->updateIconsForTheme();
		toolbar->scheduleUpdate();
		break;

	case OBS_FRONTEND_EVENT_THEME_CHANGED:
		// Theme changed, update icons for new theme
		toolbar->updateIconsForTheme();
		// Also refresh toolbar styling for the new theme
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

QIcon StreamUPToolbar::getCachedIcon(const QString& iconName)
{
	// Always use the reliable UI helpers approach instead of caching
	// This ensures consistent theming behavior between test and GitHub builds
	QString iconPath = StreamUP::UIHelpers::GetThemedIconPath(iconName);

	// Debug logging to see what's happening
	StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Icon Debug", "Icon '%s' resolved to path: %s",
		iconName.toUtf8().constData(), iconPath.toUtf8().constData());

	return QIcon(iconPath);
}

void StreamUPToolbar::clearIconCache()
{
	iconCache.clear();
}

void StreamUPToolbar::clearStyleSheetCache()
{
	// No stylesheet cache anymore - OBS theme handles all styling
}

void StreamUPToolbar::preloadCommonIcons()
{
	// Icons are now loaded on-demand using reliable UI helpers approach
	// No preloading needed since we're not caching for theme reliability
	StreamUP::DebugLogger::LogDebug("Toolbar", "Icon Preload", "Using on-demand icon loading for reliable theming");
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
		virtualCameraButton->setIcon(getCachedIcon("virtual-camera"));
		virtualCameraButton->setToolTip(vcamActive ? "Stop Virtual Camera" : "Start Virtual Camera");
	}

	if (studioModeButton) {
		studioModeButton->setChecked(studioMode);
		studioModeButton->setIcon(getCachedIcon("studio-mode"));
		studioModeButton->setToolTip(studioMode ? "Disable Studio Mode" : "Enable Studio Mode");
	}

	StreamUP::DebugLogger::LogDebug("Toolbar", "Batch Update", "Completed efficient button state update");
}


void StreamUPToolbar::updateIconsForTheme()
{
	StreamUP::DebugLogger::LogDebug("Toolbar", "Theme Update", "Updating icons for theme change");

	// Update buttons with fresh themed icons - all StreamUP toolbar buttons use custom icons
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

	// Update dock button icons for theme change
	updateDockButtonIcons();
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

	// Update layout orientation before applying theme
	updateLayoutOrientation();

	// Apply toolbar styling (including active button backgrounds if enabled)
	updateToolbarStyling();

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
		} else {
			mainLayout = new QHBoxLayout(centralWidget);
			setOrientation(Qt::Horizontal);
			mainLayout->setContentsMargins(8, 4, 8, 4);  // Added vertical padding (top, bottom)
			// No alignment needed for horizontal layout (default is fine)
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
				widget->deleteLater(); // Delete the old separator

				// Add new separator with correct orientation
				QFrame* separator;
				if (shouldBeVertical) {
					separator = createHorizontalSeparator();
					mainLayout->addWidget(separator, 0, Qt::AlignHCenter);
				} else {
					separator = createSeparator();
					mainLayout->addWidget(separator);
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
				// Clear old fixed dimensions
				widget->setMinimumSize(0, 0);
				widget->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

				if (shouldBeVertical) {
					// Now vertical, spacer size becomes the height
					widget->setFixedHeight(spacerSize);
				} else {
					// Now horizontal, spacer size becomes the width
					widget->setFixedWidth(spacerSize);
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
	centralWidget->setObjectName("StreamUPToolbarCentralWidget");
	mainLayout = new QHBoxLayout(centralWidget);
	mainLayout->setContentsMargins(8, 4, 8, 4);  // Added vertical padding (top, bottom)
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
			mainLayout->addWidget(separator);
		} else if (item->type == StreamUP::ToolbarConfig::ItemType::CustomSpacer) {
			auto spacerItem = std::static_pointer_cast<StreamUP::ToolbarConfig::CustomSpacerItem>(item);
			// Create a fixed-size spacer widget with orientation-aware dimensions
			QWidget* spacerWidget = new QWidget(centralWidget);
			spacerWidget->setProperty("class", "toolbar-spacer");

			// Set dimensions based on current toolbar orientation
			QMainWindow* mainWindow = qobject_cast<QMainWindow*>(parent());
			if (mainWindow) {
				Qt::ToolBarArea currentArea = mainWindow->toolBarArea(this);
				bool isVertical = (currentArea == Qt::LeftToolBarArea || currentArea == Qt::RightToolBarArea);

				if (isVertical) {
					// For vertical toolbar, spacer height should be the configured size
					spacerWidget->setFixedHeight(spacerItem->size);
				} else {
					// For horizontal toolbar, spacer width should be the configured size
					spacerWidget->setFixedWidth(spacerItem->size);
				}
			} else {
				// Fallback to horizontal orientation if can't determine position
				spacerWidget->setFixedWidth(spacerItem->size);
			}

			spacerWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
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
							// Get current settings for dynamic sizing
							StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
							int iconSize = settings.toolbarIconSize;

							// Create new pause button for this position
							QToolButton* newPauseButton = new QToolButton(centralWidget);
							newPauseButton->setObjectName("pauseButton");
							newPauseButton->setProperty("class", "streamup-toolbar-button");
							newPauseButton->setProperty("buttonType", "streamup-button");
							newPauseButton->setIconSize(QSize(iconSize, iconSize));
							newPauseButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
							newPauseButton->setIcon(getCachedIcon("pause"));
							newPauseButton->setToolTip("Pause Recording");
							newPauseButton->setCheckable(true);
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
							// Get current settings for dynamic sizing
							StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
							int iconSize = settings.toolbarIconSize;

							// Create new save_replay button for this position
							QToolButton* newSaveReplayButton = new QToolButton(centralWidget);
							newSaveReplayButton->setObjectName("saveReplayButton");
							newSaveReplayButton->setProperty("class", "streamup-toolbar-button");
							newSaveReplayButton->setProperty("buttonType", "streamup-button");
							newSaveReplayButton->setIconSize(QSize(iconSize, iconSize));
							newSaveReplayButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
							newSaveReplayButton->setIcon(getCachedIcon("save-replay"));
							newSaveReplayButton->setToolTip("Save Replay");
							newSaveReplayButton->setCheckable(false);
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

	// Get current settings for dynamic sizing
	StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
	int iconSize = settings.toolbarIconSize;

	// Set dynamic icon size based on settings - let CSS handle button size
	button->setIconSize(QSize(iconSize, iconSize));
	button->setToolButtonStyle(Qt::ToolButtonIconOnly);

	// Add consistent styling properties for custom StreamUP icons
	button->setProperty("class", "streamup-toolbar-button");
	button->setProperty("buttonType", "streamup-button");

	// Let OBS theme handle button sizing with proper padding and border-radius
	// No fixed size - CSS will scale padding and radius with icon size

	if (item->type == StreamUP::ToolbarConfig::ItemType::Button) {
		auto buttonItem = std::static_pointer_cast<StreamUP::ToolbarConfig::ButtonItem>(item);
		
		// Set up built-in button using appropriate theming approach
		button->setToolTip(buttonItem->tooltip.isEmpty() ?
			StreamUP::ToolbarConfig::ButtonRegistry::getButtonInfo(buttonItem->buttonType).defaultTooltip :
			buttonItem->tooltip);
		button->setCheckable(buttonItem->checkable);

		// All StreamUP toolbar icons are custom - use consistent theming approach
		if (buttonItem->buttonType == "streamup_settings") {
			// Use the special StreamUP logo icon (social icon, not themed)
			button->setIcon(QIcon(":images/icons/social/streamup-logo-button.svg"));
		} else {
			// For all other buttons, use custom StreamUP icons via cached icon system
			QString iconPath = buttonItem->iconPath;
			if (iconPath.isEmpty()) {
				iconPath = StreamUP::ToolbarConfig::ButtonRegistry::getButtonInfo(buttonItem->buttonType).defaultIcon;
			}
			if (iconPath.isEmpty()) {
				iconPath = "settings"; // Use settings icon as fallback
			}
			button->setIcon(getCachedIcon(iconPath));
		}
		
		// Connect to appropriate slot based on button type
		// Force stateful buttons to be checkable so :checked CSS works for backgrounds
		if (buttonItem->buttonType == "stream") {
			streamButton = button;
			button->setCheckable(true);
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onStreamButtonClicked);
		} else if (buttonItem->buttonType == "record") {
			recordButton = button;
			button->setCheckable(true);
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onRecordButtonClicked);
		} else if (buttonItem->buttonType == "pause") {
			pauseButton = button;
			button->setCheckable(true);
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onPauseButtonClicked);
		} else if (buttonItem->buttonType == "replay_buffer") {
			replayBufferButton = button;
			button->setCheckable(true);
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onReplayBufferButtonClicked);
		} else if (buttonItem->buttonType == "save_replay") {
			saveReplayButton = button;
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onSaveReplayButtonClicked);
		} else if (buttonItem->buttonType == "virtual_camera") {
			virtualCameraButton = button;
			button->setCheckable(true);
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onVirtualCameraButtonClicked);
		} else if (buttonItem->buttonType == "virtual_camera_config") {
			virtualCameraConfigButton = button;
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onVirtualCameraConfigButtonClicked);
		} else if (buttonItem->buttonType == "studio_mode") {
			studioModeButton = button;
			button->setCheckable(true);
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onStudioModeButtonClicked);
		} else if (buttonItem->buttonType == "settings") {
			settingsButton = button;
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onSettingsButtonClicked);
		} else if (buttonItem->buttonType == "streamup_settings") {
			streamUPSettingsButton = button;
			connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onStreamUPSettingsButtonClicked);
		}
		
	} else if (item->type == StreamUP::ToolbarConfig::ItemType::DockButton) {
		auto dockItem = std::static_pointer_cast<StreamUP::ToolbarConfig::DockButtonItem>(item);
		
		// Set up dock button
		if (dockItem->dockButtonType == "toggle_visibility_selected_sources") {
			// Use custom StreamUP visibility icon
			button->setIcon(getCachedIcon("visible"));
		} else if (!dockItem->iconPath.isEmpty()) {
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
	separator->setProperty("class", "toolbar-separator");
	separator->setFrameShape(QFrame::NoFrame);

	// Set size based on current icon size
	StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
	int separatorSize = settings.toolbarIconSize;

	if (isVertical) {
		separator->setFixedHeight(1);
		separator->setFixedWidth(separatorSize);
	} else {
		separator->setFixedWidth(1);
		separator->setFixedHeight(separatorSize);
	}
	return separator;
}

void StreamUPToolbar::updateButtonSizes()
{
	// Get current settings for dynamic sizing
	StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
	int iconSize = settings.toolbarIconSize;

	StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Button Sizes", "Updating button sizes with icon size: %d px", iconSize);

	// Update all toolbar buttons
	QList<QToolButton*> allButtons = centralWidget->findChildren<QToolButton*>();
	for (QToolButton* button : allButtons) {
		// Set icon size
		button->setIconSize(QSize(iconSize, iconSize));

		// Clear any size constraints that might limit growth
		button->setMinimumSize(0, 0);
		button->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

		// Force button to recalculate its size hint
		button->updateGeometry();
	}

	// Update all separators to match new icon size
	QList<QFrame*> allSeparators = centralWidget->findChildren<QFrame*>();
	for (QFrame* separator : allSeparators) {
		// Only update separators, not other frames
		if (separator->property("class").toString() == "toolbar-separator") {
			// Check if vertical or horizontal based on current toolbar orientation
			QMainWindow* mainWindow = qobject_cast<QMainWindow*>(parent());
			if (mainWindow) {
				Qt::ToolBarArea currentArea = mainWindow->toolBarArea(this);
				bool isVertical = (currentArea == Qt::LeftToolBarArea || currentArea == Qt::RightToolBarArea);

				if (isVertical) {
					// Horizontal separator in vertical toolbar
					separator->setFixedWidth(iconSize);
				} else {
					// Vertical separator in horizontal toolbar
					separator->setFixedHeight(iconSize);
				}
			}
		}
	}

	// Force style refresh to apply new icon sizes with theme-scaled padding and borders
	if (mainLayout) {
		mainLayout->update();
	}
	centralWidget->updateGeometry();
	updateGeometry();

	// Refresh styling to apply theme-based sizing
	style()->unpolish(this);
	style()->polish(this);

	// Force repaint
	update();
}

void StreamUPToolbar::refreshFromConfiguration()
{
	// Clear the current toolbar state
	clear();
	dynamicButtons.clear();

	// Clear icon cache since UI is being reconstructed
	clearIconCache();

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
	} else if (actionType == "group_selected_sources") {
		dock->ButtonGroupSelectedSources();
	} else if (actionType == "toggle_visibility_selected_sources") {
		dock->ButtonToggleVisibilitySelectedSources();
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
