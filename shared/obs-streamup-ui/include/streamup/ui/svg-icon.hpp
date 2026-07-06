#pragma once

// SVG icon helper — renders a Tabler-style outline icon recoloured to a token,
// the way the shipping plugins do (QSvgRenderer + per-call stroke colour). Real
// plugins should bundle the full Tabler set via qrc; here a small curated set is
// inlined to demonstrate the recolour pipeline. Sizes go through S().

#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QByteArray>
#include <QString>
#include <QSvgRenderer>

#include "gallery-style.hpp"

namespace StreamUP {
namespace UIStyles {

namespace detail {
// Inner SVG markup for a few Tabler-style outline icons (24×24, stroke only).
inline QString tablerInner(const QString &name)
{
	if (name == "search")
		return "<circle cx='10' cy='10' r='7'/><line x1='21' y1='21' x2='15.7' y2='15.7'/>";
	if (name == "check")
		return "<polyline points='5 12 10 17 19 8'/>";
	if (name == "x")
		return "<line x1='6' y1='6' x2='18' y2='18'/><line x1='18' y1='6' x2='6' y2='18'/>";
	if (name == "trash")
		return "<polyline points='4 7 20 7'/><path d='M6 7l1 13h10l1 -13'/>"
		       "<line x1='10' y1='11' x2='10' y2='17'/><line x1='14' y1='11' x2='14' y2='17'/>"
		       "<path d='M9 7v-3h6v3'/>";
	if (name == "eye")
		return "<path d='M2 12s3.5 -7 10 -7 10 7 10 7 -3.5 7 -10 7 -10 -7 -10 -7z'/>"
		       "<circle cx='12' cy='12' r='3'/>";
	if (name == "star")
		return "<polygon points='12 3 15 9 21.5 9.7 16.7 14.2 18 20.7 12 17.3 6 20.7 7.3 14.2 2.5 9.7 9 9'/>";
	if (name == "settings")
		return "<circle cx='12' cy='12' r='3'/>"
		       "<path d='M12 3v3M12 18v3M3 12h3M18 12h3M5.6 5.6l2.1 2.1M16.3 16.3l2.1 2.1"
		       "M18.4 5.6l-2.1 2.1M7.7 16.3l-2.1 2.1'/>";
	return "<circle cx='12' cy='12' r='8'/>"; // fallback
}
} // namespace detail

// Render a Tabler-style icon at design size `designPx`, stroked in `color`.
inline QPixmap svgIcon(const QString &name, const QColor &color, int designPx = 18)
{
	const int px = S(designPx);
	const QString svg =
		QString("<svg xmlns='http://www.w3.org/2000/svg' width='%1' height='%1' "
			"viewBox='0 0 24 24' fill='none' stroke='%2' stroke-width='2' "
			"stroke-linecap='round' stroke-linejoin='round'>%3</svg>")
			.arg(px)
			.arg(color.name(), detail::tablerInner(name));
	QSvgRenderer renderer(svg.toUtf8());
	QPixmap pm(px, px);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing, true);
	renderer.render(&p);
	return pm;
}

} // namespace UIStyles
} // namespace StreamUP
