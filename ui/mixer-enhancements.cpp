#include "mixer-enhancements.hpp"
#include "../utilities/debug-logger.hpp"

#include <obs.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QMainWindow>
#include <QDockWidget>
#include <QFrame>
#include <QLabel>
#include <QAbstractButton>
#include <QBoxLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QTimer>
#include <QApplication>
#include <QPointer>
#include <QToolTip>
#include <QSizePolicy>
#include <QToolBar>
#include <QPushButton>

#include <utility>

#include "moc_mixer-enhancements.cpp"

namespace StreamUP {
namespace MixerEnhancements {

// StreamUP theme prefix
static const char* STREAMUP_THEME_PREFIX = "com.streamup.";

// Global filter instance
static MixerEventFilter* g_mixerFilter = nullptr;

// Cached theme check
static bool g_isStreamUPTheme = false;
static bool g_themeChecked = false;

// Track if we've set up the mixer dock watcher
static bool g_mixerWatcherInstalled = false;

/**
 * @brief Check if the current OBS theme is a StreamUP theme
 */
bool IsUsingStreamUPTheme()
{
    if (g_themeChecked) {
        return g_isStreamUPTheme;
    }

    config_t* config = obs_frontend_get_user_config();
    if (!config) {
        return g_isStreamUPTheme;
    }

    const char* themeId = config_get_string(config, "Appearance", "Theme");
    if (!themeId || !*themeId) {
        return false;
    }

    QString theme = QString::fromUtf8(themeId);
    g_isStreamUPTheme = theme.startsWith(STREAMUP_THEME_PREFIX);
    g_themeChecked = true;

    blog(LOG_INFO, "[StreamUP] Mixer Enhancement: Theme check - %s, IsStreamUP: %s",
         themeId, g_isStreamUPTheme ? "yes" : "no");

    return g_isStreamUPTheme;
}

// ============================================================================
// MixerEventFilter Implementation
// ============================================================================

MixerEventFilter::MixerEventFilter(QObject* parent)
    : QObject(parent)
{
}

bool MixerEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    // Watch for layout changes and child additions in the mixer
    if (event->type() == QEvent::LayoutRequest ||
        event->type() == QEvent::ChildAdded ||
        event->type() == QEvent::Show) {

        // Delay the refresh slightly to let OBS finish its layout
        QTimer::singleShot(50, []() {
            RefreshMixerEnhancements();
        });
    }

