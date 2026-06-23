#pragma once

// StreamUP UI Gallery — locked design-system tokens + style helpers.
//
// This is a SELF-CONTAINED, faithful implementation of the StreamUP UI
// standard (the 2026-06-22 audit decisions). It exists so the dev-tool
// gallery can render every component without pulling the legacy ui-styles.cpp.
// When the shared `obs-streamup-ui` library lands, this header is the clean
// seed for it. Keep it in sync with .claude/skills/streamup-obs-ui.

#include <QString>
#include <QApplication>
#include <QFont>
#include <QFontInfo>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QPainter>
#include <QRectF>
#include <QPointF>
#include <QRegularExpression>
#include <algorithm>
#include <climits>
#include <cmath>

namespace StreamUP {
namespace UIStyles {

// ── OS text-size scaling (decision #5) ───────────────────────────────────────
// The factor is derived from the application font's pixel size relative to the
// platform baseline. It must NOT be permanently latched: ui_scale() can be hit
// before OBS applies its font (giving 1.0 forever), and the user can change the
// OS text size at runtime. We therefore cache against a cheap key (the current
// QApplication::font() pixel size) and recompute only when that key changes —
// QFontInfo resolution is the expensive part and is skipped on the hot path
// (S() is called per-paint-frame during animations), while a genuine font/text-
// size change is picked up the next frame.
inline qreal ui_scale()
{
#ifdef __APPLE__
	constexpr qreal baseline = 13.0;
#else
	constexpr qreal baseline = 12.0;
#endif
	const int key = QApplication::font().pixelSize(); // cheap; -1 if point-sized
	static int s_key = INT_MIN;
	static qreal s_scale = 1.0;
	if (key == s_key)
		return s_scale;
	s_key = key;
	const int px = QFontInfo(QApplication::font()).pixelSize();
	s_scale = (px <= 0) ? 1.0 : std::clamp(px / baseline, 1.0, 3.0);
	return s_scale;
}

inline int S(int px) { return (int)std::lround(px * ui_scale()); }
// Fractional overload for paint geometry that needs sub-pixel design values
// (e.g. a 3.5px glyph radius). int S() truncates these via lround; use Sf()
// where the fractional part matters to the painted result.
inline qreal Sf(qreal px) { return px * ui_scale(); }

// Draw text vertically centred inside `rect` on a CONSTANT cap-height baseline.
//
// Why not AlignCenter / ascent-descent: the em box reserves descender space that
// caps don't use, so labels read high. Why not the tight ink box either: that
// centres each STRING on its own glyphs, so a word with a descender ("Danger",
// "queued") lands its cap-line differently from one without ("Primary", "NEW") —
// a row of pills then looks inconsistent. Centring the font's cap height instead
// puts the capital-letter band dead-centre and gives EVERY label the same
// baseline regardless of its descenders. One routine for pills/badges/segmented/
// progress so they all line up.
inline void drawCentredText(QPainter &p, const QFont &f, const QString &text, const QRectF &rect)
{
	if (text.isEmpty())
		return;
	const QFontMetricsF fm(f);
	const qreal x = rect.left() + (rect.width() - fm.horizontalAdvance(text)) / 2.0;
	const qreal baseline = rect.center().y() + fm.capHeight() / 2.0;
	p.setFont(f);
	p.drawText(QPointF(x, baseline), text);
}

inline bool fontInstalled(const QString &fam)
{
	static const QStringList fams = QFontDatabase::families();
	return fams.contains(fam, Qt::CaseInsensitive);
}

// Branded BODY font for StreamUP custom windows — modern Apple-style.
// Prefers SF Pro, else Inter (closest free substitute), else clean Windows UI.
// Only applied to our custom windows — never docks / native OBS surfaces.
inline QFont brandFont()
{
	QFont f = QApplication::font();
	for (const QString &fam : {QStringLiteral("SF Pro Text"), QStringLiteral("SF Pro Display"),
				   QStringLiteral("Inter"), QStringLiteral("Segoe UI Variable Text"),
				   QStringLiteral("Segoe UI")}) {
		if (fontInstalled(fam)) {
			f.setFamily(fam);
			break;
		}
	}
	f.setStyleStrategy(QFont::PreferAntialias);
	return f;
}

// Branded DISPLAY font for titles / section headers — Bebas Neue (condensed,
// all-caps, modern). Falls back to a condensed face, else bold body font.
inline QFont displayFont(int px, int weight = 700)
{
	QFont f = brandFont();
	for (const QString &fam : {QStringLiteral("Bebas Neue Pro"), QStringLiteral("Bebas Neue"),
				   QStringLiteral("Oswald")}) {
		if (fontInstalled(fam)) {
			f.setFamily(fam);
			break;
		}
	}
	f.setPixelSize(S(px));
	f.setWeight(QFont::Weight(weight));
	f.setLetterSpacing(QFont::PercentageSpacing, 102);
	f.setStyleStrategy(QFont::PreferAntialias);
	return f;
}

inline QString scale_qss(const QString &qss)
{
	if (ui_scale() <= 1.0)
		return qss;
	static const QRegularExpression rx(QStringLiteral("(-?\\d+)px"));
	QString out;
	out.reserve(qss.size() + 16);
	qsizetype last = 0;
	auto it = rx.globalMatch(qss);
	while (it.hasNext()) {
		const QRegularExpressionMatch m = it.next();
		out += qss.mid(last, m.capturedStart() - last);
		out += QString::number(S(m.captured(1).toInt()));
		out += QStringLiteral("px");
		last = m.capturedEnd();
	}
	out += qss.mid(last);
	return out;
}

// ── Locked colour tokens (decisions #1, #6) ──────────────────────────────────
namespace Colors {
	constexpr const char *BG_DARKEST = "#1e1e2e";   // base / window
	constexpr const char *BG_PRIMARY = "#272738";   // card surface + input-on-base
	constexpr const char *BG_SECONDARY = "#1a1a2a"; // code block + input-on-card
	constexpr const char *BG_TERTIARY = "#313244";  // hover surface
	constexpr const char *SIDEBAR_BG = "#181825";   // nav rail

