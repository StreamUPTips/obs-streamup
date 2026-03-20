#pragma once

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QEvent>

namespace StreamUP {
namespace MixerEnhancements {

/**
 * @brief Event filter that maintains mixer enhancements after layout changes
 *
 * OBS recalculates mixer layouts on resize and source changes. This filter
 * watches for those events and reapplies our customizations.
 */
class MixerEventFilter : public QObject {
    Q_OBJECT

public:
    explicit MixerEventFilter(QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

/**
 * @brief Apply mixer enhancements when StreamUP theme is active
 *
 * Enhancements include:
 * - Centered dB labels
 * - Multi-line source names (word wrap instead of eliding)
 * - Centered volume controls
 */
void ApplyMixerEnhancements();

/**
 * @brief Re-apply enhancements to all VolumeControl widgets
 *
 * Called after source changes or layout updates.
 */
void RefreshMixerEnhancements();

/**
 * @brief Apply enhancements to a single VolumeControl widget
 */
void EnhanceVolumeControl(QWidget* volumeControl);

/**
 * @brief Check if StreamUP theme is active
 */
bool IsUsingStreamUPTheme();

/**
 * @brief Reset the cached theme check so it is re-evaluated on next call
 *
 * Call this when the OBS theme changes.
 */
void ResetThemeCache();

/**
 * @brief Clean up mixer enhancement resources
 *
 * Deletes the global event filter and resets state.
 * Call during plugin shutdown.
 */
void CleanupMixerEnhancements();

} // namespace MixerEnhancements
} // namespace StreamUP
