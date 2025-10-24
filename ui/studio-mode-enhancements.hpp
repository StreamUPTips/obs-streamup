#pragma once

#include <QObject>
#include <QSlider>
#include <QPaintEvent>
#include <QPainter>
#include <QEvent>

namespace StreamUP {
namespace StudioModeEnhancements {

/**
 * @brief Event filter that draws a midpoint marker on a slider
 */
class SliderMidpointFilter : public QObject {
    Q_OBJECT

public:
    explicit SliderMidpointFilter(QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void drawMidpointMarker(QSlider* slider, QPaintEvent* paintEvent);
};

/**
 * @brief Apply studio mode enhancements to OBS's transition slider
 */
void ApplyStudioModeEnhancements();

/**
 * @brief Find and enhance the transition slider in OBS main window
 */
void EnhanceTransitionSlider();

} // namespace StudioModeEnhancements
} // namespace StreamUP
