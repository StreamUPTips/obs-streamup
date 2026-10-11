// Everything Overlay Controls does to OBS itself: reading and writing linked
// settings, rendering templates, moving layout parts and keeping the snapshot.
// All of it runs under the registry lock held by overlay-controls.cpp.

#include "overlay-controls-internal.hpp"

#include <streamup/debug-logger.hpp>

#include <QHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <functional>

namespace StreamUP {
namespace OverlayControls {
namespace Internal {

// ================================================================ values

QColor ParseColour(const QString &text)
{
	QString s = text.trimmed();
	if (!s.startsWith('#'))
		s.prepend('#');
	if (s.length() == 9) // #RRGGBBAA: QColor wants the alpha first
		return QColor(QStringLiteral("#%1%2").arg(s.mid(7, 2), s.mid(1, 6)));
	if (s.length() == 7)
		return QColor(s);
	return QColor();
}

QString ColourText(const QColor &c)
{
	if (c.alpha() < 255)
		return QStringLiteral("#%1%2")
			.arg(c.name(QColor::HexRgb).mid(1))
			.arg(c.alpha(), 2, 16, QLatin1Char('0'))
			.toUpper();
	return c.name(QColor::HexRgb).toUpper();
}

// OBS stores colours as 0xAABBGGRR.
QColor FromObsColour(qint64 v)
{
	const quint32 u = static_cast<quint32>(v);
	return QColor(u & 0xff, (u >> 8) & 0xff, (u >> 16) & 0xff, (u >> 24) & 0xff);
}

qint64 ToObsColour(const QColor &c)
{
	return (static_cast<qint64>(c.alpha()) << 24) | (static_cast<qint64>(c.blue()) << 16) |
	       (static_cast<qint64>(c.green()) << 8) | static_cast<qint64>(c.red());
}

bool IsNumeric(ControlType t)
{
	return t == ControlType::Slider || t == ControlType::Number;
}

bool IsString(ControlType t)
{
	return t == ControlType::Text || t == ControlType::TextArea || t == ControlType::Select || t == ControlType::Font;
}

QJsonValue NormaliseValue(const Control &c, const QJsonValue &in)
{
	const QJsonValue undefined(QJsonValue::Undefined);
	switch (c.type) {
	case ControlType::Colour: {
		QColor colour;
		if (in.isString())
			colour = ParseColour(in.toString());
		else if (in.isDouble())
			colour = FromObsColour(static_cast<qint64>(in.toDouble()));
		return colour.isValid() ? QJsonValue(ColourText(colour)) : undefined;
	}
	case ControlType::Slider:
	case ControlType::Number: {
		bool ok = in.isDouble();
		double v = in.toDouble();
		if (in.isString())
			v = in.toString().toDouble(&ok);
		if (!ok || !std::isfinite(v))
			return undefined;
		return std::clamp(v, c.min, c.max);
	}
	case ControlType::Toggle:
		if (in.isBool())
			return in.toBool();
		if (in.isDouble())
			return in.toDouble() != 0.0;
		if (in.isString()) {
			const QString s = in.toString().trimmed().toLower();
			if (s == "toggle")
				return !c.value.toBool();
			if (s == "true" || s == "on" || s == "1")
				return true;
			if (s == "false" || s == "off" || s == "0")
				return false;
		}
		return undefined;
	case ControlType::Text:
	case ControlType::TextArea:
	case ControlType::Font: {
		if (in.isString())
			return in.toString();
		if (in.isDouble())
			return QString::number(in.toDouble());
		return undefined;
	}
	case ControlType::Select: {
		const QString s = in.isDouble() ? QString::number(in.toDouble()) : in.toString();
		if (!in.isString() && !in.isDouble())
			return undefined;
		if (c.editable)
			return s;
		for (const SelectOption &o : c.options)
			if (o.value == s)
				return s;
		return undefined;
	}
	case ControlType::Platforms: {
		if (!in.isArray())
			return undefined;
		QJsonArray out;
		for (const QJsonValue v : in.toArray()) {
			const QJsonObject row = v.toObject();
			const QString platform = row.value("platform").toString();
			bool known = false;
			bool chat = false;
			for (const PlatformDef &p : c.platforms) {
				if (p.id == platform) {
					known = true;
					chat = p.chat;
				}
			}
			if (!known)
				continue;
			out.append(QJsonObject{{"platform", platform},
					       {"username", row.value("username").toString().trimmed()},
					       {"nameplate", row.value("nameplate").toBool(true)},
					       {"chat", chat && row.value("chat").toBool(false)}});
		}
		return out;
	}
	case ControlType::Layout: {
		// Partial updates merge into the current value.
		QJsonObject cur = c.value.toObject();
		const QJsonObject o = in.toObject();
		if (!in.isObject())
			return undefined;
		for (const char *k : {"x", "y", "scale"}) {
			if (!o.contains(k))
				continue;
			if (!o.value(k).isDouble() || !std::isfinite(o.value(k).toDouble()))
				return undefined;
			cur[k] = o.value(k).toDouble();
		}
		if (o.contains("visible"))
			cur["visible"] = o.value("visible").toBool(true);
		if (!c.rig.movable) {
			cur["x"] = 0.0;
			cur["y"] = 0.0;
		}
		cur["x"] = std::clamp(cur.value("x").toDouble(0.0), -4000.0, 4000.0);
		cur["y"] = std::clamp(cur.value("y").toDouble(0.0), -4000.0, 4000.0);
		cur["scale"] = std::clamp(cur.value("scale").toDouble(1.0), c.rig.scaleMin, c.rig.scaleMax);
		if (!cur.contains("visible"))
			cur["visible"] = true;
		return cur;
	}
	}
	return undefined;
}

// ================================================================ targets

obs_source_t *ResolveTarget(QString &source, QString &sourceUuid, QString &filter, QString &filterUuid)
{
	obs_source_t *src = nullptr;
	if (!sourceUuid.isEmpty())
		src = obs_get_source_by_uuid(sourceUuid.toUtf8().constData());
	if (!src)
		src = obs_get_source_by_name(source.toUtf8().constData());

	if (filter.isEmpty()) {
		if (src) {
			sourceUuid = QString::fromUtf8(obs_source_get_uuid(src));
			source = QString::fromUtf8(obs_source_get_name(src));
		}
		return src;
	}

	obs_source_t *f = nullptr;
	if (!filterUuid.isEmpty()) {
		f = obs_get_source_by_uuid(filterUuid.toUtf8().constData());
		// A filter UUID is only trusted while the filter sits on a source.
		if (f && !obs_filter_get_parent(f)) {
			obs_source_release(f);
			f = nullptr;
		}
	}
	if (!f && src)
		f = obs_source_get_filter_by_name(src, filter.toUtf8().constData());
	if (f) {
		filterUuid = QString::fromUtf8(obs_source_get_uuid(f));
		filter = QString::fromUtf8(obs_source_get_name(f));
		if (obs_source_t *parent = obs_filter_get_parent(f)) {
			sourceUuid = QString::fromUtf8(obs_source_get_uuid(parent));
			source = QString::fromUtf8(obs_source_get_name(parent));
		}
	}
	obs_source_release(src);
	return f;
}

namespace {

// Runs fn on a scene item. The item pointer is only valid inside fn.
bool WithItem(QString &scene, QString &sceneUuid, const QString &source, const QString &sourceUuid, qint64 *itemId,
	      const std::function<void(obs_sceneitem_t *)> &fn)
{
	obs_source_t *sceneSource = nullptr;
	if (!sceneUuid.isEmpty())
		sceneSource = obs_get_source_by_uuid(sceneUuid.toUtf8().constData());
	if (!sceneSource)
		sceneSource = obs_get_source_by_name(scene.toUtf8().constData());
	if (!sceneSource)
		return false;
	obs_scene_t *sc = obs_scene_from_source(sceneSource);
	if (!sc)
		sc = obs_group_from_source(sceneSource);
	if (!sc) {
		obs_source_release(sceneSource);
		return false;
	}
	sceneUuid = QString::fromUtf8(obs_source_get_uuid(sceneSource));
	scene = QString::fromUtf8(obs_source_get_name(sceneSource));

	auto matches = [&](obs_sceneitem_t *item) {
		obs_source_t *s = obs_sceneitem_get_source(item);
		if (!s)
			return false;
		if (!sourceUuid.isEmpty())
			return sourceUuid == QString::fromUtf8(obs_source_get_uuid(s));
		return source == QString::fromUtf8(obs_source_get_name(s));
	};

	obs_sceneitem_t *item = nullptr;
	if (itemId && *itemId) {
		item = obs_scene_find_sceneitem_by_id(sc, *itemId);
		if (item && !matches(item))
			item = nullptr;
	}
	if (!item)
		item = obs_scene_find_source_recursive(sc, source.toUtf8().constData());
	if (item && itemId)
		*itemId = obs_sceneitem_get_id(item);
	if (item)
		fn(item);
	obs_source_release(sceneSource);
	return item != nullptr;
}

// The item's box on its scene's canvas, worked out from its transform rather
// than read from OBS's box matrix: that matrix only catches up on the next
// video tick, and layout moves need the answer straight away. Rotation is
// ignored; every overlay part is square to the canvas.
QRectF ItemBox(obs_sceneitem_t *item, const double *pos = nullptr, const double *scale = nullptr)
{
	obs_source_t *src = obs_sceneitem_get_source(item);
	struct obs_transform_info info;
	obs_sceneitem_get_info2(item, &info);
	struct obs_sceneitem_crop crop;
	obs_sceneitem_get_crop(item, &crop);
	const double px = pos ? pos[0] : info.pos.x;
	const double py = pos ? pos[1] : info.pos.y;
	const double sx = scale ? scale[0] : info.scale.x;
	const double sy = scale ? scale[1] : info.scale.y;
	double w;
	double h;
	if (info.bounds_type != OBS_BOUNDS_NONE) {
		// Bounds are a fixed box the source is fitted into; they scale with
		// the part like the item's own scale would.
		const double k = (scale && info.scale.x != 0.0f) ? scale[0] / info.scale.x : 1.0;
		w = info.bounds.x * k;
		h = info.bounds.y * k;
	} else {
		w = double(int(obs_source_get_width(src)) - crop.left - crop.right) * std::fabs(sx);
		h = double(int(obs_source_get_height(src)) - crop.top - crop.bottom) * std::fabs(sy);
	}
	double x = px;
	double y = py;
	if (!(info.alignment & OBS_ALIGN_LEFT))
		x -= (info.alignment & OBS_ALIGN_RIGHT) ? w : w / 2.0;
	if (!(info.alignment & OBS_ALIGN_TOP))
		y -= (info.alignment & OBS_ALIGN_BOTTOM) ? h : h / 2.0;
	return QRectF(x, y, w, h);
}

// ---------------------------------------------------------------- typed read/write

QJsonObject ReadTyped(obs_data_t *settings, const char *key, bool *hadUserValue)
{
	QJsonObject out;
	*hadUserValue = false;
	obs_data_item_t *item = obs_data_item_byname(settings, key);
	if (!item)
		return out;
	*hadUserValue = obs_data_item_has_user_value(item);
	switch (obs_data_item_gettype(item)) {
	case OBS_DATA_NUMBER:
		if (obs_data_item_numtype(item) == OBS_DATA_NUM_INT) {
			out["t"] = "i";
			out["v"] = static_cast<qint64>(obs_data_item_get_int(item));
		} else {
			out["t"] = "d";
			out["v"] = obs_data_item_get_double(item);
		}
		break;
	case OBS_DATA_BOOLEAN:
		out["t"] = "b";
		out["v"] = obs_data_item_get_bool(item);
		break;
	case OBS_DATA_STRING:
		out["t"] = "s";
		out["v"] = QString::fromUtf8(obs_data_item_get_string(item));
		break;
	case OBS_DATA_OBJECT: {
		obs_data_t *obj = obs_data_item_get_obj(item);
		out["t"] = "o";
		out["v"] = QJsonDocument::fromJson(QByteArray(obj ? obs_data_get_json(obj) : "{}")).object();
		obs_data_release(obj);
		break;
	}
	default:
		break;
	}
	obs_data_item_release(&item);
	return out;
}

void WriteTyped(obs_data_t *data, const char *key, const QJsonObject &typed)
{
	const QString t = typed.value("t").toString();
	const QJsonValue v = typed.value("v");
	if (t == "i") {
		obs_data_set_int(data, key, static_cast<long long>(v.toDouble()));
	} else if (t == "d") {
		obs_data_set_double(data, key, v.toDouble());
	} else if (t == "b") {
		obs_data_set_bool(data, key, v.toBool());
	} else if (t == "s") {
		obs_data_set_string(data, key, v.toString().toUtf8().constData());
	} else if (t == "o") {
		const QByteArray json = QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact);
		obs_data_t *obj = obs_data_create_from_json(json.constData());
		obs_data_set_obj(data, key, obj);
		obs_data_release(obj);
	}
}

// Settings keys may reach one level into an object ("font.face"). OBS replaces
// a whole object on update, so the rest of it is copied across unchanged.
void SplitKey(const QString &key, QString &parent, QString &leaf)
{
	const int dot = key.indexOf('.');
	parent = dot < 0 ? QString() : key.left(dot);
	leaf = dot < 0 ? key : key.mid(dot + 1);
}

// The object holding the key's leaf, referenced. Null when it does not exist.
obs_data_t *Container(obs_source_t *target, const QString &key, QString &leaf)
{
	QString parent;
	SplitKey(key, parent, leaf);
	obs_data_t *settings = obs_source_get_settings(target);
	if (parent.isEmpty())
		return settings;
	obs_data_t *obj = obs_data_get_obj(settings, parent.toUtf8().constData());
	obs_data_release(settings);
	return obj;
}

void UpdateKey(obs_source_t *target, const QString &key, const std::function<void(obs_data_t *, const char *)> &fill)
{
	QString parent;
	QString leaf;
	SplitKey(key, parent, leaf);
	const QByteArray leafUtf8 = leaf.toUtf8();
	obs_data_t *update = obs_data_create();
	if (parent.isEmpty()) {
		fill(update, leafUtf8.constData());
	} else {
		obs_data_t *settings = obs_source_get_settings(target);
		obs_data_t *current = obs_data_get_obj(settings, parent.toUtf8().constData());
		obs_data_t *copy = obs_data_create();
		if (current)
			obs_data_apply(copy, current);
		fill(copy, leafUtf8.constData());
		obs_data_set_obj(update, parent.toUtf8().constData(), copy);
		obs_data_release(copy);
		obs_data_release(current);
		obs_data_release(settings);
	}
	obs_source_update(target, update);
	obs_data_release(update);
}

QString TransformText(const Binding &b, QString s)
{
	if (b.textCase == "upper")
		s = s.toUpper();
	else if (b.textCase == "lower")
		s = s.toLower();
	if (b.letterSpace) {
		QStringList words;
		for (const QString &w : s.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)) {
			QStringList letters;
			for (const QChar ch : w)
				letters.append(QString(ch));
			words.append(letters.join(' '));
		}
		s = words.join(QStringLiteral("   "));
	}
	return s;
}

double SliderResult(const Binding &b, const Control &c, double v)
{
	double out = v;
	switch (b.mode) {
	case BindMode::Set:
		out = v;
		break;
	case BindMode::Multiply:
		out = b.base * v;
		break;
	case BindMode::Map: {
		const double span = c.max - c.min;
		const double t = span != 0.0 ? (v - c.min) / span : 0.0;
		out = b.mapTo0 + t * (b.mapTo1 - b.mapTo0);
		break;
	}
	}
	if (b.clamp)
		out = std::clamp(out, b.clampMin, b.clampMax);
	if (b.integer)
		out = std::round(out);
	return out;
}

// A colour keeps each target's own install alpha, scaled by the control's
// alpha, and an optional tint keeps designed relationships (a text glow a
// touch paler than the ring).
qint64 ColourResult(const Binding &b, const QColor &c)
{
	const int installAlpha = static_cast<int>((static_cast<quint32>(b.baseColour) >> 24) & 0xff);
	QColor out = c;
	if (b.lighten != 0.0) {
		const double t = std::fabs(b.lighten);
		const double target = b.lighten > 0.0 ? 255.0 : 0.0;
		out.setRgb(static_cast<int>(std::lround(c.red() + (target - c.red()) * t)),
			   static_cast<int>(std::lround(c.green() + (target - c.green()) * t)),
			   static_cast<int>(std::lround(c.blue() + (target - c.blue()) * t)));
	}
	out.setAlpha(static_cast<int>(std::lround(installAlpha * c.alphaF())));
	return ToObsColour(out);
}

bool ToggleResult(const Binding &b, bool on)
{
	return b.invert ? !on : on;
}

bool BoolValue(const Control &c)
{
	return c.type == ControlType::Toggle ? c.value.toBool() : c.value.toDouble() != 0.0;
}

} // namespace

// ================================================================ bindings

bool BindingTargetExists(Binding &b)
{
	if (!b.scene.isEmpty()) {
		qint64 id = 0;
		return WithItem(b.scene, b.sceneUuid, b.source, b.sourceUuid, &id, [](obs_sceneitem_t *) {});
	}
	obs_source_t *t = ResolveTarget(b.source, b.sourceUuid, b.filter, b.filterUuid);
	obs_source_release(t);
	return t != nullptr;
}

// Records what the setting holds now as its install value.
//
// A setting can be missing for a moment after install: the shader filter only
// publishes its uniform defaults on its first update, which OBS runs on the
// next video frame. A multiply link cannot work without a real base, so it
// stays uncaptured and tries again on its next write. Any other link can go
// ahead: its install state is simply "not set".
bool CaptureBinding(Binding &b)
{
	if (!b.scene.isEmpty()) {
		bool visible = true;
		qint64 id = 0;
		if (!WithItem(b.scene, b.sceneUuid, b.source, b.sourceUuid, &id,
			      [&](obs_sceneitem_t *item) { visible = obs_sceneitem_visible(item); }))
			return false;
		b.hadUserValue = true;
		b.original = QJsonObject{{"t", "b"}, {"v", visible}};
		b.captured = true;
		return true;
	}

	obs_source_t *target = ResolveTarget(b.source, b.sourceUuid, b.filter, b.filterUuid);
	if (!target)
		return false;

	if (b.key == kEnabledKey) {
		b.hadUserValue = true;
		b.original = QJsonObject{{"t", "b"}, {"v", obs_source_enabled(target)}};
		b.base = obs_source_enabled(target) ? 1.0 : 0.0;
		b.captured = true;
		obs_source_release(target);
		return true;
	}

	QString leaf;
	obs_data_t *container = Container(target, b.key, leaf);
	const QByteArray leafUtf8 = leaf.toUtf8();
	obs_data_item_t *item = container ? obs_data_item_byname(container, leafUtf8.constData()) : nullptr;
	const bool exists = item != nullptr;
	obs_data_item_release(&item);

	if (!exists && b.mode == BindMode::Multiply) {
		obs_data_release(container);
		obs_source_release(target);
		return false;
	}
	if (exists) {
		b.original = ReadTyped(container, leafUtf8.constData(), &b.hadUserValue);
		b.base = obs_data_get_double(container, leafUtf8.constData());
		b.baseColour = obs_data_get_int(container, leafUtf8.constData());
		// Inside an object the whole object is written back on restore, so
		// the leaf always counts as set.
		if (b.key.contains('.'))
			b.hadUserValue = true;
	} else {
		b.hadUserValue = false;
		b.original = QJsonObject();
		b.base = 0.0;
		b.baseColour = 0xff000000; // opaque, so a colour written here shows
	}
	obs_data_release(container);
	obs_source_release(target);
	b.captured = true;
	return true;
}

bool WriteBinding(Binding &b, const Control &c)
{
	if (!b.captured && !CaptureBinding(b)) {
		DebugLogger::LogWarningFormat("OverlayControls", "'%s' / '%s' has no '%s' yet, skipped this time",
					      b.source.toUtf8().constData(), b.filter.toUtf8().constData(),
					      b.key.toUtf8().constData());
		return false;
	}

	if (!b.scene.isEmpty()) {
		const bool on = ToggleResult(b, BoolValue(c));
		qint64 id = 0;
		return WithItem(b.scene, b.sceneUuid, b.source, b.sourceUuid, &id,
				[on](obs_sceneitem_t *item) { obs_sceneitem_set_visible(item, on); });
	}

	obs_source_t *target = ResolveTarget(b.source, b.sourceUuid, b.filter, b.filterUuid);
	if (!target)
		return false;

	if (b.key == kEnabledKey) {
		obs_source_set_enabled(target, ToggleResult(b, BoolValue(c)));
		obs_source_release(target);
		return true;
	}

	switch (c.type) {
	case ControlType::Colour: {
		const QColor colour = ParseColour(c.value.toString());
		if (colour.isValid()) {
			const long long v = ColourResult(b, colour);
			UpdateKey(target, b.key, [v](obs_data_t *d, const char *k) { obs_data_set_int(d, k, v); });
		}
		break;
	}
	case ControlType::Slider:
	case ControlType::Number: {
		const double v = SliderResult(b, c, c.value.toDouble());
		const bool integer = b.integer;
		UpdateKey(target, b.key, [v, integer](obs_data_t *d, const char *k) {
			if (integer)
				obs_data_set_int(d, k, static_cast<long long>(v));
			else
				obs_data_set_double(d, k, v);
		});
		break;
	}
	case ControlType::Toggle: {
		const bool on = ToggleResult(b, c.value.toBool());
		UpdateKey(target, b.key, [on](obs_data_t *d, const char *k) { obs_data_set_bool(d, k, on); });
		break;
	}
	case ControlType::Text:
	case ControlType::TextArea:
	case ControlType::Select:
	case ControlType::Font: {
		const QByteArray s = TransformText(b, c.value.toString()).toUtf8();
		UpdateKey(target, b.key, [&s](obs_data_t *d, const char *k) { obs_data_set_string(d, k, s.constData()); });
		break;
	}
	case ControlType::Platforms:
	case ControlType::Layout:
		break; // these write through templates and layout, not bindings
	}
	obs_source_release(target);
	return true;
}

void RestoreBinding(Binding &b)
{
	if (!b.captured)
		return;
	if (!b.scene.isEmpty()) {
		const bool on = b.original.value("v").toBool(true);
		qint64 id = 0;
		WithItem(b.scene, b.sceneUuid, b.source, b.sourceUuid, &id,
			 [on](obs_sceneitem_t *item) { obs_sceneitem_set_visible(item, on); });
		return;
	}
	obs_source_t *target = ResolveTarget(b.source, b.sourceUuid, b.filter, b.filterUuid);
	if (!target)
		return;

	if (b.key == kEnabledKey) {
		obs_source_set_enabled(target, b.original.value("v").toBool());
	} else if (!b.hadUserValue) {
		// The setting was on its default at install. Put it back on the
		// default rather than pinning today's default as a user value.
		obs_data_t *settings = obs_source_get_settings(target);
		obs_data_unset_user_value(settings, b.key.toUtf8().constData());
		obs_data_release(settings);
		obs_source_update(target, nullptr);
	} else {
		const QJsonObject original = b.original;
		UpdateKey(target, b.key, [&original](obs_data_t *d, const char *k) { WriteTyped(d, k, original); });
	}
	obs_source_release(target);
}

bool BindingDrifted(Binding &b, const Control &c)
{
	if (!b.captured)
		return false;
	if (!b.scene.isEmpty()) {
		bool drifted = false;
		const bool want = ToggleResult(b, BoolValue(c));
		qint64 id = 0;
		WithItem(b.scene, b.sceneUuid, b.source, b.sourceUuid, &id,
			 [&](obs_sceneitem_t *item) { drifted = obs_sceneitem_visible(item) != want; });
		return drifted;
	}
	obs_source_t *target = ResolveTarget(b.source, b.sourceUuid, b.filter, b.filterUuid);
	if (!target)
		return false;

	bool drifted = false;
	if (b.key == kEnabledKey) {
		drifted = obs_source_enabled(target) != ToggleResult(b, BoolValue(c));
	} else {
		QString leaf;
		obs_data_t *container = Container(target, b.key, leaf);
		const QByteArray k = leaf.toUtf8();
		if (container) {
			switch (c.type) {
			case ControlType::Colour: {
				const QColor colour = ParseColour(c.value.toString());
				if (colour.isValid())
					drifted = static_cast<quint32>(obs_data_get_int(container, k.constData())) !=
						  static_cast<quint32>(ColourResult(b, colour));
				break;
			}
			case ControlType::Slider:
			case ControlType::Number: {
				const double want = SliderResult(b, c, c.value.toDouble());
				const double live = obs_data_get_double(container, k.constData());
				drifted = std::fabs(live - want) > 1e-6 * std::max(1.0, std::fabs(want));
				break;
			}
			case ControlType::Toggle:
				drifted = obs_data_get_bool(container, k.constData()) != ToggleResult(b, c.value.toBool());
				break;
			case ControlType::Text:
			case ControlType::TextArea:
			case ControlType::Select:
			case ControlType::Font:
				drifted = QString::fromUtf8(obs_data_get_string(container, k.constData())) !=
					  TransformText(b, c.value.toString());
				break;
			case ControlType::Platforms:
			case ControlType::Layout:
				break;
			}
		}
		obs_data_release(container);
	}
	obs_source_release(target);
	return drifted;
}

// ================================================================ templates

namespace {

Control *FindControlIn(Overlay &o, const QString &id)
{
	for (Group &g : o.groups)
		for (Control &c : g.controls)
			if (c.id == id)
				return &c;
	return nullptr;
}

QString NumberText(double v, int decimals = 6)
{
	QString s = QString::number(v, 'f', decimals);
	if (s.contains('.')) {
		while (s.endsWith('0'))
			s.chop(1);
		if (s.endsWith('.'))
			s.chop(1);
	}
	return s == "-0" ? QStringLiteral("0") : s;
}

// Handles that end up inside a ProText tag: no quotes, commas or brackets,
// which would end the list or the tag early.
QString CleanWord(QString s)
{
	s.remove('"');
	s.remove(',');
	s.remove('[');
	s.remove(']');
	return s.trimmed();
}

QString PlatformToken(Overlay &o, const Control &c, const QString &part)
{
	const QJsonArray rows = c.value.toArray();
	QStringList icons;
	QStringList words;
	QJsonArray chat;
	for (const QJsonValue v : rows) {
		const QJsonObject row = v.toObject();
		const PlatformDef *def = nullptr;
		for (const PlatformDef &p : c.platforms)
			if (p.id == row.value("platform").toString())
				def = &p;
		if (!def)
			continue;
		QString name = CleanWord(row.value("username").toString());
		if (row.value("nameplate").toBool(true) && !name.isEmpty()) {
			icons.append(o.resourceDir + "/" + def->icon);
			words.append(c.wordCase == "upper" ? name.toUpper() : c.wordCase == "lower" ? name.toLower() : name);
		}
		QString channel = row.value("username").toString().trimmed();
		if (channel.startsWith('@'))
			channel.remove(0, 1);
		if (def->chat && row.value("chat").toBool(false) && !channel.isEmpty())
			chat.append(QJsonObject{{"platform", def->id}, {"channel", channel}, {"enabled", true}});
	}
	if (part == "icons")
		return icons.isEmpty() ? o.resourceDir + "/Icons/blank.svg" : icons.join(',');
	if (part == "words")
		return words.isEmpty() ? QStringLiteral(" ") : words.join(',');
	if (part == "chat")
		return QString::fromUtf8(QJsonDocument(chat).toJson(QJsonDocument::Compact));
	if (part == "count")
		return QString::number(words.size());
	return QString();
}

QString ControlText(const Control &c)
{
	switch (c.type) {
	case ControlType::Colour:
		return c.value.toString().toLower();
	case ControlType::Slider:
	case ControlType::Number:
		return NumberText(c.value.toDouble(), c.decimals);
	case ControlType::Toggle:
		return c.value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
	case ControlType::Text:
	case ControlType::TextArea:
	case ControlType::Select:
	case ControlType::Font:
		return c.value.toString();
	case ControlType::Platforms:
	case ControlType::Layout:
		return QString::fromUtf8((c.value.isArray() ? QJsonDocument(c.value.toArray())
							    : QJsonDocument(c.value.toObject()))
						 .toJson(QJsonDocument::Compact));
	}
	return QString();
}

QString ApplyFilter(const QString &value, const QString &filter)
{
	const QString name = filter.section(':', 0, 0).trimmed();
	const QString arg = filter.section(':', 1).trimmed();
	bool ok = false;
	const double n = value.toDouble(&ok);
	if (name == "upper")
		return value.toUpper();
	if (name == "lower")
		return value.toLower();
	if (name == "clean")
		return CleanWord(value);
	if (name == "inv")
		return (ok && n != 0.0) ? NumberText(1.0 / n) : value;
	if (name == "mul")
		return ok ? NumberText(n * arg.toDouble()) : value;
	if (name == "add")
		return ok ? NumberText(n + arg.toDouble()) : value;
	if (name == "int")
		return ok ? QString::number(static_cast<long long>(std::llround(n))) : value;
	if (name == "fixed")
		return ok ? QString::number(n, 'f', std::clamp(arg.toInt(), 0, 8)) : value;
	if (name == "json") {
		QString j = QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact));
		return j.mid(2, j.size() - 4); // strip ["..."]
	}
	return value;
}

