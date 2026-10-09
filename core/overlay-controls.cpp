// Overlay Controls: the registry, the manifest parser, what the collection file
// keeps, and the public API. What each control does to OBS lives in
// overlay-apply.cpp.

#include "overlay-controls-internal.hpp"

#include <streamup/debug-logger.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <mutex>

namespace StreamUP {
namespace OverlayControls {

using namespace Internal;

namespace {

// The manifest is versioned so a future plugin can refuse a newer file cleanly
// instead of half-reading it. Format 2 added text, platforms, layout and outputs;
// a format 1 file reads the same as before.
constexpr int kManifestFormat = 2;
constexpr const char *kFileKey = "streamup_overlay";
constexpr const char *kModuleKey = "streamup_overlay_controls";

std::recursive_mutex g_mutex;
QList<Overlay> g_overlays;
Notifier *g_notifier = nullptr;

// ---------------------------------------------------------------- lookup

Overlay *FindOverlay(const QString &id)
{
	for (Overlay &o : g_overlays)
		if (o.id == id)
			return &o;
	return nullptr;
}

Control *FindControl(Overlay &o, const QString &id)
{
	for (Group &g : o.groups)
		for (Control &c : g.controls)
			if (c.id == id)
				return &c;
	return nullptr;
}

// ---------------------------------------------------------------- manifest parsing

bool ParseBinding(const QJsonObject &j, Binding &b, QString *why)
{
	b.source = j.value("source").toString();
	b.filter = j.value("filter").toString();
	b.scene = j.value("scene").toString();
	b.key = j.value("key").toString();
	if (b.source.isEmpty() || b.key.isEmpty()) {
		*why = "a link needs a source and a key";
		return false;
	}
	if (!b.scene.isEmpty() && b.key != kVisibleKey) {
		*why = "a scene item link can only drive @visible";
		return false;
	}
	const QString mode = j.value("mode").toString("set");
	if (mode == "set") {
		b.mode = BindMode::Set;
	} else if (mode == "multiply") {
		b.mode = BindMode::Multiply;
	} else if (mode == "map") {
		const QJsonArray to = j.value("to").toArray();
		if (to.size() != 2) {
			*why = "a map link needs \"to\": [from, to]";
			return false;
		}
		b.mode = BindMode::Map;
		b.mapTo0 = to.at(0).toDouble();
		b.mapTo1 = to.at(1).toDouble();
	} else {
		*why = QString("unknown link mode \"%1\"").arg(mode);
		return false;
	}
	const QJsonArray clamp = j.value("clamp").toArray();
	if (clamp.size() == 2) {
		b.clamp = true;
		b.clampMin = std::min(clamp.at(0).toDouble(), clamp.at(1).toDouble());
		b.clampMax = std::max(clamp.at(0).toDouble(), clamp.at(1).toDouble());
	}
	b.integer = j.value("integer").toBool(false);
	b.invert = j.value("invert").toBool(false);
	b.lighten = std::clamp(j.value("lighten").toDouble(0.0), -1.0, 1.0);
	b.textCase = j.value("case").toString();
	b.letterSpace = j.value("letterspace").toBool(false);
	return true;
}

bool ParseLayout(const QJsonObject &j, Control &c, QString *why)
{
	Rig &r = c.rig;
	for (const QJsonValue v : j.value("scenes").toArray())
		r.scenes.append(v.toString());
	for (const QJsonValue v : j.value("sources").toArray())
		r.sources.append(v.toString());
	for (const QJsonValue v : j.value("frame").toArray())
		r.frameSources.append(v.toString());
	r.parent = j.value("parent").toString();
	const QJsonArray scale = j.value("scale").toArray();
	if (scale.size() == 2) {
		r.scaleMin = std::max(0.05, std::min(scale.at(0).toDouble(1.0), scale.at(1).toDouble(1.0)));
		r.scaleMax = std::max(scale.at(0).toDouble(1.0), scale.at(1).toDouble(1.0));
	}
	r.movable = j.value("movable").toBool(true);
	if (r.scenes.isEmpty() || r.sources.isEmpty()) {
		*why = QString("layout part \"%1\" needs scenes and sources").arg(c.id);
		return false;
	}
	for (const QJsonValue lv : j.value("links").toArray()) {
		const QJsonObject lj = lv.toObject();
		GeoLink l;
		l.source = lj.value("source").toString();
		l.filter = lj.value("filter").toString();
		l.key = lj.value("key").toString();
		l.kind = lj.value("kind").toString();
		l.scene = lj.value("scene").toString(r.scenes.value(0));
		l.of = lj.value("of").toString();
		l.within = lj.value("within").toString();
		if (l.source.isEmpty() || l.key.isEmpty() || l.of.isEmpty() || l.within.isEmpty() ||
		    (l.kind != "centre_x_pct" && l.kind != "width_follow")) {
			DebugLogger::LogWarningFormat("OverlayControls", "Layout part '%s': skipping an unusable link",
						      c.id.toUtf8().constData());
			continue;
		}
		r.links.append(l);
	}
	return true;
}

bool ParseControl(const QJsonObject &j, const QString &groupTab, Control &c, QString *why)
{
	c.id = j.value("id").toString();
	c.label = j.value("label").toString(c.id);
	c.help = j.value("help").toString();
	c.tab = j.value("tab").toString(groupTab);
	if (c.id.isEmpty()) {
		*why = "a control needs an id";
		return false;
	}
	const QString type = j.value("type").toString();
	QJsonValue def = j.value("default");
	if (type == "colour" || type == "color") {
		c.type = ControlType::Colour;
		if (def.isUndefined())
			def = "#FFFFFF";
	} else if (type == "slider" || type == "number") {
		c.type = type == "slider" ? ControlType::Slider : ControlType::Number;
		c.min = j.value("min").toDouble(0.0);
		c.max = j.value("max").toDouble(1.0);
		if (!(c.max > c.min)) {
			*why = QString("control \"%1\" needs max above min").arg(c.id);
			return false;
		}
		c.step = j.value("step").toDouble((c.max - c.min) / 100.0);
		if (!(c.step > 0.0))
			c.step = (c.max - c.min) / 100.0;
		c.decimals = std::clamp(j.value("decimals").toInt(2), 0, 6);
		c.unit = j.value("unit").toString();
		if (def.isUndefined())
			def = (c.min <= 1.0 && c.max >= 1.0) ? 1.0 : c.min;
	} else if (type == "toggle") {
		c.type = ControlType::Toggle;
		if (def.isUndefined())
			def = true;
	} else if (type == "text" || type == "textarea") {
		c.type = type == "text" ? ControlType::Text : ControlType::TextArea;
		if (def.isUndefined())
			def = QString();
	} else if (type == "select" || type == "font") {
		c.type = type == "select" ? ControlType::Select : ControlType::Font;
		c.editable = j.value("editable").toBool(type == "font");
		for (const QJsonValue ov : j.value("options").toArray()) {
			if (ov.isString())
				c.options.append(SelectOption{ov.toString(), ov.toString()});
			else
				c.options.append(SelectOption{ov.toObject().value("value").toString(),
							      ov.toObject().value("label").toString(
								      ov.toObject().value("value").toString())});
		}
		if (c.type == ControlType::Select && c.options.isEmpty() && !c.editable) {
			*why = QString("select \"%1\" has no options").arg(c.id);
			return false;
		}
		if (def.isUndefined())
			def = c.options.isEmpty() ? QString() : c.options.first().value;
	} else if (type == "platforms") {
		c.type = ControlType::Platforms;
		c.wordCase = j.value("case").toString();
		for (const QJsonValue pv : j.value("platforms").toArray()) {
			const QJsonObject pj = pv.toObject();
			PlatformDef p;
			p.id = pj.value("id").toString();
			p.label = pj.value("label").toString(p.id);
			p.icon = pj.value("icon").toString();
			p.chat = pj.value("chat").toBool(false);
			p.placeholder = pj.value("placeholder").toString();
			if (!p.id.isEmpty())
				c.platforms.append(p);
		}
		if (c.platforms.isEmpty()) {
			*why = QString("platforms \"%1\" lists no platforms").arg(c.id);
			return false;
		}
		if (def.isUndefined())
			def = QJsonArray();
	} else if (type == "layout") {
		c.type = ControlType::Layout;
		if (!ParseLayout(j, c, why))
			return false;
		def = QJsonObject{{"x", 0.0}, {"y", 0.0}, {"scale", 1.0}, {"visible", true}};
	} else {
		*why = QString("control \"%1\" has unknown type \"%2\"").arg(c.id, type);
		return false;
	}

	c.value = QJsonObject{{"x", 0.0}, {"y", 0.0}, {"scale", 1.0}, {"visible", true}}; // layout merge base
	c.defaultValue = NormaliseValue(c, def);
	if (c.defaultValue.isUndefined()) {
		*why = QString("control \"%1\" has an unusable default").arg(c.id);
		return false;
	}
	c.value = c.defaultValue;

	for (const QJsonValue bv : j.value("bindings").toArray()) {
		Binding b;
		QString bindWhy;
		if (!ParseBinding(bv.toObject(), b, &bindWhy)) {
			DebugLogger::LogWarningFormat("OverlayControls", "Control '%s': skipping a link, %s",
						      c.id.toUtf8().constData(), bindWhy.toUtf8().constData());
			continue;
		}
		c.bindings.append(b);
	}
	return true;
}

QStringList TemplateUses(const QString &templ)
{
	static const QRegularExpression token(QStringLiteral("\\{\\{\\s*([^{}|.]+)"));
	QStringList uses;
	auto it = token.globalMatch(templ);
	while (it.hasNext()) {
		const QString id = it.next().captured(1).trimmed();
		if (!id.startsWith('@') && !uses.contains(id))
			uses.append(id);
	}
	return uses;
}

bool ParseManifest(const QJsonObject &m, Overlay &o, QString *error)
{
	const int format = m.value("format").toInt(1);
	if (format > kManifestFormat) {
		*error = QString("This overlay needs a newer StreamUP plugin (controls format %1).").arg(format);
		return false;
	}
	o.id = m.value("id").toString().trimmed();
	o.name = m.value("name").toString(o.id);
	o.version = m.value("version").toString();
	o.description = m.value("description").toString();
	if (o.id.isEmpty()) {
		*error = "The overlay controls have no id.";
		return false;
	}
	for (const QJsonValue s : m.value("scenes").toArray())
		o.scenes.append(s.toString());

	for (const QJsonValue tv : m.value("tabs").toArray()) {
		const QJsonObject tj = tv.toObject();
		if (!tj.value("id").toString().isEmpty())
			o.tabs.append(TabInfo{tj.value("id").toString(), tj.value("label").toString(tj.value("id").toString())});
	}
	if (o.tabs.isEmpty())
		o.tabs.append(TabInfo{"look", "Look"});

	QSet<QString> ids;
	for (const QJsonValue gv : m.value("groups").toArray()) {
		const QJsonObject gj = gv.toObject();
		Group g;
		g.label = gj.value("label").toString();
		g.tab = gj.value("tab").toString(o.tabs.first().id);
		for (const QJsonValue cv : gj.value("controls").toArray()) {
			Control c;
			QString why;
			if (!ParseControl(cv.toObject(), g.tab, c, &why)) {
				DebugLogger::LogWarningFormat("OverlayControls", "Overlay '%s': skipping a control, %s",
							      o.id.toUtf8().constData(), why.toUtf8().constData());
				continue;
			}
			if (ids.contains(c.id)) {
				DebugLogger::LogWarningFormat("OverlayControls", "Overlay '%s': duplicate control id '%s'",
							      o.id.toUtf8().constData(), c.id.toUtf8().constData());
				continue;
			}
			ids.insert(c.id);
			g.controls.append(c);
		}
		if (!g.controls.isEmpty())
			o.groups.append(g);
	}
	if (o.groups.isEmpty()) {
		*error = QString("The overlay \"%1\" has no usable controls.").arg(o.name);
		return false;
	}

	for (const QJsonValue ov : m.value("outputs").toArray()) {
		const QJsonObject oj = ov.toObject();
		Output out;
		out.source = oj.value("source").toString();
		out.filter = oj.value("filter").toString();
		out.key = oj.value("key").toString();
		out.templ = oj.value("template").toString();
		out.uses = TemplateUses(out.templ);
		if (out.source.isEmpty() || out.key.isEmpty() || out.templ.isEmpty())
			continue;
		o.outputs.append(out);
	}

	for (const QJsonValue pv : m.value("presets").toArray()) {
		const QJsonObject pj = pv.toObject();
		Preset p;
		p.name = pj.value("name").toString();
		p.values = pj.value("values").toObject();
		if (!p.name.isEmpty())
			o.presets.append(p);
	}
	o.manifest = m;
	return true;
}

// ---------------------------------------------------------------- state (what the collection file keeps)

QJsonObject BindingState(const Binding &b)
{
	QJsonObject j{{"key", b.key}, {"source", b.source}, {"filter", b.filter}, {"captured", b.captured}};
	if (b.captured) {
		j["sourceUuid"] = b.sourceUuid;
		j["filterUuid"] = b.filterUuid;
		j["sceneUuid"] = b.sceneUuid;
		j["hadUserValue"] = b.hadUserValue;
		j["original"] = b.original;
		j["base"] = b.base;
		j["baseColour"] = b.baseColour;
	}
	return j;
}

void RestoreBindingState(Binding &b, const QJsonObject &j)
{
	if (j.value("key").toString() != b.key)
		return; // the manifest changed shape; capture this link fresh
	b.source = j.value("source").toString(b.source);
	b.filter = j.value("filter").toString(b.filter);
	b.captured = j.value("captured").toBool(false);
	if (b.captured) {
		b.sourceUuid = j.value("sourceUuid").toString();
		b.filterUuid = j.value("filterUuid").toString();
		b.sceneUuid = j.value("sceneUuid").toString();
		b.hadUserValue = j.value("hadUserValue").toBool();
		b.original = j.value("original").toObject();
		b.base = j.value("base").toDouble();
		b.baseColour = static_cast<qint64>(j.value("baseColour").toDouble());
	}
}

QJsonObject RectJson(const QRectF &r)
{
	return QJsonObject{{"x", r.x()}, {"y", r.y()}, {"w", r.width()}, {"h", r.height()}};
}

QRectF JsonRect(const QJsonObject &j)
{
	return QRectF(j.value("x").toDouble(), j.value("y").toDouble(), j.value("w").toDouble(), j.value("h").toDouble());
}

QJsonObject RigState(const Rig &r)
{
	QJsonArray items;
	for (const RigItem &it : r.items)
		items.append(QJsonObject{{"scene", it.scene},
					 {"source", it.source},
					 {"sceneUuid", it.sceneUuid},
					 {"sourceUuid", it.sourceUuid},
					 {"itemId", it.itemId},
					 {"captured", it.captured},
					 {"px", it.px},
					 {"py", it.py},
					 {"sx", it.sx},
					 {"sy", it.sy},
					 {"visible", it.visible},
					 {"hiddenByRig", it.hiddenByRig}});
	QJsonArray links;
	for (const GeoLink &l : r.links)
		links.append(QJsonObject{{"key", l.key},
					 {"source", l.source},
					 {"captured", l.captured},
					 {"sourceUuid", l.sourceUuid},
					 {"filterUuid", l.filterUuid},
					 {"hadUserValue", l.hadUserValue},
					 {"original", l.original},
					 {"base", l.base},
					 {"ofInstall", RectJson(l.ofInstall)},
					 {"withinInstall", RectJson(l.withinInstall)}});
	return QJsonObject{{"items", items},
			   {"frameCaptured", r.frameCaptured},
			   {"installFrame", RectJson(r.installFrame)},
			   {"frameScene", r.frameScene},
			   {"links", links}};
}

void RestoreRigState(Rig &r, const QJsonObject &j)
{
	// Only while the part still lists the same things; otherwise it is
	// measured again from scratch.
	QList<RigItem> items;
	for (const QJsonValue v : j.value("items").toArray()) {
		const QJsonObject ij = v.toObject();
		RigItem it;
		it.scene = ij.value("scene").toString();
		it.source = ij.value("source").toString();
		it.sceneUuid = ij.value("sceneUuid").toString();
		it.sourceUuid = ij.value("sourceUuid").toString();
		it.itemId = qint64(ij.value("itemId").toDouble());
		it.captured = ij.value("captured").toBool();
		it.px = ij.value("px").toDouble();
		it.py = ij.value("py").toDouble();
		it.sx = ij.value("sx").toDouble(1.0);
		it.sy = ij.value("sy").toDouble(1.0);
		it.visible = ij.value("visible").toBool(true);
		it.hiddenByRig = ij.value("hiddenByRig").toBool(false);
		if (!r.scenes.contains(it.scene) || !r.sources.contains(it.source))
			return;
		items.append(it);
	}
	r.items = items;
	r.frameCaptured = j.value("frameCaptured").toBool();
	r.installFrame = JsonRect(j.value("installFrame").toObject());
	r.frameScene = j.value("frameScene").toString();
	const QJsonArray links = j.value("links").toArray();
	for (int i = 0; i < r.links.size() && i < links.size(); ++i) {
		const QJsonObject lj = links.at(i).toObject();
		GeoLink &l = r.links[i];
		if (lj.value("key").toString() != l.key || lj.value("source").toString() != l.source)
			continue;
		l.captured = lj.value("captured").toBool();
		l.sourceUuid = lj.value("sourceUuid").toString();
		l.filterUuid = lj.value("filterUuid").toString();
		l.hadUserValue = lj.value("hadUserValue").toBool();
		l.original = lj.value("original").toObject();
		l.base = lj.value("base").toDouble();
		l.ofInstall = JsonRect(lj.value("ofInstall").toObject());
		l.withinInstall = JsonRect(lj.value("withinInstall").toObject());
	}
}

QJsonObject OverlayState(const Overlay &o)
{
	QJsonObject controls;
	for (const Group &g : o.groups) {
		for (const Control &c : g.controls) {
			QJsonArray bindings;
			for (const Binding &b : c.bindings)
				bindings.append(BindingState(b));
			QJsonObject cj{{"value", c.value}, {"applied", c.applied}, {"bindings", bindings}};
			if (c.type == ControlType::Layout)
				cj["rig"] = RigState(c.rig);
			controls[c.id] = cj;
		}
	}
	QJsonArray userPresets;
	for (const Preset &p : o.presets)
		if (!p.builtIn)
			userPresets.append(QJsonObject{{"name", p.name}, {"values", p.values}});
	QJsonArray sceneUuids;
	for (const QString &u : o.sceneUuids)
		sceneUuids.append(u);
	return QJsonObject{{"controls", controls},
			   {"userPresets", userPresets},
			   {"sceneUuids", sceneUuids},
			   {"resourceDir", o.resourceDir},
			   {"snapshot", SnapshotToJson(o)}};
}

void RestoreOverlayState(Overlay &o, const QJsonObject &state)
{
	const QJsonObject controls = state.value("controls").toObject();
	for (Group &g : o.groups) {
		for (Control &c : g.controls) {
			const QJsonObject cj = controls.value(c.id).toObject();
			if (cj.isEmpty())
				continue;
			const QJsonValue v = NormaliseValue(c, cj.value("value"));
			if (!v.isUndefined())
				c.value = v;
			c.applied = cj.value("applied").toBool(false);
			const QJsonArray bindings = cj.value("bindings").toArray();
			for (int i = 0; i < c.bindings.size() && i < bindings.size(); ++i)
				RestoreBindingState(c.bindings[i], bindings.at(i).toObject());
			if (c.type == ControlType::Layout)
				RestoreRigState(c.rig, cj.value("rig").toObject());
		}
	}
	for (const QJsonValue pv : state.value("userPresets").toArray()) {
		const QJsonObject pj = pv.toObject();
		Preset p{pj.value("name").toString(), false, pj.value("values").toObject()};
		if (!p.name.isEmpty())
			o.presets.append(p);
	}
	for (const QJsonValue u : state.value("sceneUuids").toArray())
		o.sceneUuids.append(u.toString());
	o.resourceDir = state.value("resourceDir").toString();
	SnapshotFromJson(o, state.value("snapshot").toObject());
}

// Carries install values, snapshot and settings across a reinstall of the
// same overlay. Links are matched on what they point at, so a manifest that
// gained or lost links between versions keeps everything it can.
void CarryOver(Overlay &fresh, const Overlay &old)
{
	for (Group &g : fresh.groups) {
		for (Control &c : g.controls) {
			const Control *oldControl = nullptr;
			for (const Group &og : old.groups)
				for (const Control &oc : og.controls)
					if (oc.id == c.id && oc.type == c.type)
						oldControl = &oc;
			if (!oldControl)
				continue;
			const QJsonValue v = NormaliseValue(c, oldControl->value);
			if (!v.isUndefined())
				c.value = v;
			c.applied = oldControl->applied;
			for (Binding &b : c.bindings) {
				for (const Binding &ob : oldControl->bindings) {
					if (ob.key == b.key && ob.filter == b.filter && ob.source == b.source &&
					    ob.scene == b.scene && ob.captured) {
						const Binding keep = b;
						b = ob;
						// The manifest's own options win; only captured state carries.
						b.mode = keep.mode;
						b.mapTo0 = keep.mapTo0;
						b.mapTo1 = keep.mapTo1;
						b.clamp = keep.clamp;
						b.clampMin = keep.clampMin;
						b.clampMax = keep.clampMax;
						b.integer = keep.integer;
						b.invert = keep.invert;
						b.lighten = keep.lighten;
						b.textCase = keep.textCase;
						b.letterSpace = keep.letterSpace;
						break;
					}
				}
			}
			if (c.type == ControlType::Layout && oldControl->rig.scenes == c.rig.scenes &&
			    oldControl->rig.sources == c.rig.sources) {
				c.rig.items = oldControl->rig.items;
				c.rig.frameCaptured = oldControl->rig.frameCaptured;
				c.rig.installFrame = oldControl->rig.installFrame;
				c.rig.frameScene = oldControl->rig.frameScene;
				for (GeoLink &l : c.rig.links)
					for (const GeoLink &ol : oldControl->rig.links)
						if (ol.key == l.key && ol.source == l.source && ol.filter == l.filter)
							l = ol;
			}
		}
	}
	for (const Preset &p : old.presets)
		if (!p.builtIn)
			fresh.presets.append(p);
	if (old.snapshotTaken) {
		fresh.snapshotTaken = true;
		fresh.snapSources = old.snapSources;
		fresh.snapItems = old.snapItems;
	}
}

void CaptureAll(Overlay &o)
{
	for (Group &g : o.groups) {
		for (Control &c : g.controls) {
			for (Binding &b : c.bindings) {
				if (b.captured)
					continue;
				if (!CaptureBinding(b) && !BindingTargetExists(b))
					DebugLogger::LogWarningFormat("OverlayControls",
								      "Overlay '%s': '%s' / '%s' not found for control '%s'",
								      o.id.toUtf8().constData(), b.source.toUtf8().constData(),
								      b.filter.toUtf8().constData(), c.id.toUtf8().constData());
			}
			if (c.type == ControlType::Layout)
				CaptureRig(o, c);
		}
	}
	o.sceneUuids.clear();
	for (const QString &name : o.scenes) {
		obs_source_t *scene = obs_get_source_by_name(name.toUtf8().constData());
		if (scene) {
			o.sceneUuids.append(QString::fromUtf8(obs_source_get_uuid(scene)));
			obs_source_release(scene);
		}
	}
	TakeSnapshot(o);
}

bool RegisterManifest(const QJsonObject &manifest, const QString &resourceDir, QString *overlayId, QString *error)
{
	Overlay fresh;
	QString why;
	if (!ParseManifest(manifest, fresh, &why)) {
		if (error)
			*error = why;
		DebugLogger::LogWarningFormat("OverlayControls", "Not registered: %s", why.toUtf8().constData());
		return false;
	}
	fresh.resourceDir = resourceDir;
	{
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		Overlay *old = FindOverlay(fresh.id);
		if (old)
			CarryOver(fresh, *old);
		CaptureAll(fresh);
		// Templates are rendered straight away: they point the overlay at
		// this install's Resources folder, whatever paths the file shipped.
		RenderOutputs(fresh);
		ApplyLayout(fresh);
		if (old)
			*old = fresh;
		else
			g_overlays.append(fresh);
	}
	if (overlayId)
		*overlayId = fresh.id;
	DebugLogger::LogInfoFormat("OverlayControls", "Registered overlay '%s' (%s)", fresh.name.toUtf8().constData(),
				   fresh.id.toUtf8().constData());
	emit g_notifier->overlaysChanged();
	return true;
}

// Read with Qt's parser straight from the file, never through obs_data: OBS's
// JSON reader keeps only arrays of objects and silently drops arrays of plain
// values, and a manifest is full of those (scene lists, clamps, font options).
QJsonObject ManifestFromFile(const QString &path, bool *fileHasBlock = nullptr)
{
	QFile f(path);
	if (fileHasBlock)
		*fileHasBlock = false;
	if (!f.open(QIODevice::ReadOnly))
		return QJsonObject();
	const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
	if (fileHasBlock)
		*fileHasBlock = root.contains(kFileKey);
	return root.value(kFileKey).toObject();
}

QString ResourcesBeside(const QString &filePath)
{
	return QFileInfo(filePath).absolutePath() + "/Resources";
}

// ---------------------------------------------------------------- info snapshots

ControlInfo MakeInfo(Overlay &o, Control &c)
{
	ControlInfo i;
	i.id = c.id;
	i.label = c.label;
	i.help = c.help;
	i.tab = c.tab;
	i.type = c.type;
	i.min = c.min;
	i.max = c.max;
	i.step = c.step;
	i.decimals = c.decimals;
	i.unit = c.unit;
	i.editable = c.editable;
	i.options = c.options;
	i.platforms = c.platforms;
	i.value = c.value;
	i.defaultValue = c.defaultValue;
	i.applied = c.applied;
	i.links = c.bindings.size();
	for (Binding &b : c.bindings)
		if (!BindingTargetExists(b))
			++i.missing;
	if (c.type == ControlType::Layout) {
		CaptureRig(o, c);
		i.layout.scenes = c.rig.scenes;
		i.layout.parent = c.rig.parent;
		i.layout.scaleMin = c.rig.scaleMin;
		i.layout.scaleMax = c.rig.scaleMax;
		i.layout.movable = c.rig.movable;
		i.layout.installFrame = c.rig.installFrame;
		i.layout.frameScene = c.rig.frameScene;
		i.layout.frame = FrameInScene(c, c.rig.scenes.value(0));
		i.links += c.rig.items.size();
		i.missing += RigMissing(c);
	}
	return i;
}

OverlayInfo MakeInfo(Overlay &o)
{
	OverlayInfo info;
	info.id = o.id;
	info.name = o.name;
	info.version = o.version;
	info.description = o.description;
	info.scenes = o.scenes;
	info.tabs = o.tabs;
	for (Group &g : o.groups) {
		GroupInfo gi;
		gi.label = g.label;
		gi.tab = g.tab;
		for (Control &c : g.controls) {
			ControlInfo ci = MakeInfo(o, c);
			info.links += ci.links;
			info.missing += ci.missing;
			gi.controls.append(ci);
		}
		info.groups.append(gi);
	}
	for (const Preset &p : o.presets)
		info.presets.append(PresetInfo{p.name, p.builtIn});
	return info;
}

// Writes a control's value everywhere it goes. Assumes the lock is held.
void Apply(Overlay &o, Control &c)
{
	if (c.type == ControlType::Layout) {
		ApplyLayout(o, c.id);
	} else {
		for (Binding &b : c.bindings)
			WriteBinding(b, c);
	}
	RenderOutputs(o, c.id);
}

// ---------------------------------------------------------------- frontend hooks

void OnSaveLoad(obs_data_t *moduleData, bool saving, void *)
{
	if (saving) {
		if (!moduleData)
			return;
		QJsonArray overlays;
		{
			std::lock_guard<std::recursive_mutex> lock(g_mutex);
			for (const Overlay &o : g_overlays)
				overlays.append(QJsonObject{{"manifest", o.manifest}, {"state", OverlayState(o)}});
		}
		if (overlays.isEmpty()) {
			obs_data_erase(moduleData, kModuleKey);
			return;
		}
		// Stored as a JSON string, not an object: OBS would re-parse an
		// object and drop every array of plain values in the manifest.
		const QByteArray json = QJsonDocument(QJsonObject{{"overlays", overlays}}).toJson(QJsonDocument::Compact);
		obs_data_set_string(moduleData, kModuleKey, json.constData());
		return;
	}

	// Loading a collection: whatever the last one had is no longer ours.
	QList<Overlay> loaded;
	if (moduleData) {
		const QByteArray json(obs_data_get_string(moduleData, kModuleKey));
		if (!json.isEmpty()) {
			const QJsonObject root = QJsonDocument::fromJson(json).object();
			for (const QJsonValue ov : root.value("overlays").toArray()) {
				const QJsonObject oj = ov.toObject();
				Overlay o;
				QString why;
				if (!ParseManifest(oj.value("manifest").toObject(), o, &why)) {
					DebugLogger::LogWarningFormat("OverlayControls", "Dropping saved overlay: %s",
								      why.toUtf8().constData());
					continue;
				}
				RestoreOverlayState(o, oj.value("state").toObject());
				loaded.append(o);
			}
		}
	}
	{
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		g_overlays = loaded;
	}
	emit g_notifier->overlaysChanged();
}

void OnFrontendEvent(enum obs_frontend_event event, void *)
{
	// OBS saves the collection before either of these, so clearing is safe.
	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP || event == OBS_FRONTEND_EVENT_EXIT) {
		{
			std::lock_guard<std::recursive_mutex> lock(g_mutex);
			g_overlays.clear();
		}
		emit g_notifier->overlaysChanged();
	}
}

void RequestSave()
{
	// Queued by the frontend itself, so this is fine from any thread.
	obs_frontend_save();
}

} // namespace

// ================================================================ public API

Notifier *GetNotifier()
{
	return g_notifier;
}

void Initialise()
{
	if (!g_notifier)
		g_notifier = new Notifier();
	obs_frontend_add_save_callback(OnSaveLoad, nullptr);
	obs_frontend_add_event_callback(OnFrontendEvent, nullptr);
}

void Shutdown()
{
	obs_frontend_remove_save_callback(OnSaveLoad, nullptr);
	obs_frontend_remove_event_callback(OnFrontendEvent, nullptr);
	std::lock_guard<std::recursive_mutex> lock(g_mutex);
	g_overlays.clear();
}

bool RegisterFromStreamupData(obs_data_t *fileData, const QString &filePath, QString *overlayId, QString *error)
{
	// fileData is only a cheap "is there a block at all" check; the block is
	// read from the file itself (see ManifestFromFile).
	obs_data_t *block = fileData ? obs_data_get_obj(fileData, kFileKey) : nullptr;
	if (!block)
		return false;
	obs_data_release(block);
	const QJsonObject manifest = ManifestFromFile(filePath);
	if (manifest.isEmpty()) {
		if (error)
			*error = QString("Could not read the overlay controls in %1.").arg(QFileInfo(filePath).fileName());
		return false;
	}
	if (!RegisterManifest(manifest, ResourcesBeside(filePath), overlayId, error))
		return false;
	RequestSave();
	return true;
}

bool RegisterFromFile(const QString &path, QString *overlayId, QString *error)
{
	const QJsonObject manifest = ManifestFromFile(path);
	if (manifest.isEmpty()) {
		if (error)
			*error = QString("%1 has no overlay controls in it.").arg(QFileInfo(path).fileName());
		return false;
	}
	if (!RegisterManifest(manifest, ResourcesBeside(path), overlayId, error))
		return false;
	RequestSave();
	return true;
}

bool RemoveOverlay(const QString &overlayId)
{
	bool removed = false;
	{
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		for (int i = 0; i < g_overlays.size(); ++i) {
			if (g_overlays[i].id == overlayId) {
				g_overlays.removeAt(i);
				removed = true;
				break;
			}
		}
	}
	if (removed) {
		emit g_notifier->overlaysChanged();
		RequestSave();
	}
	return removed;
}

QList<OverlayInfo> ListOverlays()
{
	std::lock_guard<std::recursive_mutex> lock(g_mutex);
	QList<OverlayInfo> out;
	for (Overlay &o : g_overlays)
		out.append(MakeInfo(o));
	return out;
}

bool GetOverlay(const QString &overlayId, OverlayInfo &out)
{
	std::lock_guard<std::recursive_mutex> lock(g_mutex);
	Overlay *o = FindOverlay(overlayId);
	if (!o)
		return false;
	out = MakeInfo(*o);
	return true;
}

bool GetControlValue(const QString &overlayId, const QString &controlId, QJsonValue &out)
{
	std::lock_guard<std::recursive_mutex> lock(g_mutex);
	Overlay *o = FindOverlay(overlayId);
	Control *c = o ? FindControl(*o, controlId) : nullptr;
	if (!c)
		return false;
	out = c->value;
	return true;
}

bool SetControlValue(const QString &overlayId, const QString &controlId, const QJsonValue &value, QString *error)
{
	{
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		Overlay *o = FindOverlay(overlayId);
		if (!o) {
			if (error)
				*error = QString("No overlay called \"%1\".").arg(overlayId);
			return false;
		}
		Control *c = FindControl(*o, controlId);
		if (!c) {
			if (error)
				*error = QString("\"%1\" has no control \"%2\".").arg(o->name, controlId);
			return false;
		}
		const QJsonValue v = NormaliseValue(*c, value);
		if (v.isUndefined()) {
			if (error)
				*error = QString("That value does not fit the \"%1\" control.").arg(c->label);
			return false;
		}
		c->value = v;
		c->applied = true;
		Apply(*o, *c);
	}
	emit g_notifier->controlChanged(overlayId, controlId);
	return true;
}

bool ResetControl(const QString &overlayId, const QString &controlId)
{
	{
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		Overlay *o = FindOverlay(overlayId);
		Control *c = o ? FindControl(*o, controlId) : nullptr;
		if (!c)
			return false;
		for (Binding &b : c->bindings)
			RestoreBinding(b);
		c->value = c->defaultValue;
		c->applied = false;
		if (c->type == ControlType::Layout)
			ApplyLayout(*o, c->id);
		RenderOutputs(*o, c->id);
	}
	emit g_notifier->controlChanged(overlayId, controlId);
	return true;
}

bool ResetAll(const QString &overlayId)
{
	{
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		Overlay *o = FindOverlay(overlayId);
		if (!o)
			return false;
		for (Group &g : o->groups) {
			for (Control &c : g.controls) {
				for (Binding &b : c.bindings)
					RestoreBinding(b);
				c.value = c.defaultValue;
				c.applied = false;
			}
		}
		if (o->snapshotTaken) {
			RestoreSnapshot(*o);
		} else {
			RestoreGeoLinks(*o);
			ApplyLayout(*o);
		}
		RenderOutputs(*o);
	}
	emit g_notifier->overlayReset(overlayId);
	return true;
}

bool ApplyPreset(const QString &overlayId, const QString &presetName, QString *error)
{
	QStringList changed;
	{
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		Overlay *o = FindOverlay(overlayId);
		if (!o) {
			if (error)
				*error = QString("No overlay called \"%1\".").arg(overlayId);
			return false;
		}
		const Preset *preset = nullptr;
		for (const Preset &p : o->presets)
			if (p.name.compare(presetName, Qt::CaseInsensitive) == 0)
				preset = &p;
		if (!preset) {
			if (error)
				*error = QString("\"%1\" has no preset called \"%2\".").arg(o->name, presetName);
			return false;
		}
		const QJsonObject values = preset->values;
		bool layoutChanged = false;
		for (auto it = values.begin(); it != values.end(); ++it) {
			Control *c = FindControl(*o, it.key());
			if (!c)
				continue;
			const QJsonValue v = NormaliseValue(*c, it.value());
			if (v.isUndefined())
				continue;
			c->value = v;
			c->applied = true;
			if (c->type == ControlType::Layout) {
				layoutChanged = true;
			} else {
				for (Binding &b : c->bindings)
					WriteBinding(b, *c);
			}
			changed.append(c->id);
		}
		if (layoutChanged)
			ApplyLayout(*o);
		RenderOutputs(*o);
	}
	for (const QString &id : changed)
		emit g_notifier->controlChanged(overlayId, id);
	return true;
}

bool SavePreset(const QString &overlayId, const QString &presetName, QString *error)
{
	const QString name = presetName.trimmed();
	if (name.isEmpty()) {
		if (error)
			*error = "Give the preset a name.";
		return false;
	}
	{
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		Overlay *o = FindOverlay(overlayId);
		if (!o)
			return false;
		for (const Preset &p : o->presets) {
			if (p.builtIn && p.name.compare(name, Qt::CaseInsensitive) == 0) {
				if (error)
					*error = QString("\"%1\" is a built-in preset. Pick another name.").arg(p.name);
				return false;
			}
		}
		QJsonObject values;
		for (const Group &g : o->groups)
			for (const Control &c : g.controls)
				values[c.id] = c.value;
		bool replaced = false;
		for (Preset &p : o->presets) {
			if (!p.builtIn && p.name.compare(name, Qt::CaseInsensitive) == 0) {
				p.values = values;
				replaced = true;
			}
		}
		if (!replaced)
			o->presets.append(Preset{name, false, values});
	}
	emit g_notifier->overlaysChanged();
	RequestSave();
	return true;
}

bool DeletePreset(const QString &overlayId, const QString &presetName)
{
	bool removed = false;
	{
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		Overlay *o = FindOverlay(overlayId);
		if (!o)
			return false;
		for (int i = 0; i < o->presets.size(); ++i) {
			if (!o->presets[i].builtIn && o->presets[i].name == presetName) {
				o->presets.removeAt(i);
				removed = true;
				break;
			}
		}
	}
	if (removed) {
		emit g_notifier->overlaysChanged();
		RequestSave();
	}
	return removed;
}

QStringList DriftedControls(const QString &overlayId)
{
	std::lock_guard<std::recursive_mutex> lock(g_mutex);
	QStringList out;
	Overlay *o = FindOverlay(overlayId);
	if (!o)
		return out;
	for (Group &g : o->groups) {
		for (Control &c : g.controls) {
			if (!c.applied)
				continue;
			for (Binding &b : c.bindings) {
				if (BindingDrifted(b, c)) {
					out.append(c.id);
					break;
				}
			}
		}
	}
	return out;
}

QString OverlayForScene(obs_source_t *scene)
{
	if (!scene)
		return QString();
	const QString uuid = QString::fromUtf8(obs_source_get_uuid(scene));
	const QString name = QString::fromUtf8(obs_source_get_name(scene));
	std::lock_guard<std::recursive_mutex> lock(g_mutex);
	for (const Overlay &o : g_overlays)
		if (o.sceneUuids.contains(uuid) || o.scenes.contains(name))
			return o.id;
	return QString();
}

QRectF LayoutFrame(const QString &overlayId, const QString &controlId, const QString &sceneName)
{
	std::lock_guard<std::recursive_mutex> lock(g_mutex);
	Overlay *o = FindOverlay(overlayId);
	Control *c = o ? FindControl(*o, controlId) : nullptr;
	if (!c || c->type != ControlType::Layout)
		return QRectF();
	CaptureRig(*o, *c);
	return FrameInScene(*c, sceneName);
}

bool ResetSource(const QString &overlayId, const QString &sourceName)
{
	{
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		Overlay *o = FindOverlay(overlayId);
		if (!o || !o->snapshotTaken)
			return false;
		RestoreSnapshot(*o, sourceName);
		// The panel's own controls stay in charge: a moved part stays moved
		// and a nameplate keeps the platforms it was given.
		ApplyLayout(*o);
		RenderOutputs(*o);
	}
	emit g_notifier->sourceReset(overlayId, sourceName);
	return true;
}

bool HasSnapshot(const QString &overlayId, const QString &sourceName)
{
	std::lock_guard<std::recursive_mutex> lock(g_mutex);
	Overlay *o = FindOverlay(overlayId);
	if (!o || !o->snapshotTaken)
		return false;
	for (const SnapSource &s : o->snapSources)
		if (s.name == sourceName)
			return true;
	return false;
}

QString FormatValue(const ControlInfo &control, const QJsonValue &value)
{
	switch (control.type) {
	case ControlType::Colour:
		return value.toString();
	case ControlType::Slider:
	case ControlType::Number:
		return QString::number(value.toDouble(), 'f', control.decimals) + control.unit;
	case ControlType::Toggle:
		return value.toBool() ? QStringLiteral("On") : QStringLiteral("Off");
	case ControlType::Text:
	case ControlType::TextArea:
	case ControlType::Select:
	case ControlType::Font:
		return value.toString();
	case ControlType::Platforms:
		return QString::number(value.toArray().size());
	case ControlType::Layout: {
		const QJsonObject v = value.toObject();
		return QString("%1, %2 @ %3x")
			.arg(value.toObject().value("x").toDouble(), 0, 'f', 0)
			.arg(v.value("y").toDouble(), 0, 'f', 0)
			.arg(v.value("scale").toDouble(1.0), 0, 'f', 2);
	}
	}
	return QString();
}

} // namespace OverlayControls
} // namespace StreamUP
