#pragma once

// Canonical StreamUP toggle (decision #3) — copied verbatim from
// obs-streamup/ui/switch-button.hpp. Do NOT diverge: 54x22 track, pill knob
// 32x18, 200ms OutCubic, off #3A3A3D / on #65C466.

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
	explicit SwitchButton(QWidget *parent = nullptr);
	~SwitchButton() override = default;

	void setChecked(bool checked);
	bool isChecked() const { return m_checked; }

	void setText(const QString &text);
	QString text() const { return m_text; }

signals:
	void toggled(bool checked);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
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
	QPropertyAnimation *m_animation;

	static constexpr int SWITCH_WIDTH = 54;
	static constexpr int SWITCH_HEIGHT = 22;
	static constexpr int KNOB_WIDTH = 32;
	static constexpr int KNOB_HEIGHT = 18;
	static constexpr int MARGIN = 2;
};

// Utility function for creating styled switches
SwitchButton *CreateStyledSwitch(const QString &text = "", bool checked = false);

} // namespace UIStyles
} // namespace StreamUP
