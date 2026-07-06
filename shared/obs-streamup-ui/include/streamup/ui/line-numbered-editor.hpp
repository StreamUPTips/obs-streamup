#pragma once

// LineNumberedEditor — QPlainTextEdit with a line-number gutter (the protext
// code-editor pattern). Gutter bg = INPUT_ON_CARD, numbers TEXT_MUTED, current
// line highlighted with the accent at low alpha. Header-only, no moc (existing
// QPlainTextEdit signals are connected to lambdas; the gutter forwards its paint
// through a std::function so there's no circular class dependency).

#include <QPlainTextEdit>
#include <QTextEdit>
#include <QTextBlock>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QWidget>
#include <functional>

#include "gallery-style.hpp"

namespace StreamUP {
namespace UIStyles {

namespace detail {
class GutterArea : public QWidget {
public:
	std::function<void(QPaintEvent *)> onPaint;
	using QWidget::QWidget;

protected:
	void paintEvent(QPaintEvent *e) override
	{
		if (onPaint)
			onPaint(e);
	}
};
} // namespace detail

class LineNumberedEditor : public QPlainTextEdit {
public:
	explicit LineNumberedEditor(QWidget *parent = nullptr) : QPlainTextEdit(parent)
	{
		m_gutter = new detail::GutterArea(this);
		m_gutter->onPaint = [this](QPaintEvent *e) { paintGutter(e); };

		QObject::connect(this, &QPlainTextEdit::blockCountChanged, this,
				 [this](int) { updateGutterWidth(); });
		QObject::connect(this, &QPlainTextEdit::updateRequest, this,
				 [this](const QRect &r, int dy) { updateGutter(r, dy); });
		QObject::connect(this, &QPlainTextEdit::cursorPositionChanged, this,
				 [this]() { highlightCurrentLine(); });

		setStyleSheet(plainTextStyle(true));
		setFrameShape(QFrame::NoFrame);
		updateGutterWidth();
		highlightCurrentLine();
	}

	int gutterWidth() const
	{
		int digits = 1;
		for (int max = std::max(1, blockCount()); max >= 10; max /= 10)
			++digits;
		return S(14) + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
	}

protected:
	void resizeEvent(QResizeEvent *e) override
	{
		QPlainTextEdit::resizeEvent(e);
		const QRect cr = contentsRect();
		m_gutter->setGeometry(cr.left(), cr.top(), gutterWidth(), cr.height());
	}

private:
	void updateGutterWidth() { setViewportMargins(gutterWidth(), 0, 0, 0); }
	void updateGutter(const QRect &r, int dy)
	{
		if (dy)
			m_gutter->scroll(0, dy);
		else
			m_gutter->update(0, r.y(), m_gutter->width(), r.height());
		if (r.contains(viewport()->rect()))
			updateGutterWidth();
	}
	void highlightCurrentLine()
	{
		QList<QTextEdit::ExtraSelection> sels;
		QTextEdit::ExtraSelection sel;
		QColor line(Colors::PRIMARY_COLOR);
		line.setAlphaF(0.12f);
		sel.format.setBackground(line);
		sel.format.setProperty(QTextFormat::FullWidthSelection, true);
		sel.cursor = textCursor();
		sel.cursor.clearSelection();
		sels.append(sel);
		setExtraSelections(sels);
	}
	void paintGutter(QPaintEvent *e)
	{
		QPainter p(m_gutter);
		p.fillRect(e->rect(), QColor(Colors::INPUT_ON_CARD));

		QTextBlock block = firstVisibleBlock();
		int num = block.blockNumber();
		int top = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
		int bottom = top + (int)blockBoundingRect(block).height();
		const int cur = textCursor().blockNumber();
		QFont f = font();
		p.setFont(f);
		while (block.isValid() && top <= e->rect().bottom()) {
			if (block.isVisible() && bottom >= e->rect().top()) {
				p.setPen(QColor(num == cur ? Colors::TEXT_SECONDARY : Colors::TEXT_MUTED));
				p.drawText(0, top, m_gutter->width() - S(6), fontMetrics().height(),
					   Qt::AlignRight | Qt::AlignVCenter, QString::number(num + 1));
			}
			block = block.next();
			top = bottom;
			bottom = top + (int)blockBoundingRect(block).height();
			++num;
		}
	}

	detail::GutterArea *m_gutter = nullptr;
};

} // namespace UIStyles
} // namespace StreamUP
