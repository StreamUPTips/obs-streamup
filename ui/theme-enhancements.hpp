#pragma once

#include <QObject>
#include <QWidget>
#include <QEvent>

class QMainWindow;

namespace StreamUP {
namespace ThemeEnhancements {

/**
 * @brief Event filter that applies pill styling to color preview labels
 *
 * This filter watches for color labels and applies rounded corners.
 */
class ColorPreviewFilter : public QObject {
    Q_OBJECT

public:
    explicit ColorPreviewFilter(QObject* parent = nullptr);
    void applyPillStyle(QWidget* widget);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

/**
 * @brief Application-wide event filter that catches widget show events
 *
 * This filter is installed on QApplication and watches for any widget
 * becoming visible. When a widget is shown, it scans for color preview
 * labels and applies styling. This is more efficient than polling.
 */
class AppWidgetFilter : public QObject {
    Q_OBJECT

public:
    explicit AppWidgetFilter(QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

/**
 * @brief Apply theme enhancements when StreamUP theme is active
 *
 * This function applies various enhancements that cannot be done
 * via CSS alone, including:
 * - Color preview pill shapes in Properties/Filters dialogs
 * - Stats dock output table object naming for CSS targeting
 */
void ApplyThemeEnhancements();

/**
 * @brief Apply color preview styling to all color labels in a dialog
 *
 * Finds QLabels used for color previews (with Panel|Sunken frame style)
 * and applies rounded corners to create a pill shape.
 */
void ApplyColorPreviewStyling(QWidget* dialog);

/**
 * @brief Apply object names to Stats dock widgets for CSS targeting
 *
 * The Stats dock output table cannot be styled via CSS because the
 * frame doesn't have an object name. This function sets appropriate
 * names so CSS selectors can target them.
 */
void ApplyStatsDockObjectNames();

/**
 * @brief Apply enhancements to the Stats window
 *
 * Centers the top stats grid horizontally and applies other styling.
 */
void ApplyStatsWindowEnhancements(QWidget* statsWidget);

/**
 * @brief Add padding around the main window edges
 *
 * Creates space between docks and window edges for visible rounded corners.
 */
void ApplyMainWindowPadding(QMainWindow* mainWindow);

/**
 * @brief Refresh theme enhancements
 *
 * Call when dialogs are opened or theme changes to reapply enhancements.
 */
void RefreshThemeEnhancements();

/**
 * @brief Check if the current OBS theme is a StreamUP theme
 */
bool IsUsingStreamUPTheme();

/**
 * @brief Apply enhancements to Advanced Audio Properties dialog
 *
 * Sets monitoring type property on comboboxes so CSS can show different icons
 * based on the monitoring mode (None, Monitor Only, Monitor and Output).
 */
void ApplyAdvAudioEnhancements(QWidget* advAudioDialog);

/**
 * @brief Event filter for monitoring combobox changes in Advanced Audio dialog
 *
 * Updates the monitoring type property when user changes the combobox,
 * allowing CSS to display the appropriate icon.
 */
class AdvAudioMonitoringFilter : public QObject {
    Q_OBJECT

public:
    explicit AdvAudioMonitoringFilter(QObject* parent = nullptr);

public slots:
    void onMonitoringTypeChanged(int index);
};

} // namespace ThemeEnhancements
} // namespace StreamUP
