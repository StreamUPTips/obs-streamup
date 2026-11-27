#include "../../core/source-manager.hpp"
#include <streamup/debug-logger.hpp>
#include "streamup-dock.hpp"
#include "../../ui_StreamUPDock.h"
#include "../../flow-layout.hpp"
#include "../../video-capture-popup.hpp"
#include "../../version.h"
#include "../settings-manager.hpp"
#include "../ui-helpers.hpp"
#include <streamup/ui/window-chrome.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/switch-button.hpp>
#include <streamup/ui/dialogs.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
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
#include <QPointer>
#include <vector>
#include <functional>

using namespace StreamUP::UIStyles;
namespace su = StreamUP::UIStyles;

// Static list to track all dock instances
static QList<StreamUPDock*> dockInstances;

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
		// 20 in a 28 button: a 4 px ring all round, whole pixels at 100%,
		// 150% and 200%, and big enough for the outline glyphs to read.
		button->setIconSize(QSize(StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(20)));
		// Force square dimensions aggressively
		button->setFixedSize(StreamUP::UIStyles::S(28), StreamUP::UIStyles::S(28));
		button->setMinimumSize(StreamUP::UIStyles::S(28), StreamUP::UIStyles::S(28));
		button->setMaximumSize(StreamUP::UIStyles::S(28), StreamUP::UIStyles::S(28));
		button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		// Remove any padding/margins that might stretch the button
		button->setContentsMargins(0, 0, 0, 0);

		// Apply minimal structural styling - only size and spacing properties, no colors
		// This ensures the FlowLayout calculates positions correctly
		button->setStyleSheet(StreamUP::UIStyles::scale_qss(
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
		));
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

	// Visibility button reflects what the next click will do, based on the
	// current selection. Idle (nothing selected) = "visible" icon. Any
	// selected item already visible = next click hides them = "visible-off"
	// icon. All selected items hidden = next click shows them = "visible".
	updateVisibilityButtonIcon();
}

void StreamUPDock::updateVisibilityButtonIcon()
{
	if (!toggleVisibilityButton) return;

	const bool hasSelection = StreamUP::SourceManager::HasAnySelectedSceneItem();
	const bool anyVisible = hasSelection && StreamUP::SourceManager::CheckIfAnySelectedVisible();

	const QString iconKey = (hasSelection && anyVisible) ? "visible-off" : "visible";
	applyFileIconToButton(toggleVisibilityButton, StreamUP::UIHelpers::GetThemedIconPath(iconKey));

	toggleVisibilityButton->setToolTip(
		hasSelection
			? (anyVisible ? obs_module_text("Dock.Tool.HideSelectedSources.Title")
				      : obs_module_text("Dock.Tool.ShowSelectedSources.Title"))
			: obs_module_text("Dock.Tool.ToggleVisibilitySelectedSources.Title"));
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
			signal_handler_connect(scene_handler, "item_select", StreamUPDock::onItemSelectionChanged, this);
			signal_handler_connect(scene_handler, "item_deselect", StreamUPDock::onItemSelectionChanged, this);
			signal_handler_connect(scene_handler, "item_visible", StreamUPDock::onItemVisibilityChanged, this);
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
			signal_handler_disconnect(scene_handler, "item_select", StreamUPDock::onItemSelectionChanged, this);
			signal_handler_disconnect(scene_handler, "item_deselect", StreamUPDock::onItemSelectionChanged, this);
			signal_handler_disconnect(scene_handler, "item_visible", StreamUPDock::onItemVisibilityChanged, this);
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

	if (event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN ||
	    event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP) {
		// Hand the scene back BEFORE OBS clears scene data. SCRIPTING_SHUTDOWN
		// is raised on the line above OBSBasic::ClearSceneData(), which is the
		// last moment this reference can be released in time. Waiting for the
		// destructor (or EXIT, which arrives after the clear) kept the current
		// scene - and every source under it - alive through the teardown, which
		// OBS reports as "Not all sources were cleared when clearing scene
		// data" and then force-destroys anyway. Our release then landed on
		// freed sources: "Double destroy just occurred", followed by a crash
		// in scene_destroy.
		dock->disconnectSceneSignals();
		dock->m_sceneCollectionChanging = true;
		return;
	}

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

void StreamUPDock::onItemSelectionChanged(void *param, calldata_t *data)
{
	Q_UNUSED(data);

	StreamUPDock *self = static_cast<StreamUPDock *>(param);
	if (self->isProcessing || self->m_sceneCollectionChanging)
		return;
	// Selection changes only affect the visibility button, no need to refresh
	// every other icon — go straight to the targeted update.
	QMetaObject::invokeMethod(self, "updateVisibilityButtonIcon", Qt::QueuedConnection);
}

void StreamUPDock::onItemVisibilityChanged(void *param, calldata_t *data)
{
	Q_UNUSED(data);

	StreamUPDock *self = static_cast<StreamUPDock *>(param);
	if (self->isProcessing || self->m_sceneCollectionChanging)
		return;
	QMetaObject::invokeMethod(self, "updateVisibilityButtonIcon", Qt::QueuedConnection);
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

	// Pick the buttons right here: tick to show, untick to hide.
	using DTS = StreamUP::SettingsManager::DockToolSettings;
	const DTS current = StreamUP::SettingsManager::GetDockToolSettings();
	struct Choice {
		const char *key;
		bool DTS::*field;
	};
	static const Choice choices[] = {
		{"Dock.Tool.LockAllSources.Title", &DTS::showLockAllSources},
		{"Dock.Tool.LockCurrentSources.Title", &DTS::showLockCurrentSources},
		{"Dock.Tool.RefreshBrowserSources.Title", &DTS::showRefreshBrowserSources},
		{"Dock.Tool.RefreshAudioMonitoring.Title", &DTS::showRefreshAudioMonitoring},
		{"Dock.Tool.VideoCaptureOptions.Title", &DTS::showVideoCaptureOptions},
		{"Dock.Tool.GroupSelectedSources.Title", &DTS::showGroupSelectedSources},
		{"Dock.Tool.ToggleVisibilitySelectedSources.Title", &DTS::showToggleVisibilitySelectedSources},
	};
	QAction *heading = contextMenu.addAction(obs_module_text("Dock.ContextMenu.Buttons"));
	heading->setEnabled(false);
	for (const Choice &c : choices) {
		QAction *a = contextMenu.addAction(obs_module_text(c.key));
		a->setCheckable(true);
		a->setChecked(current.*(c.field));
		const auto field = c.field;
		connect(a, &QAction::toggled, [field](bool on) {
			DTS s = StreamUP::SettingsManager::GetDockToolSettings();
			s.*field = on;
			StreamUP::SettingsManager::UpdateDockToolSettings(s);
		});
	}
	contextMenu.addSeparator();

	QAction* configureAction = contextMenu.addAction(obs_module_text("Dock.ContextMenu.Configure"));
	connect(configureAction, &QAction::triggered, []() {
		StreamUP::SettingsManager::ShowSettingsDialog(StreamUP::SettingsManager::SettingsTabDockConfig);
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
