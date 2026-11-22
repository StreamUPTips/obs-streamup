#include "product-registry.hpp"
#include "canvas-lookup.hpp"

#include <streamup/debug-logger.hpp>

#include <obs-module.h>
#include <util/bmem.h>
#include <util/platform.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

namespace StreamUP {
namespace Products {

namespace oc = StreamUP::OverlayControls;

namespace {

// StreamUP's own names for its product ranges, in the words they appear in
// file and scene names ("StreamUP FX - GTA Wasted", "StreamUP Alerts • Minim").
QString CategoryFromWord(const QString &word)
{
	const QString w = word.trimmed().toLower();
	if (w == "alert" || w == "alerts")
		return "alerts";
	if (w == "fx" || w == "effect" || w == "effects")
		return "effects";
	if (w == "transition" || w == "transitions")
		return "transitions";
	if (w == "widget" || w == "widgets")
		return "widgets";
	if (w == "tool" || w == "tools")
		return "tools";
	if (w == "game" || w == "games")
		return "games";
	if (w == "overlay" || w == "overlays")
		return "overlays";
	return QString();
}

// "StreamUP Transitions - Logo Card" / "StreamUP Alerts • Minim": the range and
// the product name. False for names that do not follow the pattern.
bool SplitStreamupName(const QString &text, QString *category, QString *name)
{
	static const QRegularExpression re(
		QStringLiteral("^StreamUP\\s+([A-Za-z]+)\\s*(?:\\x{2022}|-|\\x{2013}|:)\\s*(.+)$"));
	const auto m = re.match(text.trimmed());
	if (!m.hasMatch())
		return false;
	const QString cat = CategoryFromWord(m.captured(1));
	if (cat.isEmpty())
		return false;
	if (category)
		*category = cat;
	if (name)
		*name = m.captured(2).trimmed();
	return true;
}

QString Slug(const QString &text)
{
	QString s = text.toLower();
	s.replace(QRegularExpression("[^a-z0-9]+"), "-");
	while (s.startsWith('-'))
		s.remove(0, 1);
	while (s.endsWith('-'))
		s.chop(1);
	return s;
}

QString IconDir()
{
	char *path = obs_module_config_path("product-icons");
	const QString dir = path ? QString::fromUtf8(path) : QString();
	bfree(path);
	return dir;
}

QString CachedIcon(const QString &productId)
{
	if (productId.isEmpty())
		return QString();
	const QDir dir(IconDir());
	for (const char *ext : {"png", "jpg", "jpeg", "webp", "svg"}) {
		const QString p = dir.filePath(Slug(productId) + "." + ext);
		if (QFileInfo::exists(p))
			return p;
	}
	return QString();
}

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

// What a file says it is: its manifest, else its own file name.
oc::ProductStamp IdentityOf(const QString &path, const QStringList &scenes)
{
	oc::ProductStamp p = oc::ProductFromFile(path);
	QString category;
	QString name;
	const QString base = QFileInfo(path).completeBaseName();
	const bool named = SplitStreamupName(base, &category, &name) ||
			   (!scenes.isEmpty() && SplitStreamupName(scenes.first(), &category, &name));
	if (p.isEmpty()) {
		// An older product with no manifest: StreamUP files are named
		// "StreamUP <Range> - <Product>", which is enough to list it.
		p.name = named ? name : base;
		p.id = Slug(p.name);
	}
	if (p.category.isEmpty() && named)
		p.category = category;
	return p;
}

struct SceneWalk {
	QList<QPair<QString, oc::ProductStamp>> stamped; // scene name, stamp
	QStringList unstamped;
};

bool CollectStamp(void *param, obs_source_t *scene)
{
	if (!scene || obs_source_is_group(scene))
		return true;
	auto *walk = static_cast<SceneWalk *>(param);
	const oc::ProductStamp stamp = StampOf(scene);
	const QString name = QString::fromUtf8(obs_source_get_name(scene));
	if (!stamp.isEmpty())
		walk->stamped.append({name, stamp});
	else
		walk->unstamped.append(name);
	return true;
}

bool WalkCanvas(void *param, obs_canvas_t *canvas)
{
	if (!obs_canvas_removed(canvas))
		obs_canvas_enum_scenes(canvas, CollectStamp, param);
	return true;
}

} // namespace

QStringList CategoryOrder()
{
	return {"overlays", "alerts", "transitions", "effects", "widgets", "games", "tools", "other"};
}

QString CategoryLabel(const QString &key)
{
	const QByteArray k = "MyProducts.Category." + (CategoryOrder().contains(key) ? key : QString("other")).toUtf8();
	return QString::fromUtf8(obs_module_text(k.constData()));
}

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
		p.category = QString::fromUtf8(obs_data_get_string(stamp, "category"));
	}
	obs_data_release(stamp);
	obs_data_release(priv);
	return p;
}

