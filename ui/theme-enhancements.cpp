#include "theme-enhancements.hpp"

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
#include <QStatusBar>
#include <QApplication>
#include <QComboBox>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QPainterPath>

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

// Status bar visibility filter
class StatusBarFilter : public QObject {
public:
    explicit StatusBarFilter(QMainWindow* mainWin, QObject* parent = nullptr)
        : QObject(parent), m_mainWindow(mainWin) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Show || event->type() == QEvent::Hide) {
            // Status bar visibility changed - update padding
            QWidget* statusBar = qobject_cast<QWidget*>(watched);
            if (statusBar && m_mainWindow) {
                bool visible = (event->type() == QEvent::Show);
                int bottomPadding = visible ? 0 : 9;
                m_mainWindow->setContentsMargins(9, 0, 9, bottomPadding);
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QMainWindow* m_mainWindow;
};

static StatusBarFilter* g_statusBarFilter = nullptr;

/**
 * @brief Corner overlay widget that creates the illusion of rounded corners
 *
 * This widget paints a quarter-circle cutout in the background color,
 * creating the appearance of rounded corners on the underlying widget.
 */
class CornerOverlay : public QWidget {
public:
    enum Corner { TopLeft, TopRight, BottomLeft, BottomRight };

    CornerOverlay(Corner corner, int radius, const QColor& bgColor, QWidget* parent = nullptr)
        : QWidget(parent), m_corner(corner), m_radius(radius), m_bgColor(bgColor)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFixedSize(radius, radius);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // Fill entire widget with background color
        painter.fillRect(rect(), m_bgColor);

        // Create path for the quarter-circle cutout
        QPainterPath path;
        path.addRect(rect());

        QPainterPath circlePath;
        QRectF circleRect;

        switch (m_corner) {
        case TopLeft:
            circleRect = QRectF(0, 0, m_radius * 2, m_radius * 2);
            break;
        case TopRight:
            circleRect = QRectF(-m_radius, 0, m_radius * 2, m_radius * 2);
            break;
        case BottomLeft:
            circleRect = QRectF(0, -m_radius, m_radius * 2, m_radius * 2);
            break;
        case BottomRight:
            circleRect = QRectF(-m_radius, -m_radius, m_radius * 2, m_radius * 2);
            break;
        }

        circlePath.addEllipse(circleRect);

        // Subtract the circle from the rectangle to create the cutout
        QPainterPath cutout = path.subtracted(circlePath);

        // Clear the area first, then draw the cutout shape
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        painter.fillPath(cutout, m_bgColor);
    }

private:
    Corner m_corner;
    int m_radius;
    QColor m_bgColor;
};

/**
 * @brief Event filter to reposition corner overlays on preview resize
 */
class PreviewCornerFilter : public QObject {
public:
    explicit PreviewCornerFilter(int radius, QObject* parent = nullptr)
        : QObject(parent), m_radius(radius) {}

    void repositionCorners(QWidget* preview)
    {
        if (!preview) return;

        // Corners are on the parent container, not the preview itself
        QWidget* container = preview->parentWidget();
        if (!container) return;

        QPoint previewPos = preview->pos();

        // Find corners by object name on the parent container
        for (QObject* child : container->children()) {
            QWidget* w = qobject_cast<QWidget*>(child);
            if (!w) continue;

            if (w->objectName() == "streamup_corner_tl") {
                w->move(previewPos.x(), previewPos.y());
                w->raise();
            } else if (w->objectName() == "streamup_corner_tr") {
                w->move(previewPos.x() + preview->width() - m_radius, previewPos.y());
                w->raise();
            }
        }
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
            QWidget* widget = qobject_cast<QWidget*>(watched);
            if (widget) {
                repositionCorners(widget);
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    int m_radius;
};

static PreviewCornerFilter* g_previewCornerFilter = nullptr;

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
        return false;
    }

    QString theme = QString::fromUtf8(themeId);
    g_isStreamUPTheme = theme.startsWith(STREAMUP_THEME_PREFIX);
    g_themeChecked = true;

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

    // Skip widgets in the context container (they should stay small)
    QWidget* parent = label->parentWidget();
    while (parent) {
        if (parent->objectName() == "contextContainer" ||
            parent->objectName() == "contextSubContainer") {
            return; // Don't apply pill styling to context bar widgets
        }
        parent = parent->parentWidget();
    }

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
    // Check if this is the Advanced Audio Properties dialog
    else if (widget->objectName() == "OBSAdvAudio" || className == "OBSBasicAdvAudio") {
        ApplyAdvAudioEnhancements(widget);
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
        }
    }

    statsWidget->setProperty("streamup_stats_enhanced", true);
}