QString RenderTemplate(Overlay &o, const QString &templ)
{
	static const QRegularExpression token(QStringLiteral("\\{\\{\\s*([^{}|]+?)\\s*((?:\\|[^{}|]+)*)\\}\\}"));
	QString out;
	qsizetype last = 0;
	auto it = token.globalMatch(templ);
	while (it.hasNext()) {
		const QRegularExpressionMatch m = it.next();
		out += templ.mid(last, m.capturedStart() - last);
		last = m.capturedEnd();

		const QString name = m.captured(1);
		QString value;
		if (name == "@resources") {
			value = o.resourceDir;
		} else {
			const QString id = name.section('.', 0, 0);
			const QString part = name.section('.', 1);
			if (Control *c = FindControlIn(o, id))
				value = (c->type == ControlType::Platforms && !part.isEmpty()) ? PlatformToken(o, *c, part)
												 : ControlText(*c);
		}
		for (const QString &f : m.captured(2).split('|', Qt::SkipEmptyParts))
			value = ApplyFilter(value, f);
		out += value;
	}
	out += templ.mid(last);
	return out;
}

} // namespace

void RenderOutputs(Overlay &o, const QString &changedControl)
{
	for (Output &out : o.outputs) {
		if (!changedControl.isEmpty() && !out.uses.contains(changedControl))
			continue;
		obs_source_t *target = ResolveTarget(out.source, out.sourceUuid, out.filter, out.filterUuid);
		if (!target)
			continue;
		const QByteArray text = RenderTemplate(o, out.templ).toUtf8();
		// Only write what changed: rewriting ProText markup restarts its
		// sequences, which would make the nameplate jump on unrelated edits.
		QString leaf;
		obs_data_t *container = Container(target, out.key, leaf);
		const bool same = container && QByteArray(obs_data_get_string(container, leaf.toUtf8().constData())) == text;
		obs_data_release(container);
		if (!same)
			UpdateKey(target, out.key,
				  [&text](obs_data_t *d, const char *k) { obs_data_set_string(d, k, text.constData()); });
		obs_source_release(target);
	}
}

