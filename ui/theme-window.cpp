#include "theme-window.hpp"
#include <algorithm>
#include "ui-helpers.hpp"
#include <streamup/ui/window-chrome.hpp> // ShadowDialog, RoundedContainer, makeWindow, WindowShell
#include <streamup/ui/pill-button.hpp>   // PillButton
#include <streamup/ui/labels.hpp>        // makeLabel, sectionHeader
#include <streamup/ui/ui-scrollbar.hpp>  // useScrollBars
#include <streamup/ui/image-carousel.hpp> // su::ImageCarousel (SoT carousel look)
#include <streamup/ui/mac-inputs.hpp>     // detail::drawChevron (SoT arrow glyph)
#include "../utilities/error-handler.hpp"
#include "../version.h"
#include <obs-module.h>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QPointer>
#include <QGroupBox>
#include <QPixmap>
#include <QFrame>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QTimer>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QEnterEvent>
#include <QDialog>
#include <QKeyEvent>
#include <QScreen>
#include <QMouseEvent>

namespace StreamUP {
namespace ThemeWindow {

// Theme dialog is now managed by DialogManager in ui-helpers

// Carousel widget for theme images.
//
// Adopts the SoT ImageCarousel visual language (su::ImageCarousel in
// image-carousel.hpp): a rounded-clipped image area, a slide-across transition,
// custom-painted circular nav arrows, and accent dot indicators. The SoT widget
// only paints gradient placeholder slides and exposes no API to feed it real
// QPixmaps, so this consumer re-implements the same paint model but draws the
// actual theme screenshots — preserving auto-advance, click-to-zoom and the real
// preview images while matching the SoT carousel look.
class ThemeImageCarousel : public QWidget
{
    Q_OBJECT

public:
    explicit ThemeImageCarousel(QWidget* parent = nullptr) : QWidget(parent)
    {
        setFixedHeight(StreamUP::UIStyles::S(400));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);

        loadImages();

        // Slide animation, mirroring su::ImageCarousel (280ms OutCubic).
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(280);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        m_anim->setStartValue(0.0);
        m_anim->setEndValue(1.0);
        connect(m_anim, &QVariantAnimation::valueChanged, this,
                [this](const QVariant& v) { m_t = v.toReal(); update(); });
        connect(m_anim, &QVariantAnimation::finished, this, [this]() {
            currentIndex = m_incoming;
            m_animating = false;
            update();
        });

        setupTimer();
    }

public slots:
    void nextImage() { go(+1); }
    void previousImage() { go(-1); }

private slots:
    void goToImage(int index)
    {
        if (index >= 0 && index < images.size() && index != currentIndex)
            go(index > currentIndex ? +1 : -1, index);
    }

private:
    int count() const { return images.size(); }

    void go(int dir, int explicitTarget = -1)
    {
        if (m_animating || images.size() < 2)
            return;
        m_dir = dir;
        m_incoming = explicitTarget >= 0 ? explicitTarget
                                         : (currentIndex + dir + count()) % count();
        m_animating = true;
        m_anim->stop();
        m_anim->start();
    }

    // Geometry — mirrors su::ImageCarousel.
    QRectF imageRect() const { return QRectF(rect()).adjusted(0, 0, 0, -StreamUP::UIStyles::S(22)); }
    qreal arrowR() const { return StreamUP::UIStyles::S(15); }
    QPointF leftArrowC() const { return {imageRect().left() + StreamUP::UIStyles::S(24), imageRect().center().y()}; }
    QPointF rightArrowC() const { return {imageRect().right() - StreamUP::UIStyles::S(24), imageRect().center().y()}; }

    void loadImages()
    {
        // Load theme images
        QStringList imageFiles = {"obs-theme-1.png", "obs-theme-2.png", "obs-theme-3.png", "obs-theme-4.png"};

        for (const QString& fileName : imageFiles) {
            QString imagePath = QString(":/images/misc/%1").arg(fileName);
            QPixmap pixmap(imagePath);
            if (!pixmap.isNull())
                images.append(pixmap);
        }
    }

    void setupTimer()
    {
        autoAdvanceTimer = new QTimer(this);
        connect(autoAdvanceTimer, &QTimer::timeout, this, &ThemeImageCarousel::nextImage);
        autoAdvanceTimer->start(4000); // Auto-advance every 4 seconds
    }

    // Draw one image slide, aspect-fit and centred inside r.
    void drawSlide(QPainter& p, int idx, const QRectF& r)
    {
        // Dark backing so letterboxed margins read cleanly.
        p.fillRect(r, QColor(StreamUP::UIStyles::Colors::BG_SECONDARY));
        if (idx < 0 || idx >= images.size())
            return;
        const QPixmap& src = images[idx];
        if (src.isNull())
            return;
        QSizeF fit = src.size().scaled(r.size().toSize(), Qt::KeepAspectRatio);
        QRectF target(0, 0, fit.width(), fit.height());
        target.moveCenter(r.center());
        p.drawPixmap(target, src, QRectF(src.rect()));
    }

