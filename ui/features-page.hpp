#pragma once

#include <QString>

#include <functional>

// The Features page: every part of StreamUP as a card with a short demo, one
// line about it, an on/off switch and "Show me". Replaces the first-run module
// wizard and Settings > Plugins.
namespace StreamUP {
namespace FeaturesPage {

// The one StreamUP window for welcome, what's new and features. It changes
// page in place, so start-up never opens two windows.
enum class Mode { Welcome, WhatsNew, Features };

// Opens (or raises) it on the Features page. Safe from any thread.
void Show();

// Opens (or raises) it on What's new (Help > Patch Notes).
void ShowWhatsNew();

// At start-up: Welcome for a first install, What's new after an update, the
// Features page when this version adds a part the user has not seen, and
// otherwise nothing.
void ShowAtStartup();

bool IsOpen();

// Runs fn once the window has closed (straight away when it is not open), so
// a start-up check never stacks a second window on top of it.
void WhenClosed(std::function<void()> fn);

} // namespace FeaturesPage
} // namespace StreamUP