	// Input surface is context-aware (decision #7): an input must always
	// contrast the surface it sits on. On the base background use the lighter
	// grey so it reads as an input while staying flat; inside a card/group use
	// the darker grey so it contrasts the (lighter) card.
	constexpr const char *INPUT_ON_BASE = "#272738"; // input on #1e1e2e base
	constexpr const char *INPUT_ON_CARD = "#1a1a2a"; // input inside a #272738 card

	constexpr const char *PRIMARY_COLOR = "#0076df";   // accent
	constexpr const char *PRIMARY_HOVER = "#1f8ce8";   // lighter than base
	constexpr const char *PRIMARY_PRESSED = "#005abb";
	constexpr const char *PRIMARY_LIGHT = "#2997ff";   // lighter blue (selection)
	constexpr const char *TAG_COLOR = "#89b4fa";       // link/tag text only

	constexpr const char *COLOR_SUCCESS = "#65c466";   // iOS green
	constexpr const char *COLOR_WARNING = "#fab387";
	constexpr const char *COLOR_DANGER = "#e35d55";    // softened red (was iOS #ff453a)

	constexpr const char *TEXT_PRIMARY = "#cdd6f4";
	constexpr const char *TEXT_SECONDARY = "#bac2de";
	constexpr const char *TEXT_MUTED = "#6c7086";
	constexpr const char *TEXT_PLACEHOLDER = "rgba(205,214,244,0.35)";

	constexpr const char *SWITCH_TRACK_OFF = "#3a3a3d";
	constexpr const char *SEPARATOR = "#313244";        // 1px hairline divider

	// Consolidated literals that used to be inlined in several helpers.
	constexpr const char *POPUP_BG = "#1a1c32";         // opaque popup / menu fill
	constexpr const char *POPUP_BORDER = "rgba(255,255,255,0.12)"; // popup hairline
	constexpr const char *SELECTION = "rgba(41,151,255,0.55)";     // lighter-blue (PRIMARY_LIGHT) row/menu/combo selection
	constexpr const char *HOVER_ROW = "rgba(255,255,255,0.05)";    // list/menu item hover
	constexpr const char *BORDER_SUBTLE = "rgba(255,255,255,0.06)"; // version badge / neutral-btn hover
	// Scrollbar handle uses the accent (matches StreamUP.obt: handle =
	// --primary_color, hover = --primary_hover, pressed = --primary_light).
}

// ── Size tokens (minimal set used by gallery + switch-button) ─────────────────
namespace Sizes {
	constexpr int FONT_SIZE_LARGE = 18;
	constexpr int FONT_SIZE_HEADING = 14;
	constexpr int FONT_SIZE_NORMAL = 14;
	constexpr int FONT_SIZE_SMALL = 13;
	constexpr int FONT_SIZE_TINY = 12;
	constexpr int FONT_SIZE_BUTTON = 11;
	constexpr int FONT_WEIGHT_NORMAL = 500;
	constexpr int FONT_WEIGHT_BOLD = 800;

