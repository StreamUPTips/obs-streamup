#pragma once

// StreamUP's switchable parts, in one table. The Features page, the StreamUP
// menu, the websocket "switched off" replies and the start-up code all read
// from here, so a part is described once.
//
// A part that is off does nothing: nothing created, registered, hooked, timed
// or scanned. Parts marked live are switched on and off while OBS runs;
// the rest apply after an OBS restart (OBS has no way to take back a dock or a
// source type once it exists), and the page says so.

#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

namespace StreamUP {
namespace Features {

enum class Group { Products, Docks, Mixer, Sources, Hotkeys, Safety, Theme };

struct Feature {
	QString id;        // stable, used in demos/<id>.webp and the websocket error
	Group group;
	QString nameKey;   // locale keys
	QString lineKey;
	bool core = false; // always on, no switch
	bool live = false; // switches without a restart
	QString since;     // version that added it, for the "New" badge
	QString howKey;    // one line on how to turn it on and find it
	QString video;     // optional: a YouTube link, shown as "Watch the video"
	bool needsTheme = false; // only does anything on the StreamUP theme
};

const std::vector<Feature> &All();
const Feature *Find(const QString &id);

// Current setting (Core parts are always on).
bool IsOn(const QString &id);

// Whether the part is running right now. For a restart part this can differ
// from IsOn until OBS restarts.
bool IsActive(const QString &id);

// Saves the setting and, for a live part, starts or stops it now.
void SetOn(const QString &id, bool on);

// Opens or points at the part ("Show me where").
void ShowMe(const QString &id);

// Whether the StreamUP theme (or one of its colourways) is in use.
bool ThemeActive();

// The version the user last saw the Features page in (or, from before it
// existed, last ran the setup wizard in). Empty for a new user.
QString LastSeenVersion();

// Scenes, on any canvas, holding an Adjustment Layer.
int AdjustmentLayerSceneCount();

// Parts added since the user last opened the Features page.
QStringList NewSince(const QString &lastSeenVersion);

// Called once at start-up after settings load; remembers what is active.
void MarkStartupState();

// A readable websocket error for a request that belongs to an off part.
QString SwitchedOffMessage(const QString &id);

QString GroupLabel(Group g);

} // namespace Features
} // namespace StreamUP
