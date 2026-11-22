#include "google-fonts.hpp"

#include <streamup/debug-logger.hpp>

#include <obs-module.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>
#include <mutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace StreamUP {
namespace GoogleFonts {

namespace {

constexpr qint64 kCatalogTtlSecs = 7 * 24 * 3600;

std::mutex g_mutex;
std::vector<Entry> g_catalog;
QSet<QString> g_installedThisSession;
QSet<QString> g_previewLoaded; // families loaded into Qt for previews only

QString CacheDir()
{
	// Same folder as obs-streamup-media-cache's font_cache_dir().
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/streamup/fonts";
	QDir().mkpath(dir);
	return dir;
}

QString FileFor(const QString &family)
{
	// ProText's naming, so either plugin finds the other's download.
	return CacheDir() + "/" + QString(family).replace(' ', '-') + ".ttf";
}

// curl ships with Windows 10+, macOS and Linux; it avoids depending on a Qt
// TLS backend. Same approach as ProText.
QByteArray CurlGet(const QString &url, const QByteArray &userAgent, int timeoutSec)
{
	QProcess proc;
	QStringList args{"-s", "-L", "--max-time", QString::number(timeoutSec)};
	if (!userAgent.isEmpty())
		args << "-A" << QString::fromUtf8(userAgent);
	args << url;
	proc.start("curl", args);
	if (!proc.waitForFinished((timeoutSec + 5) * 1000)) {
		proc.kill();
		return {};
	}
	return proc.exitCode() == 0 ? proc.readAllStandardOutput() : QByteArray();
}

bool CurlDownload(const QString &url, const QString &target, int timeoutSec)
{
	const QString part = target + ".part";
	QProcess proc;
	proc.start("curl", {"-s", "-L", "--fail", "--max-time", QString::number(timeoutSec), "-o", part, url});
	if (!proc.waitForFinished((timeoutSec + 5) * 1000)) {
		proc.kill();
		QFile::remove(part);
		return false;
	}
	if (proc.exitCode() != 0 || QFileInfo(part).size() < 1024) {
		QFile::remove(part);
		return false;
	}
	QFile::remove(target);
	return QFile::rename(part, target);
}

std::vector<Entry> ParseCatalog(QByteArray data)
{
	const qsizetype brace = data.indexOf('{');
	if (brace > 0)
		data = data.mid(brace); // XSSI guard on the raw endpoint
	const QJsonObject root = QJsonDocument::fromJson(data).object();
	QJsonArray list = root.value("familyMetadataList").toArray();
	if (list.isEmpty())
		list = root.value("families").toArray(); // trimmed cache shape
	std::vector<Entry> out;
	out.reserve(size_t(list.size()));
	for (const QJsonValue v : list) {
		const QJsonObject o = v.toObject();
		Entry e{o.value("family").toString(), o.value("category").toString(), o.value("popularity").toInt(99999)};
		if (!e.family.isEmpty())
			out.push_back(e);
	}
	return out;
}

// The trimmed shape obs-streamup-media-cache writes, weights included, so
// ProText keeps trusting the shared file.
QByteArray TrimmedCatalog(const QByteArray &raw)
{
	QByteArray data = raw;
	const qsizetype brace = data.indexOf('{');
	if (brace > 0)
		data = data.mid(brace);
	QJsonArray arr;
	for (const QJsonValue v : QJsonDocument::fromJson(data).object().value("familyMetadataList").toArray()) {
		const QJsonObject o = v.toObject();
		QJsonArray weights;
		const QJsonObject fonts = o.value("fonts").toObject();
		QList<int> ws;
		for (auto it = fonts.begin(); it != fonts.end(); ++it)
			if (!it.key().endsWith('i') && it.key().toInt() >= 100 && it.key().toInt() <= 900)
				ws.append(it.key().toInt());
		std::sort(ws.begin(), ws.end());
		for (int w : ws)
			weights.append(w);
		arr.append(QJsonObject{{"family", o.value("family")},
				       {"category", o.value("category")},
				       {"popularity", o.value("popularity")},
				       {"weights", weights}});
	}
	return QJsonDocument(QJsonObject{{"families", arr}}).toJson(QJsonDocument::Compact);
}

} // namespace

QThreadPool *Pool()
{
	static QThreadPool *pool = [] {
		auto *p = new QThreadPool();
		p->setMaxThreadCount(3);
		return p;
	}();
	return pool;
}

void Shutdown()
{
	Pool()->clear();
	Pool()->waitForDone(5000);
}

std::vector<Entry> Catalog()
{
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		if (!g_catalog.empty())
			return g_catalog;
	}
	const QString path = CacheDir() + "/google-fonts-catalog.json";
	std::vector<Entry> entries;
	const QFileInfo fi(path);
	if (fi.exists() && fi.lastModified().secsTo(QDateTime::currentDateTime()) < kCatalogTtlSecs) {
		QFile f(path);
		if (f.open(QIODevice::ReadOnly))
			entries = ParseCatalog(f.readAll());
	}
	if (entries.empty()) {
		const QByteArray raw = CurlGet("https://fonts.google.com/metadata/fonts", QByteArray(), 20);
		entries = ParseCatalog(raw);
		if (!entries.empty()) {
			QFile f(path);
			if (f.open(QIODevice::WriteOnly))
				f.write(TrimmedCatalog(raw));
		}
	}
	if (entries.empty()) {
		QFile f(path); // stale beats nothing
		if (f.open(QIODevice::ReadOnly))
			entries = ParseCatalog(f.readAll());
	}
	if (entries.empty()) {
		int rank = 0;
		for (const char *name : {"Roboto", "Open Sans", "Montserrat", "Lato", "Poppins", "Inter", "Oswald", "Raleway",
					 "Nunito", "Playfair Display", "Rubik", "Bebas Neue", "Anton", "Bangers",
					 "Press Start 2P", "Pacifico", "Caveat", "Fira Code", "JetBrains Mono"})
			entries.push_back({QString::fromUtf8(name), QString(), rank++});
	}
	std::sort(entries.begin(), entries.end(),
		  [](const Entry &a, const Entry &b) { return a.popularity < b.popularity; });
	std::lock_guard<std::mutex> lock(g_mutex);
	g_catalog = entries;
	return g_catalog;
}

