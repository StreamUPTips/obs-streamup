#pragma once

// ProgressBar — fully custom-painted (track + chunk + caption), like the pill
// and scrollbar. QSS `QProgressBar::chunk` border-radius isn't antialiased, so
// the rounded ends come out jagged; painting the capsule ourselves with
// QPainter antialiasing makes them smooth. The caption is centred via the shared
// drawCentredText (QProgressBar's built-in text rides high in a short bar).
//
// Determinate: grey track + accent chunk (both full-height capsules).
// Indeterminate (setRange(0,0)): an accent segment sweeps across, animated by a
// looping QVariantAnimation. Header-only, no moc (only paintEvent overridden).

#include <QProgressBar>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QShowEvent>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <cmath>

#include "gallery-style.hpp"

namespace StreamUP {
namespace UIStyles {

class ProgressBar : public QProgressBar {
public:
	explicit ProgressBar(QWidget *parent = nullptr) : QProgressBar(parent)
	{
		setTextVisible(false); // we paint everything ourselves
		setFixedHeight(S(18));

		m_busy = new QVariantAnimation(this);
		m_busy->setStartValue(0.0);
		m_busy->setEndValue(1.0);
		m_busy->setDuration(1100);
		m_busy->setLoopCount(-1);
		m_busy->setEasingCurve(QEasingCurve::InOutCubic);
		QObject::connect(m_busy, &QVariantAnimation::valueChanged, this,
				 [this](const QVariant &v) {
					 m_phase = v.toReal();
					 update();
				 });
	}

protected:
	void showEvent(QShowEvent *e) override
	{
		syncBusy(maximum() == minimum());
		QProgressBar::showEvent(e);
	}

	void paintEvent(QPaintEvent *) override
	{
		// QProgressBar has no rangeChanged signal, so reconcile the sweep
		// animation here (cheap: only starts/stops on an actual state change).
		syncBusy(maximum() == minimum());

		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r(rect());
		const qreal rad = r.height() / 2.0;

		// Track — full-height grey capsule.
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(Colors::BG_TERTIARY));
		p.drawRoundedRect(r, rad, rad);

		QPainterPath clip;
		clip.addRoundedRect(r, rad, rad);
		p.setClipPath(clip);
		p.setBrush(QColor(Colors::PRIMARY_COLOR));

		if (maximum() > minimum()) {
			// Determinate accent chunk (min width keeps a clean pill at low %).
			const qreal frac = (qreal)(value() - minimum()) / (maximum() - minimum());
			const qreal w = std::max(r.height(), frac * r.width());
			p.drawRoundedRect(QRectF(r.left(), r.top(), w, r.height()), rad, rad);

			p.setClipping(false);
			QFont f = font();
			f.setPixelSize(S(11));
			f.setWeight(QFont::Weight(700));
			p.setPen(QColor(Colors::TEXT_PRIMARY));
			drawCentredText(p, f, QString::number((int)std::lround(frac * 100.0)) + "%", r);
		} else {
			// Indeterminate sweep — a segment travelling left→right.
			const qreal segW = r.width() * 0.35;
			const qreal x = r.left() - segW + m_phase * (r.width() + segW);
			p.drawRoundedRect(QRectF(x, r.top(), segW, r.height()), rad, rad);
		}
	}

private:
	void syncBusy(bool indeterminate)
	{
		// No update() here — the animation's valueChanged drives repaints, and
		// this is called from paintEvent (calling update() there would loop).
		if (indeterminate && m_busy->state() != QAbstractAnimation::Running)
			m_busy->start();
		else if (!indeterminate && m_busy->state() == QAbstractAnimation::Running)
			m_busy->stop();
	}

	QVariantAnimation *m_busy = nullptr;
	qreal m_phase = 0.0;
};

} // namespace UIStyles
} // namespace StreamUP
