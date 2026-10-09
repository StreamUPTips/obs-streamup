#pragma once

#include <QString>

// The Overlay Controls window: one panel per installed overlay, built from the
// controls its .StreamUP file declared (see core/overlay-controls.hpp).
namespace StreamUP {
namespace OverlayControlsWindow {

// Opens (or raises) the panel for an overlay. Safe from any thread.
void Show(const QString &overlayId);

// After an install: tells the user the overlay has a panel and offers to open
// it. Safe from any thread.
void OfferAfterInstall(const QString &overlayId);

// File picker for attaching controls to an overlay installed before this
// feature existed. UI thread only.
void AttachFromFile();

// Asks, then forgets an overlay's controls. Its sources are left alone. UI thread only.
void ConfirmRemove(const QString &overlayId);

} // namespace OverlayControlsWindow
} // namespace StreamUP