QString CachedFile(const QString &family)
{
	const QString path = FileFor(family);
	return QFileInfo(path).size() > 1024 ? path : QString();
}

QString Download(const QString &family, QString *error, int timeoutSec)
{
	const QString cached = CachedFile(family);
	if (!cached.isEmpty())
		return cached;
	// An old user agent gets plain TTF links rather than WOFF2.
	const QByteArray css = CurlGet(
		QString("https://fonts.googleapis.com/css2?family=%1:wght@400").arg(QString(family).replace(' ', '+')),
		"Mozilla/4.0", std::min(timeoutSec, 15));
	static const QRegularExpression re(R"(url\((https://[^\)]+\.ttf)\))");
	const auto m = re.match(QString::fromUtf8(css));
	if (!m.hasMatch()) {
		if (error)
			*error = QString::fromUtf8(obs_module_text("FontPicker.Error.NotFound")).arg(family);
		return QString();
	}
	const QString target = FileFor(family);
	if (!CurlDownload(m.captured(1), target, timeoutSec)) {
		if (error)
			*error = QString::fromUtf8(obs_module_text("FontPicker.Error.Download")).arg(family);
		return QString();
	}
	return target;
}

QString LoadForPreview(const QString &file)
{
	if (file.isEmpty())
		return QString();
	const int id = QFontDatabase::addApplicationFont(file);
	if (id < 0)
		return QString();
	const QStringList families = QFontDatabase::applicationFontFamilies(id);
	std::lock_guard<std::mutex> lock(g_mutex);
	for (const QString &f : families)
		g_previewLoaded.insert(f.toLower());
	return families.isEmpty() ? QString() : families.first();
}

bool IsPreviewOnly(const QString &family)
{
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		if (!g_previewLoaded.contains(family.toLower()))
			return false;
	}
	return !IsInstalled(family);
}

