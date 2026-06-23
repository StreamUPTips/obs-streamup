#pragma once

// macOS-feel combo box + spin box (custom-painted chevrons).
//
// Pure-QSS can't draw crisp chevrons without bundling image assets, so — as we
// already do for the switch and checkbox — we hand-paint these. The popup list
// is still styled (opaque, via the combo's stylesheet + makeComboAnimated for
// the open animation); only the field chrome is custom.

#include <QComboBox>
#include <QSpinBox>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QStyleOptionSpinBox>
#include <QPaintEvent>

#include "gallery-style.hpp"

namespace StreamUP {
namespace UIStyles {

namespace detail {
inline void drawChevron(QPainter &p, const QPointF &c, qreal w, bool up)
{
	const qreal h = w * 0.55;
	QPainterPath path;
	if (up) {
		path.moveTo(c.x() - w / 2, c.y() + h / 2);
		path.lineTo(c.x(), c.y() - h / 2);
		path.lineTo(c.x() + w / 2, c.y() + h / 2);
	} else {
		path.moveTo(c.x() - w / 2, c.y() - h / 2);
		path.lineTo(c.x(), c.y() + h / 2);
		path.lineTo(c.x() + w / 2, c.y() - h / 2);
	}
	p.drawPath(path);
}
} // namespace detail

// macOS NSPopUpButton style: subtle field + accent chevron box on the right.
class MacComboBox : public QComboBox {
public:
	explicit MacComboBox(QWidget *parent = nullptr) : QComboBox(parent)
	{
		setMinimumHeight(S(28));
		setAttribute(Qt::WA_Hover, true);
		QObject::connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
				 [this](int) { update(); });
	}
	void setOnCard(bool onCard) { m_onCard = onCard; update(); }

	// Report a compact height so the LAYOUT reserves the right row size (without a
	// field-level QSS, QComboBox's native minimumSizeHint is tall and leaves a gap
	// below the combo). setFixedHeight controls the widget; this controls the row.
	QSize sizeHint() const override { return {QComboBox::sizeHint().width(), S(28)}; }
	QSize minimumSizeHint() const override
	{
		return {QComboBox::minimumSizeHint().width(), S(28)};
	}

protected:
	void enterEvent(QEnterEvent *e) override { m_hover = true; update(); QComboBox::enterEvent(e); }
	void leaveEvent(QEvent *e) override { m_hover = false; update(); QComboBox::leaveEvent(e); }

	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);

		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		const qreal rad = S(8);
		const qreal bw = S(22); // accent drop-down block width (matches theme)
		QPainterPath fp;
		fp.addRoundedRect(r, rad, rad);

		// Field fill
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(m_onCard ? Colors::INPUT_ON_CARD : Colors::INPUT_ON_BASE));
		p.drawPath(fp);

		// Accent block on the right, clipped to the field's rounded corners
		// (so its right corners follow the field radius) — matches the theme's
		// QComboBox::drop-down { background: primary_color }.
		p.save();
		p.setClipPath(fp);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(m_hover ? Colors::PRIMARY_HOVER : Colors::PRIMARY_COLOR));
		p.drawRect(QRectF(r.right() - bw, r.top(), bw + 1, r.height()));
		p.restore();

		// White up/down chevrons centred in the accent block
		QPen cp{QColor(255, 255, 255)};
		cp.setWidthF(std::max(1.2, S(2) * 0.75));
		cp.setCapStyle(Qt::RoundCap);
		cp.setJoinStyle(Qt::RoundJoin);
		p.setPen(cp);
		p.setBrush(Qt::NoBrush);
		const qreal cw = S(7);
		const QPointF cc(r.right() - bw / 2.0, r.center().y());
		detail::drawChevron(p, cc + QPointF(0, -S(5)), cw, /*up=*/true);
		detail::drawChevron(p, cc + QPointF(0, S(5)), cw, /*up=*/false);

		// Flat by default (no border); only a 2px accent ring on focus.
		if (hasFocus()) {
			QPen fpen{QColor(Colors::PRIMARY_COLOR)};
			fpen.setWidthF(2);
			p.setPen(fpen);
			p.setBrush(Qt::NoBrush);
			p.drawRoundedRect(r, rad, rad);
		}

		// Current text (elided)
		p.setPen(QColor(Colors::TEXT_PRIMARY));
		p.setFont(font());
		QRectF tr(S(12), 0, width() - bw - S(18), height());
		const QFontMetrics fm(font());
		p.drawText(tr, Qt::AlignLeft | Qt::AlignVCenter,
			   fm.elidedText(currentText(), Qt::ElideRight, (int)tr.width()));
	}

