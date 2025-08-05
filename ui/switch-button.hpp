#ifndef STREAMUP_SWITCH_BUTTON_HPP
#define STREAMUP_SWITCH_BUTTON_HPP

#include <QWidget>
#include <QPropertyAnimation>

class QPaintEvent;
class QMouseEvent;

namespace StreamUP {
namespace UIStyles {

// iOS-style switch widget
class SwitchButton : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int offset READ offset WRITE setOffset)

public:
    explicit SwitchButton(QWidget* parent = nullptr);
    ~SwitchButton() override = default;

    void setChecked(bool checked);
    bool isChecked() const { return m_checked; }
    
    void setText(const QString& text);
    QString text() const { return m_text; }

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    QSize sizeHint() const override;

private slots:
    void setOffset(int offset);

private:
    void toggle();
    int offset() const { return m_offset; }

    bool m_checked;
    bool m_hovered;
    bool m_initializing;
    QString m_text;
    int m_offset;
    QPropertyAnimation* m_animation;
    
    static constexpr int SWITCH_WIDTH = 54;   // Slightly narrower overall switch
    static constexpr int SWITCH_HEIGHT = 22;
    static constexpr int KNOB_WIDTH = 32;     // Keep knob same size (now bigger relative to track)
    static constexpr int KNOB_HEIGHT = 18;    // Shorter for pill shape
    static constexpr int MARGIN = 2;
};

// Utility function for creating styled switches
SwitchButton* CreateStyledSwitch(const QString& text = "", bool checked = false);

} // namespace UIStyles
} // namespace StreamUP

#endif // STREAMUP_SWITCH_BUTTON_HPP