// ================================================================ layout

namespace {

// p' = a * p + b: a layout part's uniform scale and move, composed with its parent's.
struct Affine {
	double a = 1.0;
	double bx = 0.0;
	double by = 0.0;
	bool visible = true;
};

Control *LayoutControl(Overlay &o, const QString &id)
{
	Control *c = FindControlIn(o, id);
	return (c && c->type == ControlType::Layout) ? c : nullptr;
}

Affine FullTransform(Overlay &o, Control &c, int depth = 0)
{
	Affine parent;
	if (!c.rig.parent.isEmpty() && depth < 8)
		if (Control *p = LayoutControl(o, c.rig.parent))
			parent = FullTransform(o, *p, depth + 1);

	const QJsonObject v = c.value.toObject();
	// Without a measured frame there is no pivot to scale about, so the part
	// can only move until it is measured.
	const double s = c.rig.frameCaptured ? v.value("scale").toDouble(1.0) : 1.0;
	const double dx = v.value("x").toDouble(0.0);
	const double dy = v.value("y").toDouble(0.0);
	const QPointF centre = c.rig.installFrame.center();
	const double pivotX = parent.a * centre.x() + parent.bx;
	const double pivotY = parent.a * centre.y() + parent.by;

	Affine full;
	full.a = s * parent.a;
	full.bx = s * parent.bx + (1.0 - s) * pivotX + dx;
	full.by = s * parent.by + (1.0 - s) * pivotY + dy;
	full.visible = parent.visible && v.value("visible").toBool(true);
	return full;
}

int Depth(Overlay &o, Control &c)
{
	int d = 0;
	Control *p = &c;
	while (p && !p->rig.parent.isEmpty() && d < 8) {
		p = LayoutControl(o, p->rig.parent);
		++d;
	}
	return d;
}

bool FindItemBox(QString scene, const QString &source, QRectF &box)
{
	QString sceneUuid;
	qint64 id = 0;
	return WithItem(scene, sceneUuid, source, QString(), &id, [&](obs_sceneitem_t *item) { box = ItemBox(item); });
}

bool CaptureGeoLink(GeoLink &l, Overlay &o)
{
	if (l.captured)
		return true;
	obs_source_t *target = ResolveTarget(l.source, l.sourceUuid, l.filter, l.filterUuid);
	if (!target)
		return false;
	QString leaf;
	obs_data_t *container = Container(target, l.key, leaf);
	if (container) {
		l.original = ReadTyped(container, leaf.toUtf8().constData(), &l.hadUserValue);
		l.base = obs_data_get_double(container, leaf.toUtf8().constData());
	}
	obs_data_release(container);
	obs_source_release(target);

	// Boxes at install: from the parts' captured install transforms when
	// the items belong to a part, so a part moved before this ran still
	// measures from where it started.
	auto installBox = [&](const QString &source, QRectF &box) {
		for (Group &g : o.groups)
			for (Control &c : g.controls)
				if (c.type == ControlType::Layout)
					for (RigItem &it : c.rig.items)
						if (it.captured && it.scene == l.scene && it.source == source) {
							QString scene = it.scene;
							QString sceneUuid = it.sceneUuid;
							qint64 id = it.itemId;
							const double pos[2] = {it.px, it.py};
							const double scale[2] = {it.sx, it.sy};
							return WithItem(scene, sceneUuid, it.source, it.sourceUuid, &id,
									[&](obs_sceneitem_t *item) { box = ItemBox(item, pos, scale); });
						}
		return FindItemBox(l.scene, source, box);
	};
	if (!installBox(l.of, l.ofInstall) || !installBox(l.within, l.withinInstall))
		return false;
	if (l.ofInstall.width() <= 0 || l.withinInstall.width() <= 0 || l.withinInstall.height() <= 0)
		return false;
	l.captured = true;
	return true;
}

void UpdateGeoLink(GeoLink &l, Overlay &o)
{
	if (!CaptureGeoLink(l, o))
		return;
	QRectF of;
	QRectF within;
	if (!FindItemBox(l.scene, l.of, of) || !FindItemBox(l.scene, l.within, within) || within.width() <= 0 ||
	    within.height() <= 0)
		return;
	double v = l.base;
	if (l.kind == "centre_x_pct") {
		v = (of.center().x() - within.x()) / within.width() * 100.0;
	} else if (l.kind == "width_follow") {
		// A width held as a share of the container's height, kept in step
		// with the followed item's width.
		const double ofRatio = of.width() / l.ofInstall.width();
		const double withinRatio = within.height() / l.withinInstall.height();
		v = l.base * ofRatio / withinRatio;
	}
	obs_source_t *target = ResolveTarget(l.source, l.sourceUuid, l.filter, l.filterUuid);
	if (!target)
		return;
	QString leaf;
	obs_data_t *container = Container(target, l.key, leaf);
	const double live = container ? obs_data_get_double(container, leaf.toUtf8().constData()) : 0.0;
	obs_data_release(container);
	if (std::fabs(live - v) > 1e-6)
		UpdateKey(target, l.key, [v](obs_data_t *d, const char *k) { obs_data_set_double(d, k, v); });
	obs_source_release(target);
}

} // namespace

