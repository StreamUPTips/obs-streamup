#include "mixer-dock.hpp"
#include "mixer-strip.hpp"

#include <streamup/debug-logger.hpp>
#include <streamup/ui/dialogs.hpp>
#include <streamup/ui/gallery-style.hpp>

#include <obs-module.h>
#include <obs.hpp>
#include <util/config-file.h>
#include <util/platform.h>
#include "../ui-helpers.hpp"

#include <QApplication>
#include <QBoxLayout>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QActionGroup>
#include <QHash>
#include <QSet>
#include <QStackedWidget>
#include <QDockWidget>
#include <QPointer>
#include <QTimer>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

#include "moc_mixer-dock.cpp"

// OBS's frontend slots read the subject source from a QAction property of
// this type. Declared here so the property carries the same registered type.
Q_DECLARE_METATYPE(OBSSource);


namespace StreamUP {
namespace Mixer {

using StreamUP::UIStyles::S;

namespace {

constexpr const char *kDockId = "StreamUPMixer";
constexpr const char *kModuleKey = "streamup_mixer";
constexpr const char *kMimeType = "application/x-streamup-mixer-strip";
constexpr int kFormatVersion = 1;
constexpr int kSaveDelayMs = 1500;
constexpr int kHoverSwitchMs = 600;

MixerDock *g_instance = nullptr;

QString T(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

// A removed source can stay alive for a moment while the last references to
// it are let go, so it is skipped explicitly.
bool isAudioSource(obs_source_t *source)
{
	return source && !obs_source_removed(source) && (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) != 0;
}

QMimeData *encodeDrag(const QString &uuid, int fromTab)
{
	auto *mime = new QMimeData();
	mime->setData(kMimeType, (uuid + "\n" + QString::number(fromTab)).toUtf8());
	return mime;
}

StripDrag decodeDrag(const QMimeData *mime)
{
	StripDrag drag;
	if (!mime || !mime->hasFormat(kMimeType))
		return drag;
	const QStringList parts = QString::fromUtf8(mime->data(kMimeType)).split('\n');
	if (parts.size() != 2)
		return drag;
	drag.uuid = parts[0];
	drag.fromTab = parts[1].toInt();
	drag.valid = !drag.uuid.isEmpty();
	return drag;
}

bool isCopyDrop(const QDropEvent *event)
{
	return (event->modifiers() & Qt::ControlModifier) != 0;
}

// OBS's own strings, so menus and dialogs read exactly as in its mixer and
// follow the user's OBS language.
QString obsText(const char *key)
{
	const char *text = obs_frontend_get_locale_string(key);
	return QString::fromUtf8(text ? text : key);
}

// OBS's mixer keeps hide, pin and volume lock in each source's private
// settings and re-sorts when the main window's mixerStatusChanged signal
// fires. The signal is invoked by name, so on an OBS without it this quietly
// does nothing and the native mixer picks the flag up when it next reloads.
// The StreamUP Mixer listens to the same signal, so this updates both.
void setMixerFlag(obs_source_t *source, const char *key, bool value)
{
	if (!source)
		return;
	obs_data_t *priv = obs_source_get_private_settings(source);
	obs_data_set_bool(priv, key, value);
	obs_data_release(priv);

	QObject *main = static_cast<QObject *>(obs_frontend_get_main_window());
	if (main) {
		QMetaObject::invokeMethod(main, "mixerStatusChanged", Qt::DirectConnection,
					  Q_ARG(QString, QString::fromUtf8(obs_source_get_uuid(source))));
	}
}

void setMixerFlag(const QString &uuid, const char *key, bool value)
{
	obs_source_t *source = obs_get_source_by_uuid(uuid.toUtf8().constData());
	setMixerFlag(source, key, value);
	obs_source_release(source);
}

// Runs one of OBSBasic's filter clipboard or dialog slots on a source. Those
// slots read the subject from sender()->property("source"), so a throwaway
// action carrying it, connected by name, drives OBS's real implementation.
bool triggerNativeSourceAction(const QString &uuid, const char *slotSignature)
{
	QObject *main = static_cast<QObject *>(obs_frontend_get_main_window());
	obs_source_t *source = obs_get_source_by_uuid(uuid.toUtf8().constData());
	bool done = false;
	if (main && source) {
		QAction action(nullptr);
		action.setProperty("source", QVariant::fromValue(OBSSource(source)));
		if (QObject::connect(&action, SIGNAL(triggered()), main, slotSignature)) {
			action.trigger();
			done = true;
		}
	}
	obs_source_release(source);
	return done;
}

bool nativePasteFiltersAvailable()
{
	QObject *main = static_cast<QObject *>(obs_frontend_get_main_window());
	QAction *paste = main ? main->findChild<QAction *>("actionPasteFilters") : nullptr;
	return paste && paste->isEnabled();
}

const char *overrideKey(TabOverride value)
{
	switch (value) {
	case TabOverride::Show:
		return "show";
	case TabOverride::Hide:
		return "hide";
	default:
		return "follow";
	}
}

TabOverride overrideFromKey(const QString &key)
{
	if (key == "show")
		return TabOverride::Show;
	if (key == "hide")
		return TabOverride::Hide;
	return TabOverride::FollowMixer;
}

void onSaveLoad(obs_data_t *moduleData, bool saving, void *)
{
	if (!moduleData || !g_instance)
		return;
	if (saving)
		g_instance->saveTo(moduleData);
	else
		g_instance->loadFrom(moduleData);
}

} // namespace

// ============================================================================
// MixerTabBar
// ============================================================================

MixerTabBar::MixerTabBar(QWidget *parent) : QTabBar(parent)
{
	setObjectName("StreamUPMixerTabBar");
	setAcceptDrops(true);
	setMovable(true);
	setExpanding(false);
	setDrawBase(true);
	setUsesScrollButtons(true);
	setElideMode(Qt::ElideRight);
	setContextMenuPolicy(Qt::CustomContextMenu);

	m_hoverSwitch.setSingleShot(true);
	m_hoverSwitch.setInterval(kHoverSwitchMs);
	connect(&m_hoverSwitch, &QTimer::timeout, this, [this]() {
		if (m_hoverTab >= 0 && m_hoverTab < count())
			setCurrentIndex(m_hoverTab);
	});
}

void MixerTabBar::dragEnterEvent(QDragEnterEvent *event)
{
	if (decodeDrag(event->mimeData()).valid)
		event->acceptProposedAction();
	else
		event->ignore();
}

void MixerTabBar::dragMoveEvent(QDragMoveEvent *event)
{
	const int index = tabAt(event->position().toPoint());
	if (index < 0 || !decodeDrag(event->mimeData()).valid) {
		m_hoverSwitch.stop();
		m_hoverTab = -1;
		event->ignore();
		return;
	}
	// Resting on a tab opens it, so a strip can be dropped at an exact spot
	// in another tab rather than only on the end.
	if (index != m_hoverTab) {
		m_hoverTab = index;
		if (index != currentIndex())
			m_hoverSwitch.start();
		else
			m_hoverSwitch.stop();
	}
	event->acceptProposedAction();
}

void MixerTabBar::dragLeaveEvent(QDragLeaveEvent *event)
{
	m_hoverSwitch.stop();
	m_hoverTab = -1;
	QTabBar::dragLeaveEvent(event);
}

void MixerTabBar::dropEvent(QDropEvent *event)
{
	m_hoverSwitch.stop();
	m_hoverTab = -1;
	const StripDrag drag = decodeDrag(event->mimeData());
	const int index = tabAt(event->position().toPoint());
	if (!drag.valid || index < 0) {
		event->ignore();
		return;
	}
	event->acceptProposedAction();
	emit stripDroppedOnTab(index, drag.uuid, drag.fromTab, isCopyDrop(event));
}

// ============================================================================
// StripArea
// ============================================================================

StripArea::StripArea(QWidget *parent) : QWidget(parent)
{
	setAcceptDrops(true);

	// Spacing and margins as OBS's mixer: the theme draws the gaps.
	m_layout = new QBoxLayout(QBoxLayout::LeftToRight, this);
	m_layout->setContentsMargins(0, 0, 0, 0);
	m_layout->setSpacing(0);

	// Drawn in the palette's highlight colour so it follows the theme.
	m_indicator = new QWidget(this);
	m_indicator->setObjectName("streamupMixerDropIndicator");
	m_indicator->setAutoFillBackground(true);
	m_indicator->setBackgroundRole(QPalette::Highlight);
	m_indicator->hide();
}

void StripArea::setVertical(bool vertical)
{
	m_vertical = vertical;
	// The object names OBS's mixer uses, which the themes' rules hang off.
	setObjectName(vertical ? "vVolumeWidgets" : "hVolumeWidgets");
	setSizePolicy(QSizePolicy::Preferred, vertical ? QSizePolicy::Preferred : QSizePolicy::Maximum);
	m_layout->setDirection(vertical ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
	m_layout->setAlignment(vertical ? Qt::AlignLeft : Qt::AlignTop);
}

int StripArea::dropIndexAt(const QPoint &pos) const
{
	for (int i = 0; i < m_strips.size(); i++) {
		const QRect r = m_strips[i]->geometry();
		const int centre = m_vertical ? r.center().x() : r.center().y();
		const int at = m_vertical ? pos.x() : pos.y();
		if (at < centre)
			return i;
	}
	return int(m_strips.size());
}

void StripArea::showIndicator(int index)
{
	const int thickness = std::max(2, S(3));
	const int half = m_layout->spacing() / 2;
	QRect r;
	if (m_strips.isEmpty()) {
		r = m_vertical ? QRect(S(4), S(4), thickness, height() - S(8))
			       : QRect(S(4), S(4), width() - S(8), thickness);
	} else if (index < m_strips.size()) {
		const QRect g = m_strips[index]->geometry();
		r = m_vertical ? QRect(g.left() - half - thickness / 2, g.top(), thickness, g.height())
			       : QRect(g.left(), g.top() - half - thickness / 2, g.width(), thickness);
	} else {
		const QRect g = m_strips.last()->geometry();
		r = m_vertical ? QRect(g.right() + half - thickness / 2 + 1, g.top(), thickness, g.height())
			       : QRect(g.left(), g.bottom() + half - thickness / 2 + 1, g.width(), thickness);
	}
	m_indicator->setGeometry(r);
	m_indicator->raise();
	m_indicator->show();
}

void StripArea::dragEnterEvent(QDragEnterEvent *event)
{
	if (decodeDrag(event->mimeData()).valid)
		event->acceptProposedAction();
	else
		event->ignore();
}

void StripArea::dragMoveEvent(QDragMoveEvent *event)
{
	if (!decodeDrag(event->mimeData()).valid) {
		event->ignore();
		return;
	}
	showIndicator(dropIndexAt(event->position().toPoint()));
	event->acceptProposedAction();
}

void StripArea::dragLeaveEvent(QDragLeaveEvent *event)
{
	m_indicator->hide();
	QWidget::dragLeaveEvent(event);
}

void StripArea::dropEvent(QDropEvent *event)
{
	m_indicator->hide();
	const StripDrag drag = decodeDrag(event->mimeData());
	if (!drag.valid) {
		event->ignore();
		return;
	}
	event->acceptProposedAction();
	emit stripDropped(dropIndexAt(event->position().toPoint()), drag.uuid, drag.fromTab, isCopyDrop(event));
}

// ============================================================================
// MixerDock: construction and lifecycle
// ============================================================================

MixerDock *MixerDock::instance()
{
	return g_instance;
}

MixerDock::MixerDock(QWidget *parent) : ::AudioMixer(parent)
{
	g_instance = this;
	setObjectName("StreamUPMixerDock");
	setFrameShape(QFrame::NoFrame);
	setLineWidth(0);

	m_saveTimer.setSingleShot(true);
	m_saveTimer.setInterval(kSaveDelayMs);
	connect(&m_saveTimer, &QTimer::timeout, this, []() {
		// Writes the collection, which runs our save callback. OBS skips the
		// save itself while it is loading or closing.
		obs_frontend_save();
	});

	buildUi();
	obs_frontend_add_event_callback(&MixerDock::onFrontendEvent, this);

	// Visibility changes arrive in bursts (a scene switch activates and
	// deactivates many sources at once), so they are gathered into one pass.
	m_visibilityTimer.setSingleShot(true);
	m_visibilityTimer.setInterval(0);
	connect(&m_visibilityTimer, &QTimer::timeout, this, [this]() {
		if (!m_frozen)
			syncStrips();
	});

	// Connected by name: OBSBasic is internal to the frontend. On an OBS
	// without a signal that connection simply does not happen.
	if (QObject *main = static_cast<QObject *>(obs_frontend_get_main_window())) {
		const QMetaObject *meta = main->metaObject();
		if (meta->indexOfSignal("mixerStatusChanged(QString)") >= 0)
			connect(main, SIGNAL(mixerStatusChanged(QString)), this,
				SLOT(nativeMixerStatusChanged(QString)));
		if (meta->indexOfSignal("userSettingChanged(std::string,std::string)") >= 0)
			connect(main, SIGNAL(userSettingChanged(std::string, std::string)), this,
				SLOT(obsUserSettingChanged(std::string, std::string)));
		if (meta->indexOfSignal("profileSettingChanged(std::string,std::string)") >= 0)
			connect(main, SIGNAL(profileSettingChanged(std::string, std::string)), this,
				SLOT(obsProfileSettingChanged(std::string, std::string)));
	}
}

void MixerDock::obsUserSettingChanged(const std::string &category, const std::string &name)
{
	if (category == "BasicWindow" &&
	    (name == "MixerShowInactive" || name == "MixerShowHidden" || name == "MixerKeepInactiveLast" ||
	     name == "MixerKeepHiddenLast")) {
		scheduleVisibilityUpdate();
		updateToolbarState();
	} else if (category == "BasicWindow" && name == "ShowListboxToolbars") {
		updateToolbarState();
	} else if (category == "Accessibility" && name == "SettingsChanged") {
		for (MixerStrip *strip : std::as_const(m_pool))
			strip->refreshMeterColours();
	}
}

void MixerDock::obsProfileSettingChanged(const std::string &category, const std::string &name)
{
	if (category == "Audio" && (name == "MeterDecayRate" || name == "PeakMeterType")) {
		for (MixerStrip *strip : std::as_const(m_pool))
			strip->applyMeterSettings();
	}
}

bool MixerDock::obsMixerSetting(const char *name)
{
	config_t *config = obs_frontend_get_user_config();
	return config && config_get_bool(config, "BasicWindow", name);
}

void MixerDock::setObsMixerSetting(const char *name, bool value)
{
	config_t *config = obs_frontend_get_user_config();
	if (!config)
		return;
	config_set_bool(config, "BasicWindow", name, value);
	// Tell OBS (its mixer reloads) and, through the same signal, this dock.
	QObject *main = static_cast<QObject *>(obs_frontend_get_main_window());
	const bool told = main && QMetaObject::invokeMethod(main, "userSettingChanged", Qt::DirectConnection,
							   Q_ARG(std::string, std::string("BasicWindow")),
							   Q_ARG(std::string, std::string(name)));
	if (!told)
		obsUserSettingChanged("BasicWindow", name);
}

bool MixerDock::showInactiveForTab(int index) const
{
	if (index >= 0 && index < m_tabs.size() && m_tabs[index].inactive != TabOverride::FollowMixer)
		return m_tabs[index].inactive == TabOverride::Show;
	return obsMixerSetting("MixerShowInactive");
}

bool MixerDock::showHiddenForTab(int index) const
{
	if (index >= 0 && index < m_tabs.size() && m_tabs[index].hidden != TabOverride::FollowMixer)
		return m_tabs[index].hidden == TabOverride::Show;
	return obsMixerSetting("MixerShowHidden");
}

// OBS's rule (AudioMixer::getMixerVisibilityForControl), with this tab's
// overrides for showing inactive and hidden sources.
bool MixerDock::stripVisible(const MixerStrip *strip) const
{
	const bool hidden = strip->isHiddenInMixer();
	if (strip->isPinnedInMixer())
		return true;
	if (hidden && showHiddenForTab(m_current))
		return true;
	if (!strip->isActive() && showInactiveForTab(m_current))
		return !hidden;
	if (strip->isActive())
		return !hidden;
	if (strip->isPreviewed())
		return !hidden;
	return false;
}

void MixerDock::scheduleVisibilityUpdate()
{
	if (!m_frozen && !m_visibilityTimer.isActive())
		m_visibilityTimer.start();
}

void MixerDock::nativeMixerStatusChanged(QString uuid)
{
	if (MixerStrip *strip = m_pool.value(uuid))
		strip->refreshState();
	// The hidden count covers every strip of the tab, shown or not.
	scheduleVisibilityUpdate();
}

// As OBS works out its "Preview" sources in studio mode: the visible items of
// the preview scene that have audio playing.
void MixerDock::updatePreviewFlags()
{
	m_previewSources.clear();
	obs_source_t *preview = obs_frontend_preview_program_mode_active()
					? obs_frontend_get_current_preview_scene()
					: nullptr;
	if (obs_scene_t *scene = preview ? obs_scene_from_source(preview) : nullptr) {
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
				if (!obs_sceneitem_visible(item))
					return true;
				obs_source_t *source = obs_sceneitem_get_source(item);
				if (source && (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) &&
				    obs_source_audio_active(source))
					static_cast<QSet<QString> *>(param)->insert(
						QString::fromUtf8(obs_source_get_uuid(source)));
				return true;
			},
			&m_previewSources);
	}
	obs_source_release(preview);

