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

/**
 * @brief Apply padding and styling to the program display container in Studio Mode
 *
 * The programWidget is dynamically created when Studio Mode is enabled. This function
 * sets an object name and applies padding so the StreamUP theme's rounded corners
 * are visible on all four corners of the program display.
 */
void StyleProgramContainer();

/**
 * @brief Clean up studio mode enhancement resources
 *
 * Removes the frontend event callback. Call during plugin shutdown.
 */
void CleanupStudioModeEnhancements();

} // namespace StudioModeEnhancements
} // namespace StreamUP
