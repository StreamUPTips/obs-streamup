#pragma once

// HotkeyField — click-to-record keyboard-shortcut input. Read-only line edit
// styled like a normal field; on focus it captures the next key chord and shows
// it (native text). Esc/Backspace clears. Header-only, no moc.

#include <QLineEdit>
#include <QKeyEvent>
#include <QKeySequence>
#include <QFocusEvent>

#include "gallery-style.hpp"

namespace StreamUP {
namespace UIStyles {

class HotkeyField : public QLineEdit {
public:
	explicit HotkeyField(QWidget *parent = nullptr) : QLineEdit(parent)
	{
		setReadOnly(true);
		setPlaceholderText("Click, then press keys…");
		setStyleSheet(lineEditStyle(true));
		setFixedHeight(S(28));
		setCursor(Qt::PointingHandCursor);
	}
	QKeySequence sequence() const { return m_seq; }

protected:
	void focusInEvent(QFocusEvent *e) override
	{
		QLineEdit::focusInEvent(e);
		setPlaceholderText("Press keys…");
	}
	void focusOutEvent(QFocusEvent *e) override
	{
		QLineEdit::focusOutEvent(e);
		setPlaceholderText("Click, then press keys…");
	}
	void keyPressEvent(QKeyEvent *e) override
	{
		const int k = e->key();
		if (k == Qt::Key_Escape || k == Qt::Key_Backspace || k == Qt::Key_Delete) {
			m_seq = QKeySequence();
			clear();
			return;
		}
		// Ignore lone modifiers — wait for a real key.
		if (k == Qt::Key_Control || k == Qt::Key_Shift || k == Qt::Key_Alt ||
		    k == Qt::Key_Meta || k == Qt::Key_unknown)
			return;
		m_seq = QKeySequence(e->keyCombination());
		setText(m_seq.toString(QKeySequence::NativeText));
	}

private:
	QKeySequence m_seq;
};

} // namespace UIStyles
} // namespace StreamUP
