#include "multidock_dock.hpp"
#include <QEvent>
#include <QPainterPath>
#include <QPainter>
#include <streamup/debug-logger.hpp>
#include "inner_dock_host.hpp"
#include "persistence.hpp"
#include "multidock_utils.hpp"
#include <streamup/ui/gallery-style.hpp>
#include <obs-module.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QCheckBox>
#include <QAction>
#include <QIcon>
#include <QStyle>
#include <QDockWidget>
#include <QMainWindow>

namespace StreamUP {
namespace MultiDock {

MultiDockDock::MultiDockDock(const QString& id, const QString& name, QWidget* parent)
    : QFrame(parent)
    , m_id(id)
    , m_name(name)
    , m_innerHost(nullptr)
    , m_addDockAction(nullptr)
    , m_lockCheckbox(nullptr)
    , m_docksLocked(false)
{
    SetupUi();
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Creation", "Created MultiDockDock '%s' with ID '%s'",
         m_name.toUtf8().constData(), m_id.toUtf8().constData());
}

MultiDockDock::~MultiDockDock()
{
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Cleanup", "Destroying MultiDockDock '%s'",
         m_name.toUtf8().constData());
}

namespace {

// Draws ONE of the body's rounded corners, on top of whatever is beneath it.
//
// The body is a stack: a frame, a container, and a QMainWindow holding the docks.
// Children always paint over parents, so the QMainWindow's square corners cover
// any rounding the container draws underneath. Insetting it far enough to clear
// the curve costs about 14px of padding on every side, and masking it clips the
// docks inside it, so the corner is painted over the top instead.
//
// Four small widgets rather than one covering the body: a Qt widget laid over the
// whole area covers the vertical canvas preview, which renders through a native
// window rather than through Qt and stops drawing entirely. These are radius
// sized and sit in the corners, so the middle is left alone.
class CornerOverlay : public QWidget {
public:
	CornerOverlay(Qt::Corner corner, int radius, QWidget *parent)
		: QWidget(parent), m_corner(corner), m_radius(radius)
	{
		setAttribute(Qt::WA_TransparentForMouseEvents, true);
		setFixedSize(radius, radius);
		// Hidden until a theme supplies a colour.
		hide();
	}

	void setColor(const QColor &color)
	{
		m_color = color;
		setVisible(color.isValid());
		update();
	}

	void positionIn(const QRect &area)
	{
		switch (m_corner) {
		case Qt::TopLeftCorner:
			move(area.left(), area.top());
			break;
		case Qt::TopRightCorner:
			move(area.right() - m_radius + 1, area.top());
			break;
		case Qt::BottomLeftCorner:
			move(area.left(), area.bottom() - m_radius + 1);
			break;
		case Qt::BottomRightCorner:
			move(area.right() - m_radius + 1, area.bottom() - m_radius + 1);
			break;
		}
		raise();
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		// This square, less the quarter of the curve that falls inside it.
		const qreal diameter = m_radius * 2.0;
		QRectF circle;
		switch (m_corner) {
		case Qt::TopLeftCorner:
			circle = QRectF(0, 0, diameter, diameter);
			break;
		case Qt::TopRightCorner:
			circle = QRectF(-m_radius, 0, diameter, diameter);
			break;
		case Qt::BottomLeftCorner:
			circle = QRectF(0, -m_radius, diameter, diameter);
			break;
		case Qt::BottomRightCorner:
			circle = QRectF(-m_radius, -m_radius, diameter, diameter);
			break;
		}

		QPainterPath square;
		square.addRect(QRectF(rect()));

		QPainterPath rounded;
		rounded.addEllipse(circle);

		if (!m_color.isValid()) {
			return;
		}

		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(Qt::NoPen);
		// The colour of whatever sits behind the body, given by the theme.
		painter.setBrush(m_color);
		painter.drawPath(square.subtracted(rounded));
	}

private:
	Qt::Corner m_corner;
	int m_radius;
	QColor m_color;
};

} // namespace


// Keeps the corner cover the size of the container it sits in front of, and on
// top of the host, which raises itself when docks are added.
bool MultiDockDock::eventFilter(QObject *watched, QEvent *event)
{
	if (!m_cornerOverlays.isEmpty() && event->type() == QEvent::Resize) {
		if (QWidget *widget = qobject_cast<QWidget *>(watched)) {
			for (QWidget *piece : m_cornerOverlays) {
				static_cast<CornerOverlay *>(piece)->positionIn(widget->rect());
			}
		}
	}

	return QFrame::eventFilter(watched, event);
}

