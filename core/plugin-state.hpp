#ifndef STREAMUP_PLUGIN_STATE_HPP
#define STREAMUP_PLUGIN_STATE_HPP

#include "streamup-common.hpp"
#include <map>
#include <string>
#include <memory>
#include <mutex>

namespace StreamUP {

class PluginState {
public:
    static PluginState& Instance();
    
    // Plugin management
    const std::map<std::string, PluginInfo>& GetAllPlugins() const;
    const std::map<std::string, PluginInfo>& GetRequiredPlugins() const;
    void SetAllPlugins(const std::map<std::string, PluginInfo>& plugins);
    void SetRequiredPlugins(const std::map<std::string, PluginInfo>& plugins);
    void AddPlugin(const std::string& key, const PluginInfo& plugin);
    void AddRequiredPlugin(const std::string& key, const PluginInfo& plugin);
    
    // State management
    bool IsInitialized() const { return m_initialized; }
    void SetInitialized(bool initialized) { m_initialized = initialized; }
    
    // Prevent copying and assignment
    PluginState(const PluginState&) = delete;
    PluginState& operator=(const PluginState&) = delete;
    
    // Cleanup
    void Reset();

private:
    PluginState() = default;
    ~PluginState() = default;
    
    mutable std::mutex m_mutex;
    std::map<std::string, PluginInfo> m_allPlugins;
    std::map<std::string, PluginInfo> m_requiredPlugins;
    bool m_initialized = false;
};

// Convenience access functions
inline const std::map<std::string, PluginInfo>& GetAllPlugins() {
    return PluginState::Instance().GetAllPlugins();
}

inline const std::map<std::string, PluginInfo>& GetRequiredPlugins() {
    return PluginState::Instance().GetRequiredPlugins();
}

inline void SetAllPlugins(const std::map<std::string, PluginInfo>& plugins) {
    PluginState::Instance().SetAllPlugins(plugins);
}

inline void SetRequiredPlugins(const std::map<std::string, PluginInfo>& plugins) {
    PluginState::Instance().SetRequiredPlugins(plugins);
}

} // namespace StreamUP

#endif // STREAMUP_PLUGIN_STATE_HPP