	for (MixerStrip *strip : std::as_const(m_pool))
		strip->setPreviewed(m_previewSources.contains(strip->uuid()));
}

void MixerDock::onPreviewItemVisible(void *data, calldata_t *)
{
	auto *dock = static_cast<MixerDock *>(data);
	QMetaObject::invokeMethod(dock, [dock]() { dock->updatePreviewFlags(); }, Qt::QueuedConnection);
}

// Follows items being shown or hidden in the studio mode preview scene, as
// OBS's mixer does. Holds a reference to the watched scene while connected,
// released when the preview changes or the collection goes.
void MixerDock::watchPreviewScene()
{
	if (m_watchedPreview) {
		signal_handler_disconnect(obs_source_get_signal_handler(m_watchedPreview), "item_visible",
					  &MixerDock::onPreviewItemVisible, this);
		obs_source_release(m_watchedPreview);
		m_watchedPreview = nullptr;
	}
	if (m_frozen || !obs_frontend_preview_program_mode_active())
		return;
	m_watchedPreview = obs_frontend_get_current_preview_scene();
	if (m_watchedPreview)
		signal_handler_connect(obs_source_get_signal_handler(m_watchedPreview), "item_visible",
				       &MixerDock::onPreviewItemVisible, this);
}

MixerDock::~MixerDock()
{
	if (g_instance == this)
		g_instance = nullptr;
	clearStrips();
	flushGraveyard();
}

void MixerDock::connectGlobalSignals()
{
	if (m_signalsConnected)
		return;
	signal_handler_t *sh = obs_get_signal_handler();
	signal_handler_connect(sh, "source_create", &MixerDock::onSourceCreate, this);
	signal_handler_connect(sh, "source_remove", &MixerDock::onSourceRemove, this);
	m_signalsConnected = true;
}

void MixerDock::disconnectGlobalSignals()
{
	if (!m_signalsConnected)
		return;
	signal_handler_t *sh = obs_get_signal_handler();
	signal_handler_disconnect(sh, "source_create", &MixerDock::onSourceCreate, this);
	signal_handler_disconnect(sh, "source_remove", &MixerDock::onSourceRemove, this);
	m_signalsConnected = false;
}

void MixerDock::onFrontendEvent(enum obs_frontend_event event, void *data)
{
	static_cast<MixerDock *>(data)->handleFrontendEvent(event);
}

void MixerDock::handleFrontendEvent(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		connectGlobalSignals();
		m_frozen = false;
		applyPendingLoad();
		watchPreviewScene();
		updateToolbarState();
		break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
		// OBS is about to remove every source. Let go of ours first, and stop
		// treating removals as the user deleting things.
		m_frozen = true;
		m_saveTimer.stop();
		watchPreviewScene();
		clearStrips();
		// A collection that carries no layout (a brand new one) must not
		// inherit this one's tabs.
		m_hasPendingLoad = true;
		m_pendingJson.clear();
		break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		m_frozen = true;
		clearStrips();
		break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		m_frozen = false;
		applyPendingLoad();
		watchPreviewScene();
		break;

	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		// Meter decay and peak type are profile settings.
		for (MixerStrip *strip : std::as_const(m_pool))
			strip->applyMeterSettings();
		break;

	case OBS_FRONTEND_EVENT_THEME_CHANGED:
		QTimer::singleShot(250, this, [this]() { refreshTheme(); });
		break;

	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		if (!m_frozen) {
			updatePreviewFlags();
			watchPreviewScene();
		}
		break;

	case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
	case OBS_FRONTEND_EVENT_EXIT:
		// Fires before OBS clears its sources on close: drop every reference
		// the strips hold so nothing outlives the collection.
		m_frozen = true;
		m_saveTimer.stop();
		disconnectGlobalSignals();
		watchPreviewScene();
		clearStrips();
		flushGraveyard();
		if (event == OBS_FRONTEND_EVENT_EXIT)
			obs_frontend_remove_event_callback(&MixerDock::onFrontendEvent, this);
		break;

	default:
		break;
	}
}

void MixerDock::onSourceCreate(void *data, calldata_t *cd)
{
	auto *dock = static_cast<MixerDock *>(data);
	obs_source_t *source = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));
	if (!isAudioSource(source))
		return;
	const QString uuid = QString::fromUtf8(obs_source_get_uuid(source));
	QMetaObject::invokeMethod(dock, [dock, uuid]() { dock->sourceCreated(uuid); }, Qt::QueuedConnection);
}

