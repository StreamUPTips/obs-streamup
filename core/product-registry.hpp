#pragma once

// Which StreamUP products are in the current scene collection.
//
// Every scene a .StreamUP install creates is stamped in its private settings
// with the product it came from:
//
//   "streamup_product": {"id": ..., "name": ..., "version": ..., "overlay": ...}
//
// Private settings travel inside the scene collection, so the stamp survives
// restarts, collection exports and backups. My Products reads the stamps and
// the Overlay Controls registry together.

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
	QString overlayId;  // Overlay Controls id, empty when no controls are saved here
	QStringList scenes; // scenes in this collection that belong to it
	bool stamped = false; // found by a scene stamp
	bool hasControls = false;
	bool isTransition = false; // offers "use as my transition"
};

// Stamps every scene the .StreamUP file at path describes. Old files with no
// manifest leave the scenes alone. UI thread (scene private settings).
void StampFromFile(const QString &path);

// The stamp on one scene; empty when it has none.
OverlayControls::ProductStamp StampOf(obs_source_t *scene);

// Every product in the current collection, controls registry first, sorted by name.
QList<ProductEntry> ListProducts();

} // namespace Products
} // namespace StreamUP
