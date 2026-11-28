#include "videos.hpp"

#include "../utilities/http-client.hpp"
#include "../version.h"

#include <obs-module.h>
#include <util/platform.h>

#include <QApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QSet>

#include <cstdlib>
#include <string>

namespace StreamUP {
namespace Videos {

namespace {

// Everything below is touched on the UI thread only; the downloads hand their
// results over with QueuedConnection.
QList<Video> g_masters;
QList<Video> g_videos;
QHash<QString, QImage> g_thumbs;
QSet<QString> g_fetching;

struct Listener {
	QPointer<QObject> ctx;
	std::function<void()> fn;
};
QList<Listener> g_listeners;

void Notify()
{
	for (int i = g_listeners.size() - 1; i >= 0; --i)
		if (!g_listeners[i].ctx)
			g_listeners.removeAt(i);
	const QList<Listener> now = g_listeners;
	for (const Listener &l : now)
		if (l.ctx)
			l.fn();
}

void OnUi(std::function<void()> fn)
{
	QMetaObject::invokeMethod(qApp, std::move(fn), Qt::QueuedConnection);
}

bool ValidId(const QString &id)
{
	static const QString ok = QStringLiteral("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-");
	if (id.size() != 11)
		return false;
	for (QChar c : id)
		if (!ok.contains(c))
			return false;
	return true;
}

// Only YouTube links leave the plugin, whatever the list says.
bool SafeUrl(const QString &url)
{
	return url.startsWith(QStringLiteral("https://www.youtube.com/")) || url.startsWith(QStringLiteral("https://youtu.be/"));
}

Video Parse(const QJsonObject &o)
{
	Video v;
	v.release = o.value("release").toString();
	v.title = o.value("title").toString();
	for (const QJsonValue &p : o.value("parts").toArray())
		v.parts << p.toString();
	v.url = o.value("url").toString();
	v.videoId = o.value("videoId").toString();
	v.length = o.value("length").toString();
	v.chapter = o.value("chapter").toBool();
	return v;
}

void Apply(const std::string &json)
{
	const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
	if (!doc.isObject())
		return;
	const QJsonObject root = doc.object();
	QList<Video> masters;
	for (const QJsonValue &v : root.value("masters").toArray()) {
		Video m = Parse(v.toObject());
		m.parts = {QStringLiteral("whats-new")};
		if (SafeUrl(m.url) && ValidId(m.videoId))
			masters << m;
	}
	QList<Video> list;
	for (const QJsonValue &v : root.value("videos").toArray()) {
		Video video = Parse(v.toObject());
		if (SafeUrl(video.url) && ValidId(video.videoId) && !video.parts.isEmpty())
			list << video;
	}
	g_masters = masters;
	g_videos = list;
	blog(LOG_INFO, "[StreamUP] Video links: %d, master videos: %d", int(list.size()), int(masters.size()));
	Notify();
}

// YouTube has a 1280x720 thumbnail for most videos and a 480x360 one (with
// bars top and bottom) for all of them; the bars are cropped off.
void FetchThumb(const QString &id, bool large)
{
	const std::string url = QStringLiteral("https://i.ytimg.com/vi/%1/%2.jpg")
					.arg(id, large ? QStringLiteral("maxresdefault") : QStringLiteral("hqdefault"))
					.toStdString();
	HttpClient::MakeAsyncGetRequest(url, [id, large](const std::string &, const std::string &body, bool ok) {
		QImage img;
		if (ok && !body.empty())
			img.loadFromData(reinterpret_cast<const uchar *>(body.data()), int(body.size()));
		// A missing maxres thumbnail comes back as a small grey picture.
		if (!img.isNull() && large && img.width() < 640)
			img = QImage();
		if (!img.isNull() && img.width() * 9 != img.height() * 16) {
			const int h = img.width() * 9 / 16;
			img = img.copy(0, (img.height() - h) / 2, img.width(), h);
		}
		OnUi([id, large, img]() {
			if (img.isNull()) {
				if (large)
					FetchThumb(id, false);
				return;
			}
			g_thumbs.insert(id, img);
			Notify();
		});
	});
}

// A release as a number to compare: "3.0" -> 3000, "3.12" -> 3012. Rows with
// no release count as the oldest.
int ReleaseKey(const QString &release)
{
	const QStringList bits = release.split('.');
	if (bits.isEmpty() || bits[0].isEmpty())
		return 0;
	return bits.value(0).toInt() * 1000 + bits.value(1).toInt();
}

int ThisRelease()
{
	return PROJECT_VERSION_MAJOR * 1000 + PROJECT_VERSION_MINOR;
}

} // namespace

void Refresh()
{
	if (const char *file = std::getenv("STREAMUP_VIDEO_LIST"); file && *file) {
		if (char *text = os_quick_read_utf8_file(file)) {
			std::string json = text;
			bfree(text);
			blog(LOG_INFO, "[StreamUP] Video links read from %s (STREAMUP_VIDEO_LIST)", file);
			OnUi([json]() { Apply(json); });
		}
		return;
	}
	HttpClient::MakeAsyncGetRequest("https://api.streamup.tips/videos",
					[](const std::string &, const std::string &body, bool ok) {
						if (!ok || body.empty())
							return;
						OnUi([body]() { Apply(body); });
					});
}

std::optional<Video> Master()
{
	for (const Video &m : g_masters)
		if (ReleaseKey(m.release) == ThisRelease())
			return m;
	return std::nullopt;
}

std::optional<Video> ForPart(const QString &part)
{
	std::optional<Video> best;
	for (const Video &v : g_videos) {
		const int key = ReleaseKey(v.release);
		if (!v.parts.contains(part) || key > ThisRelease())
			continue;
		if (!best || key > ReleaseKey(best->release))
			best = v;
	}
	return best;
}

QImage Thumbnail(const QString &videoId)
{
	if (!ValidId(videoId))
		return {};
	auto it = g_thumbs.constFind(videoId);
	if (it != g_thumbs.constEnd())
		return it.value();
	if (!g_fetching.contains(videoId)) {
		g_fetching.insert(videoId);
		FetchThumb(videoId, true);
	}
	return {};
}

void OnChanged(QObject *ctx, std::function<void()> fn)
{
	g_listeners.append({QPointer<QObject>(ctx), std::move(fn)});
}

} // namespace Videos
} // namespace StreamUP
