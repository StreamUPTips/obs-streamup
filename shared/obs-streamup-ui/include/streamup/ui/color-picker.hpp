#pragma once

// ColorPicker — branded HSV colour picker (replaces the native QColorDialog).
// SV square + vertical hue strip + horizontal alpha strip + hex field +
// quick-swatch grid. Header-only, no moc: the sub-widgets paint themselves and
// report back via std::function.
//
// Alpha: color() returns a QColor WITH alpha and setColor() seeds it. The hex
// field accepts both #RRGGBB (alpha = 255) and #RRGGBBAA on input, and DISPLAYS
// 8-digit #RRGGBBAA only when alpha < 255 (else the familiar 6-digit form) so
// fully-opaque colours read the same as before. Default alpha = 255 so existing
// callers (which never touched alpha) are unaffected.

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QAbstractButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QLinearGradient>
#include <QColor>
#include <functional>

#include "gallery-style.hpp"
#include "flow-layout.hpp"

namespace StreamUP {
namespace UIStyles {

namespace detail {

// Saturation (x) × Value (y) square for a fixed hue.
class SVBox : public QWidget {
public:
	std::function<void(qreal, qreal)> onChange;
	explicit SVBox(QWidget *parent = nullptr) : QWidget(parent)
	{
		setFixedSize(S(168), S(120));
		setCursor(Qt::CrossCursor);
	}
	void setHue(qreal h) { m_h = h; update(); }
	void setSV(qreal s, qreal v) { m_s = s; m_v = v; update(); }

protected:
	void mousePressEvent(QMouseEvent *e) override { pick(e->position()); }
	void mouseMoveEvent(QMouseEvent *e) override { pick(e->position()); }
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r(rect());
		QPainterPath clip;
		clip.addRoundedRect(r, S(8), S(8));
		p.setClipPath(clip);

		p.fillRect(r, QColor::fromHsvF(std::clamp(m_h, 0.0, 0.9999), 1.0, 1.0));
		QLinearGradient sat(r.topLeft(), r.topRight());
		sat.setColorAt(0, QColor(255, 255, 255, 255));
		sat.setColorAt(1, QColor(255, 255, 255, 0));
		p.fillRect(r, sat);
		QLinearGradient val(r.topLeft(), r.bottomLeft());
		val.setColorAt(0, QColor(0, 0, 0, 0));
		val.setColorAt(1, QColor(0, 0, 0, 255));
		p.fillRect(r, val);

		const QPointF c(r.left() + m_s * r.width(), r.top() + (1.0 - m_v) * r.height());
		p.setPen(QPen(QColor(0, 0, 0, 160), 1.0));
		p.setBrush(Qt::NoBrush);
		p.drawEllipse(c, S(6), S(6));
		p.setPen(QPen(QColor(255, 255, 255), 1.5));
		p.drawEllipse(c, S(5), S(5));
	}

private:
	void pick(const QPointF &pos)
	{
		m_s = std::clamp(pos.x() / width(), 0.0, 1.0);
		m_v = std::clamp(1.0 - pos.y() / height(), 0.0, 1.0);
		update();
		if (onChange)
			onChange(m_s, m_v);
	}
	qreal m_h = 0.58, m_s = 1.0, m_v = 1.0;
};

// Vertical hue spectrum.
class HueStrip : public QWidget {
public:
	std::function<void(qreal)> onChange;
	explicit HueStrip(QWidget *parent = nullptr) : QWidget(parent)
	{
		setFixedSize(S(16), S(120));
		setCursor(Qt::PointingHandCursor);
	}
	void setHue(qreal h) { m_h = h; update(); }

protected:
	void mousePressEvent(QMouseEvent *e) override { pick(e->position()); }
	void mouseMoveEvent(QMouseEvent *e) override { pick(e->position()); }
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r(rect());
		QPainterPath clip;
		clip.addRoundedRect(r, S(6), S(6));
		p.setClipPath(clip);
		QLinearGradient g(r.topLeft(), r.bottomLeft());
		for (int i = 0; i <= 6; ++i)
			g.setColorAt(i / 6.0, QColor::fromHsvF(i / 6.0 * 0.9999, 1.0, 1.0));
		p.fillRect(r, g);

