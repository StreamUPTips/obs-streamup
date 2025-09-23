#ifndef STREAMUP_ERROR_HANDLER_HPP
#define STREAMUP_ERROR_HANDLER_HPP

#include <string>
#include <functional>
#include <obs-module.h>
#include <QString>

namespace StreamUP {
namespace ErrorHandler {

// Error severity levels
enum class Severity {
    Info,
    Warning,
    Error,
    Critical
};

// Error categories for better organization
enum class Category {
    General,
    FileSystem,
    Network,
    Plugin,
    Source,
    UI
};

// Error result structure
struct ErrorResult {
    bool success;
    std::string message;
    Category category;
    Severity severity;
    
    ErrorResult(bool success = true, const std::string& message = "", 
                Category category = Category::General, Severity severity = Severity::Info)
        : success(success), message(message), category(category), severity(severity) {}
    
    // Implicit conversion to bool for easy checking
    operator bool() const { return success; }
};

// Logging functions with standardized format
void Log(Severity severity, Category category, const std::string& message);
void LogInfo(const std::string& message, Category category = Category::General);
void LogWarning(const std::string& message, Category category = Category::General);
void LogError(const std::string& message, Category category = Category::General);
void LogCritical(const std::string& message, Category category = Category::General);

// Error result factory functions
ErrorResult Success(const std::string& message = "");
ErrorResult Failure(const std::string& message, Category category = Category::General, 
                   Severity severity = Severity::Error);
ErrorResult NetworkError(const std::string& message);
ErrorResult FileSystemError(const std::string& message);
ErrorResult PluginError(const std::string& message);
ErrorResult SourceError(const std::string& message);
ErrorResult UIError(const std::string& message);

// Dialog display functions for UI errors
void ShowErrorDialog(const std::string& title, const std::string& message);
void ShowWarningDialog(const std::string& title, const std::string& message);
void ShowInfoDialog(const std::string& title, const std::string& message);

// Template function for safe operations with error handling
template<typename T, typename Func>
ErrorResult SafeExecute(Func&& func, const std::string& operation_name, 
                       Category category = Category::General) {
    try {
        if constexpr (std::is_void_v<T>) {
            func();
            LogInfo("Successfully completed: " + operation_name, category);
            return Success();
        } else {
            T result = func();
            if (result) {
                LogInfo("Successfully completed: " + operation_name, category);
                return Success();
            } else {
                std::string error_msg = "Failed to complete: " + operation_name;
                LogError(error_msg, category);
                return Failure(error_msg, category);
            }
        }
    } catch (const std::exception& e) {
        std::string error_msg = "Exception in " + operation_name + ": " + e.what();
        LogError(error_msg, category);
        return Failure(error_msg, category, Severity::Critical);
    } catch (...) {
        std::string error_msg = "Unknown exception in " + operation_name;
        LogCritical(error_msg, category);
        return Failure(error_msg, category, Severity::Critical);
    }
}

// Utility functions for common error checking patterns
bool ValidateSource(obs_source_t* source, const std::string& operation = "");
bool ValidateString(const char* str, const std::string& field_name = "string");
bool ValidatePointer(void* ptr, const std::string& pointer_name = "pointer");
bool ValidateFile(const QString& file_path);

// Error context for nested operations
class ErrorContext {
public:
    explicit ErrorContext(const std::string& context_name, Category category = Category::General);
    ~ErrorContext();
    
    void AddDetail(const std::string& detail);
    ErrorResult CreateError(const std::string& message, Severity severity = Severity::Error);
    
private:
    std::string m_context_name;
    Category m_category;
    std::vector<std::string> m_details;
};

} // namespace ErrorHandler
} // namespace StreamUP

// Convenience macros for common error patterns
#define STREAMUP_LOG_INFO(msg) StreamUP::ErrorHandler::LogInfo(msg)
#define STREAMUP_LOG_WARNING(msg) StreamUP::ErrorHandler::LogWarning(msg)
#define STREAMUP_LOG_ERROR(msg) StreamUP::ErrorHandler::LogError(msg)
#define STREAMUP_LOG_CRITICAL(msg) StreamUP::ErrorHandler::LogCritical(msg)

#define STREAMUP_VALIDATE_SOURCE(source) \
    if (!StreamUP::ErrorHandler::ValidateSource(source, __FUNCTION__)) { \
        return StreamUP::ErrorHandler::SourceError("Invalid source in " + std::string(__FUNCTION__)); \
    }

#define STREAMUP_VALIDATE_POINTER(ptr, name) \
    if (!StreamUP::ErrorHandler::ValidatePointer(ptr, name)) { \
        return StreamUP::ErrorHandler::Failure("Null pointer: " + std::string(name) + " in " + std::string(__FUNCTION__)); \
    }

#endif // STREAMUP_ERROR_HANDLER_HPP
