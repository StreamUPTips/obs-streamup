#include "plugin-state.hpp"
#include "../utilities/debug-logger.hpp"
#include <obs-module.h>

namespace StreamUP {

PluginState& PluginState::Instance() {
    static PluginState instance;
    return instance;
}

const std::map<std::string, PluginInfo>& PluginState::GetAllPlugins() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_allPlugins;
}

const std::map<std::string, PluginInfo>& PluginState::GetRequiredPlugins() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_requiredPlugins;
}

void PluginState::SetAllPlugins(const std::map<std::string, PluginInfo>& plugins) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_allPlugins = plugins;
    StreamUP::DebugLogger::LogInfoFormat("PluginState", "Updated all plugins registry with %zu entries", plugins.size());
}

void PluginState::SetRequiredPlugins(const std::map<std::string, PluginInfo>& plugins) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_requiredPlugins = plugins;
    StreamUP::DebugLogger::LogInfoFormat("PluginState", "Updated required plugins registry with %zu entries", plugins.size());
}

void PluginState::AddPlugin(const std::string& key, const PluginInfo& plugin) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_allPlugins[key] = plugin;
}

void PluginState::AddRequiredPlugin(const std::string& key, const PluginInfo& plugin) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_requiredPlugins[key] = plugin;
}

void PluginState::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_allPlugins.clear();
    m_requiredPlugins.clear();
    m_initialized = false;
    m_cachedStatus = PluginCheckResults(); // Reset cached status
    StreamUP::DebugLogger::LogInfo("PluginState", "Plugin state reset");
}

const PluginState::PluginCheckResults& PluginState::GetCachedPluginStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cachedStatus;
}

void PluginState::SetPluginStatus(const PluginCheckResults& results) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cachedStatus = results;
    m_cachedStatus.lastChecked = std::chrono::system_clock::now();
    m_cachedStatus.isValid = true;
}

void PluginState::InvalidatePluginStatus() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cachedStatus.isValid = false;
}

bool PluginState::IsPluginStatusCached() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cachedStatus.isValid;
}

} // namespace StreamUP