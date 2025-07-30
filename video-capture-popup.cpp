#include "video-capture-popup.hpp"
#include <QIcon>
#include <obs-module.h>

VideoCapturePopup::VideoCapturePopup(QWidget *parent) 
	: QWidget(parent), isProcessing(false)
{
	setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
	
	// Create buttons
	activateButton = new QPushButton(this);
	deactivateButton = new QPushButton(this);
	refreshButton = new QPushButton(this);
	
	// Apply icons
	applyFileIconToButton(activateButton, ":Qt/icons/16x16/media-playback-start.png");
	applyFileIconToButton(deactivateButton, ":Qt/icons/16x16/media-playback-stop.png");
	applyFileIconToButton(refreshButton, ":Qt/icons/16x16/view-refresh.png");
	
	// Set button properties - make them square
	auto setButtonProperties = [](QPushButton *button) {
		button->setIconSize(QSize(20, 20));
		button->setFixedSize(40, 40);
		button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	};
	
	setButtonProperties(activateButton);
	setButtonProperties(deactivateButton);
	setButtonProperties(refreshButton);
	
	// Set tooltips
	activateButton->setToolTip(obs_module_text("ActivateAllVideoCaptureDevices"));
	deactivateButton->setToolTip(obs_module_text("DeactivateAllVideoCaptureDevices"));
	refreshButton->setToolTip(obs_module_text("RefreshAllVideoCaptureDevices"));
	
	// Create layout
	layout = new QVBoxLayout(this);
	layout->setContentsMargins(5, 5, 5, 5);
	layout->setSpacing(2);
	
	layout->addWidget(activateButton);
	layout->addWidget(deactivateButton);
	layout->addWidget(refreshButton);
	
	setLayout(layout);
	
	// Connect signals
	connect(activateButton, &QPushButton::clicked, this, &VideoCapturePopup::onActivateClicked);
	connect(deactivateButton, &QPushButton::clicked, this, &VideoCapturePopup::onDeactivateClicked);
	connect(refreshButton, &QPushButton::clicked, this, &VideoCapturePopup::onRefreshClicked);
}

void VideoCapturePopup::showNearButton(const QPoint &buttonPos, const QSize &buttonSize)
{
	// Position popup to the right of the button
	QPoint popupPos = buttonPos + QPoint(buttonSize.width() + 5, 0);
	move(popupPos);
	show();
	raise();
	setFocus();
}

void VideoCapturePopup::onActivateClicked()
{
	if (isProcessing) return;
	emit activateAllVideoCaptureDevices();
	deleteLater();
}

void VideoCapturePopup::onDeactivateClicked()
{
	if (isProcessing) return;
	emit deactivateAllVideoCaptureDevices();
	deleteLater();
}

void VideoCapturePopup::onRefreshClicked()
{
	if (isProcessing) return;
	emit refreshAllVideoCaptureDevices();
	deleteLater();
}

void VideoCapturePopup::applyFileIconToButton(QPushButton *button, const QString &filePath)
{
	button->setIcon(QIcon(filePath));
}