bool CaptureRig(Overlay &o, Control &c)
{
	Rig &r = c.rig;
	// Only the scene/source pairs that exist: a part may list a source that
	// is in some of its scenes and not others (the ticker glow), and those
	// gaps are by design, not missing items.
	if (r.items.isEmpty()) {
		for (const QString &scene : r.scenes) {
			for (const QString &source : r.sources) {
				RigItem it{scene, source};
				if (WithItem(it.scene, it.sceneUuid, it.source, QString(), &it.itemId, [](obs_sceneitem_t *) {}))
					r.items.append(it);
			}
		}
	}

	for (RigItem &it : r.items) {
		if (it.captured)
			continue;
		WithItem(it.scene, it.sceneUuid, it.source, QString(), &it.itemId, [&](obs_sceneitem_t *item) {
			struct obs_transform_info info;
			obs_sceneitem_get_info2(item, &info);
			it.px = info.pos.x;
			it.py = info.pos.y;
			it.sx = info.scale.x;
			it.sy = info.scale.y;
			it.visible = obs_sceneitem_visible(item);
			it.sourceUuid = QString::fromUtf8(obs_source_get_uuid(obs_sceneitem_get_source(item)));
			it.captured = true;
		});
	}

	if (!r.frameCaptured) {
		// The frame is measured from install transforms, in the first scene
		// that has any of the frame items. A text source can report no size
		// until its first frame renders, so an empty measure waits.
		const QStringList frameSources = r.frameSources.isEmpty() ? r.sources : r.frameSources;
		for (const QString &scene : r.scenes) {
			QRectF frame;
			for (RigItem &it : r.items) {
				if (!it.captured || it.scene != scene || !frameSources.contains(it.source))
					continue;
				QString sceneName = it.scene;
				QString sceneUuid = it.sceneUuid;
				qint64 id = it.itemId;
				const double pos[2] = {it.px, it.py};
				const double scale[2] = {it.sx, it.sy};
				WithItem(sceneName, sceneUuid, it.source, it.sourceUuid, &id, [&](obs_sceneitem_t *item) {
					const QRectF box = ItemBox(item, pos, scale);
					frame = frame.isNull() ? box : frame.united(box);
				});
			}
			if (!frame.isNull()) {
				if (frame.width() > 0 && frame.height() > 0) {
					r.installFrame = frame;
					r.frameScene = scene;
					r.frameCaptured = true;
				}
				break;
			}
		}
	}

	for (GeoLink &l : r.links)
		CaptureGeoLink(l, o);
	return r.frameCaptured;
}

