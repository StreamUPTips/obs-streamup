#include "error-handler.hpp"
#include "../ui/settings-manager.hpp"
#include "ui-helpers.hpp"
#include <streamup/ui/dialogs.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QFile>
#include <QFileInfo>
#include <QWidget>
#include <unordered_map>
#include <vector>

using namespace StreamUP::UIStyles;
namespace su = StreamUP::UIStyles;

namespace StreamUP {
namespace ErrorHandler {

namespace {
    // Convert severity to OBS log level
    int SeverityToLogLevel(Severity severity) {
        switch (severity) {
            case Severity::Info: return LOG_INFO;
            case Severity::Warning: return LOG_WARNING;
            case Severity::Error: return LOG_ERROR;
            case Severity::Critical: return LOG_ERROR;
            default: return LOG_INFO;
        }
    }
    
    // Convert category to string prefix
    std::string CategoryToString(Category category) {
        static const std::unordered_map<Category, std::string> category_map = {
            {Category::General, "General"},
            {Category::FileSystem, "FileSystem"},
            {Category::Network, "Network"},
            {Category::Plugin, "Plugin"},
            {Category::Source, "Source"},
            {Category::UI, "UI"}
        };
        
        auto it = category_map.find(category);
        return (it != category_map.end()) ? it->second : "Unknown";
    }
    
    // Format log message with consistent structure
    std::string FormatLogMessage(const std::string& message, Category category) {
        return "[StreamUP:" + CategoryToString(category) + "] " + message;
    }
}

// Logging functions
void Log(Severity severity, Category category, const std::string& message) {
    int log_level = SeverityToLogLevel(severity);
    std::string formatted_message = FormatLogMessage(message, category);
    blog(log_level, "%s", formatted_message.c_str());
}

void LogInfo(const std::string& message, Category category) {
    // Only log info messages when debug logging is enabled
    if (StreamUP::SettingsManager::IsDebugLoggingEnabled()) {
        Log(Severity::Info, category, message);
    }
}

void LogWarning(const std::string& message, Category category) {
    Log(Severity::Warning, category, message);
}

void LogError(const std::string& message, Category category) {
    Log(Severity::Error, category, message);
}

void LogCritical(const std::string& message, Category category) {
    Log(Severity::Critical, category, message);
}

// Error result factory functions
ErrorResult Success(const std::string& message) {
    return ErrorResult(true, message);
}

ErrorResult Failure(const std::string& message, Category category, Severity severity) {
    Log(severity, category, message);
    return ErrorResult(false, message, category, severity);
}

ErrorResult SourceError(const std::string& message) {
    return Failure(message, Category::Source, Severity::Error);
}

// Dialog display functions
//
// These were title + message + OK popups built from the legacy CreateStyledDialog
// factory. They are now the SoT branded notification: su::info(parent, title,
// message) — a modeless ShadowDialog window with the message in the content and a
// single dismiss button (brandFooter=false, secondary popup). su::info() self-
// shows; ShowDialogOnUIThread keeps the construction on the UI thread.
void ShowErrorDialog(const std::string& title, const std::string& message) {
    StreamUP::UIHelpers::ShowDialogOnUIThread([title, message]() {
        QWidget* parent = static_cast<QWidget*>(obs_frontend_get_main_window());
        su::info(parent, QString::fromStdString(title), QString::fromStdString(message),
                 obs_module_text("OK"));
    });
}

void ShowWarningDialog(const std::string& title, const std::string& message) {
    StreamUP::UIHelpers::ShowDialogOnUIThread([title, message]() {
        QWidget* parent = static_cast<QWidget*>(obs_frontend_get_main_window());
        su::info(parent, QString::fromStdString(title), QString::fromStdString(message),
                 obs_module_text("OK"));
    });
}

// Validation functions
bool ValidateSource(obs_source_t* source, const std::string& operation) {
    if (!source) {
        std::string message = "Invalid source provided";
        if (!operation.empty()) {
            message += " for operation: " + operation;
        }
        LogError(message, Category::Source);
        return false;
    }
    return true;
}

bool ValidatePointer(void* ptr, const std::string& pointer_name) {
    if (!ptr) {
        LogError("Null pointer: " + pointer_name, Category::General);
        return false;
    }
    return true;
}

bool ValidateFile(const QString& file_path) {
    if (file_path.isEmpty()) {
        LogError("Empty file path provided", Category::FileSystem);
        return false;
    }
    
    if (!QFile::exists(file_path)) {
        LogError("File does not exist: " + file_path.toStdString(), Category::FileSystem);
        return false;
    }
    
    QFileInfo file_info(file_path);
    if (!file_info.isReadable()) {
        LogError("File is not readable: " + file_path.toStdString(), Category::FileSystem);
        return false;
    }
    
    return true;
}

// ErrorContext implementation
ErrorContext::ErrorContext(const std::string& context_name, Category category)
    : m_context_name(context_name), m_category(category) {
    LogInfo("Entering context: " + context_name, category);
}

ErrorContext::~ErrorContext() {
    LogInfo("Exiting context: " + m_context_name, m_category);
}

} // namespace ErrorHandler
} // namespace StreamUP
