#pragma once

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLayout>
#include <QStyle>
#include <QString>
#include <QWidget>
#include <QPointer>
#include <QStandardItem>
#include <QAbstractButton>
#include <functional>
#include <unordered_map>
#include <mutex>

namespace StreamUP {
namespace UIHelpers {

//-------------------DIALOG MANAGEMENT-------------------
/**
 * Centralized dialog manager for singleton dialog patterns
 * Handles the common pattern of showing one instance of a dialog at a time
 */
class DialogManager {
public:
    /**
     * Show a dialog using singleton pattern - only one instance at a time
     * @param dialogId Unique identifier for the dialog (e.g., "settings", "patch-notes")
     * @param createFunction Function that creates and configures the dialog
     * @param bringToFront If true, brings existing dialog to front instead of ignoring
     * @return true if new dialog was created, false if existing dialog was found
     */
    static bool ShowSingletonDialog(const std::string& dialogId, 
                                   const std::function<QDialog*()>& createFunction,
                                   bool bringToFront = true);
    
    /**
     * Check if a singleton dialog is currently open
     * @param dialogId The dialog identifier to check
     * @return true if dialog exists and is visible
     */
    static bool IsSingletonDialogOpen(const std::string& dialogId);

private:
    static std::mutex s_dialogsMutex;
    static std::unordered_map<std::string, QPointer<QDialog>> s_dialogs;
};

//-------------------DIALOG CREATION FUNCTIONS-------------------
/**
 * Execute a dialog function on the UI thread
 * @param dialogFunction The function to execute on the UI thread
 */
void ShowDialogOnUIThread(const std::function<void()> &dialogFunction);

/**
 * Show a singleton dialog on the UI thread (combines common patterns)
 * This is the most common dialog pattern in the codebase
 * @param dialogId Unique identifier for the dialog
 * @param createFunction Function that creates the dialog (called on UI thread)
 * @param bringToFront If true, brings existing dialog to front
 */
void ShowSingletonDialogOnUIThread(const std::string& dialogId,
                                  const std::function<QDialog*()>& createFunction,
                                  bool bringToFront = true);

/**
 * Center a dialog on its parent widget or screen
 * @param dialog The dialog to center
 * @param parentWidget The parent widget to center on (nullptr for screen center)
 */
void CenterDialog(QDialog *dialog, QWidget *parentWidget = nullptr);

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

/**
 * Check if OBS is using a dark theme
 * @return bool True if dark theme is active
 */
bool IsOBSThemeDark();

/**
 * Recursively find a QStandardItem by text and type in a tree model
 * @param parent The parent item to search from (use invisibleRootItem() for full tree)
 * @param text The text to search for
 * @param itemType The type of item to match (use QStandardItem::UserType + offset)
 * @return QStandardItem* Pointer to found item or nullptr if not found
 */
QStandardItem* FindItemRecursive(QStandardItem* parent, const QString& text, int itemType);

/**
 * Puts a top-level window (a dialog, a floating dock) centred over the OBS main
 * window, kept inside the screen OBS is on. With onlyWhenElsewhere it leaves the
 * window alone when its centre is already on OBS's screen and fully reachable,
 * so a window the user placed there stays put.
 */
void PlaceOverMainWindow(QWidget *window, bool onlyWhenElsewhere = false);

} // namespace UIHelpers
} // namespace StreamUP
