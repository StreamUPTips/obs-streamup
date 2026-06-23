#pragma once

// RowDelegate — custom item painter for trees/lists. It draws the hover/selection
// highlight as a rounded rect over the item's CONTENT cell only (never the indent
// column) and draws the text itself, so:
//   • the tree no longer tints the indent/expand column (Qt does that even with
//     show-decoration-selected:0), and
//   • lists no longer show the dotted/solid focus-rectangle outline.
// Pair it with a stylesheet whose ::item:hover / ::item:selected backgrounds are
// transparent, so the view's own row fill doesn't double up under the delegate.

#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QPainter>
#include <QModelIndex>
#include <algorithm>

#include "gallery-style.hpp"

namespace StreamUP {
namespace UIStyles {

class RowDelegate : public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;

	QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &idx) const override
	{
		QSize s = QStyledItemDelegate::sizeHint(opt, idx);
		return QSize(s.width(), std::max(s.height(), S(28)));
	}

	void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override
	{
		QStyleOptionViewItem o(opt);
		initStyleOption(&o, idx);

		p->save();
		p->setRenderHint(QPainter::Antialiasing, true);

		const bool sel = o.state.testFlag(QStyle::State_Selected);
		const bool hov = o.state.testFlag(QStyle::State_MouseOver);
		const QRectF hl = QRectF(o.rect).adjusted(S(2), S(1), -S(4), -S(1));
		if (sel || hov) {
			p->setPen(Qt::NoPen);
			p->setBrush(sel ? QColor(41, 151, 255, 140)   // SELECTION (PRIMARY_LIGHT @ .55)
					: QColor(255, 255, 255, 13)); // HOVER_ROW (@ .05)
			p->drawRoundedRect(hl, S(6), S(6));
		}

		p->setPen(QColor(sel ? "#ffffff" : Colors::TEXT_PRIMARY));
		p->setFont(o.font);
		const QRectF tr = QRectF(o.rect).adjusted(S(10), 0, -S(8), 0);
		const QString txt = idx.data(Qt::DisplayRole).toString();
		p->drawText(tr, Qt::AlignVCenter | Qt::AlignLeft,
			    o.fontMetrics.elidedText(txt, Qt::ElideRight, (int)tr.width()));
		p->restore();
	}
};

} // namespace UIStyles
} // namespace StreamUP
