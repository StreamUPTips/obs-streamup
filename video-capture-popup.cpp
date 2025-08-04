#include "video-capture-popup.hpp"
#include "ui/ui-styles.hpp"
#include <QIcon>
#include <QApplication>
#include <QScreen>
#include <obs-module.h>

VideoCapturePopup::VideoCapturePopup(QWidget *parent) 
	: QWidget(parent), isProcessing(false)
{
	setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
	
	// Create buttons using squircle style
	activateButton = StreamUP::UIStyles::CreateStyledSquircleButton("", "success", 40);
	deactivateButton = StreamUP::UIStyles::CreateStyledSquircleButton("", "error", 40);
	refreshButton = StreamUP::UIStyles::CreateStyledSquircleButton("", "info", 40);
	
	// Apply icons
	applyFileIconToButton(activateButton, ":Qt/icons/16x16/media-playback-start.png");
	applyFileIconToButton(deactivateButton, ":Qt/icons/16x16/media-playback-stop.png");
	applyFileIconToButton(refreshButton, ":Qt/icons/16x16/view-refresh.png");
	
	// Set button properties for squircle style
	auto setButtonProperties = [](QPushButton *button) {
		button->setIconSize(QSize(22, 22));
		button->setFixedSize(40, 40);  // Explicitly enforce square size
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
	// Get screen geometry
	QScreen* screen = QApplication::screenAt(buttonPos);
	if (!screen) {
		screen = QApplication::primaryScreen();
	}
	QRect screenGeometry = screen->availableGeometry();
	
	// Calculate popup size
	adjustSize();
	QSize popupSize = size();
	
	// Try different positions in order of preference
	QPoint popupPos;
	
	// 1. Try to the right of the button
	popupPos = buttonPos + QPoint(buttonSize.width() + 5, 0);
	if (popupPos.x() + popupSize.width() <= screenGeometry.right()) {
		// Fits to the right, check vertical bounds
		if (popupPos.y() + popupSize.height() > screenGeometry.bottom()) {
			// Adjust vertically to fit
			popupPos.setY(screenGeometry.bottom() - popupSize.height());
		}
		if (popupPos.y() < screenGeometry.top()) {
			popupPos.setY(screenGeometry.top());
		}
	}
	// 2. Try to the left of the button
	else {
		popupPos = buttonPos + QPoint(-popupSize.width() - 5, 0);
		if (popupPos.x() >= screenGeometry.left()) {
			// Fits to the left, check vertical bounds
			if (popupPos.y() + popupSize.height() > screenGeometry.bottom()) {
				popupPos.setY(screenGeometry.bottom() - popupSize.height());
			}
			if (popupPos.y() < screenGeometry.top()) {
				popupPos.setY(screenGeometry.top());
			}
		}
		// 3. Try below the button
		else {
			popupPos = buttonPos + QPoint(0, buttonSize.height() + 5);
			if (popupPos.y() + popupSize.height() > screenGeometry.bottom()) {
				// Try above the button
				popupPos = buttonPos + QPoint(0, -popupSize.height() - 5);
			}
			// Ensure horizontal bounds
			if (popupPos.x() + popupSize.width() > screenGeometry.right()) {
				popupPos.setX(screenGeometry.right() - popupSize.width());
			}
			if (popupPos.x() < screenGeometry.left()) {
				popupPos.setX(screenGeometry.left());
			}
		}
	}
	
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