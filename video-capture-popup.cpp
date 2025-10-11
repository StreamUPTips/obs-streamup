#include "video-capture-popup.hpp"
#include "core/source-manager.hpp"
#include "ui/ui-styles.hpp"
#include "ui/ui-helpers.hpp"
#include <QIcon>
#include <QApplication>
#include <QScreen>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <obs-module.h>

VideoCapturePopup::VideoCapturePopup(QWidget *parent) 
	: QWidget(parent), isProcessing(false)
{
	setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
	setAttribute(Qt::WA_TranslucentBackground);
	
	// Create buttons using standardized squircle style to match dock buttons
	activateButton = StreamUP::UIStyles::CreateStyledSquircleButton("", "neutral", 28);
	deactivateButton = StreamUP::UIStyles::CreateStyledSquircleButton("", "neutral", 28);
	refreshButton = StreamUP::UIStyles::CreateStyledSquircleButton("", "neutral", 28);
	
	// Apply theme-aware icons
	applyFileIconToButton(activateButton, StreamUP::UIHelpers::GetThemedIconPath("video-capture-device-activate"));
	applyFileIconToButton(deactivateButton, StreamUP::UIHelpers::GetThemedIconPath("video-capture-device-deactivate"));
	applyFileIconToButton(refreshButton, StreamUP::UIHelpers::GetThemedIconPath("video-capture-device-refresh"));
	
	// Set button properties for standardized squircle style
	auto setButtonProperties = [](QPushButton *button) {
		button->setIconSize(QSize(16, 16));  // Match dock button icon size
		// Size is set by CreateStyledSquircleButton, don't override
	};
	
	setButtonProperties(activateButton);
	setButtonProperties(deactivateButton);
	setButtonProperties(refreshButton);
	
	// Set tooltips
	activateButton->setToolTip(obs_module_text("Menu.VideoCapture.ActivateAll"));
	deactivateButton->setToolTip(obs_module_text("Menu.VideoCapture.DeactivateAll"));
	refreshButton->setToolTip(obs_module_text("Menu.VideoCapture.RefreshAll"));
	
	// Create horizontal layout
	layout = new QHBoxLayout(this);
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
	
	// Install event filter on application to catch clicks outside
	qApp->installEventFilter(this);
}

VideoCapturePopup::~VideoCapturePopup()
{
	qApp->removeEventFilter(this);
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
	
	// 1. Try above the button (preferred for horizontal layout)
	popupPos = buttonPos + QPoint(0, -popupSize.height() - 5);
	if (popupPos.y() >= screenGeometry.top()) {
		// Fits above, check horizontal bounds and center with button
		int centerX = buttonPos.x() + buttonSize.width() / 2 - popupSize.width() / 2;
		if (centerX + popupSize.width() <= screenGeometry.right() && centerX >= screenGeometry.left()) {
			popupPos.setX(centerX);
		} else if (centerX < screenGeometry.left()) {
			popupPos.setX(screenGeometry.left());
		} else {
			popupPos.setX(screenGeometry.right() - popupSize.width());
		}
	}
	// 2. Try below the button
	else {
		popupPos = buttonPos + QPoint(0, buttonSize.height() + 5);
		if (popupPos.y() + popupSize.height() <= screenGeometry.bottom()) {
			// Fits below, check horizontal bounds and center with button
			int centerX = buttonPos.x() + buttonSize.width() / 2 - popupSize.width() / 2;
			if (centerX + popupSize.width() <= screenGeometry.right() && centerX >= screenGeometry.left()) {
				popupPos.setX(centerX);
			} else if (centerX < screenGeometry.left()) {
				popupPos.setX(screenGeometry.left());
			} else {
				popupPos.setX(screenGeometry.right() - popupSize.width());
			}
		}
		// 3. Try to the right of the button
		else {
			popupPos = buttonPos + QPoint(buttonSize.width() + 5, 0);
			if (popupPos.x() + popupSize.width() > screenGeometry.right()) {
				// Try to the left of the button
				popupPos = buttonPos + QPoint(-popupSize.width() - 5, 0);
			}
			// Ensure vertical bounds
			if (popupPos.y() + popupSize.height() > screenGeometry.bottom()) {
				popupPos.setY(screenGeometry.bottom() - popupSize.height());
			}
			if (popupPos.y() < screenGeometry.top()) {
				popupPos.setY(screenGeometry.top());
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
	isProcessing = true;

	StreamUP::SourceManager::ActivateAllVideoCaptureDevices(true);

	deleteLater();
}

void VideoCapturePopup::onDeactivateClicked()
{
	if (isProcessing) return;
	isProcessing = true;

	StreamUP::SourceManager::DeactivateAllVideoCaptureDevices(true);

	deleteLater();
}

void VideoCapturePopup::onRefreshClicked()
{
	if (isProcessing) return;
	isProcessing = true;

	StreamUP::SourceManager::RefreshAllVideoCaptureDevices(true);

	deleteLater();
}

void VideoCapturePopup::applyFileIconToButton(QPushButton *button, const QString &filePath)
{
	button->setIcon(QIcon(filePath));
}

bool VideoCapturePopup::eventFilter(QObject *obj, QEvent *event)
{
	Q_UNUSED(obj);
	if (event->type() == QEvent::MouseButtonPress) {
		QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
		if (mouseEvent->button() == Qt::LeftButton) {
			// Check if the click is inside our popup area using global coordinates
			QRect globalPopupRect = QRect(mapToGlobal(QPoint(0, 0)), size());
			QPoint clickPos = mouseEvent->globalPosition().toPoint();

			// If click is outside our popup, close it
			if (!globalPopupRect.contains(clickPos)) {
				deleteLater();
				return false;  // Let the event continue to be processed
			}
			// Click is inside our popup - don't filter it, let it reach our buttons
			return false;
		}
	}
	return false;
}

void VideoCapturePopup::focusOutEvent(QFocusEvent *event)
{
	// Don't auto-close on focus out - let the eventFilter handle outside clicks instead
	// This prevents the popup from closing when clicking buttons
	QWidget::focusOutEvent(event);
}

void VideoCapturePopup::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
	
	// Use a larger radius for smoother corners (18px instead of 14px)
	const int smoothRadius = 18;
	
	// Create rounded rectangle path
	QPainterPath path;
	path.addRoundedRect(rect(), smoothRadius, smoothRadius);
	
	// Set the mask to match the rounded rectangle so shadows follow the shape
	QRegion mask = QRegion(path.toFillPolygon().toPolygon());
	setMask(mask);
	
	// Fill background
	painter.fillPath(path, QColor(StreamUP::UIStyles::Colors::BACKGROUND_CARD));
	
	// Draw border with smooth pen
	QPen pen(QColor(StreamUP::UIStyles::Colors::BACKGROUND_HOVER), 1);
	pen.setStyle(Qt::SolidLine);
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	painter.setPen(pen);
	painter.drawPath(path);
	
	QWidget::paintEvent(event);
}

void VideoCapturePopup::updateIconsForTheme()
{
	// Update icons for the current theme
	applyFileIconToButton(activateButton, StreamUP::UIHelpers::GetThemedIconPath("video-capture-device-activate"));
	applyFileIconToButton(deactivateButton, StreamUP::UIHelpers::GetThemedIconPath("video-capture-device-deactivate"));
	applyFileIconToButton(refreshButton, StreamUP::UIHelpers::GetThemedIconPath("video-capture-device-refresh"));
}