    void drawArrow(QPainter& p, const QPointF& c, bool left)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 110));
        p.drawEllipse(c, arrowR(), arrowR());
        p.save();
        p.translate(c);
        p.rotate(left ? 90.0 : -90.0); // drawChevron draws a down-chevron
        QPen cp(QColor(255, 255, 255), std::max(1.4, StreamUP::UIStyles::S(2) * 0.85));
        cp.setCapStyle(Qt::RoundCap);
        cp.setJoinStyle(Qt::RoundJoin);
        p.setPen(cp);
        p.setBrush(Qt::NoBrush);
        StreamUP::UIStyles::detail::drawChevron(p, QPointF(0, 0), StreamUP::UIStyles::S(8), /*up=*/false);
        p.restore();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QRectF ir = imageRect();

        p.save();
        QPainterPath clip;
        clip.addRoundedRect(ir, StreamUP::UIStyles::S(12), StreamUP::UIStyles::S(12));
        p.setClipPath(clip);
        if (m_animating) {
            drawSlide(p, currentIndex, ir.translated(-m_dir * m_t * ir.width(), 0));
            drawSlide(p, m_incoming, ir.translated(m_dir * (1.0 - m_t) * ir.width(), 0));
        } else {
            drawSlide(p, currentIndex, ir);
        }
        p.restore();

        if (images.size() > 1) {
            drawArrow(p, leftArrowC(), /*left=*/true);
            drawArrow(p, rightArrowC(), /*left=*/false);
        }

        // Accent dots.
        const int n = count();
        if (n > 1) {
            const qreal dotY = ir.bottom() + StreamUP::UIStyles::S(13);
            const qreal gap = StreamUP::UIStyles::S(16);
            const qreal startX = width() / 2.0 - (n - 1) * gap / 2.0;
            const int active = m_animating ? m_incoming : currentIndex;
            for (int i = 0; i < n; ++i) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(i == active ? StreamUP::UIStyles::Colors::PRIMARY_COLOR
                                              : StreamUP::UIStyles::Colors::TEXT_MUTED));
                const qreal rr = i == active ? StreamUP::UIStyles::S(4) : StreamUP::UIStyles::S(3);
                p.drawEllipse(QPointF(startX + i * gap, dotY), rr, rr);
            }
        }
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        const QPointF pos = e->position();
        // Nav arrows.
        if (images.size() > 1) {
            if (QLineF(pos, leftArrowC()).length() <= arrowR() + StreamUP::UIStyles::S(3)) {
                previousImage();
                return;
            }
            if (QLineF(pos, rightArrowC()).length() <= arrowR() + StreamUP::UIStyles::S(3)) {
                nextImage();
                return;
            }
            // Dots.
            const QRectF ir = imageRect();
            const qreal dotY = ir.bottom() + StreamUP::UIStyles::S(13);
            const qreal gap = StreamUP::UIStyles::S(16);
            const qreal startX = width() / 2.0 - (count() - 1) * gap / 2.0;
            for (int i = 0; i < count(); ++i) {
                if (QLineF(pos, QPointF(startX + i * gap, dotY)).length() <= StreamUP::UIStyles::S(8)) {
                    goToImage(i);
                    return;
                }
            }
        }
        // Otherwise a click on the image opens the zoom view.
        if (imageRect().contains(pos))
            showZoomedImage();
    }

    void enterEvent(QEnterEvent* event) override
    {
        autoAdvanceTimer->stop();
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        autoAdvanceTimer->start(4000);
        QWidget::leaveEvent(event);
    }