		const qreal y = r.top() + m_h * r.height();
		p.setClipping(false);
		p.setPen(QPen(QColor(255, 255, 255), 2.0));
		p.setBrush(Qt::NoBrush);
		p.drawRoundedRect(QRectF(r.left() - 1, y - S(3), r.width() + 2, S(6)), S(3), S(3));
	}

private:
	void pick(const QPointF &pos)
	{
		m_h = std::clamp(pos.y() / height(), 0.0, 0.9999);
		update();
		if (onChange)
			onChange(m_h);
	}
	qreal m_h = 0.58;
};

// Horizontal alpha strip — a left→right transparent→opaque gradient of the
// current colour, painted over a checkerboard so the transparent end reads.
// Same visual style/width as the hue strip; draggable like it.
class AlphaStrip : public QWidget {
public:
	std::function<void(qreal)> onChange; // reports alpha 0..1
	explicit AlphaStrip(QWidget *parent = nullptr) : QWidget(parent)
	{
		setFixedSize(S(168), S(16));
		setCursor(Qt::PointingHandCursor);
	}
	// The opaque colour the gradient fades toward (ignores its own alpha).
	void setColor(const QColor &c)
	{
		m_c = QColor(c.red(), c.green(), c.blue());
		update();
	}
	void setAlpha(qreal a) { m_a = std::clamp(a, 0.0, 1.0); update(); }

protected:
	void mousePressEvent(QMouseEvent *e) override { pick(e->position()); }
	void mouseMoveEvent(QMouseEvent *e) override { pick(e->position()); }
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r(rect());
		QPainterPath clip;
		clip.addRoundedRect(r, S(6), S(6));
		p.setClipPath(clip);

		// Checkerboard backdrop so the transparent (left) end reads.
		const qreal cell = S(5);
		const QColor c1(90, 90, 100), c2(60, 60, 70);
		p.fillRect(r, c2);
		for (qreal y = r.top(); y < r.bottom(); y += cell) {
			for (qreal x = r.left(); x < r.right(); x += cell) {
				const int col = (int)((x - r.left()) / cell);
				const int row = (int)((y - r.top()) / cell);
				if ((col + row) & 1)
					p.fillRect(QRectF(x, y, cell, cell), c1);
			}
		}

		// transparent → opaque gradient of the current colour.
		QLinearGradient g(r.topLeft(), r.topRight());
		QColor c0 = m_c; c0.setAlpha(0);
		QColor cf = m_c; cf.setAlpha(255);
		g.setColorAt(0, c0);
		g.setColorAt(1, cf);
		p.fillRect(r, g);

		const qreal x = r.left() + m_a * r.width();
		p.setClipping(false);
		p.setPen(QPen(QColor(255, 255, 255), 2.0));
		p.setBrush(Qt::NoBrush);
		p.drawRoundedRect(QRectF(x - S(3), r.top() - 1, S(6), r.height() + 2), S(3), S(3));
	}

private:
	void pick(const QPointF &pos)
	{
		m_a = std::clamp(pos.x() / width(), 0.0, 1.0);
		update();
		if (onChange)
			onChange(m_a);
	}
	QColor m_c{Colors::PRIMARY_COLOR};
	qreal m_a = 1.0;
};

// Custom-painted square colour swatch. Painted (not a QSS QPushButton) so its
// sizeHint is exactly the fixed square — a QSS button's hint wasn't square and
// the FlowLayout laid the swatches out as wide rectangles.
class Swatch : public QAbstractButton {
public:
	explicit Swatch(const QColor &c, QWidget *parent = nullptr) : QAbstractButton(parent), m_c(c)
	{
		setCursor(Qt::PointingHandCursor);
		setFixedSize(S(22), S(22));
	}
	QSize sizeHint() const override { return QSize(S(22), S(22)); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		p.setPen(QPen(QColor(255, 255, 255, 31), 1.0)); // POPUP_BORDER as QColor
		p.setBrush(m_c);
		p.drawRoundedRect(r, S(6), S(6));
	}

private:
	QColor m_c;
};

} // namespace detail

class ColorPicker : public QWidget {
public:
	std::function<void(const QColor &)> onChanged;

