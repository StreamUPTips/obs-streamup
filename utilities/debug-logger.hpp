#ifndef STREAMUP_DEBUG_LOGGER_HPP
#define STREAMUP_DEBUG_LOGGER_HPP

#include <obs.h>
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

} // namespace DebugLogger
} // namespace StreamUP

#endif // STREAMUP_DEBUG_LOGGER_HPP