#include "switch-button.hpp"
#include "ui-styles.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QPropertyAnimation>
#include <QEasingCurve>

namespace StreamUP {
namespace UIStyles {

// SwitchButton implementation
SwitchButton::SwitchButton(QWidget* parent)
    : QWidget(parent)
    , m_checked(false)
    , m_hovered(false)
    , m_initializing(true)
    , m_offset(MARGIN)
    , m_animation(nullptr)
{
    setMinimumSize(SWITCH_WIDTH + 20, SWITCH_HEIGHT + 4); // Minimum size with padding for shorter switch
    setCursor(Qt::PointingHandCursor);
    
    // Create animation for smooth toggle
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
        // Set initial position without animation
        m_offset = endOffset;
        m_initializing = false;  // Mark as no longer initializing
        update();
        
        // Only emit signal if state actually changed
        if (stateChanged) {
            emit toggled(checked);
        }
    } else {
        if (stateChanged) {
            // Animate the change
            int startOffset = m_offset;
            m_animation->stop();
            m_animation->setStartValue(startOffset);
            m_animation->setEndValue(endOffset);
            m_animation->start();
            
            emit toggled(checked);
        } else {
            // State didn't change, but force visual update in case of display issues
            m_offset = endOffset;
            update();
        }
    }
}

void SwitchButton::setText(const QString& text)
{
    m_text = text;
    
    // Calculate required width based on text using standard font
    if (!m_text.isEmpty()) {
        QFontMetrics fm(QFont("Arial", Sizes::FONT_SIZE_NORMAL, Sizes::FONT_WEIGHT_NORMAL));
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
    m_initializing = false;  // Ensure we're not in initializing mode when user toggles
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
    
    // Draw text label if present using standard font
    int textWidth = 0;
    if (!m_text.isEmpty()) {
        painter.setPen(palette().text().color()); // Use system text color
        painter.setFont(QFont("Arial", Sizes::FONT_SIZE_NORMAL, Sizes::FONT_WEIGHT_NORMAL));
        QFontMetrics fm(painter.font());
        textWidth = fm.horizontalAdvance(m_text) + 10;
        painter.drawText(QRect(0, 0, textWidth, height()), Qt::AlignLeft | Qt::AlignVCenter, m_text);
    }
    
    // Calculate switch position
    int switchX = textWidth;
    int switchY = (height() - SWITCH_HEIGHT) / 2;
    
    // Ultra-flat iOS 26 track design
    QRect trackRect(switchX, switchY, SWITCH_WIDTH, SWITCH_HEIGHT);
    painter.setPen(Qt::NoPen);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // Use StreamUP theme colors from toggle SVG files
    QColor trackColorOff = m_hovered ? QColor(58, 58, 61).lighter(110) : QColor(58, 58, 61);  // #3A3A3D from toggle-off.svg
    QColor trackColorOn = m_hovered ? QColor(101, 196, 102).lighter(110) : QColor(101, 196, 102);  // #65C466 from toggle-on.svg
    
    // Create subtle gradient for depth (matching other button styles)
    QLinearGradient gradient(trackRect.topLeft(), trackRect.bottomLeft());
    
    if (m_checked) {
        // Subtle gradient for ON state - slightly lighter at top
        gradient.setColorAt(0.0, trackColorOn.lighter(108));  // 8% lighter at top
        gradient.setColorAt(1.0, trackColorOn.darker(105));   // 5% darker at bottom
    } else {
        // Subtle gradient for OFF state - slightly lighter at top  
        gradient.setColorAt(0.0, trackColorOff.lighter(110)); // 10% lighter at top
        gradient.setColorAt(1.0, trackColorOff.darker(108));  // 8% darker at bottom
    }
    
    painter.setBrush(gradient);
    painter.drawRoundedRect(trackRect, SWITCH_HEIGHT / 2, SWITCH_HEIGHT / 2);
    
    // Draw ultra-minimal iOS 26 pill-shaped knob (wider than tall)
    QRect knobRect(switchX + m_offset, switchY + MARGIN, KNOB_WIDTH, KNOB_HEIGHT);
    
    // iOS 26 style - minimal shadow for edge definition
    painter.setBrush(QColor(0, 0, 0, 8));
    QRect shadowRect = knobRect.adjusted(0, 1, 0, 1);
    // Use height for corner radius to make it truly pill-shaped
    painter.drawRoundedRect(shadowRect, KNOB_HEIGHT / 2, KNOB_HEIGHT / 2);
    
    // Draw the main knob with subtle gradient to match other buttons
    QLinearGradient knobGradient(knobRect.topLeft(), knobRect.bottomLeft());
    knobGradient.setColorAt(0.0, QColor("#ffffff"));        // Pure white at top
    knobGradient.setColorAt(1.0, QColor("#f8f9fa"));        // Very slightly darker at bottom
    
    painter.setBrush(knobGradient);
    // Corner radius based on height to create proper pill shape
    painter.drawRoundedRect(knobRect, KNOB_HEIGHT / 2, KNOB_HEIGHT / 2);
    
    // No inner shadows or additional effects for iOS 26 flat design
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

void SwitchButton::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void SwitchButton::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

QSize SwitchButton::sizeHint() const
{
    if (!m_text.isEmpty()) {
        QFontMetrics fm(QFont("Arial", Sizes::FONT_SIZE_NORMAL, Sizes::FONT_WEIGHT_NORMAL));
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
