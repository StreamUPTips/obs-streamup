#pragma once

#include "streamup-kit.hpp"

// Small widgets shared by the Overlay Controls window and its pages.

#include <streamup/ui/gallery-style.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/mac-inputs.hpp>
#include <streamup/ui/svg-icon.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPainter>
#include <QPainterPath>
#include <QSlider>
#include <QToolButton>
#include <QWheelEvent>

#include <algorithm>

#include <obs-module.h>

namespace StreamUP {
namespace OverlayUI {

namespace su = StreamUP::UIStyles;

inline QString T(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

// A slider inside a scrolling page must not grab the wheel as the pointer
// passes over it, or scrolling the page changes the overlay. It only takes the
// wheel once it has focus.
class PanelSlider : public QSlider {
public:
	// ownStyle false: the page carries the slider style once for all its
	// sliders. A stylesheet per slider costs ~70 ms each to polish under
	// OBS's app-wide theme, which made a page of sliders open slowly.
	explicit PanelSlider(bool ownStyle = true) : QSlider(Qt::Horizontal)
	{
		setFocusPolicy(Qt::StrongFocus);
		if (ownStyle)
			setStyleSheet(su::sliderStyle());
	}

protected:
	void wheelEvent(QWheelEvent *e) override
	{
		if (!hasFocus()) {
			e->ignore();
			return;
		}
		QSlider::wheelEvent(e);
	}
};

// The decimal twin of the kit's MacSpinBox: same field, same painted accent
// stepper. Ignores the wheel unless focused, for the same reason as above.
class PanelDoubleSpin : public QDoubleSpinBox {
public:
	explicit PanelDoubleSpin(bool onCard = true)
	{
		setFocusPolicy(Qt::StrongFocus);
		setMinimumHeight(su::S(28));
		setKeyboardTracking(false);
		const char *bg = onCard ? su::Colors::INPUT_ON_CARD : su::Colors::INPUT_ON_BASE;
		setStyleSheet(su::scale_qss(
			QString("QDoubleSpinBox{background:%1;color:%2;border:none;"
				"border-radius:8px;min-height:28px;max-height:28px;padding:0 28px 0 10px;font-size: 13px;}"
				"QDoubleSpinBox::up-button{subcontrol-origin:border;subcontrol-position:top right;"
				"width:22px;border:none;background:transparent;}"
				"QDoubleSpinBox::down-button{subcontrol-origin:border;subcontrol-position:bottom right;"
				"width:22px;border:none;background:transparent;}"
				"QDoubleSpinBox::up-arrow,QDoubleSpinBox::down-arrow{width:0;height:0;image:none;}")
				.arg(bg, su::Colors::TEXT_PRIMARY)));
	}
	QSize sizeHint() const override { return {QDoubleSpinBox::sizeHint().width(), su::S(28)}; }
	QSize minimumSizeHint() const override { return {QDoubleSpinBox::minimumSizeHint().width(), su::S(28)}; }

protected:
	void wheelEvent(QWheelEvent *e) override
	{
		if (!hasFocus()) {
			e->ignore();
			return;
		}
		QDoubleSpinBox::wheelEvent(e);
	}
	void paintEvent(QPaintEvent *e) override
	{
		QDoubleSpinBox::paintEvent(e);
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		const qreal rad = su::S(8);
		const qreal bw = su::S(22);
		QPainterPath fp;
		fp.addRoundedRect(r, rad, rad);
		const QRectF col(r.right() - bw, r.top(), bw + 1, r.height());
		p.save();
		p.setClipPath(fp);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(su::Colors::PRIMARY_COLOR));
		p.drawRect(col);
		QPen dv(QColor(255, 255, 255, 90));
		dv.setWidthF(std::max(1.6, su::S(2) * 0.9));
		p.setPen(dv);
		p.drawLine(QPointF(col.left(), r.center().y()), QPointF(col.right(), r.center().y()));
		p.restore();
		QPen cp{QColor(255, 255, 255)};
		cp.setWidthF(std::max(1.2, su::S(2) * 0.75));
		cp.setCapStyle(Qt::RoundCap);
		cp.setJoinStyle(Qt::RoundJoin);
		p.setPen(cp);
		const qreal cw = su::S(6);
		const qreal cx = col.center().x();
		su::detail::drawChevron(p, QPointF(cx, r.top() + r.height() * 0.25), cw, /*up=*/true);
		su::detail::drawChevron(p, QPointF(cx, r.top() + r.height() * 0.75), cw, /*up=*/false);
		if (hasFocus()) {
			QPen fpen{QColor(su::Colors::PRIMARY_COLOR)};
			fpen.setWidthF(2);
			p.setPen(fpen);
			p.setBrush(Qt::NoBrush);
			p.drawRoundedRect(r, rad, rad);
		}
	}
};

// An editable combo box. The kit's MacComboBox paints its own field text, and
// an editable combo's line edit draws the text again on top, so the two read
// as doubled. This one is styled entirely by QSS, to match the kit's inputs.
inline QComboBox *EditableCombo(bool onCard = true)
{
	auto *combo = new QComboBox();
	combo->setEditable(true);
	combo->setInsertPolicy(QComboBox::NoInsert);
	combo->setFixedHeight(su::S(28));
	const char *bg = onCard ? su::Colors::INPUT_ON_CARD : su::Colors::INPUT_ON_BASE;
	combo->setStyleSheet(
		su::scale_qss(QString("QComboBox{background:%1;color:%2;border:1px solid transparent;border-radius:8px;"
				      "padding:0 6px 0 10px;font-size: 13px;}"
				      "QComboBox:focus{border:1px solid rgba(0,118,223,0.6);}"
				      "QComboBox::drop-down{border:none;width:24px;}"
				      "QComboBox QLineEdit{background:transparent;border:none;color:%2;padding:0;}")
					  .arg(bg, su::Colors::TEXT_PRIMARY)) +
		su::comboStyle(onCard));
	return combo;
}

// A small icon button with a painted vector icon: row resets, list reorder,
// remove, and "more". One size, a hover and pressed state, a tooltip, and a
// disabled look that stays visible. "Filled" draws it as a round neutral pill
// so it sits beside a PillButton at the same height.
class GlyphButton : public QToolButton {
public:
	enum class Glyph { Reset, Up, Down, Remove, More };

