#pragma once

// StreamUP custom-window chrome (frameless, rounded card, soft elevation shadow,
// custom draggable titlebar). This is the look for our CUSTOM windows only —
// docks and normal OBS surfaces keep native OBS theming. Faithful, compact
// re-implementation of obs-streamup/ui/ui-styles (the shared lib will host the
// real one). Header-only: only virtuals are overridden, so no extra moc.

#include <QDialog>
#include <QFrame>
#include <QObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWindow>
#include <QDesktopServices>
#include <QUrl>
#include <QToolTip>
#include <functional>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <QAbstractItemView>
#include <QTableView>

#include "gallery-style.hpp"
#include "ui-scrollbar.hpp"

namespace StreamUP {
namespace UIStyles {

// Rounded fill-only card. Masked so child widgets with different backgrounds
// (header/content) don't bleed past the rounded corners.
class RoundedContainer : public QFrame {
public:
	// `fill` defaults to BG_DARKEST so existing RoundedContainer(radius, parent)
	// calls are unchanged. Pass a colour (or call setFillColor) for a coloured
	// card — e.g. a green toast — without subclassing.
	explicit RoundedContainer(int radius, QWidget *parent = nullptr,
				  const QColor &fill = QColor(Colors::BG_DARKEST))
		: QFrame(parent), m_radius(radius), m_fill(fill)
	{
		setAttribute(Qt::WA_StyledBackground, true);
	}

