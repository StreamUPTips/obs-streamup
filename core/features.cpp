#include "features.hpp"

#include "backup-manager.hpp"
#include "file-manager.hpp"
#include "plugin-manager.hpp"
#include "../multidock/multidock_dialogs.hpp"
#include "../multidock/multidock_manager.hpp"
#include "../ui/hotkey-manager.hpp"
#include "../ui/my-products-window.hpp"
#include "../ui/settings-manager.hpp"
#include "../ui/source-menu-scene-switch.hpp"
#include "../ui/theme-enhancements.hpp"
#include "../ui/websocket-window.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDockWidget>
#include <QMainWindow>

#include <cstring>
#include <set>

namespace StreamUP {
namespace Features {

using PS = SettingsManager::PluginSettings;

namespace {

// How a part's switch is stored. Module switches live under "modules"; the
// StreamUP theme's tweaks are their own settings and only do anything while
// the StreamUP theme is in use.
struct Binding {
	bool SettingsManager::ModuleSettings::*module = nullptr;
	bool PS::*tweak = nullptr;
	ThemeEnhancements::Tweak tweakKind = ThemeEnhancements::Tweak::Mixer;
};

struct Entry {
	Feature f;
	Binding b;
};

using MS = SettingsManager::ModuleSettings;
using TW = ThemeEnhancements::Tweak;

Entry mod(const char *id, Group g, const char *key, bool MS::*m, bool live, const char *since = "")
{
	Entry e;
	e.f = {QString::fromUtf8(id), g, QStringLiteral("Features.%1.Name").arg(key),
	       QStringLiteral("Features.%1.Line").arg(key), false, live, QString::fromUtf8(since)};
	e.b.module = m;
	return e;
}

Entry tweak(const char *id, Group g, const char *key, bool PS::*t, TW kind, const char *since = "")
{
	Entry e;
	e.f = {QString::fromUtf8(id), g, QStringLiteral("Features.%1.Name").arg(key),
	       QStringLiteral("Features.%1.Line").arg(key), false, true, QString::fromUtf8(since)};
	e.b.tweak = t;
	e.b.tweakKind = kind;
	return e;
}

Entry core(const char *id, Group g, const char *key, const char *since = "")
{
	Entry e;
	e.f = {QString::fromUtf8(id), g, QStringLiteral("Features.%1.Name").arg(key),
	       QStringLiteral("Features.%1.Line").arg(key), true, true, QString::fromUtf8(since)};
	return e;
}

const std::vector<Entry> &Entries()
{
	static const std::vector<Entry> list = {
		// Products
		core("install-product", Group::Products, "InstallProduct"),
		core("my-products", Group::Products, "MyProducts", "2.5.0"),
		core("plugin-check", Group::Products, "PluginCheck"),

		// Docks & Layout
		mod("scene-organiser", Group::Docks, "SceneOrganiser", &MS::sceneOrganiser, false),
		mod("streamup-dock", Group::Docks, "StreamupDock", &MS::streamupDock, false),
		mod("multidock", Group::Docks, "MultiDock", &MS::multiDock, true),
		mod("toolbar", Group::Docks, "Toolbar", &MS::toolbar, true),
		tweak("vertical-canvas", Group::Docks, "VerticalCanvas", &PS::themeVerticalCanvas, TW::VerticalCanvas),
		tweak("multistream", Group::Docks, "Multistream", &PS::themeMultistream, TW::Multistream, "2.5.0"),
		tweak("dock-spacing", Group::Docks, "DockSpacing", &PS::themeSpacing, TW::Spacing),
		tweak("window-polish", Group::Docks, "WindowPolish", &PS::themeWindowPolish, TW::WindowPolish),

		// Mixer & Audio
		mod("mixer-dock", Group::Mixer, "MixerDock", &MS::mixerDock, false, "2.5.0"),
		tweak("mixer-layout", Group::Mixer, "MixerLayout", &PS::themeMixer, TW::Mixer),

		// Sources
		mod("adjustment-layer", Group::Sources, "AdjustmentLayer", &MS::adjustmentLayerSource, false),
		mod("scene-switch", Group::Sources, "SceneSwitch", &MS::sourceMenuSceneSwitch, true, "2.5.0"),
		tweak("source-folders", Group::Sources, "SourceFolders", &PS::themeSourceFolders, TW::SourceFolders,
		      "2.5.0"),
		tweak("source-guides", Group::Sources, "SourceGuides", &PS::themeSourceGuides, TW::SourceGuides),

		// Hotkeys & Control
		mod("hotkeys", Group::Hotkeys, "Hotkeys", &MS::hotkeys, true),
		mod("websocket", Group::Hotkeys, "WebSocket", &MS::websocketApi, true),
		tweak("studio-mode", Group::Hotkeys, "StudioMode", &PS::themeStudioMode, TW::StudioMode),

		// Safety
		mod("backup", Group::Safety, "Backup", &MS::backup, true),
	};
	return list;
}

const Entry *FindEntry(const QString &id)
{
	for (const Entry &e : Entries())
		if (e.f.id == id)
			return &e;
	return nullptr;
}

// What was switched on when this OBS session started. Restart parts run, or
// do not, by this until the next start.
MS g_startup;
bool g_startupMarked = false;

QList<int> Parts(const QString &v)
{
	QList<int> out;
	for (const QString &p : v.split('.'))
		out << p.toInt();
	while (out.size() < 3)
		out << 0;
	return out;
}

bool Newer(const QString &a, const QString &b)
{
	const QList<int> x = Parts(a), y = Parts(b);
	for (int i = 0; i < 3; ++i)
		if (x[i] != y[i])
			return x[i] > y[i];
	return false;
}

void RaiseDock(const char *objectName)
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main)
		return;
	if (auto *dock = main->findChild<QDockWidget *>(QString::fromUtf8(objectName))) {
		dock->setVisible(true);
		dock->raise();
		if (dock->isFloating())
			dock->activateWindow();
	}
}

} // namespace

