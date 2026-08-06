#include "restore-manager.hpp"

#include "../utilities/debug-logger.hpp"
#include "../utilities/zip-reader.hpp"
#include "backup-manager.hpp"
#include "../ui/settings-manager.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.h>
#include <util/platform.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>

namespace StreamUP {
namespace Restore {

namespace {

constexpr const char *kManifestName = "streamup-backup.json";

/** Where staged files and the journal live between staging and shutdown. */
QString stagingRoot()
{
	char *base = obs_module_config_path("restore");
	const QString path = QString::fromUtf8(base ? base : "");
	if (base)
		bfree(base);
	return path;
}

QString journalPath()
{
	return stagingRoot() + QStringLiteral("/journal.json");
}

QString lastResultPath()
{
	char *base = obs_module_config_path("restore-result.json");
	const QString path = QString::fromUtf8(base ? base : "");
	if (base)
		bfree(base);
	return path;
}

QJsonObject readJson(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return {};
	const QByteArray raw = f.readAll();
	f.close();
	return QJsonDocument::fromJson(raw).object();
}

bool writeJson(const QString &path, const QJsonObject &obj)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
	f.close();
	return true;
}

QString sha1Of(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return {};
	QCryptographicHash hash(QCryptographicHash::Sha1);
	hash.addData(&f);
	f.close();
	return QString::fromLatin1(hash.result().toHex());
}

void removeDirectory(const QString &path)
{
	QDir dir(path);
	if (dir.exists())
		dir.removeRecursively();
}

/**
 * Map an archive path onto this machine. Areas are mapped, not absolute paths,
 * so a backup taken from a portable install restores correctly into an
 * installed one and the other way round.
 */
QString targetForEntry(const QString &archivePath, const Backup::Locations &loc)
{
	if (archivePath == QStringLiteral("config/global.ini"))
		return loc.configDir + QStringLiteral("/global.ini");
	if (archivePath == QStringLiteral("config/user.ini"))
		return loc.configDir + QStringLiteral("/user.ini");

	if (archivePath.startsWith(QStringLiteral("config/basic/profiles/")))
		return loc.profilesDir + QStringLiteral("/") +
		       archivePath.mid(QStringLiteral("config/basic/profiles/").size());

	if (archivePath.startsWith(QStringLiteral("config/basic/scenes/")))
		return loc.scenesDir + QStringLiteral("/") +
		       archivePath.mid(QStringLiteral("config/basic/scenes/").size());

	if (archivePath.startsWith(QStringLiteral("config/plugin_manager/")))
		return loc.pluginManagerDir + QStringLiteral("/") +
		       archivePath.mid(QStringLiteral("config/plugin_manager/").size());

	if (archivePath.startsWith(QStringLiteral("config/plugin_config/")))
		return loc.pluginConfigDir + QStringLiteral("/") +
		       archivePath.mid(QStringLiteral("config/plugin_config/").size());

	if (archivePath.startsWith(QStringLiteral("themes/"))) {
		if (loc.themesDir.isEmpty())
			return {}; // nowhere sensible to put them
		return loc.themesDir + QStringLiteral("/") + archivePath.mid(QStringLiteral("themes/").size());
	}

	// media/ is handled separately, manifest and anything unknown are skipped.
	return {};
}

/**
 * Should this archive entry be restored, given what the user picked?
 *
 * Filtering happens at staging time rather than at apply time, so an unselected
 * area is never even unpacked and the live copy cannot be touched by a later
 * bug in the apply step.
 */
bool wantedBySelection(const QString &archivePath, const Selection &selection)
{
	if (archivePath.startsWith(QStringLiteral("media/")))
		return selection.media;

	if (archivePath.startsWith(QStringLiteral("themes/")))
		return selection.themes;

	if (archivePath == QStringLiteral("config/global.ini") || archivePath == QStringLiteral("config/user.ini"))
		return selection.obsSettings;

	if (archivePath.startsWith(QStringLiteral("config/basic/profiles/")))
		return selection.profiles;

	if (archivePath.startsWith(QStringLiteral("config/plugin_config/")) ||
	    archivePath.startsWith(QStringLiteral("config/plugin_manager/")))
		return selection.pluginSettings;

	if (archivePath.startsWith(QStringLiteral("config/basic/scenes/"))) {
		if (!selection.sceneCollections)
			return false;
		if (selection.onlyCollections.isEmpty())
			return true;
		// Match on the collection name, which is the file name without .json.
		const QString name = QFileInfo(archivePath).completeBaseName();
		return selection.onlyCollections.contains(name, Qt::CaseInsensitive);
	}

	return false;
}

/** Where collected media is restored to, kept together and out of the way. */
QString restoredMediaRoot(const Backup::Locations &loc)
{
	return loc.configDir + QStringLiteral("/streamup-restored-media");
}

} // namespace

Inspection Inspect(const QString &archivePath)
{
	Inspection result;
	result.archivePath = archivePath;

	Zip::Reader reader;
	if (!reader.open(archivePath)) {
		result.error = reader.lastError();
		return result;
	}

	const QByteArray manifestRaw = reader.readFile(QString::fromUtf8(kManifestName));
	if (manifestRaw.isEmpty()) {
		result.error = QStringLiteral("This file is not a StreamUP backup (no manifest inside)");
		return result;
	}

	const QJsonObject manifest = QJsonDocument::fromJson(manifestRaw).object();
	if (manifest.isEmpty() || !manifest.contains(QStringLiteral("format"))) {
		result.error = QStringLiteral("The backup manifest is unreadable");
		return result;
	}

	result.created = manifest.value(QStringLiteral("created")).toString();
	result.obsVersion = manifest.value(QStringLiteral("obs_version")).toString();
	result.streamUpVersion = manifest.value(QStringLiteral("streamup_version")).toString();
	result.platform = manifest.value(QStringLiteral("platform")).toString();
	result.portable = manifest.value(QStringLiteral("portable")).toBool();
	result.installDir = manifest.value(QStringLiteral("install_dir")).toString();
	result.credentialsIncluded = manifest.value(QStringLiteral("credentials_included")).toBool();
	result.mediaCollected = manifest.value(QStringLiteral("media_collected")).toBool();

	const QStringList names = reader.entryNames();
	result.totalFiles = names.size();

	// Plugin settings and themes are counted per folder, not per file: "115
	// files" means nothing to a user, "advanced-scene-switcher (3 files)" does.
	QMap<QString, int> pluginFileCounts;
	QMap<QString, int> themeFileCounts;

	for (const QString &name : names) {
		if (name.startsWith(QStringLiteral("config/basic/scenes/")) && name.endsWith(QStringLiteral(".json"))) {
			result.sceneCollections++;
			result.sceneCollectionNames << QFileInfo(name).completeBaseName();
		} else if (name.startsWith(QStringLiteral("config/basic/profiles/"))) {
			const QString rest = name.mid(QStringLiteral("config/basic/profiles/").size());
			const QString profile = rest.section(QLatin1Char('/'), 0, 0);
			if (!profile.isEmpty() && !result.profileNames.contains(profile)) {
				result.profileNames << profile;
				result.profiles++;
			}
		} else if (name.startsWith(QStringLiteral("config/plugin_config/"))) {
			result.pluginConfigFiles++;
			const QString rest = name.mid(QStringLiteral("config/plugin_config/").size());
			const QString plugin = rest.section(QLatin1Char('/'), 0, 0);
			if (!plugin.isEmpty())
				pluginFileCounts[plugin]++;
		} else if (name.startsWith(QStringLiteral("themes/"))) {
			result.themeFiles++;
			const QString rest = name.mid(QStringLiteral("themes/").size());
			// A theme is either a single .obt/.ovt file or a folder of assets.
			const QString theme = rest.contains(QLatin1Char('/')) ? rest.section(QLatin1Char('/'), 0, 0)
									      : rest;
			if (!theme.isEmpty())
				themeFileCounts[theme]++;
		} else if (name.startsWith(QStringLiteral("media/"))) {
			result.mediaFiles++;
		}
	}

	for (auto it = pluginFileCounts.constBegin(); it != pluginFileCounts.constEnd(); ++it)
		result.pluginConfigNames << QStringLiteral("%1  (%2 files)").arg(it.key()).arg(it.value());
	for (auto it = themeFileCounts.constBegin(); it != themeFileCounts.constEnd(); ++it)
		result.themeNames << (it.value() > 1 ? QStringLiteral("%1  (%2 files)").arg(it.key()).arg(it.value())
						    : it.key());

	// Which plugins the backup expected but this machine does not load.
	QSet<QString> installed;
	obs_enum_modules(
		[](void *param, obs_module_t *module) {
			auto *set = static_cast<QSet<QString> *>(param);
			set->insert(QString::fromUtf8(obs_get_module_file_name(module)));
		},
		&installed);

	const QJsonArray plugins = manifest.value(QStringLiteral("plugins")).toArray();
	for (const QJsonValue &value : plugins) {
		const QJsonObject plugin = value.toObject();
		const QString fileName = plugin.value(QStringLiteral("file")).toString();
		if (fileName.isEmpty() || installed.contains(fileName))
			continue;
		PluginGap gap;
		gap.name = plugin.value(QStringLiteral("name")).toString();
		if (gap.name.isEmpty())
			gap.name = fileName;
		gap.missing = true;
		result.pluginGaps.append(gap);
	}

	const Backup::Locations here = Backup::ResolveLocations();
	result.layoutDiffers = (here.portable != result.portable);

	result.valid = true;
	return result;
}

bool Stage(const QString &archivePath, QString *error, QString *safetyBackupPath, ProgressCallback progress,
	   const Selection &selection)
{
	auto reportError = [error](const QString &reason) {
		if (error)
			*error = reason;
		StreamUP::DebugLogger::LogError("Restore", reason.toUtf8().constData());
		return false;
	};

	if (obs_frontend_streaming_active() || obs_frontend_recording_active() ||
	    obs_frontend_replay_buffer_active() || obs_frontend_virtualcam_active())
		return reportError(QStringLiteral("Stop streaming, recording and the virtual camera first"));

	const Inspection inspection = Inspect(archivePath);
	if (!inspection.valid)
		return reportError(inspection.error);

	const Backup::Locations loc = Backup::ResolveLocations();
	if (!loc.valid())
		return reportError(QStringLiteral("Could not work out where OBS keeps its configuration"));

	// Safety backup first, always. A restore replaces the current setup, so
	// there has to be a way back even if the user did not think to make one.
	const QString safetyDir = loc.configDir + QStringLiteral("/streamup-safety-backups");
	QDir().mkpath(safetyDir);
	const QString safetyPath =
		safetyDir + QStringLiteral("/before-restore-%1.zip")
				    .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd-HHmmss")));

	Backup::Options safetyOptions;
	safetyOptions.includeCredentials = true; // it is going back to this machine only
	safetyOptions.collectMedia = false;

	// The safety backup is the slowest part of staging, so it reports through
	// the same progress channel rather than looking like a freeze.
	const Backup::Result safety =
		Backup::CreateBackup(safetyPath, safetyOptions, [&](const QString &file, int done, int total) {
			if (!progress)
				return true;
			return progress(QStringLiteral("Saving current setup: %1").arg(file), done, total);
		});
	if (!safety.success)
		return reportError(QStringLiteral("Could not take a safety backup first: %1").arg(safety.error));
	if (safetyBackupPath)
		*safetyBackupPath = safetyPath;
	StreamUP::DebugLogger::LogInfoFormat("Restore", "Safety backup written to %s",
					     safetyPath.toUtf8().constData());

	// Safety backups accumulate one per restore and are never cleaned up by
	// anything else, so they follow the same retention rule as automatic
	// backups rather than growing without limit.
	const int keep = StreamUP::SettingsManager::GetCurrentSettings().backupKeepCount;
	const int pruned = Backup::PruneBackups(safetyDir, QStringLiteral("before-restore-*.zip"), keep);
	if (pruned > 0)
		StreamUP::DebugLogger::LogInfoFormat("Restore", "Pruned %d old safety backups", pruned);

	// Extract everything to staging. Nothing live is touched by this.
	const QString staging = stagingRoot();
	removeDirectory(staging);
	QDir().mkpath(staging);

	Zip::Reader reader;
	if (!reader.open(archivePath))
		return reportError(reader.lastError());

	const QJsonObject manifest = QJsonDocument::fromJson(reader.readFile(kManifestName)).object();
	const QString mediaRoot = restoredMediaRoot(loc);

	QJsonArray plan;
	int staged = 0;

	const QStringList allEntries = reader.entryNames();
	int seen = 0;

	for (const QString &name : allEntries) {
		seen++;
		if (progress && (seen % 10 == 0 || seen == allEntries.size())) {
			if (!progress(QStringLiteral("Unpacking backup: %1").arg(QFileInfo(name).fileName()), seen,
				      allEntries.size())) {
				removeDirectory(staging);
				return reportError(QStringLiteral("Cancelled"));
			}
		}

		if (name == QString::fromUtf8(kManifestName))
			continue;

		if (!wantedBySelection(name, selection))
			continue;

		QString target;
		if (name.startsWith(QStringLiteral("media/")))
			target = mediaRoot + QStringLiteral("/") + name.mid(QStringLiteral("media/").size());
		else
			target = targetForEntry(name, loc);

		if (target.isEmpty())
			continue;

		const QString stagedPath = staging + QStringLiteral("/files/") + name;
		if (!reader.extractTo(name, stagedPath))
			return reportError(reader.lastError());

		QJsonObject item;
		item[QStringLiteral("archive")] = name;
		item[QStringLiteral("staged")] = stagedPath;
		item[QStringLiteral("target")] = QDir::cleanPath(target);
		item[QStringLiteral("sha1")] = sha1Of(stagedPath);
		plan.append(item);
		staged++;
	}

	// Rewrite media paths in the staged scene collections so the restored
	// scenes point at the files we are about to lay down, rather than at
	// wherever they lived on the machine the backup came from.
	int rewritten = 0;
	const QJsonArray media = manifest.value(QStringLiteral("media")).toArray();
	QHash<QString, QString> pathMap;
	for (const QJsonValue &value : media) {
		const QJsonObject entry = value.toObject();
		const QString archivePathForMedia = entry.value(QStringLiteral("archive_path")).toString();
		if (archivePathForMedia.isEmpty())
			continue; // referenced but not collected, leave the path alone
		const QString original = entry.value(QStringLiteral("path")).toString();
		const QString restored =
			mediaRoot + QStringLiteral("/") + archivePathForMedia.mid(QStringLiteral("media/").size());
		pathMap.insert(original, QDir::cleanPath(restored));
	}

	if (!pathMap.isEmpty()) {
		for (const QJsonValue &value : plan) {
			const QJsonObject item = value.toObject();
			const QString name = item.value(QStringLiteral("archive")).toString();
			if (!name.startsWith(QStringLiteral("config/basic/scenes/")))
				continue;

			const QString stagedPath = item.value(QStringLiteral("staged")).toString();
			QFile f(stagedPath);
			if (!f.open(QIODevice::ReadOnly))
				continue;
			QString contents = QString::fromUtf8(f.readAll());
			f.close();

			bool touched = false;
			for (auto it = pathMap.constBegin(); it != pathMap.constEnd(); ++it) {
				// Scene JSON stores paths with forward slashes, and so do we.
				const QString from = QString(it.key()).replace(QLatin1Char('\\'), QLatin1Char('/'));
				const QString to = QString(it.value()).replace(QLatin1Char('\\'), QLatin1Char('/'));
				if (contents.contains(from)) {
					contents.replace(from, to);
					touched = true;
				}
			}

			if (touched && f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
				f.write(contents.toUtf8());
				f.close();
				rewritten++;
			}
		}
	}

	// Recompute checksums for anything rewritten, so verification compares
	// against what will actually be written.
	QJsonArray finalPlan;
	for (const QJsonValue &value : plan) {
		QJsonObject item = value.toObject();
		item[QStringLiteral("sha1")] = sha1Of(item.value(QStringLiteral("staged")).toString());
		finalPlan.append(item);
	}

	QJsonObject journal;
	journal[QStringLiteral("version")] = 1;
	journal[QStringLiteral("staged_at")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
	journal[QStringLiteral("source_archive")] = archivePath;
	journal[QStringLiteral("safety_backup")] = safetyPath;
	journal[QStringLiteral("media_paths_rewritten")] = rewritten;
	journal[QStringLiteral("partial")] = !selection.everything();
	if (!selection.everything()) {
		QJsonObject picked;
		picked[QStringLiteral("scene_collections")] = selection.sceneCollections;
		picked[QStringLiteral("profiles")] = selection.profiles;
		picked[QStringLiteral("plugin_settings")] = selection.pluginSettings;
		picked[QStringLiteral("themes")] = selection.themes;
		picked[QStringLiteral("obs_settings")] = selection.obsSettings;
		picked[QStringLiteral("media")] = selection.media;
		picked[QStringLiteral("only_collections")] = QJsonArray::fromStringList(selection.onlyCollections);
		journal[QStringLiteral("selection")] = picked;
	}
	journal[QStringLiteral("files")] = finalPlan;

	if (!writeJson(journalPath(), journal))
		return reportError(QStringLiteral("Could not write the restore journal"));

	StreamUP::DebugLogger::LogInfoFormat("Restore", "Staged %d files%s (%d scene collections had media paths rewritten)",
					     staged, selection.everything() ? "" : " [partial restore]", rewritten);
	return true;
}

bool HasPending()
{
	return QFileInfo::exists(journalPath());
}

void CancelPending()
{
	removeDirectory(stagingRoot());
	StreamUP::DebugLogger::LogInfo("Restore", "Pending restore cancelled");
}

void ApplyPending()
{
	if (!HasPending())
		return;

	const QJsonObject journal = readJson(journalPath());
	const QJsonArray files = journal.value(QStringLiteral("files")).toArray();
	if (files.isEmpty())
		return;

	StreamUP::DebugLogger::LogInfoFormat("Restore", "Applying staged restore of %d files", files.size());

	int applied = 0;
	int alreadyInPlace = 0;
	int failed = 0;
	int loggedFailures = 0;
	QJsonArray failureList;

	for (const QJsonValue &value : files) {
		const QJsonObject item = value.toObject();
		const QString staged = item.value(QStringLiteral("staged")).toString();
		const QString target = item.value(QStringLiteral("target")).toString();
		const QString expected = item.value(QStringLiteral("sha1")).toString();
		if (staged.isEmpty() || target.isEmpty())
			continue;

		// Already correct? Then there is nothing to do, and crucially nothing
		// to fail. This makes a retry cheap and stops a second pass reporting
		// failures for files it restored the first time: by the time the retry
		// runs at startup, other plugins have their config files open, so
		// re-copying a file that is already right can fail for no good reason.
		if (!expected.isEmpty() && QFileInfo::exists(target) && sha1Of(target) == expected) {
			alreadyInPlace++;
			continue;
		}

		if (!QFileInfo::exists(staged)) {
			if (loggedFailures++ < 20)
				StreamUP::DebugLogger::LogWarningFormat(
					"Restore", "Staged file is gone, cannot restore %s",
					target.toUtf8().constData());
			failureList.append(target);
			failed++;
			continue;
		}

		QDir().mkpath(QFileInfo(target).absolutePath());

		// Replace atomically enough for our purposes: the old file only goes
		// once the new one is in place, so an interruption leaves either the
		// old file or the new one, never a truncated file.
		const QString incoming = target + QStringLiteral(".streamup-new");
		QFile::remove(incoming);

		QFile source(staged);
		if (!source.copy(incoming)) {
			if (loggedFailures++ < 20)
				StreamUP::DebugLogger::LogWarningFormat("Restore", "Copy failed for %s: %s",
									target.toUtf8().constData(),
									source.errorString().toUtf8().constData());
			failureList.append(target);
			failed++;
			continue;
		}

		QFile::remove(target);
		QFile incomingFile(incoming);
		if (incomingFile.rename(target)) {
			applied++;
			continue;
		}

		// Rename can fail if something grabbed the target between the remove
		// and the rename. Fall back to writing over it directly rather than
		// giving up on the file.
		const QString renameError = incomingFile.errorString();
		if (QFile::copy(staged, target)) {
			QFile::remove(incoming);
			applied++;
			continue;
		}

		QFile::remove(incoming);
		if (loggedFailures++ < 20)
			StreamUP::DebugLogger::LogWarningFormat("Restore", "Could not replace %s: %s",
								target.toUtf8().constData(),
								renameError.toUtf8().constData());
		failureList.append(target);
		failed++;
	}

	if (loggedFailures > 20)
		StreamUP::DebugLogger::LogWarningFormat("Restore", "...and %d more failures not listed",
							loggedFailures - 20);

	QJsonObject result;
	result[QStringLiteral("applied_at")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
	result[QStringLiteral("applied")] = applied;
	result[QStringLiteral("already_in_place")] = alreadyInPlace;
	result[QStringLiteral("failed")] = failed;
	result[QStringLiteral("total")] = files.size();
	result[QStringLiteral("source_archive")] = journal.value(QStringLiteral("source_archive"));
	result[QStringLiteral("safety_backup")] = journal.value(QStringLiteral("safety_backup"));
	result[QStringLiteral("failures")] = failureList;
	result[QStringLiteral("reported")] = false; // the UI shows this once on next launch
	writeJson(lastResultPath(), result);

	StreamUP::DebugLogger::LogInfoFormat("Restore", "Restored %d files, %d already correct, %d failed (of %lld)",
					     applied, alreadyInPlace, failed, (long long)files.size());

	// The staging folder stays put when anything failed, so the next launch can
	// finish the job rather than losing the restore.
	if (failed == 0)
		removeDirectory(stagingRoot());
}

void VerifyPending()
{
	// Runs at module load, before OBS reads scene collections, so anything
	// still outstanding can be put right before it matters.
	if (HasPending()) {
		StreamUP::DebugLogger::LogWarning("Restore",
						  "A staged restore was not fully applied, finishing it now");
		ApplyPending();
		return;
	}

	const QString resultPath = lastResultPath();
	if (!QFileInfo::exists(resultPath))
		return;

	const QJsonObject result = readJson(resultPath);
	const int applied = result.value(QStringLiteral("applied")).toInt();
	const int failed = result.value(QStringLiteral("failed")).toInt();
	if (failed > 0)
		StreamUP::DebugLogger::LogWarningFormat("Restore", "Last restore applied %d files with %d failures",
							applied, failed);
	else
		StreamUP::DebugLogger::LogInfoFormat("Restore", "Last restore applied %d files cleanly", applied);
}

AppliedReport ReadAppliedReport()
{
	AppliedReport report;
	const QString resultPath = lastResultPath();
	if (!QFileInfo::exists(resultPath))
		return report;

	const QJsonObject result = readJson(resultPath);
	report.present = true;
	report.reported = result.value(QStringLiteral("reported")).toBool();
	report.applied = result.value(QStringLiteral("applied")).toInt();
	report.alreadyInPlace = result.value(QStringLiteral("already_in_place")).toInt();
	report.failed = result.value(QStringLiteral("failed")).toInt();
	report.total = result.value(QStringLiteral("total")).toInt();
	report.appliedAt = result.value(QStringLiteral("applied_at")).toString();
	report.sourceArchive = result.value(QStringLiteral("source_archive")).toString();
	report.safetyBackup = result.value(QStringLiteral("safety_backup")).toString();
	for (const QJsonValue &value : result.value(QStringLiteral("failures")).toArray())
		report.failures << value.toString();
	return report;
}

void MarkReportSeen()
{
	const QString resultPath = lastResultPath();
	if (!QFileInfo::exists(resultPath))
		return;
	QJsonObject result = readJson(resultPath);
	result[QStringLiteral("reported")] = true;
	writeJson(resultPath, result);
}

QString LastRestoreSummary()
{
	const QString resultPath = lastResultPath();
	if (!QFileInfo::exists(resultPath))
		return {};

	const QJsonObject result = readJson(resultPath);
	return QStringLiteral("%1 files restored on %2")
		.arg(result.value(QStringLiteral("applied")).toInt())
		.arg(result.value(QStringLiteral("applied_at")).toString());
}

} // namespace Restore
} // namespace StreamUP
