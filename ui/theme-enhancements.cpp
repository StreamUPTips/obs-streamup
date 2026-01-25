#include "theme-enhancements.hpp"
#include "../utilities/debug-logger.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QMainWindow>
#include <QDockWidget>
#include <QDialog>
#include <QLabel>
#include <QFrame>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QApplication>

#include "moc_theme-enhancements.cpp"

namespace StreamUP {
namespace ThemeEnhancements {

// StreamUP theme prefix
static const char* STREAMUP_THEME_PREFIX = "com.streamup.";

// Border radius for color preview pills (matches StreamUP theme)
static const int COLOR_PREVIEW_RADIUS = 10;

// Global filter instances
static ColorPreviewFilter* g_colorFilter = nullptr;
static AppWidgetFilter* g_appFilter = nullptr;

// Cached theme check result (checked once at startup)
static bool g_isStreamUPTheme = false;
static bool g_themeChecked = false;

/**
 * @brief Check if the current OBS theme is a StreamUP theme
 * @return true if StreamUP or StreamUP variant theme is active
 *
 * Note: This caches the result at first successful check since
 * obs_frontend_get_user_config() may not be available after initialization.
 */
bool IsUsingStreamUPTheme()
{
    // Return cached result if already checked
    if (g_themeChecked) {
        return g_isStreamUPTheme;
    }

    config_t* config = obs_frontend_get_user_config();
    if (!config) {
        // Don't log warning every time - just return cached or false
        return g_isStreamUPTheme;
    }

    const char* themeId = config_get_string(config, "Appearance", "Theme");
    if (!themeId || !*themeId) {
        blog(LOG_DEBUG, "[StreamUP] Theme Enhancement: No theme ID found in config");
        return false;
    }

    QString theme = QString::fromUtf8(themeId);
    g_isStreamUPTheme = theme.startsWith(STREAMUP_THEME_PREFIX);
    g_themeChecked = true;

    blog(LOG_INFO, "[StreamUP] Theme Enhancement: Current theme: %s, IsStreamUP: %s",
         themeId, g_isStreamUPTheme ? "yes" : "no");

    return g_isStreamUPTheme;
}

// ============================================================================
// ColorPreviewFilter Implementation
// ============================================================================

ColorPreviewFilter::ColorPreviewFilter(QObject* parent)
    : QObject(parent)
{
}

bool ColorPreviewFilter::eventFilter(QObject* watched, QEvent* event)
{
    // Re-apply styling when the widget's style changes (e.g., color picker updates)
    if (event->type() == QEvent::StyleChange) {
        QWidget* widget = qobject_cast<QWidget*>(watched);
        if (widget) {
            // Reset the styled flag so we can re-apply with new color
            widget->setProperty("streamup_pill_styled", false);
            applyPillStyle(widget);
        }
    }

    return QObject::eventFilter(watched, event);
}

void ColorPreviewFilter::applyPillStyle(QWidget* widget)
{
    QLabel* label = qobject_cast<QLabel*>(widget);
    if (!label) return;

    // Check if this is a color preview label (has Panel|Sunken frame style)
    int frameStyle = label->frameStyle();
    bool hasPanel = (frameStyle & QFrame::Panel) != 0;
    bool hasSunken = (frameStyle & QFrame::Sunken) != 0;

    if (hasPanel && hasSunken) {
        // Only apply if not already styled by us
        if (!label->property("streamup_pill_styled").toBool()) {
            label->setProperty("streamup_pill_styled", true);

            // Set minimum width to fit hex color codes (#RRGGBB = 7 chars + padding)
            // Also remove max width constraint and ensure it can expand
            label->setMinimumWidth(100);
            label->setMaximumWidth(QWIDGETSIZE_MAX);

            // Ensure the size policy allows horizontal expansion
            QSizePolicy policy = label->sizePolicy();
            policy.setHorizontalPolicy(QSizePolicy::MinimumExpanding);
            label->setSizePolicy(policy);

            // Get current stylesheet and append border-radius and padding
            QString currentStyle = label->styleSheet();
            QString pillStyle = QString("border-radius: %1px; padding: 2px 10px; min-width: 80px;").arg(COLOR_PREVIEW_RADIUS);

            if (!currentStyle.isEmpty()) {
                // Append to existing style
                if (!currentStyle.endsWith(';')) {
                    currentStyle += ";";
                }
                label->setStyleSheet(currentStyle + " " + pillStyle);
            } else {
                label->setStyleSheet(pillStyle);
            }

            blog(LOG_DEBUG, "[StreamUP] Theme Enhancement: Applied pill style to color preview");
        }
    }
}

// ============================================================================
// AppWidgetFilter Implementation - Application-wide event filter
// ============================================================================

AppWidgetFilter::AppWidgetFilter(QObject* parent)
    : QObject(parent)
{
}

bool AppWidgetFilter::eventFilter(QObject* watched, QEvent* event)
{
    // Only process Show events
    if (event->type() != QEvent::Show) {
        return QObject::eventFilter(watched, event);
    }

    QWidget* widget = qobject_cast<QWidget*>(watched);
    if (!widget) {
        return QObject::eventFilter(watched, event);
    }

    // Check if this is a QLabel that could be a color preview
    QLabel* label = qobject_cast<QLabel*>(widget);
    if (label) {
        // Check if it's a color preview label (Panel|Sunken frame)
        int frameStyle = label->frameStyle();
        bool hasPanel = (frameStyle & QFrame::Panel) != 0;
        bool hasSunken = (frameStyle & QFrame::Sunken) != 0;

        if (hasPanel && hasSunken) {
            // Create filter if needed
            if (!g_colorFilter) {
                g_colorFilter = new ColorPreviewFilter();
            }

            // Apply pill styling
            g_colorFilter->applyPillStyle(label);

            // Install filter for future style changes
            if (!label->property("streamup_filter_installed").toBool()) {
                label->setProperty("streamup_filter_installed", true);
                label->installEventFilter(g_colorFilter);
            }
        }
    }
    // Check if this is OBSBasicStats (dock or window)
    QString className = QString::fromUtf8(widget->metaObject()->className());
    if (className == "OBSBasicStats") {
        ApplyStatsWindowEnhancements(widget);
    }
    // Also scan top-level windows when they're shown
    else if (widget->isWindow()) {
        ApplyColorPreviewStyling(widget);
    }

    return QObject::eventFilter(watched, event);
}

// ============================================================================
// Public API Implementation
// ============================================================================

void ApplyColorPreviewStyling(QWidget* container)
{
    if (!container) return;

    // Only apply if StreamUP theme is active
    if (!IsUsingStreamUPTheme()) {
        return;
    }

    // Create filter if needed
    if (!g_colorFilter) {
        g_colorFilter = new ColorPreviewFilter();
    }

    // Find ALL QLabel descendants (recursive search)
    QList<QLabel*> labels = container->findChildren<QLabel*>(QString(), Qt::FindChildrenRecursively);

    for (QLabel* label : labels) {
        if (!label) continue;

        // Check if this looks like a color preview label
        int frameStyle = label->frameStyle();
        bool hasPanel = (frameStyle & QFrame::Panel) != 0;
        bool hasSunken = (frameStyle & QFrame::Sunken) != 0;

        if (hasPanel && hasSunken) {
            // Apply pill styling
            g_colorFilter->applyPillStyle(label);

            // Install filter for future style changes (color updates)
            if (!label->property("streamup_filter_installed").toBool()) {
                label->setProperty("streamup_filter_installed", true);
                label->installEventFilter(g_colorFilter);
            }
        }
    }
}

void ApplyStatsDockObjectNames()
{
    // Only apply if StreamUP theme is active
    if (!IsUsingStreamUPTheme()) {
        return;
    }

    QMainWindow* mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (!mainWindow) {
        blog(LOG_WARNING, "[StreamUP] Theme Enhancement: Failed to get main window");
        return;
    }

    // Find the stats dock - it's created as "OBSBasicStats"
    QList<QWidget*> statsWidgets = mainWindow->findChildren<QWidget*>();

    for (QWidget* widget : statsWidgets) {
        if (!widget) continue;

        QString className = QString::fromUtf8(widget->metaObject()->className());

        // Find OBSBasicStats widget
        if (className == "OBSBasicStats") {
            // Set object name if not already set
            if (widget->objectName().isEmpty()) {
                widget->setObjectName("OBSBasicStats");
                blog(LOG_INFO, "[StreamUP] Theme Enhancement: Set object name on OBSBasicStats widget");
            }

            // Find child QGridLayout for output section and style its frames
            QList<QGridLayout*> grids = widget->findChildren<QGridLayout*>();
            for (QGridLayout* grid : grids) {
                if (grid->objectName().isEmpty()) {
                    grid->setObjectName("statsOutputGrid");
                }
            }

            // Find QFrame children and give them object names
            QList<QFrame*> frames = widget->findChildren<QFrame*>();
            int outputFrameIndex = 0;
            for (QFrame* frame : frames) {
                if (frame->objectName().isEmpty()) {
                    frame->setObjectName(QString("statsOutputFrame%1").arg(outputFrameIndex++));
                }
            }

            break;
        }
    }
}

/**
 * @brief Apply enhancements to the OBSBasicStats window
 *
 * - Centers the top stats grid horizontally
 * - Styles the output section with background color and rounded corners
 */
void ApplyStatsWindowEnhancements(QWidget* statsWidget)
{
    if (!statsWidget) return;

    // Skip if already processed
    if (statsWidget->property("streamup_stats_enhanced").toBool()) {
        return;
    }

    // Get the main layout of the stats widget
    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(statsWidget->layout());
    if (!mainLayout) {
        blog(LOG_DEBUG, "[StreamUP] Theme Enhancement: Stats widget has no VBoxLayout");
        return;
    }

    // Find and style the QScrollArea (output section)
    QList<QScrollArea*> scrollAreas = statsWidget->findChildren<QScrollArea*>();
    for (QScrollArea* scrollArea : scrollAreas) {
        // Add margins around the scroll area for spacing from edges
        scrollArea->setContentsMargins(12, 8, 12, 8);

        // Style the scroll area itself - use bg_primary (#161617)
        scrollArea->setStyleSheet(
            "QScrollArea {"
            "    background: #161617;"  // --bg_primary
            "    border: none;"
            "    border-radius: 12px;"  // more rounded
            "    margin: 0 8px;"        // horizontal margin
            "}"
        );

        // Style the viewport (inner widget container)
        if (scrollArea->viewport()) {
            scrollArea->viewport()->setStyleSheet(
                "background: #161617;"
                "border-radius: 12px;"
            );
        }

        // Style the content widget inside the scroll area
        if (scrollArea->widget()) {
            scrollArea->widget()->setStyleSheet(
                "background: #161617;"
                "border-radius: 12px;"
                "padding: 8px;"
            );
        }

        blog(LOG_INFO, "[StreamUP] Theme Enhancement: Styled stats output section");
    }

    // Center the top stats grid
    // The first item should be the top stats QGridLayout
    QLayoutItem* firstItem = mainLayout->itemAt(0);
    if (firstItem) {
        QGridLayout* topLayout = qobject_cast<QGridLayout*>(firstItem->layout());
        if (topLayout) {
            // Wrap the grid layout in a widget with horizontal centering
            mainLayout->removeItem(firstItem);

            // Create a container widget for the grid
            QWidget* gridContainer = new QWidget(statsWidget);
            QHBoxLayout* hLayout = new QHBoxLayout(gridContainer);
            hLayout->setContentsMargins(0, 0, 0, 0);

            // Create a widget to hold the grid
            QWidget* gridWidget = new QWidget(gridContainer);
            gridWidget->setLayout(topLayout);

            // Add spacers on both sides to center the grid
            hLayout->addStretch(1);
            hLayout->addWidget(gridWidget);
            hLayout->addStretch(1);

            // Insert the container at the beginning of main layout
            mainLayout->insertWidget(0, gridContainer);

            blog(LOG_INFO, "[StreamUP] Theme Enhancement: Centered stats grid layout");
        }
    }

    statsWidget->setProperty("streamup_stats_enhanced", true);
}

void ApplyThemeEnhancements()
{
    // Only apply if StreamUP theme is active
    if (!IsUsingStreamUPTheme()) {
        blog(LOG_INFO, "[StreamUP] Theme Enhancement: Skipping - StreamUP theme not active");
        return;
    }

    blog(LOG_INFO, "[StreamUP] Applying theme enhancements...");

    // Apply stats dock object names immediately
    ApplyStatsDockObjectNames();

    // Install application-wide event filter to catch window show events
    // This is more efficient than polling with a timer
    if (!g_appFilter) {
        g_appFilter = new AppWidgetFilter();
        QApplication::instance()->installEventFilter(g_appFilter);
        blog(LOG_INFO, "[StreamUP] Theme Enhancement: Installed application event filter");
    }

    // Apply to any currently open windows
    QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
    for (QWidget* widget : topLevelWidgets) {
        if (widget && widget->isVisible()) {
            ApplyColorPreviewStyling(widget);
        }
    }

    // Find and style any OBSBasicStats widgets (both dock and window versions)
    QMainWindow* mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (mainWindow) {
        QList<QWidget*> allWidgets = mainWindow->findChildren<QWidget*>();
        for (QWidget* widget : allWidgets) {
            QString className = QString::fromUtf8(widget->metaObject()->className());
            if (className == "OBSBasicStats") {
                ApplyStatsWindowEnhancements(widget);
            }
        }
    }

    blog(LOG_INFO, "[StreamUP] Theme Enhancement: Registered theme enhancements");
}

void RefreshThemeEnhancements()
{
    // Re-apply to all open windows
    if (!IsUsingStreamUPTheme()) {
        return;
    }

    QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
    for (QWidget* widget : topLevelWidgets) {
        if (widget && widget->isVisible()) {
            ApplyColorPreviewStyling(widget);
        }
    }
}

} // namespace ThemeEnhancements
} // namespace StreamUP
