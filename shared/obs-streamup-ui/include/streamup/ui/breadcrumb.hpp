#pragma once

// Breadcrumb — a horizontal "Scene › Group › Source" hierarchy trail. Segments
// are TEXT_SECONDARY, the last (current) one TEXT_PRIMARY/bold, separators a
// muted "›". Custom-painted so spacing + the chevron separators are crisp.

#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QStringList>
#include <QFontMetricsF>

#include "gallery-style.hpp"

namespace StreamUP {
namespace UIStyles {

class Breadcrumb : public QWidget {
public:
	explicit Breadcrumb(QWidget *parent = nullptr) : QWidget(parent)
	{
		setMinimumHeight(S(22));
	}
	void setPath(const QStringList &segments)
	{
		m_segments = segments;
		updateGeometry();
		update();
	}
	QSize sizeHint() const override
	{
		const QFontMetricsF fm(segFont());
		qreal w = 0;
		for (const QString &s : m_segments)
			w += fm.horizontalAdvance(s) + S(20);
		return QSize((int)w + S(8), S(22));
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QFont f = segFont();
		const QFontMetricsF fm(f);
		const qreal y = height() / 2.0 + fm.capHeight() / 2.0;
		qreal x = 0;
		for (int i = 0; i < m_segments.size(); ++i) {
			const bool last = (i == m_segments.size() - 1);
			QFont sf = f;
			sf.setWeight(last ? QFont::Weight(800) : QFont::Weight(500));
			p.setFont(sf);
			p.setPen(QColor(last ? Colors::TEXT_PRIMARY : Colors::TEXT_SECONDARY));
			p.drawText(QPointF(x, y), m_segments[i]);
			x += QFontMetricsF(sf).horizontalAdvance(m_segments[i]) + S(6);
			if (!last) {
				p.setPen(QColor(Colors::TEXT_MUTED));
				p.setFont(f);
				p.drawText(QPointF(x, y), QStringLiteral("›")); // ›
				x += fm.horizontalAdvance(QStringLiteral("›")) + S(6);
			}
		}
	}

private:
	QFont segFont() const
	{
		QFont f = font();
		f.setPixelSize(S(13));
		return f;
	}
	QStringList m_segments;
};

} // namespace UIStyles
} // namespace StreamUP
