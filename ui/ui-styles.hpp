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
class QTableWidget;
class QPoint;

#include <functional>

namespace StreamUP {
namespace UIStyles {

// Minimal color constants for compilation compatibility
// These should eventually be replaced with theme-aware solutions
namespace Colors {
    // Basic colors - these will be overridden by StreamUP theme anyway
    constexpr const char* BACKGROUND_DARK = "transparent";
    constexpr const char* BACKGROUND_CARD = "transparent";
    constexpr const char* BACKGROUND_INPUT = "transparent";
    constexpr const char* BACKGROUND_HOVER = "transparent";
    constexpr const char* TEXT_PRIMARY = "";
    constexpr const char* TEXT_SECONDARY = "";
    constexpr const char* TEXT_MUTED = "";
    constexpr const char* SUCCESS = "";
    constexpr const char* SUCCESS_HOVER = "";
    constexpr const char* ERROR = "";
    constexpr const char* ERROR_HOVER = "";
    constexpr const char* WARNING = "";
    constexpr const char* WARNING_HOVER = "";
    constexpr const char* INFO = "";
    constexpr const char* INFO_HOVER = "";
    constexpr const char* NEUTRAL = "";
    constexpr const char* NEUTRAL_HOVER = "";
    constexpr const char* BORDER_LIGHT = "";
    constexpr const char* BORDER_DISABLED = "";
    constexpr const char* TEXT_DISABLED = "";
}

// Minimal size constants for compilation compatibility
namespace Sizes {
    // Basic sizes - these will be overridden by StreamUP theme anyway
    constexpr int FONT_SIZE_LARGE = 16;
    constexpr int FONT_SIZE_MEDIUM = 14;
    constexpr int FONT_SIZE_NORMAL = 13;
    constexpr int FONT_SIZE_SMALL = 12;
    constexpr int FONT_SIZE_TINY = 11;
    constexpr int FONT_WEIGHT_NORMAL = 400;
    constexpr int FONT_WEIGHT_BOLD = 600;
    constexpr int SPACING_TINY = 4;
    constexpr int SPACING_SMALL = 8;
    constexpr int SPACING_MEDIUM = 12;
    constexpr int SPACING_LARGE = 16;
    constexpr int SPACING_XL = 20;
    constexpr int PADDING_SMALL = 6;
    constexpr int PADDING_MEDIUM = 10;
    constexpr int PADDING_LARGE = 14;
    constexpr int PADDING_XL = 18;
    constexpr int BORDER_RADIUS = 6;
    constexpr int BORDER_RADIUS_LARGE = 8;
    constexpr int BORDER_RADIUS_XL = 10;
    constexpr int BORDER_WIDTH = 1;
    constexpr int SPACING_XXS = 2;
    constexpr int SPACING_XXL = 24;
}

// Common style strings - minimal styling that inherits from StreamUP theme
QString GetDialogStyle();
QString GetTitleLabelStyle();
QString GetDescriptionLabelStyle();
QString GetGroupBoxStyle(const QString& borderColor = "", const QString& titleColor = "");
QString GetContentLabelStyle();
QString GetButtonStyle(const QString& baseColor = "", const QString& hoverColor = "", int height = 0);
QString GetSquircleButtonStyle(const QString& baseColor = "", const QString& hoverColor = "", int size = 16);
QString GetScrollAreaStyle();
QString GetTableStyle();

// Utility functions for creating components that inherit from StreamUP theme
QDialog* CreateStyledDialog(const QString& title, QWidget* parentWidget = nullptr);
QLabel* CreateStyledTitle(const QString& text);
QLabel* CreateStyledDescription(const QString& text);
QLabel* CreateStyledContent(const QString& text);
QPushButton* CreateStyledButton(const QString& text, const QString& type = "neutral", int height = 0, int minWidth = 0);
QPushButton* CreateStyledSquircleButton(const QString& text, const QString& type = "neutral", int size = 16);
QGroupBox* CreateStyledGroupBox(const QString& title, const QString& type = "info");
QScrollArea* CreateStyledScrollArea();

// Table utilities - inherit from StreamUP theme styling
QTableWidget* CreateStyledTableWidget(QWidget* parent = nullptr);
QTableWidget* CreateStyledTable(const QStringList& headers, QWidget* parent = nullptr);
void AutoResizeTableColumns(QTableWidget* table);
void HandleTableCellClick(QTableWidget* table, int row, int column);

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