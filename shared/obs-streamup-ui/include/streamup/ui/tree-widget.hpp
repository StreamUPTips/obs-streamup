#pragma once

// TreeWidget — a QTreeWidget styled to the standard (treeStyle: lighter-blue
// rounded selection, hover, custom scrollbars) with **custom-painted branch
// chevrons** (Qt's default branch images don't match). drawBranches() is
// overridden to draw a chevron (▸ collapsed → ▾ expanded) for parent rows.

#include <QTreeWidget>
#include <QPainter>
#include <QModelIndex>

#include "gallery-style.hpp"
#include "mac-inputs.hpp"    // detail::drawChevron
#include "ui-scrollbar.hpp"  // useScrollBars
#include "item-delegate.hpp" // RowDelegate

namespace StreamUP {
namespace UIStyles {

class TreeWidget : public QTreeWidget {
public:
	explicit TreeWidget(QWidget *parent = nullptr) : QTreeWidget(parent)
	{
		setStyleSheet(treeStyle());
		setItemDelegate(new RowDelegate(this)); // content-only highlight, no indent tint
		setHeaderHidden(true);
		setIndentation(S(16));
		setAnimated(true);
		setRootIsDecorated(true);
		setExpandsOnDoubleClick(true);
		useScrollBars(this);
	}

protected:
	void drawBranches(QPainter *p, const QRect &rect, const QModelIndex &index) const override
	{
		// No base call → no default branch lines/arrows. We paint our own: faint
		// vertical indent guides (one per nesting level) so the hierarchy reads,
		// plus a chevron for rows that have children. The guides give the indent
		// gutter structure instead of leaving it as blank space.
		const int indent = std::max(1, indentation());
		int depth = 0;
		for (QModelIndex a = index.parent(); a.isValid(); a = a.parent())
			++depth;

		p->save();
		p->setRenderHint(QPainter::Antialiasing, true);

		// Indent guides for each ancestor column (centred in the column).
		QPen guide(QColor(255, 255, 255, 28));
		guide.setWidthF(1.0);
		p->setPen(guide);
		for (int j = 1; j <= depth; ++j) {
			const qreal x = rect.right() - (j + 0.5) * indent;
			p->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
		}

		// Chevron for parents, in the item's own (right-most) indent column.
		if (model()->hasChildren(index)) {
			QPen cp(QColor(Colors::TEXT_SECONDARY), std::max(1.3, S(2) * 0.8));
			cp.setCapStyle(Qt::RoundCap);
			cp.setJoinStyle(Qt::RoundJoin);
			p->setPen(cp);
			p->setBrush(Qt::NoBrush);
			p->save();
			p->translate(rect.right() - S(8), rect.center().y());
			if (!isExpanded(index))
				p->rotate(-90.0); // ▸ when collapsed
			detail::drawChevron(*p, QPointF(0, 0), S(7), /*up=*/false); // ▾ baseline
			p->restore();
		}
		p->restore();
	}
};

} // namespace UIStyles
} // namespace StreamUP