	constexpr int RADIUS_CARD = 18;   // decision #10
	constexpr int RADIUS_INPUT = 8;   // decision #7
	constexpr int RADIUS_SMALL = 4;   // dock-native / scrollbar handle
	constexpr int BUTTON_HEIGHT = 28; // decision #5
	constexpr int BUTTON_RADIUS = 15; // (28+2)/2 pill, decision #4
	constexpr int INPUT_HEIGHT = 28;
	constexpr int SCROLLBAR_SIZE = 12; // decision #11 (12px channel, 6px capsule handle)
}

// ── Style-string helpers (return scaled QSS) ─────────────────────────────────

// Pill button (decision #4/#5). variant: primary|danger|success|warning|outline|neutral
// NOTE: a same-colour border is REQUIRED — Qt only clips border-radius on the
// background fill when a border is present; `border:none` renders square corners.
inline QString buttonStyle(const QString &variant = "primary")
{
	QString fill, text, hover, pressed, border;
	if (variant == "primary") {
		fill = Colors::PRIMARY_COLOR; text = "#ffffff";
		hover = Colors::PRIMARY_HOVER; pressed = Colors::PRIMARY_PRESSED;
		border = QString("1px solid %1").arg(fill);
	} else if (variant == "danger") {
		fill = Colors::COLOR_DANGER; text = "#ffffff";
		hover = "#ec7a73"; pressed = "#c44d46";
		border = QString("1px solid %1").arg(fill);
	} else if (variant == "success") {
		fill = Colors::COLOR_SUCCESS; text = "#11111b";
		hover = "#79d07a"; pressed = "#54b055";
		border = QString("1px solid %1").arg(fill);
	} else if (variant == "warning") {
		fill = Colors::COLOR_WARNING; text = "#11111b"; // dark ink on amber
		hover = "#fbc4a0"; pressed = "#e89b6f";
		border = QString("1px solid %1").arg(fill);
	} else if (variant == "outline") {
		fill = "transparent"; text = Colors::PRIMARY_COLOR;
		hover = "rgba(0,118,223,0.08)"; pressed = "rgba(0,118,223,0.16)";
		border = QString("1px solid %1").arg(Colors::PRIMARY_COLOR);
	} else { // neutral
		fill = Colors::BG_TERTIARY; text = Colors::TEXT_PRIMARY;
		hover = "rgba(255,255,255,0.06)"; pressed = "rgba(255,255,255,0.10)";
		border = QString("1px solid %1").arg(fill);
	}
	const QString qss = QString(
		"QPushButton{background:%1;color:%2;border:%3;border-radius:15px;"
		"min-height:28px;padding:0 18px;font-size:11px;font-weight:800;}"
		"QPushButton:hover{background:%4;}"
		"QPushButton:pressed{background:%5;}"
		"QPushButton:disabled{background:#2a2a3a;border:1px solid #2a2a3a;"
		"color:rgba(205,214,244,0.35);}")
		.arg(fill, text, border, hover, pressed);
	return scale_qss(qss);
}

// Inputs (decision #7). onCard=false → input sits on the base bg (lighter grey);
// onCard=true → input sits inside a card/group (darker grey) so it still
// contrasts. Borderless until focus → 2px accent.
// macOS-feel text field: subtle hairline edge, 6px radius, soft accent focus.
inline QString lineEditStyle(bool onCard = false)
{
	const char *bg = onCard ? Colors::INPUT_ON_CARD : Colors::INPUT_ON_BASE;
	const QString qss = QString(
		"QLineEdit{background:%1;color:%2;border:1px solid transparent;"
		"border-radius:8px;min-height:28px;max-height:28px;padding:0 10px;font-size:13px;}"
		"QLineEdit:focus{border:2px solid %3;padding:0 9px;}"
		"QLineEdit::placeholder{color:%4;}")
		.arg(bg, Colors::TEXT_PRIMARY, Colors::PRIMARY_COLOR, Colors::TEXT_PLACEHOLDER);
	return scale_qss(qss);
}

inline QString plainTextStyle(bool onCard = false)
{
	const char *bg = onCard ? Colors::INPUT_ON_CARD : Colors::INPUT_ON_BASE;
	const QString qss = QString(
		"QPlainTextEdit{background:%1;color:%2;border:1px solid transparent;"
		"border-radius:8px;padding:8px 10px;font-size:13px;}"
		"QPlainTextEdit:focus{border:2px solid %3;}")
		.arg(bg, Colors::TEXT_PRIMARY, Colors::PRIMARY_COLOR);
	return scale_qss(qss);
}

// The dropdown list renders as a SOLID opaque menu (POPUP_BG). Pair with
// makeComboAnimated() (glass.hpp) for the slide-down + fade open animation —
// no transparency, no DWM.
inline QString comboStyle(bool onCard = false)
{
	Q_UNUSED(onCard);
	// Opaque popup fill (matches menuStyle()); the field surface is painted by
	// MacComboBox itself and is unaffected by this popup-only stylesheet.
	const QString popupBg = QString(Colors::POPUP_BG);
	// IMPORTANT: no QComboBox{...} field rules. MacComboBox custom-paints the
	// field, and a field-level QSS makes QStyleSheetStyle's polish recompute the
	// combo height (QSS height + native combo frame), overwriting setFixedHeight
	// — which is why the combo rendered taller than the line edit/spin. Style ONLY
	// the popup here; the field height is controlled purely by setFixedHeight.
	const QString qss = QString(
		"QComboBox QAbstractItemView{background:%1;color:%2;"
		"border:1px solid %3;"
		"border-radius:9px;padding:4px;outline:none;}"
		"QComboBox QAbstractItemView::item{border-radius:7px;padding:3px 10px;"
		"margin:1px 4px;min-height:18px;}"
		"QComboBox QAbstractItemView::item:selected{background:%4;color:#ffffff;}")
		.arg(popupBg, Colors::TEXT_PRIMARY, Colors::POPUP_BORDER, Colors::SELECTION);
	return scale_qss(qss);
}

// Opaque dropdown menu (solid POPUP_BG fill, hairline border, rounded). Pair
// with makeMenuAnimated() for the slide-down + fade open animation.
inline QString menuStyle()
{
	const QString qss = QString(
		"QMenu{background:%1;color:%2;"
		"border:1px solid %3;border-radius:9px;padding:4px;}"
		"QMenu::item{padding:5px 12px;border-radius:7px;margin:1px 4px;}"
		"QMenu::item:selected{background:%4;color:#ffffff;}"
		"QMenu::item:disabled{color:%5;}"
		"QMenu::separator{height:1px;background:%6;margin:4px 8px;}")
		.arg(Colors::POPUP_BG, Colors::TEXT_PRIMARY, Colors::POPUP_BORDER,
		     Colors::SELECTION, Colors::TEXT_MUTED, Colors::SEPARATOR);
	return scale_qss(qss);
}

// Scrollbar (decision #11) is the custom-painted `ScrollBar` widget
// (ui-scrollbar.hpp) — QSS `border-radius` on a QScrollBar handle clips its
// corners (same failure as QSS pills), so the handle is hand-painted as a true
// capsule. Install it with `useScrollBars(area)`; there is no scrollbar QSS.

// Slider (decision: mirror StreamUP.obt) — grey groove, accent filled side,
// white round handle that gains an accent ring on hover/press.
inline QString sliderStyle()
{
	const QString qss = QString(
		"QSlider::groove:horizontal{height:6px;border-radius:3px;background:%1;}"
		"QSlider::sub-page:horizontal{height:6px;border-radius:3px;background:%2;}"
		"QSlider::add-page:horizontal{height:6px;border-radius:3px;background:%1;}"
		"QSlider::handle:horizontal{width:16px;height:16px;margin:-6px 0;"
		"border-radius:8px;background:#ffffff;border:2px solid %3;}"
		"QSlider::handle:horizontal:hover{background:#f0f0f0;}"
		"QSlider::handle:horizontal:pressed{background:#e0e0e0;}"
		"QSlider::groove:horizontal:disabled,QSlider::sub-page:horizontal:disabled{background:#2a2a3a;}"
		"QSlider::handle:horizontal:disabled{background:#a0a0a0;border:2px solid #2a2a3a;}")
		.arg(Colors::BG_TERTIARY, Colors::PRIMARY_COLOR, Colors::SIDEBAR_BG);
	return scale_qss(qss);
}

// Progress bar is the fully custom-painted `ProgressBar` widget (progress-bar.hpp)
// — QSS `QProgressBar::chunk` rounding isn't antialiased (jagged), so the track,
// chunk and caption are all hand-painted. No progress QSS helper.

// Table — header on base bg, transparent rows, hairline gridlines, rounded
// lighter-blue selection. Wrap the table in a RoundedContainer for corner clip.
inline QString tableStyle()
{
	const QString qss = QString(
		"QTableWidget,QTableView{background:%1;color:%2;border:none;outline:none;"
		"gridline-color:transparent;font-size:13px;selection-background-color:transparent;}"
		"QTableWidget::item,QTableView::item{padding:6px 10px;border:none;"
		"border-bottom:1px solid %3;}"
		"QTableWidget::item:hover,QTableView::item:hover{background:%4;}"
		"QTableWidget::item:selected,QTableView::item:selected{background:%5;color:#ffffff;}"
		"QHeaderView{background:transparent;}"
		"QHeaderView::section{background:%6;color:%7;padding:7px 10px;border:none;"
		"border-bottom:1px solid %3;font-size:12px;font-weight:700;}"
		"QTableCornerButton::section{background:%6;border:none;}")
		.arg(Colors::BG_PRIMARY, Colors::TEXT_PRIMARY, Colors::SEPARATOR,
		     Colors::HOVER_ROW, Colors::SELECTION, Colors::BG_TERTIARY, Colors::TEXT_PRIMARY);
	return scale_qss(qss);
}

// Tooltip — dark glass-style chip with a hairline border. Apply once on the
// dialog (or app) so every QToolTip in our custom window matches.
inline QString tooltipStyle()
{
	const QString qss = QString(
		"QToolTip{background:%1;color:%2;border:1px solid %3;border-radius:8px;"
		"padding:6px 10px;font-size:12px;}")
		.arg(Colors::POPUP_BG, Colors::TEXT_PRIMARY, Colors::POPUP_BORDER);
	return scale_qss(qss);
}

// Status chip / badge / tag is the custom-painted `Badge` widget (pill-button.hpp)
// — QSS QLabel chips don't baseline-centre their text. See makeBadge() / Badge.

// Underline tabs (decision #8)
inline QString tabStyle()
{
	const QString qss = QString(
		"QTabWidget::pane{border:none;}"
		"QTabBar{background:transparent;}"
		"QTabBar::tab{background:transparent;color:%1;padding:7px 14px;"
		"border:none;border-bottom:2px solid transparent;font-size:13px;}"
		"QTabBar::tab:selected{color:%2;border-bottom:2px solid %3;}"
		"QTabBar::tab:hover{color:%2;}")
		.arg(Colors::TEXT_SECONDARY, Colors::TEXT_PRIMARY, Colors::PRIMARY_COLOR);
	return scale_qss(qss);
}

inline QString listStyle()
{
	const QString qss = QString(
		"QListWidget{background:%1;color:%2;border:none;border-radius:8px;"
		"outline:none;font-size:13px;}"
		"QListWidget::item{padding:6px 10px;border-radius:8px;margin:2px 4px;}"
		"QListWidget::item:hover{background:%4;}"
		"QListWidget::item:selected{background:%3;color:#ffffff;}")
		.arg(Colors::BG_PRIMARY, Colors::TEXT_PRIMARY, Colors::SELECTION, Colors::HOVER_ROW);
	return scale_qss(qss);
}

// Tree view (QTreeWidget/QTreeView). Branch chevrons are custom-painted by the
// TreeWidget subclass (drawBranches), so default branch images are suppressed.
inline QString treeStyle()
{
	// The hover/selection highlight + text are painted by RowDelegate (content cell
	// only, no indent tint, no focus rect), so EVERY item background here is
	// transparent — otherwise the view's own row fill (which Qt extends into the
	// indent/expand column) would show under the delegate. show-decoration-selected:0
	// keeps the view's fill (if any) off the indent; branch transparent (we paint
	// the chevron + indent guides). No item margin → contiguous rows.
	const QString qss = QString(
		"QTreeWidget,QTreeView{background:%1;color:%2;border:none;outline:none;"
		"font-size:13px;show-decoration-selected:0;}"
		"QTreeWidget::item,QTreeView::item{padding:5px 8px;border:none;background:transparent;}"
		"QTreeWidget::item:hover,QTreeView::item:hover,"
		"QTreeWidget::item:selected,QTreeView::item:selected{background:transparent;}"
		"QTreeView::branch,QTreeView::branch:hover,QTreeView::branch:selected,"
		"QTreeView::branch:has-children:hover{background:transparent;border:none;}")
		.arg(Colors::BG_PRIMARY, Colors::TEXT_PRIMARY);
	return scale_qss(qss);
}

// Rounded card, radius 18, fill only (decisions #9/#10)
inline QString cardStyle(const char *fill = Colors::BG_PRIMARY, int radius = Sizes::RADIUS_CARD)
{
	const QString qss = QString(
		"QFrame#card{background:%1;border:none;border-radius:%2px;}")
		.arg(fill).arg(radius);
	return scale_qss(qss);
}

} // namespace UIStyles
} // namespace StreamUP
