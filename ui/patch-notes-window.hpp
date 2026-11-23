#pragma once

#include <QDialog>
#include <QPointer>

namespace StreamUP {
namespace PatchNotesWindow {

// Show patch notes window
void ShowPatchNotesWindow();

// Check if patch notes window is currently open
bool IsPatchNotesWindowOpen();

// Internal dialog creation (opens the StreamUP window on its What's new page)
void CreatePatchNotesDialog();

// The What's new page: picture cards for this release, every change folded
// underneath and the membership strip. Hosted by the Features window.
QWidget *BuildWhatsNewContent();

} // namespace PatchNotesWindow
} // namespace StreamUP