/**
 * @brief Add padding around the main window edges
 *
 * This creates space between the docks and the window edges so
 * the rounded corners of the docks are fully visible.
 */
void ApplyMainWindowPadding(QMainWindow* mainWindow)
{
    if (!mainWindow) return;

    // Skip if already processed
    if (mainWindow->property("streamup_padding_applied").toBool()) {
        return;
    }

    // Find the status bar
    QStatusBar* statusBar = mainWindow->statusBar();
    bool statusBarVisible = statusBar && statusBar->isVisible();
    int bottomPadding = statusBarVisible ? 0 : 9;

    // Set content margins on the main window edges (9px to match dock gaps)
    mainWindow->setContentsMargins(9, 0, 9, bottomPadding);

    // Install event filter on status bar to watch for visibility changes
    if (statusBar && !g_statusBarFilter) {
        g_statusBarFilter = new StatusBarFilter(mainWindow);
        statusBar->installEventFilter(g_statusBarFilter);
    }

    // Adjust central widget layout spacing to match dock spacing
    QWidget* centralWidget = mainWindow->centralWidget();
    if (centralWidget) {
        QLayout* layout = centralWidget->layout();
        if (layout) {
            layout->setSpacing(7); // Gap between preview area and context bar
            layout->setContentsMargins(0, 0, 0, 0);
        }

        // Find canvasEditor and adjust its layout (previewLayout)
        QWidget* canvasEditor = centralWidget->findChild<QWidget*>("canvasEditor");
        if (canvasEditor) {
            QLayout* canvasLayout = canvasEditor->layout();
            if (canvasLayout) {
                canvasLayout->setSpacing(0);
                canvasLayout->setContentsMargins(0, 0, 0, 0);
            }
        }

        // Find previewContainer and adjust its layout
        QWidget* previewContainer = centralWidget->findChild<QWidget*>("previewContainer");
        if (previewContainer) {
            previewContainer->setContentsMargins(0, 0, 0, 0);
            QLayout* previewLayout = previewContainer->layout();
            if (previewLayout) {
                previewLayout->setSpacing(0);
                // Add padding for border visibility (left, top, right - no bottom)
                // Use 6px to make rounded corners more prominent
                previewLayout->setContentsMargins(6, 6, 6, 0);
            }

            // Find and adjust gridLayout inside previewContainer
            // Add padding to create a visible border effect with rounded corners
            QGridLayout* gridLayout = previewContainer->findChild<QGridLayout*>("gridLayout");
            if (gridLayout) {
                gridLayout->setSpacing(0);
                // Add padding around the preview to create border effect
                // Top: 6px to match outer 6px = 12px total (same as sides)
                gridLayout->setContentsMargins(6, 6, 6, 0);
            }

            // Find previewXContainer (zoom controls bar) and minimize its spacing
            QWidget* previewXContainer = previewContainer->findChild<QWidget*>("previewXContainer");
            if (previewXContainer) {
                previewXContainer->setContentsMargins(0, 0, 0, 0);
                QLayout* xLayout = previewXContainer->layout();
                if (xLayout) {
                    xLayout->setSpacing(0);
                    xLayout->setContentsMargins(0, 0, 0, 0);
                }
            }

            // Note: Corner overlays removed - they painted black on top of preview content.
            // Instead, relying on CSS border-radius on previewContainer with sufficient padding
            // to create the rounded corner appearance. The preview content has sharp corners
            // but the surrounding padding area shows the rounded container edges.
        }

    }

    mainWindow->setProperty("streamup_padding_applied", true);
}

