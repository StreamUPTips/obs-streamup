#pragma once

// StreamUP UI kit: the small building blocks every StreamUP window shares, so a
// setting, a status, a section, an empty list, a toast, a picture or a lightbox
// looks and reads the same in every StreamUP plugin. Each one is one line of
// text at most; anything longer belongs in a "?" tooltip.
//
// Text: the kit looks its few strings up with obs_module_text() under "Kit.*"
// keys, so a plugin can word them in its own locale; a missing key falls back
// to the English built into the kit.
//
// Icons: names like "camera" resolve through IconPath(), which prefers the
// kit's own Tabler outline set (:/streamup-ui/icons/), then the plugin's
// :images/icons/ui/ files, picking the -dark or -light file for the theme.

#include <QImage>
#include <QString>
#include <QWidget>
#include <QFrame>
#include <QList>

#include <functional>

#include "theme.hpp"

class QLabel;
class QVBoxLayout;
class QMovie;
class QPushButton;
class QLineEdit;

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

// How a toolbar button will look: its icon on a rounded tile the size of a
// toolbar button, with the caption and tooltip beside it. The Hotkey and
// WebSocket button windows show it live as you type.
class ButtonPreview : public QFrame {
public:
	explicit ButtonPreview(QWidget *parent = nullptr);
	// An empty path shows a dashed empty tile.
	void setIconPath(const QString &path);
	void setText(const QString &caption, const QString &tooltip);

private:
	QLabel *m_tile = nullptr;
	QLabel *m_caption = nullptr;
	QLabel *m_tooltip = nullptr;
};

// A labelled text field for a form card: the label on the left, the field
// filling the rest. Returns the row; the field comes back through *edit.
QWidget *FieldRow(const QString &label, const QString &placeholder, QLineEdit **edit);

// The navigation rail every sidebar window uses (Settings, My Products, the
// controls window): a soft raised row for the current page, a fainter one on
// hover, never a saturated accent block. The accent stays for the main action.
QString NavListQss();

// Theme-aware icon file for a kit icon name ("camera" ->
// ":/streamup-ui/icons/camera-dark.svg" on a dark theme). Falls back to the
// plugin's own ":images/icons/ui/<name>-dark|light.svg". An absolute path or a
// ":" resource path is returned unchanged.
QString IconPath(const QString &name);
// Replaces how icon names resolve, for a plugin that keeps its icons elsewhere.
void SetIconResolver(std::function<QString(const QString &)> resolver);

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
	// Runs after the person opens or closes it, so a window can refit.
	void onToggled(std::function<void(bool)> fn) { m_onToggled = std::move(fn); }

private:
	void setOpen(bool open, bool save);
	QString m_key;
	QString m_label;
	int m_count = -1;
	QPushButton *m_header = nullptr;
	QWidget *m_body = nullptr;
	QVBoxLayout *m_bodyLayout = nullptr;
	bool m_open = false;
	std::function<void(bool)> m_onToggled;
};

// An empty list: a large icon, one line, and the next step as a button.
QWidget *EmptyState(const QString &iconPath, const QString &line, const QString &button = QString(),
		    std::function<void()> onClick = {}, QWidget *extra = nullptr);

// A small note in the bottom corner of OBS for a couple of seconds: an icon
// and a few words. Replaces tray messages. Safe from any thread.
void Toast(const QString &text, Status status = Status::Ok, const QString &iconPath = QString());


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
	// As tall as the picture is for the width it gets, and drawn whole.
	void setNaturalAspect();
	bool hasHeightForWidth() const override { return m_natural; }
	int heightForWidth(int w) const override;

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
	bool m_natural = false;
	QImage m_scaled;       // the current frame at this slot's device size
	qint64 m_scaledKey = 0;
};

// Opens a picture or clip big over a window and dims the rest of it. A click
// outside it, the X or Esc closes it. Buttons go in a row under the line.
QWidget *Lightbox(QWidget *window, const QString &folder, const QString &id, const QString &title,
		  const QString &line = QString(), const QList<QWidget *> &buttons = {});


} // namespace Kit
} // namespace StreamUP
