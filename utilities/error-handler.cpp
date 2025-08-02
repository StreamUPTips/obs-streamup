#include "error-handler.hpp"
#include "ui-helpers.hpp"
#include <obs-module.h>
#include <QFile>
#include <QFileInfo>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QGroupBox>
#include <QObject>
#include <unordered_map>
#include <vector>

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
    Log(Severity::Info, category, message);
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

ErrorResult NetworkError(const std::string& message) {
    return Failure(message, Category::Network, Severity::Error);
}

ErrorResult FileSystemError(const std::string& message) {
    return Failure(message, Category::FileSystem, Severity::Error);
}

ErrorResult PluginError(const std::string& message) {
    return Failure(message, Category::Plugin, Severity::Error);
}

ErrorResult SourceError(const std::string& message) {
    return Failure(message, Category::Source, Severity::Error);
}

ErrorResult UIError(const std::string& message) {
    return Failure(message, Category::UI, Severity::Error);
}

// Dialog display functions
void ShowErrorDialog(const std::string& title, const std::string& message) {
    StreamUP::UIHelpers::ShowDialogOnUIThread([title, message]() {
        QDialog* dialog = StreamUP::UIHelpers::CreateDialogWindow(title.c_str());
        dialog->setStyleSheet("QDialog { background: #13171f; }");
        dialog->resize(500, 300);
        dialog->setMinimumSize(450, 250);
        
        QVBoxLayout* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(25, 20, 25, 20);
        layout->setSpacing(20);
        
        // Header section
        QLabel* titleLabel = new QLabel(QString::fromStdString(title));
        titleLabel->setStyleSheet(
            "QLabel {"
            "color: white;"
            "font-size: 20px;"
            "font-weight: bold;"
            "margin: 0px 0px 15px 0px;"
            "padding: 0px;"
            "}");
        titleLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(titleLabel);
        
        // Message section with red border for errors
        QGroupBox* messageGroup = new QGroupBox();
        messageGroup->setStyleSheet(
            "QGroupBox {"
            "border: 2px solid #ef4444;"
            "border-radius: 10px;"
            "padding-top: 15px;"
            "background: #1f2937;"
            "}");
        
        QVBoxLayout* messageLayout = new QVBoxLayout(messageGroup);
        messageLayout->setContentsMargins(15, 15, 15, 15);
        messageLayout->addStretch();
        
        QLabel* messageLabel = new QLabel(QString::fromStdString(message));
        messageLabel->setStyleSheet(
            "QLabel {"
            "color: #f3f4f6;"
            "font-size: 14px;"
            "margin: 0px;"
            "padding: 0px;"
            "background: transparent;"
            "border: none;"
            "text-align: center;"
            "}");
        messageLabel->setWordWrap(true);
        messageLabel->setAlignment(Qt::AlignCenter);
        messageLayout->addWidget(messageLabel, 0, Qt::AlignCenter);
        messageLayout->addStretch();
        layout->addWidget(messageGroup);
        
        // Button section
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        
        QPushButton* okButton = new QPushButton("OK");
        okButton->setStyleSheet(
            "QPushButton {"
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #64748b, stop:1 #475569);"
            "color: white;"
            "border: none;"
            "padding: 12px 24px;"
            "font-size: 14px;"
            "font-weight: bold;"
            "border-radius: 6px;"
            "min-width: 100px;"
            "}"
            "QPushButton:hover {"
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #74859b, stop:1 #576579);"
            "}");
        QObject::connect(okButton, &QPushButton::clicked, [dialog]() { dialog->close(); });
        buttonLayout->addWidget(okButton);
        buttonLayout->addStretch();
        
        layout->addLayout(buttonLayout);
        dialog->setLayout(layout);
        dialog->show();
    });
}