void MixerDock::onSourceRemove(void *data, calldata_t *cd)
{
	auto *dock = static_cast<MixerDock *>(data);
	obs_source_t *source = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));
	if (!source)
		return;
	const QString uuid = QString::fromUtf8(obs_source_get_uuid(source));
	QMetaObject::invokeMethod(dock, [dock, uuid]() { dock->sourceRemoved(uuid); }, Qt::QueuedConnection);
}

// ============================================================================
// Persistence
// ============================================================================

void MixerDock::saveTo(obs_data_t *moduleData)
{
	// Between a collection switch and its first load the model still holds
	// the previous collection's tabs; write back only what was loaded.
	QString json;
	if (m_hasPendingLoad)
		json = m_pendingJson;
	else
		json = serialiseLayout();

	if (json.isEmpty())
		return;

	// A string, never an object: obs_data silently drops arrays of plain
	// values, which is exactly what a tab's UUID list is.
	obs_data_t *blob = obs_data_create();
	obs_data_set_int(blob, "version", kFormatVersion);
	obs_data_set_string(blob, "layout", json.toUtf8().constData());
	obs_data_set_obj(moduleData, kModuleKey, blob);
	obs_data_release(blob);
}

void MixerDock::loadFrom(obs_data_t *moduleData)
{
	m_hasPendingLoad = true;
	m_pendingJson.clear();

	obs_data_t *blob = obs_data_get_obj(moduleData, kModuleKey);
	if (!blob)
		return;
	m_pendingJson = QString::fromUtf8(obs_data_get_string(blob, "layout"));
	obs_data_release(blob);
}

