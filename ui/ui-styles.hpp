#ifndef STREAMUP_UI_STYLES_HPP
#define STREAMUP_UI_STYLES_HPP

#include <QString>

class QDialog;
class QLabel;
class QPushButton;
class QGroupBox;
class QScrollArea;
class QWidget;
class QVBoxLayout;

#include <functional>

namespace StreamUP {
namespace UIStyles {

// Color constants
namespace Colors {
    constexpr const char* BACKGROUND_DARK = "#13171f";
    constexpr const char* BACKGROUND_CARD = "#1f2937";
    constexpr const char* BACKGROUND_INPUT = "#2d3748";
    constexpr const char* BACKGROUND_HOVER = "#374151";
    
    constexpr const char* BORDER_LIGHT = "#718096";
    constexpr const char* BORDER_SUCCESS = "#22c55e";
    constexpr const char* BORDER_ERROR = "#ef4444";
    constexpr const char* BORDER_WARNING = "#f59e0b";
    constexpr const char* BORDER_INFO = "#3b82f6";
    
    constexpr const char* TEXT_PRIMARY = "#ffffff";
    constexpr const char* TEXT_SECONDARY = "#f3f4f6";
    constexpr const char* TEXT_MUTED = "#9ca3af";
    constexpr const char* TEXT_DISABLED = "#6b7280";
    
    constexpr const char* SUCCESS = "#22c55e";
    constexpr const char* SUCCESS_HOVER = "#16a34a";
    constexpr const char* ERROR = "#ef4444";
    constexpr const char* ERROR_HOVER = "#dc2626";
    constexpr const char* WARNING = "#f59e0b";
    constexpr const char* WARNING_HOVER = "#d97706";
    constexpr const char* INFO = "#3b82f6";
    constexpr const char* INFO_HOVER = "#2563eb";
    constexpr const char* NEUTRAL = "#64748b";
    constexpr const char* NEUTRAL_HOVER = "#475569";
}

// Size constants
namespace Sizes {
    constexpr int BORDER_RADIUS = 8;
    constexpr int BORDER_RADIUS_LARGE = 10;
    constexpr int BORDER_WIDTH = 2;
    
    constexpr int FONT_SIZE_LARGE = 20;
    constexpr int FONT_SIZE_MEDIUM = 16;
    constexpr int FONT_SIZE_NORMAL = 14;
    constexpr int FONT_SIZE_SMALL = 13;
    constexpr int FONT_SIZE_TINY = 12;
    
    constexpr int SPACING_TINY = 5;
    constexpr int SPACING_SMALL = 10;
    constexpr int SPACING_MEDIUM = 15;
    constexpr int SPACING_LARGE = 20;
    constexpr int SPACING_XL = 25;
    
    constexpr int PADDING_SMALL = 8;
    constexpr int PADDING_MEDIUM = 12;
    constexpr int PADDING_LARGE = 15;
    constexpr int PADDING_XL = 20;
}

// Common style strings
QString GetDialogStyle();
QString GetTitleLabelStyle();
QString GetDescriptionLabelStyle();
QString GetGroupBoxStyle(const QString& borderColor, const QString& titleColor);
QString GetContentLabelStyle();
QString GetButtonStyle(const QString& baseColor, const QString& hoverColor, int height = 30);
QString GetSquircleButtonStyle(const QString& baseColor, const QString& hoverColor, int size = 32);
QString GetScrollAreaStyle();
QString GetTableStyle();

// Utility functions for creating styled components
QDialog* CreateStyledDialog(const QString& title, QWidget* parentWidget = nullptr);
QLabel* CreateStyledTitle(const QString& text);
QLabel* CreateStyledDescription(const QString& text);
QLabel* CreateStyledContent(const QString& text);
QPushButton* CreateStyledButton(const QString& text, const QString& type = "neutral", int height = 30, int minWidth = 0);
QPushButton* CreateStyledSquircleButton(const QString& text, const QString& type = "neutral", int size = 32);
QGroupBox* CreateStyledGroupBox(const QString& title, const QString& type = "info");
QScrollArea* CreateStyledScrollArea();

// Auto-sizing utilities
void ApplyAutoSizing(QDialog* dialog, int minWidth = 700, int maxWidth = 900, int minHeight = 150, int maxHeight = 600);
void ApplyContentBasedSizing(QDialog* dialog);
void ApplyScrollAreaContentSizing(QScrollArea* scrollArea, int maxHeight = 300);
void ApplyDynamicSizing(QDialog* dialog, int minWidth = 500, int maxWidth = 900, int minHeight = 400, int maxHeight = 650);
void ApplyConsistentSizing(QDialog* dialog, int preferredWidth = 580, int maxWidth = 900, int minHeight = 400, int maxHeight = 750);

// Standardized dialog template system
struct StandardDialogComponents {
    QDialog* dialog;
    QWidget* headerWidget;
    QLabel* titleLabel;
    QLabel* subtitleLabel;
    QScrollArea* scrollArea;
    QWidget* contentWidget;
    QVBoxLayout* contentLayout;
    QPushButton* mainButton;
};

StandardDialogComponents CreateStandardDialog(const QString& windowTitle, const QString& headerTitle, const QString& headerDescription, QWidget* parentWidget = nullptr);
void UpdateDialogHeader(StandardDialogComponents& components, const QString& title, const QString& description);
void SetupDialogNavigation(StandardDialogComponents& components, std::function<void()> onMainButton);

} // namespace UIStyles
} // namespace StreamUP

#endif // STREAMUP_UI_STYLES_HPP