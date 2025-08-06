#pragma once

#include <QDialog>
#include <QPointer>

namespace StreamUP {
namespace PatchNotesWindow {

// Show patch notes window
void ShowPatchNotesWindow();

// Check if patch notes window is currently open
bool IsPatchNotesWindowOpen();

// Internal dialog creation
void CreatePatchNotesDialog();

} // namespace PatchNotesWindow
} // namespace StreamUP