QString MixerDock::serialiseLayout() const
{
	QJsonArray tabs;
	for (const MixerTab &tab : m_tabs) {
		QJsonObject obj;
		if (tab.unassigned) {
			obj["unassigned"] = true;
		} else {
			obj["name"] = tab.name;
			obj["sources"] = QJsonArray::fromStringList(tab.sources);
		}
		if (tab.inactive != TabOverride::FollowMixer)
			obj["showInactive"] = overrideKey(tab.inactive);
		if (tab.hidden != TabOverride::FollowMixer)
			obj["showHidden"] = overrideKey(tab.hidden);
		tabs.append(obj);
	}

	QJsonObject root;
	root["version"] = kFormatVersion;
	root["vertical"] = m_vertical;
	root["current"] = m_current;
	root["tabs"] = tabs;
	return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool MixerDock::parseLayout(const QString &json)
{
	if (json.isEmpty())
		return false;
	QJsonParseError error{};
	const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
	if (error.error != QJsonParseError::NoError || !doc.isObject()) {
		blog(LOG_WARNING, "[StreamUP] [Mixer] Saved layout could not be read (%s), starting fresh",
		     error.errorString().toUtf8().constData());
		return false;
	}

	const QJsonObject root = doc.object();
	QVector<MixerTab> tabs;
	bool haveUnassigned = false;
	for (const QJsonValue value : root["tabs"].toArray()) {
		const QJsonObject obj = value.toObject();
		MixerTab tab;
		if (obj["unassigned"].toBool()) {
			if (haveUnassigned)
				continue;
			tab.unassigned = true;
			haveUnassigned = true;
		} else {
			tab.name = obj["name"].toString().trimmed();
			if (tab.name.isEmpty())
				tab.name = T("Mixer.Tab.DefaultName");
			for (const QJsonValue uuid : obj["sources"].toArray()) {
				const QString id = uuid.toString();
				if (!id.isEmpty() && !tab.sources.contains(id))
					tab.sources.append(id);
			}
		}
		tab.inactive = overrideFromKey(obj["showInactive"].toString());
		tab.hidden = overrideFromKey(obj["showHidden"].toString());
		tabs.append(tab);
	}
	if (tabs.isEmpty())
		return false;

	m_tabs = tabs;
	m_vertical = root["vertical"].toBool(true);
	m_current = root["current"].toInt(0);
	ensureUnassignedTab();
	return true;
}

void MixerDock::ensureUnassignedTab()
{
	if (unassignedIndex() < 0) {
		MixerTab tab;
		tab.unassigned = true;
		m_tabs.append(tab);
	}
	m_current = std::clamp(m_current, 0, int(m_tabs.size()) - 1);
}

void MixerDock::makeDefaultLayout()
{
	// Start with everything OBS's own mixer would show, in its order, so
	// switching to this dock loses nothing. Sources added later land in
	// Unassigned until they are given a tab.
	m_tabs.clear();
	MixerTab main;
	main.name = T("Mixer.Tab.Main");
	main.sources = allAudioSources();
	m_tabs.append(main);
	ensureUnassignedTab();
	m_current = 0;
	m_vertical = true;
}

void MixerDock::applyPendingLoad()
{
	if (m_hasPendingLoad) {
		m_hasPendingLoad = false;
		const QString json = m_pendingJson;
		m_pendingJson.clear();
		if (!parseLayout(json))
			makeDefaultLayout();
	} else if (m_tabs.isEmpty()) {
		makeDefaultLayout();
	}

	pruneMissing();
	StreamUP::DebugLogger::LogDebug("Mixer", "Load",
					QString("Mixer layout ready: %1 tabs").arg(m_tabs.size()).toUtf8().constData());
	rebuildTabBar();
	rebuildStrips();
}

void MixerDock::pruneMissing()
{
	for (MixerTab &tab : m_tabs) {
		QStringList kept;
		for (const QString &uuid : tab.sources) {
			obs_source_t *source = obs_get_source_by_uuid(uuid.toUtf8().constData());
			if (source && isAudioSource(source))
				kept.append(uuid);
			obs_source_release(source);
		}
		tab.sources = kept;
	}
}

// ============================================================================
// Model queries
// ============================================================================

QStringList MixerDock::allAudioSources() const
{
	QStringList ordered;

	// OBS's global audio devices first (Desktop Audio, Mic/Aux and friends),
	// then every other audio source by name, as the native mixer lists them.
	for (uint32_t channel = 1; channel <= 6; channel++) {
		obs_source_t *source = obs_get_output_source(channel);
		if (isAudioSource(source))
			ordered.append(QString::fromUtf8(obs_source_get_uuid(source)));
		obs_source_release(source);
	}

	struct Entry {
		QString name;
		QString uuid;
	};
	QVector<Entry> rest;
	auto collect = [](void *param, obs_source_t *source) -> bool {
		if (isAudioSource(source)) {
			auto *list = static_cast<QVector<Entry> *>(param);
			list->append({QString::fromUtf8(obs_source_get_name(source)),
				      QString::fromUtf8(obs_source_get_uuid(source))});
		}
		return true;
	};
	obs_enum_sources(collect, &rest);
	std::sort(rest.begin(), rest.end(),
		  [](const Entry &a, const Entry &b) { return a.name.compare(b.name, Qt::CaseInsensitive) < 0; });
	for (const Entry &e : rest) {
		if (!ordered.contains(e.uuid))
			ordered.append(e.uuid);
	}
	return ordered;
}

QStringList MixerDock::unassignedSources() const
{
	QSet<QString> assigned;
	for (const MixerTab &tab : m_tabs) {
		for (const QString &uuid : tab.sources)
			assigned.insert(uuid);
	}
	QStringList out;
	for (const QString &uuid : allAudioSources()) {
		if (!assigned.contains(uuid))
			out.append(uuid);
	}
	return out;
}

QStringList MixerDock::sourcesForTab(int index) const
{
	if (index < 0 || index >= m_tabs.size())
		return {};
	return m_tabs[index].unassigned ? unassignedSources() : m_tabs[index].sources;
}

int MixerDock::unassignedIndex() const
{
	for (int i = 0; i < m_tabs.size(); i++) {
		if (m_tabs[i].unassigned)
			return i;
	}
	return -1;
}

bool MixerDock::isRegularTab(int index) const
{
	return index >= 0 && index < m_tabs.size() && !m_tabs[index].unassigned;
}

// ============================================================================
// Edits
// ============================================================================

void MixerDock::layoutChanged(bool rebuild)
{
	if (rebuild) {
		updateTabTexts();
		syncStrips();
	}
	scheduleSave();
}

void MixerDock::addTab(const QString &name)
{
	MixerTab tab;
	tab.name = name.trimmed().isEmpty() ? T("Mixer.Tab.DefaultName") : name.trimmed();
	// New tabs go in before Unassigned when it sits at the end, which is
	// where people expect a new tab to appear.
	int at = int(m_tabs.size());
	if (unassignedIndex() == at - 1)
		at--;
	m_tabs.insert(at, tab);
	m_current = at;
	rebuildTabBar();
	layoutChanged();
}

void MixerDock::renameTab(int index, const QString &name)
{
	if (!isRegularTab(index) || name.trimmed().isEmpty())
		return;
	m_tabs[index].name = name.trimmed();
	layoutChanged();
}

void MixerDock::deleteTab(int index)
{
	if (!isRegularTab(index))
		return;
	m_tabs.removeAt(index);
	if (m_current >= m_tabs.size() || m_current > index)
		m_current = std::max(0, m_current - 1);
	rebuildTabBar();
	layoutChanged();
}

void MixerDock::addToTab(int index, const QString &uuid, int position)
{
	if (!isRegularTab(index))
		return;
	QStringList &list = m_tabs[index].sources;
	const int existing = int(list.indexOf(uuid));
	if (existing >= 0) {
		list.removeAt(existing);
		if (position > existing)
			position--;
	}
	if (position < 0 || position > list.size())
		position = int(list.size());
	list.insert(position, uuid);
}

void MixerDock::removeFromTab(int index, const QString &uuid)
{
	if (isRegularTab(index))
		m_tabs[index].sources.removeAll(uuid);
}

void MixerDock::moveStrip(const QString &uuid, int fromTab, int toTab, int position, bool copy)
{
	if (toTab < 0 || toTab >= m_tabs.size())
		return;

	if (m_tabs[toTab].unassigned) {
		// Dropping into Unassigned takes the source out of the tab it came from.
		if (isRegularTab(fromTab))
			removeFromTab(fromTab, uuid);
	} else {
		addToTab(toTab, uuid, position);
		if (!copy && fromTab != toTab && isRegularTab(fromTab))
			removeFromTab(fromTab, uuid);
	}
	layoutChanged();
}

void MixerDock::sourceRemoved(const QString &uuid)
{
	if (m_frozen)
		return;
	bool changed = false;
	for (MixerTab &tab : m_tabs)
		changed |= tab.sources.removeAll(uuid) > 0;

	bool shown = false;
	for (MixerStrip *strip : m_strips)
		shown |= strip->uuid() == uuid;

	// The strip holds a reference to the source: let it go now, shown or
	// parked.
	if (MixerStrip *strip = m_pool.take(uuid)) {
		m_strips.removeAll(strip);
		retireStrip(strip);
	}

	// The strip holds a reference to the source, so it must go now.
	if (shown || changed || !isRegularTab(m_current))
		layoutChanged(true);
	else
		updateTabTexts();
}

void MixerDock::sourceCreated(const QString &)
{
	if (m_frozen)
		return;
	// A new source lands in Unassigned, so only its count and view change.
	updateTabTexts();
	if (!isRegularTab(m_current))
		syncStrips();
}

// ============================================================================
// UI
// ============================================================================

void MixerDock::buildUi()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);

	m_tabBar = new MixerTabBar(this);
	root->addWidget(m_tabBar);

	// stackedMixerArea > [vh]MixerScrollArea > [vh]VolumeWidgets, as OBS's
	// mixer nests them.
	m_stack = new QStackedWidget(this);
	m_stack->setObjectName("stackedMixerArea");

	m_scroll = new QScrollArea(m_stack);
	m_scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_scroll->setWidgetResizable(true);

	m_area = new StripArea();
	m_scroll->setWidget(m_area);
	m_stack->addWidget(m_scroll);
	root->addWidget(m_stack, 1);

	m_empty = new QLabel(m_area);
	m_empty->setProperty("class", "text-muted");
	m_empty->setAlignment(Qt::AlignCenter);
	m_empty->setWordWrap(true);
	m_empty->setMargin(S(12));
	m_empty->hide();

	// The same toolbar as OBS's mixer: hidden count, layout, advanced audio
	// properties and the Options menu, plus this dock's own "add sources".
	auto *toolbar = new QToolBar(this);
	m_toolbar = toolbar;
	toolbar->setObjectName("StreamUPMixerToolbar");
	toolbar->setMovable(false);
	toolbar->setFloatable(false);
	toolbar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
	toolbar->setIconSize(QSize(16, 16));

	m_addButton = new QToolButton(toolbar);
	m_addButton->setObjectName("StreamUPMixerAddButton");
	m_addButton->setProperty("themeID", "addIconSmall");
	m_addButton->setProperty("class", "icon-plus");
	m_addButton->setToolTip(T("Mixer.Tooltip.AddSources"));
	toolbar->addWidget(m_addButton);
	toolbar->addSeparator();

	m_hiddenButton = new QPushButton(toolbar);
	m_hiddenButton->setCheckable(true);
	m_hiddenButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
	StyleClass::add(m_hiddenButton, "toolbar-button");
	StyleClass::add(m_hiddenButton, "toggle-hidden");
	toolbar->addWidget(m_hiddenButton);

	toolbar->addSeparator();
	auto *spacer = new QWidget(toolbar);
	spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
	toolbar->addWidget(spacer);
	toolbar->addSeparator();

	m_layoutAction = new QAction(this);
	m_layoutAction->setObjectName("actionStreamUPMixerToggleLayout");
	toolbar->addAction(m_layoutAction);
	toolbar->addSeparator();

	m_advAudioAction = new QAction(this);
	m_advAudioAction->setText(obsText("Basic.AdvAudio"));
	m_advAudioAction->setToolTip(obsText("Basic.AdvAudio"));
	m_advAudioAction->setIcon(QIcon(QStringLiteral(":/settings/images/settings/advanced.svg")));
	toolbar->addAction(m_advAudioAction);
	toolbar->addSeparator();
	if (QWidget *w = toolbar->widgetForAction(m_advAudioAction))
		StyleClass::add(w, "icon-cogs");

	m_optionsButton = new QPushButton(obsText("Basic.AudioMixer.Options"), toolbar);
	m_optionsButton->setObjectName("StreamUPMixerOptionsButton");
	StyleClass::add(m_optionsButton, "toolbar-button");
	StyleClass::add(m_optionsButton, "text-bold");
	// A menu so the button draws OBS's drop-down arrow; it is rebuilt each
	// time it opens so it always shows the current settings.
	auto *optionsMenu = new QMenu(m_optionsButton);
	optionsMenu->setObjectName("StreamUPMixerOptionsMenu");
	connect(optionsMenu, &QMenu::aboutToShow, this, [this, optionsMenu]() {
		optionsMenu->clear();
		buildMixerMenu(optionsMenu);
	});
	m_optionsButton->setMenu(optionsMenu);
	toolbar->addWidget(m_optionsButton);
	root->addWidget(toolbar);

	connect(m_addButton, &QToolButton::clicked, this, &MixerDock::showAddMenu);
	connect(m_hiddenButton, &QPushButton::clicked, this,
		[this](bool checked) { setObsMixerSetting("MixerShowHidden", checked); });
	connect(m_layoutAction, &QAction::triggered, this, [this]() { setVerticalStrips(!m_vertical); });
	connect(m_advAudioAction, &QAction::triggered, this, &MixerDock::openAdvancedAudio);

	// Right-clicking empty mixer space opens the same menu, as in OBS.
	m_scroll->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_scroll, &QWidget::customContextMenuRequested, this,
		[this](const QPoint &pos) { showMixerMenu(m_scroll->mapToGlobal(pos)); });
	m_area->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_area, &QWidget::customContextMenuRequested, this,
		[this](const QPoint &pos) { showMixerMenu(m_area->mapToGlobal(pos)); });
	updateToolbarState();

	connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
		if (m_updatingTabBar || index < 0)
			return;
		m_current = index;
		syncStrips();
		scheduleSave();
	});
	connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
		if (m_updatingTabBar || from < 0 || to < 0 || from >= m_tabs.size() || to >= m_tabs.size())
			return;
		m_tabs.move(from, to);
		m_current = m_tabBar->currentIndex();
		scheduleSave();
	});
	connect(m_tabBar, &QTabBar::tabBarDoubleClicked, this, [this](int index) {
		if (index < 0)
			promptNewTab();
		else if (isRegularTab(index))
			promptRenameTab(index);
	});
	connect(m_tabBar, &QWidget::customContextMenuRequested, this, &MixerDock::showTabMenu);
	connect(m_tabBar, &MixerTabBar::stripDroppedOnTab, this,
		[this](int tab, const QString &uuid, int fromTab, bool copy) {
			moveStrip(uuid, fromTab, tab, -1, copy);
		});
	connect(m_area, &StripArea::stripDropped, this, [this](int index, const QString &uuid, int fromTab, bool copy) {
		moveStrip(uuid, fromTab, m_current, modelPositionForDisplayIndex(index), copy);
	});
}

