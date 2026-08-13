#ifndef STREAMUP_BACKUP_UI_COMMON_HPP
#define STREAMUP_BACKUP_UI_COMMON_HPP

// The backup and restore windows build their sections from the shared design
// system component (streamup/ui/section-card.hpp), so they group information
// exactly like every other StreamUP window. This header only exists to pull
// that in under a short alias.

#include <streamup/ui/section-card.hpp>

namespace StreamUP {
namespace BackupUI {

using StreamUP::UIStyles::cardFact;
using StreamUP::UIStyles::cardFacts;
using StreamUP::UIStyles::cardFileTable;
using StreamUP::UIStyles::exportFileRows;
using StreamUP::UIStyles::FileExportFormat;
using StreamUP::UIStyles::cardList;
using StreamUP::UIStyles::FileRow;
using StreamUP::UIStyles::cardText;
using StreamUP::UIStyles::formatBytes;
using StreamUP::UIStyles::sectionCard;

} // namespace BackupUI
} // namespace StreamUP

#endif // STREAMUP_BACKUP_UI_COMMON_HPP
