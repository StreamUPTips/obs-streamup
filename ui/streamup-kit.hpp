#pragma once

// obs-streamup's own additions to the shared StreamUP UI kit
// (shared/obs-streamup-ui, streamup/ui/kit.hpp): the StreamUP Membership card
// and its links. Everything else in the kit lives in the shared library.

#include <streamup/ui/kit.hpp>

namespace StreamUP {
namespace Kit {

// The StreamUP Membership card. Strip: one row for a footer. Card: a little
// taller, for an empty state or About. Returns nullptr for members (there is
// no member detection yet, so it always shows; see IsMember).
enum class MemberStyle { Strip, Card };
QWidget *MembershipCard(MemberStyle style = MemberStyle::Strip, const QString &line = QString());
bool IsMember();
QString MembershipUrl();

} // namespace Kit
} // namespace StreamUP