	explicit ColorPicker(QWidget *parent = nullptr) : QWidget(parent)
	{
		auto *v = new QVBoxLayout(this);
		v->setContentsMargins(0, 0, 0, 0);
		v->setSpacing(S(10));

		auto *top = new QHBoxLayout();
		top->setSpacing(S(10));
		m_sv = new detail::SVBox(this);
		m_hue = new detail::HueStrip(this);
		top->addWidget(m_sv);
		top->addWidget(m_hue);
		top->addStretch();
		v->addLayout(top);

		// Horizontal alpha strip below the hue strip, same width as the SV box.
		m_alpha = new detail::AlphaStrip(this);
		auto *alphaRow = new QHBoxLayout();
		alphaRow->setSpacing(S(10));
		alphaRow->addWidget(m_alpha);
		alphaRow->addStretch();
		v->addLayout(alphaRow);

		m_hex = new QLineEdit(this);
		m_hex->setStyleSheet(lineEditStyle(true));
		m_hex->setFixedHeight(S(28));
		m_hex->setMaxLength(9); // "#RRGGBBAA"
		v->addWidget(m_hex);

		// Quick swatches.
		auto *sw = new QWidget(this);
		auto *fl = new FlowLayout(sw, 6);
		for (const char *hex : {Colors::PRIMARY_COLOR, Colors::PRIMARY_LIGHT, Colors::COLOR_SUCCESS,
					Colors::COLOR_WARNING, Colors::COLOR_DANGER, Colors::TAG_COLOR,
					"#ffffff", "#000000", Colors::TEXT_MUTED})
			fl->addWidget(makeSwatch(QColor(hex)));
		v->addWidget(sw);

		m_sv->onChange = [this](qreal s, qreal val) {
			m_s = s;
			m_v = val;
			m_alpha->setColor(QColor::fromHsvF(m_h, m_s, m_v));
			emitColour();
		};
		m_hue->onChange = [this](qreal h) {
			m_h = h;
			m_sv->setHue(h);
			m_alpha->setColor(QColor::fromHsvF(m_h, m_s, m_v));
			emitColour();
		};
		m_alpha->onChange = [this](qreal a) {
			m_a = a;
			emitColour();
		};
		QObject::connect(m_hex, &QLineEdit::editingFinished, this, [this]() {
			QColor c = parseHex(m_hex->text());
			if (c.isValid())
				setColor(c);
		});
		setColor(QColor(Colors::PRIMARY_COLOR)); // QColor(name) → alpha 255
	}

	// Returns the colour WITH alpha.
	QColor color() const
	{
		QColor c = QColor::fromHsvF(m_h, m_s, m_v);
		c.setAlphaF(m_a);
		return c;
	}
	// Seeds hue/sat/value AND alpha. A QColor built from a 6-digit name has
	// alpha 255, so existing callers stay fully opaque.
	void setColor(const QColor &c)
	{
		float h, s, val;
		c.getHsvF(&h, &s, &val);
		if (h < 0)
			h = 0; // achromatic
		m_h = h;
		m_s = s;
		m_v = val;
		m_a = c.alphaF();
		m_sv->setHue(m_h);
		m_sv->setSV(m_s, m_v);
		m_hue->setHue(m_h);
		m_alpha->setColor(QColor::fromHsvF(m_h, m_s, m_v));
		m_alpha->setAlpha(m_a);
		emitColour();
	}

private:
	QAbstractButton *makeSwatch(const QColor &c)
	{
		auto *b = new detail::Swatch(c, this);
		QObject::connect(b, &QAbstractButton::clicked, this, [this, c]() { setColor(c); });
		return b;
	}
	void emitColour()
	{
		const QColor c = color();
		// Show 8-digit #RRGGBBAA only when there's actual transparency; an
		// opaque colour displays the familiar 6-digit form.
		const QString hex = (c.alpha() < 255)
					    ? c.name(QColor::HexArgb).replace(
						      QRegularExpression("^#(..)(......)$"), "#\\2\\1")
					    : c.name(QColor::HexRgb);
		if (m_hex->text().compare(hex, Qt::CaseInsensitive) != 0)
			m_hex->setText(hex);
		if (onChanged)
			onChanged(c);
	}

	// Accept #RRGGBB (alpha 255) and #RRGGBBAA. QColor's own #AARRGGBB parser
	// has the alpha at the FRONT, so handle the #RRGGBBAA case explicitly.
	static QColor parseHex(const QString &text)
	{
		QString s = text.trimmed();
		if (!s.startsWith('#'))
			s.prepend('#');
		if (s.length() == 9) { // #RRGGBBAA → reorder to QColor's #AARRGGBB
			const QString rgb = s.mid(1, 6);
			const QString aa = s.mid(7, 2);
			return QColor(QStringLiteral("#%1%2").arg(aa, rgb));
		}
		return QColor(s); // #RRGGBB (alpha 255) or invalid
	}

	detail::SVBox *m_sv = nullptr;
	detail::HueStrip *m_hue = nullptr;
	detail::AlphaStrip *m_alpha = nullptr;
	QLineEdit *m_hex = nullptr;
	qreal m_h = 0.58, m_s = 1.0, m_v = 1.0, m_a = 1.0;
};

} // namespace UIStyles
} // namespace StreamUP