const std::vector<Feature> &All()
{
	static const std::vector<Feature> list = [] {
		std::vector<Feature> out;
		for (const Entry &e : Entries())
			out.push_back(e.f);
		return out;
	}();
	return list;
}

const Feature *Find(const QString &id)
{
	for (const Feature &f : All())
		if (f.id == id)
			return &f;
	return nullptr;
}

bool IsOn(const QString &id)
{
	const Entry *e = FindEntry(id);
	if (!e)
		return false;
	if (e->f.core)
		return true;
	const PS s = SettingsManager::GetCurrentSettings();
	if (e->b.module)
		return s.modules.*(e->b.module);
	if (e->b.tweak)
		return s.*(e->b.tweak);
	return false;
}

bool IsActive(const QString &id)
{
	const Entry *e = FindEntry(id);
	if (!e)
		return false;
	if (e->f.core)
		return true;
	if (e->b.tweak)
		return ThemeEnhancements::IsTweakActive(e->b.tweakKind);
	if (!e->f.live && g_startupMarked)
		return g_startup.*(e->b.module);
	return IsOn(id);
}

void SetOn(const QString &id, bool on)
{
	const Entry *e = FindEntry(id);
	if (!e || e->f.core)
		return;
	PS s = SettingsManager::GetCurrentSettings();
	if (e->b.module)
		s.modules.*(e->b.module) = on;
	else if (e->b.tweak)
		s.*(e->b.tweak) = on;
	SettingsManager::UpdateSettings(s);

	if (e->b.tweak) {
		ThemeEnhancements::ReloadTweakSettings();
		ThemeEnhancements::ReapplyThemeTweaks();
		return;
	}
	if (id == "multidock")
		MultiDock::MultiDockManager::SetGlobalEnabled(on);
	else if (id == "toolbar")
		SetToolbarEnabled(on);
	else if (id == "hotkeys")
		HotkeyManager::SetHotkeyGroupEnabled(on);
	else if (id == "scene-switch")
		on ? SourceMenuSceneSwitch::Install() : SourceMenuSceneSwitch::Uninstall();
	else if (id == "backup" && on)
		Backup::ResolveLocations();
	// backup and websocket are read where they are used: the menu, the
	// automatic backup and each websocket request check the setting.
}

