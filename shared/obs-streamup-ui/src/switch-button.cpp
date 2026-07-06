#include "switch-button.hpp"
#include "gallery-style.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QPropertyAnimation>
#include <QEasingCurve>

namespace StreamUP {
namespace UIStyles {

// SwitchButton implementation
SwitchButton::SwitchButton(QWidget *parent)
	: QWidget(parent), m_checked(false), m_hovered(false), m_initializing(true), m_offset(MARGIN),
	  m_animation(nullptr)
{
	setMinimumSize(S(SWITCH_WIDTH + 20), S(SWITCH_HEIGHT + 4));
	setCursor(Qt::PointingHandCursor);

	m_animation = new QPropertyAnimation(this, "offset");
	m_animation->setDuration(200);
	m_animation->setEasingCurve(QEasingCurve::OutCubic);
}

void SwitchButton::setChecked(bool checked)
{
	bool stateChanged = (m_checked != checked);
	m_checked = checked;

	int endOffset = checked ? (SWITCH_WIDTH - KNOB_WIDTH - MARGIN) : MARGIN;

	if (m_initializing) {
		// Initial state set during construction: snap, do NOT emit toggled().
		// Emitting here fires handlers connected BEFORE this first setChecked
		// mid-construction (premature adjustSize, etc.). Genuine toggles after
		// init still emit (below).
		m_offset = endOffset;
		m_initializing = false;
		update();
	} else {
		if (stateChanged) {
			int startOffset = m_offset;
			m_animation->stop();
			m_animation->setStartValue(startOffset);
			m_animation->setEndValue(endOffset);
			m_animation->start();
			emit toggled(checked);
		} else {
			m_offset = endOffset;
			update();
		}
	}
}

void SwitchButton::setText(const QString &text)
{
	m_text = text;

	if (!m_text.isEmpty()) {
		QFontMetrics fm(font());
		int textWidth = fm.horizontalAdvance(m_text) + S(20);
		int totalWidth = textWidth + S(SWITCH_WIDTH + 20);
		setFixedSize(totalWidth, S(SWITCH_HEIGHT + 4));
	} else {
		setFixedSize(S(SWITCH_WIDTH + 20), S(SWITCH_HEIGHT + 4));
	}

	update();
}

void SwitchButton::toggle()
{
	m_initializing = false;
	setChecked(!m_checked);
}

void SwitchButton::setOffset(int offset)
{
	m_offset = offset;
	update();
}

void SwitchButton::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event)

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	const int switchW = S(SWITCH_WIDTH);
	const int switchH = S(SWITCH_HEIGHT);
	const int knobW = S(KNOB_WIDTH);
	const int knobH = S(KNOB_HEIGHT);
	const int margin = S(MARGIN);

	// Toggle on the LEFT, label to its right — so a column of switch rows all
	// line their toggles up regardless of label length.
	int switchX = 0;
	int switchY = (height() - switchH) / 2;

	if (!m_text.isEmpty()) {
		painter.setPen(palette().text().color());
		painter.setFont(font());
		const int textX = switchW + S(10);
		painter.drawText(QRect(textX, 0, width() - textX, height()), Qt::AlignLeft | Qt::AlignVCenter,
				 m_text);
	}

	QRect trackRect(switchX, switchY, switchW, switchH);
	painter.setPen(Qt::NoPen);
	painter.setRenderHint(QPainter::Antialiasing, true);

	// Route through the design tokens so a token change propagates here.
	const QColor offBase(Colors::SWITCH_TRACK_OFF); // #3A3A3D
	const QColor onBase(Colors::COLOR_SUCCESS);     // #65C466
	QColor trackColorOff = m_hovered ? offBase.lighter(110) : offBase;
	QColor trackColorOn = m_hovered ? onBase.lighter(110) : onBase;

	QLinearGradient gradient(trackRect.topLeft(), trackRect.bottomLeft());
	if (m_checked) {
		gradient.setColorAt(0.0, trackColorOn.lighter(108));
		gradient.setColorAt(1.0, trackColorOn.darker(105));
	} else {
		gradient.setColorAt(0.0, trackColorOff.lighter(110));
		gradient.setColorAt(1.0, trackColorOff.darker(108));
	}

	painter.setBrush(gradient);
	painter.drawRoundedRect(trackRect, switchH / 2, switchH / 2);

	QRect knobRect(switchX + S(m_offset), switchY + margin, knobW, knobH);

	painter.setBrush(QColor(0, 0, 0, 8));
	QRect shadowRect = knobRect.adjusted(0, 1, 0, 1);
	painter.drawRoundedRect(shadowRect, knobH / 2, knobH / 2);

	QLinearGradient knobGradient(knobRect.topLeft(), knobRect.bottomLeft());
	knobGradient.setColorAt(0.0, QColor("#ffffff"));
	knobGradient.setColorAt(1.0, QColor("#f8f9fa"));

	painter.setBrush(knobGradient);
	painter.drawRoundedRect(knobRect, knobH / 2, knobH / 2);
}

void SwitchButton::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
		toggle();
	QWidget::mousePressEvent(event);
}

void SwitchButton::mouseReleaseEvent(QMouseEvent *event)
{
	QWidget::mouseReleaseEvent(event);
}

void SwitchButton::enterEvent(QEnterEvent *event)
{
	m_hovered = true;
	update();
	QWidget::enterEvent(event);
}

void SwitchButton::leaveEvent(QEvent *event)
{
	m_hovered = false;
	update();
	QWidget::leaveEvent(event);
}

QSize SwitchButton::sizeHint() const
{
	if (!m_text.isEmpty()) {
		QFontMetrics fm(font());
		int textWidth = fm.horizontalAdvance(m_text) + S(20);
		int totalWidth = textWidth + S(SWITCH_WIDTH + 20);
		return QSize(totalWidth, S(SWITCH_HEIGHT + 4));
	} else {
		return QSize(S(SWITCH_WIDTH + 20), S(SWITCH_HEIGHT + 4));
	}
}

SwitchButton *CreateStyledSwitch(const QString &text, bool checked)
{
	SwitchButton *switchButton = new SwitchButton();
	switchButton->setText(text);
	switchButton->setChecked(checked);
	return switchButton;
}

} // namespace UIStyles
} // namespace StreamUP
