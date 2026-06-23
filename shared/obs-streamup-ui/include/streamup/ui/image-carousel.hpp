#pragma once

// ImageCarousel — prev/next image browser with a sliding transition, circular
// nav arrows, and accent dot indicators. Header-only, custom-painted. The demo
// paints gradient placeholder "slides"; a real consumer feeds it QPixmaps.

#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QLinearGradient>
#include <QLineF>
#include <QVariantAnimation>
#include <QEasingCurve>

#include "gallery-style.hpp"
#include "mac-inputs.hpp" // detail::drawChevron

namespace StreamUP {
namespace UIStyles {

class ImageCarousel : public QWidget {
public:
	explicit ImageCarousel(int count = 4, QWidget *parent = nullptr) : QWidget(parent), m_count(count)
	{
		setMinimumHeight(S(170));
		setCursor(Qt::PointingHandCursor);
		m_anim = new QVariantAnimation(this);
		m_anim->setDuration(280);
		m_anim->setEasingCurve(QEasingCurve::OutCubic);
		m_anim->setStartValue(0.0);
		m_anim->setEndValue(1.0);
		QObject::connect(m_anim, &QVariantAnimation::valueChanged, this,
				 [this](const QVariant &v) { m_t = v.toReal(); update(); });
		QObject::connect(m_anim, &QVariantAnimation::finished, this, [this]() {
			m_index = m_incoming;
			m_animating = false;
			update();
		});
	}

protected:
	QRectF imageRect() const { return QRectF(rect()).adjusted(0, 0, 0, -S(18)); }
	qreal arrowR() const { return S(13); }
	QPointF leftArrowC() const { return {imageRect().left() + S(20), imageRect().center().y()}; }
	QPointF rightArrowC() const { return {imageRect().right() - S(20), imageRect().center().y()}; }

	void go(int dir)
	{
		if (m_animating)
			return;
		m_dir = dir;
		m_incoming = (m_index + dir + m_count) % m_count;
		m_animating = true;
		m_anim->stop();
		m_anim->start();
	}

	void mousePressEvent(QMouseEvent *e) override
	{
		const QPointF pos = e->position();
		if (QLineF(pos, leftArrowC()).length() <= arrowR() + S(3)) {
			go(-1);
			return;
		}
		if (QLineF(pos, rightArrowC()).length() <= arrowR() + S(3)) {
			go(+1);
			return;
		}
		// Dots.
		const qreal dotY = imageRect().bottom() + S(11);
		const qreal gap = S(14);
		const qreal startX = width() / 2.0 - (m_count - 1) * gap / 2.0;
		for (int i = 0; i < m_count; ++i) {
			if (QLineF(pos, QPointF(startX + i * gap, dotY)).length() <= S(7) && i != m_index)
				go(i > m_index ? 1 : -1);
		}
	}

	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF ir = imageRect();

		p.save();
		QPainterPath clip;
		clip.addRoundedRect(ir, S(12), S(12));
		p.setClipPath(clip);
		if (m_animating) {
			drawSlide(p, m_index, ir.translated(-m_dir * m_t * ir.width(), 0));
			drawSlide(p, m_incoming, ir.translated(m_dir * (1.0 - m_t) * ir.width(), 0));
		} else {
			drawSlide(p, m_index, ir);
		}
		p.restore();

		drawArrow(p, leftArrowC(), /*left=*/true);
		drawArrow(p, rightArrowC(), /*left=*/false);

		// Dots.
		const qreal dotY = ir.bottom() + S(11);
		const qreal gap = S(14);
		const qreal startX = width() / 2.0 - (m_count - 1) * gap / 2.0;
		const int active = m_animating ? m_incoming : m_index;
		for (int i = 0; i < m_count; ++i) {
			p.setPen(Qt::NoPen);
			p.setBrush(QColor(i == active ? Colors::PRIMARY_COLOR : Colors::TEXT_MUTED));
			p.drawEllipse(QPointF(startX + i * gap, dotY), i == active ? S(4) : S(3),
				      i == active ? S(4) : S(3));
		}
	}

private:
	void drawSlide(QPainter &p, int idx, const QRectF &r)
	{
		const qreal hue = (qreal)idx / m_count;
		QLinearGradient g(r.topLeft(), r.bottomRight());
		g.setColorAt(0, QColor::fromHsvF((float)std::clamp(hue, 0.0, 0.9999), 0.55f, 0.72f));
		g.setColorAt(1, QColor::fromHsvF((float)std::clamp(hue + 0.08, 0.0, 0.9999), 0.7f, 0.5f));
		p.fillRect(r, g);
		QFont f = font();
		f.setPixelSize(S(20));
		f.setWeight(QFont::Weight(800));
		p.setPen(QColor(255, 255, 255, 230));
		drawCentredText(p, f, QString("Slide %1").arg(idx + 1), r);
	}
	void drawArrow(QPainter &p, const QPointF &c, bool left)
	{
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(0, 0, 0, 110));
		p.drawEllipse(c, arrowR(), arrowR());
		p.save();
		p.translate(c);
		p.rotate(left ? 90.0 : -90.0); // drawChevron draws a down-chevron
		QPen cp(QColor(255, 255, 255), std::max(1.4, S(2) * 0.85));
		cp.setCapStyle(Qt::RoundCap);
		cp.setJoinStyle(Qt::RoundJoin);
		p.setPen(cp);
		p.setBrush(Qt::NoBrush);
		detail::drawChevron(p, QPointF(0, 0), S(8), /*up=*/false);
		p.restore();
	}

	int m_count;
	int m_index = 0;
	int m_incoming = 0;
	int m_dir = 1;
	bool m_animating = false;
	qreal m_t = 0.0;
	QVariantAnimation *m_anim = nullptr;
};

} // namespace UIStyles
} // namespace StreamUP