private:
    void showZoomedImage()
    {
        if (currentIndex >= images.size()) return;

        // Create modal zoom dialog (frameless rounded card + elevation shadow)
        const int sm = StreamUP::UIStyles::ShadowDialog::kShadowMargin;
        QDialog* zoomDialog = new StreamUP::UIStyles::ShadowDialog(this->window());
        zoomDialog->setWindowTitle("Theme Preview - Full Size");
        zoomDialog->setModal(true);
        zoomDialog->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        zoomDialog->setAttribute(Qt::WA_TranslucentBackground);

        QVBoxLayout* outerLayout = new QVBoxLayout(zoomDialog);
        outerLayout->setContentsMargins(sm, sm, sm, sm);
        outerLayout->setSpacing(0);

        auto* zoomCard = new StreamUP::UIStyles::RoundedContainer(14);
        outerLayout->addWidget(zoomCard);

        QVBoxLayout* layout = new QVBoxLayout(zoomCard);
        layout->setContentsMargins(StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(20));

        // Full-size image label with fixed container size
        QLabel* fullImageLabel = new QLabel();
        fullImageLabel->setAlignment(Qt::AlignCenter);
        fullImageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        
        // Scale image to fit in fixed dialog size while maintaining aspect ratio
        QPixmap scaledPixmap = images[currentIndex].scaled(
            QSize(800, 600), 
            Qt::KeepAspectRatio, 
            Qt::SmoothTransformation
        );
        fullImageLabel->setPixmap(scaledPixmap);

        // Close instruction
        QLabel* instructionLabel = StreamUP::UIStyles::makeLabel("Click anywhere or press ESC to close", 14, 500,
                                                                 StreamUP::UIStyles::Colors::TEXT_PRIMARY);
        instructionLabel->setAlignment(Qt::AlignCenter);

        layout->addWidget(fullImageLabel, 1);
        layout->addSpacing(StreamUP::UIStyles::S(10));
        layout->addWidget(instructionLabel);

        // Fixed dialog size (+ shadow margin)
        zoomDialog->setFixedSize(StreamUP::UIStyles::S(860) + 2 * sm, StreamUP::UIStyles::S(720) + 2 * sm);

        // Center on screen
        zoomDialog->move(
            (QApplication::primaryScreen()->geometry().width() - (StreamUP::UIStyles::S(860) + 2 * sm)) / 2,
            (QApplication::primaryScreen()->geometry().height() - (StreamUP::UIStyles::S(720) + 2 * sm)) / 2
        );

        // Install event filter for closing on click
        zoomDialog->installEventFilter(this);
        fullImageLabel->installEventFilter(this);

        // Store reference for event handling
        currentZoomDialog = zoomDialog;

        zoomDialog->exec();
        delete zoomDialog;
        currentZoomDialog = nullptr;
    }

    bool eventFilter(QObject* obj, QEvent* event) override
    {
        // Handle zoom dialog events (close on click or Esc).
        if (currentZoomDialog && (obj == currentZoomDialog || obj->parent() == currentZoomDialog)) {
            if (event->type() == QEvent::MouseButtonPress ||
                (event->type() == QEvent::KeyPress &&
                 static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)) {
                currentZoomDialog->accept();
                return true;
            }
        }
        return QWidget::eventFilter(obj, event);
    }

private:
    QList<QPixmap> images;
    QTimer* autoAdvanceTimer = nullptr;
    QDialog* currentZoomDialog = nullptr;
    int currentIndex = 0;

    // Slide-transition state (mirrors su::ImageCarousel).
    QVariantAnimation* m_anim = nullptr;
    int m_incoming = 0;
    int m_dir = 1;
    bool m_animating = false;
    qreal m_t = 0.0;
};