void ApplyLayout(Overlay &o, const QString &changedPart)
{
	// Only the changed part and the parts riding on it move, so a transform
	// edited by hand in All Settings is not undone by moving something else.
	auto affected = [&](Control &c) {
		if (changedPart.isEmpty())
			return true;
		Control *p = &c;
		for (int depth = 0; p && depth < 9; ++depth) {
			if (p->id == changedPart)
				return true;
			p = p->rig.parent.isEmpty() ? nullptr : LayoutControl(o, p->rig.parent);
		}
		return false;
	};

	// Each item is placed by the deepest part that lists it; that part's
	// transform already includes every parent above it.
	struct Target {
		Control *control = nullptr;
		RigItem *item = nullptr;
		int depth = -1;
	};
	QHash<QString, Target> targets;
	for (Group &g : o.groups) {
		for (Control &c : g.controls) {
			if (c.type != ControlType::Layout)
				continue;
			CaptureRig(o, c);
			const int depth = Depth(o, c);
			for (RigItem &it : c.rig.items) {
				if (!it.captured)
					continue;
				const QString key = it.sceneUuid + '\n' + it.sourceUuid;
				Target &t = targets[key];
				if (depth > t.depth)
					t = Target{&c, &it, depth};
			}
		}
	}

	for (Target &t : targets) {
		if (!affected(*t.control))
			continue;
		const Affine xf = FullTransform(o, *t.control);
		RigItem &it = *t.item;
		WithItem(it.scene, it.sceneUuid, it.source, it.sourceUuid, &it.itemId, [&](obs_sceneitem_t *item) {
			struct obs_transform_info info;
			obs_sceneitem_get_info2(item, &info);
			const float px = float(xf.a * it.px + xf.bx);
			const float py = float(xf.a * it.py + xf.by);
			const float sx = float(it.sx * xf.a);
			const float sy = float(it.sy * xf.a);
			if (std::fabs(info.pos.x - px) > 1e-3f || std::fabs(info.pos.y - py) > 1e-3f ||
			    std::fabs(info.scale.x - sx) > 1e-6f || std::fabs(info.scale.y - sy) > 1e-6f) {
				info.pos.x = px;
				info.pos.y = py;
				info.scale.x = sx;
				info.scale.y = sy;
				obs_sceneitem_set_info2(item, &info);
			}
			// A part only hides, and only shows again what it hid itself, so
			// it never fights anything else that shows or hides the item.
			if (!xf.visible && obs_sceneitem_visible(item)) {
				obs_sceneitem_set_visible(item, false);
				it.hiddenByRig = true;
			} else if (xf.visible && it.hiddenByRig) {
				obs_sceneitem_set_visible(item, it.visible);
				it.hiddenByRig = false;
			}
		});
	}

	for (Group &g : o.groups)
		for (Control &c : g.controls)
			if (c.type == ControlType::Layout)
				for (GeoLink &l : c.rig.links)
					UpdateGeoLink(l, o);
}

