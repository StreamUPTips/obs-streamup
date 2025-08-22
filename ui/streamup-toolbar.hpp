#pragma once

#include <QToolBar>
#include <QToolButton>
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
    void onVirtualCameraButtonClicked();
    void onStudioModeButtonClicked();
    void onSettingsButtonClicked();

private:
    void setupUI();
    void updateStreamButton();
    void updateRecordButton();
    void updatePauseButton();
    void updateReplayBufferButton();
    void updateVirtualCameraButton();
    void updateStudioModeButton();
    void updateAllButtons();
    
    QToolButton* streamButton;
    QToolButton* recordButton;
    QToolButton* pauseButton;
    QToolButton* replayBufferButton;
    QToolButton* virtualCameraButton;
    QToolButton* studioModeButton;
    QToolButton* settingsButton;
};