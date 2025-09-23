#pragma once

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QPoint>
#include <QEvent>
#include <QFocusEvent>
#include <QPaintEvent>

class VideoCapturePopup : public QWidget {
	Q_OBJECT

public:
	explicit VideoCapturePopup(QWidget *parent = nullptr);
	~VideoCapturePopup();
	void showNearButton(const QPoint &buttonPos, const QSize &buttonSize);
	void updateIconsForTheme();

private slots:
	void onActivateClicked();
	void onDeactivateClicked();
	void onRefreshClicked();

private:
	QPushButton *activateButton;
	QPushButton *deactivateButton;
	QPushButton *refreshButton;
	QHBoxLayout *layout;
	bool isProcessing;

	void applyFileIconToButton(QPushButton *button, const QString &filePath);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
	void focusOutEvent(QFocusEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

signals:
	void activateAllVideoCaptureDevices();
	void deactivateAllVideoCaptureDevices();
	void refreshAllVideoCaptureDevices();
};
