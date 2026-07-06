#pragma once

// SegmentedControl — custom-painted iOS/macOS-style segmented picker for a
// mutually-exclusive choice of 2–4 short options (the branded replacement for a
// radio-button group; radios themselves are out of scope — see the skill).
//
// Hand-painted like the switch / checkbox / pill so it needs no image assets and
// no moc: a rounded track with a sliding accent pill behind the selected label.
// Selection changes invoke an optional std::function callback.

#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QStringList>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <functional>

#include "gallery-style.hpp"

namespace StreamUP {
namespace UIStyles {

class SegmentedControl : public QWidget {
public:
	explicit SegmentedControl(const QStringList &items, QWidget *parent = nullptr)
		: QWidget(parent), m_items(items)
	{
		setCursor(Qt::PointingHandCursor);
		setAttribute(Qt::WA_Hover, true);
		setMinimumHeight(S(30));

		// Animate the selected pill's position (fractional index) so it slides
		// between segments. Header-only: drive via QVariantAnimation, no moc.
		m_anim = new QVariantAnimation(this);
		m_anim->setDuration(200);
		m_anim->setEasingCurve(QEasingCurve::OutCubic);
		QObject::connect(m_anim, &QVariantAnimation::valueChanged, this,
				 [this](const QVariant &v) {
					 m_pos = v.toReal();
					 update();
				 });
	}

	int currentIndex() const { return m_index; }
	void setCurrentIndex(int i)
	{
		if (i < 0 || i >= m_items.size() || i == m_index)
			return;
		m_index = i;
		m_anim->stop();
		m_anim->setStartValue(m_pos);
		m_anim->setEndValue((qreal)i);
		m_anim->start();
		if (m_onChange)
			m_onChange(i);
	}
	void onChanged(std::function<void(int)> cb) { m_onChange = std::move(cb); }

	QSize sizeHint() const override
	{
		const QFontMetrics fm(segFont());
		int w = 0;
		for (const QString &s : m_items)
			w = std::max(w, fm.horizontalAdvance(s));
		return QSize((w + S(24)) * std::max(1, (int)m_items.size()) + S(6), S(30));
	}
	QSize minimumSizeHint() const override { return sizeHint(); }

protected:
	void mousePressEvent(QMouseEvent *e) override
	{
		if (m_items.isEmpty())
			return;
		const qreal segW = (qreal)width() / m_items.size();
		setCurrentIndex(std::clamp((int)(e->position().x() / segW), 0, (int)m_items.size() - 1));
	}

	void paintEvent(QPaintEvent *) override
	{
		if (!m_initialised) { // snap to current segment on first paint (no slide)
			m_pos = m_index;
			m_initialised = true;
		}

		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);

		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		const qreal rad = S(8);

		// Track (input-on-base surface).
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(Colors::INPUT_ON_BASE));
		p.drawRoundedRect(r, rad, rad);

		if (m_items.isEmpty())
			return;

		// Selected accent pill, drawn at the animated (fractional) position so it
		// slides between segments.
		const qreal segW = r.width() / m_items.size();
		const QRectF pill(r.left() + segW * m_pos + S(2), r.top() + S(2),
				  segW - S(4), r.height() - S(4));
		p.setBrush(QColor(Colors::PRIMARY_COLOR));
		p.drawRoundedRect(pill, rad - S(2), rad - S(2));

		// Labels (cap-height centred via drawCentredText, like the pill/badge).
		const QFont lf = segFont();
		for (int i = 0; i < m_items.size(); ++i) {
			const QRectF seg(r.left() + segW * i, r.top(), segW, r.height());
			p.setPen(i == m_index ? QColor("#ffffff") : QColor(Colors::TEXT_SECONDARY));
			drawCentredText(p, lf, m_items[i], seg);
		}
	}

private:
	QFont segFont() const
	{
		QFont f = font();
		f.setPixelSize(S(12));
		f.setWeight(QFont::Weight(700));
		return f;
	}

	QStringList m_items;
	int m_index = 0;
	qreal m_pos = 0.0;       // animated (fractional) pill position
	bool m_initialised = false;
	QVariantAnimation *m_anim = nullptr;
	std::function<void(int)> m_onChange;
};

} // namespace UIStyles
} // namespace StreamUP
