#include "backup-manager.hpp"

#include "../ui/settings-manager.hpp"
#include <streamup/debug-logger.hpp>
#include "../utilities/zip-writer.hpp"
#include "version.h"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.h>
#include <util/config-file.h>
#include <util/platform.h>

#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QSysInfo>

namespace StreamUP {
namespace Backup {

namespace {

// Chromium cache under plugin_config/obs-browser is by far the biggest thing in
// an OBS config tree (333 MB of Cache plus 122 MB of Code Cache on the machine
// this was measured on) and every byte of it regenerates. Backing it up turns a
// 4 MB archive into a 563 MB one for no benefit.
const QStringList kExcludedPluginConfigDirs = {QStringLiteral("obs-browser")};

// Regenerated or historical data. Never worth carrying.
const QStringList kExcludedConfigDirs = {QStringLiteral("logs"), QStringLiteral("crashes"),
					 QStringLiteral("profiler_data"), QStringLiteral("updates")};

// Settings keys in a scene collection that hold a path to an external file.
const QStringList kPathKeys = {QStringLiteral("file"),  QStringLiteral("local_file"),
			       QStringLiteral("path"),  QStringLiteral("shader_file_name"),
			       QStringLiteral("image"), QStringLiteral("font_file")};

QString cleanDir(const QString &path)
{
	return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

/**
 * Resolve one of the [Locations] entries to its BASE directory.
 *
 * These are not config paths. OBS appends "obs-studio/..." to whatever is
 * stored here (see OBSApp.cpp: `userProfilesLocation / "obs-studio/basic/
 * profiles"`), so a real installed config holds
 * `Profiles=C:\Users\<you>\AppData\Roaming` and OBS reads
 * `.../Roaming/obs-studio/basic/profiles`. Treating the stored value as the
 * config root lands one directory too high and finds nothing.
 *
 * Portable installs ignore the overrides entirely, which mirrors
 * OBSApp::InitGlobalConfig.
 */
QString resolveLocationBase(config_t *appConfig, const char *key, const QString &fallbackBase, bool portable)
{
	if (portable)
		return fallbackBase;

	const char *configured = config_get_string(appConfig, "Locations", key);
	if (!configured || !*configured)
		return fallbackBase;

	QString candidate = cleanDir(QString::fromUtf8(configured));
	if (QDir::isRelativePath(candidate))
		candidate = cleanDir(fallbackBase + QStringLiteral("/") + candidate);

	return QDir(candidate).exists() ? candidate : fallbackBase;
}

/** Strip the stream key out of a profile's service.json. */
QByteArray stripServiceJson(const QByteArray &raw, bool *changed)
{
	QJsonParseError err{};
	QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
		return raw;

	QJsonObject root = doc.object();
	QJsonObject settings = root.value(QStringLiteral("settings")).toObject();
	bool touched = false;
	for (const QString &key : {QStringLiteral("key"), QStringLiteral("password"), QStringLiteral("bearer_token")}) {
		if (settings.contains(key)) {
			settings.remove(key);
			touched = true;
		}
	}
	if (!touched)
		return raw;

	root[QStringLiteral("settings")] = settings;
	if (changed)
		*changed = true;
	return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

/**
 * Strip OAuth tokens out of a profile basic.ini. Done as a line filter rather
 * than by parsing: config_t would reformat the whole file, and we want the
 * restored ini to be byte-identical apart from the removed keys.
 */
QByteArray stripBasicIni(const QByteArray &raw, bool *changed)
{
	static const QStringList secretKeys = {QStringLiteral("Token"),        QStringLiteral("RefreshToken"),
					       QStringLiteral("Key"),          QStringLiteral("Password"),
					       QStringLiteral("BearerToken"),  QStringLiteral("ScopeVer")};

	QByteArray out;
	out.reserve(raw.size());
	bool inAuthSection = false;
	bool touched = false;

	const QList<QByteArray> lines = raw.split('\n');
	for (const QByteArray &line : lines) {
		const QByteArray trimmed = line.trimmed();
		if (trimmed.startsWith('[')) {
			const QString section = QString::fromUtf8(trimmed).mid(1, trimmed.size() - 2);
			// [Auth] plus any service section that carries a token
			inAuthSection = (section.compare(QStringLiteral("Auth"), Qt::CaseInsensitive) == 0) ||
					(section.compare(QStringLiteral("Twitch"), Qt::CaseInsensitive) == 0) ||
					(section.compare(QStringLiteral("YouTube"), Qt::CaseInsensitive) == 0) ||
					(section.compare(QStringLiteral("Restream"), Qt::CaseInsensitive) == 0);
		} else if (inAuthSection) {
			const int eq = trimmed.indexOf('=');
			if (eq > 0) {
				const QString key = QString::fromUtf8(trimmed.left(eq)).trimmed();
				if (secretKeys.contains(key, Qt::CaseInsensitive)) {
					touched = true;
					continue; // drop the line entirely
				}
			}
		}
		out.append(line);
		out.append('\n');
	}

	// split()/join round trip adds a trailing newline; drop it if the original
	// did not have one.
	if (!raw.endsWith('\n') && out.endsWith('\n'))
		out.chop(1);

	if (changed)
		*changed = touched;
	return touched ? out : raw;
}

/**
 * Recursively walk a directory, returning (absolute path, relative path) pairs.
 *
 * maxBytes drops anything larger and records it in skipped, so a 3 GB AI model
 * sitting in a plugin's config folder cannot quietly turn a 4 MB backup into an
 * hour-long compress of something the plugin will just re-download.
 */
void collectDir(const QString &rootDir, const QString &relativePrefix, const QStringList &skipDirs,
		QList<QPair<QString, QString>> &out, qint64 maxBytes = 0, QList<SkippedFile> *skipped = nullptr)
{
	QDir dir(rootDir);
	if (!dir.exists())
		return;

	const QFileInfoList entries =
		dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
	for (const QFileInfo &info : entries) {
		if (info.isDir()) {
			if (skipDirs.contains(info.fileName(), Qt::CaseInsensitive))
				continue;
			collectDir(info.absoluteFilePath(), relativePrefix + info.fileName() + QStringLiteral("/"),
				   skipDirs, out, maxBytes, skipped);
			continue;
		}

		// OBS writes .bak beside its own files; the live file is what we want.
		if (info.suffix().compare(QStringLiteral("bak"), Qt::CaseInsensitive) == 0)
			continue;

		// Runtime state, not configuration. Plugins drop lock and sentinel
		// files next to their settings (advanced-scene-switcher leaves a
		// ".running"), and restoring a stale one tells that plugin it is
		// already running, or tells OBS it crashed. Same reasoning as leaving
		// out the config root's own .sentinel and safe_mode.
		static const QStringList runtimeState = {QStringLiteral(".running"), QStringLiteral(".lock"),
							 QStringLiteral(".sentinel"), QStringLiteral("safe_mode")};
		if (runtimeState.contains(info.fileName(), Qt::CaseInsensitive) ||
		    info.suffix().compare(QStringLiteral("lock"), Qt::CaseInsensitive) == 0) {
			StreamUP::DebugLogger::LogDebugFormat("Backup", "Skip",
							      "Runtime state file %s left out",
							      info.fileName().toUtf8().constData());
			continue;
		}

		if (maxBytes > 0 && info.size() > maxBytes) {
			if (skipped)
				skipped->append({info.absoluteFilePath(), info.size()});
			StreamUP::DebugLogger::LogInfoFormat("Backup", "Skipping %s (%.1f MB, over the size limit)",
							     info.absoluteFilePath().toUtf8().constData(),
							     info.size() / (1024.0 * 1024.0));
			continue;
		}

		out.append({info.absoluteFilePath(), relativePrefix + info.fileName()});
	}
}

/**
 * Which folder inside media/ a referenced file belongs in.
 *
 * A collected backup is also how someone moves a setup to another machine, so
 * the media folder is something they will open and work in rather than an
 * opaque blob. Sorting by kind means the images sit with the images and an
 * overlay pack's html and css stay together, instead of ninety hashed folders
 * in one list. The category is part of the archive path and the manifest
 * records the full path per file, so restore keeps working unchanged and older
 * backups (which have no category folder) still restore.
 */
QString mediaCategory(const QString &sourcePath)
{
	const QString suffix = QFileInfo(sourcePath).suffix().toLower();

	static const QStringList images = {"png",  "jpg", "jpeg", "gif", "bmp", "webp",
					   "tga", "tiff", "tif",  "jxr", "psd", "svg"};
	static const QStringList video = {"mp4", "mov", "mkv", "webm", "avi", "flv", "m4v", "mpg", "mpeg", "wmv", "ts"};
	static const QStringList audio = {"wav", "mp3", "ogg", "flac", "aac", "m4a", "opus", "wma", "aiff"};
	static const QStringList fonts = {"ttf", "otf", "ttc", "woff", "woff2"};
	static const QStringList web = {"html", "htm", "css", "js", "json", "wasm"};
	static const QStringList shaders = {"shader", "effect", "hlsl", "glsl", "fx"};
	static const QStringList luts = {"cube", "3dl", "lut"};

	if (images.contains(suffix))
		return QStringLiteral("images");
	if (video.contains(suffix))
		return QStringLiteral("video");
	if (audio.contains(suffix))
		return QStringLiteral("audio");
	if (fonts.contains(suffix))
		return QStringLiteral("fonts");
	if (shaders.contains(suffix))
		return QStringLiteral("shaders");
	if (luts.contains(suffix))
		return QStringLiteral("luts");
	// Checked after shaders and LUTs on purpose: an overlay pack's .json is web
	// content, but a shader's .effect is not, and the specific lists win.
	if (web.contains(suffix))
		return QStringLiteral("web");
	return QStringLiteral("other");
}

/**
 * Where a referenced media file lands inside the archive.
 *
 * Flattening to media/<filename> collides: overlay packs are full of files
 * called index.html, style.css, image.png, and two different sources pointing
 * at two different index.html files would land on one entry. That is silent
 * data loss on backup and an ambiguous restore. Keying on a hash of the full
 * source path keeps every file distinct, and keeping the original file name in
 * the leaf keeps the archive readable. The manifest carries the mapping, so
 * restore never has to guess.
 */
QString mediaArchiveName(const QString &sourcePath)
{
	const QByteArray digest =
		QCryptographicHash::hash(sourcePath.toUtf8(), QCryptographicHash::Sha1).toHex().left(10);
	return QStringLiteral("media/%1/%2/%3")
		.arg(mediaCategory(sourcePath), QString::fromLatin1(digest), QFileInfo(sourcePath).fileName());
}

/** Pull every path-looking value out of a scene collection JSON tree. */
void walkForPaths(const QJsonValue &value, const QString &collection, QList<MediaReference> &out, QSet<QString> &seen)
{
	if (value.isObject()) {
		const QJsonObject obj = value.toObject();
		for (auto it = obj.begin(); it != obj.end(); ++it) {
			if (it.value().isString() && kPathKeys.contains(it.key())) {
				QString path = it.value().toString();
				if (path.isEmpty() || path.startsWith(QStringLiteral("http")))
					continue;
				const QFileInfo info(path);
				if (!info.isAbsolute())
					continue;
				const QString normalised = cleanDir(path);
				if (seen.contains(normalised))
					continue;
				seen.insert(normalised);

				MediaReference ref;
				ref.path = normalised;
				ref.collection = collection;
				ref.key = it.key();
				ref.archiveName = mediaArchiveName(normalised);
				ref.exists = info.exists();
				ref.size = ref.exists ? info.size() : 0;
				out.append(ref);
			} else {
				walkForPaths(it.value(), collection, out, seen);
			}
		}
	} else if (value.isArray()) {
		const QJsonArray arr = value.toArray();
		for (const QJsonValue &child : arr)
			walkForPaths(child, collection, out, seen);
	}
}

QJsonArray pluginInventory()
{
	QJsonArray plugins;
	// The plugin manager's own module list is the authoritative record of what
	// is installed and whether it is switched on.
	obs_enum_modules(
		[](void *param, obs_module_t *module) {
			auto *arr = static_cast<QJsonArray *>(param);
			QJsonObject entry;
			entry[QStringLiteral("name")] = QString::fromUtf8(obs_get_module_name(module)
										 ? obs_get_module_name(module)
										 : obs_get_module_file_name(module));
			entry[QStringLiteral("file")] = QString::fromUtf8(obs_get_module_file_name(module));
			arr->append(entry);
		},
		&plugins);
	return plugins;
}

} // namespace

// Resolved once while OBS is alive and kept, because the automatic backup runs
// during obs_shutdown when the frontend API has already been torn down:
// obs_frontend_get_app_config() and obs_frontend_get_current_profile_path()
// both return null by then, so resolving from scratch at that point fails.
static Locations s_cachedLocations;

Locations ResolveLocations()
{
	Locations loc;

	// App config, not user config: [Locations] lives in global.ini.
	config_t *appConfig = obs_frontend_get_app_config();
	if (!appConfig) {
		if (s_cachedLocations.valid()) {
			StreamUP::DebugLogger::LogDebug("Backup", "Locations",
							"Frontend is gone, using the locations resolved earlier");
			return s_cachedLocations;
		}
		StreamUP::DebugLogger::LogWarning("Backup", "Could not get OBS global config");
		return loc;
	}

	// Anchor everything on the known-good path OBS hands us for the current
	// profile, then walk up: <config>/basic/profiles/<name>
	char *profilePath = obs_frontend_get_current_profile_path();
	if (!profilePath) {
		if (s_cachedLocations.valid())
			return s_cachedLocations;
		StreamUP::DebugLogger::LogWarning("Backup", "Could not get current profile path");
		return loc;
	}
	// Absolute immediately: in portable mode OBS hands back a path relative to
	// the working directory (".../../config/obs-studio/basic/profiles/..."),
	// and every path derived from it would inherit that. It resolves today
	// only because OBS runs from bin/64bit, which is not something a backup
	// should depend on.
	const QString profileDir = cleanDir(QFileInfo(QString::fromUtf8(profilePath)).absoluteFilePath());
	bfree(profilePath);

	// Split the profile path into the base directory and the config root.
	// OBS composes it as <base>/obs-studio/basic/profiles/<name>, and the base
	// is what the [Locations] overrides store, so both are needed. Anchoring on
	// the segment rather than walking up a fixed number of levels keeps this
	// working when the profile name is absent.
	QString baseDir;
	QString configRoot;
	const int marker = profileDir.indexOf(QStringLiteral("/obs-studio/basic/profiles"), 0, Qt::CaseInsensitive);
	if (marker > 0) {
		baseDir = profileDir.left(marker);
		configRoot = baseDir + QStringLiteral("/obs-studio");
	} else {
		const int loose = profileDir.indexOf(QStringLiteral("/basic/profiles"), 0, Qt::CaseInsensitive);
		configRoot = (loose > 0) ? profileDir.left(loose) : cleanDir(profileDir + QStringLiteral("/../../.."));
		baseDir = cleanDir(configRoot + QStringLiteral("/.."));
		StreamUP::DebugLogger::LogWarningFormat("Backup", "Unexpected profile path %s, resolved config to %s",
							profileDir.toUtf8().constData(),
							configRoot.toUtf8().constData());
	}

	// Portable detection, most reliable signal first. This matters more than it
	// looks: portable and installed keep themes in completely different places,
	// and getting it wrong means silently backing up the wrong tree (or none).
	//
	// 1. Where the config actually is. OBS puts config under the user's app
	//    config dir on a normal install, and inside the install folder when
	//    portable. This is behavioural, so it holds however portable was
	//    triggered.
	// 2. portable_mode.txt beside the executable.
	// 3. The --portable / -p command line flag, which OBS also honours and
	//    which leaves no file on disk.
	char *userConfigBase = os_get_config_path_ptr("");
	const QString userConfigRoot = userConfigBase ? cleanDir(QString::fromUtf8(userConfigBase)) : QString();
	if (userConfigBase)
		bfree(userConfigBase);

	const bool configOutsideUserDir =
		!userConfigRoot.isEmpty() && !configRoot.startsWith(userConfigRoot, Qt::CaseInsensitive);

	const QString markerTwoUp = cleanDir(configRoot + QStringLiteral("/../../portable_mode.txt"));
	const QString markerOneUp = cleanDir(configRoot + QStringLiteral("/../portable_mode.txt"));
	const bool hasMarker = QFileInfo::exists(markerTwoUp) || QFileInfo::exists(markerOneUp);

	bool cmdlinePortable = false;
	const struct obs_cmdline_args args = obs_get_cmdline_args();
	for (int i = 1; i < args.argc; ++i) {
		const QString arg = QString::fromUtf8(args.argv[i]);
		if (arg == QStringLiteral("--portable") || arg == QStringLiteral("-p")) {
			cmdlinePortable = true;
			break;
		}
	}

	loc.portable = configOutsideUserDir || hasMarker || cmdlinePortable;
	QStringList reasons;
	if (configOutsideUserDir)
		reasons << QStringLiteral("config lives outside the user config directory");
	if (hasMarker)
		reasons << QStringLiteral("portable_mode.txt found");
	if (cmdlinePortable)
		reasons << QStringLiteral("--portable on the command line");
	loc.portableReason = loc.portable ? reasons.join(QStringLiteral(", "))
					  : QStringLiteral("config lives under the user config directory");

	// Only meaningful for a portable layout (<install>/config/obs-studio). On an
	// installed OBS the config lives in the user's app data and has no fixed
	// relationship to where the executable is, so claiming one would be a lie.
	loc.installDir = loc.portable ? cleanDir(configRoot + QStringLiteral("/../.."))
				      : QString();

	loc.configDir =
		resolveLocationBase(appConfig, "Configuration", baseDir, loc.portable) + QStringLiteral("/obs-studio");
	loc.profilesDir = resolveLocationBase(appConfig, "Profiles", baseDir, loc.portable) +
			  QStringLiteral("/obs-studio/basic/profiles");
	loc.scenesDir = resolveLocationBase(appConfig, "SceneCollections", baseDir, loc.portable) +
			QStringLiteral("/obs-studio/basic/scenes");
	loc.pluginManagerDir = resolveLocationBase(appConfig, "PluginManagerSettings", baseDir, loc.portable) +
			       QStringLiteral("/obs-studio/plugin_manager");
	loc.pluginConfigDir = loc.configDir + QStringLiteral("/plugin_config");

	// Themes live in a different place depending on the mode, so branch on it
	// explicitly rather than probing and hoping. A portable install must never
	// pull themes out of %APPDATA%: that is a different OBS instance's data.
	//
	// Portable:  <install>/data/obs-studio/themes
	// Installed: <user config>/obs-studio/themes  (%APPDATA% on Windows)
	//
	// The portable path is derived from our own module's data directory
	// (<install>/data/obs-plugins/streamup) rather than obs_module_file, which
	// returns null for paths that do not exist and cannot reach a sibling tree.
	if (loc.portable) {
		// Derived from the install root rather than the module data path:
		// obs_get_module_data_path can hand back a path relative to the
		// working directory, and a backup must never depend on what the
		// working directory happened to be.
		QStringList candidates = {loc.installDir + QStringLiteral("/data/obs-studio/themes")};
		if (const char *moduleData = obs_get_module_data_path(obs_current_module()))
			candidates << QFileInfo(QString::fromUtf8(moduleData) +
						QStringLiteral("/../../obs-studio/themes"))
					      .absoluteFilePath();

		for (const QString &candidate : candidates) {
			const QString absolute = cleanDir(QFileInfo(candidate).absoluteFilePath());
			if (QDir(absolute).exists()) {
				loc.themesDir = absolute;
				break;
			}
		}

		if (loc.themesDir.isEmpty())
			StreamUP::DebugLogger::LogWarningFormat("Backup",
								"Portable themes directory not found (tried %s)",
								candidates.join(QStringLiteral(", "))
									.toUtf8()
									.constData());
	} else {
		const QString candidate = loc.configDir + QStringLiteral("/themes");
		if (QDir(candidate).exists())
			loc.themesDir = candidate;
		else
			StreamUP::DebugLogger::LogDebugFormat("Backup", "Themes",
							      "No user themes directory at %s (nothing to back up)",
							      candidate.toUtf8().constData());
	}

	// Every resolved path gets logged with whether it exists. A backup that
	// silently misses an area is worse than one that fails, so the log has to
	// make it obvious after the fact which tree each area came from.
	StreamUP::DebugLogger::LogInfoFormat("Backup", "Mode: %s (%s)", loc.portable ? "PORTABLE" : "INSTALLED",
					     loc.portableReason.toUtf8().constData());
	StreamUP::DebugLogger::LogInfoFormat("Backup", "Install dir:    %s", loc.installDir.toUtf8().constData());

	const QList<QPair<const char *, QString>> resolved = {
		{"config", loc.configDir},         {"profiles", loc.profilesDir},
		{"scenes", loc.scenesDir},         {"plugin_config", loc.pluginConfigDir},
		{"plugin_manager", loc.pluginManagerDir}, {"themes", loc.themesDir},
	};
	for (const auto &entry : resolved) {
		const bool present = !entry.second.isEmpty() && QDir(entry.second).exists();
		StreamUP::DebugLogger::LogInfoFormat("Backup", "%-15s %s%s", entry.first,
						     entry.second.isEmpty() ? "(not found)"
									    : entry.second.toUtf8().constData(),
						     present ? "" : "   [MISSING]");
	}

	s_cachedLocations = loc;
	return loc;
}

QList<MediaReference> ScanMediaReferences(const Locations &locations)
{
	QList<MediaReference> refs;
	QSet<QString> seen;

	QDir scenes(locations.scenesDir);
	const QFileInfoList files = scenes.entryInfoList({QStringLiteral("*.json")}, QDir::Files);
	for (const QFileInfo &info : files) {
		QFile f(info.absoluteFilePath());
		if (!f.open(QIODevice::ReadOnly))
			continue;
		const QByteArray raw = f.readAll();
		f.close();

		QJsonParseError err{};
		const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
		if (err.error != QJsonParseError::NoError)
			continue;

		walkForPaths(doc.object(), info.completeBaseName(), refs, seen);
	}

	return refs;
}

Estimate EstimateBackup(const Options &options)
{
	Estimate estimate;
	const Locations loc = ResolveLocations();
	if (!loc.valid())
		return estimate;

	QList<QPair<QString, QString>> files;
	QList<SkippedFile> skipped;

	for (const QString &name : {QStringLiteral("global.ini"), QStringLiteral("user.ini")}) {
		const QString path = loc.configDir + QStringLiteral("/") + name;
		if (QFileInfo::exists(path))
			files.append({path, name});
	}
	collectDir(loc.profilesDir, QString(), {}, files, options.maxFileSizeBytes, &skipped);
	collectDir(loc.scenesDir, QString(), {}, files, options.maxFileSizeBytes, &skipped);
	collectDir(loc.pluginManagerDir, QString(), {}, files, options.maxFileSizeBytes, &skipped);
	if (options.includePluginConfig)
		collectDir(loc.pluginConfigDir, QString(), kExcludedPluginConfigDirs, files, options.maxFileSizeBytes,
			   &skipped);
	if (options.includeThemes && !loc.themesDir.isEmpty())
		collectDir(loc.themesDir, QString(), {}, files, options.maxFileSizeBytes, &skipped);

	for (const auto &entry : files) {
		estimate.fileCount++;
		estimate.totalBytes += QFileInfo(entry.first).size();
	}

	if (options.collectMedia) {
		for (const MediaReference &ref : ScanMediaReferences(loc)) {
			if (!ref.exists)
				continue;
			if (options.maxFileSizeBytes > 0 && ref.size > options.maxFileSizeBytes) {
				estimate.largeFileCount++;
				estimate.largeFileBytes += ref.size;
				continue;
			}
			estimate.fileCount++;
			estimate.totalBytes += ref.size;
		}
	}

	for (const SkippedFile &s : skipped) {
		estimate.largeFileCount++;
		estimate.largeFileBytes += s.size;
	}

	return estimate;
}

QString SuggestedFileName()
{
	return QStringLiteral("streamup-backup-%1.zip")
		.arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd-HHmm")));
}

QString DefaultBackupFolder()
{
	const Locations loc = ResolveLocations();
	if (!loc.valid())
		return {};
	// Beside the config, so it travels with a portable install and the restore
	// dialog can find it without being told where to look.
	return loc.configDir + QStringLiteral("/streamup-backups");
}

QString ResolveBackupFolder()
{
	const StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
	const QString configured = QString::fromStdString(settings.backupLocation);
	return configured.isEmpty() ? DefaultBackupFolder() : cleanDir(configured);
}

int PruneBackups(const QString &folder, const QString &pattern, int keep)
{
	if (keep <= 0 || folder.isEmpty())
		return 0;

	QDir dir(folder);
	if (!dir.exists())
		return 0;

	// Newest first, so everything past `keep` is the old end of the list.
	const QFileInfoList files = dir.entryInfoList({pattern}, QDir::Files, QDir::Time);
	int removed = 0;
	for (int i = keep; i < files.size(); ++i) {
		if (QFile::remove(files[i].absoluteFilePath())) {
			removed++;
			StreamUP::DebugLogger::LogInfoFormat("Backup", "Pruned old backup %s",
							     files[i].fileName().toUtf8().constData());
		}
	}
	return removed;
}

void RunAutomaticBackupIfDue()
{
	StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
	if (!settings.modules.backup) {
		StreamUP::DebugLogger::LogDebug("Backup", "Automatic", "Backup module is switched off, skipping");
		return;
	}
	if (!settings.backupAutomatic)
		return;

	// One a day. Closing OBS five times in an evening should not produce five
	// archives, and the day's work is what is worth keeping.
	const QString today = QDate::currentDate().toString(Qt::ISODate);
	if (QString::fromStdString(settings.backupLastAutoDate) == today) {
		StreamUP::DebugLogger::LogInfo("Backup", "Automatic backup already done today, skipping");
		return;
	}

	const QString folder = ResolveBackupFolder();
	if (folder.isEmpty()) {
		StreamUP::DebugLogger::LogWarning("Backup", "No backup folder resolved, skipping automatic backup");
		return;
	}
	if (!QDir().mkpath(folder)) {
		StreamUP::DebugLogger::LogWarningFormat("Backup", "Could not create backup folder %s",
							folder.toUtf8().constData());
		return;
	}

	const QString path = QDir(folder).filePath(
		QStringLiteral("streamup-auto-%1.zip")
			.arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd-HHmm"))));

	Options options;
	// This copy never leaves the machine, so it is the full-fidelity one: a
	// restore from it should not need the stream key re-entering.
	options.includeCredentials = true;
	options.collectMedia = false;

	StreamUP::DebugLogger::LogInfoFormat("Backup", "Running automatic backup to %s", path.toUtf8().constData());
	const QElapsedTimer timer = [] {
		QElapsedTimer t;
		t.start();
		return t;
	}();

	const Result result = CreateBackup(path, options);
	if (!result.success) {
		StreamUP::DebugLogger::LogWarningFormat("Backup", "Automatic backup failed: %s",
							result.error.toUtf8().constData());
		return;
	}

	StreamUP::DebugLogger::LogInfoFormat("Backup", "Automatic backup done in %lld ms (%d files, %lld bytes)",
					     (long long)timer.elapsed(), result.fileCount,
					     (long long)result.archiveBytes);

	PruneBackups(folder, QStringLiteral("streamup-auto-*.zip"), settings.backupKeepCount);

	settings.backupLastAutoDate = today.toStdString();
	StreamUP::SettingsManager::UpdateSettings(settings);
}

Result CreateBackup(const QString &archivePath, const Options &options, ProgressCallback progress)
{
	Result result;
	result.archivePath = archivePath;
	result.credentialsIncluded = options.includeCredentials;

	const Locations loc = ResolveLocations();
	if (!loc.valid()) {
		result.error = QStringLiteral("Could not work out where OBS keeps its configuration");
		return result;
	}

	// OBS only flushes config on save, so without this the archive holds the
	// last flush rather than what is on screen right now. During shutdown the
	// frontend has already saved and the API is gone, so this is skipped.
	if (obs_frontend_get_app_config())
		obs_frontend_save();

	// Build the file list first so progress can be reported against a total,
	// and so each area's count can be logged and checked before anything is
	// written.
	QList<QPair<QString, QString>> files;
	QList<QPair<QString, int>> areaCounts;

	// Collect one area and record how many files it contributed. An area whose
	// source directory exists but yields nothing is a bug, not a quiet no-op,
	// so it gets logged as a warning.
	auto addArea = [&](const QString &label, const QString &sourceDir, const QString &prefix,
			   const QStringList &skip) {
		const int before = files.size();
		collectDir(sourceDir, prefix, skip, files, options.maxFileSizeBytes, &result.skippedLargeFiles);
		const int added = files.size() - before;
		areaCounts.append({label, added});

		if (sourceDir.isEmpty()) {
			StreamUP::DebugLogger::LogInfoFormat("Backup", "%-15s skipped (no path resolved)",
							     label.toUtf8().constData());
		} else if (added == 0 && QDir(sourceDir).exists()) {
			StreamUP::DebugLogger::LogWarningFormat(
				"Backup", "%s: 0 files collected from %s, which exists. This is unexpected.",
				label.toUtf8().constData(), sourceDir.toUtf8().constData());
		} else {
			StreamUP::DebugLogger::LogInfoFormat("Backup", "%-15s %d files from %s",
							     label.toUtf8().constData(), added,
							     sourceDir.toUtf8().constData());
		}
	};

	int rootFiles = 0;
	for (const QString &name : {QStringLiteral("global.ini"), QStringLiteral("user.ini")}) {
		const QString path = loc.configDir + QStringLiteral("/") + name;
		if (QFileInfo::exists(path)) {
			files.append({path, QStringLiteral("config/") + name});
			rootFiles++;
		} else {
			StreamUP::DebugLogger::LogWarningFormat("Backup", "%s not found at %s",
								name.toUtf8().constData(),
								path.toUtf8().constData());
		}
	}
	areaCounts.append({QStringLiteral("config root"), rootFiles});

	addArea(QStringLiteral("profiles"), loc.profilesDir, QStringLiteral("config/basic/profiles/"), {});
	addArea(QStringLiteral("scenes"), loc.scenesDir, QStringLiteral("config/basic/scenes/"), {});
	addArea(QStringLiteral("plugin_manager"), loc.pluginManagerDir, QStringLiteral("config/plugin_manager/"), {});

	if (options.includePluginConfig)
		addArea(QStringLiteral("plugin_config"), loc.pluginConfigDir, QStringLiteral("config/plugin_config/"),
			kExcludedPluginConfigDirs);

	if (options.includeThemes)
		addArea(QStringLiteral("themes"), loc.themesDir, QStringLiteral("themes/"), {});

	const QList<MediaReference> media = ScanMediaReferences(loc);
	result.mediaReferenced = media.size();
	for (const MediaReference &ref : media) {
		if (!ref.exists) {
			result.mediaMissing++;
			result.missingMedia.append(ref);
			continue;
		}
		if (options.collectMedia)
			files.append({ref.path, ref.archiveName});
	}

	Zip::Writer zip;
	if (!zip.open(archivePath)) {
		result.error = zip.lastError();
		return result;
	}

	const int total = files.size() + 1; // +1 for the manifest
	int done = 0;
	QJsonArray fileList;

	for (const QPair<QString, QString> &entry : files) {
		if (progress && !progress(QFileInfo(entry.first).fileName(), done, total)) {
			zip.close();
			QFile::remove(archivePath);
			result.error = QStringLiteral("Cancelled");
			return result;
		}

		const QString name = QFileInfo(entry.first).fileName();
		bool wrote = false;

		// Credential-bearing files get filtered on the way in unless the user
		// asked for a full-fidelity backup.
		if (!options.includeCredentials && entry.second.startsWith(QStringLiteral("config/basic/profiles/"))) {
			QFile in(entry.first);
			if (in.open(QIODevice::ReadOnly)) {
				const QByteArray raw = in.readAll();
				in.close();
				bool stripped = false;
				QByteArray filtered = raw;
				if (name.compare(QStringLiteral("service.json"), Qt::CaseInsensitive) == 0)
					filtered = stripServiceJson(raw, &stripped);
				else if (name.compare(QStringLiteral("basic.ini"), Qt::CaseInsensitive) == 0)
					filtered = stripBasicIni(raw, &stripped);

				if (stripped) {
					wrote = zip.addData(filtered, entry.second);
					StreamUP::DebugLogger::LogDebugFormat(
						"Backup", "Credentials", "Stripped secrets from %s",
						entry.second.toUtf8().constData());
				}
			}
		}

		if (!wrote) {
			// Chunk callback keeps the UI alive inside a single large file.
			// Without it, one big file looks like a hang, which is exactly
			// what a multi-gigabyte model file used to do.
			const qint64 fileSize = QFileInfo(entry.first).size();
			wrote = zip.addFile(entry.first, entry.second,
					    [&](qint64 doneBytes, qint64 totalBytes) {
						    if (!progress || totalBytes < 8 * 1024 * 1024)
							    return true;
						    const QString label =
							    QStringLiteral("%1 (%2 of %3 MB)")
								    .arg(name)
								    .arg(doneBytes / (1024 * 1024))
								    .arg(totalBytes / (1024 * 1024));
						    return progress(label, done, total);
					    });
			Q_UNUSED(fileSize);
		}

		if (!wrote) {
			// A single unreadable file (locked, permissions) should not sink
			// the whole backup; note it and carry on.
			StreamUP::DebugLogger::LogWarning("Backup", QStringLiteral("Skipped %1: %2")
									  .arg(entry.first, zip.lastError())
									  .toUtf8()
									  .constData());
			done++;
			continue;
		}

		if (entry.second.startsWith(QStringLiteral("media/")))
			result.mediaCollected++;

		fileList.append(entry.second);
		result.fileCount++;
		done++;
	}

	// Manifest: what this backup is, what it came from, and what it points at.
	QJsonObject manifest;
	manifest[QStringLiteral("format")] = 1;
	manifest[QStringLiteral("created")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
	manifest[QStringLiteral("streamup_version")] = QStringLiteral(PROJECT_VERSION);
	manifest[QStringLiteral("obs_version")] = QString::fromUtf8(obs_get_version_string());
	manifest[QStringLiteral("platform")] = QSysInfo::prettyProductName();
	manifest[QStringLiteral("portable")] = loc.portable;
	manifest[QStringLiteral("portable_detected_by")] = loc.portableReason;
	manifest[QStringLiteral("install_dir")] = loc.installDir;

	// Where every area came from. Restore needs this to put things back in the
	// right tree, and it makes a backup auditable without guessing.
	QJsonObject locations;
	locations[QStringLiteral("config")] = loc.configDir;
	locations[QStringLiteral("profiles")] = loc.profilesDir;
	locations[QStringLiteral("scenes")] = loc.scenesDir;
	locations[QStringLiteral("plugin_config")] = loc.pluginConfigDir;
	locations[QStringLiteral("plugin_manager")] = loc.pluginManagerDir;
	locations[QStringLiteral("themes")] = loc.themesDir;
	manifest[QStringLiteral("locations")] = locations;

	QJsonObject counts;
	for (const auto &area : areaCounts)
		counts[area.first] = area.second;
	manifest[QStringLiteral("area_counts")] = counts;

	manifest[QStringLiteral("credentials_included")] = options.includeCredentials;
	manifest[QStringLiteral("media_collected")] = options.collectMedia;
	manifest[QStringLiteral("plugins")] = pluginInventory();
	manifest[QStringLiteral("files")] = fileList;

	QJsonArray mediaArray;
	for (const MediaReference &ref : media) {
		QJsonObject m;
		m[QStringLiteral("path")] = ref.path;
		m[QStringLiteral("collection")] = ref.collection;
		m[QStringLiteral("key")] = ref.key;
		m[QStringLiteral("exists")] = ref.exists;
		m[QStringLiteral("size")] = ref.size;
		// Only set when the file was actually copied in, so restore can tell
		// "collected" from "referenced but absent".
		if (options.collectMedia && ref.exists)
			m[QStringLiteral("archive_path")] = ref.archiveName;
		mediaArray.append(m);
	}
	manifest[QStringLiteral("media")] = mediaArray;

	// Anything left out for being oversized is recorded, so a restore can tell
	// the user exactly what to fetch again rather than leaving them to notice a
	// plugin misbehaving later.
	QJsonArray skippedArray;
	for (const SkippedFile &skipped : result.skippedLargeFiles) {
		QJsonObject s;
		s[QStringLiteral("path")] = skipped.path;
		s[QStringLiteral("size")] = skipped.size;
		skippedArray.append(s);
		result.skippedBytes += skipped.size;
	}
	manifest[QStringLiteral("skipped_large_files")] = skippedArray;
	manifest[QStringLiteral("max_file_size_bytes")] = options.maxFileSizeBytes;

	if (!zip.addData(QJsonDocument(manifest).toJson(QJsonDocument::Indented),
			 QStringLiteral("streamup-backup.json"))) {
		result.error = zip.lastError();
		zip.close();
		return result;
	}

	if (progress)
		progress(QStringLiteral("streamup-backup.json"), total, total);

	result.archiveBytes = zip.bytesWritten();
	if (!zip.close()) {
		result.error = zip.lastError();
		return result;
	}

	// Read the archive back before calling it a success. A backup that is
	// quietly truncated is worth less than no backup at all, because it is
	// trusted right up until the moment it is needed.
	QString verifyError;
	if (!Zip::VerifyArchive(archivePath, result.fileCount + 1, &verifyError)) {
		result.error = QStringLiteral("Backup could not be verified: %1").arg(verifyError);
		StreamUP::DebugLogger::LogError("Backup", result.error.toUtf8().constData());
		return result;
	}

	result.success = true;
	result.areaCounts = areaCounts;

	StreamUP::DebugLogger::LogInfoFormat("Backup", "--- Backup complete ---");
	for (const auto &area : areaCounts)
		StreamUP::DebugLogger::LogInfoFormat("Backup", "  %-15s %d", area.first.toUtf8().constData(),
						     area.second);
	StreamUP::DebugLogger::LogInfoFormat("Backup", "  %-15s %d referenced, %d missing, %d collected", "media",
					     result.mediaReferenced, result.mediaMissing, result.mediaCollected);
	StreamUP::DebugLogger::LogInfoFormat("Backup", "  %-15s %s", "credentials",
					     options.includeCredentials ? "INCLUDED" : "stripped");
	if (!result.skippedLargeFiles.isEmpty()) {
		StreamUP::DebugLogger::LogInfoFormat("Backup", "  %-15s %d files, %.1f MB total", "skipped (large)",
						     result.skippedLargeFiles.size(),
						     result.skippedBytes / (1024.0 * 1024.0));
		for (const SkippedFile &skipped : result.skippedLargeFiles)
			StreamUP::DebugLogger::LogInfoFormat("Backup", "      %.1f MB  %s",
							     skipped.size / (1024.0 * 1024.0),
							     skipped.path.toUtf8().constData());
	}
	StreamUP::DebugLogger::LogInfoFormat("Backup", "Verified %d files, %lld bytes -> %s", result.fileCount,
					     (long long)result.archiveBytes, archivePath.toUtf8().constData());
	return result;
}

} // namespace Backup
} // namespace StreamUP
