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

// StreamUP Theme Design Variables - These are used for custom UI elements only
// For OBS-native docks and controls, use OBS/Qt theming instead
namespace Colors {
    // Primary Theme Colors - only for custom StreamUP UI
    constexpr const char* PRIMARY_COLOR = "#0076df";
    constexpr const char* PRIMARY_HOVER = "#0071e3";
    constexpr const char* PRIMARY_LIGHT = "#2997ff";
    constexpr const char* PRIMARY_INACTIVE = "#002e5d";
    
    // Note: Background colors should use Qt palette for OBS integration
    // These are preserved for custom floating panels and dialogs only
    constexpr const char* BG_PRIMARY = "#161617";
    constexpr const char* BG_SECONDARY = "#111111";
    constexpr const char* BG_TERTIARY = "#2c2c2e";
    constexpr const char* BG_DARKEST = "#090909";
    
    // Status Colors
    constexpr const char* COLOR_SUCCESS = "#32d74b";
    constexpr const char* COLOR_WARNING = "#ff9f0a";
    constexpr const char* COLOR_DANGER = "#ff453a";
    
    // Interactive State Colors
    constexpr const char* PRIMARY_ALPHA_30 = "rgba(0, 118, 223, 0.3)";
    constexpr const char* HOVER_OVERLAY = "rgba(60, 60, 67, 0.6)";
    constexpr const char* MENU_OVERLAY = "rgba(28, 28, 30, 0.95)";
    constexpr const char* MENU_BACKDROP = "rgba(0, 0, 0, 0.35)";
    constexpr const char* POPUP_OVERLAY = "rgba(0, 0, 0, 0.95)";
    
    // Border & Surface Colors
    constexpr const char* BORDER_SUBTLE = "rgba(255, 255, 255, 0.08)";
    constexpr const char* BORDER_SOFT = "rgba(255, 255, 255, 0.10)";
    constexpr const char* BORDER_MEDIUM = "rgba(255, 255, 255, 0.15)";
    constexpr const char* BORDER_HOVER = "rgba(255, 255, 255, 0.14)";
    constexpr const char* BORDER_DISABLED = "#002e5d"; // --primary_inactive
    constexpr const char* SURFACE_SUBTLE = "rgba(255, 255, 255, 0.06)";
    
    // Text Colors
    constexpr const char* TEXT_PRIMARY = "#ffffff";
    constexpr const char* TEXT_SECONDARY = "#c7c7cc";
    constexpr const char* TEXT_MUTED = "#8e8e93";
    constexpr const char* TEXT_DISABLED = "rgba(255, 255, 255, 0.4)";
    constexpr const char* TEXT_PLACEHOLDER = "rgba(255, 255, 255, 0.35)";
    
    // Legacy aliases for backward compatibility
    constexpr const char* BACKGROUND_DARK = BG_DARKEST;
    constexpr const char* BACKGROUND_CARD = BG_PRIMARY;
    constexpr const char* BACKGROUND_INPUT = BG_SECONDARY;
    constexpr const char* BACKGROUND_HOVER = BG_TERTIARY;
    constexpr const char* SUCCESS = COLOR_SUCCESS;
    constexpr const char* SUCCESS_HOVER = COLOR_SUCCESS; // Use same for now
    constexpr const char* ERROR = COLOR_DANGER;
    constexpr const char* ERROR_HOVER = COLOR_DANGER;
    constexpr const char* WARNING = COLOR_WARNING;
    constexpr const char* WARNING_HOVER = COLOR_WARNING;
    constexpr const char* INFO = PRIMARY_COLOR;
    constexpr const char* INFO_HOVER = PRIMARY_HOVER;
    constexpr const char* NEUTRAL = BG_TERTIARY;
    constexpr const char* NEUTRAL_HOVER = HOVER_OVERLAY;
    constexpr const char* BORDER_LIGHT = BORDER_MEDIUM;
}

// StreamUP Theme Spacing & Size Variables
namespace Sizes {
    // Spacing Scale (from StreamUP theme)
    constexpr int SPACE_1 = 1;
    constexpr int SPACE_2 = 2;
    constexpr int SPACE_4 = 4;
    constexpr int SPACE_6 = 6;
    constexpr int SPACE_8 = 8;
    constexpr int SPACE_10 = 10;
    constexpr int SPACE_12 = 12;
    constexpr int SPACE_14 = 14;
    constexpr int SPACE_16 = 16;
    constexpr int SPACE_20 = 20;
    constexpr int SPACE_22 = 22;
    constexpr int SPACE_24 = 24;
    constexpr int SPACE_28 = 28;
    
    // Radius Scale (from StreamUP theme)
    constexpr int RADIUS_XS = 6;
    constexpr int RADIUS_SM = 8;
    constexpr int RADIUS_MD = 10;
    constexpr int RADIUS_LG = 14;
    constexpr int RADIUS_XL = 16;
    constexpr int RADIUS_DOCK = 22;
    
    // Component-specific radius
    constexpr int BUTTON_PILL_RADIUS = 10;
    constexpr int MENU_ITEM_PILL_RADIUS = 6;
    constexpr int LIST_ITEM_PILL_RADIUS = 5;
    constexpr int RADIUS_BUTTON = 10;
    constexpr int RADIUS_BUTTON_CIRCLE = 8;
    constexpr int RADIUS_MENU_ITEM = 6;
    constexpr int ICON_BUTTON_RADIUS = 11;
    constexpr int RADIUS_GROUPBOX = 32;  // Increased rounded corners for dialog group boxes
    
