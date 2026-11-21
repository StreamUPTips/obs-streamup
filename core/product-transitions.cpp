#include "product-transitions.hpp"

#include <streamup/debug-logger.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QLineEdit>
#include <QMetaObject>
#include <QPointer>
#include <QWidget>

namespace StreamUP {
namespace ProductTransitions {

namespace {

QString Text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

// While OBS adds a transition it asks for a name in a NameDialog and then
// opens an OBSBasicProperties window. This answers the first with our name and
// remembers the second so it can be closed straight after.
class AddTransitionDriver : public QObject {
public:
	explicit AddTransitionDriver(const QString &name) : m_name(name) {}

	QPointer<QWidget> properties;
	bool answered = false;

protected:
	bool eventFilter(QObject *obj, QEvent *event) override
	{
		if (event->type() != QEvent::Show || !obj->isWidgetType())
			return false;
		auto *w = static_cast<QWidget *>(obj);
		if (!w->isWindow())
			return false;
		const char *cls = w->metaObject()->className();
		if (!answered && qstrcmp(cls, "NameDialog") == 0) {
			auto *dlg = qobject_cast<QDialog *>(w);
			QLineEdit *edit = w->findChild<QLineEdit *>();
			if (dlg && edit) {
				answered = true;
				// Never seen: it is answered on the next turn of its own loop.
				dlg->setWindowOpacity(0.0);
				edit->setText(m_name);
				QMetaObject::invokeMethod(dlg, "accept", Qt::QueuedConnection);
			}
		} else if (qstrcmp(cls, "OBSBasicProperties") == 0) {
			properties = w;
			w->setWindowOpacity(0.0);
		}
		return false;
	}

private:
	QString m_name;
};

bool IsTransition(obs_source_t *s)
{
	return s && obs_source_get_type(s) == OBS_SOURCE_TYPE_TRANSITION;
}

} // namespace

obs_source_t *Find(const QString &name, const QString &uuidHint)
{
	if (!uuidHint.isEmpty()) {
		obs_source_t *s = obs_get_source_by_uuid(uuidHint.toUtf8().constData());
		if (IsTransition(s)) {
			// Only while it is still in the frontend's list: a removed
			// transition can linger until its last reference goes.
			bool listed = false;
			struct obs_frontend_source_list list = {};
			obs_frontend_get_transitions(&list);
			for (size_t i = 0; i < list.sources.num; i++)
				listed = listed || list.sources.array[i] == s;
			obs_frontend_source_list_free(&list);
			if (listed)
				return s;
		}
		obs_source_release(s);
	}
	if (name.isEmpty())
		return nullptr;
	obs_source_t *found = nullptr;
	struct obs_frontend_source_list list = {};
	obs_frontend_get_transitions(&list);
	const QByteArray wanted = name.toUtf8();
	for (size_t i = 0; i < list.sources.num && !found; i++) {
		obs_source_t *s = list.sources.array[i];
		const char *n = obs_source_get_name(s);
		if (n && wanted == n)
			found = obs_source_get_ref(s);
	}
	obs_frontend_source_list_free(&list);
	return found;
}

bool IsCurrent(const QString &name)
{
	obs_source_t *cur = obs_frontend_get_current_transition();
	const bool same = cur && name == QString::fromUtf8(obs_source_get_name(cur));
	obs_source_release(cur);
	return same;
}

bool UseSceneAsTransition(const OverlayControls::ActionInfo &action, bool *created, QString *error)
{
	if (created)
		*created = false;
	auto fail = [error](const QString &why) {
		if (error)
			*error = why;
		DebugLogger::LogWarningFormat("Products", "Use as transition: %s", why.toUtf8().constData());
		return false;
	};

	if (!obs_source_get_display_name(kSceneAsTransitionId))
		return fail(Text("Products.Transition.NeedsPlugin"));

	// Scene as Transition finds its scene by name on the main canvas.
	obs_source_t *scene = obs_get_source_by_name(action.scene.toUtf8().constData());
	const bool sceneOk = scene && obs_source_is_scene(scene);
	obs_source_release(scene);
	if (!sceneOk)
		return fail(Text("Products.Transition.NoScene").arg(action.scene));

	obs_source_t *tr = Find(action.transition);
	if (tr && qstrcmp(obs_source_get_unversioned_id(tr), kSceneAsTransitionId) != 0) {
		obs_source_release(tr);
		return fail(Text("Products.Transition.NameTaken").arg(action.transition));
	}

	obs_source_t *previous = obs_frontend_get_current_transition();
	bool isNew = false;
	if (!tr) {
		QWidget *main = static_cast<QWidget *>(obs_frontend_get_main_window());
		AddTransitionDriver driver(action.transition);
		qApp->installEventFilter(&driver);
		const bool invoked = QMetaObject::invokeMethod(main, "AddTransition", Qt::DirectConnection,
							       Q_ARG(const char *, kSceneAsTransitionId));
		qApp->removeEventFilter(&driver);
		// Closing does not undo anything: OBS only reverts on Cancel.
		if (driver.properties)
			driver.properties->close();
		if (!invoked) {
			obs_source_release(previous);
			return fail(Text("Products.Transition.CouldNotAdd"));
		}
		tr = Find(action.transition);
		if (!tr) {
			obs_source_release(previous);
			return fail(Text("Products.Transition.CouldNotAdd"));
		}
		isNew = true;
	}

	// A new transition gets the product's timings. An existing one keeps
	// whatever the user has set since, and is only pointed at the scene.
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "scene", action.scene.toUtf8().constData());
	if (isNew) {
		obs_data_set_double(settings, "duration", double(action.duration));
		obs_data_set_int(settings, "tp_type", 0);
		obs_data_set_double(settings, "transition_point", action.transitionPoint);
	}
	obs_source_update(tr, settings);
	obs_data_release(settings);

	// OBS picks every transition it adds. Put the old one back unless the
	// product wants this one picked.
	if (action.makeCurrent)
		obs_frontend_set_current_transition(tr);
	else if (isNew && previous && previous != tr)
		obs_frontend_set_current_transition(previous);

	DebugLogger::LogInfoFormat("Products", "%s transition '%s' playing scene '%s'", isNew ? "Added" : "Updated",
				   action.transition.toUtf8().constData(), action.scene.toUtf8().constData());
	obs_source_release(previous);
	obs_source_release(tr);
	if (created)
		*created = isNew;
	obs_frontend_save();
	return true;
}

} // namespace ProductTransitions
} // namespace StreamUP
