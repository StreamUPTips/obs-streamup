#ifndef STREAMUP_BACKUP_MANAGER_HPP
#define STREAMUP_BACKUP_MANAGER_HPP

#include <QString>
#include <QStringList>
#include <functional>

namespace StreamUP {
namespace Backup {

/**
 * Where OBS actually keeps its configuration for this install.
 *
 * These are resolved at runtime, never assumed: global.ini's [Locations]
 * section can point scene collections, profiles, config and plugin manager
 * settings anywhere on disk (see OBSApp.cpp, InitGlobalConfig). Portable mode
 * ignores those overrides and always uses the defaults.
 */
struct Locations {
	QString configDir;        // holds global.ini / user.ini
	QString profilesDir;      // basic/profiles
	QString scenesDir;        // basic/scenes
	QString pluginConfigDir;  // plugin_config
	QString pluginManagerDir; // plugin_manager
	QString themesDir;        // user themes, may not exist
	QString installDir;       // the OBS install root, for portable layouts
	bool portable = false;
	QString portableReason;   // how portable mode was detected, for the log and manifest
	bool valid() const { return !configDir.isEmpty(); }
};

/** What to put in the archive. */
struct Options {
	// Stream keys and OAuth tokens are stripped unless this is set, so a
	// backup is safe to share by default.
	bool includeCredentials = false;

	// Copy the media that scene collections reference into the archive.
	// Off by default: the audit still runs and reports missing files.
	bool collectMedia = false;

	bool includeThemes = true;
	bool includePluginConfig = true;

	// Per-file size ceiling. Plugin config directories can hold enormous
	// downloadable assets: a Whisper model shipped with obs-localvocal is
	// nearly 3 GB on its own. Compressing that into a backup takes minutes,
	// produces a file nobody can move around, and gains nothing because the
	// plugin re-downloads it. Files above this are skipped and listed in the
	// manifest so a restore can say what to fetch again. 0 disables the limit.
	qint64 maxFileSizeBytes = 100LL * 1024 * 1024;
};

/** A file left out because it was over the size ceiling. */
struct SkippedFile {
	QString path;
	qint64 size = 0;
};

/** One external file a scene collection points at. */
struct MediaReference {
	QString path;         // absolute path as stored in the collection
	QString collection;   // which scene collection referenced it
	QString key;          // the settings key it came from (file, local_file, ...)
	QString archiveName;  // where it lands inside the archive, unique per source path
	bool exists = false;
	qint64 size = 0;
};

/** Outcome of a backup run. */
struct Result {
	bool success = false;
	QString archivePath;
	QString error;
	int fileCount = 0;
	qint64 archiveBytes = 0;
	int mediaReferenced = 0;
	int mediaMissing = 0;
	int mediaCollected = 0;
	bool credentialsIncluded = false;
	QList<SkippedFile> skippedLargeFiles;
	qint64 skippedBytes = 0;

	// Referenced by a scene but not on disk, so it could not be included.
	// Carried out so the completion dialog can show exactly which files and
	// which scene collection each belongs to, rather than only a count.
	QList<MediaReference> missingMedia;

	// Per-area counts, so the result can be shown as a breakdown.
	QList<QPair<QString, int>> areaCounts;
};

/** Estimate of what a backup will contain, for showing before it runs. */
struct Estimate {
	int fileCount = 0;
	qint64 totalBytes = 0;
	int largeFileCount = 0;
	qint64 largeFileBytes = 0;
};

/** Size up a backup without writing anything. */
Estimate EstimateBackup(const Options &options);

/** Progress callback: (stage description, done, total). Return false to cancel. */
using ProgressCallback = std::function<bool(const QString &, int, int)>;

/** Resolve where this OBS install keeps everything. */
Locations ResolveLocations();

/**
 * Scan every scene collection for referenced external files. Used both for the
 * audit line in the manifest and, when collectMedia is on, for what to copy.
 */
QList<MediaReference> ScanMediaReferences(const Locations &locations);

/**
 * Write a backup archive. Forces OBS to save first so the archive holds current
 * state rather than the last flush.
 */
Result CreateBackup(const QString &archivePath, const Options &options, ProgressCallback progress = nullptr);

/** Default file name for a new backup, e.g. streamup-backup-2026-08-05-0914.zip */
QString SuggestedFileName();

/** Where automatic backups go by default: beside the OBS config. */
QString DefaultBackupFolder();

/** The folder in use: the user's chosen one, or the default. */
QString ResolveBackupFolder();

/**
 * Keep only the newest `keep` files matching a pattern, deleting the rest.
 * Used for automatic backups and for the safety backups a restore leaves
 * behind, both of which otherwise grow forever.
 */
int PruneBackups(const QString &folder, const QString &pattern, int keep);

/**
 * Write an automatic backup if one is due.
 *
 * Called during shutdown, after OBS has written its final state, so the archive
 * holds the session that just ended. Does nothing when automatic backups are
 * off, when one has already run today, or when a restore is staged (that path
 * takes its own safety backup).
 */
void RunAutomaticBackupIfDue();

} // namespace Backup
} // namespace StreamUP

#endif // STREAMUP_BACKUP_MANAGER_HPP
