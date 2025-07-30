#pragma once

#include <QFrame>
#include <QPushButton>
#include <QVBoxLayout>
#include <obs.h>
#include <obs-frontend-api.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class StreamUPDock;
}
QT_END_NAMESPACE

class StreamUPDock : public QFrame {
	Q_OBJECT

public:
	explicit StreamUPDock(QWidget *parent = nullptr);
	~StreamUPDock();

private:
	Ui::StreamUPDock *ui;
	QPushButton *button1;
	QPushButton *button2;
	QPushButton *button3;
	QPushButton *button4;
	QPushButton *button5;
	QPushButton *button6;
	QPushButton *button7;
	QHBoxLayout *mainDockLayout;
	bool isProcessing;

	void applyFileIconToButton(QPushButton *button, const QString &filePath);

	void ButtonToggleLockAllSources();
	void ButtonToggleLockSourcesInCurrentScene();
	void ButtonRefreshAudioMonitoring();
	void ButtonRefreshBrowserSources();
	void ButtonActivateAllVideoCaptureDevices();
	void ButtonDeactivateAllVideoCaptureDevices();
	void ButtonRefreshAllVideoCaptureDevices();
	void updateButtonIcons();

	bool AreAllSourcesLockedInAllScenes();
	bool AreAllSourcesLockedInCurrentScene();

	void setupObsSignals();
	void connectSceneSignals();
	void disconnectSceneSignals();

	static void onFrontendEvent(enum obs_frontend_event event, void *private_data);
	static void onSceneItemAdded(void *param, calldata_t *data);
	static void onSceneItemRemoved(void *param, calldata_t *data);
	static void onItemLockChanged(void *param, calldata_t *data);
};
