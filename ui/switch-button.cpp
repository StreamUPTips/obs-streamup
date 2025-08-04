#include "switch-button.hpp"
#include "ui-styles.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QPropertyAnimation>
#include <QEasingCurve>

namespace StreamUP {
namespace UIStyles {

// SwitchButton implementation
SwitchButton::SwitchButton(QWidget* parent)
    : QWidget(parent)
    , m_checked(false)
    , m_offset(MARGIN)
    , m_animation(nullptr)
{
    setMinimumSize(SWITCH_WIDTH + 20, SWITCH_HEIGHT + 4); // Minimum size
    setCursor(Qt::PointingHandCursor);
    
    // Create animation for smooth toggle
    m_animation = new QPropertyAnimation(this, "offset");
    m_animation->setDuration(200);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
}

void SwitchButton::setChecked(bool checked)
{
    if (m_checked != checked) {
        bool wasFirstTime = (m_animation->state() == QPropertyAnimation::Stopped);
        m_checked = checked;
        
        int endOffset = checked ? (SWITCH_WIDTH - KNOB_SIZE - MARGIN) : MARGIN;
        
        if (wasFirstTime) {
            // Set initial position without animation
            m_offset = endOffset;
            update();
        } else {
            // Animate the change
            int startOffset = m_offset;
            m_animation->stop();
            m_animation->setStartValue(startOffset);
            m_animation->setEndValue(endOffset);
            m_animation->start();
        }
        
        emit toggled(checked);
    }
}

void SwitchButton::setText(const QString& text)
{
    m_text = text;
    
    // Calculate required width based on text
    if (!m_text.isEmpty()) {
        QFontMetrics fm(QFont("Arial", Sizes::FONT_SIZE_NORMAL));
        int textWidth = fm.horizontalAdvance(m_text) + 20; // Extra padding
        int totalWidth = textWidth + SWITCH_WIDTH + 20; // Text + switch + padding
        setFixedSize(totalWidth, SWITCH_HEIGHT + 4);
    } else {
        setFixedSize(SWITCH_WIDTH + 20, SWITCH_HEIGHT + 4);
    }
    
    update();
}

void SwitchButton::toggle()
{
    setChecked(!m_checked);
}

void SwitchButton::setOffset(int offset)
{
    m_offset = offset;
    update();
}

void SwitchButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw text label if present
    int textWidth = 0;
    if (!m_text.isEmpty()) {
        painter.setPen(QColor(Colors::TEXT_PRIMARY));
        painter.setFont(QFont("Arial", Sizes::FONT_SIZE_NORMAL));
        QFontMetrics fm(painter.font());
        textWidth = fm.horizontalAdvance(m_text) + 10;
        painter.drawText(QRect(0, 0, textWidth, height()), Qt::AlignLeft | Qt::AlignVCenter, m_text);
    }
    
    // Calculate switch position
    int switchX = textWidth;
    int switchY = (height() - SWITCH_HEIGHT) / 2;
    
    // Draw switch background track
    QRect trackRect(switchX, switchY, SWITCH_WIDTH, SWITCH_HEIGHT);
    painter.setPen(Qt::NoPen);
    
    if (m_checked) {
        painter.setBrush(QColor(Colors::SUCCESS));
    } else {
        painter.setBrush(QColor(Colors::BACKGROUND_INPUT));
    }
    
    painter.drawRoundedRect(trackRect, SWITCH_HEIGHT / 2, SWITCH_HEIGHT / 2);
    
    // Draw switch knob
    QRect knobRect(switchX + m_offset, switchY + MARGIN, KNOB_SIZE, KNOB_SIZE);
    painter.setBrush(QColor(Colors::TEXT_PRIMARY));
    painter.drawEllipse(knobRect);
}

void SwitchButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        toggle();
    }
    QWidget::mousePressEvent(event);
}

void SwitchButton::mouseReleaseEvent(QMouseEvent* event)
{
    QWidget::mouseReleaseEvent(event);
}

QSize SwitchButton::sizeHint() const
{
    if (!m_text.isEmpty()) {
        QFontMetrics fm(QFont("Arial", Sizes::FONT_SIZE_NORMAL));
        int textWidth = fm.horizontalAdvance(m_text) + 20; // Extra padding
        int totalWidth = textWidth + SWITCH_WIDTH + 20; // Text + switch + padding
        return QSize(totalWidth, SWITCH_HEIGHT + 4);
    } else {
        return QSize(SWITCH_WIDTH + 20, SWITCH_HEIGHT + 4);
    }
}

SwitchButton* CreateStyledSwitch(const QString& text, bool checked)
{
    SwitchButton* switchButton = new SwitchButton();
    switchButton->setText(text);
    switchButton->setChecked(checked);
    return switchButton;
}

} // namespace UIStyles
} // namespace StreamUP

#include "switch-button.moc"