	void setFillColor(const QColor &fill)
	{
		m_fill = fill;
		update();
	}
	QColor fillColor() const { return m_fill; }

protected:
	// m_radius is a DESIGN value; scale it by the OS text size so the painted
	// card corners match the header/footer/badge QSS radii (which scale_qss
	// already scales). Painted radius and QSS radius MUST end up equal.
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		p.setPen(Qt::NoPen);
		p.setBrush(m_fill);
		const int r = S(m_radius);
		p.drawRoundedRect(rect(), r, r);
	}
	void resizeEvent(QResizeEvent *e) override
	{
		const int r = S(m_radius);
		QPainterPath path;
		path.addRoundedRect(QRectF(rect()), r, r);
		setMask(QRegion(path.toFillPolygon().toPolygon()));
		QFrame::resizeEvent(e);
	}

private:
	int m_radius;
	QColor m_fill;
};

// Ready table card: applies tableStyle() to the table, installs the custom
// scrollbars, and wraps the table in a RoundedContainer(radius) so the corners
// clip — returns the container to add to a layout. Mirrors the manual
// "style + useScrollBars + roundClip" pattern the gallery used everywhere.
// Templated on the view type so it works for QTableWidget and QTableView alike.
template <typename TableT>
inline RoundedContainer *makeTableCard(TableT *table, int radius = Sizes::RADIUS_CARD)
{
	table->setStyleSheet(tableStyle());
	useScrollBars(table);
	auto *card = new RoundedContainer(radius);
	auto *l = new QVBoxLayout(card);
	l->setContentsMargins(0, 0, 0, 0);
	l->setSpacing(0);
	l->addWidget(table);
	return card;
}

// Frameless host that paints a soft ambient shadow into a transparent margin so
// stacked dark windows read as elevated rather than merging.
class ShadowDialog : public QDialog {
public:
	static constexpr int kShadowMargin = 20;
	explicit ShadowDialog(QWidget *parent = nullptr) : QDialog(parent)
	{
		// Qt::Window (not Qt::Dialog) so each custom window is a real top-level
		// window OBS can show in the taskbar (see makeWindow's WS_EX_APPWINDOW).
		setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
		setAttribute(Qt::WA_TranslucentBackground, true);
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		// Scale the shadow margin with the OS text size so the shadow grows with
		// the card (the outer layout margins use the same S(kShadowMargin) — see
		// applyChrome — so paint and layout agree). radius = S(18) matches the
		// RoundedContainer card body (which paints with S(18)).
		const int sm = S(kShadowMargin);
		const QRectF card = QRectF(rect()).adjusted(sm, sm, -sm, -sm);
		const int radius = S(18);
		for (int i = sm; i >= 1; --i) {
			QColor c(0, 0, 0);
			c.setAlphaF(0.022 * (1.0 - (qreal)i / sm));
			p.setPen(Qt::NoPen);
			p.setBrush(c);
			p.drawRoundedRect(card.adjusted(-i, -i + 2, i, i + 2), radius + i, radius + i);
		}
	}
};

#ifdef Q_OS_WIN
// Re-asserts WS_EX_APPWINDOW on the FIRST show. Qt can drop the ex-style set off
// winId() before show() when it (re)creates the native window, so the taskbar
// button/thumbnail would be lost. This re-applies once after the window is
// native + shown, then removes itself. Only installed for brand-footer windows.
class AppWindowStyleFilter : public QObject {
public:
	using QObject::QObject;

protected:
	bool eventFilter(QObject *obj, QEvent *e) override
	{
		if (e->type() == QEvent::Show) {
			if (auto *w = qobject_cast<QWidget *>(obj)) {
				if (const HWND hwnd = reinterpret_cast<HWND>(w->winId())) {
					LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
					ex |= WS_EX_APPWINDOW;
					ex &= ~static_cast<LONG_PTR>(WS_EX_TOOLWINDOW);
					SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
				}
				w->removeEventFilter(this);
				deleteLater();
			}
		}
		return QObject::eventFilter(obj, e);
	}
};
#endif

// Drag the window from a header widget via the compositor.
class DragFilter : public QObject {
public:
	using QObject::QObject;

protected:
	bool eventFilter(QObject *obj, QEvent *e) override
	{
		if (e->type() == QEvent::MouseButtonPress) {
			auto *me = static_cast<QMouseEvent *>(e);
			if (me->button() == Qt::LeftButton) {
				if (auto *w = qobject_cast<QWidget *>(obj)) {
					if (QWindow *win = w->window()->windowHandle()) {
						win->startSystemMove();
						return true;
					}
				}
			}
		}
		return QObject::eventFilter(obj, e);
	}
};

// Custom-painted close button — paints its own chip + X so the glyph is always
// visible regardless of the active font (the unicode × often isn't present).
class CloseButton : public QAbstractButton {
public:
	explicit CloseButton(QWidget *parent = nullptr) : QAbstractButton(parent)
	{
		setCursor(Qt::PointingHandCursor);
		setAttribute(Qt::WA_Hover, true);
		setFixedSize(S(30), S(30));
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

		QColor bg(255, 255, 255, 16);
		if (underMouse())
			bg = isDown() ? QColor("#d93a30") : QColor(255, 69, 58);
		p.setPen(Qt::NoPen);
		p.setBrush(bg);
		p.drawRoundedRect(r, S(8), S(8));

		QColor x = underMouse() ? QColor("#ffffff") : QColor(Colors::TEXT_PRIMARY);
		QPen pen(x, std::max(1.4, S(2) * 0.8));
		pen.setCapStyle(Qt::RoundCap);
		p.setPen(pen);
		const QPointF c = r.center();
		const qreal d = S(5);
		p.drawLine(c + QPointF(-d, -d), c + QPointF(d, d));
		p.drawLine(c + QPointF(-d, d), c + QPointF(d, -d));
	}
	QSize sizeHint() const override { return QSize(S(30), S(30)); }
};

// Header feedback button — same S(30) chip + hover behaviour as CloseButton, but
// paints a speech/chat bubble glyph (rounded-rect bubble + small tail + three
// dots) instead of an X, and hovers to the accent blue rather than red. Sits to
// the LEFT of the CloseButton on main plugin windows (brand-footer windows).
class FeedbackButton : public QAbstractButton {
public:
	explicit FeedbackButton(QWidget *parent = nullptr) : QAbstractButton(parent)
	{
		setCursor(Qt::PointingHandCursor);
		setAttribute(Qt::WA_Hover, true);
		setFixedSize(S(30), S(30));
		setToolTip(QStringLiteral("Send feedback"));
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

		QColor bg(255, 255, 255, 16);
		if (underMouse())
			bg = isDown() ? QColor(Colors::PRIMARY_PRESSED) : QColor(Colors::PRIMARY_COLOR);
		p.setPen(Qt::NoPen);
		p.setBrush(bg);
		p.drawRoundedRect(r, S(8), S(8));

		QColor ink = underMouse() ? QColor("#ffffff") : QColor(Colors::TEXT_PRIMARY);
		QPen pen(ink, std::max(1.4, S(2) * 0.8));
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);

