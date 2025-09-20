#include "debug-logger.hpp"
#include "../ui/settings-manager.hpp"
#include <cstdarg>
#include <cstdio>
#include <memory>

namespace StreamUP {
namespace DebugLogger {

static std::string FormatMessage(const char* feature, const char* operation, const char* message)
{
    if (operation && strlen(operation) > 0) {
        return std::string("[StreamUP] [") + feature + "] " + operation + ": " + message;
    } else {
        return std::string("[StreamUP] [") + feature + "] " + message;
    }
}

static std::string FormatMessageSimple(const char* feature, const char* message)
{
    return std::string("[StreamUP] [") + feature + "] " + message;
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
    if (StreamUP::SettingsManager::IsDebugLoggingEnabled()) {
        std::string formatted = FormatMessage(feature, operation, message);
        blog(LOG_DEBUG, "%s", formatted.c_str());
    }
}

void LogDebugFormat(const char* feature, const char* operation, const char* format, ...)
{
    if (StreamUP::SettingsManager::IsDebugLoggingEnabled()) {
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

} // namespace DebugLogger
} // namespace StreamUP