    return QObject::eventFilter(watched, event);
}

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Find the mixer dock widget
 */
static QWidget* FindMixerDock()
{
    QMainWindow* mainWindow = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (!mainWindow) {
        return nullptr;
    }

    QDockWidget* mixerDock = mainWindow->findChild<QDockWidget*>("mixerDock");
    if (!mixerDock) {
        return nullptr;
    }

    return mixerDock->widget();
}

/**
 * @brief Find all VolumeControl widgets in the mixer
 */
static QList<QWidget*> FindVolumeControls(QWidget* mixerWidget)
{
    QList<QWidget*> controls;
    if (!mixerWidget) {
        return controls;
    }

    // VolumeControl widgets are QFrames that contain a child with objectName "volLabel"
    QList<QFrame*> frames = mixerWidget->findChildren<QFrame*>();
    for (QFrame* frame : frames) {
        QLabel* volLabel = frame->findChild<QLabel*>("volLabel");
        if (volLabel) {
            controls.append(frame);
        }
    }

    return controls;
}

/**
 * @brief Center the dB label in a VolumeControl
 */
static void CenterVolumeLabel(QWidget* volumeControl)
{
    QLabel* volLabel = volumeControl->findChild<QLabel*>("volLabel");
    if (!volLabel) {
        blog(LOG_DEBUG, "[StreamUP] Mixer Enhancement: volLabel not found in control");
        return;
    }

    // Skip if already processed
    if (volLabel->property("streamup_centered").toBool()) {
        return;
    }

    // Set center alignment on the label itself
    volLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    // Force the label to expand horizontally to fill its container
    // This is key - without expanding, the label stays at its minimum size
    QSizePolicy policy = volLabel->sizePolicy();
    policy.setHorizontalPolicy(QSizePolicy::Expanding);
    policy.setHorizontalStretch(1);
    volLabel->setSizePolicy(policy);

    // Remove any maximum width constraint
    volLabel->setMaximumWidth(QWIDGETSIZE_MAX);

    volLabel->setProperty("streamup_centered", true);

    blog(LOG_INFO, "[StreamUP] Mixer Enhancement: Centered volume label '%s'",
         volLabel->text().toUtf8().constData());
}

/**
 * @brief Center all content within a VolumeControl horizontally
 */
static void CenterVolumeControlContent(QWidget* volumeControl)
{
    if (!volumeControl) return;

    // Skip if already processed
    if (volumeControl->property("streamup_content_centered").toBool()) {
        return;
    }

    // Get the main layout of the VolumeControl
    QLayout* mainLayout = volumeControl->layout();
    if (!mainLayout) return;

    QBoxLayout* boxLayout = qobject_cast<QBoxLayout*>(mainLayout);
    if (boxLayout) {
        // Center all items in the layout
        boxLayout->setAlignment(Qt::AlignHCenter);

        // Iterate through layout items and center each one
        for (int i = 0; i < boxLayout->count(); ++i) {
            QLayoutItem* item = boxLayout->itemAt(i);
            if (item && item->widget()) {
                boxLayout->setAlignment(item->widget(), Qt::AlignHCenter);

                // If the widget has its own layout, center that too
                QLayout* childLayout = item->widget()->layout();
                if (childLayout) {
                    QBoxLayout* childBox = qobject_cast<QBoxLayout*>(childLayout);
                    if (childBox) {
                        childBox->setAlignment(Qt::AlignHCenter);
                    }
                }
            }
        }
    }

    // Find and center the volMeterFrame specifically
    QFrame* meterFrame = volumeControl->findChild<QFrame*>("volMeterFrame");
    if (meterFrame) {
        QLayout* meterLayout = meterFrame->layout();
        if (meterLayout) {
            // Add left margin to shift meter+slider to the right (center them)
            QMargins margins = meterLayout->contentsMargins();
            meterLayout->setContentsMargins(14, margins.top(), margins.right(), margins.bottom());

            QBoxLayout* meterBox = qobject_cast<QBoxLayout*>(meterLayout);
            if (meterBox) {
                meterBox->setAlignment(Qt::AlignHCenter);
            }
        }
        blog(LOG_DEBUG, "[StreamUP] Mixer Enhancement: Centered volMeterFrame");
    }

    volumeControl->setProperty("streamup_content_centered", true);
    blog(LOG_DEBUG, "[StreamUP] Mixer Enhancement: Centered VolumeControl content");
}

// Forward declaration
static QString GetFullSourceName(const QString& elidedText);

/**
 * @brief Event filter for VolumeName buttons to maintain full text and word wrap
 *
 * OBS's VolumeName::updateLabelText() re-applies eliding on resize.
 * This filter watches for those events and restores the full source name.
 * Caches the name to avoid expensive source enumeration on every event.
 */
class VolumeNameFilter : public QObject {
public:
    explicit VolumeNameFilter(QLabel* label, const QString& initialName, QObject* parent = nullptr)
        : QObject(parent), m_label(label), m_cachedName(initialName), m_pendingUpdate(false) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        // Only respond to resize events, not every layout request
        if (event->type() == QEvent::Resize) {

            if (m_label && !m_pendingUpdate) {
                m_pendingUpdate = true;
                // Schedule restoration after OBS finishes its updates
                QTimer::singleShot(50, this, [this]() {
                    m_pendingUpdate = false;
                    if (m_label) {
                        QString currentText = m_label->text();

                        // Only re-lookup if text changed significantly (possible rename)
                        // If it's just elided version of our cached name, use cache
                        bool needsLookup = false;
                        if (!m_cachedName.isEmpty() && !currentText.startsWith(m_cachedName.left(5))) {
                            // Text prefix changed - might be a rename
                            needsLookup = true;
                        }

                        QString fullName = m_cachedName;
                        if (needsLookup || m_cachedName.isEmpty()) {
                            fullName = GetFullSourceName(currentText);
                            if (!fullName.isEmpty()) {
                                m_cachedName = fullName;
                            }
                        }

                        m_label->setWordWrap(true);
                        m_label->setAlignment(Qt::AlignCenter);
                        m_label->setMaximumWidth(QWIDGETSIZE_MAX);

                        if (!fullName.isEmpty() && fullName != currentText) {
                            m_label->setText(fullName);
                        }
                    }
                });
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QPointer<QLabel> m_label;
    QString m_cachedName;
    bool m_pendingUpdate;
};

/**
 * @brief Get the full source name from OBS by matching elided text
 *
 * OBS's VolumeName widget elides the text, so we need to find the actual
 * source name by enumerating all sources and finding a match.
 */
static QString GetFullSourceName(const QString& elidedText)
{
    QString fullName;

    // Remove the elide character (…) and get the prefix/suffix
    QString searchText = elidedText;
    int elidePos = searchText.indexOf(QChar(0x2026)); // Unicode ellipsis
    if (elidePos < 0) {
        elidePos = searchText.indexOf("...");
    }

    QString prefix, suffix;
    if (elidePos >= 0) {
        prefix = searchText.left(elidePos);
        suffix = searchText.mid(elidePos + 1); // Skip the ellipsis
        if (suffix.startsWith("..")) {
            suffix = suffix.mid(2);
        }
    } else {
        // No eliding, return as-is
        return elidedText;
    }

    blog(LOG_DEBUG, "[StreamUP] Mixer Enhancement: Looking for source with prefix='%s' suffix='%s'",
         prefix.toUtf8().constData(), suffix.toUtf8().constData());

    // Enumerate all sources
    auto enumSources = [](void* param, obs_source_t* source) -> bool {
        if (!source) return true;

        auto* data = static_cast<std::pair<QString, QString*>*>(param);
        const QString& searchPrefix = data->first;
        QString* result = data->second;

        const char* name = obs_source_get_name(source);
        if (!name) return true;

        QString sourceName = QString::fromUtf8(name);

        // Check if this source name starts with our prefix
        if (sourceName.startsWith(searchPrefix)) {
            *result = sourceName;
            return false; // Stop enumeration
        }

        return true;
    };

    std::pair<QString, QString*> data{prefix, &fullName};
    obs_enum_sources(enumSources, &data);

    // If not found in regular sources, try scene sources (which can also appear in mixer)
    if (fullName.isEmpty()) {
        obs_enum_scenes(enumSources, &data);
    }

    if (fullName.isEmpty()) {
        blog(LOG_DEBUG, "[StreamUP] Mixer Enhancement: Could not find full name for '%s'",
             elidedText.toUtf8().constData());
        return elidedText;
    }

    blog(LOG_DEBUG, "[StreamUP] Mixer Enhancement: Found full name '%s' for elided '%s'",
         fullName.toUtf8().constData(), elidedText.toUtf8().constData());

    return fullName;
}

/**
 * @brief Style the VolumeName button - ensure label is centered
 * Note: Visual styling (rounded corners, hover) is handled by theme CSS
 */
static void StyleVolumeNameButton(QAbstractButton* button)
{
    // Center the internal label vertically
    QLabel* label = button->findChild<QLabel*>();
    if (label) {
        label->setAlignment(Qt::AlignCenter);
    }
}

/**
 * @brief Enable multi-line source names in a VolumeControl
 */
static void EnableMultiLineName(QWidget* volumeControl)
{
    // Find the VolumeName button (it's a QAbstractButton with class "VolumeName")
    QList<QAbstractButton*> buttons = volumeControl->findChildren<QAbstractButton*>();

    blog(LOG_DEBUG, "[StreamUP] Mixer Enhancement: Found %d buttons in control",
         (int)buttons.size());

    for (QAbstractButton* button : buttons) {
        QString className = QString::fromUtf8(button->metaObject()->className());
        blog(LOG_DEBUG, "[StreamUP] Mixer Enhancement: Button class: %s",
             className.toUtf8().constData());

        if (className == "VolumeName") {
            // Skip if already processed
            if (button->property("streamup_multiline").toBool()) {
                return;
            }

            // Apply styling (rounded corners, no border on hover, margin)
            StyleVolumeNameButton(button);

            // Find the internal QLabel
            QLabel* nameLabel = button->findChild<QLabel*>();
            if (nameLabel) {
                QString currentText = nameLabel->text();

                // Get the full source name by looking it up from OBS
                QString fullName = GetFullSourceName(currentText);

                // Enable word wrap
                nameLabel->setWordWrap(true);
                nameLabel->setAlignment(Qt::AlignCenter);

                // Set the full text (not elided)
                if (!fullName.isEmpty() && fullName != currentText) {
                    nameLabel->setText(fullName);
                }

                // Remove maximum width constraint to allow wrapping
                // The default is 140px which forces eliding
                button->setMaximumWidth(QWIDGETSIZE_MAX);
                nameLabel->setMaximumWidth(QWIDGETSIZE_MAX);

                // Set a reasonable minimum height for multi-line
                button->setMinimumHeight(32);

                // Install event filter to maintain our changes after OBS re-elides
                // Filter caches the name to avoid expensive lookups
                VolumeNameFilter* filter = new VolumeNameFilter(nameLabel, fullName, button);
                button->installEventFilter(filter);

                button->setProperty("streamup_multiline", true);

                blog(LOG_INFO, "[StreamUP] Mixer Enhancement: Enabled multi-line for '%s' (was '%s')",
                     fullName.toUtf8().constData(), currentText.toUtf8().constData());
            } else {
                blog(LOG_DEBUG, "[StreamUP] Mixer Enhancement: No QLabel found in VolumeName");
            }
            break;
        }
    }
}

// ============================================================================
// Public API
// ============================================================================

void EnhanceVolumeControl(QWidget* volumeControl)
{
    if (!volumeControl) {
        return;
    }

    // Reduce internal margins to make channel background tighter
    if (!volumeControl->property("streamup_margins_set").toBool()) {
        // Adjust the VolumeControl's own layout margins
        QLayout* layout = volumeControl->layout();
        if (layout) {
            layout->setContentsMargins(2, 4, 2, 4);
        }

        // Also adjust any child widget layouts
        QList<QWidget*> children = volumeControl->findChildren<QWidget*>();
        for (QWidget* child : children) {
            QLayout* childLayout = child->layout();
            if (childLayout) {
                QMargins margins = childLayout->contentsMargins();
                // Reduce horizontal margins
                childLayout->setContentsMargins(
                    qMin(margins.left(), 2),
                    margins.top(),
                    qMin(margins.right(), 2),
                    margins.bottom()
                );
            }
        }

        volumeControl->setProperty("streamup_margins_set", true);
        blog(LOG_INFO, "[StreamUP] Mixer Enhancement: Reduced VolumeControl margins");
    }

    // Center the dB label
    CenterVolumeLabel(volumeControl);

    // Center all content horizontally
    CenterVolumeControlContent(volumeControl);

    // Enable multi-line source names
    EnableMultiLineName(volumeControl);
}

void RefreshMixerEnhancements()
{
    if (!IsUsingStreamUPTheme()) {
        return;
    }

    QWidget* mixerWidget = FindMixerDock();
    if (!mixerWidget) {
        blog(LOG_DEBUG, "[StreamUP] Mixer Enhancement: Mixer dock not found");
        return;
    }

    QList<QWidget*> volumeControls = FindVolumeControls(mixerWidget);

    blog(LOG_INFO, "[StreamUP] Mixer Enhancement: Found %d volume controls",
         (int)volumeControls.size());

    for (QWidget* control : volumeControls) {
        EnhanceVolumeControl(control);
    }
}

/**
 * @brief Adjust the mixer toolbar to add padding on left and right edges
 */
static void AdjustMixerToolbar(QWidget* mixerWidget)
{
    if (!mixerWidget) return;

    // Find the toolbar
    QToolBar* toolbar = mixerWidget->findChild<QToolBar*>();
    if (!toolbar) {
        blog(LOG_DEBUG, "[StreamUP] Mixer Enhancement: Toolbar not found");
        return;
    }

    // Skip if already processed
    if (toolbar->property("streamup_toolbar_adjusted").toBool()) {
        return;
    }

    // Add margins to toolbar contents
    toolbar->setContentsMargins(8, 0, 8, 0);

    // Find buttons and adjust their margins via stylesheet
    QList<QPushButton*> buttons = toolbar->findChildren<QPushButton*>();
    for (QPushButton* btn : buttons) {
        QString text = btn->text();
        if (text.contains("hidden", Qt::CaseInsensitive)) {
            // Add left margin to the hidden button via stylesheet
            QString currentStyle = btn->styleSheet();
            btn->setStyleSheet(currentStyle + " QPushButton { margin-left: 8px; }");
            blog(LOG_DEBUG, "[StreamUP] Mixer Enhancement: Adjusted hidden button margins");
        } else if (text.contains("Options", Qt::CaseInsensitive)) {
            // Add left margin to push Options button away from the icons
            QString currentStyle = btn->styleSheet();
            btn->setStyleSheet(currentStyle + " QPushButton { margin-left: 16px; }");
            blog(LOG_DEBUG, "[StreamUP] Mixer Enhancement: Adjusted options button margins");
        }
    }

    toolbar->setProperty("streamup_toolbar_adjusted", true);
    blog(LOG_INFO, "[StreamUP] Mixer Enhancement: Adjusted toolbar margins");
}

void ApplyMixerEnhancements()
{
    if (!IsUsingStreamUPTheme()) {
        blog(LOG_INFO, "[StreamUP] Mixer Enhancement: Skipping - StreamUP theme not active");
        return;
    }

    blog(LOG_INFO, "[StreamUP] Applying mixer enhancements...");

    // Create event filter if needed
    if (!g_mixerFilter) {
        g_mixerFilter = new MixerEventFilter();
    }

    // Find and watch the mixer dock
    QWidget* mixerWidget = FindMixerDock();
    if (mixerWidget && !g_mixerWatcherInstalled) {
        // Install filter on the mixer widget to catch layout changes
        mixerWidget->installEventFilter(g_mixerFilter);

        // Also install on scroll areas inside the mixer
        QList<QScrollArea*> scrollAreas = mixerWidget->findChildren<QScrollArea*>();
        for (QScrollArea* area : scrollAreas) {
            area->installEventFilter(g_mixerFilter);
            if (area->widget()) {
                area->widget()->installEventFilter(g_mixerFilter);
            }
        }

        g_mixerWatcherInstalled = true;
        blog(LOG_INFO, "[StreamUP] Mixer Enhancement: Installed mixer event filter");
    }

    // Adjust toolbar margins
    if (mixerWidget) {
        AdjustMixerToolbar(mixerWidget);
    }

    // Apply enhancements to existing volume controls
    RefreshMixerEnhancements();

    // Re-apply after a delay to catch late-loaded sources
    QTimer::singleShot(500, []() {
        RefreshMixerEnhancements();
    });

    QTimer::singleShot(2000, []() {
        RefreshMixerEnhancements();
    });

    blog(LOG_INFO, "[StreamUP] Mixer Enhancement: Setup complete");
}

} // namespace MixerEnhancements
} // namespace StreamUP
