#pragma once

#include <QToolBar>
#include <QToolButton>
#include <QFrame>
#include <QBoxLayout>
#include <QMargins>
#include <QMenu>
#include <QAction>
#include <QHash>
#include <QIcon>
#include <QTimer>
#include <obs.h>
#include <obs-frontend-api.h>
#include "streamup-toolbar-config.hpp"
#include "streamup-toolbar-editor.hpp"
#include "settings-manager.hpp"

namespace StreamUP {
class ToolbarEditPanel;
}

class StreamUPToolbar : public QToolBar {
    Q_OBJECT

public:
    explicit StreamUPToolbar(QWidget *parent = nullptr);
    ~StreamUPToolbar();
    
    // Public methods for external access
    void updatePositionAwareTheme();
    void refreshFromConfiguration();
    void updateButtonSizes();  // Update button and icon sizes without rebuilding
    void refreshSizeClass();   // Re-read toolbar size setting and re-polish styles
    void refreshAlignment();   // Re-read toolbar alignment setting and re-lay out

    // Edit mode turns the bar itself into the editing surface: the live run is
    // swapped for a ToolbarEditor and swapped back when you finish. There is
    // only ever one laid-out run, so nothing can drift out of step with it.
    void setEditMode(bool enabled);
    bool isEditModeActive() const { return editModeActive; }

private slots:
    void onStreamButtonClicked();
    void onRecordButtonClicked();
    void onPauseButtonClicked();
    void onReplayBufferButtonClicked();
    void onSaveReplayButtonClicked();
    void onVirtualCameraButtonClicked();
    void onVirtualCameraConfigButtonClicked();
    void onStudioModeButtonClicked();
    void onSettingsButtonClicked();
    void onStreamUPSettingsButtonClicked();
    void onEditToolbarClicked();
    void onToolbarSettingsClicked();
    void onDockButtonClicked();
    void onHotkeyButtonClicked();
    void onWebSocketButtonClicked();

private:
    void setupDynamicUI();
    void clearLayout();
    QToolButton* createButtonFromConfig(std::shared_ptr<StreamUP::ToolbarConfig::ToolbarItem> item);
    void executeDockAction(const QString& actionType);
    void executeDockActionWithButton(const QString& actionType, QToolButton* button);
    void showToolbarContextMenu(const QPoint& position);
    void updateStreamButton();
    void updateRecordButton();
    void updatePauseButton();
    void updateReplayBufferButton();
    void updateSaveReplayButton();
    void updateVirtualCameraButton();
    void updateVirtualCameraConfigButton();
    void updateStudioModeButton();
    void updateSettingsButton();
    void updateStreamUPSettingsButton();
    void updateDockButtonIcons();
    void updateAllButtons();
    void updateButtonVisibility();
    void updateIconsForTheme();
    void updateLayoutOrientation();
    // Driven by QToolBar::orientationChanged, which carries the new orientation.
    // toolBarArea() is not reliably updated at that moment.
    void onOrientationChanged(bool vertical);
    void repositionEditPanel();
    // Tells the theme which window edge the toolbar is docked to, so the main
    // window's inset can be dropped on that side.
    void reportDockedEdge();
    // Pins QToolBar's own layout margin, which the stylesheet cannot reach.
    void applyLayoutMargins();

    // True when the toolbar is (or is about to be) docked to the left or right
    // edge. Falls back to the saved position setting while the toolbar has not
    // yet been handed to the main window, which is the case for the whole of
    // the constructor, where toolBarArea() cannot answer for us.
    bool isVerticalOrientation() const;
    // Current alignment tier from settings (Start / Centre / End)
    StreamUP::SettingsManager::ToolbarAlignment currentAlignment() const;

    
    // Helper functions to check button availability
    bool isReplayBufferAvailable();
    bool isRecordingPausable();

    // Invokes an OBSBasic action slot by name so the toolbar inherits OBS's
    // own confirmation dialogs and pre-flight checks. Returns false if the
    // slot could not be found.
    bool invokeMainWindowAction(const char* slotName);

    // Theme-aware icon helper
    QString getThemedIconPath(const QString& iconName);

    // Enhanced cached icon loading system
    QIcon getCachedIcon(const QString& iconName);
    void clearIconCache();
    void preloadCommonIcons(); // Preload frequently used icons

    // Optimized update system
    void scheduleUpdate();
    void processBatchedUpdates();
    void updateButtonStatesEfficiently();
    
    // Apply theme-aware styling
    void updateToolbarStyling();

    // Update toolbar size constraints based on icon size and orientation
    void updateToolbarSizeConstraints();

    // Apply the user-selected size tier as a Qt dynamic property so the OBS
    // theme can react via [size="small|medium|large"] selectors. Pixel sizes
    // and chrome scaling are owned by the theme — the plugin only signals.
    void applySizeClass();

    // Flag to prevent updates during UI reconstruction
    bool isReconstructingUI = false;
    
    // Enhanced icon cache for performance optimization
    QHash<QString, QIcon> iconCache;
    bool currentThemeIsDark = false;  // Track theme for cache invalidation
    mutable QTimer* iconUpdateTimer; // Debounce icon updates


    // Stylesheet cache for performance optimization
    QString cachedStyleSheet;
    bool styleSheetCacheValid = false;
    void clearStyleSheetCache();

    // Update batching system
    bool m_updatesPending = false;
    QTimer* m_updateBatchTimer;
    
    // OBS event handling
    static void OnFrontendEvent(enum obs_frontend_event event, void *data);
    
    QToolButton* streamButton;
    QToolButton* recordButton;
    QToolButton* pauseButton;
    QToolButton* replayBufferButton;
    QToolButton* saveReplayButton;
    QToolButton* virtualCameraButton;
    QToolButton* virtualCameraConfigButton;
    QToolButton* studioModeButton;
    QToolButton* settingsButton;
    QToolButton* streamUPSettingsButton;
    
    // Layout management for orientation changes
    QWidget* centralWidget;
    QBoxLayout* mainLayout;
    
    // Configuration system
    StreamUP::ToolbarConfig::ToolbarConfiguration toolbarConfig;
    QMap<QString, QToolButton*> dynamicButtons; // Maps item ID to button
    QMenu* contextMenu;
    QAction* editToolbarAction = nullptr;
    QAction* toolbarSettingsAction;

    StreamUP::ToolbarEditor* editor = nullptr;
    StreamUP::ToolbarEditPanel* editPanel = nullptr;
    bool editModeActive = false;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
};
