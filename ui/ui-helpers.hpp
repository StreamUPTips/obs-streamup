#ifndef STREAMUP_UI_HELPERS_HPP
#define STREAMUP_UI_HELPERS_HPP

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLayout>
#include <QStyle>
#include <QString>
#include <QWidget>
#include <functional>

namespace StreamUP {
namespace UIHelpers {

//-------------------DIALOG CREATION FUNCTIONS-------------------
/**
 * Execute a dialog function on the UI thread
 * @param dialogFunction The function to execute on the UI thread
 */
void ShowDialogOnUIThread(const std::function<void()> &dialogFunction);

/**
 * Create a standard dialog window with common properties
 * @param windowTitle The window title (obs_module_text key)
 * @param parentWidget Optional parent widget for positioning (defaults to OBS main window)
 * @return QDialog* Pointer to the created dialog
 */
QDialog *CreateDialogWindow(const char *windowTitle, QWidget *parentWidget = nullptr);

/**
 * Center a dialog on its parent widget or screen
 * @param dialog The dialog to center
 * @param parentWidget The parent widget to center on (nullptr for screen center)
 */
void CenterDialog(QDialog *dialog, QWidget *parentWidget = nullptr);

/**
 * Show a dialog with proper positioning after a brief delay to ensure proper sizing
 * @param dialog The dialog to show and center
 * @param parentWidget Optional parent widget for positioning
 */
void ShowDialogCentered(QDialog *dialog, QWidget *parentWidget = nullptr);

/**
 * Create a tool dialog with multiple information sections
 * @param infoText1 First information text (obs_module_text key)
 * @param infoText2 Second information text (obs_module_text key) 
 * @param infoText3 Third information text (obs_module_text key)
 * @param titleText Dialog title
 * @param iconType Icon to display in dialog
 */
void CreateToolDialog(const char *infoText1, const char *infoText2, const char *infoText3, const QString &titleText,
		      const QStyle::StandardPixmap &iconType);

//-------------------LABEL CREATION FUNCTIONS-------------------
/**
 * Create a rich text label with formatting options
 * @param text The text content
 * @param bold Whether text should be bold
 * @param wrap Whether text should wrap
 * @param alignment Text alignment (optional)
 * @return QLabel* Pointer to the created label
 */
QLabel *CreateRichTextLabel(const QString &text, bool bold, bool wrap, Qt::Alignment alignment = Qt::Alignment(), bool roundedBackground = false);

/**
 * Create a label with an icon
 * @param iconName The standard icon to display
 * @return QLabel* Pointer to the created icon label
 */
QLabel *CreateIconLabel(const QStyle::StandardPixmap &iconName);

//-------------------LAYOUT CREATION FUNCTIONS-------------------
/**
 * Create a horizontal layout with icon and text
 * @param iconText The icon to display
 * @param labelText The text to display (obs_module_text key)
 * @return QHBoxLayout* Pointer to the created layout
 */
QHBoxLayout *AddIconAndText(const QStyle::StandardPixmap &iconText, const char *labelText);

/**
 * Create a vertical box layout with standard margins
 * @param parent Parent widget
 * @return QVBoxLayout* Pointer to the created layout
 */
QVBoxLayout *CreateVBoxLayout(QWidget *parent);

//-------------------CONTROL CREATION FUNCTIONS-------------------
/**
 * Create a clickable label with link functionality
 * @param layout The layout to add the label to
 * @param text The label text
 * @param url The URL to open when clicked
 * @param row Grid row position
 * @param column Grid column position
 */
void CreateLabelWithLink(QLayout *layout, const QString &text, const QString &url, int row, int column);

/**
 * Create a button with click handler
 * @param layout The layout to add the button to
 * @param text The button text
 * @param onClick The function to call when clicked
 */
void CreateButton(QLayout *layout, const QString &text, const std::function<void()> &onClick);

//-------------------UTILITY FUNCTIONS-------------------
/**
 * Copy text to system clipboard
 * @param text The text to copy
 */
void CopyToClipboard(const QString &text);

/**
 * Get theme-aware icon path for UI icons
 * @param iconName The base name of the icon (without suffix)
 * @return QString Full resource path to the appropriate light/dark icon
 */
QString GetThemedIconPath(const QString &iconName);

} // namespace UIHelpers  
} // namespace StreamUP

#endif // STREAMUP_UI_HELPERS_HPP