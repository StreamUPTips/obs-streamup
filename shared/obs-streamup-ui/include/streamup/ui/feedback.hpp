#pragma once

#include <QString>

class QWidget;

namespace StreamUP {
namespace UIStyles {

// Shared in-app feedback dialog (feature request / bug report / general
// feedback). Posts to the StreamUP feedback API. This is the default action of
// the window-chrome header feedback button: every non-popup StreamUP window
// gets it for free, so a plugin only needs its own handler when it wants
// something bespoke (e.g. the Chat and ProText plugins keep localized variants).
//
// productName  - shown in the brand footer and sent as the feedback productName
//                (e.g. "StreamUP Source Explorer"). The window chrome passes the
//                window's brandName here.
// productVersion - the plugin version for display (e.g. "v1.0.1"); a leading
//                "v" is stripped before it is sent to the API.
void show_feedback_dialog(QWidget *parent, const QString &productName,
			  const QString &productVersion);

} // namespace UIStyles
} // namespace StreamUP
