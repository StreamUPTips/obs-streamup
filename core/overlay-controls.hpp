#pragma once

// Overlay Controls: one panel for everything about an overlay.
//
// A .StreamUP file can carry a "streamup_overlay" block next to its sources. It
// names controls and, for each, the settings it drives:
//
//   colour, slider, number, toggle  write to filter or source settings
//   text, textarea, select, font    write strings (titles, fonts, handles)
//   platforms                       a list of socials that templates render into
//                                   nameplate sequences and chat channel lists
//   layout                          a "part" of the overlay (a camera panel with
//                                   its ring, glass and nameplate) moved and
//                                   scaled as one, with geometry links that keep
//                                   derived settings (a ring's cut-out) in step
//
// "outputs" are templates: a setting whose value is built from several
// controls, e.g. a nameplate's ProText markup from the platform list.
//
// Every link records the value its setting had at install; "multiply" sliders
// scale each panel's own value and Reset puts things back. On top of that the
// whole overlay (every source, filter and scene item in its scenes) is
// snapshotted at install, so any single source can be put back as it shipped.
//
// The registry travels inside the scene collection file (the frontend save
// callback). Everything here is safe to call from the obs-websocket thread; the
// window hears about changes through the Notifier's signals.

#include <obs.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace StreamUP {
namespace OverlayControls {

enum class ControlType { Colour, Slider, Number, Toggle, Text, TextArea, Select, Font, Platforms, Layout };

struct SelectOption {
	QString value;
	QString label;
};

struct PlatformDef {
	QString id;
	QString label;
	QString icon;       // path inside the product's Resources folder, e.g. "Icons/twitch.svg"
	bool chat = false;  // the chat source can read this platform
	QString placeholder;
};

struct LayoutInfo {
	QStringList scenes;
	QString parent;     // id of the part this one rides on, or empty
	double scaleMin = 1.0;
	double scaleMax = 1.0;
	bool movable = true;
	QRectF frame;       // current frame on the canvas, first scene
	QRectF installFrame;
	QString frameScene; // the scene the frame was measured in
};

struct ControlInfo {
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
	bool editable = false;             // select/font: free text allowed
	QList<SelectOption> options;       // select
	QList<PlatformDef> platforms;      // platforms
	LayoutInfo layout;                 // layout
	QJsonValue value;                  // shape depends on type, see FormatValue
	QJsonValue defaultValue;
	bool applied = false;              // written since install or the last reset
	int links = 0;
	int missing = 0;                   // links whose target no longer exists
};

struct GroupInfo {
	QString label;
	QString tab;
	QList<ControlInfo> controls;
};

struct TabInfo {
	QString id;
	QString label;
};

struct PresetInfo {
	QString name;
	bool builtIn = true;
};

struct OverlayInfo {
	QString id;
	QString name;
	QString version;
	QString description;
	QStringList scenes;
	QList<TabInfo> tabs;
	QList<GroupInfo> groups;
	QList<PresetInfo> presets;
	int links = 0;
	int missing = 0;
};

// Signals fire on whichever thread made the change; receivers in the UI thread
// get them queued.
class Notifier : public QObject {
	Q_OBJECT
signals:
	void overlaysChanged();
	void controlChanged(const QString &overlayId, const QString &controlId);
	void overlayReset(const QString &overlayId);
	void sourceReset(const QString &overlayId, const QString &sourceName);
};

Notifier *GetNotifier();

void Initialise();
void Shutdown();

// Reads the "streamup_overlay" block of a parsed .StreamUP file and registers
// it against the sources now in OBS. filePath is the .StreamUP file, whose
// folder holds the product's Resources. Returns false (no error) when the file
// has no such block. Reinstalling keeps install values and current settings.
bool RegisterFromStreamupData(obs_data_t *fileData, const QString &filePath, QString *overlayId = nullptr,
			      QString *error = nullptr);

// Attaches controls from a .StreamUP file to sources that already exist,
// without loading any sources.
bool RegisterFromFile(const QString &path, QString *overlayId = nullptr, QString *error = nullptr);

bool RemoveOverlay(const QString &overlayId);

QList<OverlayInfo> ListOverlays();
bool GetOverlay(const QString &overlayId, OverlayInfo &out);
bool GetControlValue(const QString &overlayId, const QString &controlId, QJsonValue &out);

// value shapes:
//   colour     "#RRGGBB" / "#RRGGBBAA" (or an OBS colour integer)
//   slider/number  a number, clamped to the range
//   toggle     bool, or "toggle"
//   text/textarea/select/font  a string
//   platforms  [{"platform": id, "username": s, "nameplate": b, "chat": b}]
//   layout     {"x": px, "y": px, "scale": s, "visible": b} (any subset)
bool SetControlValue(const QString &overlayId, const QString &controlId, const QJsonValue &value,
		     QString *error = nullptr);
bool ResetControl(const QString &overlayId, const QString &controlId);

// Everything back as installed: every source, filter and scene item, then
// every control to its default.
bool ResetAll(const QString &overlayId);

bool ApplyPreset(const QString &overlayId, const QString &presetName, QString *error = nullptr);
bool SavePreset(const QString &overlayId, const QString &presetName, QString *error = nullptr);
bool DeletePreset(const QString &overlayId, const QString &presetName);

QStringList DriftedControls(const QString &overlayId);

QString OverlayForScene(obs_source_t *scene);

// Current frame of a layout part in one of its scenes (canvas pixels). Empty
// when the part has nothing in that scene.
QRectF LayoutFrame(const QString &overlayId, const QString &controlId, const QString &sceneName);

// Snapshot-backed reset of a single source: its settings, its filters and its
// transform in every overlay scene.
bool ResetSource(const QString &overlayId, const QString &sourceName);
bool HasSnapshot(const QString &overlayId, const QString &sourceName);

QString FormatValue(const ControlInfo &control, const QJsonValue &value);

} // namespace OverlayControls
} // namespace StreamUP
