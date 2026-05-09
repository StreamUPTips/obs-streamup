#pragma once

#include <functional>

namespace StreamUP {
namespace ModuleSetupWizard {

/**
 * @brief Show the first-launch module setup wizard.
 *
 * Lets a brand-new user pick which sections of the StreamUP plugin they want
 * enabled. Always sets settings.moduleSetupComplete = true on close (Save,
 * Skip, or window close), so the wizard never appears twice.
 *
 * @param onFinished Optional callback invoked after the wizard closes. Used by
 *                   OnOBSFinishedLoading to chain the existing welcome splash
 *                   afterwards. Pass nullptr if no follow-up is needed.
 */
void Show(std::function<void()> onFinished);

} // namespace ModuleSetupWizard
} // namespace StreamUP
