#ifndef STREAMUP_DEBUG_LOGGER_HPP
#define STREAMUP_DEBUG_LOGGER_HPP

#include <obs.h>
#include <functional>
#include <string>

namespace StreamUP {
namespace DebugLogger {

/**
 * @brief Log a debug message only if debug logging is enabled
 * @param feature The feature name for context (e.g., "WebSocket", "Toolbar", "Settings")
 * @param operation The operation being performed (e.g., "Initialize", "Connect", "Update")
 * @param message The debug message
 */
void LogDebug(const char* feature, const char* operation, const char* message);

/**
 * @brief Log a debug message with formatted string only if debug logging is enabled
 * @param feature The feature name for context
 * @param operation The operation being performed
 * @param format Printf-style format string
 * @param ... Printf-style arguments
 */
void LogDebugFormat(const char* feature, const char* operation, const char* format, ...);

/**
 * @brief Log an info message (always logged regardless of debug setting)
 * @param feature The feature name for context
 * @param message The info message
 */
void LogInfo(const char* feature, const char* message);

/**
 * @brief Log a warning message (always logged regardless of debug setting)
 * @param feature The feature name for context
 * @param message The warning message
 */
void LogWarning(const char* feature, const char* message);

/**
 * @brief Log an error message (always logged regardless of debug setting)
 * @param feature The feature name for context
 * @param message The error message
 */
void LogError(const char* feature, const char* message);

/**
 * @brief Log an info message with formatted string (always logged)
 * @param feature The feature name for context
 * @param format Printf-style format string
 * @param ... Printf-style arguments
 */
void LogInfoFormat(const char* feature, const char* format, ...);

/**
 * @brief Log a warning message with formatted string (always logged)
 * @param feature The feature name for context
 * @param format Printf-style format string
 * @param ... Printf-style arguments
 */
void LogWarningFormat(const char* feature, const char* format, ...);

/**
 * @brief Log an error message with formatted string (always logged)
 * @param feature The feature name for context
 * @param format Printf-style format string
 * @param ... Printf-style arguments
 */
void LogErrorFormat(const char* feature, const char* format, ...);

/**
 * @brief Set the initialization phase completion status
 * @param completed True when plugin initialization is complete, false during startup
 */
void SetInitializationComplete(bool completed);

/**
 * @brief Check if plugin initialization is complete
 * @return True if initialization is complete, false during startup
 */
bool IsInitializationComplete();

/**
 * @brief Set the bracketed prefix every line is tagged with
 *
 * This is what makes the logger shareable: each plugin owns its own name in the
 * OBS log, so the reader can tell which StreamUP module wrote a line. Call it
 * once from obs_module_load, before anything logs. Defaults to "[StreamUP]".
 *
 * @param prefix The prefix, brackets included (e.g. "[StreamUP Source Explorer]")
 */
void SetLogPrefix(const char* prefix);

/**
 * @brief Point the debug gate at the plugin's own settings
 *
 * Plugins disagree about where the "debug logging" toggle lives, so the logger
 * refuses to own that decision: the gate is a single predicate the plugin
 * installs, once, from obs_module_load. The main StreamUP plugin points it at
 * its SettingsManager toggle.
 *
 * A plugin that never calls this gets no debug output at all, which is the
 * intended default: debug lines are noise in the OBS log until someone has
 * deliberately asked for them, and a plugin with no user-facing toggle has no
 * way for anyone to ask.
 *
 * The predicate is called on whatever thread logs, so it must be thread-safe.
 *
 * @param predicate Returns true while debug messages should be written
 */
void SetDebugLoggingPredicate(std::function<bool()> predicate);

/**
 * @brief Check if debug logging is currently enabled
 * @return The predicate's answer, or false while no predicate is set
 */
bool IsDebugLoggingEnabled();

} // namespace DebugLogger
} // namespace StreamUP

#endif // STREAMUP_DEBUG_LOGGER_HPP
