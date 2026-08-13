#ifndef STREAMUP_RESTORE_DIALOG_HPP
#define STREAMUP_RESTORE_DIALOG_HPP

namespace StreamUP {
namespace Restore {

/**
 * Pick a backup, show what is inside it and what is missing on this machine,
 * then stage the restore. UI thread only.
 */
void ShowRestoreDialog();

/**
 * Show the outcome of a restore that was applied during the last shutdown.
 * Called once after OBS finishes loading; does nothing if there is no report
 * or it has already been shown.
 */
void ShowAppliedReportIfAny();

} // namespace Restore
} // namespace StreamUP

#endif // STREAMUP_RESTORE_DIALOG_HPP
