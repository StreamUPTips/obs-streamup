#pragma once

// CollapsibleSection — an expand/collapse group (the branded replacement for a
// QGroupBox). A clickable header row (title + chevron that rotates on toggle)
// over a body whose height animates open/closed. Header-only, no moc: the header
// is a painted QAbstractButton and the body height is a QPropertyAnimation.

#include <QWidget>
#include <QAbstractButton>
#include <QVBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QEasingCurve>

#include "gallery-style.hpp"
#include "mac-inputs.hpp" // detail::drawChevron

namespace StreamUP {
namespace UIStyles {

namespace detail {
// Painted header row: title (left) + a chevron (right) that rotates from ▸ to ▾.
class SectionHeaderButton : public QAbstractButton {
public:
	explicit SectionHeaderButton(const QString &title, QWidget *parent = nullptr)
		: QAbstractButton(parent)
	{
		setText(title);
		setCursor(Qt::PointingHandCursor);
		setAttribute(Qt::WA_Hover, true);
		setMinimumHeight(S(34));
		m_rot = new QVariantAnimation(this);
		m_rot->setDuration(160);
		m_rot->setEasingCurve(QEasingCurve::OutCubic);
		QObject::connect(m_rot, &QVariantAnimation::valueChanged, this,
				 [this](const QVariant &v) { m_t = v.toReal(); update(); });
	}
	void setExpanded(bool on, bool animate)
	{
		const qreal end = on ? 1.0 : 0.0;
		if (!animate) { m_t = end; update(); return; }
		m_rot->stop();
		m_rot->setStartValue(m_t);
		m_rot->setEndValue(end);
		m_rot->start();
	}
	QSize sizeHint() const override { return QSize(S(120), S(34)); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

		QColor bg(Colors::BG_PRIMARY);
		if (underMouse())
			bg = QColor(Colors::BG_PRIMARY).lighter(112);
		p.setPen(Qt::NoPen);
		p.setBrush(bg);
		p.drawRoundedRect(r, S(8), S(8));

		// Title
		QFont tf = font();
		tf.setPixelSize(S(13));
		tf.setWeight(QFont::Weight(800));
		p.setFont(tf);
		p.setPen(QColor(Colors::TEXT_PRIMARY));
		const QFontMetricsF fm(tf);
		p.drawText(QPointF(S(14), r.center().y() + fm.capHeight() / 2.0), text());

		// Chevron: lerp from pointing-right (collapsed) to pointing-down.
		p.save();
		const QPointF c(r.right() - S(16), r.center().y());
		p.translate(c);
		p.rotate(90.0 * m_t); // 0 → ▸ , 1 → ▾ (drawChevron draws a down-chevron)
		QPen cp(QColor(Colors::TEXT_SECONDARY), std::max(1.3, S(2) * 0.8));
		cp.setCapStyle(Qt::RoundCap);
		cp.setJoinStyle(Qt::RoundJoin);
		p.setPen(cp);
		p.setBrush(Qt::NoBrush);
		// A right-pointing chevron rotated by 90*m_t; draw as a down chevron at
		// rotation -90 baseline so m_t=0 points right.
		p.rotate(-90.0);
		detail::drawChevron(p, QPointF(0, 0), S(8), /*up=*/false);
		p.restore();
	}

private:
	QVariantAnimation *m_rot = nullptr;
	qreal m_t = 0.0;
};
} // namespace detail

class CollapsibleSection : public QWidget {
public:
	explicit CollapsibleSection(const QString &title, QWidget *parent = nullptr) : QWidget(parent)
	{
		auto *v = new QVBoxLayout(this);
		v->setContentsMargins(0, 0, 0, 0);
		v->setSpacing(S(6));

		m_header = new detail::SectionHeaderButton(title, this);
		v->addWidget(m_header);

		m_body = new QWidget(this);
		m_bodyLay = new QVBoxLayout(m_body);
		m_bodyLay->setContentsMargins(S(14), S(4), S(14), S(10));
		m_bodyLay->setSpacing(S(8));
		v->addWidget(m_body);

		m_anim = new QPropertyAnimation(m_body, "maximumHeight", this);
		m_anim->setDuration(180);
		m_anim->setEasingCurve(QEasingCurve::OutCubic);

		QObject::connect(m_header, &QAbstractButton::clicked, this,
				 [this]() { setExpanded(!m_expanded); });
		QObject::connect(m_anim, &QPropertyAnimation::finished, this, [this]() {
			if (m_expanded)
				m_body->setMaximumHeight(QWIDGETSIZE_MAX); // allow growth
			else
				m_body->hide();
		});
		applyImmediate();
	}

	void addWidget(QWidget *w) { m_bodyLay->addWidget(w); }

	bool isExpanded() const { return m_expanded; }
	void setExpanded(bool on)
	{
		if (on == m_expanded)
			return;
		m_expanded = on;
		m_header->setExpanded(on, /*animate=*/true);
		const int full = m_body->sizeHint().height();
		m_anim->stop();
		if (on) {
			m_body->setMaximumHeight(0);
			m_body->show();
			m_anim->setStartValue(0);
			m_anim->setEndValue(full);
		} else {
			m_anim->setStartValue(m_body->height());
			m_anim->setEndValue(0);
		}
		m_anim->start();
	}

private:
	void applyImmediate()
	{
		// Start expanded by default.
		m_expanded = true;
		m_header->setExpanded(true, /*animate=*/false);
		m_body->setMaximumHeight(QWIDGETSIZE_MAX);
	}

	detail::SectionHeaderButton *m_header = nullptr;
	QWidget *m_body = nullptr;
	QVBoxLayout *m_bodyLay = nullptr;
	QPropertyAnimation *m_anim = nullptr;
	bool m_expanded = true;
};

} // namespace UIStyles
} // namespace StreamUP