// Both of these are driven by the theme through qproperty. Until a theme sets
// them the dock looks exactly as it always did, which is what keeps the rounded
// body a StreamUP theme feature rather than something imposed on every theme.
void MultiDockDock::setMultidockBodyColor(const QColor& color)
{
	m_bodyColor = color;

	// The host paints the body. It has to paint the same colour the container
	// behind it is painted, or the join shows; painting nothing at all leaves
	// stale pixels in the corners.
	if (m_innerHost) {
		if (color.isValid()) {
			QPalette hostPalette = m_innerHost->palette();
			hostPalette.setColor(QPalette::Window, color);
			m_innerHost->setPalette(hostPalette);
			m_innerHost->setAutoFillBackground(true);
		} else {
			m_innerHost->setAutoFillBackground(false);
		}
	}
}

void MultiDockDock::setMultidockCornerColor(const QColor& color)
{
	m_cornerColor = color;

	for (QWidget* piece : m_cornerOverlays) {
		static_cast<CornerOverlay*>(piece)->setColor(color);
	}
}

void MultiDockDock::SetupUi()
{
    // Set object name for identification  
    setObjectName(QString("streamup_multidock_%1").arg(m_id));

    // The object name carries the dock's id, so no stylesheet can match it. This
    // class is the stable hook the theme needs: this frame sits behind the whole
    // MultiDock body, and a rounded corner on the body is only visible if what
    // is behind it is a different colour.
    setProperty("class", "multidock-frame");
    
    // Create main layout directly on this QFrame
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Create a container widget for the inner host with padding
    QFrame* innerContainer = new QFrame();
    innerContainer->setObjectName("MultiDockInnerContainer");
    innerContainer->setFrameStyle(QFrame::NoFrame);
    // No custom styling - let OBS theme handle everything
    QVBoxLayout* innerLayout = new QVBoxLayout(innerContainer);
    // 2px here plus 4px in the host below, so the gap between the body edge and
    // a dock inside it matches the spacing the theme gives docks in the main
    // window. It used to be 4 plus 8, which read as noticeably roomier than
    // every other dock.
    // The host inside paints a square, so it has to sit far enough in that its
    // corners stay clear of the container's curve. This is that clearance, and
    // the host itself now adds nothing on top.
    innerLayout->setContentsMargins(StreamUP::UIStyles::S(6), StreamUP::UIStyles::S(6), StreamUP::UIStyles::S(6), StreamUP::UIStyles::S(6));
    innerLayout->setSpacing(0);
    
    // Create inner host as a direct child
    m_innerHost = new InnerDockHost(m_id, this);
    
    // Remove window flags to make it look integrated
    m_innerHost->setWindowFlags(Qt::Widget);
    
    // Add inner host to the container layout
    innerLayout->addWidget(m_innerHost, 1);
    
    // Add container to main layout (takes most space)
    mainLayout->addWidget(innerContainer, 1);

    // The 4 corner covers. See CornerOverlay: this is what makes the body's
    // rounded corners visible without padding the docks in or clipping them.
    //
    // The radius must match --radius_multidock_body in the theme, since these
    // paint the corners that rounding leaves; a mismatch shows as a double curve.
    const int cornerRadius = StreamUP::UIStyles::S(28);
    for (Qt::Corner corner : {Qt::TopLeftCorner, Qt::TopRightCorner,
                              Qt::BottomLeftCorner, Qt::BottomRightCorner}) {
        auto *piece = new CornerOverlay(corner, cornerRadius, innerContainer);
        piece->positionIn(innerContainer->rect());
        m_cornerOverlays.append(piece);
    }
    innerContainer->installEventFilter(this);
    
    // Create and add the toolbar at the bottom of our layout (no padding)
    CreateBottomToolbar(mainLayout);
    
    // No auto-save - we save on OBS shutdown
    
    // Keep the floor low so the dock can be dragged narrow. 400x300 was wide
    // enough that the MultiDock refused to shrink to a sensible sidebar width,
    // especially at 125% scaling where it became 500px. The contained docks
    // still contribute their own minimums, so anything genuinely too small to
    // use is still prevented by its content.
    setMinimumSize(StreamUP::UIStyles::S(120), StreamUP::UIStyles::S(120));

    // Let OBS theme handle all styling
    setFrameStyle(QFrame::NoFrame);
}

