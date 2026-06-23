#pragma once

// IOSCheckBox — custom-painted iOS-style checkbox for multi-select lists
// (decision #9). Circular indicator: hollow grey ring when off; accent-filled
// circle with a white tick when on, with a 150ms pop-in animation. Keeps the
// full QCheckBox API (isChecked / setChecked / toggled / text).
//
// No Q_OBJECT macro is added here, so no extra moc is required: we only override
// virtuals and drive the animation via a separate QVariantAnimation object.

#include <QCheckBox>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QEnterEvent>

#include "gallery-style.hpp"

namespace StreamUP {
namespace UIStyles {

class IOSCheckBox : public QCheckBox {
public:
	explicit IOSCheckBox(const QString &text = QString(), QWidget *parent = nullptr)
		: QCheckBox(text, parent)
	{
		setCursor(Qt::PointingHandCursor);
		m_anim = new QVariantAnimation(this);
		m_anim->setDuration(150);
		m_anim->setEasingCurve(QEasingCurve::OutCubic);
		QObject::connect(m_anim, &QVariantAnimation::valueChanged, this,
				 [this](const QVariant &v) {
					 m_progress = v.toReal();
					 update();
				 });
		QObject::connect(this, &QCheckBox::toggled, this, [this](bool on) {
			m_anim->stop();
			m_anim->setStartValue(m_progress);
			m_anim->setEndValue(on ? 1.0 : 0.0);
			m_anim->start();
		});
	}

	QSize sizeHint() const override
	{
		const int d = S(kDiam);
		const QFontMetrics fm(font());
		int w = d + S(kGap) + fm.horizontalAdvance(text()) + S(2);
		int h = std::max(d, fm.height());
		return QSize(w, h);
	}
	QSize minimumSizeHint() const override { return sizeHint(); }

protected:
	void enterEvent(QEnterEvent *e) override
	{
		m_hover = true;
		update();
		QCheckBox::enterEvent(e);
	}
	void leaveEvent(QEvent *e) override
	{
		m_hover = false;
		update();
		QCheckBox::leaveEvent(e);
	}

	void paintEvent(QPaintEvent *) override
	{
		// Sync progress to state without animation the first time we paint.
		if (!m_initialised) {
			m_progress = isChecked() ? 1.0 : 0.0;
			m_initialised = true;
		}

		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);

		const qreal d = S(kDiam);
		const qreal x = 0.0;
		const qreal y = (height() - d) / 2.0;
		const QRectF box(x, y, d, d);
		const QPointF c = box.center();
		const qreal prog = m_progress;

		// Hollow grey ring (fades out as the fill grows in).
		if (prog < 1.0) {
			QColor ring(m_hover ? "#8a8fa3" : Colors::TEXT_MUTED);
			ring.setAlphaF(1.0 - prog);
			QPen pen(ring, S(2) > 0 ? 1.5 * ui_scale() : 1.5);
			p.setPen(pen);
			p.setBrush(Qt::NoBrush);
			const qreal inset = pen.widthF();
			p.drawEllipse(box.adjusted(inset, inset, -inset, -inset));
		}

		// Accent-filled circle, pops in from the centre.
		if (prog > 0.0) {
			const qreal r = (d / 2.0) * prog;
			p.setPen(Qt::NoPen);
			p.setBrush(QColor(Colors::PRIMARY_COLOR));
			p.drawEllipse(c, r, r);

			// White tick, fades in with progress.
			QColor tick(255, 255, 255);
			tick.setAlphaF(prog);
			QPen tickPen(tick, std::max(2.0, d * 0.12));
			tickPen.setCapStyle(Qt::RoundCap);
			tickPen.setJoinStyle(Qt::RoundJoin);
			p.setPen(tickPen);
			QPainterPath path;
			path.moveTo(c.x() - d * 0.22, c.y() + d * 0.02);
			path.lineTo(c.x() - d * 0.04, c.y() + d * 0.18);
			path.lineTo(c.x() + d * 0.24, c.y() - d * 0.16);
			p.drawPath(path);
		}

		// Label.
		if (!text().isEmpty()) {
			p.setPen(QColor(Colors::TEXT_PRIMARY));
			p.setFont(font());
			const int tx = (int)(d + S(kGap));
			p.drawText(QRect(tx, 0, width() - tx, height()),
				   Qt::AlignLeft | Qt::AlignVCenter, text());
		}
	}

private:
	static constexpr int kDiam = 20;
	static constexpr int kGap = 8;
	qreal m_progress = 0.0;
	bool m_hover = false;
	bool m_initialised = false;
	QVariantAnimation *m_anim;
};

} // namespace UIStyles
} // namespace StreamUP