void ShowMe(const QString &id)
{
	if (id == "install-product") {
		FileManager::LoadStreamupFileWithWarning();
	} else if (id == "my-products") {
		MyProductsWindow::Show();
	} else if (id == "plugin-check") {
		PluginManager::ShowCachedPluginUpdatesDialog();
	} else if (id == "scene-organiser") {
		RaiseDock("StreamUPSceneOrganiserNormal");
	} else if (id == "streamup-dock") {
		RaiseDock("StreamUPDock");
	} else if (id == "mixer-dock") {
		RaiseDock("StreamUPMixer");
	} else if (id == "multidock") {
		MultiDock::ShowManageMultiDocksDialog();
	} else if (id == "toolbar") {
		SettingsManager::ShowSettingsDialog(SettingsManager::SettingsTabToolbar);
	} else if (id == "hotkeys") {
		SettingsManager::ShowSettingsDialog(SettingsManager::SettingsTabHotkeys);
	} else if (id == "websocket") {
		WebSocketWindow::ShowWebSocketWindow(false);
	} else if (id == "backup") {
		SettingsManager::ShowSettingsDialog(SettingsManager::SettingsTabBackup);
	} else if (id == "mixer-layout") {
		RaiseDock("mixerDock");
	} else if (id == "vertical-canvas" || id == "multistream") {
		// Aitum's docks: the first match by title part is enough.
		auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		if (main) {
			const QString want = id == "multistream" ? "Multistream" : "Vertical";
			for (QDockWidget *d : main->findChildren<QDockWidget *>()) {
				if (d->windowTitle().contains(want, Qt::CaseInsensitive) &&
				    !d->objectName().startsWith("StreamUP")) {
					d->setVisible(true);
					d->raise();
					break;
				}
			}
		}
	} else if (id == "studio-mode") {
		RaiseDock("transitionsDock");
	} else {
		// Sources dock parts (adjustment layer, scene switch, folders,
		// guides) and the window tweaks: the Sources dock is where they show.
		RaiseDock("sourcesDock");
	}
}

QString LastSeenVersion()
{
	const PS s = SettingsManager::GetCurrentSettings();
	if (!s.featuresVersionSeen.empty())
		return QString::fromStdString(s.featuresVersionSeen);
	// Someone updating from before the Features page: the wizard's stamp says
	// which version they last set things up in.
	return QString::fromStdString(s.wizardVersionShown);
}

QStringList NewSince(const QString &lastSeenVersion)
{
	QStringList out;
	if (lastSeenVersion.isEmpty())
		return out; // a new user: everything is new, so nothing is singled out
	for (const Feature &f : All())
		if (!f.since.isEmpty() && Newer(f.since, lastSeenVersion))
			out << f.id;
	return out;
}

void MarkStartupState()
{
	g_startup = SettingsManager::GetCurrentSettings().modules;
	g_startupMarked = true;
}

QString SwitchedOffMessage(const QString &id)
{
	const Feature *f = Find(id);
	const QString name = f ? QString::fromUtf8(obs_module_text(f->nameKey.toUtf8().constData())) : id;
	return QString::fromUtf8(obs_module_text("Features.SwitchedOff")).arg(name);
}

QString GroupLabel(Group g)
{
	switch (g) {
	case Group::Products:
		return obs_module_text("Features.Group.Products");
	case Group::Docks:
		return obs_module_text("Features.Group.Docks");
	case Group::Mixer:
		return obs_module_text("Features.Group.Mixer");
	case Group::Sources:
		return obs_module_text("Features.Group.Sources");
	case Group::Hotkeys:
		return obs_module_text("Features.Group.Hotkeys");
	case Group::Safety:
		return obs_module_text("Features.Group.Safety");
	}
	return {};
}

namespace {

bool CountInScene(obs_scene_t *scene, bool &found);

bool ItemHasLayer(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	bool &found = *static_cast<bool *>(param);
	obs_source_t *src = obs_sceneitem_get_source(item);
	const char *uid = src ? obs_source_get_unversioned_id(src) : nullptr;
	if (uid && strcmp(uid, "streamup_adjustment_layer") == 0) {
		found = true;
		return false;
	}
	if (obs_sceneitem_is_group(item))
		CountInScene(obs_sceneitem_group_get_scene(item), found);
	return !found;
}

bool CountInScene(obs_scene_t *scene, bool &found)
{
	if (scene)
		obs_scene_enum_items(scene, ItemHasLayer, &found);
	return found;
}

} // namespace

int AdjustmentLayerSceneCount()
{
	// Every scene on every canvas, groups included. A scene counts once
	// however many layers it holds.
	std::set<obs_source_t *> seen;
	int count = 0;
	auto visit = [&](obs_source_t *source) {
		// Groups are counted inside the scene that holds them, not on
		// their own.
		if (!source || seen.count(source) || obs_source_is_group(source))
			return;
		seen.insert(source);
		bool found = false;
		if (CountInScene(obs_scene_from_source(source), found))
			++count;
	};

	struct Ctx {
		decltype(visit) *fn;
	} ctx{&visit};
	obs_enum_scenes(
		[](void *p, obs_source_t *src) {
			(*static_cast<Ctx *>(p)->fn)(src);
			return true;
		},
		&ctx);
	obs_enum_canvases(
		[](void *p, obs_canvas_t *canvas) {
			obs_canvas_enum_scenes(
				canvas,
				[](void *p2, obs_source_t *src) {
					(*static_cast<Ctx *>(p2)->fn)(src);
					return true;
				},
				p);
			return true;
		},
		&ctx);
	return count;
}

} // namespace Features
} // namespace StreamUP
