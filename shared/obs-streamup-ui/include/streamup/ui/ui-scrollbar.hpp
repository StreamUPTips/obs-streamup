#pragma once

// Custom-painted scrollbar (StreamUP standard). Like the pill / switch / combo,
// the handle is hand-painted so its capsule radius is ALWAYS a clean half-width
// round and can never be clipped — QSS `border-radius` on a QScrollBar handle
// clips its corners exactly the way it clips QSS pills.
//
// The widget keeps all native QScrollBar interaction (drag, click, wheel): we
// only override paintEvent. A small internal stylesheet zeroes the arrow buttons
// (so the slider groove spans the full length) and sets the channel width; the
// geometry the base class hit-tests against therefore matches what we paint.

#include <QScrollBar>
#include <QAbstractScrollArea>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QPainter>
#include <QPaintEvent>

#include "gallery-style.hpp"

namespace StreamUP {
namespace UIStyles {

class ScrollBar : public QScrollBar {
public:
	explicit ScrollBar(Qt::Orientation o, QWidget *parent = nullptr) : QScrollBar(o, parent)
	{
		setAttribute(Qt::WA_Hover, true);
		// No arrows (groove spans the whole length); set the channel thickness.
		// Track + handle are painted in paintEvent, so leave their fills to none.
		setStyleSheet(scale_qss(
			QString("QScrollBar:vertical{background:transparent;border:none;width:%1px;margin:0;}"
				"QScrollBar:horizontal{background:transparent;border:none;height:%1px;margin:0;}"
				"QScrollBar::add-line,QScrollBar::sub-line{height:0;width:0;background:none;border:none;}"
				"QScrollBar::add-page,QScrollBar::sub-page{background:none;}")
				.arg(Sizes::SCROLLBAR_SIZE)));
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		const bool vert = orientation() == Qt::Vertical;
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);

		// No channel: just a quiet grey thumb that firms up on hover and drag,
		// so a scrollable list reads calm and the accent stays for actions.
		p.setPen(Qt::NoPen);

		if (maximum() <= minimum())
			return; // nothing to scroll → no handle

		// Handle rect from the (QSS) style so it matches the base hit-testing,
		// then inset to a narrower capsule and paint — radius = half its width,
		// so it's a true pill that QPainter draws inside its own rect (no clip).
		QStyleOptionSlider opt;
		initStyleOption(&opt);
		const QRect hr = style()->subControlRect(QStyle::CC_ScrollBar, &opt,
							 QStyle::SC_ScrollBarSlider, this);
		if (!hr.isValid())
			return;

		const qreal inset = S(2);
		const QRectF h = QRectF(hr).adjusted(inset, inset, -inset, -inset);
		QColor c(255, 255, 255, 46);
		if (isSliderDown())
			c.setAlpha(110);
		else if (underMouse())
			c.setAlpha(80);
		const qreal hrad = (vert ? h.width() : h.height()) / 2.0;
		p.setBrush(c);
		p.drawRoundedRect(h, hrad, hrad);
	}
};

// Install custom-painted scrollbars on any scroll area (QScrollArea, QListWidget,
// QTableWidget, …). Call instead of appending a scrollbar stylesheet.
inline void useScrollBars(QAbstractScrollArea *area)
{
	if (!area)
		return;
	area->setVerticalScrollBar(new ScrollBar(Qt::Vertical));
	area->setHorizontalScrollBar(new ScrollBar(Qt::Horizontal));
}

} // namespace UIStyles
} // namespace StreamUP