QRectF CurrentFrame(Overlay &o, Control &c)
{
	CaptureRig(o, c);
	if (!c.rig.frameCaptured)
		return QRectF();
	const Affine xf = FullTransform(o, c);
	const QRectF f = c.rig.installFrame;
	return QRectF(xf.a * f.x() + xf.bx, xf.a * f.y() + xf.by, f.width() * xf.a, f.height() * xf.a);
}

QRectF FrameInScene(Control &c, const QString &scene)
{
	const QStringList frameSources = c.rig.frameSources.isEmpty() ? c.rig.sources : c.rig.frameSources;
	QRectF frame;
	for (RigItem &it : c.rig.items) {
		if (!it.captured || it.scene != scene || !frameSources.contains(it.source))
			continue;
		WithItem(it.scene, it.sceneUuid, it.source, it.sourceUuid, &it.itemId, [&](obs_sceneitem_t *item) {
			// Hidden items count too, so a hidden part can still be found
			// on the canvas and shown again.
			const QRectF box = ItemBox(item);
			frame = frame.isNull() ? box : frame.united(box);
		});
	}
	return frame;
}

int RigMissing(Control &c)
{
	int missing = 0;
	for (RigItem &it : c.rig.items) {
		if (!it.captured) {
			++missing;
			continue;
		}
		if (!WithItem(it.scene, it.sceneUuid, it.source, it.sourceUuid, &it.itemId, [](obs_sceneitem_t *) {}))
			++missing;
	}
	return missing;
}

