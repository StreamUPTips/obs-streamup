#pragma once

// SearchField — a line edit that live-filters a list/tree. Branded look:
// lineEditStyle() base + a painted leading magnifier glyph + a functional clear
// button. Wire textChanged() to your proxy filter / setRowHidden().

#include <QLineEdit>
#include <QPainter>
#include <QPaintEvent>

#include "gallery-style.hpp"

namespace StreamUP {
namespace UIStyles {

class SearchField : public QLineEdit {
public:
	explicit SearchField(QWidget *parent = nullptr) : QLineEdit(parent)
	{
		setPlaceholderText("Search…");
		setClearButtonEnabled(true);
		setOnCard(false);
	}
	void setOnCard(bool onCard)
	{
		setStyleSheet(lineEditStyle(onCard));
		setTextMargins(S(22), 0, 0, 0); // room for the leading glyph
	}

protected:
	void paintEvent(QPaintEvent *e) override
	{
		QLineEdit::paintEvent(e);
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		QPen pen(QColor(Colors::TEXT_MUTED), std::max(1.3, S(2) * 0.8));
		pen.setCapStyle(Qt::RoundCap);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		const qreal cx = S(11), cy = height() / 2.0, rr = Sf(3.5);
		p.drawEllipse(QPointF(cx - rr * 0.25, cy - rr * 0.25), rr, rr);
		p.drawLine(QPointF(cx + rr * 0.5, cy + rr * 0.5), QPointF(cx + rr * 1.3, cy + rr * 1.3));
	}
};

} // namespace UIStyles
} // namespace StreamUP
