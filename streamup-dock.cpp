#include "source-manager.hpp"
#include "streamup-dock.hpp"
#include "ui_StreamUPDock.h"
#include "flow-layout.hpp"
#include "video-capture-popup.hpp"
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

void StreamUPDock::applyFileIconToButton(QPushButton *button, const QString &filePath)
{
	button->setIcon(QIcon(filePath));
}

StreamUPDock::StreamUPDock(QWidget *parent) : QFrame(parent), ui(new Ui::StreamUPDock), isProcessing(false), videoCapturePopup(nullptr)
{
	ui->setupUi(this);

	// Create buttons
	button1 = new QPushButton(this);
	button2 = new QPushButton(this);
	button3 = new QPushButton(this);
	button4 = new QPushButton(this);
	videoCaptureButton = new QPushButton(this);

	// Apply initial icons to buttons
	applyFileIconToButton(button1, ":images/all-scene-source-locked.svg");
	applyFileIconToButton(button2, ":images/current-scene-source-locked.svg");
	applyFileIconToButton(button3, ":images/refresh-browser-sources.svg");
	applyFileIconToButton(button4, ":images/refresh-audio-monitoring.svg");
	applyFileIconToButton(videoCaptureButton, ":Qt/icons/16x16/camera-video.png");

	auto setButtonProperties = [](QPushButton *button) {
		button->setIconSize(QSize(20, 20));
		button->setFixedSize(40, 40);
		button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	};

	// Set properties for each button
	setButtonProperties(button1);
	setButtonProperties(button2);
	setButtonProperties(button3);
	setButtonProperties(button4);
	setButtonProperties(videoCaptureButton);

	// Set tooltips for buttons
	button1->setToolTip(obs_module_text("LockAllSources"));
	button2->setToolTip(obs_module_text("LockAllCurrentSources"));
	button3->setToolTip(obs_module_text("RefreshBrowserSources"));
	button4->setToolTip(obs_module_text("RefreshAudioMonitoring"));
	videoCaptureButton->setToolTip("Video Capture Device Options");

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
}

StreamUPDock::~StreamUPDock()
{
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

	// Close existing popup if open
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
		applyFileIconToButton(button1, ":images/all-scene-source-locked.svg");
	} else {
		applyFileIconToButton(button1, ":images/all-scene-source-unlocked.svg");
	}

	// Update button2 icon based on whether all sources are locked in the current scene
	if (StreamUP::SourceManager::AreAllSourcesLockedInCurrentScene()) {
		applyFileIconToButton(button2, ":images/current-scene-source-locked.svg");
	} else {
		applyFileIconToButton(button2, ":images/current-scene-source-unlocked.svg");
	}

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
