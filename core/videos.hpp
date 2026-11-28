#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace StreamUP {
namespace Videos {

// A tutorial video on YouTube that a part of StreamUP links to. The list comes
// from api.streamup.tips/videos (the Notion "StreamUP Plugin Videos" table), so
// a new video shows up in everyone's OBS without a plugin update.
struct Video {
	QString release;   // the StreamUP release it covers, e.g. "3.0"
	QString title;
	QStringList parts; // the StreamUP parts that link to it, e.g. "mixer-dock"
	QString url;       // opens on YouTube, already at the right time
	QString videoId;   // for the thumbnail
	QString length;    // "2:00", as typed in Notion
	bool chapter = false; // a chapter of the master video, not its own video
};

// Fetches the list in the background. Called once when OBS has loaded.
// STREAMUP_VIDEO_LIST=<file> reads it from a JSON file instead, to try links
// before they are in Notion.
void Refresh();

// This release's master video (the whole update in one video), or nothing
// until it is up. A patch (3.0.2) shows its release's (3.0) video.
std::optional<Video> Master();

// The video for a part, or nothing if it has none yet. When a part has a video
// for more than one release, the newest one this StreamUP is not older than.
std::optional<Video> ForPart(const QString &part);

// The video's thumbnail, or a null image while it is still downloading.
QImage Thumbnail(const QString &videoId);

// Calls fn on the UI thread whenever the list or a thumbnail arrives, for as
// long as ctx is alive.
void OnChanged(QObject *ctx, std::function<void()> fn);

} // namespace Videos
} // namespace StreamUP
