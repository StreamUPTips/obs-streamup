#pragma once

// FlowLayout — a QLayout that flows items left-to-right and wraps to the next
// line when it runs out of width (for chip / tag / badge rows that must wrap).
// Canonical Qt FlowLayout pattern; spacing is scaled via S().

#include <QLayout>
#include <QWidgetItem>
#include <QStyle>
#include <QList>

#include "gallery-style.hpp"

namespace StreamUP {
namespace UIStyles {

class FlowLayout : public QLayout {
public:
	explicit FlowLayout(QWidget *parent = nullptr, int spacing = 8)
		: QLayout(parent), m_space(S(spacing))
	{
		setContentsMargins(0, 0, 0, 0);
	}
	~FlowLayout() override
	{
		QLayoutItem *item;
		while ((item = takeAt(0)))
			delete item;
	}

	void addItem(QLayoutItem *item) override { m_items.append(item); }
	int count() const override { return (int)m_items.size(); }
	QLayoutItem *itemAt(int i) const override { return m_items.value(i); }
	QLayoutItem *takeAt(int i) override
	{
		return (i >= 0 && i < m_items.size()) ? m_items.takeAt(i) : nullptr;
	}
	Qt::Orientations expandingDirections() const override { return {}; }
	bool hasHeightForWidth() const override { return true; }
	int heightForWidth(int w) const override { return doLayout(QRect(0, 0, w, 0), true); }

	void setGeometry(const QRect &rect) override
	{
		QLayout::setGeometry(rect);
		doLayout(rect, false);
	}
	QSize sizeHint() const override { return minimumSize(); }
	QSize minimumSize() const override
	{
		QSize s;
		for (QLayoutItem *item : m_items)
			s = s.expandedTo(item->minimumSize());
		const QMargins m = contentsMargins();
		return s + QSize(m.left() + m.right(), m.top() + m.bottom());
	}

private:
	int doLayout(const QRect &rect, bool testOnly) const
	{
		const QMargins m = contentsMargins();
		const QRect eff = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
		int x = eff.x(), y = eff.y(), lineH = 0;
		for (QLayoutItem *item : m_items) {
			const QSize sz = item->sizeHint();
			int next = x + sz.width() + m_space;
			if (next - m_space > eff.right() && lineH > 0) {
				x = eff.x();
				y += lineH + m_space;
				next = x + sz.width() + m_space;
				lineH = 0;
			}
			if (!testOnly)
				item->setGeometry(QRect(QPoint(x, y), sz));
			x = next;
			lineH = qMax(lineH, sz.height());
		}
		return y + lineH - rect.y() + m.bottom();
	}

	QList<QLayoutItem *> m_items;
	int m_space;
};

} // namespace UIStyles
} // namespace StreamUP
