#include "product-registry.hpp"
#include "canvas-lookup.hpp"

#include <streamup/debug-logger.hpp>

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>

namespace StreamUP {
namespace Products {

namespace oc = StreamUP::OverlayControls;

namespace {

// Scene names a .StreamUP file creates. Read with Qt's parser: OBS's drops
// arrays of plain values, and the file is read once more for the manifest anyway.
QStringList ScenesInFile(const QString &path)
{
	QStringList out;
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return out;
	const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
	for (const QJsonValue v : root.value("sources").toArray()) {
		const QJsonObject s = v.toObject();
		if (s.value("id").toString() == "scene" && !s.value("name").toString().isEmpty())
			out.append(s.value("name").toString());
	}
	return out;
}

struct SceneWalk {
	QList<QPair<QString, oc::ProductStamp>> stamped; // scene name, stamp
};

bool CollectStamp(void *param, obs_source_t *scene)
{
	if (!scene || obs_source_is_group(scene))
		return true;
	const oc::ProductStamp stamp = StampOf(scene);
	if (!stamp.isEmpty())
		static_cast<SceneWalk *>(param)->stamped.append({QString::fromUtf8(obs_source_get_name(scene)), stamp});
	return true;
}

bool WalkCanvas(void *param, obs_canvas_t *canvas)
{
	if (!obs_canvas_removed(canvas))
		obs_canvas_enum_scenes(canvas, CollectStamp, param);
	return true;
}

} // namespace

oc::ProductStamp StampOf(obs_source_t *scene)
{
	oc::ProductStamp p;
	if (!scene)
		return p;
	obs_data_t *priv = obs_source_get_private_settings(scene);
	obs_data_t *stamp = priv ? obs_data_get_obj(priv, kStampKey) : nullptr;
	if (stamp) {
		p.id = QString::fromUtf8(obs_data_get_string(stamp, "id"));
		p.name = QString::fromUtf8(obs_data_get_string(stamp, "name"));
		p.version = QString::fromUtf8(obs_data_get_string(stamp, "version"));
		p.overlay = QString::fromUtf8(obs_data_get_string(stamp, "overlay"));
	}
	obs_data_release(stamp);
	obs_data_release(priv);
	return p;
}

void StampFromFile(const QString &path)
{
	const oc::ProductStamp product = oc::ProductFromFile(path);
	if (product.isEmpty())
		return; // an older file with no manifest: nothing to say who it is
	const QString installed = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
	int count = 0;
	for (const QString &name : ScenesInFile(path)) {
		obs_source_t *scene = StreamUP::CanvasLookup::GetSourceByName(name.toUtf8().constData());
		if (!scene || !obs_source_is_scene(scene)) {
			obs_source_release(scene);
			continue;
		}
		obs_data_t *stamp = obs_data_create();
		obs_data_set_string(stamp, "id", product.id.toUtf8().constData());
		obs_data_set_string(stamp, "name", product.name.toUtf8().constData());
		obs_data_set_string(stamp, "version", product.version.toUtf8().constData());
		obs_data_set_string(stamp, "overlay", product.overlay.toUtf8().constData());
		obs_data_set_string(stamp, "installed", installed.toUtf8().constData());
		obs_data_t *priv = obs_source_get_private_settings(scene);
		obs_data_set_obj(priv, kStampKey, stamp);
		obs_data_release(priv);
		obs_data_release(stamp);
		obs_source_release(scene);
		++count;
	}
	DebugLogger::LogInfoFormat("Products", "Stamped %d scene(s) as %s %s", count, product.id.toUtf8().constData(),
				   product.version.toUtf8().constData());
}

QList<ProductEntry> ListProducts()
{
	QList<ProductEntry> out;
	auto entryFor = [&out](const QString &productId, const QString &overlayId) -> ProductEntry * {
		for (ProductEntry &e : out)
			if ((!productId.isEmpty() && e.id == productId) || (!overlayId.isEmpty() && e.overlayId == overlayId))
				return &e;
		return nullptr;
	};

	for (const oc::OverlayInfo &o : oc::ListOverlays()) {
		ProductEntry e;
		e.id = o.product.id.isEmpty() ? o.id : o.product.id;
		e.name = o.product.name.isEmpty() ? o.name : o.product.name;
		e.version = o.product.version.isEmpty() ? o.version : o.product.version;
		e.overlayId = o.id;
		e.hasControls = o.hasControls;
		for (const oc::ActionInfo &a : o.actions)
			e.isTransition = e.isTransition || a.type == oc::ActionType::UseAsTransition;
		// The manifest's own scene list, while those scenes still exist.
		for (const QString &scene : o.scenes) {
			obs_source_t *s = StreamUP::CanvasLookup::GetSourceByName(scene.toUtf8().constData());
			if (s)
				e.scenes.append(scene);
			obs_source_release(s);
		}
		out.append(e);
	}

	SceneWalk walk;
	obs_enum_canvases(WalkCanvas, &walk);
	for (const auto &[scene, stamp] : walk.stamped) {
		ProductEntry *e = entryFor(stamp.id, stamp.overlay);
		if (!e) {
			ProductEntry fresh;
			fresh.id = stamp.id;
			fresh.name = stamp.name.isEmpty() ? stamp.id : stamp.name;
			fresh.version = stamp.version;
			out.append(fresh);
			e = &out.last();
		}
		e->stamped = true;
		if (!e->scenes.contains(scene))
			e->scenes.append(scene);
	}

	std::sort(out.begin(), out.end(), [](const ProductEntry &a, const ProductEntry &b) {
		return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
	});
	return out;
}

} // namespace Products
} // namespace StreamUP
