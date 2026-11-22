#pragma once

// Small building blocks every StreamUP window shares, so a setting, a status,
// a folded section, an empty list, a toast or the membership card looks and
// reads the same wherever it appears. Each one is one line of text at most;
// anything longer belongs in a "?" tooltip.

#include <QImage>
#include <QString>
#include <QWidget>

#include <functional>

class QLabel;
class QVBoxLayout;
class QMovie;
class QPushButton;

namespace StreamUP {
namespace Kit {

enum class Status { Ok, Warn, Bad, Info, Neutral };

// A coloured pill with an icon and a few words: "✓ Up to date".
QWidget *StatusPill(Status status, const QString &text, QWidget *parent = nullptr);

// A small "?" that shows its text as a tooltip. Empty help returns nullptr.
QLabel *HelpDot(const QString &help, QWidget *parent = nullptr);

// One setting on one line: optional icon, label, optional "?", the control on
// the right.
QWidget *SettingRow(const QString &label, QWidget *control, const QString &help = QString(),
		    const QString &iconPath = QString());

// A folded "More options" section. Whether it is open is remembered per key.
class MoreOptions : public QWidget {
public:
	explicit MoreOptions(const QString &key, const QString &label = QString(), QWidget *parent = nullptr);
	QVBoxLayout *body() const { return m_bodyLayout; }
	void setCountHint(int count);

private:
	void setOpen(bool open, bool save);
	QString m_key;
	QString m_label;
	int m_count = -1;
	QPushButton *m_header = nullptr;
	QWidget *m_body = nullptr;
	QVBoxLayout *m_bodyLayout = nullptr;
	bool m_open = false;
};

// An empty list: a large icon, one line, and the next step as a button.
QWidget *EmptyState(const QString &iconPath, const QString &line, const QString &button = QString(),
		    std::function<void()> onClick = {}, QWidget *extra = nullptr);

// A small note in the bottom corner of OBS for a couple of seconds: an icon
// and a few words. Replaces tray messages. Safe from any thread.
void Toast(const QString &text, Status status = Status::Ok, const QString &iconPath = QString());

// The StreamUP Membership card. Strip: one row for a footer. Card: a little
// taller, for an empty state or About. Returns nullptr for members (there is
// no member detection yet, so it always shows; see IsMember).
enum class MemberStyle { Strip, Card };
QWidget *MembershipCard(MemberStyle style = MemberStyle::Strip, const QString &line = QString());
bool IsMember();
QString MembershipUrl();
QString SupportUrl();

// A picture slot: an animated WebP or GIF when there is one, a still PNG
// otherwise, a quiet placeholder when there is neither. Files are looked up as
// data/<folder>/<id>.webp|.gif|.png. Animations run only while setPlaying(true).
class PreviewSlot : public QWidget {
public:
	PreviewSlot(const QString &folder, const QString &id, int height, const QString &placeholder = QString(),
		    QWidget *parent = nullptr);
	void setPlaying(bool on);
	bool animated() const { return m_movie != nullptr; }
	bool hasPicture() const;

protected:
	void paintEvent(QPaintEvent *) override;

private:
	QMovie *m_movie = nullptr;
	QImage m_still;
	QString m_placeholder;
};

} // namespace Kit
} // namespace StreamUP
