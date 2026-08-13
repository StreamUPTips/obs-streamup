#pragma once

// The toolbar has two axes: the one items run along, and the one they sit
// across. Which of those is x and which is y depends on where the toolbar is
// docked, and that is the only thing orientation should ever mean.
//
// Every orientation bug this plugin has had came from the same mistake: two
// pieces of code deciding independently what the cross axis does, and
// disagreeing. Spacers pinned their flow axis and left the cross axis on a zero
// size hint, so they vanished. Separators were given a fixed thickness and a
// comment promising the other axis would stretch, then added with an alignment
// flag that gave them their size hint instead, so they were zero wide and
// invisible on a side-docked bar for as long as the feature has existed.
//
// So orientation lives here and nowhere else. Everything downstream is written
// in terms of along and across, and Axis maps that to x and y once. There
// should be no `if (vertical)` anywhere outside this file.

#include <QBoxLayout>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QSizePolicy>

namespace StreamUP {
namespace ToolbarGeom {

class Axis {
public:
	explicit Axis(bool vertical = false) : vertical_(vertical) {}

	bool vertical() const { return vertical_; }

	// Reading a point or a rect in axis terms.
	int along(const QPoint &p) const { return vertical_ ? p.y() : p.x(); }
	int across(const QPoint &p) const { return vertical_ ? p.x() : p.y(); }
	int alongLength(const QSize &s) const { return vertical_ ? s.height() : s.width(); }
	int acrossLength(const QSize &s) const { return vertical_ ? s.width() : s.height(); }

	int alongStart(const QRect &r) const { return vertical_ ? r.top() : r.left(); }
	int alongEnd(const QRect &r) const { return vertical_ ? r.bottom() : r.right(); }
	int alongCentre(const QRect &r) const { return vertical_ ? r.center().y() : r.center().x(); }
	int alongLength(const QRect &r) const { return alongLength(r.size()); }
	int acrossStart(const QRect &r) const { return vertical_ ? r.left() : r.top(); }
	int acrossLength(const QRect &r) const { return acrossLength(r.size()); }

	// Building a rect from axis terms.
	QRect rect(int alongPos, int alongLen, int acrossPos, int acrossLen) const
	{
		return vertical_ ? QRect(acrossPos, alongPos, acrossLen, alongLen)
				 : QRect(alongPos, acrossPos, alongLen, acrossLen);
	}

	QSize size(int alongLen, int acrossLen) const
	{
		return vertical_ ? QSize(acrossLen, alongLen) : QSize(alongLen, acrossLen);
	}

	QBoxLayout::Direction layoutDirection() const
	{
		return vertical_ ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
	}

	// A size policy expressed in axis terms rather than in width and height,
	// so "pinned along, fills across" reads the same whichever way we run.
	QSizePolicy policy(QSizePolicy::Policy alongPolicy, QSizePolicy::Policy acrossPolicy) const
	{
		return vertical_ ? QSizePolicy(acrossPolicy, alongPolicy) : QSizePolicy(alongPolicy, acrossPolicy);
	}

private:
	bool vertical_ = false;
};

} // namespace ToolbarGeom
} // namespace StreamUP
