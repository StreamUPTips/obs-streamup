#pragma once

#include <QToolBar>
#include <QToolButton>
#include <QFrame>
#include <QBoxLayout>
#include <QMenu>
#include <QAction>
#include <QHash>
#include <QIcon>
#include <QTimer>
#include <obs.h>
#include <obs-frontend-api.h>
#include "streamup-toolbar-config.hpp"

class StreamUPToolbar : public QToolBar {
    Q_OBJECT

public:
    explicit StreamUPToolbar(QWidget *parent = nullptr);
    ~StreamUPToolbar();
    
    // Public methods for external access
    void updatePositionAwareTheme();
    void refreshFromConfiguration();

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
    void onConfigureToolbarClicked();
    void onToolbarSettingsClicked();
    void onDockButtonClicked();
    void onHotkeyButtonClicked();

private:
    void setupDynamicUI();
    void clearLayout();
    QToolButton* createButtonFromConfig(std::shared_ptr<StreamUP::ToolbarConfig::ToolbarItem> item);
    QFrame* createSeparatorFromConfig(bool isVertical);
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
    QFrame* createSeparator();
    QFrame* createHorizontalSeparator();
    
    // Helper functions to check button availability
    bool isReplayBufferAvailable();
    bool isRecordingPausable();

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
    QAction* configureAction;
    QAction* toolbarSettingsAction;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
};