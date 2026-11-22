#pragma once

// Google Fonts for the Overlay Controls font picker.
//
// Shares ProText's and StreamUP Chat's font cache (QStandardPaths::CacheLocation
// + "/streamup/fonts") and its catalogue file, so a font or catalogue fetched
// by any StreamUP plugin is there for the others.
//
// Two levels of "having" a font:
//   - previewing: the TTF is in the cache and loaded into Qt only, so the
//     picker can draw its name in its own face;
//   - installing: the TTF is installed for the current user and announced to
//     the system, so OBS's own text sources can use it. GDI+ text picks it up
//     straight away (a source refresh); FreeType 2 text only reads fonts
//     installed for all users, and only when OBS starts.
//
// Network calls block: call them off the UI thread (see Pool()).

#include <QString>
#include <QStringList>
#include <QThreadPool>

#include <vector>

namespace StreamUP {
namespace GoogleFonts {

struct Entry {
	QString family;
	QString category; // "Sans Serif", "Serif", "Display", "Handwriting", "Monospace"
	int popularity = 0;
};

// ~1900 families by popularity. Cached for a week; falls back to the stale
// cache, then to a short built-in list. Blocking.
std::vector<Entry> Catalog();

// The cached TTF for a family, or empty when it has not been downloaded.
QString CachedFile(const QString &family);

// Downloads the family's regular TTF into the cache (no-op when cached).
// Returns the file, or empty with *error set. Blocking.
QString Download(const QString &family, QString *error = nullptr, int timeoutSec = 30);

// Loads a TTF into Qt (previews only). Returns the family Qt registered it
// under, or empty. Thread-safe.
QString LoadForPreview(const QString &file);

struct InstallResult {
	bool ok = false;
	QString family;  // the name to write into the text source
	QString file;    // where it was installed
	QString error;
};

// Installs a downloaded TTF for the current user and tells the system, so
// GDI+ text sources can use it now and it survives restarts. Blocking (the
// system-wide font change broadcast can take a moment).
InstallResult InstallForUser(const QString &family, const QString &file);

// True when the family is installed on the system (not just previewable).
bool IsInstalled(const QString &family);

// True for a family Qt only knows because a preview loaded it: Qt lists it
// with the installed fonts, but OBS's text sources cannot see it.
bool IsPreviewOnly(const QString &family);

// A small pool for font downloads, drained on plugin unload.
QThreadPool *Pool();
void Shutdown();

} // namespace GoogleFonts
} // namespace StreamUP