    // Component Heights & Sizes
    constexpr int BUTTON_HEIGHT = 20;
    constexpr int BUTTON_BORDER_SIZE = 2;
    constexpr int BUTTON_CIRCLE_SIZE = 16;
    constexpr int BOX_HEIGHT = 20;
    constexpr int BOX_MARGIN = 4;
    constexpr int INPUT_HEIGHT = 20;
    constexpr int MENU_ITEM_HEIGHT = 28;
    constexpr int STATUSBAR_HEIGHT = 22;
    
    // Icon & Button Sizes
    constexpr int ICON_BUTTON_SIZE = 16;
    constexpr int MIXER_BUTTON_SIZE = 16;
    constexpr int MIXER_BUTTON_RADIUS = 14;
    constexpr int MIXER_ICON_SIZE = 12;
    constexpr int PREVIEW_ZOOM_SIZE = 14;
    constexpr int PREVIEW_ZOOM_SIZE_SM = 12;
    
    // Padding Values
    constexpr int BUTTON_CIRCLE_PADDING_X = 4;
    constexpr int BUTTON_CIRCLE_PADDING_Y = 2;
    constexpr int INPUT_PAD_Y = 2;
    constexpr int INPUT_PAD_X = 2;
    constexpr int INPUT_PAD_X_WITH_ICON = 28;
    constexpr int LIST_ITEM_PAD_Y = 2;
    constexpr int LIST_ITEM_PAD_X = 8;
    constexpr int MENU_PAD_Y = 8;
    constexpr int MENU_PAD_X = 8;
    constexpr int MENU_ITEM_PAD_Y = 6;
    constexpr int MENU_ITEM_PAD_X = 16;
    constexpr int STATUSBAR_PAD_X = 6;
    constexpr int STATUS_ITEM_PAD_X = 8;
    constexpr int TOOLBAR_PAD_X = 14;
    
    // Dialog & Settings Sizes
    constexpr int DIALOG_MARGIN = 20;
    constexpr int DIALOG_PADDING = 24;
    constexpr int DIALOG_RADIUS = 16;
    constexpr int DIALOG_TITLE_HEIGHT = 40;
    constexpr int DIALOG_CONTENT_SPACING = 16;
    constexpr int DIALOG_BUTTON_SPACING = 12;
    constexpr int SETTINGS_GROUP_PADDING = 20;
    constexpr int SETTINGS_GROUP_RADIUS = 12;
    constexpr int SETTINGS_LABEL_WIDTH = 180;
    constexpr int SETTINGS_CONTROL_SPACING = 12;
    constexpr int SETTINGS_ROW_HEIGHT = 32;
    
    // Scrollbar Sizes
    constexpr int SCROLLBAR_SIZE = 8;
    constexpr int SCROLLBAR_HANDLE_MIN_SIZE = 20;
    
    // Slider Sizes
    constexpr int SLIDER_GROOVE_SIZE = 4;
    constexpr int SLIDER_HANDLE_SIZE = 16;
    
    // Typography
    constexpr int FONT_SIZE_LARGE = 16;
    constexpr int FONT_SIZE_MEDIUM = 14;
    constexpr int FONT_SIZE_NORMAL = 14;
    constexpr int FONT_SIZE_SMALL = 13;
    constexpr int FONT_SIZE_TINY = 12;
    constexpr int FONT_WEIGHT_NORMAL = 500; // StreamUP theme uses 500
    constexpr int FONT_WEIGHT_BOLD = 800;   // StreamUP theme uses 800
    
    // Legacy aliases for backward compatibility
    constexpr int SPACING_XXS = SPACE_2;
    constexpr int SPACING_TINY = SPACE_4;
    constexpr int SPACING_SMALL = SPACE_8;
    constexpr int SPACING_MEDIUM = SPACE_12;
    constexpr int SPACING_LARGE = SPACE_16;
    constexpr int SPACING_XL = SPACE_20;
    constexpr int SPACING_XXL = SPACE_24;
    constexpr int PADDING_SMALL = SPACE_8;
    constexpr int PADDING_MEDIUM = SPACE_12;
    constexpr int PADDING_LARGE = SPACE_16;
    constexpr int PADDING_XL = SPACE_20;
    constexpr int BORDER_RADIUS = RADIUS_XS;
    constexpr int BORDER_RADIUS_LARGE = RADIUS_SM;
    constexpr int BORDER_RADIUS_XL = RADIUS_LG;
    constexpr int BORDER_WIDTH = BUTTON_BORDER_SIZE;
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

// New StreamUP theme style functions
QString GetLineEditStyle();
QString GetCheckBoxStyle();
QString GetComboBoxStyle();
QString GetSpinBoxStyle();
QString GetSliderStyle();
QString GetListWidgetStyle();
QString GetMenuStyle();
QString GetEnhancedScrollAreaStyle();

// Apply global StreamUP theme styles to widgets
void ApplyStreamUPThemeStyles(QWidget* widget);

// OBS/Qt-compliant styling functions (preferred for new code)
// These use OBS theme colors and Qt palette system
QString GetOBSCompliantDialogStyle();
QString GetOBSCompliantButtonStyle(const QString& variant = "default");
QString GetOBSCompliantGroupBoxStyle();
QString GetMinimalNativeStyle(); // Lets Qt/OBS handle all theming
void ApplyOBSNativeTheming(QWidget* widget);
bool IsOBSThemeDark(); // Wrapper for obs_frontend_is_theme_dark()

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
void ApplyAutoSizing(QDialog* dialog, int minWidth = 700, int maxWidth = 900, int minHeight = 150, int maxHeight = 750);
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