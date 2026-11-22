#pragma once

#include <QString>

#include <functional>

// The Features page: every part of StreamUP as a card with a short demo, one
// line about it, an on/off switch and "Show me". Replaces the first-run module
// wizard and Settings > Plugins.
namespace StreamUP {
namespace FeaturesPage {

// Opens (or raises) the page. Safe from any thread.
void Show();

// At start-up: opens the page for a new user, or when this version adds a
// part the user has not seen, and otherwise does nothing. onDone runs after
// the page closes (or straight away when it does not open), so the welcome
// flow can follow it.
void ShowIfNeeded(std::function<void()> onDone);

} // namespace FeaturesPage
} // namespace StreamUP
