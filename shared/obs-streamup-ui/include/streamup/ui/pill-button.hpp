#pragma once

// Custom-painted pill button. Hand-painted (like the switch/checkbox/combo) so
// the pill is ALWAYS drawn inside the widget rect — it can never be clipped, and
// the text is always perfectly centred. Variants: primary|danger|success|
// warning|outline|neutral.

#include <QPushButton>
#include <QPainter>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPolygonF>
#include <QPainterPath>
#include <QIcon>
#include <QPixmap>

#include "gallery-style.hpp"

namespace StreamUP {
namespace UIStyles {

class PillButton : public QPushButton {
public:
	PillButton(const QString &text, const QString &variant, QWidget *parent = nullptr)
		: QPushButton(text, parent), m_variant(variant)
	{
		setCursor(Qt::PointingHandCursor);
		setAttribute(Qt::WA_Hover, true); // repaint on hover
		setMinimumHeight(S(30));
	}

	// Optional leading icon, painted to the LEFT of the label. The icon+gap+label
	// are centred as a GROUP within the pill. `sizePx` is the design-size square
	// edge of the icon (scaled via S()). Pass a null QIcon to clear it.
	void setLeadingIcon(const QIcon &icon, int sizePx = 14)
	{
		m_icon = icon;
		m_iconPx = sizePx;
		updateGeometry();
		update();
	}

	QSize sizeHint() const override
	{
		QFontMetrics fm(buttonFont());
		int w = fm.horizontalAdvance(text()) + S(36);
		if (!m_icon.isNull())
			w += S(m_iconPx) + S(6); // icon square + gap before the label
		return QSize(std::max(w, S(64)), S(30));
	}
	QSize minimumSizeHint() const override { return sizeHint(); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);

		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		const qreal rad = r.height() / 2.0; // always a true pill

		QColor fill, textc;
		const bool outline = (m_variant == "outline");
		resolve(fill, textc);

		if (!isEnabled()) {
			fill = QColor("#2a2a3a");
			textc = QColor(205, 214, 244);
			textc.setAlphaF(0.35f);
		} else if (isDown()) {
			fill = pressedColor();
		} else if (underMouse()) {
			fill = hoverColor();
		}

		if (outline && isEnabled()) {
			QColor accent(Colors::PRIMARY_COLOR);
			QColor f(0, 118, 223);
			f.setAlphaF(isDown() ? 0.16f : (underMouse() ? 0.08f : 0.0f));
			p.setBrush(f);
			QPen pen(accent);
			pen.setWidthF(1.2);
			p.setPen(pen);
			p.drawRoundedRect(r, rad, rad);
			textc = accent;
		} else {
			p.setPen(Qt::NoPen);
			p.setBrush(fill);
			p.drawRoundedRect(r, rad, rad);
		}

		// Centre on the font's CAP HEIGHT (via drawCentredText), not the em box.
		// AlignVCenter / the ascent-descent baseline both read high for caps/
		// no-descender labels because the em box reserves descender space the
		// glyphs don't use. Cap-height centring puts the capital band dead-centre
		// and gives every label the same baseline regardless of descenders.
		const QFont bf = buttonFont();
		p.setFont(bf);
		p.setPen(textc);