		const QPointF c = r.center();
		const qreal d = S(5);
		// Speech bubble (rounded rect) + tail + three dots — matches IconButton's
		// Feedback glyph so the header button and any in-content feedback icon read
		// identically.
		const qreal bw = d * 1.5;
		const qreal bh = d * 1.1;
		const qreal by = c.y() - d * 0.25;
		const QRectF bubble(c.x() - bw, by - bh, bw * 2, bh * 2);
		QPainterPath path;
		path.addRoundedRect(bubble, S(3), S(3));
		QPolygonF tail;
		tail << QPointF(c.x() - bw * 0.35, by + bh - 0.5)
		     << QPointF(c.x() - bw * 0.65, by + bh + d * 0.7)
		     << QPointF(c.x() - bw * 0.02, by + bh - 0.5);
		path.addPolygon(tail);
		p.drawPath(path.simplified());
		p.setPen(Qt::NoPen);
		p.setBrush(ink);
		const qreal dot = std::max(1.0, S(1) * 0.95);
		for (int i = -1; i <= 1; ++i)
			p.drawEllipse(QPointF(c.x() + i * d * 0.62, by), dot, dot);
	}
	QSize sizeHint() const override { return QSize(S(30), S(30)); }
};

struct WindowShell {
	ShadowDialog *dialog;
	QVBoxLayout *content;       // add your widgets here
	QVBoxLayout *footer;        // optional extra footer rows (above the brand/button row)
	QHBoxLayout *footerButtons; // add action buttons here — right-anchored, inline with the
				    // brand line (natural size, never full-width). No stretch needed.
};

