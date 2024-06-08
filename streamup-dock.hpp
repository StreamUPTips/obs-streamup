#pragma once

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>

QT_BEGIN_NAMESPACE
namespace Ui {
class StreamupDock;
}
QT_END_NAMESPACE

class StreamupDock : public QWidget {
	Q_OBJECT

public:
	explicit StreamupDock(QWidget *parent = nullptr);
	~StreamupDock();

private:
	Ui::StreamupDock *ui;

private slots:
	void onButton1Clicked(); // ToggleLockAllSources
	void onButton2Clicked(); // ToggleLockSourcesInCurrentScene
	void onButton3Clicked(); // RefreshAudioMonitoring
	void onButton4Clicked(); // RefreshBrowserSources
};