		if (m_icon.isNull()) {
			drawCentredText(p, bf, text(), QRectF(rect()));
		} else {
			// Icon + gap + label, centred as a GROUP horizontally within the pill.
			const int iconSz = S(m_iconPx);
			const int gap = S(6);
			const QFontMetricsF fm(bf);
			const qreal textW = fm.horizontalAdvance(text());
			const qreal groupW = iconSz + gap + textW;
			const qreal x0 = r.center().x() - groupW / 2.0;

			// Icon square, vertically centred. Dim with the label when disabled.
			const QRectF iconRect(x0, r.center().y() - iconSz / 2.0, iconSz, iconSz);
			QPixmap pm = m_icon.pixmap(QSize(iconSz, iconSz));
			if (!pm.isNull()) {
				p.save();
				if (!isEnabled())
					p.setOpacity(0.35);
				p.drawPixmap(iconRect.toRect(), pm);
				p.restore();
			}

			// Label, on the same cap-height baseline used everywhere else.
			const qreal baseline = r.center().y() + fm.capHeight() / 2.0;
			p.drawText(QPointF(x0 + iconSz + gap, baseline), text());
		}
	}

private:
	QFont buttonFont() const
	{
		QFont f = font();
		f.setPixelSize(S(11));
		f.setWeight(QFont::Weight(800));
		return f;
	}
	void resolve(QColor &fill, QColor &textc) const
	{
		if (m_variant == "danger") {
			fill = QColor(Colors::COLOR_DANGER);
			textc = QColor("#ffffff");
		} else if (m_variant == "success") {
			fill = QColor(Colors::COLOR_SUCCESS);
			textc = QColor("#11111b");
		} else if (m_variant == "warning") {
			fill = QColor(Colors::COLOR_WARNING);
			textc = QColor("#11111b"); // dark ink on amber, like success
		} else if (m_variant == "neutral") {
			fill = QColor(Colors::BG_TERTIARY);
			textc = QColor(Colors::TEXT_PRIMARY);
		} else if (m_variant == "outline") {
			fill = QColor(0, 0, 0, 0);
			textc = QColor(Colors::PRIMARY_COLOR);
		} else { // primary
			fill = QColor(Colors::PRIMARY_COLOR);
			textc = QColor("#ffffff");
		}
	}
	QColor hoverColor() const
	{
		if (m_variant == "danger")
			return QColor("#ff6258");
		if (m_variant == "success")
			return QColor("#79d07a");
		if (m_variant == "warning")
			return QColor("#fbc4a0"); // slightly lighter amber
		if (m_variant == "neutral")
			return QColor(Colors::BG_TERTIARY).lighter(125);
		return QColor(Colors::PRIMARY_HOVER);
	}
	QColor pressedColor() const
	{
		if (m_variant == "danger")
			return QColor("#d93a30");
		if (m_variant == "success")
			return QColor("#54b055");
		if (m_variant == "warning")
			return QColor("#e89b6f"); // slightly darker amber
		if (m_variant == "neutral")
			return QColor(Colors::BG_TERTIARY).lighter(110);
		return QColor(Colors::PRIMARY_PRESSED);
	}

	QString m_variant;
	QIcon m_icon;       // optional leading icon (null = text-only pill)
	int m_iconPx = 14;  // design-size square edge of the leading icon
};

// Square icon button — S(28), radius 8 (NOT a pill), centred vector glyph, same
// hover tokens as the neutral pill. Glyphs are hand-painted so no assets/font
// glyphs are needed (the standard's "icon button" pattern).
class IconButton : public QPushButton {
public:
	enum class Glyph { Plus, Minus, More, Refresh, Settings, Bold, Italic, Trash, Activate, Deactivate, Feedback };

	explicit IconButton(Glyph glyph, QWidget *parent = nullptr)
		: QPushButton(parent), m_glyph(glyph)
	{
		setCursor(Qt::PointingHandCursor);
		setAttribute(Qt::WA_Hover, true);
		setFixedSize(S(28), S(28));
	}
	QSize sizeHint() const override { return QSize(S(28), S(28)); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

		// Checkable (toggle) icon button: checked = accent fill + white glyph.
		const bool on = isCheckable() && isChecked();
		QColor bg = on ? QColor(Colors::PRIMARY_COLOR) : QColor(Colors::BG_TERTIARY);
		if (!isEnabled())
			bg = QColor("#2a2a3a");
		else if (isDown())
			bg = on ? QColor(Colors::PRIMARY_PRESSED) : QColor(Colors::BG_TERTIARY).lighter(110);
		else if (underMouse())
			bg = on ? QColor(Colors::PRIMARY_HOVER) : QColor(Colors::BG_TERTIARY).lighter(125);
		p.setPen(Qt::NoPen);
		p.setBrush(bg);
		p.drawRoundedRect(r, S(8), S(8));

		// Letter glyphs (Bold/Italic) draw text, not strokes.
		if (m_glyph == Glyph::Bold || m_glyph == Glyph::Italic) {
			QColor tc = on ? QColor("#ffffff") : QColor(Colors::TEXT_PRIMARY);
			if (!isEnabled())
				tc.setAlphaF(0.35f);
			QFont gf = font();
			gf.setPixelSize(S(14));
			gf.setBold(m_glyph == Glyph::Bold);
			gf.setItalic(m_glyph == Glyph::Italic);
			gf.setWeight(m_glyph == Glyph::Bold ? QFont::Black : QFont::Medium);
			p.setPen(tc);
			drawCentredText(p, gf, m_glyph == Glyph::Bold ? "B" : "I", r);
			return;
		}

		QColor ink = on ? QColor("#ffffff") : QColor(Colors::TEXT_PRIMARY);
		if (!isEnabled())
			ink.setAlphaF(0.35f);
		QPen pen(ink, std::max(1.4, S(2) * 0.85));
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);