	GlyphButton(Glyph glyph, const QString &tooltip, const QString &accessibleName, bool filled = false)
		: m_glyph(glyph), m_filled(filled)
	{
		setToolTip(tooltip);
		setAccessibleName(accessibleName);
		setCursor(Qt::PointingHandCursor);
		setAttribute(Qt::WA_Hover, true);
		setFocusPolicy(Qt::TabFocus);
		const int side = filled ? su::S(30) : su::S(28);
		setFixedSize(side, side);
		setAutoRaise(true);
		setStyleSheet("QToolButton{border:none;background:transparent;}");
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		const bool hover = underMouse() && isEnabled();
		const bool down = isDown();

		// Background: a filled pill always, a plain button only on hover.
		QColor bg = Qt::transparent;
		if (m_filled)
			bg = down ? QColor("#3a3a4e") : hover ? QColor("#34344a") : QColor("#2a2a3a");
		else if (down)
			bg = QColor(255, 255, 255, 36);
		else if (hover)
			bg = QColor(255, 255, 255, 20);
		if (bg.alpha() > 0) {
			p.setPen(Qt::NoPen);
			p.setBrush(bg);
			const qreal rad = m_filled ? r.height() / 2.0 : su::S(8);
			p.drawRoundedRect(r, rad, rad);
		}
		if (hasFocus()) {
			QPen fp{QColor(su::Colors::PRIMARY_COLOR)};
			fp.setWidthF(1.5);
			p.setPen(fp);
			p.setBrush(Qt::NoBrush);
			const qreal rad = m_filled ? r.height() / 2.0 : su::S(8);
			p.drawRoundedRect(r.adjusted(1, 1, -1, -1), rad, rad);
		}

		QColor ink(su::Colors::TEXT_SECONDARY);
		if (!isEnabled())
			ink = QColor(su::Colors::TEXT_MUTED);
		else if (hover)
			ink = m_glyph == Glyph::Remove ? QColor(su::Colors::COLOR_DANGER) : QColor(su::Colors::TEXT_PRIMARY);
		if (m_filled && isEnabled())
			ink = QColor(205, 214, 244);
		QPen pen(ink);
		pen.setWidthF(std::max(1.4, su::S(2) * 0.8));
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);

		const QPointF c = r.center();
		const qreal u = su::S(16) / 16.0; // icons are drawn on a 16 px grid
		switch (m_glyph) {
		case Glyph::Reset: {
			// Tabler "restore", the same set as every other icon in our windows.
			const QPixmap glyph = su::svgIcon(QStringLiteral("restore"), ink, 16, devicePixelRatioF());
			const QSizeF gs = QSizeF(glyph.size()) / glyph.devicePixelRatio();
			p.drawPixmap(QPointF(c.x() - gs.width() / 2, c.y() - gs.height() / 2), glyph);
			break;
		}
		case Glyph::Up:
		case Glyph::Down: {
			const qreal dir = m_glyph == Glyph::Up ? -1.0 : 1.0;
			QPainterPath path;
			path.moveTo(c + QPointF(-4 * u, -2 * u * dir));
			path.lineTo(c + QPointF(0, 2 * u * dir));
			path.lineTo(c + QPointF(4 * u, -2 * u * dir));
			p.drawPath(path);
			break;
		}
		case Glyph::Remove:
			p.drawLine(c + QPointF(-3.8 * u, -3.8 * u), c + QPointF(3.8 * u, 3.8 * u));
			p.drawLine(c + QPointF(3.8 * u, -3.8 * u), c + QPointF(-3.8 * u, 3.8 * u));
			break;
		case Glyph::More:
			p.setPen(Qt::NoPen);
			p.setBrush(ink);
			for (int i = -1; i <= 1; ++i)
				p.drawEllipse(c + QPointF(i * 5.0 * u, 0), 1.6 * u, 1.6 * u);
			break;
		}
	}

	void enterEvent(QEnterEvent *e) override
	{
		update();
		QToolButton::enterEvent(e);
	}
	void leaveEvent(QEvent *e) override
	{
		update();
		QToolButton::leaveEvent(e);
	}

private:
	Glyph m_glyph;
	bool m_filled;
};

inline QLabel *RowLabel(const QString &text, const QString &tooltip = QString())
{
	auto *l = su::makeLabel(text, su::Sizes::FONT_SIZE_SMALL, 600, su::Colors::TEXT_PRIMARY);
	l->setWordWrap(false);
	l->setToolTip(tooltip);
	return l;
}

inline QLabel *DimLabel(const QString &text)
{
	auto *l = su::makeLabel(text, su::Sizes::FONT_SIZE_TINY, 500, su::Colors::TEXT_MUTED);
	return l;
}

// The rounded list used as a navigation rail, matching StreamUP Settings.
inline QString NavListStyle()
{
	return StreamUP::Kit::NavListQss();
}

} // namespace OverlayUI
} // namespace StreamUP
