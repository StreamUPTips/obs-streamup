#ifndef STREAMUP_BACKUP_DIALOG_HPP
#define STREAMUP_BACKUP_DIALOG_HPP

namespace StreamUP {
namespace Backup {

/**
 * Ask what to include, pick a destination, then write the archive.
 * Safe to call from the UI thread only.
 */
void ShowCreateBackupDialog();

} // namespace Backup
} // namespace StreamUP

#endif // STREAMUP_BACKUP_DIALOG_HPP