void CreateThemeDialog()
{
    UIHelpers::ShowSingletonDialogOnUIThread("theme", []() -> QDialog* {
        // Create modern unified dialog (StreamUP frameless custom window)
        StreamUP::UIStyles::WindowShell shell =
            StreamUP::UIStyles::makeWindow("Theme", "v" PROJECT_VERSION, nullptr,
                                           /*brandFooter=*/true, "StreamUP");
        QDialog* dialog = shell.dialog;
        dialog->setModal(false);
        dialog->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

        QVBoxLayout* mainLayout = shell.content;
        mainLayout->setContentsMargins(StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(16),
                                       StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(16));

        // Modern unified content area with scroll - everything inside
        QScrollArea* scrollArea = new QScrollArea();
        scrollArea->setFrameShape(QFrame::NoFrame);
        StreamUP::UIStyles::useScrollBars(scrollArea);
        scrollArea->setWidgetResizable(true); // Ensure this is set for proper alignment
        scrollArea->setAlignment(Qt::AlignTop | Qt::AlignHCenter); // Keep content top-aligned when shorter than viewport
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // Disable horizontal scrolling

        QWidget* contentWidget = new QWidget();
        contentWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
        contentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        contentWidget->setMaximumWidth(StreamUP::UIStyles::S(900)); // Match dialog width minus padding
        QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(20),
                                          StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(20));
        contentLayout->setSpacing(StreamUP::UIStyles::S(20));

        // Modern header inside scrollable area
        QWidget* headerSection = new QWidget();
        QVBoxLayout* headerLayout = new QVBoxLayout(headerSection);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(StreamUP::UIStyles::S(8));

        // Brief subtitle (title already in frameless chrome header)
        QLabel* subtitleLabel = StreamUP::UIStyles::makeLabel(
            "The cleanest OBS theme out there - available to all supporters of any tier", 13, 500,
            StreamUP::UIStyles::Colors::TEXT_SECONDARY);
        subtitleLabel->setWordWrap(true);
        subtitleLabel->setAlignment(Qt::AlignCenter);

        headerLayout->addWidget(subtitleLabel);

        contentLayout->addWidget(headerSection);
        
        // Description section
        contentLayout->addWidget(StreamUP::UIStyles::sectionHeader("About the StreamUP Theme"));
        QWidget* descriptionGroup = new QWidget();
        QVBoxLayout* descriptionLayout = new QVBoxLayout(descriptionGroup);
        descriptionLayout->setContentsMargins(0, 0, 0, 0);
        descriptionLayout->setSpacing(StreamUP::UIStyles::S(12));

        QLabel* descriptionText = StreamUP::UIStyles::makeLabel(
            "The StreamUP OBS Theme is carefully crafted to provide the cleanest and most "
            "professional OBS Studio experience. This theme perfectly matches the StreamUP plugin's "
            "design language, creating a seamless and cohesive interface throughout OBS Studio. "
            "Featuring modern design elements, improved readability, and streamlined interfaces - "
            "it's the perfect companion to your streaming setup.", 14, 500,
            StreamUP::UIStyles::Colors::TEXT_PRIMARY);
        descriptionText->setWordWrap(true);
        descriptionLayout->addWidget(descriptionText);

        contentLayout->addWidget(descriptionGroup);

        // Theme images carousel section
        contentLayout->addWidget(StreamUP::UIStyles::sectionHeader("Theme Preview"));
        QWidget* previewGroup = new QWidget();
        QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
        previewLayout->setContentsMargins(0, 0, 0, 0);
        previewLayout->setSpacing(StreamUP::UIStyles::S(12));

        // The SoT-styled carousel paints its own circular nav arrows + accent
        // dots internally and slides between the real theme screenshots, so it
        // fills the full preview width (no external pill buttons needed).
        ThemeImageCarousel* carousel = new ThemeImageCarousel();
        previewLayout->addWidget(carousel);

        contentLayout->addWidget(previewGroup);
        
        // Access information section
        contentLayout->addWidget(StreamUP::UIStyles::sectionHeader("How to Get the Theme"));
        QWidget* accessGroup = new QWidget();
        QVBoxLayout* accessLayout = new QVBoxLayout(accessGroup);
        accessLayout->setContentsMargins(0, 0, 0, 0);
        accessLayout->setSpacing(StreamUP::UIStyles::S(12));

        QLabel* accessText = StreamUP::UIStyles::makeLabel(
            "The StreamUP OBS Theme is available to all StreamUP supporters of any tier. "
            "Simply become a supporter and you'll gain access to download and use this "
            "professional theme for your OBS Studio setup.", 14, 500,
            StreamUP::UIStyles::Colors::TEXT_PRIMARY);
        accessText->setWordWrap(true);
        accessLayout->addWidget(accessText);

        // Support button
        QPushButton* supportButton = new StreamUP::UIStyles::PillButton("Become a Supporter", "primary");
        QObject::connect(supportButton, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamup.tips/premium"));
        });
        accessLayout->addWidget(supportButton);
        
        contentLayout->addWidget(accessGroup);
        
        scrollArea->setWidget(contentWidget);
        mainLayout->addWidget(scrollArea);

        // Action/close buttons in footer (right-anchored, inline with brand line)
        QPushButton* visitButton = new StreamUP::UIStyles::PillButton("Visit StreamUP.tips", "primary");
        QObject::connect(visitButton, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamup.tips/"));
        });
        QPushButton* closeButton = new StreamUP::UIStyles::PillButton("Close", "outline");
        QObject::connect(closeButton, &QPushButton::clicked, [dialog]() { dialog->close(); });
        shell.footerButtons->addWidget(closeButton);
        shell.footerButtons->addWidget(visitButton);

        // Flexible sizing that fits content - made wider for carousel (preferred width + height)
        dialog->resize(StreamUP::UIStyles::S(900), StreamUP::UIStyles::S(820));

        // Show dialog
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
        return dialog;
    });
}

void ShowThemeWindow()
{
    try {
        CreateThemeDialog();
    } catch (const std::exception& e) {
        StreamUP::ErrorHandler::LogError("Failed to show theme window: " + std::string(e.what()), StreamUP::ErrorHandler::Category::UI);
    }
}

bool IsThemeWindowOpen()
{
    return UIHelpers::DialogManager::IsSingletonDialogOpen("theme");
}

} // namespace ThemeWindow
} // namespace StreamUP

#include "theme-window.moc"
