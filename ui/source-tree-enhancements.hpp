#pragma once

#include <QObject>
#include <QEvent>

namespace StreamUP {
namespace SourceTreeEnhancements {

/**
 * @brief Gives groups in the Sources dock the Scene Organiser's folder look
 *
 * OBS's Sources dock (and Aitum Vertical's copy of it) is a flat list, so a
 * theme can only style the group arrow. This adds the rest at runtime, on the
 * StreamUP theme only:
 * - a group's children are indented one step past the group's folder icon
 * - guide lines run down from the folder icon to its children
 * - the folder icon shows open or closed with the group
 */
class SourceTreeFilter : public QObject {
    Q_OBJECT

public:
    explicit SourceTreeFilter(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

/**
 * @brief Hook every source tree that exists now, and any created later
 */
void ApplySourceTreeEnhancements();

/**
 * @brief Remove the hooks and put the trees back as OBS drew them
 */
void CleanupSourceTreeEnhancements();

} // namespace SourceTreeEnhancements
} // namespace StreamUP