void RestoreGeoLinks(Overlay &o)
{
	for (Group &g : o.groups) {
		for (Control &c : g.controls) {
			if (c.type != ControlType::Layout)
				continue;
			for (GeoLink &l : c.rig.links) {
				if (!l.captured)
					continue;
				obs_source_t *target = ResolveTarget(l.source, l.sourceUuid, l.filter, l.filterUuid);
				if (!target)
					continue;
				const QJsonObject original = l.original;
				if (l.hadUserValue)
					UpdateKey(target, l.key,
						  [&original](obs_data_t *d, const char *k) { WriteTyped(d, k, original); });
				obs_source_release(target);
			}
		}
	}
}

// ================================================================ snapshot

namespace {

QJsonObject JsonOf(obs_data_t *data)
{
	return QJsonDocument::fromJson(QByteArray(data ? obs_data_get_json(data) : "{}")).object();
}

obs_data_t *DataOf(const QJsonObject &j)
{
	const QByteArray json = QJsonDocument(j).toJson(QJsonDocument::Compact);
	return obs_data_create_from_json(json.constData());
}

QJsonObject TransformOf(obs_sceneitem_t *item)
{
	struct obs_transform_info info;
	obs_sceneitem_get_info2(item, &info);
	struct obs_sceneitem_crop crop;
	obs_sceneitem_get_crop(item, &crop);
	return QJsonObject{{"x", info.pos.x},
			   {"y", info.pos.y},
			   {"rot", info.rot},
			   {"sx", info.scale.x},
			   {"sy", info.scale.y},
			   {"align", qint64(info.alignment)},
			   {"bounds_type", int(info.bounds_type)},
			   {"bounds_align", qint64(info.bounds_alignment)},
			   {"bx", info.bounds.x},
			   {"by", info.bounds.y},
			   {"crop_to_bounds", info.crop_to_bounds},
			   {"cl", crop.left},
			   {"ct", crop.top},
			   {"cr", crop.right},
			   {"cb", crop.bottom},
			   {"visible", obs_sceneitem_visible(item)}};
}

void ApplyTransform(obs_sceneitem_t *item, const QJsonObject &t)
{
	struct obs_transform_info info;
	obs_sceneitem_get_info2(item, &info);
	info.pos.x = float(t.value("x").toDouble());
	info.pos.y = float(t.value("y").toDouble());
	info.rot = float(t.value("rot").toDouble());
	info.scale.x = float(t.value("sx").toDouble(1.0));
	info.scale.y = float(t.value("sy").toDouble(1.0));
	info.alignment = uint32_t(t.value("align").toInteger());
	info.bounds_type = obs_bounds_type(t.value("bounds_type").toInt());
	info.bounds_alignment = uint32_t(t.value("bounds_align").toInteger());
	info.bounds.x = float(t.value("bx").toDouble());
	info.bounds.y = float(t.value("by").toDouble());
	info.crop_to_bounds = t.value("crop_to_bounds").toBool();
	obs_sceneitem_set_info2(item, &info);
	struct obs_sceneitem_crop crop = {t.value("cl").toInt(), t.value("ct").toInt(), t.value("cr").toInt(),
					  t.value("cb").toInt()};
	obs_sceneitem_set_crop(item, &crop);
	obs_sceneitem_set_visible(item, t.value("visible").toBool(true));
}

struct SnapWalk {
	Overlay *overlay;
	QSet<QString> seen;
};

void SnapSourceOf(SnapWalk &w, obs_source_t *src)
{
	const QString uuid = QString::fromUtf8(obs_source_get_uuid(src));
	if (w.seen.contains(uuid))
		return;
	w.seen.insert(uuid);
	SnapSource s;
	s.name = QString::fromUtf8(obs_source_get_name(src));
	s.uuid = uuid;
	s.kind = QString::fromUtf8(obs_source_get_unversioned_id(src));
	obs_data_t *settings = obs_source_get_settings(src);
	s.settings = JsonOf(settings);
	obs_data_release(settings);
	obs_source_enum_filters(
		src,
		[](obs_source_t *, obs_source_t *filter, void *param) {
			auto *snap = static_cast<SnapSource *>(param);
			obs_data_t *fs = obs_source_get_settings(filter);
			snap->filters.append(SnapFilter{QString::fromUtf8(obs_source_get_name(filter)),
							QString::fromUtf8(obs_source_get_uuid(filter)), JsonOf(fs),
							obs_source_enabled(filter)});
			obs_data_release(fs);
		},
		&s);
	w.overlay->snapSources.append(s);
}

void SnapScene(SnapWalk &w, obs_source_t *sceneSource)
{
	obs_scene_t *sc = obs_scene_from_source(sceneSource);
	if (!sc)
		sc = obs_group_from_source(sceneSource);
	if (!sc)
		return;
	SnapSourceOf(w, sceneSource);
	struct Ctx {
		SnapWalk *w;
		QString scene;
		QString sceneUuid;
	} ctx{&w, QString::fromUtf8(obs_source_get_name(sceneSource)),
	      QString::fromUtf8(obs_source_get_uuid(sceneSource))};
	obs_scene_enum_items(
		sc,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			auto *c = static_cast<Ctx *>(param);
			obs_source_t *src = obs_sceneitem_get_source(item);
			c->w->overlay->snapItems.append(SnapItem{c->scene, c->sceneUuid,
								 QString::fromUtf8(obs_source_get_name(src)),
								 obs_sceneitem_get_id(item), TransformOf(item)});
			if (obs_sceneitem_is_group(item))
				SnapScene(*c->w, src);
			else
				SnapSourceOf(*c->w, src);
			return true;
		},
		&ctx);
}

} // namespace