void MultiDockDock::SetName(const QString& name)
{
    m_name = name;
    
    // Find and update the OBS dock widget's title
    QMainWindow* mainWindow = GetObsMainWindow();
    if (mainWindow) {
        QDockWidget* obsDocWidget = mainWindow->findChild<QDockWidget*>(m_id);
        if (obsDocWidget) {
            obsDocWidget->setWindowTitle(name);
            StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Management", "Updated OBS dock title to '%s'",
                 name.toUtf8().constData());
        } else {
            StreamUP::DebugLogger::LogWarning("MultiDock", "Management: Could not find OBS dock widget to update title");
        }
    }
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Management", "Renamed MultiDock '%s' to '%s'",
         m_id.toUtf8().constData(), m_name.toUtf8().constData());
}

void MultiDockDock::SaveState()
{
    if (!m_innerHost) {
        return;
    }

    QStringList capturedDockIds = m_innerHost->GetCapturedDockIds();
    QByteArray layout = m_innerHost->SaveLayout();

    // Preserve any dock IDs that were saved previously but haven't been
    // restored yet this session (e.g. the OBS Twitch/browser dock loads late
    // via CEF). Without this, a session that never saw the dock would rewrite
    // config without it and permanently erase it from the MultiDock.
    for (const QString& pendingId : m_unresolvedDockIds) {
        if (!capturedDockIds.contains(pendingId)) {
            capturedDockIds.append(pendingId);
        }
    }

    SaveMultiDockState(m_id, capturedDockIds, layout, m_docksLocked);

    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "State", "Saved state for MultiDock '%s': %d captured docks (%d preserved unresolved), locked=%s",
         m_id.toUtf8().constData(), capturedDockIds.size(), m_unresolvedDockIds.size(), m_docksLocked ? "true" : "false");
}

void MultiDockDock::MarkDockResolved(const QString& dockId)
{
    m_unresolvedDockIds.removeAll(dockId);
}

void MultiDockDock::LoadState()
{
    if (!m_innerHost) {
        return;
    }

    QStringList capturedDockIds;
    QByteArray layout;
    bool locked = false;

    if (!LoadMultiDockState(m_id, capturedDockIds, layout, locked)) {
        StreamUP::DebugLogger::LogDebugFormat("MultiDock", "State", "No saved state found for MultiDock '%s'",
             m_id.toUtf8().constData());
        return;
    }

    // Restore lock state
    m_docksLocked = locked;
    if (m_lockCheckbox) {
        m_lockCheckbox->setChecked(locked);
        m_lockCheckbox->setToolTip(locked ? obs_module_text("MultiDock.Tooltip.Locked") : obs_module_text("MultiDock.Tooltip.Unlocked"));
    }
    m_innerHost->SetDocksLocked(locked);
    
    // Try to restore captured docks
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        StreamUP::DebugLogger::LogError("MultiDock", "Cannot restore docks: main window not found");
        return;
    }
    
    QList<QDockWidget*> allDocks = FindAllObsDocks(mainWindow);
    int restoredCount = 0;

    // Start fresh: any captured ID we fail to restore below is recorded as
    // unresolved so SaveState preserves it instead of dropping it.
    m_unresolvedDockIds.clear();

    for (const QString& dockId : capturedDockIds) {
        QDockWidget* dock = nullptr;

        // Try to find dock by ID
        for (QDockWidget* candidate : allDocks) {
            QString candidateId = GenerateDockId(candidate);
            if (candidateId == dockId) {
                dock = candidate;
                break;
            }
        }

        if (dock && !IsMultiDockContainer(dock)) {
            m_innerHost->AddDock(dock);
            restoredCount++;
            StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Restoration", "Successfully restored dock '%s' with ID '%s'",
                 dock->windowTitle().toUtf8().constData(), dockId.toUtf8().constData());
        } else {
            // Not found yet (e.g. a late-loading CEF dock). Preserve the ID so
            // it survives save and can be picked up by a later retry.
            m_unresolvedDockIds.append(dockId);
            // Enhanced debugging for failed dock restoration
            StreamUP::DebugLogger::LogWarningFormat("MultiDock", "Restoration", "Could not restore dock with ID '%s'",
                 dockId.toUtf8().constData());
            
            // Log all available dock IDs for debugging
            StreamUP::DebugLogger::LogDebug("MultiDock", "Restoration", "Available docks for debugging:");
            for (QDockWidget* candidate : allDocks) {
                QString candidateId = GenerateDockId(candidate);
                QString objectName = candidate->objectName();
                QString title = candidate->windowTitle();
                bool isMultiDock = IsMultiDockContainer(candidate);
                
                StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Restoration", "  - ID:'%s' ObjectName:'%s' Title:'%s' IsMultiDock:%s",
                     candidateId.toUtf8().constData(),
                     objectName.toUtf8().constData(),
                     title.toUtf8().constData(),
                     isMultiDock ? "true" : "false");
            }
        }
    }
    
    // Restore layout after adding docks
    if (!layout.isEmpty()) {
        m_innerHost->RestoreLayout(layout);
    }
    
    // Ensure the MultiDock is properly shown and toolbar state is updated
    if (restoredCount > 0) {
        // Make sure the inner host is visible
        m_innerHost->show();
        
        // Use a timer to reapply dock features after OBS finishes initialization
        QTimer::singleShot(1000, [this]() {
            if (m_innerHost) {
                m_innerHost->ReapplyDockFeatures();
            }
        });
    }
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Restoration", "Restored %d out of %d docks for MultiDock '%s'",
         restoredCount, capturedDockIds.size(), m_id.toUtf8().constData());
}