		const QPointF c = r.center();
		const qreal d = S(5);
		switch (m_glyph) {
		case Glyph::Plus:
			p.drawLine(c + QPointF(-d, 0), c + QPointF(d, 0));
			p.drawLine(c + QPointF(0, -d), c + QPointF(0, d));
			break;
		case Glyph::Minus:
			p.drawLine(c + QPointF(-d, 0), c + QPointF(d, 0));
			break;
		case Glyph::More: { // three horizontal dots
			p.setPen(Qt::NoPen);
			p.setBrush(ink);
			const qreal dot = std::max(1.4, S(2) * 0.9);
			for (int i = -1; i <= 1; ++i)
				p.drawEllipse(c + QPointF(i * S(6), 0), dot, dot);
			break;
		}
		case Glyph::Refresh: { // open circular arrow
			QRectF a(c.x() - d, c.y() - d, 2 * d, 2 * d);
			p.drawArc(a, 60 * 16, 280 * 16);
			p.setPen(Qt::NoPen);
			p.setBrush(ink);
			QPolygonF head;
			head << c + QPointF(d, -d * 0.2) << c + QPointF(d - S(3), -d * 0.9)
			     << c + QPointF(d + S(2), -d * 0.9);
			p.drawPolygon(head);
			break;
		}
		case Glyph::Settings: { // gear-ish: ring + centre dot
			p.drawEllipse(c, d, d);
			p.setPen(Qt::NoPen);
			p.setBrush(ink);
			p.drawEllipse(c, d * 0.32, d * 0.32);
			break;
		}
		case Glyph::Trash: { // waste bin: lid + body + inner bars
			const qreal w = d * 1.0;                 // half-width of the body
			const qreal top = c.y() - d * 0.7;       // lid line y
			const qreal bot = c.y() + d * 1.05;      // bin bottom
			// Lid (full width, slightly wider than body) + handle nub on top.
			p.drawLine(QPointF(c.x() - w - S(1), top), QPointF(c.x() + w + S(1), top));
			p.drawLine(QPointF(c.x() - w * 0.45, top - S(2)),
				   QPointF(c.x() + w * 0.45, top - S(2)));
			p.drawLine(QPointF(c.x() - w * 0.45, top - S(2)), QPointF(c.x() - w * 0.45, top));
			p.drawLine(QPointF(c.x() + w * 0.45, top - S(2)), QPointF(c.x() + w * 0.45, top));
			// Body: two slanted sides + bottom.
			p.drawLine(QPointF(c.x() - w * 0.8, top + S(1)), QPointF(c.x() - w * 0.6, bot));
			p.drawLine(QPointF(c.x() + w * 0.8, top + S(1)), QPointF(c.x() + w * 0.6, bot));
			p.drawLine(QPointF(c.x() - w * 0.6, bot), QPointF(c.x() + w * 0.6, bot));
			// Two vertical bars.
			p.drawLine(QPointF(c.x() - w * 0.28, top + d * 0.55),
				   QPointF(c.x() - w * 0.28, bot - S(2)));
			p.drawLine(QPointF(c.x() + w * 0.28, top + d * 0.55),
				   QPointF(c.x() + w * 0.28, bot - S(2)));
			break;
		}
		case Glyph::Activate: { // eye-open (show / turn on)
			const qreal ew = d * 1.25; // half eye width
			QPainterPath eye;
			eye.moveTo(c.x() - ew, c.y());
			eye.quadTo(c.x(), c.y() - d * 1.05, c.x() + ew, c.y());
			eye.quadTo(c.x(), c.y() + d * 1.05, c.x() - ew, c.y());
			p.drawPath(eye);
			p.drawEllipse(c, d * 0.42, d * 0.42); // pupil ring
			break;
		}
		case Glyph::Deactivate: { // eye-open with a slash (hide / turn off)
			const qreal ew = d * 1.25;
			QPainterPath eye;
			eye.moveTo(c.x() - ew, c.y());
			eye.quadTo(c.x(), c.y() - d * 1.05, c.x() + ew, c.y());
			eye.quadTo(c.x(), c.y() + d * 1.05, c.x() - ew, c.y());
			p.drawPath(eye);
			p.drawEllipse(c, d * 0.42, d * 0.42);
			// Diagonal slash across the eye.
			p.drawLine(c + QPointF(-d * 1.2, -d * 1.2), c + QPointF(d * 1.2, d * 1.2));
			break;
		}
		case Glyph::Feedback: { // speech/chat bubble (rounded rect) + tail + 3 dots
			const qreal bw = d * 1.5;  // half bubble width
			const qreal bh = d * 1.1;  // half bubble height
			const qreal by = c.y() - d * 0.25; // bubble centred slightly high (tail below)
			const QRectF bubble(c.x() - bw, by - bh, bw * 2, bh * 2);
			QPainterPath path;
			path.addRoundedRect(bubble, S(3), S(3));
			// Small tail dropping from the lower-left of the bubble.
			QPolygonF tail;
			tail << QPointF(c.x() - bw * 0.35, by + bh - 0.5)
			     << QPointF(c.x() - bw * 0.65, by + bh + d * 0.7)
			     << QPointF(c.x() - bw * 0.02, by + bh - 0.5);
			path.addPolygon(tail);
			p.drawPath(path.simplified());
			// Three dots inside the bubble.
			p.setPen(Qt::NoPen);
			p.setBrush(ink);
			const qreal dot = std::max(1.0, S(1) * 0.95);
			for (int i = -1; i <= 1; ++i)
				p.drawEllipse(QPointF(c.x() + i * d * 0.62, by), dot, dot);
			break;
		}
		case Glyph::Bold:   // handled above (letter glyphs)
		case Glyph::Italic:
			break;
		}
	}

private:
	Glyph m_glyph;
};

