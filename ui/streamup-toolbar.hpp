#pragma once

#include <QToolBar>
#include <QToolButton>
#include <QFrame>
#include <QBoxLayout>
#include <QMenu>
#include <QAction>
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
    void onDockButtonClicked();

private:
    void setupUI();
    void setupDynamicUI();
    void clearLayout();
    QToolButton* createButtonFromConfig(std::shared_ptr<StreamUP::ToolbarConfig::ToolbarItem> item);
    QFrame* createSeparatorFromConfig(bool isVertical);
    void executeDockAction(const QString& actionType);
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
    
    // Apply theme-aware styling
    void updateToolbarStyling();
    
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

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
};