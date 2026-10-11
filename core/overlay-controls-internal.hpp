#pragma once

// Private to core/overlay-controls*.cpp. The data model behind the public API,
// and the functions that touch OBS (overlay-apply.cpp). Everything here assumes
// the caller holds the registry lock.

#include "overlay-controls.hpp"

#include <obs.h>

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace StreamUP {
namespace OverlayControls {
namespace Internal {

constexpr const char *kEnabledKey = "@enabled"; // filter or source enabled state
constexpr const char *kVisibleKey = "@visible"; // scene item visibility (link has a scene)

enum class BindMode { Set, Multiply, Map };

// One setting a control writes.
struct Binding {
	// From the manifest.
	QString source;
	QString filter;   // empty: the source's own settings
	QString scene;    // set: a scene item in this scene (key must be @visible)
	QString key;      // a setting key ("a.b" reaches into an object), @enabled or @visible
	BindMode mode = BindMode::Set;
	double mapTo0 = 0.0;
	double mapTo1 = 1.0;
	bool clamp = false;
	double clampMin = 0.0;
	double clampMax = 0.0;
	bool integer = false;
	bool invert = false;
	double lighten = 0.0;   // colours: >0 mixes toward white, <0 toward black
	QString textCase;       // strings: "upper" / "lower"
	bool letterSpace = false; // strings: "BE RIGHT" -> "B E   R I G H T"

	// Captured the first time the target is found, normally at install.
	bool captured = false;
	QString sourceUuid;
	QString filterUuid;
	QString sceneUuid;
	bool hadUserValue = false;
	QJsonObject original; // {"t": "i"|"d"|"b"|"s"|"o", "v": value}
	double base = 0.0;
	qint64 baseColour = 0;
};

// A setting derived from where things sit on the canvas, e.g. a ring's
// cut-out centred on its nameplate. Recomputed after every layout change.
struct GeoLink {
	QString source;
	QString filter;
	QString key;
	QString kind;   // "centre_x_pct" | "width_follow"
	QString scene;  // where "of" and "within" are measured
	QString of;     // the item followed
	QString within; // the item the value is relative to

	bool captured = false;
	QString sourceUuid;
	QString filterUuid;
	bool hadUserValue = false;
	QJsonObject original;
	double base = 0.0;
	QRectF ofInstall;     // boxes at install, for width_follow
	QRectF withinInstall;
};

// One scene item a layout part moves.
struct RigItem {
	QString scene;
	QString source;
	QString sceneUuid;
	QString sourceUuid;
	qint64 itemId = 0;
	bool captured = false;
	double px = 0, py = 0; // install position
	double sx = 1, sy = 1; // install scale
	bool visible = true;   // install visibility
	bool hiddenByRig = false; // this part hid it, so this part may show it again
};

struct Rig {
	QStringList scenes;
	QStringList sources;
	QStringList frameSources; // items whose boxes define the frame (all when empty)
	QString parent;
	double scaleMin = 1.0;
	double scaleMax = 1.0;
	bool movable = true;
	QList<RigItem> items;
	bool frameCaptured = false;
	QRectF installFrame;
	QString frameScene;
	QList<GeoLink> links;
};

struct Control {
	QString id;
	QString label;
	QString help;
	QString tab;
	ControlType type = ControlType::Slider;
	double min = 0.0;
	double max = 1.0;
	double step = 0.01;
	int decimals = 2;
	QString unit;
	bool editable = false;
	QList<SelectOption> options;
	QList<PlatformDef> platforms;
	QString wordCase; // platforms: case of the names in a nameplate
	Rig rig;
	QJsonValue defaultValue;
	QJsonValue value;
	bool applied = false;
	QList<Binding> bindings;
};

struct Group {
	QString label;
	QString tab;
	QList<Control> controls;
};

struct Preset {
	QString name;
	bool builtIn = true;
	QJsonObject values;
};

// A setting built from a template over control values.
struct Output {
	QString source;
	QString filter;
	QString key;
	QString templ;
	QStringList uses; // control ids the template reads
	QString sourceUuid;
	QString filterUuid;
};

struct SnapFilter {
	QString name;
	QString uuid;
	QJsonObject settings;
	bool enabled = true;
};

struct SnapSource {
	QString name;
	QString uuid;
	QString kind;
	QJsonObject settings;
	QList<SnapFilter> filters;
};

struct SnapItem {
	QString scene;
	QString sceneUuid;
	QString source;
	qint64 itemId = 0;
	QJsonObject transform; // pos, rot, scale, alignment, bounds, crop, visible
};

struct Overlay {
	QString id;
	QString name;
	QString version;
	QString description;
	QString resourceDir; // absolute path of the product's Resources folder
	QStringList scenes;
	QStringList sceneUuids;
	QList<TabInfo> tabs;
	QList<Group> groups;
	QList<Preset> presets;
	QList<Output> outputs;
	bool snapshotTaken = false;
	QList<SnapSource> snapSources;
	QList<SnapItem> snapItems;
	QJsonObject manifest;
};

// ---------------------------------------------------------------- values (overlay-apply.cpp)

QColor ParseColour(const QString &text);
QString ColourText(const QColor &c);
QColor FromObsColour(qint64 v);
qint64 ToObsColour(const QColor &c);
bool IsNumeric(ControlType t);
bool IsString(ControlType t);
QJsonValue NormaliseValue(const Control &c, const QJsonValue &in);

// ---------------------------------------------------------------- settings links

obs_source_t *ResolveTarget(QString &source, QString &sourceUuid, QString &filter, QString &filterUuid);
bool BindingTargetExists(Binding &b);
bool CaptureBinding(Binding &b);
bool WriteBinding(Binding &b, const Control &c);
void RestoreBinding(Binding &b);
bool BindingDrifted(Binding &b, const Control &c);

// ---------------------------------------------------------------- templates

void RenderOutputs(Overlay &o, const QString &changedControl = QString());

// ---------------------------------------------------------------- layout

bool CaptureRig(Overlay &o, Control &c);
// Places the items of one part and every part riding on it (all parts when
// changedPart is empty), then recomputes geometry links.
void ApplyLayout(Overlay &o, const QString &changedPart = QString());
QRectF CurrentFrame(Overlay &o, Control &c);
QRectF FrameInScene(Control &c, const QString &scene);
int RigMissing(Control &c);
void RestoreGeoLinks(Overlay &o);

// ---------------------------------------------------------------- snapshot

void TakeSnapshot(Overlay &o);
void RestoreSnapshot(Overlay &o, const QString &onlySource = QString());

// ---------------------------------------------------------------- persistence helpers

QJsonObject SnapshotToJson(const Overlay &o);
void SnapshotFromJson(Overlay &o, const QJsonObject &j);

} // namespace Internal
} // namespace OverlayControls
} // namespace StreamUP
