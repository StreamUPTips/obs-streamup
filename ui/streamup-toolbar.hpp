#pragma once

#include <QToolBar>
#include <QToolButton>
#include <QFrame>
#include <obs.h>
#include <obs-frontend-api.h>

class StreamUPToolbar : public QToolBar {
    Q_OBJECT

public:
    explicit StreamUPToolbar(QWidget *parent = nullptr);
    ~StreamUPToolbar();

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

private:
    void setupUI();
    void updateStreamButton();
    void updateRecordButton();
    void updatePauseButton();
    void updateReplayBufferButton();
    void updateSaveReplayButton();
    void updateVirtualCameraButton();
    void updateStudioModeButton();
    void updateAllButtons();
    void updateButtonVisibility();
    QFrame* createSeparator();
    
    // Helper functions to check button availability
    bool isReplayBufferAvailable();
    bool isRecordingPausable();
    
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
};