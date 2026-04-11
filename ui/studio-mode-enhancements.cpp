#include "studio-mode-enhancements.hpp"
#include "theme-enhancements.hpp"
#include "ui-styles.hpp"
#include "../utilities/debug-logger.hpp"
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <QTimer>
#include <QPalette>

namespace StreamUP {
namespace StudioModeEnhancements {

// Guard against timer callbacks firing after cleanup (issue #10)
static bool g_studioModeActive = false;

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
    Q_UNUSED(paintEvent);

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
        // Use the slider's own palette text colour so we follow the active OBS
        // theme instead of forcing a hardcoded StreamUP value.
        painter.setPen(QPen(slider->palette().color(QPalette::WindowText).lighter(), 2));
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
        // Use the slider's own palette text colour so we follow the active OBS
        // theme instead of forcing a hardcoded StreamUP value.
        painter.setPen(QPen(slider->palette().color(QPalette::WindowText).lighter(), 2));
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

    blog(LOG_INFO, "[StreamUP] Studio Mode Enhancement: Found %d sliders in main window", (int)sliders.size());

    for (QSlider* slider : sliders) {
        if (!slider) continue;

        QString objectName = slider->objectName();
        QString toolTip = slider->toolTip();

        // Log all sliders for debugging
        blog(LOG_INFO, "[StreamUP] Studio Mode Enhancement: Checking slider - objectName: '%s', tooltip: '%s'",
             objectName.toUtf8().constData(), toolTip.toUtf8().constData());

        // The transition slider is typically named "transitionDuration" or similar
        // Only install on transition-related sliders, not all horizontal sliders
        if (objectName.contains("transition", Qt::CaseInsensitive) ||
            objectName.contains("duration", Qt::CaseInsensitive) ||
            toolTip.contains("transition", Qt::CaseInsensitive)) {

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

void StyleProgramContainer()
{
    // Only apply if StreamUP theme is active
    if (!ThemeEnhancements::IsUsingStreamUPTheme()) {
        return;
    }

    QMainWindow* mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (!mainWindow) {
        return;
    }

    QWidget* centralWidget = mainWindow->centralWidget();
    if (!centralWidget) {
        return;
    }

    // Find canvasEditor which contains the previewLayout
    QWidget* canvasEditor = centralWidget->findChild<QWidget*>("canvasEditor");
    if (!canvasEditor) {
        return;
    }

    QLayout* previewLayout = canvasEditor->layout();
    if (!previewLayout) {
        return;
    }

    // Find previewContainer to get the previewXContainer height for reference
    QWidget* previewContainer = centralWidget->findChild<QWidget*>("previewContainer");
    int bottomBarHeight = 28; // Default height matching zoom controls bar

    if (previewContainer) {
        QWidget* previewXContainer = previewContainer->findChild<QWidget*>("previewXContainer");
        if (previewXContainer && previewXContainer->height() > 0) {
            bottomBarHeight = previewXContainer->height();
        }
    }

    // Iterate through previewLayout children to find programWidget
    // programWidget is dynamically created when Studio Mode is enabled
    for (int i = 0; i < previewLayout->count(); ++i) {
        QLayoutItem* item = previewLayout->itemAt(i);
        QWidget* widget = item ? item->widget() : nullptr;
        if (!widget) continue;

        // Skip known OBS widgets
        QString name = widget->objectName();
        if (name == "previewDisabledWidget" || name == "previewContainer") {
            continue;
        }

        // Skip if already processed
        if (name == "programContainer") {
            continue;
        }

        // Check if this is an unnamed widget with OBSQTDisplay child
        QString className = QString::fromUtf8(widget->metaObject()->className());
        if (className == "QWidget" && name.isEmpty()) {
            QList<QWidget*> children = widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
            bool hasProgramDisplay = false;
            for (QWidget* child : children) {
                QString childClass = QString::fromUtf8(child->metaObject()->className());
                if (childClass == "OBSQTDisplay") {
                    hasProgramDisplay = true;
                    break;
                }
            }

            if (hasProgramDisplay) {
                // Set object name for theme CSS styling
                widget->setObjectName("programContainer");

                QVBoxLayout* programLayout = qobject_cast<QVBoxLayout*>(widget->layout());
                if (programLayout) {
                    // Create the bottom bar widget
                    // Styling is handled by the StreamUP theme CSS rules for #programBottomBar
                    QFrame* bottomBar = new QFrame(widget);
                    bottomBar->setObjectName("programBottomBar");
                    bottomBar->setFixedHeight(bottomBarHeight);
                    bottomBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                    bottomBar->setFrameShape(QFrame::NoFrame);

                    programLayout->addWidget(bottomBar);

                    // Apply layout settings
                    programLayout->setSpacing(0);
                    programLayout->setContentsMargins(0, 0, 0, 0);

                    blog(LOG_INFO, "[StreamUP] Studio Mode: Added programBottomBar (height=%d)", bottomBarHeight);
                }

                blog(LOG_INFO, "[StreamUP] Studio Mode: Styled programContainer");
                break;
            }
        }
    }
}

static void OnStudioModeEvent(enum obs_frontend_event event, void *data)
{
    Q_UNUSED(data);

    if (event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED) {
        blog(LOG_INFO, "[StreamUP] Studio Mode enabled - applying enhancements with delay");
        // Use a timer to delay applying enhancements, giving OBS time to create the programWidget
        QTimer::singleShot(100, []() {
            if (!g_studioModeActive)
                return;
            blog(LOG_INFO, "[StreamUP] Studio Mode: Applying delayed enhancements");
            EnhanceTransitionSlider();
            StyleProgramContainer();
        });
    }
}

void ApplyStudioModeEnhancements()
{
    blog(LOG_INFO, "[StreamUP] Registering Studio Mode event handler...");
    g_studioModeActive = true;

    // Check if studio mode is already enabled
    if (obs_frontend_preview_program_mode_active()) {
        blog(LOG_INFO, "[StreamUP] Studio Mode already active - applying enhancements immediately");
        EnhanceTransitionSlider();
        StyleProgramContainer();
    }

    // Register event callback to apply enhancements when studio mode is enabled
    obs_frontend_add_event_callback(OnStudioModeEvent, nullptr);
}

void CleanupStudioModeEnhancements()
{
    g_studioModeActive = false;
    obs_frontend_remove_event_callback(OnStudioModeEvent, nullptr);
}

} // namespace StudioModeEnhancements
} // namespace StreamUP
