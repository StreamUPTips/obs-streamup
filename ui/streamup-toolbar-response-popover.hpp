#pragma once

#include <QString>

class QWidget;

namespace StreamUP {

// Shows the result of a toolbar WebSocket button next to the button that fired
// it, and puts the raw JSON on the clipboard at the same time.
//
// The popover is a Qt::Popup, so it closes on the next click anywhere without
// needing a timer or a close button, and it never steals focus from OBS while
// you are live.
//
// anchor       - the button that was clicked; the popover is placed against it
// title        - the request type, shown as the heading
// responseJson - response_data straight from obs-websocket; may be empty
// errorText    - non-empty when the request failed, shown instead of the data
void showWebSocketResponsePopover(QWidget *anchor, const QString &title, const QString &responseJson,
				  const QString &errorText = QString());

} // namespace StreamUP
