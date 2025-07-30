#pragma once

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QPoint>

class VideoCapturePopup : public QWidget {
	Q_OBJECT

public:
	explicit VideoCapturePopup(QWidget *parent = nullptr);
	void showNearButton(const QPoint &buttonPos, const QSize &buttonSize);

private slots:
	void onActivateClicked();
	void onDeactivateClicked();
	void onRefreshClicked();

private:
	QPushButton *activateButton;
	QPushButton *deactivateButton;
	QPushButton *refreshButton;
	QVBoxLayout *layout;
	bool isProcessing;

	void applyFileIconToButton(QPushButton *button, const QString &filePath);

signals:
	void activateAllVideoCaptureDevices();
	void deactivateAllVideoCaptureDevices();
	void refreshAllVideoCaptureDevices();
};