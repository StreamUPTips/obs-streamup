#pragma once

// Which StreamUP products are in the current scene collection.
//
// Every scene a .StreamUP install creates is stamped in its private settings
// with the product it came from:
//
//   "streamup_product": {"id", "name", "version", "overlay", "category", "installed"}
//
// Private settings travel inside the scene collection, so the stamp survives
// restarts, collection exports and backups. A file with no manifest is stamped
// with a name and category read from its file name. Scenes installed before
// stamps existed are recognised by StreamUP's scene naming ("StreamUP Alerts •
// Minim"). My Products reads all of these and the Overlay Controls registry.
//
// A product's icon (manifest "icon") is copied at install into the plugin's
// config folder, so it survives the user deleting or moving the download.

#include "overlay-controls.hpp"

#include <obs.h>

#include <QList>
#include <QString>
#include <QStringList>

namespace StreamUP {
namespace Products {

constexpr const char *kStampKey = "streamup_product";

struct ProductEntry {
	QString id;
	QString name;
	QString version;
	QString category;   // lower case key: alerts, effects, transitions, widgets, tools, games, overlays, other
	QString overlayId;  // Overlay Controls id, empty when no controls are saved here
	QString iconPath;   // cached icon, empty when the product has none
	QStringList scenes; // scenes in this collection that belong to it
	bool stamped = false;     // found by a scene stamp
	bool foundByName = false; // installed before stamps; found by StreamUP's scene naming
	bool hasControls = false;
	bool shippedControls = false; // its file had controls, so they can be added back from it
	bool isTransition = false; // offers "use as my transition"
	QString transitionAction;  // that action's id
	bool usedAsTransition = false; // its transition is in OBS's list right now
};

// Stamps every scene the .StreamUP file at path describes and caches its icon.
// UI thread (scene private settings).
// Returns who the scenes were stamped as (empty when nothing was).
OverlayControls::ProductStamp StampFromFile(const QString &path);

// Copies the icon a .StreamUP file's manifest names into the icon cache.
void CacheIconFromFile(const QString &path);

// The stamp on one scene; empty when it has none.
OverlayControls::ProductStamp StampOf(obs_source_t *scene);

// Every product in the current collection, sorted by category then name.
QList<ProductEntry> ListProducts();

// Display name of a category key ("transitions" -> "Transitions"), localised.
QString CategoryLabel(const QString &key);

// Category keys in display order.
QStringList CategoryOrder();

} // namespace Products
} // namespace StreamUP
