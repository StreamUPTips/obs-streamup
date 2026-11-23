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
	// Tabler outline icons (MIT), copied path for path so they match the set.
	if (name == "message")
		return "<path d='M8 9h8'/><path d='M8 13h6'/>"
		       "<path d='M18 4a3 3 0 0 1 3 3v8a3 3 0 0 1 -3 3h-5l-5 3v-3h-2a3 3 0 0 1 -3 -3v-8a3 3 0 0 1 3 -3h12z'/>";
	if (name == "restore")
		return "<path d='M3.06 13a9 9 0 1 0 .49 -4.087'/><path d='M3 4.001v5h5'/>"
		       "<path d='M11 12a1 1 0 1 0 2 0a1 1 0 1 0 -2 0'/>";
	if (name == "trash-tabler")
		return "<path d='M4 7l16 0'/><path d='M10 11l0 6'/><path d='M14 11l0 6'/>"
		       "<path d='M5 7l1 12a2 2 0 0 0 2 2h8a2 2 0 0 0 2 -2l1 -12'/>"
		       "<path d='M9 7v-3a1 1 0 0 1 1 -1h4a1 1 0 0 1 1 1v3'/>";
	if (name == "plus")
		return "<path d='M12 5l0 14'/><path d='M5 12l14 0'/>";
	if (name == "pencil")
		return "<path d='M4 20h4l10.5 -10.5a2.828 2.828 0 1 0 -4 -4l-10.5 10.5v4'/><path d='M13.5 6.5l4 4'/>";
	if (name == "eraser")
		return "<path d='M19 20h-10.5l-4.21 -4.3a1 1 0 0 1 0 -1.41l10 -10a1 1 0 0 1 1.41 0l5 5a1 1 0 0 1 0 1.41l-9.2 9.3'/>"
		       "<path d='M18 13.3l-6.3 -6.3'/>";
	if (name == "circle-check")
		return "<path d='M3 12a9 9 0 1 0 18 0a9 9 0 1 0 -18 0'/><path d='M9 12l2 2l4 -4'/>";
	if (name == "alert-triangle")
		return "<path d='M12 9v4'/>"
		       "<path d='M10.363 3.591l-8.106 13.534a1.914 1.914 0 0 0 1.636 2.871h16.214a1.914 1.914 0 0 0 1.636 -2.87l-8.106 -13.536a1.914 1.914 0 0 0 -3.274 0z'/>"
		       "<path d='M12 16h.01'/>";
	if (name == "circle-x")
		return "<path d='M3 12a9 9 0 1 0 18 0a9 9 0 1 0 -18 0'/><path d='M10 10l4 4m0 -4l-4 4'/>";
	if (name == "info-circle")
		return "<path d='M3 12a9 9 0 1 0 18 0a9 9 0 0 0 -18 0'/><path d='M12 9h.01'/><path d='M11 12h1v4h1'/>";
	if (name == "player-play")
		return "<path d='M7 4v16l13 -8z'/>";
	if (name == "brand-youtube")
		return "<path d='M2 8a4 4 0 0 1 4 -4h12a4 4 0 0 1 4 4v8a4 4 0 0 1 -4 4h-12a4 4 0 0 1 -4 -4v-8z'/>"
		       "<path d='M10 9l5 3l-5 3z'/>";
	if (name == "external-link")
		return "<path d='M12 6h-6a2 2 0 0 0 -2 2v10a2 2 0 0 0 2 2h10a2 2 0 0 0 2 -2v-6'/>"
		       "<path d='M11 13l9 -9'/><path d='M15 4h5v5'/>";
	if (name == "download")
		return "<path d='M4 17v2a2 2 0 0 0 2 2h12a2 2 0 0 0 2 -2v-2'/><path d='M7 11l5 5l5 -5'/><path d='M12 4l0 12'/>";
	if (name == "maximize")
		return "<path d='M4 8v-2a2 2 0 0 1 2 -2h2'/><path d='M4 16v2a2 2 0 0 0 2 2h2'/>"
		       "<path d='M16 4h2a2 2 0 0 1 2 2v2'/><path d='M16 20h2a2 2 0 0 0 2 -2v-2'/>";
	if (name == "palette")
		return "<path d='M12 21a9 9 0 0 1 0 -18c4.97 0 9 3.582 9 8c0 1.06 -.474 2.078 -1.318 2.828c-.844 .75 -1.989 1.172 -3.182 1.172h-2.5a2 2 0 0 0 -1 3.75a1.3 1.3 0 0 1 -1 2.25'/>"
		       "<path d='M7.5 10.5a1 1 0 1 0 2 0a1 1 0 1 0 -2 0'/><path d='M11.5 7.5a1 1 0 1 0 2 0a1 1 0 1 0 -2 0'/>"
		       "<path d='M15.5 10.5a1 1 0 1 0 2 0a1 1 0 1 0 -2 0'/>";
	if (name == "heart")
		return "<path d='M19.5 12.572l-7.5 7.428l-7.5 -7.428a5 5 0 1 1 7.5 -6.566a5 5 0 1 1 7.5 6.572'/>";
	if (name == "sparkles")
		return "<path d='M16 18a2 2 0 0 1 2 2a2 2 0 0 1 2 -2a2 2 0 0 1 -2 -2a2 2 0 0 1 -2 2zm0 -12a2 2 0 0 1 2 2a2 2 0 0 1 2 -2a2 2 0 0 1 -2 -2a2 2 0 0 1 -2 2zm-7 12a6 6 0 0 1 6 -6a6 6 0 0 1 -6 -6a6 6 0 0 1 -6 6a6 6 0 0 1 6 6z'/>";
	if (name == "users")
		return "<path d='M5 7a4 4 0 1 0 8 0a4 4 0 1 0 -8 0'/><path d='M3 21v-2a4 4 0 0 1 4 -4h4a4 4 0 0 1 4 4v2'/>"
		       "<path d='M16 3.13a4 4 0 0 1 0 7.75'/><path d='M21 21v-2a4 4 0 0 0 -3 -3.85'/>";
	if (name == "tools")
		return "<path d='M3 21h4l13 -13a1.5 1.5 0 0 0 -4 -4l-13 13v4'/><path d='M14.5 5.5l4 4'/>"
		       "<path d='M12 8l-5 -5l-4 4l5 5'/><path d='M7 8l-1.5 1.5'/><path d='M16 12l5 5l-4 4l-5 -5'/><path d='M16 17l-1.5 1.5'/>";
	if (name == "package")
		return "<path d='M12 3l8 4.5l0 9l-8 4.5l-8 -4.5l0 -9l8 -4.5'/><path d='M12 12l8 -4.5'/>"
		       "<path d='M12 12l0 9'/><path d='M12 12l-8 -4.5'/>";
	if (name == "typography")
		return "<path d='M4 20l3 0'/><path d='M14 20l7 0'/><path d='M6.9 15l6.9 0'/><path d='M10.2 6.3l5.8 13.7'/>"
		       "<path d='M5 20l6 -16l2 0l7 16'/>";
	if (name == "minus")
		return "<path d='M5 12l14 0'/>";
	if (name == "dots")
		return "<path d='M4 12a1 1 0 1 0 2 0a1 1 0 1 0 -2 0'/><path d='M11 12a1 1 0 1 0 2 0a1 1 0 1 0 -2 0'/>"
		       "<path d='M18 12a1 1 0 1 0 2 0a1 1 0 1 0 -2 0'/>";
	if (name == "eye-tabler")
		return "<path d='M10 12a2 2 0 1 0 4 0a2 2 0 0 0 -4 0'/>"
		       "<path d='M21 12c-2.4 4 -5.4 6 -9 6c-3.6 0 -6.6 -2 -9 -6c2.4 -4 5.4 -6 9 -6c3.6 0 6.6 2 9 6'/>";
	if (name == "eye-off")
		return "<path d='M10.585 10.587a2 2 0 0 0 2.829 2.828'/>"
		       "<path d='M16.681 16.673a8.717 8.717 0 0 1 -4.681 1.327c-3.6 0 -6.6 -2 -9 -6c1.272 -2.12 2.712 -3.678 4.32 -4.674m2.86 -1.146a9.055 9.055 0 0 1 1.82 -.18c3.6 0 6.6 2 9 6c-.666 1.11 -1.379 2.067 -2.138 2.87'/>"
		       "<path d='M3 3l18 18'/>";
	if (name == "refresh")
		return "<path d='M20 11a8.1 8.1 0 0 0 -15.5 -2m-.5 -4v4h4'/><path d='M4 13a8.1 8.1 0 0 0 15.5 2m.5 4v-4h-4'/>";
	return "<circle cx='12' cy='12' r='8'/>"; // fallback
}
} // namespace detail

// Render a Tabler-style icon at design size `designPx`, stroked in `color`.
inline QPixmap svgIcon(const QString &name, const QColor &color, int designPx = 18, qreal dpr = 1.0)
{
	const int px = qRound(S(designPx) * dpr);
	const QString svg =
		QString("<svg xmlns='http://www.w3.org/2000/svg' width='%1' height='%1' "
			"viewBox='0 0 24 24' fill='none' stroke='%2' stroke-opacity='%4' stroke-width='2' "
			"stroke-linecap='round' stroke-linejoin='round'>%3</svg>")
			.arg(px)
			.arg(color.name(), detail::tablerInner(name))
			.arg(color.alphaF(), 0, 'f', 2);
	QSvgRenderer renderer(svg.toUtf8());
	QPixmap pm(px, px);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing, true);
	renderer.render(&p);
	p.end();
	pm.setDevicePixelRatio(dpr);
	return pm;
}

} // namespace UIStyles
} // namespace StreamUP
