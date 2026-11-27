#include "theme-window.hpp"
#include <algorithm>
#include "ui-helpers.hpp"
#include <streamup/ui/window-chrome.hpp> // ShadowDialog, RoundedContainer, makeWindow, WindowShell
#include <streamup/ui/pill-button.hpp>   // PillButton
#include "streamup-kit.hpp"
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
// only paints flat placeholder slides and exposes no API to feed it real
// QPixmaps, so this consumer re-implements the same paint model but draws the
// actual theme screenshots — preserving auto-advance, click-to-zoom and the real
// preview images while matching the SoT carousel look.
class ThemeImageCarousel : public QWidget
{
    Q_OBJECT

public:
    explicit ThemeImageCarousel(QWidget* parent = nullptr, int height = 400) : QWidget(parent)
    {
        setFixedHeight(StreamUP::UIStyles::S(height));
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
        // obs-theme-1.png, -2, ... one per colourway, as many as are bundled.
        for (int i = 1; i <= 12; ++i) {
            QPixmap pixmap(QString(":/images/misc/obs-theme-%1.png").arg(i));
            if (pixmap.isNull())
                break;
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
        zoomDialog->setWindowTitle(obs_module_text("StreamUP.ThemeWindow.PreviewTitle"));
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

        // One column the window fits to: the pictures (16:9), one line, the
        // membership strip. No spare space under them.
        QVBoxLayout* contentLayout = StreamUP::Kit::ScrollingBody(shell.content);
        ThemeImageCarousel* carousel = new ThemeImageCarousel(nullptr, 390);
        contentLayout->addWidget(carousel);

        QLabel* subtitleLabel = StreamUP::UIStyles::makeLabel(obs_module_text("ThemeWindow.Line"), 14, 500,
                                                              StreamUP::UIStyles::Colors::TEXT_SECONDARY);
        subtitleLabel->setWordWrap(true);
        subtitleLabel->setAlignment(Qt::AlignCenter);
        contentLayout->addWidget(subtitleLabel);

        if (QWidget *member = StreamUP::Kit::MembershipCard(StreamUP::Kit::MemberStyle::Strip,
                                                             obs_module_text("ThemeWindow.Membership")))
            contentLayout->addWidget(member);

        // Action/close buttons in footer (right-anchored, inline with brand line)
        QPushButton* visitButton = new StreamUP::UIStyles::PillButton(obs_module_text("ThemeWindow.Get"), "primary");
        QObject::connect(visitButton, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamup.tips/product/streamup-obs-theme"));
        });
        QPushButton* closeButton = new StreamUP::UIStyles::PillButton(obs_module_text("UI.Button.Done"), "outline");
        QObject::connect(closeButton, &QPushButton::clicked, [dialog]() { dialog->close(); });
        shell.footerButtons->addWidget(closeButton);
        shell.footerButtons->addWidget(visitButton);

        StreamUP::Kit::FitToContent(dialog, 720);
        StreamUP::UIHelpers::PlaceOverMainWindow(dialog);

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

} // namespace ThemeWindow
} // namespace StreamUP

#include "theme-window.moc"
