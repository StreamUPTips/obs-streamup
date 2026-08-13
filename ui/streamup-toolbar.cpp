#include "streamup-toolbar.hpp"
#include <streamup/debug-logger.hpp>
#include "streamup-toolbar-edit-panel.hpp"
#include "dock/streamup-dock.hpp"
#include "../video-capture-popup.hpp"
#include "ui-helpers.hpp"
#include "settings-manager.hpp"
#include "streamup-toolbar-builder.hpp"
#include "streamup-toolbar-response-popover.hpp"
#include "streamup-toolbar-status.hpp"
#include "theme-enhancements.hpp"
#include "obs-hotkey-manager.hpp"
#include "../obs-websocket-api.h"
#include <QGuiApplication>
#include <QScreen>
#include <QStyle>
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
#include <streamup/ui/dialogs.hpp>
#include <QFile>
#include <QTimer>
#include <util/config-file.h>

namespace su = StreamUP::UIStyles;

StreamUPToolbar::StreamUPToolbar(QWidget *parent) : QToolBar(parent),
	iconUpdateTimer(nullptr), m_updateBatchTimer(nullptr), streamButton(nullptr),
	recordButton(nullptr), pauseButton(nullptr), replayBufferButton(nullptr),
	saveReplayButton(nullptr), virtualCameraButton(nullptr), virtualCameraConfigButton(nullptr),
	studioModeButton(nullptr), settingsButton(nullptr), streamUPSettingsButton(nullptr),
	centralWidget(nullptr), mainLayout(nullptr), contextMenu(nullptr)
{
	setObjectName("StreamUPToolbar");
	setWindowTitle(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Title")));

	// Once the main window has handed the toolbar its dock area, tell the theme
	// which edge to drop its inset on.
	QTimer::singleShot(0, this, [this]() { reportDockedEdge(); });

	// Initialize optimized update system
	m_updateBatchTimer = new QTimer(this);
	m_updateBatchTimer->setSingleShot(true);
	m_updateBatchTimer->setInterval(50); // 50ms batching delay
	connect(m_updateBatchTimer, &QTimer::timeout, this, &StreamUPToolbar::processBatchedUpdates);

	// Setup context menu
	contextMenu = new QMenu(this);
	contextMenu->setObjectName("StreamUPToolbarContextMenu");
	// Configuring the toolbar and editing it are the same act now, so there is
	// one entry rather than two that did nearly the same thing.
	editToolbarAction = contextMenu->addAction(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.EditMode")));
	editToolbarAction->setObjectName("StreamUPToolbarEditAction");
	editToolbarAction->setCheckable(true);
	connect(editToolbarAction, &QAction::triggered, this, &StreamUPToolbar::onEditToolbarClicked);

	toolbarSettingsAction = contextMenu->addAction(QString::fromUtf8(obs_module_text("StreamUP.Settings.ToolbarSettings")));
	toolbarSettingsAction->setObjectName("StreamUPToolbarSettingsAction");
	connect(toolbarSettingsAction, &QAction::triggered, this, &StreamUPToolbar::onToolbarSettingsClicked);

	// Qt is the authority on which way the run flows once we are docked, and it
	// knows before toolBarArea() will answer. Without this the only thing that
	// re-laid the toolbar out was a manual save from the configurator.
	connect(this, &QToolBar::orientationChanged, this, [this](Qt::Orientation orientation) {
		onOrientationChanged(orientation == Qt::Vertical);
	});

	// Load configuration and setup UI
	toolbarConfig.loadFromSettings();
	setupDynamicUI();

	// An old configuration was carried across to the new format. Say so once,
	// after OBS has finished putting its window together, since a dialog
	// raised from a constructor has nothing to sit on top of yet.
	if (toolbarConfig.consumeMigrationNotice()) {
		QTimer::singleShot(2000, this, []() {
			QWidget *mainWindow = static_cast<QWidget *>(obs_frontend_get_main_window());
			StreamUP::UIStyles::info(mainWindow,
						 QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Upgrade.Title")),
						 QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Upgrade.Message")));
		});
	}

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

bool StreamUPToolbar::isVerticalOrientation() const
{
	// Once the main window owns us, its answer is authoritative.
	if (QMainWindow* mainWindow = qobject_cast<QMainWindow*>(parent())) {
		Qt::ToolBarArea area = mainWindow->toolBarArea(this);
		if (area == Qt::LeftToolBarArea || area == Qt::RightToolBarArea)
			return true;
		if (area == Qt::TopToolBarArea || area == Qt::BottomToolBarArea)
			return false;
		// NoToolBarArea: added but not yet placed. Fall through to settings.
	}

	// Constructed but not yet added, which is how the toolbar is always built:
	// setupDynamicUI() runs before LoadStreamUPToolbar() calls addToolBar().
	// Assuming horizontal here is what left side-docked spacers with a fixed
	// width and a zero-height hint, so they rendered as nothing.
	StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
	return settings.toolbarPosition == StreamUP::SettingsManager::ToolbarPosition::Left ||
	       settings.toolbarPosition == StreamUP::SettingsManager::ToolbarPosition::Right;
}

void StreamUPToolbar::updateToolbarStyling()
{
	// Toolbar is OBS-theme-compliant: it ships no inline stylesheet and lets the
	// active OBS theme paint everything (idle, hover, checked, pressed).
	//
	// Theme hooks exposed for OBS theme files to target:
	//   QToolBar#StreamUPToolbar             - floating
	//   QToolBar#StreamUPToolbar-{Top,Bottom,Left,Right}
	//   QToolBar[toolbarPosition="{top|bottom|left|right|floating}"]
	//   QToolButton[buttonType="streamup-button"]            - every dynamic button
	//   QToolButton#{streamButton,recordButton,...}{-Top|...} - per-action selectors
	//
	// Calling clear here so any previously set fallback (older builds, or
	// re-application via theme reload) is removed.
	setStyleSheet("");
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

bool StreamUPToolbar::invokeMainWindowAction(const char *slotName)
{
	// Route start/stop through OBS's own action slots rather than calling
	// obs_frontend_*_start/stop directly. Those slots own all of the pre-flight
	// UI: the "Are you sure you want to start/stop streaming?" confirmations
	// (BasicWindow/WarnBeforeStartingStream, WarnBeforeStoppingStream,
	// WarnBeforeStoppingRecord), the no-sources check, stream settings
	// validation, the bandwidth-test prompt and the YouTube broadcast flow.
	// The frontend API sits below all of that, which is why the toolbar used
	// to bypass the confirmation dialogs entirely.
	QMainWindow *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow) {
		return false;
	}

	// The slots are private, but the meta-object system does not enforce
	// access control, so invoking them by name works. Direct connection keeps
	// the modal dialog synchronous, exactly as OBS's own controls dock does.
	// If a future OBS release renames them, invokeMethod returns false and we
	// fall back to the plain frontend call.
	return QMetaObject::invokeMethod(mainWindow, slotName, Qt::DirectConnection);
}

void StreamUPToolbar::onStreamButtonClicked()
{
	if (!invokeMainWindowAction("StreamActionTriggered")) {
		StreamUP::DebugLogger::LogDebug("Toolbar", "Stream Button Clicked",
						"StreamActionTriggered unavailable, using frontend API fallback");
		if (obs_frontend_streaming_active()) {
			obs_frontend_streaming_stop();
		} else {
			obs_frontend_streaming_start();
		}
	}
	updateStreamButton();
}

void StreamUPToolbar::onRecordButtonClicked()
{
	StreamUP::DebugLogger::LogDebug("Toolbar", "Record Button Clicked", "Button click handler triggered");

	if (!invokeMainWindowAction("RecordActionTriggered")) {
		StreamUP::DebugLogger::LogDebug("Toolbar", "Record Button Clicked",
						"RecordActionTriggered unavailable, using frontend API fallback");
		if (obs_frontend_recording_active()) {
			obs_frontend_recording_stop();
		} else {
			obs_frontend_recording_start();
		}
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
		streamButton->setToolTip(streaming ? obs_module_text("Toolbar.Tooltip.StopStreaming")
					     : obs_module_text("Toolbar.Tooltip.StartStreaming"));

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
		recordButton->setToolTip(recording ? obs_module_text("Toolbar.Tooltip.StopRecording")
					     : obs_module_text("Toolbar.Tooltip.StartRecording"));

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
		pauseButton->setToolTip(paused ? obs_module_text("Toolbar.Tooltip.ResumeRecording")
					  : obs_module_text("Toolbar.Tooltip.PauseRecording"));
	}
}

void StreamUPToolbar::updateReplayBufferButton()
{
	if (replayBufferButton) {
		bool active = obs_frontend_replay_buffer_active();
		replayBufferButton->setChecked(active);
		QString iconName = active ? "replay-buffer-on" : "replay-buffer-off";
		replayBufferButton->setIcon(getCachedIcon(iconName));
		replayBufferButton->setToolTip(active ? obs_module_text("Toolbar.Tooltip.StopReplayBuffer")
					  : obs_module_text("Toolbar.Tooltip.StartReplayBuffer"));

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
		virtualCameraButton->setToolTip(active ? obs_module_text("Toolbar.Tooltip.StopVirtualCamera")
					  : obs_module_text("Toolbar.Tooltip.StartVirtualCamera"));
	}
}

void StreamUPToolbar::updateStudioModeButton()
{
	if (studioModeButton) {
		bool active = obs_frontend_preview_program_mode_active();
		studioModeButton->setChecked(active);
		studioModeButton->setToolTip(active ? obs_module_text("Toolbar.Tooltip.DisableStudioMode")
					  : obs_module_text("Toolbar.Tooltip.EnableStudioMode"));
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
	// The live buttons do not exist while the editor is on the bar.
	if (editModeActive)
		return;

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
	if (editModeActive)
		return;

	if (!m_updatesPending) {
		m_updatesPending = true;
		m_updateBatchTimer->start();
	}
}

void StreamUPToolbar::processBatchedUpdates()
{
	if (editModeActive)
		return;

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
		streamButton->setToolTip(streaming ? obs_module_text("Toolbar.Tooltip.StopStreaming")
					     : obs_module_text("Toolbar.Tooltip.StartStreaming"));
	}

	if (recordButton) {
		recordButton->setChecked(recording);
		QString iconName = recording ? "record-on" : "record-off";
		recordButton->setIcon(getCachedIcon(iconName));
		recordButton->setToolTip(recording ? obs_module_text("Toolbar.Tooltip.StopRecording")
					     : obs_module_text("Toolbar.Tooltip.StartRecording"));
	}

	if (pauseButton) {
		bool canPause = recording && isRecordingPausable();
		pauseButton->setVisible(canPause);
		pauseButton->setEnabled(canPause);
		pauseButton->setChecked(paused);
		pauseButton->setIcon(getCachedIcon("pause"));
		pauseButton->setToolTip(paused ? obs_module_text("Toolbar.Tooltip.ResumeRecording")
					  : obs_module_text("Toolbar.Tooltip.PauseRecording"));
	}

	if (replayBufferButton) {
		replayBufferButton->setChecked(replayActive);
		QString iconName = replayActive ? "replay-buffer-on" : "replay-buffer-off";
		replayBufferButton->setIcon(getCachedIcon(iconName));
		replayBufferButton->setToolTip(replayActive ? obs_module_text("Toolbar.Tooltip.StopReplayBuffer")
						: obs_module_text("Toolbar.Tooltip.StartReplayBuffer"));
	}

	if (saveReplayButton) {
		saveReplayButton->setVisible(replayActive);
		saveReplayButton->setEnabled(replayActive && !paused);
		saveReplayButton->setIcon(getCachedIcon("save-replay"));
	}

	if (virtualCameraButton) {
		virtualCameraButton->setChecked(vcamActive);
		virtualCameraButton->setIcon(getCachedIcon("virtual-camera"));
		virtualCameraButton->setToolTip(vcamActive ? obs_module_text("Toolbar.Tooltip.StopVirtualCamera")
					       : obs_module_text("Toolbar.Tooltip.StartVirtualCamera"));
	}

	if (studioModeButton) {
		studioModeButton->setChecked(studioMode);
		studioModeButton->setIcon(getCachedIcon("studio-mode"));
		studioModeButton->setToolTip(studioMode ? obs_module_text("Toolbar.Tooltip.DisableStudioMode")
					       : obs_module_text("Toolbar.Tooltip.EnableStudioMode"));
	}

	StreamUP::DebugLogger::LogDebug("Toolbar", "Batch Update", "Completed efficient button state update");
}


void StreamUPToolbar::updateIconsForTheme()
{
	if (editModeActive)
		return;

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

	// This is the one place that always runs when the position changes, whether
	// that came from the settings, a theme refresh or an orientation flip, so
	// the window inset is told from here. Hanging it off orientationChanged
	// alone missed left to right and top to bottom, where the position moves
	// but the orientation does not, and the inset stayed on the old edge.
	StreamUP::ThemeEnhancements::SetToolbarDockedEdge(currentArea);

	// Update layout orientation before applying theme
	updateLayoutOrientation();

	// Apply toolbar styling (including active button backgrounds if enabled)
	updateToolbarStyling();

	// Force style sheet refresh to apply position-based styling
	style()->unpolish(this);
	style()->polish(this);

	// Now that the position-aware objectName is in place, apply the size tier
	// so the theme's `QToolBar#StreamUPToolbar-{Top|...}[size="..."]` selectors
	// can match.
	applySizeClass();
}

void StreamUPToolbar::onEditToolbarClicked()
{
	setEditMode(!editModeActive);
}

void StreamUPToolbar::setEditMode(bool enabled)
{
	if (editModeActive == enabled)
		return;

	editModeActive = enabled;
	if (editToolbarAction)
		editToolbarAction->setChecked(enabled);

	if (!enabled) {
		if (editPanel) {
			editPanel->close();
			editPanel->deleteLater();
			editPanel = nullptr;
		}
		editor = nullptr; // owned by the toolbar's action, cleared by clear()

		// Whatever the dragging produced is the configuration now.
		toolbarConfig.saveToSettings();
		refreshFromConfiguration();
		return;
	}

	// Swap the live run for the editable one. Only one run exists at a time,
	// so there is no preview to fall out of step with the real thing.
	clear();
	centralWidget = nullptr;
	mainLayout = nullptr;
	dynamicButtons.clear();

	// clear() destroyed every button, so the cached pointers are dangling.
	// An OBS frontend event arriving mid-edit would walk freed memory.
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

	editor = new StreamUP::ToolbarEditor(this);
	editor->setAxis(StreamUP::ToolbarGeom::Axis(isVerticalOrientation()));
	editor->setAlignment(currentAlignment());
	editor->setConfiguration(&toolbarConfig);
	addWidget(editor);

	editPanel = new StreamUP::ToolbarEditPanel(editor, this);
	connect(editPanel, &StreamUP::ToolbarEditPanel::doneRequested, this, [this]() { setEditMode(false); });
	connect(editPanel, &StreamUP::ToolbarEditPanel::configurationChanged, this, [this]() {
		// Held in memory until edit mode ends, so a session of dragging is one
		// write rather than one per gesture.
	});
	connect(editPanel, &StreamUP::ToolbarEditPanel::resetRequested, this, [this]() {
		toolbarConfig.setDefaultConfiguration();
		if (editor)
			editor->rebuild();
	});
	connect(editPanel, &StreamUP::ToolbarEditPanel::itemCreated, this,
		[this](std::shared_ptr<StreamUP::ToolbarConfig::ToolbarItem> item) {
			if (!item)
				return;
			toolbarConfig.addItem(item);
			if (editor) {
				editor->rebuild();
				editor->setSelectedItemId(item->id);
			}
		});

	repositionEditPanel();

	editPanel->show();
	editPanel->raise();
}

// The main window carries a 9px inset so docks sit off the window edge. The
// toolbar wants the opposite on the edge it is docked to, so the theme is told
// where we are and drops the inset on that side only.
void StreamUPToolbar::reportDockedEdge()
{
	QMainWindow *mainWindow = qobject_cast<QMainWindow *>(parentWidget());
	if (!mainWindow) {
		StreamUP::ThemeEnhancements::SetToolbarDockedEdge(Qt::NoToolBarArea);
		return;
	}
	StreamUP::ThemeEnhancements::SetToolbarDockedEdge(isFloating() ? Qt::NoToolBarArea
								      : mainWindow->toolBarArea(this));
}

void StreamUPToolbar::onOrientationChanged(bool vertical)
{
	// While editing, the bar is showing the editor rather than a live run, so
	// the editor is what has to turn. Re-laying it out here rather than waiting
	// for edit mode to be toggled off and on again.
	if (editModeActive) {
		if (editor)
			editor->setAxis(StreamUP::ToolbarGeom::Axis(vertical));
		repositionEditPanel();
		return;
	}

	updateLayoutOrientation();
}

void StreamUPToolbar::repositionEditPanel()
{
	if (!editPanel)
		return;

	// Sit beside the bar rather than wherever the window manager would drop it,
	// and stay on screen when the toolbar is near an edge.
	editPanel->adjustSize();
	const QRect barRect(mapToGlobal(QPoint(0, 0)), size());
	QScreen *scr = QGuiApplication::screenAt(barRect.center());
	const QRect screen = scr ? scr->availableGeometry() : QGuiApplication::primaryScreen()->availableGeometry();
	const QSize panelSize = editPanel->sizeHint();
	const int gap = StreamUP::UIStyles::S(12);

	QPoint where = isVerticalOrientation() ? QPoint(barRect.right() + gap, barRect.top())
					       : QPoint(barRect.left(), barRect.bottom() + gap);
	where.setX(qBound(screen.left(), where.x(), screen.right() - panelSize.width()));
	where.setY(qBound(screen.top(), where.y(), screen.bottom() - panelSize.height()));
	editPanel->move(where);
}

void StreamUPToolbar::updateLayoutOrientation()
{
	if (!centralWidget || !mainLayout) {
		StreamUP::DebugLogger::LogWarning("Toolbar", "Layout: Cannot update layout orientation - missing central widget or layout");
		return;
	}

	// Where the toolbar is, or where the settings say it is headed while the
	// main window has not taken ownership yet. Bailing out in that second case
	// (as this used to) left a side-docked toolbar laid out horizontally.
	const bool shouldBeVertical = isVerticalOrientation();

	if (editModeActive)
		return;

	const bool currentlyVertical =
		(mainLayout->direction() == QBoxLayout::TopToBottom || mainLayout->direction() == QBoxLayout::BottomToTop);

	if (shouldBeVertical == currentlyVertical)
		return;

	// Rebuild from the configuration rather than shuffling the existing widgets
	// about. The old rebuild re-derived each spacer's size from its rendered
	// geometry, which reads as nothing on an axis the previous orientation left
	// free, so spacers collapsed every time the toolbar changed edge.
	//
	// setupDynamicUI() calls setOrientation(), which re-enters here through
	// QToolBar::orientationChanged. That pass finds the layout already matching
	// and returns at the check above.
	setupDynamicUI();
	updateAllButtons();
	updateIconsForTheme();
}

void StreamUPToolbar::updateToolbarSizeConstraints()
{
	// No custom size constraints — let OBS theme CSS handle all sizing
	setMinimumSize(0, 0);
	setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
}

StreamUP::SettingsManager::ToolbarAlignment StreamUPToolbar::currentAlignment() const
{
	return StreamUP::SettingsManager::GetCurrentSettings().toolbarAlignment;
}

void StreamUPToolbar::refreshAlignment()
{
	// Alignment is expressed purely as stretch items around the button run, so
	// a full rebuild is the cheapest correct way to re-apply it.
	refreshFromConfiguration();
}

void StreamUPToolbar::applySizeClass()
{
	// Read current size tier from settings.
	StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
	const char *sizeKey;
	switch (settings.toolbarSize) {
	case StreamUP::SettingsManager::ToolbarSize::Small:
		sizeKey = "small";
		break;
	case StreamUP::SettingsManager::ToolbarSize::Large:
		sizeKey = "large";
		break;
	case StreamUP::SettingsManager::ToolbarSize::Medium:
	default:
		sizeKey = "medium";
		break;
	}

	// Set the dynamic property the OBS theme uses for [size="..."] selectors.
	// Set on the toolbar AND every child QToolButton so themes can target either
	// the descendant selector form or the button-direct form.
	const QVariant value = QString::fromLatin1(sizeKey);
	setProperty("size", value);
	if (centralWidget) {
		centralWidget->setProperty("size", value);
	}
	const QList<QToolButton*> buttons = findChildren<QToolButton*>();
	for (QToolButton *b : buttons) {
		if (b) {
			b->setProperty("size", value);
		}
	}

	// Force the style engine to re-evaluate selectors with the new property
	// FIRST. QStyle::polish() on a QToolBar re-syncs iconSize from the style's
	// PM_ToolBarIconSize metric, so anything we set before polish gets wiped.
	if (QStyle *s = style()) {
		s->unpolish(this);
		s->polish(this);
	}
	for (QToolButton *b : buttons) {
		if (!b) continue;
		if (QStyle *s = b->style()) {
			s->unpolish(b);
			s->polish(b);
		}
	}

	// The polish above re-reads QToolBar's layout margin from the style, so the
	// pinned margin has to go back on after it.
	applyLayoutMargins();

	// THEN set iconSize at the Qt level so themes without per-size selectors
	// (Aitum, Yami, System) still differentiate the tiers. The pixel values
	// here match what StreamUP.obt declares per tier so behavior is consistent
	// across themes.
	//
	// Crucially, we set iconSize on every child QToolButton too. QToolBar's
	// iconSize property only affects the QToolButtons it creates internally
	// from QActions; the StreamUP toolbar adds buttons via addWidget() inside
	// a centralWidget, and those buttons do NOT inherit the toolbar's
	// iconSize. Without this loop, the toolbar's iconSize is purely cosmetic
	// for our layout.
	QSize fallbackIconSize(16, 16);
	switch (settings.toolbarSize) {
	case StreamUP::SettingsManager::ToolbarSize::Small:
		fallbackIconSize = QSize(12, 12);
		break;
	case StreamUP::SettingsManager::ToolbarSize::Large:
		fallbackIconSize = QSize(22, 22);
		break;
	default:
		break;
	}
	setIconSize(fallbackIconSize);
	for (QToolButton *b : buttons) {
		if (b) {
			b->setIconSize(fallbackIconSize);
		}
	}

	// Layout may need to re-measure after icon size change
	if (centralWidget) {
		centralWidget->updateGeometry();
		QLayout *layout = centralWidget->layout();
		if (layout) {
			layout->invalidate();
			layout->activate();
		}
	}
	updateGeometry();
	update();
}

// QToolBar's own layout carries a margin from the style, and the stylesheet
// cannot reach it: with the theme's padding at zero the bar still measured 8px
// clear of the buttons on every side. That is a fixed 16px of chrome across the
// bar, which swamps the size tiers. Small to large only moves the buttons by
// 10px, so the bar went 35 to 45 and read as barely responding.
//
// Pinned here instead, so the bar is the buttons plus a deliberate margin and
// the tiers actually show. Re-applied after every polish, because polishing a
// QToolBar re-reads the margin from the style and puts it back.
void StreamUPToolbar::applyLayoutMargins()
{
	static constexpr int kBarMarginPx = 3;
	if (QLayout *l = layout()) {
		const int m = StreamUP::UIStyles::S(kBarMarginPx);
		l->setContentsMargins(m, m, m, m);
	}
}

void StreamUPToolbar::refreshSizeClass()
{
	applySizeClass();
}

void StreamUPToolbar::setupDynamicUI()
{
	// Set flag to prevent updates during reconstruction
	isReconstructingUI = true;

	// Set basic toolbar properties like obs-toolbar
	setMovable(false);
	setFloatable(false);
	// Build straight into the orientation we are headed for. Starting
	// horizontal and flipping afterwards meant every side-docked launch went
	// through a rebuild before it had ever been laid out.
	const bool buildVertical = isVerticalOrientation();
	setOrientation(buildVertical ? Qt::Vertical : Qt::Horizontal);

	// Clear existing toolbar contents
	clear();
	
	// Delete old central widget if it exists to ensure complete cleanup
	if (centralWidget) {
		centralWidget->setParent(nullptr);
		centralWidget->deleteLater();
		centralWidget = nullptr;
		mainLayout = nullptr;
	}
	
	// Build the run through the shared builder, so the live toolbar and the
	// configurator's preview are laid out by the same code and cannot disagree
	// about orientation, alignment or spacer sizing.
	StreamUP::ToolbarBuild::Options buildOpts;
	buildOpts.axis = StreamUP::ToolbarGeom::Axis(buildVertical);
	buildOpts.alignment = currentAlignment();
	buildOpts.spacing = StreamUP::UIStyles::S(1);

	auto makeWidgets = [this, buildVertical](const std::shared_ptr<StreamUP::ToolbarConfig::ToolbarItem>& item) -> QList<QWidget*> {
		QList<QWidget*> widgets;

		// A status readout is text, not a button, so it never goes near
		// createButtonFromConfig. The compact form is decided here, where the
		// orientation we are building into is already known.
		if (item->type == StreamUP::ToolbarConfig::ItemType::StatusItem) {
			auto statusItem = std::static_pointer_cast<StreamUP::ToolbarConfig::StatusItem>(item);
			StreamUP::ToolbarStatus::Kind kind;
			if (!StreamUP::ToolbarStatus::kindFromKey(statusItem->kind, kind))
				return widgets;

			auto* readout = new StreamUP::ToolbarStatus::StatusWidget(
				kind, buildVertical, statusItem->showIcon, statusItem->showHours, centralWidget);
			readout->setObjectName(item->id);
			widgets.append(readout);
			return widgets;
		}

		QToolButton* button = createButtonFromConfig(item);
		if (!button)
			return widgets;

		button->setObjectName(item->id);
		dynamicButtons[item->id] = button;
		widgets.append(button);

		// Some buttons bring a companion that sits immediately after them and
		// stays hidden until it applies.
		if (item->type == StreamUP::ToolbarConfig::ItemType::Button) {
			auto buttonItem = std::dynamic_pointer_cast<StreamUP::ToolbarConfig::ButtonItem>(item);
			if (buttonItem && buttonItem->buttonType == "record") {
				QToolButton* newPauseButton = new QToolButton(centralWidget);
				newPauseButton->setObjectName("pauseButton");
				newPauseButton->setProperty("class", "streamup-toolbar-button");
				newPauseButton->setProperty("buttonType", "streamup-button");
				newPauseButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
				newPauseButton->setIcon(getCachedIcon("pause"));
				newPauseButton->setToolTip(obs_module_text("Toolbar.Tooltip.PauseRecording"));
				newPauseButton->setCheckable(true);
				newPauseButton->setVisible(false);
				connect(newPauseButton, &QToolButton::clicked, this, &StreamUPToolbar::onPauseButtonClicked);
				pauseButton = newPauseButton;
				recordButton = button;
				widgets.append(newPauseButton);
			} else if (buttonItem && buttonItem->buttonType == "replay_buffer") {
				QToolButton* newSaveReplayButton = new QToolButton(centralWidget);
				newSaveReplayButton->setObjectName("saveReplayButton");
				newSaveReplayButton->setProperty("class", "streamup-toolbar-button");
				newSaveReplayButton->setProperty("buttonType", "streamup-button");
				newSaveReplayButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
				newSaveReplayButton->setIcon(getCachedIcon("save-replay"));
				newSaveReplayButton->setToolTip(obs_module_text("Toolbar.Tooltip.SaveReplay"));
				newSaveReplayButton->setCheckable(false);
				newSaveReplayButton->setVisible(false);
				connect(newSaveReplayButton, &QToolButton::clicked, this, &StreamUPToolbar::onSaveReplayButtonClicked);
				saveReplayButton = newSaveReplayButton;
				replayBufferButton = button;
				widgets.append(newSaveReplayButton);
			}
		}

		return widgets;
	};

	auto built = StreamUP::ToolbarBuild::build(toolbarConfig, buildOpts, this, makeWidgets);
	centralWidget = built.container;
	mainLayout = built.layout;

	applyLayoutMargins();

	// Add the central widget to toolbar
	addWidget(centralWidget);

	// Update toolbar size constraints based on icon size and orientation
	updateToolbarSizeConstraints();

	// Note: applySizeClass() is called from updatePositionAwareTheme(), which
	// runs immediately after this in the construction/refresh sequence. We
	// need the position-aware objectName ("StreamUPToolbar-Top" etc.) on the
	// toolbar before applying size, otherwise the theme's
	// `QToolBar#StreamUPToolbar-Top[size="..."]` selectors don't match.

	// Pin natural minimums once the theme has styled the run, so a bar that is
	// too short loses space out of its spacers rather than squashing every
	// button to the 4px the theme allows.
	QMetaObject::invokeMethod(
		this, [this, placed = built.placed, buildVertical]() {
			StreamUP::ToolbarBuild::Result styled;
			styled.placed = placed;
			StreamUP::ToolbarBuild::pinNaturalMinimums(styled, StreamUP::ToolbarGeom::Axis(buildVertical));
		},
		Qt::QueuedConnection);

	// Clear flag and update buttons now that reconstruction is complete
	isReconstructingUI = false;
	updateAllButtons();
}

QToolButton* StreamUPToolbar::createButtonFromConfig(std::shared_ptr<StreamUP::ToolbarConfig::ToolbarItem> item)
{
	QToolButton* button = new QToolButton(centralWidget);

	// Icon size inherited from QToolBar::iconSize() which is set by OBS theme
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
		button->setProperty("hotkeyContext", hotkeyItem->hotkeyContext);
		connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onHotkeyButtonClicked);
	} else if (item->type == StreamUP::ToolbarConfig::ItemType::WebSocketButton) {
		auto wsItem = std::static_pointer_cast<StreamUP::ToolbarConfig::WebSocketButtonItem>(item);

		button->setIcon(StreamUP::ToolbarBuild::iconForItem(item));
		button->setToolTip(wsItem->tooltip.isEmpty()
					   ? StreamUP::ToolbarBuild::labelForItem(item)
					   : wsItem->tooltip);
		button->setCheckable(false);
		button->setProperty("websocketItemId", wsItem->id);
		connect(button, &QToolButton::clicked, this, &StreamUPToolbar::onWebSocketButtonClicked);
	}

	// Nothing resolved to an icon, so show the label instead. An icon-only
	// button with a null icon is an invisible square on the bar.
	if (button->icon().isNull()) {
		const QString label = StreamUP::ToolbarBuild::labelForItem(item);
		if (!label.isEmpty()) {
			button->setToolButtonStyle(Qt::ToolButtonTextOnly);
			button->setText(label);
		}
	}

	return button;
}

void StreamUPToolbar::onWebSocketButtonClicked()
{
	QToolButton* button = qobject_cast<QToolButton*>(sender());
	if (!button)
		return;

	const QString itemId = button->property("websocketItemId").toString();
	auto item = toolbarConfig.findItem(itemId);
	auto wsItem = std::dynamic_pointer_cast<StreamUP::ToolbarConfig::WebSocketButtonItem>(item);
	if (!wsItem || wsItem->requestType.isEmpty())
		return;

	// The arguments are stored as JSON text so the item stays serialisable.
	// obs_data_create_from_json returns null on malformed input, which the call
	// below treats as no arguments rather than refusing to fire.
	obs_data_t* requestData = nullptr;
	if (!wsItem->requestData.trimmed().isEmpty()) {
		requestData = obs_data_create_from_json(wsItem->requestData.toUtf8().constData());
		if (!requestData) {
			StreamUP::DebugLogger::LogWarning(
				"Toolbar", "WebSocket button has request data that is not valid JSON, sending none");
		}
	}

	using Source = StreamUP::ToolbarConfig::WebSocketButtonItem::Source;
	obs_websocket_request_response* response = nullptr;

	if (wsItem->source == Source::ObsWebSocket) {
		response = obs_websocket_call_request(wsItem->requestType.toUtf8().constData(), requestData);
	} else {
		// A vendor request goes through obs-websocket's own CallVendorRequest,
		// which takes the vendor and the real request nested inside it.
		const QString vendor = (wsItem->source == Source::StreamUP) ? QStringLiteral("streamup")
									    : wsItem->vendorName;
		if (vendor.isEmpty()) {
			obs_data_release(requestData);
			return;
		}

		obs_data_t* wrapper = obs_data_create();
		obs_data_set_string(wrapper, "vendorName", vendor.toUtf8().constData());
		obs_data_set_string(wrapper, "requestType", wsItem->requestType.toUtf8().constData());
		if (requestData)
			obs_data_set_obj(wrapper, "requestData", requestData);
		response = obs_websocket_call_request("CallVendorRequest", wrapper);
		obs_data_release(wrapper);
	}

	obs_data_release(requestData);

	if (response) {
		const QString responseJson =
			response->response_data ? QString::fromUtf8(response->response_data) : QString();

		QString errorText;
		if (response->status_code != 100) {
			StreamUP::DebugLogger::LogWarningFormat(
				"Toolbar", "WebSocket request '%s' failed with status %d: %s",
				wsItem->requestType.toUtf8().constData(), response->status_code,
				response->comment ? response->comment : "no detail");

			errorText = QString(obs_module_text("StreamUP.Toolbar.WebSocket.Response.Failed"))
					    .arg(response->status_code)
					    .arg(response->comment ? QString::fromUtf8(response->comment)
								   : QString(obs_module_text(
									     "StreamUP.Toolbar.WebSocket.Response.NoDetail")));
		}

		// Read everything off the response before freeing it — response_data and
		// comment are bfree'd inside the call below.
		obs_websocket_request_response_free(response);

		// A request that returns data is worth seeing; one that returns nothing
		// and succeeded is not worth a popover interrupting a live stream.
		const bool hasData = !responseJson.trimmed().isEmpty() && responseJson.trimmed() != "{}";
		if (hasData || !errorText.isEmpty())
			StreamUP::showWebSocketResponsePopover(button, wsItem->requestType, responseJson, errorText);
	} else {
		// Null means obs-websocket is not loaded at all.
		StreamUP::DebugLogger::LogWarning("Toolbar",
						  "WebSocket request could not be sent, obs-websocket is not available");
		StreamUP::showWebSocketResponsePopover(button, wsItem->requestType, QString(),
						       obs_module_text("StreamUP.Toolbar.WebSocket.Response.Unavailable"));
	}
}

void StreamUPToolbar::updateButtonSizes()
{
	// No custom sizing — OBS theme CSS handles icon sizes and button dimensions.
	// Just refresh geometry so the theme can reapply.
	if (centralWidget) {
		centralWidget->updateGeometry();
	}
	updateGeometry();
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
	
	// The context names the source that registered it, so a mute button fires
	// the right camera rather than whichever source enumerated first.
	const QString hotkeyContext = button->property("hotkeyContext").toString();
	bool success = StreamUP::OBSHotkeyManager::triggerHotkey(hotkeyName, hotkeyContext);
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
		su::info(this, QString::fromUtf8(obs_module_text("Dock.Title")),
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
		su::info(this, QString::fromUtf8(obs_module_text("StreamUP.Toolbar.UnknownAction")),
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
		su::info(this, QString::fromUtf8(obs_module_text("Dock.Title")),
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
