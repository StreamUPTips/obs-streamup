#include "streamup-toolbar.hpp"
#include "ui-styles.hpp"
#include <QIcon>
#include <QHBoxLayout>
#include <QWidget>
#include <QSizePolicy>
#include <QMainWindow>
#include <QAction>

StreamUPToolbar::StreamUPToolbar(QWidget *parent) : QToolBar(parent), 
	streamButton(nullptr), recordButton(nullptr), pauseButton(nullptr), 
	replayBufferButton(nullptr), virtualCameraButton(nullptr), 
	studioModeButton(nullptr), settingsButton(nullptr)
{
	setObjectName("StreamUPToolbar");
	setWindowTitle("StreamUP Controls");

	setupUI();
	updateAllButtons();
}

StreamUPToolbar::~StreamUPToolbar()
{
	// Simple cleanup
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
	layout->setContentsMargins(8, 4, 8, 4);
	layout->setSpacing(4);
	
	// Create streaming button
	streamButton = new QToolButton(centralWidget);
	streamButton->setFixedSize(34, 34);
	streamButton->setIcon(QIcon(":images/icons/ui/stream.svg"));
	streamButton->setIconSize(QSize(20, 20));
	streamButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	streamButton->setToolTip("Start/Stop Streaming");
	streamButton->setCheckable(true);
	connect(streamButton, &QToolButton::clicked, this, &StreamUPToolbar::onStreamButtonClicked);
	layout->addWidget(streamButton);
	
	// Create recording button
	recordButton = new QToolButton(centralWidget);
	recordButton->setFixedSize(34, 34);
	recordButton->setIcon(QIcon(":images/icons/ui/record.svg"));
	recordButton->setIconSize(QSize(20, 20));
	recordButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	recordButton->setToolTip("Start/Stop Recording");
	recordButton->setCheckable(true);
	connect(recordButton, &QToolButton::clicked, this, &StreamUPToolbar::onRecordButtonClicked);
	layout->addWidget(recordButton);
	
	// Create pause recording button
	pauseButton = new QToolButton(centralWidget);
	pauseButton->setFixedSize(34, 34);
	pauseButton->setIcon(QIcon(":images/icons/ui/pause.svg"));
	pauseButton->setIconSize(QSize(20, 20));
	pauseButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	pauseButton->setToolTip("Pause/Resume Recording");
	pauseButton->setCheckable(true);
	connect(pauseButton, &QToolButton::clicked, this, &StreamUPToolbar::onPauseButtonClicked);
	layout->addWidget(pauseButton);
	
	// Create replay buffer button
	replayBufferButton = new QToolButton(centralWidget);
	replayBufferButton->setFixedSize(34, 34);
	replayBufferButton->setIcon(QIcon(":images/icons/ui/replay-buffer.svg"));
	replayBufferButton->setIconSize(QSize(20, 20));
	replayBufferButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	replayBufferButton->setToolTip("Start/Stop Replay Buffer");
	replayBufferButton->setCheckable(true);
	connect(replayBufferButton, &QToolButton::clicked, this, &StreamUPToolbar::onReplayBufferButtonClicked);
	layout->addWidget(replayBufferButton);
	
	// Create virtual camera button
	virtualCameraButton = new QToolButton(centralWidget);
	virtualCameraButton->setFixedSize(34, 34);
	virtualCameraButton->setIcon(QIcon(":images/icons/ui/virtual-camera.svg"));
	virtualCameraButton->setIconSize(QSize(20, 20));
	virtualCameraButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	virtualCameraButton->setToolTip("Start/Stop Virtual Camera");
	virtualCameraButton->setCheckable(true);
	connect(virtualCameraButton, &QToolButton::clicked, this, &StreamUPToolbar::onVirtualCameraButtonClicked);
	layout->addWidget(virtualCameraButton);
	
	// Create studio mode button
	studioModeButton = new QToolButton(centralWidget);
	studioModeButton->setFixedSize(34, 34);
	studioModeButton->setIcon(QIcon(":images/icons/ui/studio-mode.svg"));
	studioModeButton->setIconSize(QSize(20, 20));
	studioModeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	studioModeButton->setToolTip("Toggle Studio Mode");
	studioModeButton->setCheckable(true);
	connect(studioModeButton, &QToolButton::clicked, this, &StreamUPToolbar::onStudioModeButtonClicked);
	layout->addWidget(studioModeButton);
	
	// Create settings button
	settingsButton = new QToolButton(centralWidget);
	settingsButton->setFixedSize(34, 34);
	settingsButton->setIcon(QIcon(":images/icons/ui/settings.svg"));
	settingsButton->setIconSize(QSize(20, 20));
	settingsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	settingsButton->setToolTip("Open Settings");
	settingsButton->setCheckable(false); // Settings button is not checkable
	connect(settingsButton, &QToolButton::clicked, this, &StreamUPToolbar::onSettingsButtonClicked);
	layout->addWidget(settingsButton);
	
	// Add the central widget to toolbar
	addWidget(centralWidget);
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

void StreamUPToolbar::updateStreamButton()
{
	if (streamButton) {
		bool streaming = obs_frontend_streaming_active();
		streamButton->setChecked(streaming);
		streamButton->setToolTip(streaming ? "Stop Streaming" : "Start Streaming");
	}
}

void StreamUPToolbar::updateRecordButton()
{
	if (recordButton) {
		bool recording = obs_frontend_recording_active();
		recordButton->setChecked(recording);
		recordButton->setToolTip(recording ? "Stop Recording" : "Start Recording");
	}
}

void StreamUPToolbar::updatePauseButton()
{
	if (pauseButton) {
		bool recording = obs_frontend_recording_active();
		bool paused = obs_frontend_recording_paused();
		pauseButton->setEnabled(recording);
		pauseButton->setChecked(paused);
		pauseButton->setToolTip(paused ? "Resume Recording" : "Pause Recording");
	}
}

void StreamUPToolbar::updateReplayBufferButton()
{
	if (replayBufferButton) {
		bool active = obs_frontend_replay_buffer_active();
		replayBufferButton->setChecked(active);
		replayBufferButton->setToolTip(active ? "Stop Replay Buffer" : "Start Replay Buffer");
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

void StreamUPToolbar::updateAllButtons()
{
	updateStreamButton();
	updateRecordButton();
	updatePauseButton();
	updateReplayBufferButton();
	updateVirtualCameraButton();
	updateStudioModeButton();
}

#include "streamup-toolbar.moc"
