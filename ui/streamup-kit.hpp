#pragma once

// Small building blocks every StreamUP window shares, so a setting, a status,
// a folded section, an empty list, a toast or the membership card looks and
// reads the same wherever it appears. Each one is one line of text at most;
// anything longer belongs in a "?" tooltip.

#include <QImage>
#include <QString>
#include <QWidget>
#include <QFrame>
#include <QList>

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

// One spacing scale for every StreamUP window (design px, pass through S()).
namespace Space {
constexpr int Page = 20;    // window content, left and right
constexpr int PageTop = 16; // window content, top and bottom
constexpr int Block = 12;   // between blocks (header, a section, a note)
constexpr int CardX = 16;   // inside a card, left and right
constexpr int CardY = 6;    // inside a card, top and bottom (rows add their own)
constexpr int Row = 10;     // a row's own top and bottom padding
constexpr int Gap = 8;      // between widgets on one row
} // namespace Space

// A rounded card of rows, one thin line between them. Every list of plugins,
// fonts or products uses this so their rows look and line up the same:
// [leading] title + small detail line ......... [trailing widgets]
class RowList : public QFrame {
public:
	explicit RowList(QWidget *parent = nullptr);
	QWidget *addRow(const QString &title, const QString &detail, const QList<QWidget *> &trailing = {},
			QWidget *leading = nullptr, const QString &detailTip = QString());
	int count() const { return m_rows; }

private:
	QVBoxLayout *m_v = nullptr;
	int m_rows = 0;
};

// A short note in a tinted box with an icon, e.g. what "Install anyway" means.
QWidget *Note(Status status, const QString &text, QWidget *parent = nullptr);

// A scroll area that asks for the height of its contents, so a window opens
// exactly as tall as it needs and only scrolls when the screen runs out.
// Returns the layout to fill; the page margins are already set.
QVBoxLayout *ScrollingBody(QVBoxLayout *shellContent);

// Sizes a window to its contents at the given design width: as tall as it
// needs, at most 85% of the screen.
void FitToContent(QWidget *window, int designWidth);

// A settings page whose feature is switched off on the Features page: one line
// saying so with a button to go there, and the settings below greyed out.
QWidget *SwitchedOffBanner(const QString &featureName);
void DimForSwitchedOff(QWidget *content, bool off);
void SetOpenFeaturesHook(std::function<void()> fn);

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
	// Click to see it big in a lightbox over the window, with a title and a line.
	void setZoom(const QString &title, const QString &line = QString());
	// Show the whole picture (letterboxed) instead of filling the slot.
	void setFit(bool fit) { m_fit = fit; update(); }
	// A thin outline round the picture so it stands off the card behind it.
	void setOutline(bool on) { m_outline = on; update(); }

protected:
	void paintEvent(QPaintEvent *) override;
	void mouseReleaseEvent(QMouseEvent *e) override;

private:
	QMovie *m_movie = nullptr;
	QImage m_still;
	QString m_placeholder;
	QString m_folder;
	QString m_id;
	QString m_zoomTitle;
	QString m_zoomLine;
	bool m_zoom = false;
	bool m_fit = false;
	bool m_outline = true;
};

// Opens a picture or clip big over a window and dims the rest of it. A click
// outside it, the X or Esc closes it. Buttons go in a row under the line.
QWidget *Lightbox(QWidget *window, const QString &folder, const QString &id, const QString &title,
		  const QString &line = QString(), const QList<QWidget *> &buttons = {});

} // namespace Kit
} // namespace StreamUP