void MultiDockDock::CreateBottomToolbar(QVBoxLayout* layout)
{
    if (!layout || !m_innerHost) {
        return;
    }
    
    // Create a proper QToolBar instead of a custom widget
    QToolBar* toolBar = new QToolBar("MultiDock Controls", this);
    toolBar->setObjectName("MultiDockBottomToolbar");
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolBar->setOrientation(Qt::Horizontal);
    
    // Match standard OBS toolbar icon size
    toolBar->setIconSize(QSize(StreamUP::UIStyles::S(16), StreamUP::UIStyles::S(16)));

    // Let OBS theme handle all toolbar styling - no custom stylesheets
    
    // Add Dock action with OBS theme plus icon
    QAction* addDockAction = toolBar->addAction(QIcon(), "");
    QToolButton* addButton = qobject_cast<QToolButton*>(toolBar->widgetForAction(addDockAction));
    if (addButton) {
        addButton->setProperty("class", "icon-plus");
        addButton->setProperty("themeID", "addIconSmall");
    }
    addDockAction->setToolTip(obs_module_text("MultiDock.Tooltip.AddDock"));
    connect(addDockAction, &QAction::triggered, [this]() {
        if (m_innerHost) {
            m_innerHost->ShowAddDockDialog();
        }
    });

    // Lock Docks checkbox - using exact same approach as OBS source dock
    QCheckBox* lockCheckbox = new QCheckBox(this);
    lockCheckbox->setProperty("class", "checkbox-icon indicator-lock");
    lockCheckbox->setChecked(false); // Start unlocked
    lockCheckbox->setToolTip(obs_module_text("MultiDock.Tooltip.Unlocked"));

    connect(lockCheckbox, &QCheckBox::toggled, [this, lockCheckbox](bool checked) {
        m_docksLocked = checked;
        lockCheckbox->setToolTip(m_docksLocked ? obs_module_text("MultiDock.Tooltip.Locked") : obs_module_text("MultiDock.Tooltip.Unlocked"));

        if (m_innerHost) {
            m_innerHost->SetDocksLocked(m_docksLocked);
            // Update toolbar state after lock change
            UpdateToolbarState();
        }
    });

    toolBar->addWidget(lockCheckbox);

    // Store references for later access
    m_addDockAction = addDockAction;
    m_lockCheckbox = lockCheckbox;
    
    // Add toolbar widget to the bottom of the layout
    layout->addWidget(toolBar, 0); // 0 means don't stretch
    
    // Initialize toolbar button states
    UpdateToolbarState();
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "UI", "Created bottom toolbar for MultiDock '%s'",
         m_id.toUtf8().constData());
}

void MultiDockDock::UpdateToolbarState()
{
    if (!m_innerHost) {
        return;
    }
    
    // Update button states based on lock status
    if (m_addDockAction) {
        // Keep add button always enabled - lock only affects dock manipulation, not adding new docks
        // This matches OBS source dock behavior where add source is always available
        m_addDockAction->setEnabled(true);
    }
}


} // namespace MultiDock
} // namespace StreamUP

#include "multidock_dock.moc"
