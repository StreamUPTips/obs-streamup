#include "streamup-dock.hpp"
#include "ui_StreamUPDock.h"
#include <QMainWindow>
#include <QDockWidget>
#include "streamup.hpp"
#include <obs-frontend-api.h>
#include <QIcon>
#include <QStyle>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <obs.h>
#include <obs-module.h>

// Function to apply a themeID to a button
void StreamUPDock::applyThemeIDToButton(QPushButton *button,
					const QString &themeID)
{
	button->setProperty("themeID", themeID);
	button->setMinimumSize(40, 40);
	button->setMaximumSize(40, 40);
	button->setIconSize(QSize(20, 20));
	button->style()->unpolish(button);
	button->style()->polish(button);
}

// Function to apply an icon from a file to a button
void StreamUPDock::applyFileIconToButton(QPushButton *button,
					 const QString &filePath)
{
	button->setIcon(QIcon(filePath));
	button->setMinimumSize(40, 40);
	button->setMaximumSize(40, 40);
	button->setIconSize(QSize(20, 20));
}

StreamUPDock::StreamUPDock(QWidget *parent)
	: QWidget(parent), ui(new Ui::StreamUPDock), isProcessing(false)
{
	ui->setupUi(this);

	// Create buttons
	button1 = new QPushButton(this);
	button2 = new QPushButton(this);
	button3 = new QPushButton(this);
	button4 = new QPushButton(this);

	// Apply initial icons to buttons
	applyFileIconToButton(button1, ":images/all-scene-source-locked.svg");
	applyFileIconToButton(button2,
			      ":images/current-scene-source-locked.svg");
	applyFileIconToButton(button3, ":images/refresh-browser-sources.svg");
	applyFileIconToButton(button4, ":images/refresh-audio-monitoring.svg");

	// Set tooltips for buttons
	button1->setToolTip(obs_module_text("LockAllSources"));
	button2->setToolTip(obs_module_text("LockAllCurrentSources"));
	button3->setToolTip(obs_module_text("RefreshBrowserSources"));
	button4->setToolTip(obs_module_text("RefreshAudioMonitoring"));

	// Create a horizontal layout to hold the buttons
	QHBoxLayout *mainLayout = new QHBoxLayout;

	mainLayout->addWidget(button1);
	mainLayout->addWidget(button2);
	mainLayout->addWidget(button3);
	mainLayout->addWidget(button4);

	// Set the layout to the StreamupDock
	this->setLayout(mainLayout);

	// Connect buttons to their respective slots
	connect(button1, &QPushButton::clicked, this,
		&StreamUPDock::ButtonToggleLockAllSources);
	connect(button2, &QPushButton::clicked, this,
		&StreamUPDock::ButtonToggleLockSourcesInCurrentScene);
	connect(button3, &QPushButton::clicked, this,
		&StreamUPDock::ButtonRefreshBrowserSources);
	connect(button4, &QPushButton::clicked, this,
		&StreamUPDock::ButtonRefreshAudioMonitoring);

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
	delete ui;
}

void StreamUPDock::ButtonToggleLockAllSources()
{
	if (isProcessing)
		return;
	isProcessing = true;

	ToggleLockAllSources(false);
	updateButtonIcons();

	isProcessing = false;
}

void StreamUPDock::ButtonToggleLockSourcesInCurrentScene()
{
	if (isProcessing)
		return; 
	isProcessing = true;

	ToggleLockSourcesInCurrentScene(false);
	updateButtonIcons();

	isProcessing = false;
}

void StreamUPDock::ButtonRefreshAudioMonitoring()
{
	if (isProcessing)
		return; 
	isProcessing = true;

	obs_enum_sources(RefreshAudioMonitoring, nullptr);

	isProcessing = false;
}

void StreamUPDock::ButtonRefreshBrowserSources()
{
	if (isProcessing)
		return; 
	isProcessing = true;

	obs_enum_sources(RefreshBrowserSources, nullptr);

	isProcessing = false;
}

void StreamUPDock::updateButtonIcons()
{
	if (AreAllSourcesLockedInAllScenes()) {
		applyFileIconToButton(button1, ":images/all-scene-source-locked.svg");
	} else {
		applyFileIconToButton(button1, ":images/all-scene-source-unlocked.svg");
	}

	if (AreAllSourcesLockedInCurrentScene()) {
		applyFileIconToButton(button2, ":images/current-scene-source-locked.svg");
	} else {
		applyFileIconToButton(button2, ":images/current-scene-source-unlocked.svg");
	}
}

bool StreamUPDock::AreAllSourcesLockedInAllScenes()
{
	return !CheckIfAnyUnlockedInAllScenes();
}

bool StreamUPDock::AreAllSourcesLockedInCurrentScene()
{
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		return false;
	}

	obs_scene_t *scene = obs_scene_from_source(current_scene);
	bool result = !CheckIfAnyUnlocked(scene);
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
		signal_handler_t *scene_handler =
			obs_source_get_signal_handler(current_scene);
		if (scene_handler) {
			signal_handler_connect(scene_handler, "item_add",
					       StreamUPDock::onSceneItemAdded,
					       this);
			signal_handler_connect(scene_handler, "item_remove",
					       StreamUPDock::onSceneItemRemoved,
					       this);
			signal_handler_connect(scene_handler, "item_locked",
					       StreamUPDock::onItemLockChanged,
					       this);
		}
		obs_source_release(current_scene);
	}
}

void StreamUPDock::disconnectSceneSignals()
{
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (current_scene) {
		signal_handler_t *scene_handler =
			obs_source_get_signal_handler(current_scene);
		if (scene_handler) {
			signal_handler_disconnect(
				scene_handler, "item_add",
				StreamUPDock::onSceneItemAdded, this);
			signal_handler_disconnect(
				scene_handler, "item_remove",
				StreamUPDock::onSceneItemRemoved, this);
			signal_handler_disconnect(
				scene_handler, "item_locked",
				StreamUPDock::onItemLockChanged, this);
		}
		obs_source_release(current_scene);
	}
}

void StreamUPDock::onFrontendEvent(enum obs_frontend_event event,
				   void *private_data)
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