void CacheIconFromFile(const QString &path)
{
	const oc::ProductStamp p = oc::ProductFromFile(path);
	if (p.isEmpty() || p.icon.isEmpty())
		return;
	const QString dir = IconDir();
	if (dir.isEmpty() || !QDir().mkpath(dir))
		return;
	const QString target = QDir(dir).filePath(Slug(p.id) + "." + QFileInfo(p.icon).suffix().toLower());
	// One icon per product, whatever format the last install brought.
	const QString old = CachedIcon(p.id);
	if (!old.isEmpty())
		QFile::remove(old);
	if (QFile::copy(p.icon, target))
		DebugLogger::LogInfoFormat("Products", "Cached the icon for %s", p.id.toUtf8().constData());
}

void StampFromFile(const QString &path)
{
	const QStringList scenes = ScenesInFile(path);
	const oc::ProductStamp product = IdentityOf(path, scenes);
	if (product.isEmpty())
		return;
	CacheIconFromFile(path);
	const QString installed = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
	int count = 0;
	for (const QString &name : scenes) {
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
		obs_data_set_string(stamp, "category", product.category.toUtf8().constData());
		obs_data_set_string(stamp, "installed", installed.toUtf8().constData());
		obs_data_t *priv = obs_source_get_private_settings(scene);
		obs_data_set_obj(priv, kStampKey, stamp);
		obs_data_release(priv);
		obs_data_release(stamp);
		obs_source_release(scene);
		++count;
	}
	DebugLogger::LogInfoFormat("Products", "Stamped %d scene(s) as %s %s (%s)", count, product.id.toUtf8().constData(),
				   product.version.toUtf8().constData(), product.category.toUtf8().constData());
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
		e.category = o.product.category;
		e.overlayId = o.id;
		e.hasControls = o.hasControls;
		e.shippedControls = true;
		for (const oc::ActionInfo &a : o.actions) {
			if (a.type == oc::ActionType::UseAsTransition && !e.isTransition) {
				e.isTransition = true;
				e.transitionAction = a.id;
			}
		}
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
		if (!stamp.overlay.isEmpty())
			e->shippedControls = true;
		if (e->category.isEmpty())
			e->category = stamp.category;
		if (!e->scenes.contains(scene))
			e->scenes.append(scene);
	}

	// Installed before stamps: StreamUP names its scenes "StreamUP <Range> •
	// <Product>", so those still say what they are.
	for (const QString &scene : walk.unstamped) {
		QString category;
		QString name;
		if (!SplitStreamupName(scene, &category, &name))
			continue;
		bool claimed = false;
		for (const ProductEntry &e : out)
			claimed = claimed || e.scenes.contains(scene);
		if (claimed)
			continue;
		const QString id = Slug(name);
		ProductEntry *e = entryFor(id, QString());
		if (!e) {
			ProductEntry fresh;
			fresh.id = id;
			fresh.name = name;
			fresh.category = category;
			fresh.foundByName = true;
			out.append(fresh);
			e = &out.last();
		}
		e->scenes.append(scene);
	}

	const QStringList order = CategoryOrder();
	for (ProductEntry &e : out) {
		if (!order.contains(e.category))
			e.category = e.isTransition ? "transitions" : (e.hasControls ? "overlays" : "other");
		e.iconPath = CachedIcon(e.id);
	}
	std::sort(out.begin(), out.end(), [&order](const ProductEntry &a, const ProductEntry &b) {
		const int ca = int(order.indexOf(a.category));
		const int cb = int(order.indexOf(b.category));
		if (ca != cb)
			return ca < cb;
		return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
	});
	return out;
}

} // namespace Products
} // namespace StreamUP