void ShowWarningDialog(const std::string& title, const std::string& message) {
    StreamUP::UIHelpers::ShowDialogOnUIThread([title, message]() {
        QDialog* dialog = StreamUP::UIHelpers::CreateDialogWindow(title.c_str());
        dialog->setStyleSheet("QDialog { background: #13171f; }");
        dialog->resize(500, 300);
        dialog->setMinimumSize(450, 250);
        
        QVBoxLayout* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(25, 20, 25, 20);
        layout->setSpacing(20);
        
        // Header section
        QLabel* titleLabel = new QLabel(QString::fromStdString(title));
        titleLabel->setStyleSheet(
            "QLabel {"
            "color: white;"
            "font-size: 20px;"
            "font-weight: bold;"
            "margin: 0px 0px 15px 0px;"
            "padding: 0px;"
            "}");
        titleLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(titleLabel);
        
        // Message section with orange border for warnings
        QGroupBox* messageGroup = new QGroupBox();
        messageGroup->setStyleSheet(
            "QGroupBox {"
            "border: 2px solid #f59e0b;"
            "border-radius: 10px;"
            "padding-top: 15px;"
            "background: #1f2937;"
            "}");
        
        QVBoxLayout* messageLayout = new QVBoxLayout(messageGroup);
        messageLayout->setContentsMargins(15, 15, 15, 15);
        messageLayout->addStretch();
        
        QLabel* messageLabel = new QLabel(QString::fromStdString(message));
        messageLabel->setStyleSheet(
            "QLabel {"
            "color: #f3f4f6;"
            "font-size: 14px;"
            "margin: 0px;"
            "padding: 0px;"
            "background: transparent;"
            "border: none;"
            "text-align: center;"
            "}");
        messageLabel->setWordWrap(true);
        messageLabel->setAlignment(Qt::AlignCenter);
        messageLayout->addWidget(messageLabel, 0, Qt::AlignCenter);
        messageLayout->addStretch();
        layout->addWidget(messageGroup);
        
        // Button section
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        
        QPushButton* okButton = new QPushButton("OK");
        okButton->setStyleSheet(
            "QPushButton {"
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #d69e2e, stop:1 #b45309);"
            "color: white;"
            "border: none;"
            "padding: 12px 24px;"
            "font-size: 14px;"
            "font-weight: bold;"
            "border-radius: 6px;"
            "min-width: 100px;"
            "}"
            "QPushButton:hover {"
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #eab308, stop:1 #ca8a04);"
            "}");
        QObject::connect(okButton, &QPushButton::clicked, [dialog]() { dialog->close(); });
        buttonLayout->addWidget(okButton);
        buttonLayout->addStretch();
        
        layout->addLayout(buttonLayout);
        dialog->setLayout(layout);
        dialog->show();
    });
}

void ShowInfoDialog(const std::string& title, const std::string& message) {
    StreamUP::UIHelpers::ShowDialogOnUIThread([title, message]() {
        QDialog* dialog = StreamUP::UIHelpers::CreateDialogWindow(title.c_str());
        dialog->setStyleSheet("QDialog { background: #13171f; }");
        dialog->resize(500, 300);
        dialog->setMinimumSize(450, 250);
        
        QVBoxLayout* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(25, 20, 25, 20);
        layout->setSpacing(20);
        
        // Header section
        QLabel* titleLabel = new QLabel(QString::fromStdString(title));
        titleLabel->setStyleSheet(
            "QLabel {"
            "color: white;"
            "font-size: 20px;"
            "font-weight: bold;"
            "margin: 0px 0px 15px 0px;"
            "padding: 0px;"
            "}");
        titleLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(titleLabel);
        
        // Message section with blue border for info
        QGroupBox* messageGroup = new QGroupBox();
        messageGroup->setStyleSheet(
            "QGroupBox {"
            "border: 2px solid #3b82f6;"
            "border-radius: 10px;"
            "padding-top: 15px;"
            "background: #1f2937;"
            "}");
        
        QVBoxLayout* messageLayout = new QVBoxLayout(messageGroup);
        messageLayout->setContentsMargins(15, 15, 15, 15);
        messageLayout->addStretch();
        
        QLabel* messageLabel = new QLabel(QString::fromStdString(message));
        messageLabel->setStyleSheet(
            "QLabel {"
            "color: #f3f4f6;"
            "font-size: 14px;"
            "margin: 0px;"
            "padding: 0px;"
            "background: transparent;"
            "border: none;"
            "text-align: center;"
            "}");
        messageLabel->setWordWrap(true);
        messageLabel->setAlignment(Qt::AlignCenter);
        messageLayout->addWidget(messageLabel, 0, Qt::AlignCenter);
        messageLayout->addStretch();
        layout->addWidget(messageGroup);
        
        // Button section
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        
        QPushButton* okButton = new QPushButton("OK");
        okButton->setStyleSheet(
            "QPushButton {"
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3b82f6, stop:1 #2563eb);"
            "color: white;"
            "border: none;"
            "padding: 12px 24px;"
            "font-size: 14px;"
            "font-weight: bold;"
            "border-radius: 6px;"
            "min-width: 100px;"
            "}"
            "QPushButton:hover {"
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #60a5fa, stop:1 #3b82f6);"
            "}");
        QObject::connect(okButton, &QPushButton::clicked, [dialog]() { dialog->close(); });
        buttonLayout->addWidget(okButton);
        buttonLayout->addStretch();
        
        layout->addLayout(buttonLayout);
        dialog->setLayout(layout);
        dialog->show();
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

bool ValidateString(const char* str, const std::string& field_name) {
    if (!str || strlen(str) == 0) {
        LogError("Invalid or empty string for field: " + field_name, Category::General);
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

void ErrorContext::AddDetail(const std::string& detail) {
    m_details.push_back(detail);
    LogInfo("Context detail [" + m_context_name + "]: " + detail, m_category);
}

ErrorResult ErrorContext::CreateError(const std::string& message, Severity severity) {
    std::string full_message = "Error in " + m_context_name + ": " + message;
    
    // Add details if any
    if (!m_details.empty()) {
        full_message += " (Details: ";
        for (size_t i = 0; i < m_details.size(); ++i) {
            if (i > 0) full_message += ", ";
            full_message += m_details[i];
        }
        full_message += ")";
    }
    
    return Failure(full_message, m_category, severity);
}

} // namespace ErrorHandler
} // namespace StreamUP