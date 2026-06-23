#pragma once

// GradientEditor — a gradient with draggable colour stops + the ColorPicker for
// the selected stop + a direction combo + a remove button. The GradientBar
// paints a live preview and the stop markers; click a marker to select/drag,
// click the empty bar to add a stop. Header-only, no moc (std::function callbacks).

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QLinearGradient>
#include <QList>
#include <algorithm>
#include <functional>

#include "gallery-style.hpp"
#include "color-picker.hpp"
#include "mac-inputs.hpp"
#include "pill-button.hpp"
#include "glass.hpp"

namespace StreamUP {
namespace UIStyles {

namespace detail {
struct GradStop {
	qreal pos;
	QColor color;
};

class GradientBar : public QWidget {
public:
	std::function<void()> onChanged;          // stops changed (incl. selection move)
	std::function<void(const QColor &)> onSelected; // selection changed → sync picker

	explicit GradientBar(QWidget *parent = nullptr) : QWidget(parent)
	{
		setMinimumHeight(S(56));
		m_stops = {{0.0, QColor(Colors::PRIMARY_COLOR)}, {1.0, QColor(Colors::COLOR_DANGER)}};
	}

	QColor selectedColor() const { return m_stops[m_sel].color; }
	void setSelectedColor(const QColor &c)
	{
		m_stops[m_sel].color = c;
		update();
		if (onChanged)
			onChanged();
	}
	void removeSelected()
	{
		if (m_stops.size() <= 2)
			return;
		m_stops.removeAt(m_sel);
		m_sel = std::clamp(m_sel, 0, (int)m_stops.size() - 1);
		update();
		notifySel();
		if (onChanged)
			onChanged();
	}
	QList<GradStop> stops() const { return m_stops; }

protected:
	QRectF barRect() const { return QRectF(rect()).adjusted(0, 0, 0, -S(16)); }

	void mousePressEvent(QMouseEvent *e) override
	{
		const qreal x = e->position().x();
		// Hit-test markers first.
		for (int i = 0; i < m_stops.size(); ++i) {
			const qreal mx = m_stops[i].pos * width();
			if (std::abs(mx - x) <= S(7)) {
				m_sel = i;
				m_drag = true;
				update();
				notifySel();
				return;
			}
		}
		// Else add a stop at this position with the interpolated colour.
		const qreal pos = std::clamp(x / width(), 0.0, 1.0);
		m_stops.append({pos, colorAt(pos)});
		std::sort(m_stops.begin(), m_stops.end(),
			  [](const GradStop &a, const GradStop &b) { return a.pos < b.pos; });
		for (int i = 0; i < m_stops.size(); ++i)
			if (qFuzzyCompare(m_stops[i].pos, pos))
				m_sel = i;
		update();
		notifySel();
		if (onChanged)
			onChanged();
	}
	void mouseMoveEvent(QMouseEvent *e) override
	{
		if (!m_drag)
			return;
		m_stops[m_sel].pos = std::clamp(e->position().x() / width(), 0.0, 1.0);
		update();
		if (onChanged)
			onChanged();
	}
	void mouseReleaseEvent(QMouseEvent *) override { m_drag = false; }

	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF br = barRect();

		QLinearGradient g(br.topLeft(), br.topRight());
		for (const GradStop &s : m_stops)
			g.setColorAt(std::clamp(s.pos, 0.0, 1.0), s.color);
		QPainterPath path;
		path.addRoundedRect(br, S(8), S(8));
		p.fillPath(path, g);

		// Stop markers along the bottom strip.
		for (int i = 0; i < m_stops.size(); ++i) {
			const qreal mx = m_stops[i].pos * width();
			const QPointF c(mx, br.bottom() + S(9));
			p.setPen(QPen(QColor(i == m_sel ? Colors::PRIMARY_LIGHT : Colors::TEXT_MUTED),
				      i == m_sel ? 2.0 : 1.2));
			p.setBrush(m_stops[i].color);
			p.drawEllipse(c, S(5), S(5));
		}
	}

private:
	QColor colorAt(qreal pos) const
	{
		for (int i = 0; i + 1 < m_stops.size(); ++i) {
			if (pos >= m_stops[i].pos && pos <= m_stops[i + 1].pos) {
				const qreal t = (pos - m_stops[i].pos) /
						std::max(1e-6, m_stops[i + 1].pos - m_stops[i].pos);
				return interp(m_stops[i].color, m_stops[i + 1].color, t);
			}
		}
		return m_stops.last().color;
	}
	static QColor interp(const QColor &a, const QColor &b, qreal t)
	{
		return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
					a.greenF() + (b.greenF() - a.greenF()) * t,
					a.blueF() + (b.blueF() - a.blueF()) * t);
	}
	void notifySel()
	{
		if (onSelected)
			onSelected(m_stops[m_sel].color);
	}

	QList<GradStop> m_stops;
	int m_sel = 0;
	bool m_drag = false;
};
} // namespace detail

class GradientEditor : public QWidget {
public:
	explicit GradientEditor(QWidget *parent = nullptr) : QWidget(parent)
	{
		auto *v = new QVBoxLayout(this);
		v->setContentsMargins(0, 0, 0, 0);
		v->setSpacing(S(10));

		m_bar = new detail::GradientBar(this);
		v->addWidget(m_bar);

		auto *row = new QHBoxLayout();
		row->setSpacing(S(8));
		m_dir = new MacComboBox(this);
		m_dir->addItems({"Horizontal", "Vertical", "Diagonal"});
		m_dir->setOnCard(true);
		m_dir->setStyleSheet(comboStyle(true));
		m_dir->setFixedHeight(S(28));
		makeComboAnimated(m_dir); // opaque popup + slide-down open animation
		row->addWidget(m_dir, 1);
		auto *del = new PillButton("Remove stop", "outline", this);
		row->addWidget(del);
		v->addLayout(row);

		m_picker = new ColorPicker(this);
		v->addWidget(m_picker);

		m_bar->onSelected = [this](const QColor &c) { m_picker->setColor(c); };
		m_picker->onChanged = [this](const QColor &c) { m_bar->setSelectedColor(c); };
		QObject::connect(del, &QPushButton::clicked, this, [this]() { m_bar->removeSelected(); });
		m_picker->setColor(m_bar->selectedColor());
	}

private:
	detail::GradientBar *m_bar = nullptr;
	MacComboBox *m_dir = nullptr;
	ColorPicker *m_picker = nullptr;
};

} // namespace UIStyles
} // namespace StreamUP