bool IsInstalled(const QString &family)
{
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		if (g_installedThisSession.contains(family.toLower()))
			return true;
	}
#ifdef _WIN32
	// Installed for this user or for everyone: a registry value per font.
	for (HKEY root : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
		HKEY key = nullptr;
		if (RegOpenKeyExW(root, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_READ, &key) !=
		    ERROR_SUCCESS)
			continue;
		wchar_t name[512];
		DWORD index = 0;
		DWORD len = 512;
		bool found = false;
		while (!found && RegEnumValueW(key, index++, name, &len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
			const QString n = QString::fromWCharArray(name, int(len));
			found = n.startsWith(family + " (", Qt::CaseInsensitive) ||
				n.compare(family, Qt::CaseInsensitive) == 0 ||
				n.startsWith(family + " Regular", Qt::CaseInsensitive);
			len = 512;
		}
		RegCloseKey(key);
		if (found)
			return true;
	}
	return false;
#else
	return QFontDatabase::families().contains(family, Qt::CaseInsensitive);
#endif
}

InstallResult InstallForUser(const QString &family, const QString &file)
{
	InstallResult r;
	r.family = family;
	const QString loaded = LoadForPreview(file); // Qt knows its real family name
	if (!loaded.isEmpty())
		r.family = loaded;
#ifdef _WIN32
	// Per-user fonts (Windows 10 1809+): the file in the user's font folder
	// and a value under HKCU. No admin rights needed.
	const QString dir = QDir::fromNativeSeparators(qEnvironmentVariable("LOCALAPPDATA")) + "/Microsoft/Windows/Fonts";
	QDir().mkpath(dir);
	const QString target = dir + "/" + QFileInfo(file).fileName();
	if (!QFileInfo::exists(target) && !QFile::copy(file, target)) {
		r.error = QString::fromUtf8(obs_module_text("FontPicker.Error.Install")).arg(family);
		return r;
	}
	HKEY key = nullptr;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, nullptr, 0,
			    KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
		const std::wstring name = (r.family + " (TrueType)").toStdWString();
		const std::wstring value = QDir::toNativeSeparators(target).toStdWString();
		RegSetValueExW(key, name.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE *>(value.c_str()),
			       DWORD((value.size() + 1) * sizeof(wchar_t)));
		RegCloseKey(key);
	}
	// Available to GDI in every process this session, OBS included.
	if (AddFontResourceW(QDir::toNativeSeparators(target).toStdWString().c_str()) == 0) {
		r.error = QString::fromUtf8(obs_module_text("FontPicker.Error.Install")).arg(family);
		return r;
	}
	DWORD_PTR result = 0;
	SendMessageTimeoutW(HWND_BROADCAST, WM_FONTCHANGE, 0, 0, SMTO_ABORTIFHUNG, 2000, &result);
	r.file = target;
#elif defined(__APPLE__)
	const QString target = QDir::homePath() + "/Library/Fonts/" + QFileInfo(file).fileName();
	if (!QFileInfo::exists(target) && !QFile::copy(file, target)) {
		r.error = QString::fromUtf8(obs_module_text("FontPicker.Error.Install")).arg(family);
		return r;
	}
	r.file = target;
#else
	const QString dir = QDir::homePath() + "/.local/share/fonts";
	QDir().mkpath(dir);
	const QString target = dir + "/" + QFileInfo(file).fileName();
	if (!QFileInfo::exists(target) && !QFile::copy(file, target)) {
		r.error = QString::fromUtf8(obs_module_text("FontPicker.Error.Install")).arg(family);
		return r;
	}
	QProcess::execute("fc-cache", {"-f", dir});
	r.file = target;
#endif
	r.ok = true;
	std::lock_guard<std::mutex> lock(g_mutex);
	g_installedThisSession.insert(family.toLower());
	g_installedThisSession.insert(r.family.toLower());
	DebugLogger::LogInfoFormat("FontPicker", "Installed '%s' for this user (%s)", r.family.toUtf8().constData(),
				   r.file.toUtf8().constData());
	return r;
}

} // namespace GoogleFonts
} // namespace StreamUP
