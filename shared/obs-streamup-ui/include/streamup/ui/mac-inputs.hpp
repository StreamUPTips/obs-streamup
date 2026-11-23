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

// A quiet field with a hairline and one muted chevron, like Linear or Raycast.
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
		const qreal bw = S(26); // chevron column
		QPainterPath fp;
		fp.addRoundedRect(r, rad, rad);

		// Quiet field: fill, a hairline that brightens on hover, one muted
		// down chevron. No accent block; the accent is kept for focus.
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(m_onCard ? Colors::INPUT_ON_CARD : Colors::INPUT_ON_BASE));
		p.drawPath(fp);
		if (m_hover && isEnabled())
			p.fillPath(fp, QColor(255, 255, 255, 10));

		QPen hp(hasFocus() ? QColor(Colors::PRIMARY_COLOR)
				   : QColor(255, 255, 255, m_hover && isEnabled() ? 40 : 22));
		hp.setWidthF(hasFocus() ? 1.5 : 1.0);
		p.setPen(hp);
		p.setBrush(Qt::NoBrush);
		p.drawRoundedRect(r, rad, rad);

		QPen cp{QColor(m_hover && isEnabled() ? Colors::TEXT_PRIMARY : Colors::TEXT_SECONDARY)};
		cp.setWidthF(std::max(1.2, S(2) * 0.7));
		cp.setCapStyle(Qt::RoundCap);
		cp.setJoinStyle(Qt::RoundJoin);
		p.setPen(cp);
		detail::drawChevron(p, QPointF(r.right() - bw / 2.0, r.center().y() + S(1)), S(8), /*up=*/false);

		// Current text (elided)
		p.setPen(QColor(isEnabled() ? Colors::TEXT_PRIMARY : Colors::TEXT_MUTED));
		p.setFont(font());
		QRectF tr(S(12), 0, width() - bw - S(14), height());
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

		// Two small muted steppers on the right and a hairline round the
		// field; the accent only shows as the focus ring.
		const QRectF col(r.right() - bw, r.top(), bw, r.height());
		QPen hp(hasFocus() ? QColor(Colors::PRIMARY_COLOR) : QColor(255, 255, 255, 22));
		hp.setWidthF(hasFocus() ? 1.5 : 1.0);
		p.setPen(hp);
		p.setBrush(Qt::NoBrush);
		p.drawRoundedRect(r, rad, rad);

		QPen cp{QColor(Colors::TEXT_SECONDARY)};
		cp.setWidthF(std::max(1.2, S(2) * 0.7));
		cp.setCapStyle(Qt::RoundCap);
		cp.setJoinStyle(Qt::RoundJoin);
		p.setPen(cp);
		const qreal cw = S(6);
		const qreal cx = col.center().x();
		detail::drawChevron(p, QPointF(cx, r.center().y() - S(4)), cw, /*up=*/true);
		detail::drawChevron(p, QPointF(cx, r.center().y() + S(4)), cw, /*up=*/false);
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
