#include "studio-mode-enhancements.hpp"
#include "../utilities/debug-logger.hpp"
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QApplication>
#include <QWidget>

namespace StreamUP {
namespace StudioModeEnhancements {

SliderMidpointFilter::SliderMidpointFilter(QObject* parent)
    : QObject(parent)
{
}

bool SliderMidpointFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Paint) {
        QSlider* slider = qobject_cast<QSlider*>(watched);
        if (slider) {
            QPaintEvent* paintEvent = static_cast<QPaintEvent*>(event);
            drawMidpointMarker(slider, paintEvent);
        }
    }

    // Always pass the event to the base class
    return QObject::eventFilter(watched, event);
}

void SliderMidpointFilter::drawMidpointMarker(QSlider* slider, QPaintEvent* paintEvent)
{
    // Let the slider paint itself first
    QSlider temp(slider->orientation());
    temp.setMinimum(slider->minimum());
    temp.setMaximum(slider->maximum());
    temp.setValue(slider->value());

    // Now draw our custom marker on top
    QPainter painter(slider);
    painter.setRenderHint(QPainter::Antialiasing);

    // Get the slider's style option to calculate positions
    QStyleOptionSlider opt;
    opt.initFrom(slider);
    opt.minimum = slider->minimum();
    opt.maximum = slider->maximum();
    opt.sliderPosition = slider->value();
    opt.sliderValue = slider->value();
    opt.orientation = slider->orientation();
    opt.tickPosition = slider->tickPosition();
    opt.tickInterval = slider->tickInterval();

    // Calculate the midpoint position
    int midValue = (slider->minimum() + slider->maximum()) / 2;

    // Get the groove rect where the slider track is drawn
    QRect grooveRect = slider->style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, slider);

    int midPosition;
    if (slider->orientation() == Qt::Horizontal) {
        // Calculate horizontal midpoint position
        int range = slider->maximum() - slider->minimum();
        if (range > 0) {
            double ratio = (double)(midValue - slider->minimum()) / range;
            midPosition = grooveRect.left() + (int)(grooveRect.width() * ratio);
        } else {
            midPosition = grooveRect.center().x();
        }

        // Draw vertical line at midpoint
        painter.setPen(QPen(QColor(255, 255, 255, 200), 2));
        painter.drawLine(midPosition, grooveRect.top(), midPosition, grooveRect.bottom());
    } else {
        // Calculate vertical midpoint position
        int range = slider->maximum() - slider->minimum();
        if (range > 0) {
            double ratio = (double)(midValue - slider->minimum()) / range;
            midPosition = grooveRect.top() + (int)(grooveRect.height() * ratio);
        } else {
            midPosition = grooveRect.center().y();
        }

        // Draw horizontal line at midpoint
        painter.setPen(QPen(QColor(255, 255, 255, 200), 2));
        painter.drawLine(grooveRect.left(), midPosition, grooveRect.right(), midPosition);
    }
}

void EnhanceTransitionSlider()
{
    QMainWindow* mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (!mainWindow) {
        blog(LOG_WARNING, "[StreamUP] Studio Mode Enhancement: Failed to get main window");
        return;
    }

    // Try to find the transition slider by searching for QSlider widgets
    QList<QSlider*> sliders = mainWindow->findChildren<QSlider*>();

    blog(LOG_INFO, "[StreamUP] Studio Mode Enhancement: Found %d sliders in main window", sliders.size());

    for (QSlider* slider : sliders) {
        if (!slider) continue;

        QString objectName = slider->objectName();
        QString toolTip = slider->toolTip();

        // Log all sliders for debugging
        blog(LOG_INFO, "[StreamUP] Studio Mode Enhancement: Checking slider - objectName: '%s', tooltip: '%s'",
             objectName.toUtf8().constData(), toolTip.toUtf8().constData());

        // The transition slider is typically named "transitionDuration" or similar
        // Also check if it's horizontal and in a reasonable range
        if (objectName.contains("transition", Qt::CaseInsensitive) ||
            objectName.contains("duration", Qt::CaseInsensitive) ||
            toolTip.contains("transition", Qt::CaseInsensitive) ||
            slider->orientation() == Qt::Horizontal) {

            blog(LOG_INFO, "[StreamUP] Studio Mode Enhancement: Installing event filter on slider - objectName: '%s'",
                 objectName.toUtf8().constData());

            // Install our custom event filter to draw the midpoint marker
            SliderMidpointFilter* filter = new SliderMidpointFilter(slider);
            slider->installEventFilter(filter);

            // Force the slider to repaint so the marker appears
            slider->update();

            blog(LOG_INFO, "[StreamUP] Studio Mode Enhancement: Successfully installed midpoint marker on transition slider");
        }
    }
}

static void OnStudioModeEvent(enum obs_frontend_event event, void *data)
{
    UNUSED_PARAMETER(data);

    if (event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED) {
        blog(LOG_INFO, "[StreamUP] Studio Mode enabled - applying transition slider enhancements");
        EnhanceTransitionSlider();
    }
}

void ApplyStudioModeEnhancements()
{
    blog(LOG_INFO, "[StreamUP] Registering Studio Mode event handler...");

    // Check if studio mode is already enabled
    if (obs_frontend_preview_program_mode_active()) {
        blog(LOG_INFO, "[StreamUP] Studio Mode already active - applying enhancements immediately");
        EnhanceTransitionSlider();
    }

    // Register event callback to apply enhancements when studio mode is enabled
    obs_frontend_add_event_callback(OnStudioModeEvent, nullptr);
}

} // namespace StudioModeEnhancements
} // namespace StreamUP
