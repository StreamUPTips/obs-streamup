// DraggableList — a QListWidget the user reorders by dragging, with a branded
// accent drop-indicator line (Qt's native indicator can't be recoloured by QSS).
// Internal move is enabled; the drop position is tracked in dragMoveEvent and an
// accent line is painted over the viewport in paintEvent.
#pragma once

#include <QListWidget>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QPainter>
#include <QPaintEvent>

#include "gallery-style.hpp"
#include "ui-scrollbar.hpp"
#include "item-delegate.hpp"

namespace StreamUP {
namespace UIStyles {

class DraggableList : public QListWidget {
public:
	explicit DraggableList(QWidget *parent = nullptr) : QListWidget(parent)
	{
		// Delegate paints the highlight (no focus-rect outline) → make the QSS
		// item backgrounds transparent so the view's own fill doesn't double up.
		setStyleSheet(listStyle() +
			      scale_qss("QListWidget::item:hover,QListWidget::item:selected{"
					"background:transparent;}"));
		setItemDelegate(new RowDelegate(this));
		setDragDropMode(QAbstractItemView::InternalMove);
		setDefaultDropAction(Qt::MoveAction);
		setSelectionMode(QAbstractItemView::SingleSelection);
		setDropIndicatorShown(false); // we draw our own
		useScrollBars(this);
	}

protected:
	void dragMoveEvent(QDragMoveEvent *e) override
	{
		QListWidget::dragMoveEvent(e);
		const QPoint pos = e->position().toPoint();
		const QModelIndex idx = indexAt(pos);
		QRect vr = idx.isValid() ? visualRect(idx)
					 : (count() ? visualRect(indexAt(QPoint(2, height() - 4)))
						    : viewport()->rect());
		m_dropY = (dropIndicatorPosition() == AboveItem) ? vr.top() : vr.bottom();
		m_showDrop = true;
		viewport()->update();
	}
	void dragLeaveEvent(QDragLeaveEvent *e) override
	{
		m_showDrop = false;
		viewport()->update();
		QListWidget::dragLeaveEvent(e);
	}
	void dropEvent(QDropEvent *e) override
	{
		m_showDrop = false;
		QListWidget::dropEvent(e);
		viewport()->update();
	}
	void paintEvent(QPaintEvent *e) override
	{
		QListWidget::paintEvent(e); // items (paints on the viewport)
		if (!m_showDrop)
			return;
		QPainter p(viewport());
		p.setRenderHint(QPainter::Antialiasing, true);
		QPen pen(QColor(Colors::PRIMARY_COLOR), S(2));
		pen.setCapStyle(Qt::RoundCap);
		p.setPen(pen);
		const int x0 = S(8), x1 = viewport()->width() - S(8);
		p.drawLine(x0, m_dropY, x1, m_dropY);
		p.setBrush(QColor(Colors::PRIMARY_COLOR));
		p.setPen(Qt::NoPen);
		p.drawEllipse(QPointF(x0, m_dropY), S(3), S(3));
	}

private:
	int m_dropY = 0;
	bool m_showDrop = false;
};

} // namespace UIStyles
} // namespace StreamUP
