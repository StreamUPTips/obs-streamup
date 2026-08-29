#ifndef STREAMUP_MULTIDOCK_DOCK_HPP
#define STREAMUP_MULTIDOCK_DOCK_HPP

#include <QFrame>
#include <QEvent>
#include <QList>
#include <QColor>
#include <QStringList>

class QVBoxLayout;
class QAction;
class QCheckBox;

namespace StreamUP {
namespace MultiDock {

class InnerDockHost;

/**
 * @brief The main MultiDock widget registered with OBS
 * 
 * This is the widget that appears in OBS's View->Docks menu and can be
 * docked alongside other OBS docks. It contains an InnerDockHost that
 * provides the inner docking functionality.
 */
class MultiDockDock : public QFrame
{
    Q_OBJECT

    // Set by the theme, not by us. A theme that says nothing gets the plain
    // square dock it has always had: the body keeps the application's own
    // background and no corners are painted. Only a theme that asks for this,
    // and supplies the 2 colours it needs, gets the rounded body.
    //
    //   .multidock-frame {
    //       qproperty-multidockBodyColor: #090909;    the body
    //       qproperty-multidockCornerColor: #111111;  what shows at the corners
    //   }
    Q_PROPERTY(QColor multidockBodyColor READ multidockBodyColor WRITE setMultidockBodyColor)
    Q_PROPERTY(QColor multidockCornerColor READ multidockCornerColor WRITE setMultidockCornerColor)

public:
    QColor multidockBodyColor() const { return m_bodyColor; }
    void setMultidockBodyColor(const QColor& color);
    QColor multidockCornerColor() const { return m_cornerColor; }
    void setMultidockCornerColor(const QColor& color);

public:
    explicit MultiDockDock(const QString& id, const QString& name, QWidget* parent = nullptr);
    ~MultiDockDock();

    /**
     * @brief Get the MultiDock ID
     * @return Unique identifier for this MultiDock
     */
    QString GetId() const { return m_id; }

    /**
     * @brief Get the MultiDock name
     * @return Display name for this MultiDock
     */
    QString GetName() const { return m_name; }

    /**
     * @brief Get the inner dock host
     * @return Pointer to the inner docking area
     */
    InnerDockHost* GetInnerHost() const { return m_innerHost; }

    /**
     * @brief Set the MultiDock name and update display
     * @param name New name for the MultiDock
     */
    void SetName(const QString& name);

    /**
     * @brief Save the current state to persistent storage
     */
    void SaveState();

    /**
     * @brief Load and restore state from persistent storage
     */
    void LoadState();

    /**
     * @brief Update the toolbar state based on current docks
     */
    void UpdateToolbarState();

    /**
     * @brief Dock IDs that were saved but have not yet been restored this
     *        session (e.g. their source dock loaded late or not at all).
     *        These must be preserved on save so a late-loading dock is never
     *        permanently dropped from persistent storage.
     */
    QStringList GetUnresolvedDockIds() const { return m_unresolvedDockIds; }

    /**
     * @brief Mark a previously-unresolved dock ID as now restored, so it is
     *        no longer force-preserved on save.
     */
    void MarkDockResolved(const QString& dockId);

    // No slots needed - we save on OBS shutdown

protected:
    // Keeps the corner cover sized to the container it sits in front of.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // Paint the body's rounded corners over the top of the dock host. Four small
    // widgets, one per corner, so the middle of the body stays clear of Qt
    // widgets: the vertical canvas preview renders natively and stops drawing if
    // anything is laid over it.
    QList<QWidget*> m_cornerOverlays;

    // Both invalid until a theme sets them, which is what keeps this off
    // everywhere else.
    QColor m_bodyColor;
    QColor m_cornerColor;

    void SetupUi();
    void CreateBottomToolbar(QVBoxLayout* layout);

    QString m_id;
    QString m_name;
    InnerDockHost* m_innerHost;
    
    // Toolbar references for status updates
    QAction* m_addDockAction;
    QCheckBox* m_lockCheckbox;
    bool m_docksLocked;

    // Saved dock IDs not yet restored this session; preserved on save so a
    // late-loading (or temporarily absent) dock is never erased from config.
    QStringList m_unresolvedDockIds;
};

} // namespace MultiDock
} // namespace StreamUP

#endif // STREAMUP_MULTIDOCK_DOCK_HPP
