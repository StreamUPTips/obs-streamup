#include "source-menu-scene-switch.hpp"
#include "settings-manager.hpp"
#include "scene-organiser/scene-canvas.hpp"
#include "../core/canvas-lookup.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QLabel>
#include <QListView>
#include <QMainWindow>
#include <QMenu>
#include <QPersistentModelIndex>
#include <QPointer>

#include <cstring>

namespace StreamUP {
namespace SourceMenuSceneSwitch {

namespace {

constexpr const char *kTreeClass = "SourceTree";
// The menu follows the right-click within the same event, so this only guards
// against a stale click being matched to some unrelated menu much later.
constexpr qint64 kArmWindowMs = 2000;

QString T(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

bool isScene(obs_source_t *source)
{
	return source && obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE && !obs_source_is_group(source);
}

// A canvas by its name (Source Clone stores the canvas it clones from by name).
obs_canvas_t *canvasByName(const char *name)
{
	struct Find {
		const char *name;
		obs_canvas_t *found;
	} find{name, nullptr};
	obs_enum_canvases(
		[](void *param, obs_canvas_t *canvas) {
			auto *f = static_cast<Find *>(param);
			const char *n = obs_canvas_get_name(canvas);
			if (n && strcmp(n, f->name) == 0 && !obs_canvas_removed(canvas)) {
				f->found = obs_canvas_get_ref(canvas);
				return false;
			}
			return true;
		},
		&find);
	return find.found;
}

// The source a Sources dock row shows, by its name. A vertical dock's rows can
// be vertical scenes, whose names are only unique on that canvas, so that
// canvas is asked first there. NEW reference or nullptr.
obs_source_t *rowSource(const QString &name, bool verticalTree)
{
	const QByteArray utf8 = name.toUtf8();
	if (verticalTree) {
		if (obs_canvas_t *vertical = StreamUP::CanvasLookup::AcquireVertical()) {
			obs_source_t *source = obs_canvas_get_source_by_name(vertical, utf8.constData());
			obs_canvas_release(vertical);
			if (source)
				return source;
		}
	}
	return StreamUP::CanvasLookup::GetSourceByName(utf8.constData());
}

// The scene this row would switch to: the row's own source when it is a
// nested scene, or what a Source Clone clones when that is a scene. Clones of
// "current scene" or "previous scene" follow whatever is live, so they have
// no scene of their own and get nothing. NEW reference or nullptr.
obs_source_t *targetScene(obs_source_t *source)
{
	if (!source)
		return nullptr;
	if (isScene(source))
		return obs_source_get_ref(source);

	const char *id = obs_source_get_unversioned_id(source);
	if (!id || strcmp(id, "source-clone") != 0)
		return nullptr;

	obs_data_t *settings = obs_source_get_settings(source);
	const long long cloneType = obs_data_get_int(settings, "clone_type");
	const QByteArray cloneName = obs_data_get_string(settings, "clone");
	const QByteArray canvasName = obs_data_get_string(settings, "canvas");
	obs_data_release(settings);

	if (cloneType != 0 || cloneName.isEmpty())
		return nullptr;

	obs_source_t *cloned = nullptr;
	if (!canvasName.isEmpty()) {
		if (obs_canvas_t *canvas = canvasByName(canvasName.constData())) {
			cloned = obs_canvas_get_source_by_name(canvas, cloneName.constData());
			obs_canvas_release(canvas);
		}
	}
	if (!cloned)
		cloned = StreamUP::CanvasLookup::GetSourceByName(cloneName.constData());

	if (!isScene(cloned)) {
		obs_source_release(cloned);
		return nullptr;
	}
	return cloned;
}

// The name a SourceTree row shows: its widget carries the source icon (a
// label with no text) and the name label.
QString rowName(QAbstractItemView *tree, const QModelIndex &index)
{
	QWidget *row = index.isValid() ? tree->indexWidget(index) : nullptr;
	if (!row)
		return QString();
	for (QLabel *label : row->findChildren<QLabel *>()) {
		if (!label->text().isEmpty())
			return label->text();
	}
	return QString();
}

void runOnScene(const QString &uuid, void (*action)(obs_source_t *))
{
	obs_source_t *scene = obs_get_source_by_uuid(uuid.toUtf8().constData());
	if (scene)
		action(scene);
	obs_source_release(scene);
}

class MenuFilter : public QObject {
public:
	bool eventFilter(QObject *watched, QEvent *event) override
	{
		switch (event->type()) {
		case QEvent::ContextMenu:
			noteRightClick(watched, static_cast<QContextMenuEvent *>(event));
			break;
		case QEvent::Polish:
		case QEvent::Show:
			// Polish comes as the menu is laid out, before it is sized, so
			// entries added then fit; Show is the fallback for a menu that
			// was polished earlier.
			if (m_armed && m_clock.isValid())
				if (auto *menu = qobject_cast<QMenu *>(watched))
					maybeExtend(menu);
			break;
		default:
			break;
		}
		return QObject::eventFilter(watched, event);
	}

private:
	void noteRightClick(QObject *watched, QContextMenuEvent *event)
	{
		auto *viewport = qobject_cast<QWidget *>(watched);
		auto *tree = viewport ? qobject_cast<QAbstractItemView *>(viewport->parentWidget()) : nullptr;
		if (!tree || tree->viewport() != viewport || !tree->inherits(kTreeClass))
			return;

		m_tree = tree;
		m_index = event->reason() == QContextMenuEvent::Mouse ? QPersistentModelIndex(tree->indexAt(event->pos()))
								      : QPersistentModelIndex(tree->currentIndex());
		m_armed = true;
		m_clock.start();
	}

	void maybeExtend(QMenu *menu)
	{
		// Only the menu itself, not one of its submenus.
		if (qobject_cast<QMenu *>(menu->parentWidget()))
			return;
		m_armed = false;
		if (m_clock.elapsed() > kArmWindowMs || !m_tree || !m_index.isValid())
			return;
		if (menu->property("streamup_scene_switch").toBool())
			return;
		menu->setProperty("streamup_scene_switch", true);

		if (!StreamUP::SettingsManager::GetCurrentSettings().modules.sourceMenuSceneSwitch)
			return;

		auto *main = static_cast<QWidget *>(obs_frontend_get_main_window());
		QWidget *mainTree = main ? main->findChild<QWidget *>("sources") : nullptr;
		const bool verticalTree = m_tree.data() != mainTree;

		const QString name = rowName(m_tree, m_index);
		if (name.isEmpty())
			return;

		obs_source_t *source = rowSource(name, verticalTree);
		obs_source_t *scene = targetScene(source);
		obs_source_release(source);
		if (!scene)
			return;

		const QString sceneName = QString::fromUtf8(obs_source_get_name(scene));
		const QString uuid = QString::fromUtf8(obs_source_get_uuid(scene));
		const bool vertical = StreamUP::SceneOrganiser::Canvas::SceneBelongsTo(
			StreamUP::SceneOrganiser::CanvasType::Vertical, scene);
		obs_source_release(scene);

		QList<QAction *> entries;
		if (vertical) {
			// The vertical canvas has no preview side: always live, through
			// Aitum's own switch and transition.
			auto *a = new QAction(T("SourceMenu.SwitchToVerticalScene"), menu);
			QObject::connect(a, &QAction::triggered, a, [uuid]() {
				runOnScene(uuid, [](obs_source_t *s) {
					StreamUP::SceneOrganiser::Canvas::SetCurrentScene(
						StreamUP::SceneOrganiser::CanvasType::Vertical, s);
				});
			});
			entries << a;
		} else if (obs_frontend_preview_program_mode_active()) {
			// Studio mode: preview first, as picking it in the Scenes dock
			// does, and straight to program as the second choice.
			auto *preview = new QAction(T("SourceMenu.SwitchToSceneInPreview"), menu);
			QObject::connect(preview, &QAction::triggered, preview, [uuid]() {
				runOnScene(uuid, [](obs_source_t *s) { obs_frontend_set_current_preview_scene(s); });
			});
			auto *program = new QAction(T("SourceMenu.SwitchToSceneInProgram"), menu);
			QObject::connect(program, &QAction::triggered, program, [uuid]() {
				// In studio mode this runs OBS's transition to program.
				runOnScene(uuid, [](obs_source_t *s) { obs_frontend_set_current_scene(s); });
			});
			entries << preview << program;
		} else {
			auto *a = new QAction(T("SourceMenu.SwitchToScene"), menu);
			QObject::connect(a, &QAction::triggered, a, [uuid]() {
				runOnScene(uuid, [](obs_source_t *s) { obs_frontend_set_current_scene(s); });
			});
			entries << a;
		}

		// Short labels keep the menu its normal width; the scene's name is
		// the tooltip.
		menu->setToolTipsVisible(true);
		for (QAction *a : entries)
			a->setToolTip(sceneName);
		QAction *first = menu->actions().value(0);
		for (QAction *a : entries)
			menu->insertAction(first, a);
		if (first)
			menu->insertSeparator(first);
	}

	QPointer<QAbstractItemView> m_tree;
	QPersistentModelIndex m_index;
	QElapsedTimer m_clock;
	bool m_armed = false;
};

MenuFilter *g_filter = nullptr;

} // namespace



void Install()
{
	if (g_filter)
		return;
	g_filter = new MenuFilter();
	QApplication::instance()->installEventFilter(g_filter);
}

void Uninstall()
{
	if (!g_filter)
		return;
	QApplication::instance()->removeEventFilter(g_filter);
	delete g_filter;
	g_filter = nullptr;
}

} // namespace SourceMenuSceneSwitch
} // namespace StreamUP