void MixerDock::rebuildTabBar()
{
	m_updatingTabBar = true;
	while (m_tabBar->count() > 0)
		m_tabBar->removeTab(0);
	for (int i = 0; i < m_tabs.size(); i++)
		m_tabBar->addTab(QString());
	m_current = std::clamp(m_current, 0, std::max(0, int(m_tabs.size()) - 1));
	m_tabBar->setCurrentIndex(m_current);
	m_updatingTabBar = false;
	updateTabTexts();
}

void MixerDock::updateTabTexts()
{
	const int unassigned = int(unassignedSources().size());
	for (int i = 0; i < m_tabs.size() && i < m_tabBar->count(); i++) {
		if (m_tabs[i].unassigned) {
			m_tabBar->setTabText(i, T("Mixer.Tab.Unassigned").arg(unassigned));
			m_tabBar->setTabToolTip(i, T("Mixer.Tooltip.UnassignedTab"));
		} else {
			m_tabBar->setTabText(i, m_tabs[i].name);
			m_tabBar->setTabToolTip(i, QString());
		}
	}
}

void MixerDock::retireStrip(MixerStrip *strip)
{
	m_area->stripLayout()->removeWidget(strip);
	if (m_deferDeletes > 0) {
		strip->hide();
		m_graveyard.append(strip);
	} else {
		delete strip;
	}
}

void MixerDock::clearStrips()
{
	for (MixerStrip *strip : std::as_const(m_pool))
		retireStrip(strip);
	m_pool.clear();
	m_strips.clear();
	m_area->setStrips(m_strips);
}

MixerStrip *MixerDock::createStrip(const QString &uuid)
{
	MixerStrip *strip = nullptr;
	obs_source_t *source = obs_get_source_by_uuid(uuid.toUtf8().constData());
	if (isAudioSource(source)) {
		strip = new MixerStrip(source, m_vertical, m_area);
		connect(strip, &MixerStrip::dragRequested, this, &MixerDock::startStripDrag);
		connect(strip, &MixerStrip::menuRequested, this, &MixerDock::showStripMenu);
		connect(strip, &MixerStrip::mixerStateChanged, this, [this]() { scheduleVisibilityUpdate(); });
		strip->setPreviewed(m_previewSources.contains(uuid));
		m_pool.insert(uuid, strip);
	}
	obs_source_release(source);
	return strip;
}

void MixerDock::layoutStrips()
{
	// Re-seat the strips in model order. They already belong to the strip
	// area, so this only moves them: nothing is re-created or re-styled.
	QBoxLayout *layout = m_area->stripLayout();
	while (layout->count() > 0)
		delete layout->takeAt(0);
	for (MixerStrip *strip : m_strips) {
		layout->addWidget(strip);
		strip->show();
	}
	m_area->setStrips(m_strips);

	if (m_strips.isEmpty()) {
		const bool filtered = !sourcesForTab(m_current).isEmpty();
		m_empty->setText(T(filtered			  ? "Mixer.Empty.Filtered"
				   : isRegularTab(m_current) ? "Mixer.Empty.Tab"
							     : "Mixer.Empty.Unassigned"));
		layout->addWidget(m_empty, 1);
		m_empty->show();
	} else {
		m_empty->hide();
	}
	m_addButton->setEnabled(isRegularTab(m_current));
}

void MixerDock::syncStrips()
{
	if (m_frozen || m_tabs.isEmpty()) {
		rebuildStrips();
		return;
	}

	// Every source of the tab gets a strip (it is what tracks the source's
	// state), then OBS's visibility rule decides which of them show.
	QList<MixerStrip *> tabStrips;
	for (const QString &uuid : sourcesForTab(m_current)) {
		if (MixerStrip *strip = m_pool.value(uuid))
			tabStrips.append(strip);
		else if (MixerStrip *strip = createStrip(uuid))
			tabStrips.append(strip);
	}

	struct Ranked {
		MixerStrip *strip;
		int weight;
	};
	const bool keepHiddenLast = obsMixerSetting("MixerKeepHiddenLast");
	const bool keepInactiveLast = obsMixerSetting("MixerKeepInactiveLast");
	QVector<Ranked> ranked;
	int hiddenCount = 0;
	for (MixerStrip *strip : tabStrips) {
		if (strip->isHiddenInMixer())
			hiddenCount++;
		if (!stripVisible(strip))
			continue;
		// The tab's own order, with OBS's "keep hidden / inactive last"
		// options moving those to the end (previewed ones just ahead of the
		// rest), as OBS weights them.
		int weight = 0;
		if (strip->isHiddenInMixer() && keepHiddenLast)
			weight += strip->isPreviewed() ? 4 : 5;
		if (!strip->isActive() && keepInactiveLast)
			weight += strip->isPreviewed() ? 4 : 5;
		ranked.append({strip, weight});
	}
	std::stable_sort(ranked.begin(), ranked.end(),
			 [](const Ranked &a, const Ranked &b) { return a.weight < b.weight; });

	QList<MixerStrip *> next;
	for (const Ranked &r : ranked)
		next.append(r.strip);

	m_hiddenCount = hiddenCount;
	updateToolbarState();

	// Nothing to move: leave the layout alone so a state change costs nothing.
	if (next == m_strips && (next.isEmpty() == !m_empty->isHidden()))
		return;

	// Strips not shown are parked: out of the layout and hidden (a hidden
	// meter does not repaint), ready for when they are next shown.
	for (MixerStrip *strip : std::as_const(m_strips)) {
		if (!next.contains(strip)) {
			m_area->stripLayout()->removeWidget(strip);
			strip->hide();
		}
	}
	for (MixerStrip *strip : std::as_const(m_pool)) {
		if (!next.contains(strip) && strip->isVisible())
			strip->hide();
	}

	m_strips = next;
	layoutStrips();
}

