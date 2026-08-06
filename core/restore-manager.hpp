#ifndef STREAMUP_RESTORE_MANAGER_HPP
#define STREAMUP_RESTORE_MANAGER_HPP

#include <QString>
#include <QStringList>
#include <functional>

namespace StreamUP {
namespace Restore {

/** A plugin the backup used that this machine does not have (or has older). */
struct PluginGap {
	QString name;
	bool missing = false; // not installed at all
};

/** What is inside a backup, read without changing anything. */
struct Inspection {
	bool valid = false;
	QString error;

	QString archivePath;
	QString created;      // ISO timestamp from the manifest
	QString obsVersion;
	QString streamUpVersion;
	QString platform;
	bool portable = false;
	QString installDir;
	bool credentialsIncluded = false;
	bool mediaCollected = false;

	int sceneCollections = 0;
	int profiles = 0;
	int pluginConfigFiles = 0;
	int themeFiles = 0;
	int mediaFiles = 0;
	int totalFiles = 0;

	// Names for each area, so the dialog can show what is actually inside
	// rather than only how much of it there is.
	QStringList sceneCollectionNames;
	QStringList profileNames;
	QStringList pluginConfigNames; // plugin folder names, with a file count each
	QStringList themeNames;        // top-level theme names, not every asset file

	// Plugins the backup was taken with that are not installed here. Restoring
	// without them leaves dead sources, so this is shown before confirming.
	QList<PluginGap> pluginGaps;

	// True when the backup came from a different layout than this install
	// (portable vs installed). Restore still works, since everything is mapped
	// by area rather than by absolute path, but it is worth saying out loud.
	bool layoutDiffers = false;
};

/** Read a backup and report what it holds. Never writes anything. */
Inspection Inspect(const QString &archivePath);

/** Progress callback: (what is happening, done, total). Return false to cancel. */
using ProgressCallback = std::function<bool(const QString &, int, int)>;

/**
 * Which parts of a backup to put back.
 *
 * Restoring everything is the common case, but not the only one: after losing
 * one scene collection there is no reason to roll back profiles, plugin
 * settings and themes as well. Anything left out is simply not staged, so the
 * live copy is untouched.
 */
struct Selection {
	bool sceneCollections = true;
	bool profiles = true;
	bool pluginSettings = true;
	bool themes = true;
	bool obsSettings = true; // global.ini / user.ini
	bool media = true;       // only meaningful when the backup collected media

	// When non-empty, only these scene collections are restored (by name,
	// without the .json). Ignored unless sceneCollections is on.
	QStringList onlyCollections;

	bool everything() const
	{
		return sceneCollections && profiles && pluginSettings && themes && obsSettings && media &&
		       onlyCollections.isEmpty();
	}
};

/**
 * Stage a restore: extract to a staging folder, rewrite collected media paths,
 * and write the journal that the shutdown step applies.
 *
 * Nothing in the live config is touched here beyond the automatic safety
 * backup. Returns false and sets error on failure.
 */
bool Stage(const QString &archivePath, QString *error, QString *safetyBackupPath,
	   ProgressCallback progress = nullptr, const Selection &selection = Selection());

/** Is a staged restore waiting to be applied? */
bool HasPending();

/** Drop a staged restore without applying it. */
void CancelPending();

/**
 * Apply a staged restore. Called from obs_module_unload, which runs after OBS
 * has written its final state, so nothing overwrites what we put down.
 */
void ApplyPending();

/**
 * Check a restore that was applied at the last shutdown, finishing anything
 * incomplete. Called from obs_module_load, before OBS reads scene collections.
 */
void VerifyPending();

/** Human-readable description of the last completed restore, for the UI. */
QString LastRestoreSummary();

/** Outcome of the restore applied during the last shutdown. */
struct AppliedReport {
	bool present = false;   // was a restore applied at all
	bool reported = false;  // has the user already been shown it
	int applied = 0;
	int alreadyInPlace = 0;
	int failed = 0;
	int total = 0;
	QString appliedAt;
	QString sourceArchive;
	QString safetyBackup;
	QStringList failures; // file paths that could not be written
};

/** Read the report left by the last apply, if there is one. */
AppliedReport ReadAppliedReport();

/** Mark the report as shown so it is not repeated on every launch. */
void MarkReportSeen();

} // namespace Restore
} // namespace StreamUP

#endif // STREAMUP_RESTORE_MANAGER_HPP