private:
	bool m_onCard = false;
	bool m_hover = false;
};

// macOS-feel spin box: subtle field + stacked chevron steppers (custom-painted).
class MacSpinBox : public QSpinBox {
public:
	explicit MacSpinBox(QWidget *parent = nullptr) : QSpinBox(parent)
	{
		applyStyle(false);
		setMinimumHeight(S(28));
	}
	void setOnCard(bool onCard) { applyStyle(onCard); }

	// Report a compact height so the LAYOUT reserves the right row size (matches
	// MacComboBox). Without this the native spin minimumSizeHint is tall and
	// leaves a gap below the field in form layouts.
	QSize sizeHint() const override { return {QSpinBox::sizeHint().width(), S(28)}; }
	QSize minimumSizeHint() const override
	{
		return {QSpinBox::minimumSizeHint().width(), S(28)};
	}

protected:
	void paintEvent(QPaintEvent *e) override
	{
		QSpinBox::paintEvent(e); // field bg + (transparent) buttons + text via QSS

		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);

		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		const qreal rad = S(8);
		const qreal bw = S(22);
		QPainterPath fp;
		fp.addRoundedRect(r, rad, rad);

		// Accent column clipped to the field (so it never extends outside),
		// split by a centre divider into up (top) / down (bottom).
		const QRectF col(r.right() - bw, r.top(), bw + 1, r.height());
		p.save();
		p.setClipPath(fp);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(Colors::PRIMARY_COLOR));
		p.drawRect(col);
		QPen dv(QColor(255, 255, 255, 90));
		dv.setWidthF(std::max(1.6, S(2) * 0.9));
		p.setPen(dv);
		p.drawLine(QPointF(col.left(), r.center().y()), QPointF(col.right(), r.center().y()));
		p.restore();

		// White chevrons centred in each half.
		QPen cp{QColor(255, 255, 255)};
		cp.setWidthF(std::max(1.2, S(2) * 0.75));
		cp.setCapStyle(Qt::RoundCap);
		cp.setJoinStyle(Qt::RoundJoin);
		p.setPen(cp);
		const qreal cw = S(6);
		const qreal cx = col.center().x();
		detail::drawChevron(p, QPointF(cx, r.top() + r.height() * 0.25), cw, /*up=*/true);
		detail::drawChevron(p, QPointF(cx, r.top() + r.height() * 0.75), cw, /*up=*/false);

		// Focus ring only (flat otherwise).
		if (hasFocus()) {
			QPen fpen{QColor(Colors::PRIMARY_COLOR)};
			fpen.setWidthF(2);
			p.setPen(fpen);
			p.setBrush(Qt::NoBrush);
			p.drawRoundedRect(r, rad, rad);
		}
	}

private:
	void applyStyle(bool onCard)
	{
		// Field only; the accent stepper column is custom-painted in paintEvent.
		// Buttons are transparent (kept for hit-testing) so nothing extends past
		// the field, and arrows are suppressed.
		const char *bg = onCard ? Colors::INPUT_ON_CARD : Colors::INPUT_ON_BASE;
		// No QSS border at all — the focus ring is painted in paintEvent (a QSS
		// :focus border here would double up with the painted ring).
		const QString qss = QString(
			"QSpinBox{background:%1;color:%2;border:none;"
			"border-radius:8px;min-height:28px;max-height:28px;padding:0 28px 0 10px;font-size:13px;}"
			"QSpinBox::up-button{subcontrol-origin:border;subcontrol-position:top right;"
			"width:22px;border:none;background:transparent;}"
			"QSpinBox::down-button{subcontrol-origin:border;subcontrol-position:bottom right;"
			"width:22px;border:none;background:transparent;}"
			"QSpinBox::up-arrow,QSpinBox::down-arrow{width:0;height:0;image:none;}")
			.arg(bg, Colors::TEXT_PRIMARY);
		setStyleSheet(scale_qss(qss));
	}
};

} // namespace UIStyles
} // namespace StreamUP
