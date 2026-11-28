#pragma once

#include <QDialog>
#include <QPointer>

namespace StreamUP {
namespace PatchNotesWindow {

// Show patch notes window
void ShowPatchNotesWindow();

// The What's new page: picture cards for this release, every change folded
// underneath and the membership strip. Hosted by the Features window.
QWidget *BuildWhatsNewContent();

} // namespace PatchNotesWindow
} // namespace StreamUP