// Decorate an EXISTING ShadowDialog (or subclass) with StreamUP chrome: shadow
// margin → rounded card → header (hairline) → content → footer (hairline +
// brand/version line). Use this when your dialog SUBCLASSES ShadowDialog and
// owns its widgets as members (the common plugin pattern — settings dialogs,
// pickers); use makeWindow() when you just want a fresh window. Returns the
// content + footer layouts to fill. Matches our other custom UIs.
// brandName: the PLUGIN name shown in the footer brand line (e.g. "StreamUP
// Hotkey Display") — distinct from `title`, which is the window/header caption
// (e.g. "Settings"). Defaults to `title` when empty. A leading "StreamUP" in the
// brand name links to streamup.tips; "Andi Stone" links to andistone.uk.
// onFeedback: optional handler for the header feedback button. When clicked it
// calls onFeedback if set; otherwise opens https://streamup.tips. Plugins with
// their own feedback dialog pass a handler that opens it.
// popup: true for transient SoT dialogs (confirm/prompt/info/confirmDiscard) —
// they get NO taskbar button and NO feedback button. EVERY other custom window
// (popup=false, the default) gets BOTH a Windows taskbar button/thumbnail and a
// header feedback button, regardless of brandFooter (which now only controls the
// footer brand line). So all real plugin windows are hoverable in the taskbar
// and carry feedback; only throwaway popups are excluded.
inline WindowShell applyChrome(ShadowDialog *dlg, const QString &title,
			       const QString &version = QString(), bool brandFooter = false,
			       const QString &brandName = QString(),
			       std::function<void()> onFeedback = {}, bool popup = false)
{
	dlg->setWindowTitle(title);
	dlg->setFont(brandFont());

#ifdef Q_OS_WIN
	// Every non-popup window gets its own taskbar button + hover thumbnail in the
	// OBS group: an owned frameless Qt::Window is otherwise a secondary window with
	// no taskbar presence. APPWINDOW forces it; only transient popups (confirm/
	// prompt/info, popup=true) stay buttonless.
	if (!popup) {
		const HWND hwnd = reinterpret_cast<HWND>(dlg->winId());
		LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
		ex |= WS_EX_APPWINDOW;
		ex &= ~static_cast<LONG_PTR>(WS_EX_TOOLWINDOW);
		SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
		// Re-assert after show() — Qt may recreate the native window and drop
		// the ex-style set above.
		dlg->installEventFilter(new AppWindowStyleFilter(dlg));
	}
#endif

	auto *outer = new QVBoxLayout(dlg);
	// Scale the shadow margin with the OS text size so the layout reserves the
	// same space the (scaled) shadow paints into (ShadowDialog::paintEvent uses
	// S(kShadowMargin) too).
	const int m = S(ShadowDialog::kShadowMargin);
	outer->setContentsMargins(m, m, m, m);
	outer->setSpacing(0);

	auto *card = new RoundedContainer(18, dlg);
	outer->addWidget(card);

	auto *cardLay = new QVBoxLayout(card);
	cardLay->setContentsMargins(0, 0, 0, 0);
	cardLay->setSpacing(0);

	// ── Header (scoped QSS so the hairline doesn't bleed onto children) ──
	auto *header = new QWidget(card);
	header->setObjectName("suHeader");
	header->setFixedHeight(S(48));
	header->setStyleSheet(scale_qss(
		QString("QWidget#suHeader{background:%1;border-top-left-radius:18px;"
			"border-top-right-radius:18px;border-bottom:1px solid %2;}")
			.arg(Colors::SIDEBAR_BG, Colors::SEPARATOR)));
	auto *hl = new QHBoxLayout(header);
	hl->setContentsMargins(S(18), 0, S(10), 0);
	hl->setSpacing(S(8));

	// Header caption reads "StreamUP <Plugin> • <Window> (v0.0.0)": the plugin
	// (brandName) and window name (title) joined by a bullet, with the version
	// inline in parens. When there's no distinct brandName (confirm/prompt and
	// other makeWindow popups) it degrades to just the window title.
	//
	// Title de-dup: plugins sometimes pass a `title` that already embeds the
	// plugin name (e.g. brandName "StreamUP Chat", title "StreamUP Chat
	// Designer"), which would compose the ugly "StreamUP Chat • StreamUP Chat
	// Designer". When `title` starts with `brandName` (case-insensitive) we strip
	// that prefix — plus any leading separator/space — so the window name reads
	// just "Designer", giving "StreamUP Chat • Designer". An empty stripped name
	// (title == brandName, modulo case) degrades to just the brand name with no
	// dangling bullet.
	QString windowName = title;
	if (!brandName.isEmpty() &&
	    windowName.startsWith(brandName, Qt::CaseInsensitive)) {
		windowName = windowName.mid(brandName.size());
		// Drop a leading separator/space run ("• ", " - ", whitespace, etc.).
		while (!windowName.isEmpty() &&
		       (windowName.at(0).isSpace() || windowName.at(0) == QChar(0x2022) ||
			windowName.at(0) == QLatin1Char('-') || windowName.at(0) == QLatin1Char(':')))
			windowName.remove(0, 1);
		windowName = windowName.trimmed();
	}

	QString headerText = windowName;
	if (!brandName.isEmpty()) {
		// brandName present: show "brand • window", or just "brand" when the
		// window name collapsed to empty (title was the plugin name itself).
		headerText = windowName.isEmpty()
				     ? brandName
				     : brandName + QStringLiteral(" \xE2\x80\xA2 ") + windowName;
	}
	if (!version.isEmpty())
		headerText += QStringLiteral(" (") + version + QStringLiteral(")");

	auto *titleLbl = new QLabel(headerText, header);
	titleLbl->setFont(displayFont(22, 700));
	titleLbl->setStyleSheet(
		QString("color:%1;background:transparent;").arg(Colors::TEXT_PRIMARY));
	hl->addWidget(titleLbl);
	hl->addStretch();

	// Feedback button — on every non-popup window, to the LEFT of the close
	// button. Clicking calls onFeedback if supplied, else opens streamup.tips.
	if (!popup) {
		auto *feedback = new FeedbackButton(header);
		QObject::connect(feedback, &QAbstractButton::clicked, dlg, [onFeedback]() {
			if (onFeedback)
				onFeedback();
			else
				QDesktopServices::openUrl(QUrl(QStringLiteral("https://streamup.tips")));
		});
		hl->addWidget(feedback, 0, Qt::AlignVCenter);
	}

	auto *close = new CloseButton(header);
	QObject::connect(close, &QAbstractButton::clicked, dlg, &QDialog::close);
	hl->addWidget(close, 0, Qt::AlignVCenter);

	auto *drag = new DragFilter(dlg);
	header->installEventFilter(drag);
	titleLbl->installEventFilter(drag);
	cardLay->addWidget(header);

	// ── Content ──
	auto *contentWidget = new QWidget(card);
	contentWidget->setObjectName("suContent");
	contentWidget->setStyleSheet(
		QString("QWidget#suContent{background:%1;}").arg(Colors::BG_DARKEST));
	auto *content = new QVBoxLayout(contentWidget);
	content->setContentsMargins(0, 0, 0, 0);
	content->setSpacing(0);
	cardLay->addWidget(contentWidget, 1);

	// ── Footer (hairline + extra rows + brand line) ──
	auto *footerWidget = new QWidget(card);
	footerWidget->setObjectName("suFooter");
	footerWidget->setStyleSheet(scale_qss(
		QString("QWidget#suFooter{background:%1;border-top:1px solid %2;"
			"border-bottom-left-radius:18px;border-bottom-right-radius:18px;}")
			.arg(Colors::SIDEBAR_BG, Colors::SEPARATOR)));
	auto *footer = new QVBoxLayout(footerWidget);
	footer->setContentsMargins(S(16), S(10), S(16), S(10));
	footer->setSpacing(S(8));

	// One row: brand line on the LEFT (wraps when the window is narrow), action
	// buttons anchored to the RIGHT at their natural size. Secondary windows
	// (confirms/pickers, brandFooter=false) drop the brand line but keep the
	// right-anchored button slot.
	auto *brandRow = new QHBoxLayout();
	brandRow->setContentsMargins(0, 0, 0, 0);
	brandRow->setSpacing(S(12));

	if (brandFooter) {
		const QString name = brandName.isEmpty() ? title : brandName;
		// Link a leading "StreamUP" to streamup.tips; the rest of the plugin
		// name stays plain text.
		QString nameHtml = name.toHtmlEscaped();
		if (name.startsWith(QStringLiteral("StreamUP")))
			nameHtml = QString("<a href='https://streamup.tips' style='color:%1;"
					   "text-decoration:none;'>StreamUP</a>%2")
					   .arg(Colors::TAG_COLOR, name.mid(8).toHtmlEscaped());
		const QString verSeg =
			version.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(version.toHtmlEscaped());

		auto *brand = new QLabel(footerWidget);
		brand->setTextFormat(Qt::RichText);
		brand->setOpenExternalLinks(true);
		brand->setWordWrap(true);
		brand->setText(
			QString("%1 by <a href='https://andistone.uk' style='color:%2;"
				"text-decoration:none;'>Andi Stone</a>%3 &bull; "
				"<a href='https://paypal.me/andilippi' style='color:%2;"
				"text-decoration:none;'>Send us a beer!</a>")
				.arg(nameHtml, Colors::TAG_COLOR, verSeg));
		brand->setStyleSheet(scale_qss(
			QString("color:%1;font-size:12px;background:transparent;").arg(Colors::TEXT_SECONDARY)));
		brandRow->addWidget(brand, 1); // stretch=1 → takes the slack, wraps; pushes buttons right
	} else {
		brandRow->addStretch();
	}

	auto *footerButtons = new QHBoxLayout();
	footerButtons->setContentsMargins(0, 0, 0, 0);
	footerButtons->setSpacing(S(10));
	brandRow->addLayout(footerButtons, 0);
	footer->addLayout(brandRow);

	cardLay->addWidget(footerWidget);

	return {dlg, content, footer, footerButtons};
}

// Build a fresh StreamUP custom window (creates + owns the dialog, deletes on
// close). Thin wrapper over applyChrome().
inline WindowShell makeWindow(const QString &title, const QString &version,
			      QWidget *parent = nullptr, bool brandFooter = true,
			      const QString &brandName = QString(),
			      std::function<void()> onFeedback = {}, bool popup = false)
{
	auto *dlg = new ShadowDialog(parent);
	dlg->setAttribute(Qt::WA_DeleteOnClose, true);
	return applyChrome(dlg, title, version, brandFooter, brandName, onFeedback, popup);
}

} // namespace UIStyles
} // namespace StreamUP