int MixerDock::modelPositionForDisplayIndex(int displayIndex) const
{
	if (!isRegularTab(m_current))
		return -1;
	const QStringList &list = m_tabs[m_current].sources;
	if (displayIndex >= 0 && displayIndex < m_strips.size())
		return int(list.indexOf(m_strips[displayIndex]->uuid()));
	if (!m_strips.isEmpty())
		return int(list.indexOf(m_strips.last()->uuid())) + 1;
	return -1;
}

void MixerDock::flushGraveyard()
{
	if (m_deferDeletes > 0)
		return;
	for (const QPointer<MixerStrip> &strip : m_graveyard)
		delete strip.data();
	m_graveyard.clear();
}

void MixerDock::rebuildStrips()
{
	clearStrips();

	QBoxLayout *layout = m_area->stripLayout();
	// Drop the old trailing stretch and empty-state label.
	while (layout->count() > 0) {
		QLayoutItem *item = layout->takeAt(0);
		delete item;
	}
	m_area->setVertical(m_vertical);
	m_scroll->setObjectName(m_vertical ? "vMixerScrollArea" : "hMixerScrollArea");
	m_scroll->setHorizontalScrollBarPolicy(m_vertical ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
	m_scroll->setVerticalScrollBarPolicy(m_vertical ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
	// Object names changed, so the theme's rules need applying again.
	for (QWidget *w : {static_cast<QWidget *>(m_scroll), static_cast<QWidget *>(m_area)}) {
		w->style()->unpolish(w);
		w->style()->polish(w);
	}

	if (m_frozen || m_tabs.isEmpty()) {
		m_empty->hide();
		return;
	}

	updatePreviewFlags();
	m_empty->hide();
	syncStrips();
	if (m_strips.isEmpty())
		layoutStrips();
}

void MixerDock::startStripDrag(MixerStrip *strip)
{
	if (!strip || m_dragActive)
		return;

	// Parented to the dock, not the strip: the strip can be replaced while the
	// drag is running (dropping it rebuilds the tab).
	auto *drag = new QDrag(this);
	drag->setMimeData(encodeDrag(strip->uuid(), m_current));
	const QPixmap pixmap = strip->grab();
	drag->setPixmap(pixmap);
	drag->setHotSpot(strip->mapFromGlobal(QCursor::pos()));

	m_dragActive = true;
	m_deferDeletes++;
	drag->exec(Qt::MoveAction | Qt::CopyAction, Qt::MoveAction);
	m_deferDeletes--;
	m_dragActive = false;

	// The strip that started this is still on the call stack; delete any
	// replaced strips once control is back in the main event loop.
	QTimer::singleShot(0, this, [this]() { flushGraveyard(); });
}

void MixerDock::showStripMenu(MixerStrip *strip, const QPoint &globalPos)
{
	if (!strip)
		return;
	const QString uuid = strip->uuid();
	const int current = m_current;
	const bool regular = isRegularTab(current);

	QMenu menu(this);
	menu.setObjectName("StreamUPMixerStripMenu");

	// OBS's own channel menu first, in its order and wording.
	const bool global = strip->isGlobal();
	const bool pinned = strip->isPinnedInMixer();
	const bool hidden = strip->isHiddenInMixer();

	connect(menu.addAction(obsText("UnhideAll")), &QAction::triggered, this, &MixerDock::unhideAll);
	menu.addSeparator();
	QAction *pin = menu.addAction(obsText(pinned ? "Basic.AudioMixer.Unpin" : "Basic.AudioMixer.Pin"));
	pin->setEnabled(!global);
	connect(pin, &QAction::triggered, this, [this, uuid, pinned]() { setPinned(uuid, !pinned); });
	QAction *hide = menu.addAction(
		obsText(hidden && !global ? "Basic.AudioMixer.Unhide" : "Basic.AudioMixer.Hide"));
	hide->setEnabled(!global && !pinned);
	connect(hide, &QAction::triggered, this, [this, uuid, hidden]() { setHidden(uuid, !hidden); });
	QAction *lock = menu.addAction(obsText("LockVolume"));
	lock->setCheckable(true);
	lock->setChecked(strip->isVolumeLocked());
	connect(lock, &QAction::toggled, this, [this, uuid](bool checked) { setLocked(uuid, checked); });

	menu.addSeparator();
	QAction *copyFilters = menu.addAction(obsText("Copy.Filters"));
	copyFilters->setEnabled(obs_source_filter_count(strip->source()) > 0);
	connect(copyFilters, &QAction::triggered, this,
		[uuid]() { triggerNativeSourceAction(uuid, SLOT(actionCopyFilters())); });
	QAction *pasteFilters = menu.addAction(obsText("Paste.Filters"));
	pasteFilters->setEnabled(nativePasteFiltersAvailable());
	connect(pasteFilters, &QAction::triggered, this,
		[uuid]() { triggerNativeSourceAction(uuid, SLOT(actionPasteFilters())); });

	menu.addSeparator();
	connect(menu.addAction(obsText("Rename")), &QAction::triggered, this, [this, uuid]() {
		// Deferred, as OBS does, so the dialog does not open while the menu
		// is still closing.
		QTimer::singleShot(0, this, [this, uuid]() { renameSource(uuid); });
	});

	menu.addSeparator();
	connect(menu.addAction(obsText("Filters")), &QAction::triggered, this, [uuid]() {
		obs_source_t *source = obs_get_source_by_uuid(uuid.toUtf8().constData());
		if (source)
			obs_frontend_open_source_filters(source);
		obs_source_release(source);
	});
	connect(menu.addAction(obsText("Properties")), &QAction::triggered, this, [uuid]() {
		obs_source_t *source = obs_get_source_by_uuid(uuid.toUtf8().constData());
		if (source)
			obs_frontend_open_source_properties(source);
		obs_source_release(source);
	});

	// Then this dock's own: arranging the strip and its tabs.
	menu.addSeparator();
	if (regular) {
		const int at = int(m_strips.indexOf(strip));
		const int last = int(m_strips.size()) - 1;
		QAction *earlier = menu.addAction(T(m_vertical ? "Mixer.Action.MoveLeft" : "Mixer.Action.MoveUp"));
		earlier->setEnabled(at > 0);
		connect(earlier, &QAction::triggered, this, [this, uuid, current, at]() {
			addToTab(current, uuid, modelPositionForDisplayIndex(at - 1));
			layoutChanged();
		});
		QAction *later = menu.addAction(T(m_vertical ? "Mixer.Action.MoveRight" : "Mixer.Action.MoveDown"));
		later->setEnabled(at >= 0 && at < last);
		connect(later, &QAction::triggered, this, [this, uuid, current, at]() {
			addToTab(current, uuid, modelPositionForDisplayIndex(at + 2));
			layoutChanged();
		});
	}

	// Other tabs this source could go to.
	QMenu *moveMenu = menu.addMenu(T(regular ? "Mixer.Action.MoveTo" : "Mixer.Action.AddTo"));
	for (int i = 0; i < m_tabs.size(); i++) {
		if (i == current || !isRegularTab(i))
			continue;
		QAction *a = moveMenu->addAction(m_tabs[i].name);
		connect(a, &QAction::triggered, this,
			[this, uuid, current, i]() { moveStrip(uuid, current, i, -1, false); });
	}
	if (!moveMenu->isEmpty())
		moveMenu->addSeparator();
	connect(moveMenu->addAction(T("Mixer.Action.NewTabEllipsis")), &QAction::triggered, this,
		[this, uuid, current]() { promptNewTab(uuid, current); });

	if (regular) {
		QMenu *alsoMenu = menu.addMenu(T("Mixer.Action.AlsoShowIn"));
		for (int i = 0; i < m_tabs.size(); i++) {
			if (i == current || !isRegularTab(i) || m_tabs[i].sources.contains(uuid))
				continue;
			QAction *a = alsoMenu->addAction(m_tabs[i].name);
			connect(a, &QAction::triggered, this,
				[this, uuid, current, i]() { moveStrip(uuid, current, i, -1, true); });
		}
		alsoMenu->setEnabled(!alsoMenu->isEmpty());

		connect(menu.addAction(T("Mixer.Action.RemoveFromTab")), &QAction::triggered, this,
			[this, uuid, current]() {
				removeFromTab(current, uuid);
				layoutChanged();
			});
	}


	// The strip asking for this menu is on the call stack, and an action can
	// rebuild the tab, so hold its deletion until the menu has returned.
	m_deferDeletes++;
	menu.exec(globalPos);
	m_deferDeletes--;
	QTimer::singleShot(0, this, [this]() { flushGraveyard(); });
}

void MixerDock::showTabMenu(const QPoint &pos)
{
	const int index = m_tabBar->tabAt(pos);
	QMenu menu(this);
	menu.setObjectName("StreamUPMixerTabMenu");

	connect(menu.addAction(T("Mixer.Action.NewTabEllipsis")), &QAction::triggered, this,
		[this]() { promptNewTab(); });

	if (isRegularTab(index)) {
		connect(menu.addAction(T("Mixer.Action.RenameTab")), &QAction::triggered, this,
			[this, index]() { promptRenameTab(index); });
		connect(menu.addAction(T("Mixer.Action.DeleteTab")), &QAction::triggered, this,
			[this, index]() { confirmDeleteTab(index); });
	}

	if (index >= 0) {
		menu.addSeparator();
		addTabOverrideMenu(&menu, index);
		menu.addSeparator();
		connect(menu.addAction(T("Mixer.Action.HideTabInObsMixer")), &QAction::triggered, this,
			[this, index]() { setTabHiddenInNativeMixer(index, true); });
		connect(menu.addAction(T("Mixer.Action.ShowTabInObsMixer")), &QAction::triggered, this,
			[this, index]() { setTabHiddenInNativeMixer(index, false); });
	}

	menu.exec(m_tabBar->mapToGlobal(pos));
}

void MixerDock::showOptionsMenu()
{
	m_optionsButton->showMenu();
}

// OBS's mixer Options menu, item for item, then this dock's tab options.
void MixerDock::buildMixerMenu(QMenu *menu)
{
	const int current = m_current;
	const bool vertical = m_vertical;

	connect(menu->addAction(obsText("UnhideAll")), &QAction::triggered, this, &MixerDock::unhideAll);
	menu->addSeparator();

	auto addSetting = [this, menu](const char *textKey, const char *setting) {
		QAction *a = menu->addAction(obsText(textKey));
		a->setCheckable(true);
		a->setChecked(obsMixerSetting(setting));
		connect(a, &QAction::toggled, this, [this, setting](bool on) { setObsMixerSetting(setting, on); });
	};
	addSetting("Basic.AudioMixer.ShowHidden", "MixerShowHidden");
	addSetting("Basic.AudioMixer.ShowInactive", "MixerShowInactive");
	addSetting(vertical ? "Basic.AudioMixer.KeepHiddenRight" : "Basic.AudioMixer.KeepHiddenBottom",
		   "MixerKeepHiddenLast");
	addSetting(vertical ? "Basic.AudioMixer.KeepInactiveRight" : "Basic.AudioMixer.KeepInactiveBottom",
		   "MixerKeepInactiveLast");
	menu->addSeparator();

	connect(menu->addAction(obsText(vertical ? "Basic.AudioMixer.Layout.Horizontal"
						 : "Basic.AudioMixer.Layout.Vertical")),
		&QAction::triggered, this, [this]() { setVerticalStrips(!m_vertical); });
	menu->addSeparator();
	connect(menu->addAction(obsText("Basic.AdvAudio")), &QAction::triggered, this, &MixerDock::openAdvancedAudio);

	// This dock's own.
	menu->addSeparator();
	QAction *add = menu->addAction(T("Mixer.Action.AddSourcesEllipsis"));
	add->setEnabled(isRegularTab(current));
	connect(add, &QAction::triggered, this, [this]() { QTimer::singleShot(0, this, &MixerDock::showAddMenu); });
	connect(menu->addAction(T("Mixer.Action.NewTabEllipsis")), &QAction::triggered, this,
		[this]() { promptNewTab(); });
	QAction *rename = menu->addAction(T("Mixer.Action.RenameTab"));
	rename->setEnabled(isRegularTab(current));
	connect(rename, &QAction::triggered, this, [this, current]() { promptRenameTab(current); });
	QAction *del = menu->addAction(T("Mixer.Action.DeleteTab"));
	del->setEnabled(isRegularTab(current));
	connect(del, &QAction::triggered, this, [this, current]() { confirmDeleteTab(current); });
	menu->addSeparator();
	addTabOverrideMenu(menu, current);
	menu->addSeparator();
	connect(menu->addAction(T("Mixer.Action.HideTabInObsMixer")), &QAction::triggered, this,
		[this, current]() { setTabHiddenInNativeMixer(current, true); });
	connect(menu->addAction(T("Mixer.Action.ShowTabInObsMixer")), &QAction::triggered, this,
		[this, current]() { setTabHiddenInNativeMixer(current, false); });
}

void MixerDock::showMixerMenu(const QPoint &globalPos)
{
	QMenu menu(this);
	menu.setObjectName("StreamUPMixerOptionsMenu");
	buildMixerMenu(&menu);
	menu.exec(globalPos);
}

// "This tab: inactive sources" and "hidden sources", each following OBS's
// mixer setting or overriding it for the one tab.
void MixerDock::addTabOverrideMenu(QMenu *menu, int tabIndex)
{
	if (tabIndex < 0 || tabIndex >= m_tabs.size())
		return;
	auto addOne = [this, menu, tabIndex](const char *titleKey, bool inactive) {
		QMenu *sub = menu->addMenu(T(titleKey));
		const TabOverride currentValue = inactive ? m_tabs[tabIndex].inactive : m_tabs[tabIndex].hidden;
		auto *group = new QActionGroup(sub);
		const struct {
			TabOverride value;
			const char *key;
		} options[] = {{TabOverride::FollowMixer, "Mixer.Override.Follow"},
			       {TabOverride::Show, "Mixer.Override.Show"},
			       {TabOverride::Hide, "Mixer.Override.Hide"}};
		for (const auto &option : options) {
			QAction *a = sub->addAction(T(option.key));
			a->setCheckable(true);
			a->setChecked(option.value == currentValue);
			group->addAction(a);
			const TabOverride value = option.value;
			connect(a, &QAction::triggered, this, [this, tabIndex, inactive, value]() {
				if (tabIndex >= m_tabs.size())
					return;
				(inactive ? m_tabs[tabIndex].inactive : m_tabs[tabIndex].hidden) = value;
				scheduleVisibilityUpdate();
				scheduleSave();
			});
		}
	};
	addOne("Mixer.Override.InactiveTitle", true);
	addOne("Mixer.Override.HiddenTitle", false);
}

void MixerDock::updateToolbarState()
{
	if (!m_toolbar)
		return;
	config_t *config = obs_frontend_get_user_config();
	// OBS's "Show toolbars" setting hides its list toolbars, this one included.
	const bool showToolbar = !config || !config_has_user_value(config, "BasicWindow", "ShowListboxToolbars") ||
				 config_get_bool(config, "BasicWindow", "ShowListboxToolbars");
	m_toolbar->setVisible(showToolbar);

	const bool showHidden = obsMixerSetting("MixerShowHidden");
	m_hiddenButton->setText(obsText("Basic.AudioMixer.HiddenTotal").arg(m_hiddenCount));
	m_hiddenButton->setToolTip(obsText(showHidden ? "Basic.AudioMixer.HideHidden" : "Basic.AudioMixer.ShowHidden"));
	{
		const QSignalBlocker block(m_hiddenButton);
		m_hiddenButton->setChecked(showHidden);
	}
	m_hiddenButton->setDisabled(m_hiddenCount == 0);
	StyleClass::toggle(m_hiddenButton, "text-muted", m_hiddenCount == 0);

	m_layoutAction->setIcon(QIcon(m_vertical ? QStringLiteral(":/res/images/layout-horizontal.svg")
						 : QStringLiteral(":/res/images/layout-vertical.svg")));
	m_layoutAction->setToolTip(
		obsText(m_vertical ? "Basic.AudioMixer.Layout.Horizontal" : "Basic.AudioMixer.Layout.Vertical"));
	if (QWidget *w = m_toolbar->widgetForAction(m_layoutAction)) {
		// setIcon above puts the plain fallback back on the button on every
		// refresh, so the theme's qproperty-icon has to be applied again even
		// when the class did not change.
		const bool changed = StyleClass::toggle(w, "icon-layout-horizontal", m_vertical) |
				     StyleClass::toggle(w, "icon-layout-vertical", !m_vertical);
		if (!changed)
			StyleClass::repolish(w);
	}
}

// ---- Native mixer actions ----

void MixerDock::unhideAll()
{
	// As OBS's "Unhide All": every audio source, not only this tab's.
	for (const QString &uuid : allAudioSources()) {
		obs_source_t *source = obs_get_source_by_uuid(uuid.toUtf8().constData());
		obs_data_t *priv = source ? obs_source_get_private_settings(source) : nullptr;
		const bool hidden = priv && obs_data_get_bool(priv, "mixer_hidden");
		obs_data_release(priv);
		if (hidden)
			setMixerFlag(source, "mixer_hidden", false);
		obs_source_release(source);
	}
}

void MixerDock::setPinned(const QString &uuid, bool pinned)
{
	// Pinning unhides, as in OBS.
	if (pinned)
		setMixerFlag(uuid, "mixer_hidden", false);
	setMixerFlag(uuid, "mixer_pinned", pinned);
}

void MixerDock::setHidden(const QString &uuid, bool hidden)
{
	setMixerFlag(uuid, "mixer_hidden", hidden);
}

void MixerDock::setLocked(const QString &uuid, bool locked)
{
	setMixerFlag(uuid, "volume_locked", locked);
}

// OBS's mixer rename: its dialog text, its checks for empty and taken names.
void MixerDock::renameSource(const QString &uuid)
{
	auto *main = static_cast<QWidget *>(obs_frontend_get_main_window());
	for (;;) {
		obs_source_t *source = obs_get_source_by_uuid(uuid.toUtf8().constData());
		if (!source)
			return;
		const QString prevName = QString::fromUtf8(obs_source_get_name(source));
		bool accepted = false;
		const QString name = QInputDialog::getText(main, obsText("Basic.Main.MixerRename.Title"),
							   obsText("Basic.Main.MixerRename.Text"), QLineEdit::Normal,
							   prevName, &accepted)
					     .trimmed();
		if (!accepted || name == prevName) {
			obs_source_release(source);
			return;
		}
		if (name.isEmpty()) {
			obs_source_release(source);
			QMessageBox::warning(main, obsText("NoNameEntered.Title"), obsText("NoNameEntered.Text"));
			continue;
		}
		obs_source_t *existing = obs_get_source_by_name(name.toUtf8().constData());
		if (existing) {
			obs_source_release(existing);
			obs_source_release(source);
			QMessageBox::warning(main, obsText("NameExists.Title"), obsText("NameExists.Text"));
			continue;
		}
		obs_source_set_name(source, name.toUtf8().constData());
		obs_source_release(source);
		return;
	}
}

void MixerDock::openAdvancedAudio()
{
	// OBS's own Advanced Audio Properties window, through its menu action.
	QObject *main = static_cast<QObject *>(obs_frontend_get_main_window());
	if (main)
		QMetaObject::invokeMethod(main, "on_actionAdvAudioProperties_triggered", Qt::DirectConnection);
}

void MixerDock::showAddMenu()
{
	const int current = m_current;
	if (!isRegularTab(current))
		return;

	QMenu menu(this);
	menu.setObjectName("StreamUPMixerAddMenu");

	const QStringList unassigned = unassignedSources();
	QAction *all = menu.addAction(T("Mixer.Action.AddAllUnassigned"));
	all->setEnabled(!unassigned.isEmpty());
	connect(all, &QAction::triggered, this, [this, current, unassigned]() {
		for (const QString &uuid : unassigned)
			addToTab(current, uuid);
		layoutChanged();
	});
	menu.addSeparator();

	// Every audio source, ticked when it is already in this tab.
	for (const QString &uuid : allAudioSources()) {
		obs_source_t *source = obs_get_source_by_uuid(uuid.toUtf8().constData());
		if (!source)
			continue;
		QAction *a = menu.addAction(QString::fromUtf8(obs_source_get_name(source)));
		obs_source_release(source);
		a->setCheckable(true);
		a->setChecked(m_tabs[current].sources.contains(uuid));
		connect(a, &QAction::toggled, this, [this, current, uuid](bool checked) {
			if (checked)
				addToTab(current, uuid);
			else
				removeFromTab(current, uuid);
			layoutChanged();
		});
	}

	menu.exec(m_addButton->mapToGlobal(QPoint(0, m_addButton->height())));
}

void MixerDock::promptNewTab(const QString &uuidToMove, int fromTab)
{
	QPointer<MixerDock> self(this);
	StreamUP::UIStyles::prompt(
		this, T("Mixer.Dialog.NewTab.Title"), T("Mixer.Dialog.TabName"), T("Mixer.Tab.DefaultName"),
		[self, uuidToMove, fromTab](const QString &name) {
			if (!self || name.trimmed().isEmpty())
				return;
			self->addTab(name);
			if (!uuidToMove.isEmpty())
				self->moveStrip(uuidToMove, fromTab, self->m_current, -1, false);
		},
		T("Mixer.Dialog.Create"));
}

void MixerDock::promptRenameTab(int index)
{
	if (!isRegularTab(index))
		return;
	QPointer<MixerDock> self(this);
	StreamUP::UIStyles::prompt(
		this, T("Mixer.Dialog.RenameTab.Title"), T("Mixer.Dialog.TabName"), m_tabs[index].name,
		[self, index](const QString &name) {
			if (self)
				self->renameTab(index, name);
		},
		T("Mixer.Dialog.Save"));
}

void MixerDock::confirmDeleteTab(int index)
{
	if (!isRegularTab(index))
		return;
	QPointer<MixerDock> self(this);
	const QString name = m_tabs[index].name;
	StreamUP::UIStyles::confirm(this, T("Mixer.Dialog.DeleteTab.Title"), T("Mixer.Dialog.DeleteTab.Message").arg(name),
				    T("Mixer.Dialog.Delete"), "danger", [self, index, name]() {
					    // The tab list may have changed while the dialog was open.
					    if (self && self->isRegularTab(index) && self->m_tabs[index].name == name)
						    self->deleteTab(index);
				    });
}

void MixerDock::setTabHiddenInNativeMixer(int index, bool hidden)
{
	for (const QString &uuid : sourcesForTab(index))
		setMixerFlag(uuid, "mixer_hidden", hidden);
}

void MixerDock::setVerticalStrips(bool vertical)
{
	if (m_vertical == vertical)
		return;
	m_vertical = vertical;
	// Every strip lays itself out for one orientation, so this one rebuilds.
	updateTabTexts();
	rebuildStrips();
	updateToolbarState();
	scheduleSave();
}

void MixerDock::scheduleSave()
{
	if (!m_frozen)
		m_saveTimer.start();
}

void MixerDock::refreshTheme()
{
	// Fresh strips pick up every rule of the new theme, and the StreamUP
	// theme's mixer tweak only where it now applies, just as OBS's own
	// channels do.
	if (!m_frozen)
		rebuildStrips();
}



// ============================================================================
// Registration
// ============================================================================

// Puts the floating mixer over OBS whenever it opens somewhere else (another
// screen, or off every screen). The very first time it opens floating it is
// centred over OBS, wherever Qt would have put it.
class FloatingDockPlacer : public QObject {
public:
	explicit FloatingDockPlacer(QDockWidget *frame) : QObject(frame), m_frame(frame) {}

	bool eventFilter(QObject *obj, QEvent *e) override
	{
		if (obj == m_frame && e->type() == QEvent::Show && m_frame->isFloating()) {
			// After Qt has finished restoring the frame's own geometry.
			QPointer<QDockWidget> frame(m_frame);
			QTimer::singleShot(0, this, [frame]() {
				if (!frame || !frame->isFloating() || !frame->isVisible())
					return;
				char *marker = obs_module_config_path("mixer-dock-placed");
				const bool placedBefore = marker && os_file_exists(marker);
				StreamUP::UIHelpers::PlaceOverMainWindow(frame, /*onlyWhenElsewhere=*/placedBefore);
				if (marker && !placedBefore) {
					char *dir = obs_module_config_path("");
					if (dir)
						os_mkdirs(dir);
					bfree(dir);
					os_quick_write_utf8_file(marker, "1", 1, false);
				}
				bfree(marker);
			});
		}
		return false;
	}

private:
	QDockWidget *m_frame;
};

void RegisterMixerDock()
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main) {
		blog(LOG_ERROR, "[StreamUP] [Mixer] No main window, mixer dock not created");
		return;
	}

	obs_frontend_push_ui_translation(obs_module_get_string);
	auto *dock = new MixerDock(main);
	obs_frontend_add_dock_by_id(kDockId, obs_module_text("Mixer.Title"), dock);
	obs_frontend_pop_ui_translation();

	// OBS wraps the widget in its own dock frame. When that frame opens
	// floating, Qt and OBS's saved layout can put it on any monitor; it
	// belongs over OBS. Docked, it is left exactly where the user put it.
	if (auto *frame = qobject_cast<QDockWidget *>(dock->parentWidget()))
		frame->installEventFilter(new FloatingDockPlacer(frame));

	obs_frontend_add_save_callback(onSaveLoad, nullptr);
	blog(LOG_INFO, "[StreamUP] Mixer dock added");
}

} // namespace Mixer
} // namespace StreamUP