void TakeSnapshot(Overlay &o)
{
	if (o.snapshotTaken)
		return;
	o.snapSources.clear();
	o.snapItems.clear();
	SnapWalk w{&o, {}};
	for (const QString &name : o.scenes) {
		obs_source_t *scene = obs_get_source_by_name(name.toUtf8().constData());
		if (!scene)
			continue;
		SnapScene(w, scene);
		obs_source_release(scene);
	}
	o.snapshotTaken = true;
}

void RestoreSnapshot(Overlay &o, const QString &onlySource)
{
	for (SnapSource &s : o.snapSources) {
		if (!onlySource.isEmpty() && s.name != onlySource)
			continue;
		obs_source_t *src = obs_get_source_by_uuid(s.uuid.toUtf8().constData());
		if (!src)
			src = obs_get_source_by_name(s.name.toUtf8().constData());
		if (!src)
			continue;
		// A scene's own settings are its item list; items are restored
		// one by one below instead, so nothing added since is thrown away.
		if (s.kind != "scene" && s.kind != "group") {
			obs_data_t *data = DataOf(s.settings);
			obs_source_reset_settings(src, data);
			obs_data_release(data);
		}
		for (SnapFilter &f : s.filters) {
			obs_source_t *filter = obs_get_source_by_uuid(f.uuid.toUtf8().constData());
			if (filter && obs_filter_get_parent(filter) != src) {
				obs_source_release(filter);
				filter = nullptr;
			}
			if (!filter)
				filter = obs_source_get_filter_by_name(src, f.name.toUtf8().constData());
			if (!filter)
				continue;
			obs_data_t *data = DataOf(f.settings);
			obs_source_reset_settings(filter, data);
			obs_data_release(data);
			obs_source_set_enabled(filter, f.enabled);
			obs_source_release(filter);
		}
		obs_source_release(src);
	}
	for (SnapItem &it : o.snapItems) {
		if (!onlySource.isEmpty() && it.source != onlySource)
			continue;
		QString scene = it.scene;
		QString sceneUuid = it.sceneUuid;
		qint64 id = it.itemId;
		const QJsonObject t = it.transform;
		WithItem(scene, sceneUuid, it.source, QString(), &id, [&t](obs_sceneitem_t *item) { ApplyTransform(item, t); });
	}
}

QJsonObject SnapshotToJson(const Overlay &o)
{
	QJsonArray sources;
	for (const SnapSource &s : o.snapSources) {
		QJsonArray filters;
		for (const SnapFilter &f : s.filters)
			filters.append(QJsonObject{
				{"name", f.name}, {"uuid", f.uuid}, {"settings", f.settings}, {"enabled", f.enabled}});
		sources.append(QJsonObject{{"name", s.name},
					   {"uuid", s.uuid},
					   {"kind", s.kind},
					   {"settings", s.settings},
					   {"filters", filters}});
	}
	QJsonArray items;
	for (const SnapItem &it : o.snapItems)
		items.append(QJsonObject{{"scene", it.scene},
					 {"sceneUuid", it.sceneUuid},
					 {"source", it.source},
					 {"id", it.itemId},
					 {"transform", it.transform}});
	return QJsonObject{{"taken", o.snapshotTaken}, {"sources", sources}, {"items", items}};
}

void SnapshotFromJson(Overlay &o, const QJsonObject &j)
{
	o.snapshotTaken = j.value("taken").toBool(false);
	o.snapSources.clear();
	o.snapItems.clear();
	for (const QJsonValue sv : j.value("sources").toArray()) {
		const QJsonObject sj = sv.toObject();
		SnapSource s;
		s.name = sj.value("name").toString();
		s.uuid = sj.value("uuid").toString();
		s.kind = sj.value("kind").toString();
		s.settings = sj.value("settings").toObject();
		for (const QJsonValue fv : sj.value("filters").toArray()) {
			const QJsonObject fj = fv.toObject();
			s.filters.append(SnapFilter{fj.value("name").toString(), fj.value("uuid").toString(),
						    fj.value("settings").toObject(), fj.value("enabled").toBool(true)});
		}
		o.snapSources.append(s);
	}
	for (const QJsonValue iv : j.value("items").toArray()) {
		const QJsonObject ij = iv.toObject();
		o.snapItems.append(SnapItem{ij.value("scene").toString(), ij.value("sceneUuid").toString(),
					    ij.value("source").toString(), qint64(ij.value("id").toDouble()),
					    ij.value("transform").toObject()});
	}
}

} // namespace Internal
} // namespace OverlayControls
} // namespace StreamUP