// Status chip / badge / tag — custom-painted (like the pill) so the text is
// baseline-centred and the rounded fill never clips. Pass a fill + ink colour
// (use an alpha'd fill + solid ink for a soft tinted chip).
class Badge : public QWidget {
public:
	Badge(const QString &text, const QColor &fill, const QColor &ink, QWidget *parent = nullptr)
		: QWidget(parent), m_text(text), m_fill(fill), m_ink(ink)
	{
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); // chip never stretches
	}
	QSize sizeHint() const override
	{
		const QFontMetrics fm(badgeFont());
		return QSize(fm.horizontalAdvance(m_text) + S(20), S(20));
	}
	QSize minimumSizeHint() const override { return sizeHint(); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		p.setPen(Qt::NoPen);
		p.setBrush(m_fill);
		p.drawRoundedRect(r, r.height() / 2.0, r.height() / 2.0);

		p.setPen(m_ink);
		drawCentredText(p, badgeFont(), m_text, QRectF(rect()));
	}

private:
	QFont badgeFont() const
	{
		QFont f = font();
		f.setPixelSize(S(11));
		f.setWeight(QFont::Weight(800));
		return f;
	}
	QString m_text;
	QColor m_fill, m_ink;
};

} // namespace UIStyles
} // namespace StreamUP
