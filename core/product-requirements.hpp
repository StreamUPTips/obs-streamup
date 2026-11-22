#pragma once

// What a .StreamUP file needs before it can install properly.
//
// The plugins a product needs are read from the file itself, not from the
// global "required" list: every source, filter and transition in it names its
// type, and a type OBS has not registered means the plugin behind it is
// missing. A built-in table turns type ids into plugin names and links, and a
// manifest can name plugins of its own ("requires") for anything that adds no
// source types (docks, frontend tools). Plugins the file uses that are older
// than the StreamUP plugin list wants are reported as updates, as before.

#include "plugin-manager.hpp"

#include <QString>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace StreamUP {
namespace ProductRequirements {

struct Result {
	std::vector<PluginManager::PluginRow> missing;
	std::vector<PluginManager::PluginRow> outdated;
	std::map<std::string, bool> disabled; // plugin name -> blocked by safe mode
	std::set<std::string> usedTypes;      // every type id the file uses
	QString productName;
	bool readable = true;                 // false: the file could not be parsed
	bool anyUnknown = false;              // a type no table entry knows
	bool ok() const { return missing.empty() && outdated.empty() && disabled.empty(); }
};

// Reads the file and checks it against what OBS has loaded. UI thread or any
// thread: it only reads registered types and the OBS log.
Result Check(const QString &path);

// Shows the issues window for a result. continueCallback (optional) gets a
// Continue Anyway button. UI thread.
void ShowIssues(const Result &result, std::function<void()> continueCallback);

// Picks the file's requirements, then either runs onReady straight away or
// shows what is missing first (with Continue Anyway running onReady).
void CheckThen(const QString &path, std::function<void()> onReady);

} // namespace ProductRequirements
} // namespace StreamUP