void ApplyThemeEnhancements()
{
    // Only apply if StreamUP theme is active
    if (!IsUsingStreamUPTheme()) {
        return;
    }

    // Apply stats dock object names immediately
    ApplyStatsDockObjectNames();

    // Install application-wide event filter to catch window show events
    // This is more efficient than polling with a timer
    if (!g_appFilter) {
        g_appFilter = new AppWidgetFilter();
        QApplication::instance()->installEventFilter(g_appFilter);
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

        // Add padding around the main window edges
        ApplyMainWindowPadding(mainWindow);
    }
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

// ============================================================================
// Advanced Audio Properties Enhancements
// ============================================================================

// Global filter instance for monitoring comboboxes
static AdvAudioMonitoringFilter* g_advAudioFilter = nullptr;

/**
 * @brief Helper to update the monitoring type property on a combobox
 *
 * Sets a Qt property that CSS can target with selectors like:
 * QComboBox[monitoringType="0"] - Monitor Off
 * QComboBox[monitoringType="1"] - Monitor Only
 * QComboBox[monitoringType="2"] - Monitor and Output
 */
static void updateMonitoringTypeProperty(QComboBox* combo)
{
    if (!combo) return;

    int currentIndex = combo->currentIndex();
    QVariant data = combo->currentData();

    // Use the data value (monitoring type enum) as the property
    int monitoringType = data.isValid() ? data.toInt() : currentIndex;
    combo->setProperty("monitoringType", monitoringType);

    // Force style recalculation
    QString qss = combo->styleSheet();
    combo->setStyleSheet("/* */");
    combo->setStyleSheet(qss);
}

AdvAudioMonitoringFilter::AdvAudioMonitoringFilter(QObject* parent)
    : QObject(parent)
{
}

void AdvAudioMonitoringFilter::onMonitoringTypeChanged(int index)
{
    Q_UNUSED(index);

    // Get the combobox that sent the signal
    QComboBox* combo = qobject_cast<QComboBox*>(sender());
    if (combo) {
        updateMonitoringTypeProperty(combo);
    }
}

void ApplyAdvAudioEnhancements(QWidget* advAudioDialog)
{
    if (!advAudioDialog) return;

    // Only apply if StreamUP theme is active
    if (!IsUsingStreamUPTheme()) {
        return;
    }

    // Skip if already processed
    if (advAudioDialog->property("streamup_advadio_enhanced").toBool()) {
        return;
    }

    // Create filter if needed
    if (!g_advAudioFilter) {
        g_advAudioFilter = new AdvAudioMonitoringFilter();
    }

    // Find the main grid layout
    QGridLayout* mainLayout = nullptr;
    QList<QGridLayout*> grids = advAudioDialog->findChildren<QGridLayout*>();
    for (QGridLayout* grid : grids) {
        if (grid->objectName() == "mainLayout") {
            mainLayout = grid;
            break;
        }
    }

    // Find all QComboBox widgets in the dialog
    // The monitoring type comboboxes have 3 items (None, Monitor Only, Monitor and Output)
    QList<QComboBox*> combos = advAudioDialog->findChildren<QComboBox*>();

    for (QComboBox* combo : combos) {
        if (!combo) continue;

        // Check if this looks like a monitoring type combobox (3 items with specific data values)
        if (combo->count() == 3) {
            // Verify the data values match monitoring types (0, 1, 2)
            bool isMonitoringCombo = true;
            for (int i = 0; i < 3; i++) {
                QVariant data = combo->itemData(i);
                if (!data.isValid() || data.toInt() != i) {
                    isMonitoringCombo = false;
                    break;
                }
            }

            if (isMonitoringCombo) {
                // Set initial property value
                updateMonitoringTypeProperty(combo);

                // Connect to changes if not already connected
                if (!combo->property("streamup_monitoring_connected").toBool()) {
                    combo->setProperty("streamup_monitoring_connected", true);
                    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                                   g_advAudioFilter, &AdvAudioMonitoringFilter::onMonitoringTypeChanged);
                }
            }
        }
    }

    // Find and center status labels (they show "Active" or "Inactive" text)
    // These are QLabels with the text-danger class or specific text
    QList<QLabel*> labels = advAudioDialog->findChildren<QLabel*>();
    for (QLabel* label : labels) {
        if (!label) continue;

        QString text = label->text();
        QString classProperty = label->property("class").toString();

        // Check if this is a status label (has text-danger class or shows Active/Inactive)
        if (classProperty == "text-danger" || text == "Active" || text == "Inactive" ||
            text == QObject::tr("Active") || text == QObject::tr("Inactive")) {

            // Find parent layout and center the label
            QWidget* parent = label->parentWidget();
            while (parent) {
                QGridLayout* grid = qobject_cast<QGridLayout*>(parent->layout());
                if (grid) {
                    // Find the label in this layout and set alignment
                    int index = grid->indexOf(label);
                    if (index >= 0) {
                        int row, col, rowSpan, colSpan;
                        grid->getItemPosition(index, &row, &col, &rowSpan, &colSpan);
                        grid->setAlignment(label, Qt::AlignCenter);
                        break;
                    }
                }
                parent = parent->parentWidget();
            }
        }
    }

    advAudioDialog->setProperty("streamup_advadio_enhanced", true);
}

} // namespace ThemeEnhancements
} // namespace StreamUP
