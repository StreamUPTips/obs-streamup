#include <streamup/debug-logger.hpp>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <atomic>
#include <mutex>

namespace StreamUP {
namespace DebugLogger {

// Thread-safe initialization tracking
static std::atomic<bool> initializationComplete{false};

// The built-in debug toggle, used by plugins that have no settings store of
// their own to read the flag from.
static std::atomic<bool> debugLoggingEnabled{false};

// The prefix and the debug gate are both set once at module load and read from
// every logging thread afterwards, so a plain mutex around the swap is enough:
// it never contends in practice, and it keeps a half-written std::function or
// std::string from being read mid-assignment.
static std::mutex configMutex;
static std::string logPrefix = "[StreamUP]";
static std::function<bool()> debugPredicate;

static std::string Prefix()
{
    std::lock_guard<std::mutex> lock(configMutex);
    return logPrefix;
}

static std::string FormatMessage(const char* feature, const char* operation, const char* message)
{
    if (operation && strlen(operation) > 0) {
        return Prefix() + " [" + feature + "] " + operation + ": " + message;
    } else {
        return Prefix() + " [" + feature + "] " + message;
    }
}

static std::string FormatMessageSimple(const char* feature, const char* message)
{
    return Prefix() + " [" + feature + "] " + message;
}

// Helper function to reduce code duplication for formatted logging
static std::string FormatStringArgs(const char* format, va_list args)
{
    // Calculate required size for formatted string
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    if (size <= 0) {
        return "";
    }

    // Create buffer and format string
    std::unique_ptr<char[]> buffer(new char[size + 1]);
    vsnprintf(buffer.get(), size + 1, format, args);

    return std::string(buffer.get());
}

void LogDebug(const char* feature, const char* operation, const char* message)
{
    // During initialization, always log debug messages to avoid mutex deadlock
    // After initialization, respect the user's debug logging setting
    if (!initializationComplete.load() || IsDebugLoggingEnabled()) {
        std::string formatted = FormatMessage(feature, operation, message);
        blog(LOG_DEBUG, "%s", formatted.c_str());
    }
}

void LogDebugFormat(const char* feature, const char* operation, const char* format, ...)
{
    // During initialization, always log debug messages to avoid mutex deadlock
    // After initialization, respect the user's debug logging setting
    if (!initializationComplete.load() || IsDebugLoggingEnabled()) {
        va_list args;
        va_start(args, format);

        std::string message = FormatStringArgs(format, args);
        if (!message.empty()) {
            std::string formatted = FormatMessage(feature, operation, message.c_str());
            blog(LOG_DEBUG, "%s", formatted.c_str());
        }

        va_end(args);
    }
}

void LogInfo(const char* feature, const char* message)
{
    std::string formatted = FormatMessageSimple(feature, message);
    blog(LOG_INFO, "%s", formatted.c_str());
}

void LogWarning(const char* feature, const char* message)
{
    std::string formatted = FormatMessageSimple(feature, message);
    blog(LOG_WARNING, "%s", formatted.c_str());
}

void LogError(const char* feature, const char* message)
{
    std::string formatted = FormatMessageSimple(feature, message);
    blog(LOG_ERROR, "%s", formatted.c_str());
}

void LogInfoFormat(const char* feature, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    std::string message = FormatStringArgs(format, args);
    if (!message.empty()) {
        std::string formatted = FormatMessageSimple(feature, message.c_str());
        blog(LOG_INFO, "%s", formatted.c_str());
    }

    va_end(args);
}

void LogWarningFormat(const char* feature, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    std::string message = FormatStringArgs(format, args);
    if (!message.empty()) {
        std::string formatted = FormatMessageSimple(feature, message.c_str());
        blog(LOG_WARNING, "%s", formatted.c_str());
    }

    va_end(args);
}

void LogErrorFormat(const char* feature, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    std::string message = FormatStringArgs(format, args);
    if (!message.empty()) {
        std::string formatted = FormatMessageSimple(feature, message.c_str());
        blog(LOG_ERROR, "%s", formatted.c_str());
    }

    va_end(args);
}

void SetInitializationComplete(bool completed)
{
    initializationComplete.store(completed);
}

bool IsInitializationComplete()
{
    return initializationComplete.load();
}

void SetLogPrefix(const char* prefix)
{
    if (!prefix || !*prefix) {
        return;
    }
    std::lock_guard<std::mutex> lock(configMutex);
    logPrefix = prefix;
}

void SetDebugLoggingPredicate(std::function<bool()> predicate)
{
    std::lock_guard<std::mutex> lock(configMutex);
    debugPredicate = std::move(predicate);
}

void SetDebugLoggingEnabled(bool enabled)
{
    debugLoggingEnabled.store(enabled);
}

bool IsDebugLoggingEnabled()
{
    // Copy the predicate out before calling it, so it is never invoked with the
    // config lock held: the main plugin's predicate reads SettingsManager,
    // which takes locks of its own.
    std::function<bool()> predicate;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        predicate = debugPredicate;
    }
    return predicate ? predicate() : debugLoggingEnabled.load();
}

} // namespace DebugLogger
} // namespace StreamUP
