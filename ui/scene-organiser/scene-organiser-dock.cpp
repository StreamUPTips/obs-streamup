#include "scene-organiser-dock.hpp"
#include "scene-canvas.hpp"
#include <streamup/ui/dialogs.hpp>
#include <streamup/ui/color-picker.hpp>
#include <streamup/ui/window-chrome.hpp>
#include <streamup/ui/pill-button.hpp>
#include "../ui-helpers.hpp"
#include "../settings-manager.hpp"
#include <streamup/debug-logger.hpp>
#include "../../utilities/obs-data-helpers.hpp"
#include "../../utilities/path-utils.hpp"
#include "../../core/plugin-manager.hpp"
#include "../../version.h" // PROJECT_VERSION
#include <obs-module.h>
#include <QHeaderView>
#include <QApplication>
#include <QMimeData>
#include <QDrag>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QWidgetAction>
#include <QSpinBox>
#include <QGridLayout>
#include <QKeyEvent>
#include <QPointer>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QFileInfo>
#include <QFileDialog>
#include <QColor>
#include <QStyle>
#include <QFile>
#include <QTextStream>
#include <QtSvg/QSvgRenderer>
#include <QSortFilterProxyModel>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QRegularExpression>
#include <QScreen>
#include <QGuiApplication>
#include <QDir>
#include <QDateTime>
#include <util/platform.h>
#include <functional>

namespace su = StreamUP::UIStyles;
using namespace StreamUP::UIStyles;

namespace StreamUP {
namespace SceneOrganiser {

// Studio mode's preview/program pair belongs to the MAIN canvas only. Aitum's
// vertical canvas has no preview side, so a vertical dock always acts live: no
// preview staging, no transition trigger, just switch the scene.
// Folder layout lives in a per-canvas file. Scene names are only unique within
// a canvas, so a shared file would merge a main "Intro" with a vertical "Intro".
// The main canvas keeps its historic filename so existing layouts still load.
static inline QString sceneTreeFileName(CanvasType type)
{
    return (type == CanvasType::Vertical) ? QStringLiteral("scene_tree_vertical.json")
                                          : QStringLiteral("scene_tree_normal.json");
}

static inline bool studioModeFor(CanvasType type)
{
    return type == CanvasType::Normal && obs_frontend_preview_program_mode_active();
}


// The eight preset colours OBS offers in its native Sources > Set Colour menu,
// at the same 33% alpha (see OBSBasic::AddBackgroundColorMenu). Matched exactly
// so a scene coloured here reads the same as a source coloured there.
static const QList<QColor> &PresetColors()
{
    static const QList<QColor> presets = {
        QColor(255, 68, 68, 84),   QColor(255, 255, 68, 84), QColor(68, 255, 68, 84),
        QColor(68, 255, 255, 84),  QColor(68, 68, 255, 84),  QColor(255, 68, 255, 84),
        QColor(68, 68, 68, 84),    QColor(255, 255, 255, 84),
    };
    return presets;
}

// Data role marking the scene item that is currently LIVE on program.
// Drawn with the theme's own selected-row look, independent of what the user
// has actually got selected. UserRole+1 is the custom colour, UserRole+100 is
// the creation timestamp, so +2 is free.
static constexpr int ProgramSceneRole = Qt::UserRole + 2;

// The live row wears the theme's selected fill, which is right when it is also
// the row the user has picked. It stops being right the moment something ELSE
// is highlighted - a folder, or a run of scenes being reorganised - because two
// rows then carry the same fill with nothing to say which one is on program.
// In that case the live row keeps its fill and gains an outline.
static bool ProgramRowNeedsOutline(const QStyleOptionViewItem &option, const QModelIndex &index)
{
    const QAbstractItemView *view = qobject_cast<const QAbstractItemView *>(option.widget);
    if (!view || !view->selectionModel()) {
        return false;
    }

    // selectedIndexes(), not selectedRows(): the tree is left on Qt's default
    // SelectItems behaviour, and selectedRows() hands back an empty list unless
    // the view selects whole rows - which is why this check never once fired.
    const QModelIndexList selected = view->selectionModel()->selectedIndexes();
    for (const QModelIndex &sel : selected) {
        if (sel.row() != index.row() || sel.parent() != index.parent()) {
            return true;
        }
    }
    return false;
}

// A one pixel ring just inside the row, in whatever colour the text on that
// fill is drawn in, so it reads on a theme highlight and on a hand-set colour
// alike. Radius 4 matches the rounded fill the colour delegates paint.
static void DrawProgramOutline(QPainter *painter, const QRect &rowRect, const QColor &inkColor)
{
    QColor pen = inkColor.isValid() ? inkColor : QColor(255, 255, 255);
    pen.setAlpha(200);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(pen, 2));
    QRectF ring = QRectF(rowRect).adjusted(3, 2, -3, -2);
    painter->drawRoundedRect(ring, 4, 4);
    painter->restore();
}

// The item's custom icon spec (see ResolveIconSpec). +3 is the next role free
// after the colour, the program marker and before the timestamp at +100.
static constexpr int CustomIconRole = Qt::UserRole + 3;

// Marks a row in a TAB's tree as a folder rather than a scene reference. Only
// the tab trees use it; the Scenes tree distinguishes them by item type.
static constexpr int TabItemIsFolderRole = Qt::UserRole + 10;

// Whether a folder row is currently open. Expansion is the view's business, not
// the model's, so the dock writes it here when a row expands or collapses and
// the item reads it back when it works out which icon to wear.
static constexpr int FolderExpandedRole = Qt::UserRole + 11;

// The colour an item's icon is tinted with. Separate from the icon spec so the
// same icon can be re-coloured without re-picking it, and from UserRole+1,
// which colours the ROW rather than the icon.
static constexpr int CustomIconColorRole = Qt::UserRole + 4;

// Optimized theme icon cache
static QHash<QString, QIcon> s_themeIconCache;
static QWidget* s_cachedMainWindow = nullptr;

// Helper function to get theme icon from OBS main window properties (cached)
static QIcon GetThemeIcon(const QString& propertyName)
{
    // Check cache first
    auto it = s_themeIconCache.find(propertyName);
    if (it != s_themeIconCache.end()) {
        return it.value();
    }

    // Find main window only once and cache it
    if (!s_cachedMainWindow) {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (widget->objectName() == "OBSBasic") {
                s_cachedMainWindow = widget;
                break;
            }
        }
    }

    QIcon icon;
    if (s_cachedMainWindow) {
        // Get the icon from the main window's theme properties
        QVariant iconProperty = s_cachedMainWindow->property(propertyName.toUtf8().constData());
        if (iconProperty.isValid()) {
            icon = iconProperty.value<QIcon>();
        }
    }

    // Only a real icon is cached. An empty result means we asked before OBS had
    // published its theme properties - caching that would freeze the blank in
    // place for the rest of the session, which is exactly what happened when the
    // Set Icon menu was built during dock construction and swept all sixteen
    // properties before any of them existed.
    if (!icon.isNull()) {
        s_themeIconCache.insert(propertyName, icon);
    }
    return icon;
}

// Every icon OBS exposes on its main window as a theme property. These follow
// the active theme, so an item using one keeps matching when the theme changes -
// which a baked-in image cannot do.
struct ObsThemeIcon {
    const char *property;
    const char *label;
};

static const ObsThemeIcon kObsThemeIcons[] = {
    {"sceneIcon",              "Scene"},
    {"groupIcon",              "Group"},
    {"imageIcon",              "Image"},
    {"colorIcon",              "Colour"},
    {"slideshowIcon",          "Slideshow"},
    {"audioInputIcon",         "Audio Input"},
    {"audioOutputIcon",        "Audio Output"},
    {"audioProcessOutputIcon", "Application Audio"},
    {"desktopCapIcon",         "Display Capture"},
    {"windowCapIcon",          "Window Capture"},
    {"gameCapIcon",            "Game Capture"},
    {"cameraIcon",             "Camera"},
    {"textIcon",               "Text"},
    {"mediaIcon",              "Media"},
    {"browserIcon",            "Browser"},
    {"defaultIcon",            "Default"},
};

// Custom icons are stored as a small spec string rather than image data, so the
// scene tree JSON stays readable and a themed icon stays themed:
//   "obs:sceneIcon"     - an OBS theme icon, resolved fresh on every theme change
//   "file:C:/pic.png"   - an image on disk
// Anything else (or empty) means "use the default for this item type".
static QHash<QString, QIcon> s_customIconCache;

// Repaints an icon in a single colour, keeping its shape. Every pixel the icon
// draws becomes the colour; everything transparent stays transparent, so the
// silhouette survives and only the ink changes.
static QIcon TintIcon(const QIcon &icon, const QColor &color)
{
    if (icon.isNull() || !color.isValid()) {
        return icon;
    }

    QIcon tinted;
    // The sizes the dock actually asks for, plus headroom for a large row
    // height and for high-DPI. An icon rendered at the wrong size looks soft.
    for (int size : {16, 20, 24, 32, 48, 64}) {
        QPixmap pixmap = icon.pixmap(QSize(size, size));
        if (pixmap.isNull()) {
            continue;
        }

        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
        painter.end();

        tinted.addPixmap(pixmap);
    }

    return tinted.isNull() ? icon : tinted;
}

// The open and closed folder icons the plugin ships: Lucide's folder and
// folder-open, ISC licensed, with the licence alongside them in data/icons.
// OBS themes provide one folder icon (the group icon, which is the open shape),
// so a closed one has to come from somewhere. They are single colour strokes and
// are tinted to the theme's text colour, the same treatment the chevron gets, so
// they sit right on a light theme and a dark one without shipping a pair of each.
static QIcon GetFolderIcon(bool expanded, const QColor &tint)
{
    const char *fileName = expanded ? "icons/folder-open.svg" : "icons/folder-closed.svg";

    const QString cacheKey = QString::fromLatin1(fileName) +
                             (tint.isValid() ? ("|" + tint.name(QColor::HexArgb)) : QString());

    auto cached = s_customIconCache.find(cacheKey);
    if (cached != s_customIconCache.end()) {
        return cached.value();
    }

    QIcon icon;
    if (char *path = obs_module_file(fileName)) {
        icon = QIcon(QString::fromUtf8(path));
        bfree(path);
    }

    if (icon.isNull()) {
        // Shipped icon missing: the theme's group icon is a reasonable stand-in
        // and at least tells you the row is a folder.
        return GetThemeIcon("groupIcon");
    }

    icon = TintIcon(icon, tint.isValid() ? tint : QColor(255, 255, 255));

    s_customIconCache.insert(cacheKey, icon);
    return icon;
}

static QIcon ResolveIconSpec(const QString &spec, const QColor &tint = QColor())
{
    if (spec.isEmpty()) {
        return QIcon();
    }

    // Colour is part of the cache key: the same icon in two colours is two
    // different pixmaps.
    const QString cacheKey = tint.isValid() ? (spec + "|" + tint.name(QColor::HexArgb)) : spec;

    auto cached = s_customIconCache.find(cacheKey);
    if (cached != s_customIconCache.end()) {
        return cached.value();
    }

    QIcon icon;
    if (spec.startsWith(QLatin1String("obs:"))) {
        icon = GetThemeIcon(spec.mid(4));
    } else if (spec.startsWith(QLatin1String("file:"))) {
        const QString path = spec.mid(5);
        if (QFileInfo::exists(path)) {
            icon = QIcon(path);
        }
    }

    if (tint.isValid()) {
        icon = TintIcon(icon, tint);
    }

    // Same rule as GetThemeIcon: a miss is not cached, because it may only be a
    // miss for now (theme not up yet, file not mounted yet).
    if (!icon.isNull()) {
        s_customIconCache.insert(cacheKey, icon);
    }
    return icon;
}

// Optimized colored icon cache
static QHash<QString, QIcon> s_coloredIconCache;

// Helper function to create colored icons from SVG resources (optimized with caching)
static QIcon CreateColoredIcon(const QString& svgPath, const QColor& color, const QSize& size = QSize(16, 16))
{
    // Create cache key from path, color, and size
    QString cacheKey = QString("%1_%2_%3x%4").arg(svgPath, color.name(), QString::number(size.width()), QString::number(size.height()));

    // Check cache first
    auto it = s_coloredIconCache.find(cacheKey);
    if (it != s_coloredIconCache.end()) {
        return it.value();
    }

    // Create the icon only if not cached
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QSvgRenderer renderer(svgPath);
    if (renderer.isValid()) {
        renderer.render(&painter);

        // Apply color overlay
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
    }

    QIcon icon(pixmap);

    // Cache the result for future use
    s_coloredIconCache.insert(cacheKey, icon);

    return icon;
}

// Cache management functions
static void ClearIconCaches()
{
    s_themeIconCache.clear();
    s_coloredIconCache.clear();
    s_cachedMainWindow = nullptr;
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cache", "Cleared icon caches");
}

// OBS themes style list ROWS by the classes their own docks use - mostly
// QListView::item / QListWidget::item, since SceneTree and SourceTree are both
// QListView subclasses. Our organiser is a QTreeView, and two separate problems
// follow from that.
//
// One: a theme that never mentions QTreeView (Yami and the stock themes) leaves
// our rows to Qt's built-in hover, which reads nothing like the dock beside us.
//
// Two, and less obvious: QTreeView paints a row background AND an item
// background, and Qt applies the ::item rule to both. A theme whose hover is a
// translucent overlay - every StreamUP theme - therefore paints that overlay
// TWICE on a tree and once on a list. On SilverLink that is #436393 against the
// Sources dock's #37527b, from the same rule.
//
// So the rules are lifted out of the stylesheet the theme actually installed
// (qApp->styleSheet() is fully resolved by OBSApp::PrepareQSS, variables and
// all), re-emitted against QTreeView, and any translucent row background is
// flattened against the view's own backdrop first. Painting an opaque colour
// twice looks exactly like painting it once, so the tree matches the list in
// whatever theme is loaded - including themes we have never seen.
static QString BuildThemeRowQss(const QColor &paletteBackdrop)
{
    const QString appQss = qApp ? qApp->styleSheet() : QString();
    if (appQss.isEmpty()) {
        return QString();
    }

    // The backdrop to flatten against is whatever the theme paints BEHIND the
    // rows, which is not the palette: the StreamUP themes give QTreeView
    // bg_secondary while palette Base is bg_darkest, several shades darker.
    // Flattening against the palette would have produced a hover nobody asked
    // for. Read the view's own background out of the stylesheet, and fall back
    // to the palette only when the theme never states one.
    QColor backdrop = paletteBackdrop;
    {
        static const QRegularExpression viewBgRe(
            QStringLiteral(R"(QTreeView\s*\{[^{}]*?background(?:-color)?\s*:\s*([^;}]+))"));
        const QRegularExpressionMatch m = viewBgRe.match(appQss);
        if (m.hasMatch()) {
            const QColor stated(m.captured(1).trimmed());
            if (stated.isValid()) {
                backdrop = stated;
            }
        }
    }

    const QString cacheKey = appQss + QLatin1Char('|') + backdrop.name(QColor::HexArgb);
    static QString s_cachedKey;
    static QString s_cachedResult;
    if (cacheKey == s_cachedKey) {
        return s_cachedResult;
    }

    // Top-level "selectors { body }" blocks. QSS has no nesting, so a flat
    // scan is enough and is far cheaper than pulling in a parser.
    static const QRegularExpression blockRe(QStringLiteral(R"(([^{}]+)\{([^{}]*)\})"));
    // rgba(r, g, b, a) with a as either 0-1 or 0-255. Anything opaque (#hex,
    // rgb(), a named colour) is left exactly as the theme wrote it.
    static const QRegularExpression rgbaRe(
        QStringLiteral(R"(rgba\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*([0-9.]+)\s*\))"));

    auto flatten = [&](QString body) {
        QString out;
        int last = 0;
        QRegularExpressionMatchIterator hits = rgbaRe.globalMatch(body);
        while (hits.hasNext()) {
            const QRegularExpressionMatch m = hits.next();
            double alpha = m.captured(4).toDouble();
            if (alpha > 1.0) {
                alpha /= 255.0; // the 0-255 spelling
            }
            alpha = qBound(0.0, alpha, 1.0);

            const int r = m.captured(1).toInt();
            const int g = m.captured(2).toInt();
            const int b = m.captured(3).toInt();
            const QColor solid(qRound(alpha * r + (1.0 - alpha) * backdrop.red()),
                               qRound(alpha * g + (1.0 - alpha) * backdrop.green()),
                               qRound(alpha * b + (1.0 - alpha) * backdrop.blue()));

            out += body.mid(last, m.capturedStart() - last);
            out += solid.name();
            last = m.capturedEnd();
        }
        out += body.mid(last);
        return out;
    };

    // Every class whose row styling should carry across to our tree. SourceTree
    // is included so we follow the Sources dock exactly when a theme singles it
    // out, and QTreeView so a theme's own tree rule gets the same flattening.
    static const QStringList kPrefixes = {
        QStringLiteral("QListView::item"),
        QStringLiteral("QListWidget::item"),
        QStringLiteral("SourceTree::item"),
        QStringLiteral("QTreeView::item"),
    };

    QString result;
    QRegularExpressionMatchIterator it = blockRe.globalMatch(appQss);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString body = match.captured(2).trimmed();
        if (body.isEmpty()) {
            continue;
        }

        QStringList rewritten;
        const QStringList selectors = match.captured(1).split(QLatin1Char(','));
        for (const QString &rawSelector : selectors) {
            const QString selector = rawSelector.trimmed();
            for (const QString &prefix : kPrefixes) {
                if (!selector.startsWith(prefix)) {
                    continue;
                }
                // Everything after "::item" is the pseudo-state chain
                // (:hover, :selected:hover, :disabled and so on). Carried
                // across untouched so the whole state matrix comes with it.
                const QString states = selector.mid(prefix.length());
                // A descendant selector would target a child widget we do not
                // have; only the row itself transfers cleanly.
                if (states.contains(QLatin1Char(' ')) || states.contains(QLatin1Char('>'))) {
                    continue;
                }
                const QString candidate = QStringLiteral("QTreeView::item") + states;
                if (!rewritten.contains(candidate)) {
                    rewritten << candidate;
                }
                break;
            }
        }

        if (!rewritten.isEmpty()) {
            result += rewritten.join(QStringLiteral(",\n")) + QStringLiteral(" {\n") + flatten(body) +
                      QStringLiteral("\n}\n");
        }
    }

    s_cachedKey = cacheKey;
    s_cachedResult = result;
    return result;
}

// Theme change handler (call this when theme changes)
static void OnThemeChanged()
{
    // Clear theme-dependent caches
    s_themeIconCache.clear();
    s_coloredIconCache.clear(); // Color icons may also be theme-dependent
    s_customIconCache.clear();  // Holds resolved obs: icons, which are themed
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Theme", "Cleared caches for theme change");
}

// Static member initialization
QList<SceneOrganiserDock*> SceneOrganiserDock::s_dockInstances;

//==============================================================================
// SceneOrganiserDock Implementation
//==============================================================================

SceneOrganiserDock::SceneOrganiserDock(CanvasType canvasType, QWidget *parent)
    : QFrame(parent)
    , m_canvasType(canvasType)
    , m_mainLayout(nullptr)
    , m_treeView(nullptr)
    , m_model(nullptr)
    , m_proxyModel(nullptr)
    , m_searchWidget(nullptr)
    , m_searchLayout(nullptr)
    , m_searchEdit(nullptr)
    , m_toolbar(nullptr)
    , m_addFolderAction(nullptr)
    , m_removeAction(nullptr)
    , m_filtersAction(nullptr)
    , m_moveUpAction(nullptr)
    , m_moveDownAction(nullptr)
    , m_addButton(nullptr)
    , m_removeButton(nullptr)
    , m_filtersButton(nullptr)
    , m_moveUpButton(nullptr)
    , m_moveDownButton(nullptr)
    , m_expandCollapseButton(nullptr)
    , m_lockButton(nullptr)
    , m_settingsButton(nullptr)
    , m_folderContextMenu(nullptr)
    , m_sceneContextMenu(nullptr)
    , m_backgroundContextMenu(nullptr)
    , m_sceneOrderMenu(nullptr)
    , m_sceneProjectorMenu(nullptr)
    , m_folderLockAction(nullptr)
    , m_sceneLockAction(nullptr)
    , m_backgroundLockAction(nullptr)
    , m_saveTimer(new QTimer(this))
    , m_copyFiltersSource(nullptr)
    , m_currentContextItem(nullptr)
    , m_isLocked(false)
    , m_initialLoadComplete(false)
    , m_allExpanded(false)
    , m_hideSceneAction(nullptr)
    , m_showSceneAction(nullptr)
    , m_updateBatchTimer(new QTimer(this))
    , m_updatesPending(false)
    , currentThemeIsDark(false)
{
    s_dockInstances.append(this);

    // Set configuration key. Per canvas, so the Normal and Vertical docks keep
    // their own lock state, hidden scenes and folder expansion.
    m_configKey = (m_canvasType == CanvasType::Vertical) ? "scene_organiser_vertical"
                                                         : "scene_organiser_normal";

    // Initialize optimized update system
    m_updateBatchTimer->setSingleShot(true);
    m_updateBatchTimer->setInterval(30); // 30ms batching delay for smooth updates
    connect(m_updateBatchTimer, &QTimer::timeout, this, &SceneOrganiserDock::processBatchedUpdates);

    setupSearchBar();
    setupUI();
    setupContextMenu();
    setupObsSignals();

    // Setup auto-save timer
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(300); // Save 300ms after last change for responsive saving
    connect(m_saveTimer, &QTimer::timeout, this, &SceneOrganiserDock::SaveConfiguration);


    // Load configuration after setup
    QTimer::singleShot(100, this, &SceneOrganiserDock::LoadConfiguration);

    // Initialize current theme state
    currentThemeIsDark = StreamUP::UIHelpers::IsOBSThemeDark();


    // Initialize toggle icons state in context menus
    updateToggleIconsState();

    // Initialize lock action states in context menus
    updateLockActionStates();

    // Initialize active scene highlighting
    QTimer::singleShot(200, this, &SceneOrganiserDock::updateActiveSceneHighlight);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Initialize",
        "Scene Organiser Dock created for normal canvas");
}

SceneOrganiserDock::~SceneOrganiserDock()
{
    // Remove frontend event callback to prevent use-after-free
    obs_frontend_remove_event_callback(onFrontendEvent, this);
    disconnectCanvasSignals();

    signal_handler_disconnect(obs_get_signal_handler(), "source_rename", OnSourceRenamed, this);

    // Clean up copy filters source
    if (m_copyFiltersSource) {
        obs_weak_source_release(m_copyFiltersSource);
        m_copyFiltersSource = nullptr;
    }

    s_dockInstances.removeAll(this);
    SaveConfiguration();
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cleanup", "Scene Organiser Dock destroyed");
}


void SceneOrganiserDock::NotifyAllDocksSettingsChanged()
{
    for (auto dock : s_dockInstances) {
        dock->onSettingsChanged();
    }
}

void SceneOrganiserDock::NotifySceneOrganiserIconsChanged()
{
    for (auto dock : s_dockInstances) {
        dock->onIconsChanged();
    }
}


void SceneOrganiserDock::setupUI()
{
    setObjectName("StreamUPSceneOrganiserNormal");

    // Use zero margins and spacing to match OBS dock style
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create model, proxy model, and view
    m_model = new SceneTreeModel(m_canvasType, this);

    // Create proxy model for search/filtering
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setRecursiveFilteringEnabled(true);

    // Create tree view - SceneTreeView only for event handling, no custom styling
    m_treeView = new SceneTreeView(this);
    m_treeView->setModel(m_proxyModel);

    // Install custom delegate to handle color painting (overrides theme stylesheet)
    CustomColorDelegate *colorDelegate = new CustomColorDelegate(this, m_treeView);
    m_treeView->setItemDelegate(colorDelegate);

    // Set object name to match OBS scenes list for proper theme styling
    // NOTE: Using "scenes" will apply OBS theme styling for the native scenes dock
    // Try changing this to see if it affects the double-selection issue
    m_treeView->setObjectName("streamupSceneOrganiser");  // Try: "sceneorganiser", "streamupScenes", or ""

    // Borrow the theme's own row rules (see BuildThemeRowQss). Re-applied on
    // every theme change so hover never drifts away from the docks beside us.
    applyThemeRowStyling();

    // DEBUG: Log the current stylesheet to see what's being applied
    QString currentStyleSheet = m_treeView->styleSheet();
    if (!currentStyleSheet.isEmpty()) {
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "TreeViewStyle",
            QString("TreeView has custom stylesheet: %1").arg(currentStyleSheet).toUtf8().constData());
    }

    // DEBUG: Log the palette to see selection colors
    QPalette pal = m_treeView->palette();
    QColor highlight = pal.color(QPalette::Highlight);
    QColor highlightText = pal.color(QPalette::HighlightedText);
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "TreeViewPalette",
        QString("Highlight color: %1, HighlightedText: %2")
        .arg(highlight.name()).arg(highlightText.name()).toUtf8().constData());

    // Configure tree view with minimal settings - match OBS scenes dock
    m_treeView->setHeaderHidden(true);

    // Stated rather than left to the default, so the tree and the flat tabs are
    // known to agree: names elide, the view never scrolls sideways.
    m_treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_treeView->setTextElideMode(Qt::ElideRight);
    // Extended selection so a reorganise can move a run of scenes in one drag.
    // Ctrl-click adds, Shift-click takes a range, and the drag payload carries
    // whatever is selected - see SceneTreeModel::mimeData().
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeView->setDragDropMode(QAbstractItemView::InternalMove);
    m_treeView->setDefaultDropAction(Qt::MoveAction);
    m_treeView->setDropIndicatorShown(true);
    m_treeView->setIndentation(20);
    m_treeView->setRootIsDecorated(true);

    m_treeView->setExpandsOnDoubleClick(false);

    // Apply initial item height setting. sceneOrganiserItemHeight is now an absolute
    // row height in pixels (19-48, default 24 = native OBS row). The delegate's
    // sizeHint() enforces the row height; icon and font are derived from it so the
    // row scales as a whole (previously only the icon grew past a certain point).
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    int rowHeight = settings.sceneOrganiserItemHeight;
    int iconSize = std::max(10, rowHeight - 8);   // 24px row -> 16px icon (matches native)
    m_treeView->setIconSize(QSize(iconSize, iconSize));

    int fontSize = std::max(8, (11 * rowHeight) / 24);  // 24px row -> 11pt (matches native)
    QFont font = m_treeView->font();
    font.setPointSize(fontSize);
    m_treeView->setFont(font);

    // Connect signals
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &SceneOrganiserDock::onSceneSelectionChanged);
    connect(m_treeView, &QAbstractItemView::clicked,
            this, &SceneOrganiserDock::onItemClicked);
    connect(m_treeView, &QAbstractItemView::doubleClicked,
            this, &SceneOrganiserDock::onItemDoubleClicked);
    connect(m_treeView, &QWidget::customContextMenuRequested,
            this, &SceneOrganiserDock::onCustomContextMenuRequested);

    // Connect expansion signals to save folder state and update button
    connect(m_treeView, &QTreeView::expanded, this, [this](const QModelIndex &index) {
        setFolderExpandedState(index, true);
        m_saveTimer->start();
        updateExpandCollapseButtonState();
    });
    connect(m_treeView, &QTreeView::collapsed, this, [this](const QModelIndex &index) {
        setFolderExpandedState(index, false);
        m_saveTimer->start();
        updateExpandCollapseButtonState();
    });

    // A folder rename finishes in the inline editor, long after the menu action
    // returned, so the undo entry is pushed when the item's text actually
    // changes rather than when editing was requested.
    connect(m_model, &QStandardItemModel::itemChanged, this, [this](QStandardItem *item) {
        if (m_renameLayoutBefore.isEmpty() || !item) {
            return;
        }
        const QString before = m_renameLayoutBefore;
        m_renameLayoutBefore.clear();
        pushLayoutUndo(QString::fromUtf8(obs_module_text("SceneOrganiser.Undo.RenameFolder")), before);
    });

    connect(m_model, &SceneTreeModel::modelChanged,
            [this]() {
                applySortingIfEnabled();
                m_saveTimer->start();
                // Use optimized batched update instead of immediate repaints
                scheduleOptimizedUpdate();
            });

    // Tabs switch the whole dock between the organiser tree and the flat lists.
    // The tree is still the default and is untouched by the other tabs. The bar
    // itself is added further down, under the search field - see
    // createBottomToolbar().
    setupQuickTabs();
    m_mainLayout->addWidget(m_viewStack, 1);

    // Create and add the toolbar at the bottom
    createBottomToolbar();
}

// Copies the height of OBS's Sources dock toolbar onto ours. Runs once the
// widget tree is up, since a toolbar has no meaningful height before that.
void SceneOrganiserDock::matchObsToolbarHeight()
{
    if (!m_toolbar) {
        return;
    }

    QWidget *mainWindow = static_cast<QWidget *>(obs_frontend_get_main_window());
    if (!mainWindow) {
        return;
    }

    // sourcesToolbar is the object name OBS gives it in OBSBasic.ui.
    QToolBar *reference = mainWindow->findChild<QToolBar *>("sourcesToolbar");
    if (!reference) {
        return;
    }

    const int height = reference->sizeHint().height();
    if (height > 0) {
        m_toolbar->setFixedHeight(height);
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Toolbar",
            QString("Matched OBS sources toolbar height: %1px").arg(height).toUtf8().constData());
    }
}

void SceneOrganiserDock::createBottomToolbar()
{
    if (!m_mainLayout) {
        return;
    }

    // Add search bar above the toolbar
    if (m_searchWidget) {
        m_mainLayout->addWidget(m_searchWidget);
    }

    // Tabs sit under the search field, so the dock's controls are gathered at
    // the bottom together rather than split across both ends.
    if (m_quickTabs) {
        m_mainLayout->addWidget(m_quickTabs, 0);
    }

    // Create a proper QToolBar like in the multidock
    m_toolbar = new QToolBar("Scene Organiser Controls", this);
    m_toolbar->setObjectName("SceneOrganiserBottomToolbar");
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setOrientation(Qt::Horizontal);

    // Match native OBS scenes dock styling exactly
    m_toolbar->setIconSize(QSize(StreamUP::UIStyles::S(16), StreamUP::UIStyles::S(16)));  // Standard OBS toolbar icon size

    // Let OBS theme handle all toolbar styling - no custom stylesheets
    // The toolbar will inherit appearance from the active OBS theme

    // Create QToolButtons and add them to toolbar (to properly support themeID)
    // This approach allows themeID properties to work while maintaining proper toolbar styling

    // Add button - appears as normal button without dropdown arrow
    QToolButton *addButton = new QToolButton(this);
    addButton->setObjectName("SceneOrganiserAddButton");
    addButton->setProperty("themeID", "addIconSmall");
    addButton->setProperty("class", "icon-plus");
    addButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.Add"));

    // Create a menu for the add button (shown on click, no arrow indicator)
    QMenu *addMenu = new QMenu(this);
    addMenu->setObjectName("SceneOrganiserAddMenu");
    addMenu->addAction(obs_module_text("SceneOrganiser.Action.AddFolder"), this, &SceneOrganiserDock::onAddFolderClicked);
    addMenu->addAction(obs_module_text("SceneOrganiser.Action.CreateScene"), this, &SceneOrganiserDock::onCreateSceneClicked);

    // A tab gains scenes and folders of its own, not OBS scenes and tree folders,
    // so the menu shown depends on which tab is open. Previously this always
    // showed the Scenes menu, which is why the tabs appeared to have no add
    // options at all.
    QMenu *addTabMenu = new QMenu(this);
    addTabMenu->setObjectName("SceneOrganiserAddTabMenu");
    addTabMenu->addAction(obs_module_text("SceneOrganiser.Menu.AddScenes"), this, &SceneOrganiserDock::showAddToTabMenu);
    addTabMenu->addAction(obs_module_text("SceneOrganiser.Action.AddFolder"), this, &SceneOrganiserDock::onAddTabFolderClicked);

    // Show menu on click without dropdown arrow
    connect(addButton, &QToolButton::clicked, [this, addButton, addMenu, addTabMenu]() {
        QMenu *menu = (m_currentKind == QuickTabKind::Scenes) ? addMenu : addTabMenu;
        QPoint pos = addButton->mapToGlobal(QPoint(0, addButton->height()));
        menu->exec(pos);
    });

    m_toolbar->addWidget(addButton);
    m_addFolderAction = nullptr; // No separate action needed

    // Remove button
    QToolButton *removeButton = new QToolButton(this);
    removeButton->setObjectName("SceneOrganiserRemoveButton");
    removeButton->setProperty("themeID", "removeIconSmall");
    removeButton->setProperty("class", "icon-trash");
    removeButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.Remove"));
    removeButton->setEnabled(false);
    connect(removeButton, &QToolButton::clicked, this, &SceneOrganiserDock::onRemoveClicked);

    m_toolbar->addWidget(removeButton);
    m_removeAction = nullptr; // Direct button connection

    // Separator after remove button
    m_toolbar->addSeparator();

    // Filters button (using theme info: .icon-filter uses url(theme:Dark/filter.svg))
    QToolButton *filtersButton = new QToolButton(this);
    filtersButton->setObjectName("SceneOrganiserFiltersButton");
    filtersButton->setProperty("class", "icon-filter");
    filtersButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.Filters"));
    filtersButton->setEnabled(false);
    connect(filtersButton, &QToolButton::clicked, this, &SceneOrganiserDock::onFiltersClicked);

    m_toolbar->addWidget(filtersButton);
    m_filtersAction = nullptr; // Direct button connection

    // Separator after filters button
    m_toolbar->addSeparator();

    // Move up button
    QToolButton *moveUpButton = new QToolButton(this);
    moveUpButton->setObjectName("SceneOrganiserMoveUpButton");
    moveUpButton->setProperty("themeID", "upArrowIconSmall");
    moveUpButton->setProperty("class", "icon-up");
    moveUpButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.MoveUp"));
    moveUpButton->setEnabled(false);
    connect(moveUpButton, &QToolButton::clicked, this, &SceneOrganiserDock::onMoveUpClicked);

    m_toolbar->addWidget(moveUpButton);
    m_moveUpAction = nullptr; // Direct button connection

    // Move down button
    QToolButton *moveDownButton = new QToolButton(this);
    moveDownButton->setObjectName("SceneOrganiserMoveDownButton");
    moveDownButton->setProperty("themeID", "downArrowIconSmall");
    moveDownButton->setProperty("class", "icon-down");
    moveDownButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.MoveDown"));
    moveDownButton->setEnabled(false);
    connect(moveDownButton, &QToolButton::clicked, this, &SceneOrganiserDock::onMoveDownClicked);

    m_toolbar->addWidget(moveDownButton);
    m_moveDownAction = nullptr; // Direct button connection

    // Add spacer to push remaining buttons to the right
    QWidget *spacer = new QWidget(this);
    spacer->setObjectName("SceneOrganiserToolbarSpacer");
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);

    // Create container for right-aligned buttons with consistent spacing
    QWidget *rightButtonsContainer = new QWidget(this);
    rightButtonsContainer->setObjectName("SceneOrganiserRightButtonsContainer");
    rightButtonsContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout *rightButtonsLayout = new QHBoxLayout(rightButtonsContainer);
    rightButtonsLayout->setContentsMargins(0, 0, 0, 0);
    rightButtonsLayout->setSpacing(StreamUP::UIStyles::S(2)); // Match toolbar button spacing

    // Lock checkbox - using exact same approach as OBS source dock
    QCheckBox *lockCheckbox = new QCheckBox(rightButtonsContainer);
    lockCheckbox->setObjectName("SceneOrganiserLockCheckbox");
    lockCheckbox->setProperty("class", "checkbox-icon indicator-lock");
    lockCheckbox->setChecked(false); // Start unlocked
    lockCheckbox->setToolTip(obs_module_text("SceneOrganiser.Tooltip.Unlocked"));
    lockCheckbox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(lockCheckbox, &QCheckBox::toggled, this, &SceneOrganiserDock::onToggleLockClicked);
    rightButtonsLayout->addWidget(lockCheckbox);

    // Expand/Collapse All button - using QCheckBox like OBS does for expand indicators
    QCheckBox *expandCollapseButton = new QCheckBox(rightButtonsContainer);
    expandCollapseButton->setObjectName("SceneOrganiserExpandCollapseButton");
    expandCollapseButton->setProperty("class", "checkbox-icon indicator-expand");
    expandCollapseButton->setChecked(false); // Unchecked = expanded, checked = collapsed
    expandCollapseButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.ExpandAll"));
    expandCollapseButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(expandCollapseButton, &QCheckBox::toggled, this, &SceneOrganiserDock::onExpandCollapseAllClicked);
    rightButtonsLayout->addWidget(expandCollapseButton);

    // Add the container to the toolbar
    m_toolbar->addWidget(rightButtonsContainer);

    // Add separator before settings button (using proper toolbar separator)
    m_toolbar->addSeparator();

    // Settings button - using OBS theming (added directly to toolbar, not in container)
    QToolButton *settingsButton = new QToolButton(this);
    settingsButton->setObjectName("SceneOrganiserSettingsButton");
    settingsButton->setProperty("themeID", "configIconSmall");
    settingsButton->setProperty("class", "icon-gear");
    settingsButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.Settings"));
    connect(settingsButton, &QToolButton::clicked, this, &SceneOrganiserDock::onSettingsClicked);
    m_toolbar->addWidget(settingsButton);

    // Store button references for additional state management if needed
    m_addButton = addButton;
    m_removeButton = removeButton;
    m_filtersButton = filtersButton;
    m_moveUpButton = moveUpButton;
    m_moveDownButton = moveDownButton;
    m_expandCollapseButton = expandCollapseButton;
    m_settingsButton = settingsButton;
    m_lockButton = lockCheckbox;

    // Add toolbar to the bottom of the layout
    m_mainLayout->addWidget(m_toolbar, 0); // 0 means don't stretch

    // Height is taken from OBS's own Sources toolbar rather than styled to a
    // number. Our bar holds real widgets (icon checkboxes in a container, a
    // spacer) where OBS's holds only actions, so the two never size alike from
    // the same rules - every attempt to express this in the theme landed either
    // side of it. Measuring the thing we are matching cannot land beside it.
    QTimer::singleShot(0, this, [this]() { matchObsToolbarHeight(); });

    // Initialize toolbar button states
    updateToolbarState();
}

void SceneOrganiserDock::setupContextMenu()
{
    // Folder context menu
    m_folderContextMenu = new QMenu(this);
    QAction *renameFolderAction = m_folderContextMenu->addAction(obs_module_text("SceneOrganiser.Action.RenameFolder"), this, &SceneOrganiserDock::onRenameFolderClicked);
    renameFolderAction->setShortcut(QKeySequence(Qt::Key_F2));

    m_deleteFolderAction = m_folderContextMenu->addAction(obs_module_text("SceneOrganiser.Action.DeleteFolder"), [this]() {
        // Implement delete folder functionality
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                // Move children to root before deleting folder
                while (item->hasChildren()) {
                    auto child = item->takeChild(0);
                    m_model->invisibleRootItem()->appendRow(child);
                }
                m_model->removeRow(item->row(), item->parent() ? item->parent()->index() : QModelIndex());
                SaveConfiguration(); // Immediate save on folder deletion
            }
        }
    });
    m_deleteFolderAction->setShortcut(QKeySequence(Qt::Key_Delete));

    m_folderContextMenu->addSeparator();
    // One submenu instance serves both context menus; both act on
    // m_currentContextItem, which is set before either menu is exec'd.
    m_colorMenu = createColorSubmenu();
    m_folderContextMenu->addMenu(m_colorMenu);
    // Same instance serves both menus, for the same reason the colour one does.
    m_iconMenu = createIconSubmenu();
    m_folderContextMenu->addMenu(m_iconMenu);
    m_folderContextMenu->addSeparator();
    m_folderToggleIconsAction = m_folderContextMenu->addAction(obs_module_text("SceneOrganiser.Action.ToggleIcons"), this, &SceneOrganiserDock::onToggleIconsClicked);
    m_folderToggleIconsAction->setCheckable(true);

    // Add lock/unlock option
    m_folderContextMenu->addSeparator();
    m_folderLockAction = m_folderContextMenu->addAction("", this, &SceneOrganiserDock::onToggleLockClicked);
    m_folderLockAction->setCheckable(true);

    // Sort submenu for folders
    m_folderContextMenu->addSeparator();
    QMenu *folderSortMenu = new QMenu(obs_module_text("SceneOrganiser.Action.Sort"), this);
    folderSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.AlphabeticalAZ"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                sortManually(StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ, item);
            }
        }
    });
    folderSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.AlphabeticalZA"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                sortManually(StreamUP::SettingsManager::SceneSortMethod::AlphabeticalZA, item);
            }
        }
    });
    folderSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.NewestFirst"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                sortManually(StreamUP::SettingsManager::SceneSortMethod::NewestFirst, item);
            }
        }
    });
    folderSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.OldestFirst"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                sortManually(StreamUP::SettingsManager::SceneSortMethod::OldestFirst, item);
            }
        }
    });
    m_folderContextMenu->addMenu(folderSortMenu);

    // Scene context menu - matching OBS standard functionality
    m_sceneContextMenu = new QMenu(this);

    // Main actions
    QAction *renameSceneAction = m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Rename"), -1), this, &SceneOrganiserDock::onRenameSceneClicked);
    renameSceneAction->setShortcut(QKeySequence(Qt::Key_F2));
    m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Duplicate"), -1), this, &SceneOrganiserDock::onDuplicateSceneClicked);

    // Filter actions
    m_sceneContextMenu->addSeparator();
    m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Copy.Filters"), -1), this, &SceneOrganiserDock::onCopyFiltersClicked);
    m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Paste.Filters"), -1), this, &SceneOrganiserDock::onPasteFiltersClicked);

    // Delete action
    m_sceneContextMenu->addSeparator();
    m_deleteSceneAction = m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Remove"), -1), this, &SceneOrganiserDock::onDeleteSceneClicked);
    m_deleteSceneAction->setShortcut(QKeySequence(Qt::Key_Delete));

    // Favourite toggle - drives the Favourites tab
    m_favouriteToggleAction = m_sceneContextMenu->addAction(obs_module_text("SceneOrganiser.Action.AddFavourite"), this, &SceneOrganiserDock::onToggleFavouriteClicked);

    // Custom tabs the scene can be put in. Rebuilt each time the menu opens.
    m_addToTabMenu = new QMenu(obs_module_text("SceneOrganiser.Menu.AddToTab"), this);
    m_sceneContextMenu->addMenu(m_addToTabMenu);

    // Scene visibility actions
    m_hideSceneAction = m_sceneContextMenu->addAction(obs_module_text("SceneOrganiser.Action.HideScene"), this, &SceneOrganiserDock::onHideSceneClicked);
    m_showSceneAction = m_sceneContextMenu->addAction(obs_module_text("SceneOrganiser.Action.ShowScene"), this, &SceneOrganiserDock::onShowSceneClicked);

    // Order submenu
    m_sceneContextMenu->addSeparator();
    m_sceneOrderMenu = new QMenu(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order"), -1), this);
    m_sceneMoveUpAction = m_sceneOrderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveUp"), -1), this, &SceneOrganiserDock::onSceneMoveUpClicked);
    m_sceneMoveDownAction = m_sceneOrderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveDown"), -1), this, &SceneOrganiserDock::onSceneMoveDownClicked);
    m_sceneOrderMenu->addSeparator();
    m_sceneMoveToTopAction = m_sceneOrderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveToTop"), -1), this, &SceneOrganiserDock::onSceneMoveToTopClicked);
    m_sceneMoveToBottomAction = m_sceneOrderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveToBottom"), -1), this, &SceneOrganiserDock::onSceneMoveToBottomClicked);
    m_sceneContextMenu->addMenu(m_sceneOrderMenu);

    // Projector submenu
    m_sceneProjectorMenu = new QMenu(QString::fromUtf8(obs_frontend_get_locale_string("Projector.Open.Scene"), -1), this);
    // We'll populate projector options dynamically when the menu is shown
    m_sceneProjectorMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Projector.Window"), -1), this, &SceneOrganiserDock::onOpenProjectorWindowClicked);
    m_sceneContextMenu->addMenu(m_sceneProjectorMenu);

    // Transition override, matching the scene list in OBS itself. Main canvas
    // only: the vertical canvas runs its scene switches through Aitum's own
    // transition, which does not read these settings, so offering it there
    // would be a control that quietly does nothing.
    if (m_canvasType == CanvasType::Normal) {
        m_sceneTransitionMenu = new QMenu(QString::fromUtf8(obs_frontend_get_locale_string("TransitionOverride"), -1), this);
        m_sceneContextMenu->addMenu(m_sceneTransitionMenu);
    }

    // Linked scenes, the same as Aitum's own vertical scene list. Vertical dock
    // only: a link says "when this main scene goes live, put that vertical scene
    // on the vertical canvas", so it only means anything from the vertical side.
    if (m_canvasType == CanvasType::Vertical) {
        m_sceneLinkedScenesMenu = new QMenu(obs_module_text("SceneOrganiser.Action.LinkedScenes"), this);
        m_sceneContextMenu->addMenu(m_sceneLinkedScenesMenu);
    }

    // Additional OBS actions
    m_sceneContextMenu->addSeparator();
    m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Screenshot.Scene"), -1), this, &SceneOrganiserDock::onScreenshotSceneClicked);
    m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Filters"), -1), this, &SceneOrganiserDock::onSceneFiltersClicked);

    // Show in multiview toggle
    m_sceneContextMenu->addSeparator();
    m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("ShowInMultiview"), -1), this, &SceneOrganiserDock::onShowInMultiviewClicked);

    // Custom actions
    m_sceneContextMenu->addSeparator();
    m_sceneContextMenu->addMenu(m_colorMenu);
    m_sceneContextMenu->addMenu(m_iconMenu);
    m_sceneContextMenu->addSeparator();
    m_sceneToggleIconsAction = m_sceneContextMenu->addAction(obs_module_text("SceneOrganiser.Action.ToggleIcons"), this, &SceneOrganiserDock::onToggleIconsClicked);
    m_sceneToggleIconsAction->setCheckable(true);

    // Add lock/unlock option
    m_sceneContextMenu->addSeparator();
    m_sceneLockAction = m_sceneContextMenu->addAction("", this, &SceneOrganiserDock::onToggleLockClicked);
    m_sceneLockAction->setCheckable(true);

    // Sort submenu for scenes (sorts parent folder)
    m_sceneContextMenu->addSeparator();
    QMenu *sceneSortMenu = new QMenu(obs_module_text("SceneOrganiser.Action.Sort"), this);
    sceneSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.AlphabeticalAZ"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneTreeItem::UserType + 2) {
                // Sort the parent folder (or root if no parent)
                QStandardItem *parent = item->parent();
                sortManually(StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ, parent);
            }
        }
    });
    sceneSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.AlphabeticalZA"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneTreeItem::UserType + 2) {
                QStandardItem *parent = item->parent();
                sortManually(StreamUP::SettingsManager::SceneSortMethod::AlphabeticalZA, parent);
            }
        }
    });
    sceneSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.NewestFirst"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneTreeItem::UserType + 2) {
                QStandardItem *parent = item->parent();
                sortManually(StreamUP::SettingsManager::SceneSortMethod::NewestFirst, parent);
            }
        }
    });
    sceneSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.OldestFirst"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneTreeItem::UserType + 2) {
                QStandardItem *parent = item->parent();
                sortManually(StreamUP::SettingsManager::SceneSortMethod::OldestFirst, parent);
            }
        }
    });
    m_sceneContextMenu->addMenu(sceneSortMenu);

    // Background context menu
    m_backgroundContextMenu = new QMenu(this);
    m_backgroundContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("AddScene"), -1), this, &SceneOrganiserDock::onCreateSceneClicked);
    m_backgroundContextMenu->addAction(obs_module_text("SceneOrganiser.Action.AddFolder"), this, &SceneOrganiserDock::onAddFolderClicked);
    m_backgroundContextMenu->addSeparator();
    m_backgroundToggleIconsAction = m_backgroundContextMenu->addAction(obs_module_text("SceneOrganiser.Action.ToggleIcons"), this, &SceneOrganiserDock::onToggleIconsClicked);
    m_backgroundToggleIconsAction->setCheckable(true);

    // Add lock/unlock option
    m_backgroundContextMenu->addSeparator();
    m_backgroundLockAction = m_backgroundContextMenu->addAction("", this, &SceneOrganiserDock::onToggleLockClicked);
    m_backgroundLockAction->setCheckable(true);

    // Sort submenu for background (sorts entire tree)
    m_backgroundContextMenu->addSeparator();
    QMenu *backgroundSortMenu = new QMenu(obs_module_text("SceneOrganiser.Action.Sort"), this);
    backgroundSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.AlphabeticalAZ"), [this]() {
        sortManually(StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ);
    });
    backgroundSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.AlphabeticalZA"), [this]() {
        sortManually(StreamUP::SettingsManager::SceneSortMethod::AlphabeticalZA);
    });
    backgroundSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.NewestFirst"), [this]() {
        sortManually(StreamUP::SettingsManager::SceneSortMethod::NewestFirst);
    });
    backgroundSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.OldestFirst"), [this]() {
        sortManually(StreamUP::SettingsManager::SceneSortMethod::OldestFirst);
    });
    m_backgroundContextMenu->addMenu(backgroundSortMenu);
}

void SceneOrganiserDock::setupObsSignals()
{
    obs_frontend_add_event_callback(onFrontendEvent, this);

    // Favourites, custom tabs and hidden scenes are all keyed by scene name, so
    // they have to hear about renames from wherever they happen - the scene
    // list, a hotkey, a websocket call. The global signal is the only place
    // that sees all of them.
    signal_handler_connect(obs_get_signal_handler(), "source_rename", OnSourceRenamed, this);
    connectCanvasSignals();
    watchVerticalCurrentScene();
}

// The canvas signals below cover scenes being added, removed and renamed, but
// not the live scene changing: Aitum switches through its own transition, which
// stays put on the canvas channel, so channel_change never fires and the green
// on-air marker sat on whichever scene was live when the dock loaded.
//
// There is no signal to connect to and no proc to subscribe with, so this asks.
// One call a second, only while a vertical dock exists, and the tree is only
// touched when the answer actually changes.
void SceneOrganiserDock::watchVerticalCurrentScene()
{
    if (m_canvasType != CanvasType::Vertical || m_verticalSceneWatch)
        return;

    m_verticalSceneWatch = new QTimer(this);
    m_verticalSceneWatch->setInterval(1000);
    connect(m_verticalSceneWatch, &QTimer::timeout, this, [this]() {
        if (!m_initialLoadComplete)
            return;

        QString name;
        if (obs_source_t *current = Canvas::GetCurrentScene(m_canvasType)) {
            name = QString::fromUtf8(obs_source_get_name(current));
            obs_source_release(current);
        }

        if (name == m_lastVerticalScene)
            return;

        m_lastVerticalScene = name;
        updateActiveSceneHighlight();
    });
    m_verticalSceneWatch->start();
}

// Canvas scenes are invisible to the frontend's scene events: adding a scene on
// Aitum's canvas raises no SCENE_LIST_CHANGED, and switching it raises no
// SCENE_CHANGED. The canvas has its own signal handler, so the vertical dock
// listens there and refreshes off the same code paths the frontend events use.
void SceneOrganiserDock::connectCanvasSignals()
{
    if (m_canvasType != CanvasType::Vertical || m_weakCanvas)
        return;

    obs_canvas_t *canvas = Canvas::Acquire(m_canvasType);
    if (!canvas)
        return;

    signal_handler_t *sh = obs_canvas_get_signal_handler(canvas);
    if (sh) {
        signal_handler_connect(sh, "source_add", OnCanvasSourceAdded, this);
        signal_handler_connect(sh, "source_remove", OnCanvasSourceRemoved, this);
        signal_handler_connect(sh, "source_rename", OnCanvasSourceRenamed, this);
        signal_handler_connect(sh, "channel_change", OnCanvasChannelChanged, this);
        // Weak, so a canvas torn down before this dock cannot keep it alive.
        m_weakCanvas = obs_canvas_get_weak_canvas(canvas);
    }
    obs_canvas_release(canvas);
}

void SceneOrganiserDock::disconnectCanvasSignals()
{
    if (!m_weakCanvas)
        return;

    // The canvas may already be gone, in which case its signal handler went with
    // it and there is nothing to disconnect.
    if (obs_canvas_t *canvas = obs_weak_canvas_get_canvas(m_weakCanvas)) {
        if (signal_handler_t *sh = obs_canvas_get_signal_handler(canvas)) {
            signal_handler_disconnect(sh, "source_add", OnCanvasSourceAdded, this);
            signal_handler_disconnect(sh, "source_remove", OnCanvasSourceRemoved, this);
            signal_handler_disconnect(sh, "source_rename", OnCanvasSourceRenamed, this);
            signal_handler_disconnect(sh, "channel_change", OnCanvasChannelChanged, this);
        }
        obs_canvas_release(canvas);
    }

    obs_weak_canvas_release(m_weakCanvas);
    m_weakCanvas = nullptr;
}

// Signal callbacks arrive on OBS threads, never the UI thread, so every one of
// these hops to the dock's thread before touching the model or widgets.
void SceneOrganiserDock::OnCanvasSourceAdded(void *data, calldata_t *)
{
    auto *dock = static_cast<SceneOrganiserDock *>(data);
    QMetaObject::invokeMethod(dock, [dock]() {
        if (!dock->m_initialLoadComplete)
            return;
        dock->refreshSceneList();
    }, Qt::QueuedConnection);
}

void SceneOrganiserDock::OnCanvasSourceRemoved(void *data, calldata_t *)
{
    auto *dock = static_cast<SceneOrganiserDock *>(data);
    QMetaObject::invokeMethod(dock, [dock]() {
        if (!dock->m_initialLoadComplete)
            return;
        dock->refreshSceneList();
    }, Qt::QueuedConnection);
}

void SceneOrganiserDock::OnSourceRenamed(void *data, calldata_t *cd)
{
    auto *dock = static_cast<SceneOrganiserDock *>(data);
    if (!dock || !cd) {
        return;
    }

    const char *prevName = calldata_string(cd, "prev_name");
    const char *newName = calldata_string(cd, "new_name");
    if (!prevName || !newName || strcmp(prevName, newName) == 0) {
        return;
    }

    const QString oldNameCopy = QString::fromUtf8(prevName);
    const QString newNameCopy = QString::fromUtf8(newName);

    // The signal arrives on OBS' thread; everything this touches is Qt state.
    QMetaObject::invokeMethod(dock, [dock, oldNameCopy, newNameCopy]() {
        dock->renameStoredScene(oldNameCopy, newNameCopy);
    }, Qt::QueuedConnection);
}

void SceneOrganiserDock::renameSceneInNodes(QVector<TabNode> &nodes, const QString &oldName, const QString &newName)
{
    for (TabNode &node : nodes) {
        if (node.isFolder) {
            // A folder's name is the tab's own, not a scene's, so it is left alone.
            renameSceneInNodes(node.children, oldName, newName);
        } else if (node.name == oldName) {
            node.name = newName;
        }
    }
}

// The Scenes tree survives a rename on its own - it tracks scenes by weak
// source and simply relabels the item. These three do not: they hold names, and
// a name that no longer matches anything means the scene silently disappears
// from a tab or quietly stops being hidden. So they are rewritten here.
void SceneOrganiserDock::renameStoredScene(const QString &oldName, const QString &newName)
{
    bool changed = false;
    bool hiddenChanged = false;

    if (m_hiddenScenes.contains(oldName)) {
        m_hiddenScenes.remove(oldName);
        m_hiddenScenes.insert(newName);
        changed = true;
        hiddenChanged = true;
    }

    const int recentIndex = m_recentScenes.indexOf(oldName);
    if (recentIndex >= 0) {
        m_recentScenes[recentIndex] = newName;
        changed = true;
    }

    if (nodesContainScene(m_favouriteNodes, oldName)) {
        renameSceneInNodes(m_favouriteNodes, oldName, newName);
        changed = true;
    }

    for (CustomSceneTab &tab : m_customTabs) {
        if (nodesContainScene(tab.nodes, oldName)) {
            renameSceneInNodes(tab.nodes, oldName, newName);
            changed = true;
        }
    }

    if (!changed) {
        return;
    }

    if (hiddenChanged) {
        // The row's greyed-out styling is applied by name too, so it has to be
        // reapplied or the renamed scene looks visible while still being hidden.
        applySceneVisibility();
        updateHiddenScenesStyling();
    }

    refreshQuickList();
    SaveConfiguration();

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Rename",
        QString("Followed rename '%1' -> '%2' through the stored lists")
            .arg(oldName, newName)
            .toUtf8()
            .constData());
}

void SceneOrganiserDock::OnCanvasSourceRenamed(void *data, calldata_t *)
{
    auto *dock = static_cast<SceneOrganiserDock *>(data);
    QMetaObject::invokeMethod(dock, [dock]() {
        if (!dock->m_initialLoadComplete)
            return;
        dock->refreshSceneList();
    }, Qt::QueuedConnection);
}

void SceneOrganiserDock::OnCanvasChannelChanged(void *data, calldata_t *)
{
    auto *dock = static_cast<SceneOrganiserDock *>(data);
    QMetaObject::invokeMethod(dock, [dock]() {
        if (!dock->m_initialLoadComplete)
            return;
        dock->updateActiveSceneHighlight();
    }, Qt::QueuedConnection);
}

void SceneOrganiserDock::setupSearchBar()
{
    // Create search widget container
    m_searchWidget = new QWidget(this);
    m_searchLayout = new QHBoxLayout(m_searchWidget);
    m_searchLayout->setContentsMargins(StreamUP::UIStyles::S(4), StreamUP::UIStyles::S(2), StreamUP::UIStyles::S(4), StreamUP::UIStyles::S(2));
    m_searchLayout->setSpacing(StreamUP::UIStyles::S(4));

    // Create search input
    m_searchEdit = new QLineEdit(m_searchWidget);
    m_searchEdit->setPlaceholderText(obs_module_text("SceneOrganiser.Search.Placeholder"));
    m_searchEdit->setClearButtonEnabled(true);

    // Layout search elements
    m_searchLayout->addWidget(m_searchEdit);

    // Connect search functionality
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SceneOrganiserDock::onSearchTextChanged);

    // Enter goes live with the first scene still showing under the filter.
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &SceneOrganiserDock::activateFirstSearchMatch);

    // Add Escape key shortcut to clear search
    QAction *escapeAction = new QAction(this);
    escapeAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(escapeAction, &QAction::triggered, this, &SceneOrganiserDock::onClearSearch);
    m_searchEdit->addAction(escapeAction);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Search", "Search bar initialized");
}

void SceneOrganiserDock::refreshSceneList()
{
    m_model->updateTree();
    updateActiveSceneHighlight();
    updateHiddenScenesStyling();
    applySceneVisibility();
    applySortingIfEnabled();
    updateExpandCollapseButtonState();
}

void SceneOrganiserDock::applySortingIfEnabled()
{
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();

    // Don't sort if method is None
    if (settings.sceneOrganiserSortMethod == StreamUP::SettingsManager::SceneSortMethod::None) {
        return;
    }

    QStandardItem *root = m_model->invisibleRootItem();
    if (!root) return;

    bool groupFolders = settings.sceneOrganiserGroupFolders;
    auto sortMethod = settings.sceneOrganiserSortMethod;

    // Recursively sort items within each parent
    std::function<void(QStandardItem*)> sortItemsRecursive = [&](QStandardItem *parent) {
        if (!parent) return;

        int rowCount = parent->rowCount();
        if (rowCount <= 1) return;

        // Simple bubble sort to swap rows into correct position
        bool swapped;
        do {
            swapped = false;
            for (int i = 0; i < parent->rowCount() - 1; ++i) {
                QStandardItem *item1 = parent->child(i);
                QStandardItem *item2 = parent->child(i + 1);

                if (!item1 || !item2) continue;

                bool item1IsFolder = (item1->type() == SceneFolderItem::UserType + 1);
                bool item2IsFolder = (item2->type() == SceneFolderItem::UserType + 1);

                bool shouldSwap = false;

                if (groupFolders) {
                    // Folders should come before scenes
                    if (!item1IsFolder && item2IsFolder) {
                        shouldSwap = true;
                    }
                    // Within same type, sort by selected method
                    else if (item1IsFolder == item2IsFolder) {
                        if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ ||
                            sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalZA) {
                            int comparison = QString::compare(item1->text(), item2->text(), Qt::CaseInsensitive);
                            if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ) {
                                shouldSwap = (comparison > 0);
                            } else {
                                shouldSwap = (comparison < 0);
                            }
                        } else if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst ||
                                   sortMethod == StreamUP::SettingsManager::SceneSortMethod::OldestFirst) {
                            // Get timestamps
                            qint64 timestamp1 = 0, timestamp2 = 0;
                            if (item1IsFolder) {
                                SceneFolderItem *folder1 = static_cast<SceneFolderItem*>(item1);
                                SceneFolderItem *folder2 = static_cast<SceneFolderItem*>(item2);
                                timestamp1 = folder1->getCreationTimestamp();
                                timestamp2 = folder2->getCreationTimestamp();
                            } else {
                                SceneTreeItem *scene1 = static_cast<SceneTreeItem*>(item1);
                                SceneTreeItem *scene2 = static_cast<SceneTreeItem*>(item2);
                                timestamp1 = scene1->getCreationTimestamp();
                                timestamp2 = scene2->getCreationTimestamp();
                            }

                            if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst) {
                                // Newest first: higher timestamp comes first
                                shouldSwap = (timestamp1 < timestamp2);
                            } else {
                                // Oldest first: lower timestamp comes first
                                shouldSwap = (timestamp1 > timestamp2);
                            }
                        }
                    }
                } else {
                    // Mix folders and scenes, sort by selected method
                    if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ ||
                        sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalZA) {
                        int comparison = QString::compare(item1->text(), item2->text(), Qt::CaseInsensitive);
                        if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ) {
                            shouldSwap = (comparison > 0);
                        } else {
                            shouldSwap = (comparison < 0);
                        }
                    } else if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst ||
                               sortMethod == StreamUP::SettingsManager::SceneSortMethod::OldestFirst) {
                        // Get timestamps (works for both folders and scenes)
                        qint64 timestamp1 = 0, timestamp2 = 0;
                        if (item1->type() == SceneFolderItem::UserType + 1) {
                            SceneFolderItem *folder1 = static_cast<SceneFolderItem*>(item1);
                            timestamp1 = folder1->getCreationTimestamp();
                        } else {
                            SceneTreeItem *scene1 = static_cast<SceneTreeItem*>(item1);
                            timestamp1 = scene1->getCreationTimestamp();
                        }

                        if (item2->type() == SceneFolderItem::UserType + 1) {
                            SceneFolderItem *folder2 = static_cast<SceneFolderItem*>(item2);
                            timestamp2 = folder2->getCreationTimestamp();
                        } else {
                            SceneTreeItem *scene2 = static_cast<SceneTreeItem*>(item2);
                            timestamp2 = scene2->getCreationTimestamp();
                        }

                        if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst) {
                            shouldSwap = (timestamp1 < timestamp2);
                        } else {
                            shouldSwap = (timestamp1 > timestamp2);
                        }
                    }
                }

                if (shouldSwap) {
                    // Swap the two rows
                    QList<QStandardItem*> row1 = parent->takeRow(i);
                    QList<QStandardItem*> row2 = parent->takeRow(i); // i because we just removed row i
                    parent->insertRow(i, row2);
                    parent->insertRow(i + 1, row1);
                    swapped = true;
                }
            }
        } while (swapped);

        // Recursively sort all folders
        for (int i = 0; i < parent->rowCount(); ++i) {
            QStandardItem *item = parent->child(i);
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                sortItemsRecursive(item);
            }
        }
    };

    sortItemsRecursive(root);
    m_model->saveSceneTree();
}

void SceneOrganiserDock::sortManually(StreamUP::SettingsManager::SceneSortMethod method, QStandardItem *parent)
{
    // If no parent specified, sort the root
    if (!parent) {
        parent = m_model->invisibleRootItem();
    }

    if (!parent) return;

    // Get grouping setting
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    bool groupFolders = settings.sceneOrganiserGroupFolders;

    // Use the same sorting logic as applySortingIfEnabled
    std::function<void(QStandardItem*, StreamUP::SettingsManager::SceneSortMethod)> sortItemsRecursive =
        [&](QStandardItem *currentParent, StreamUP::SettingsManager::SceneSortMethod sortMethod) {
        if (!currentParent) return;

        int rowCount = currentParent->rowCount();
        if (rowCount <= 1) return;

        // Simple bubble sort to swap rows into correct position
        bool swapped;
        do {
            swapped = false;
            for (int i = 0; i < currentParent->rowCount() - 1; ++i) {
                QStandardItem *item1 = currentParent->child(i);
                QStandardItem *item2 = currentParent->child(i + 1);

                if (!item1 || !item2) continue;

                bool item1IsFolder = (item1->type() == SceneFolderItem::UserType + 1);
                bool item2IsFolder = (item2->type() == SceneFolderItem::UserType + 1);

                bool shouldSwap = false;

                if (groupFolders) {
                    // Folders should come before scenes
                    if (!item1IsFolder && item2IsFolder) {
                        shouldSwap = true;
                    }
                    // Within same type, sort by selected method
                    else if (item1IsFolder == item2IsFolder) {
                        if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ ||
                            sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalZA) {
                            int comparison = QString::compare(item1->text(), item2->text(), Qt::CaseInsensitive);
                            if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ) {
                                shouldSwap = (comparison > 0);
                            } else {
                                shouldSwap = (comparison < 0);
                            }
                        } else if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst ||
                                   sortMethod == StreamUP::SettingsManager::SceneSortMethod::OldestFirst) {
                            // Get timestamps
                            qint64 timestamp1 = 0, timestamp2 = 0;
                            if (item1IsFolder) {
                                SceneFolderItem *folder1 = static_cast<SceneFolderItem*>(item1);
                                SceneFolderItem *folder2 = static_cast<SceneFolderItem*>(item2);
                                timestamp1 = folder1->getCreationTimestamp();
                                timestamp2 = folder2->getCreationTimestamp();
                            } else {
                                SceneTreeItem *scene1 = static_cast<SceneTreeItem*>(item1);
                                SceneTreeItem *scene2 = static_cast<SceneTreeItem*>(item2);
                                timestamp1 = scene1->getCreationTimestamp();
                                timestamp2 = scene2->getCreationTimestamp();
                            }

                            if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst) {
                                shouldSwap = (timestamp1 < timestamp2);
                            } else {
                                shouldSwap = (timestamp1 > timestamp2);
                            }
                        }
                    }
                } else {
                    // Mix folders and scenes, sort by selected method
                    if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ ||
                        sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalZA) {
                        int comparison = QString::compare(item1->text(), item2->text(), Qt::CaseInsensitive);
                        if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ) {
                            shouldSwap = (comparison > 0);
                        } else {
                            shouldSwap = (comparison < 0);
                        }
                    } else if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst ||
                               sortMethod == StreamUP::SettingsManager::SceneSortMethod::OldestFirst) {
                        // Get timestamps (works for both folders and scenes)
                        qint64 timestamp1 = 0, timestamp2 = 0;
                        if (item1->type() == SceneFolderItem::UserType + 1) {
                            SceneFolderItem *folder1 = static_cast<SceneFolderItem*>(item1);
                            timestamp1 = folder1->getCreationTimestamp();
                        } else {
                            SceneTreeItem *scene1 = static_cast<SceneTreeItem*>(item1);
                            timestamp1 = scene1->getCreationTimestamp();
                        }

                        if (item2->type() == SceneFolderItem::UserType + 1) {
                            SceneFolderItem *folder2 = static_cast<SceneFolderItem*>(item2);
                            timestamp2 = folder2->getCreationTimestamp();
                        } else {
                            SceneTreeItem *scene2 = static_cast<SceneTreeItem*>(item2);
                            timestamp2 = scene2->getCreationTimestamp();
                        }

                        if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst) {
                            shouldSwap = (timestamp1 < timestamp2);
                        } else {
                            shouldSwap = (timestamp1 > timestamp2);
                        }
                    }
                }

                if (shouldSwap) {
                    // Swap the two rows
                    QList<QStandardItem*> row1 = currentParent->takeRow(i);
                    QList<QStandardItem*> row2 = currentParent->takeRow(i);
                    currentParent->insertRow(i, row2);
                    currentParent->insertRow(i + 1, row1);
                    swapped = true;
                }
            }
        } while (swapped);

        // Recursively sort all folders
        for (int i = 0; i < currentParent->rowCount(); ++i) {
            QStandardItem *item = currentParent->child(i);
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                sortItemsRecursive(item, sortMethod);
            }
        }
    };

    sortItemsRecursive(parent, method);
    m_model->saveSceneTree();
}

void SceneOrganiserDock::updateFromObsScenes()
{
    refreshSceneList();
}

void SceneOrganiserDock::onSceneSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected)

    bool hasSelection = !selected.indexes().isEmpty();
    bool isScene = false;
    bool isFolder = false;
    (void)isFolder; // Suppress unused variable warning
    bool canMoveUp = false;
    bool canMoveDown = false;

    if (hasSelection) {
        QModelIndex index = selected.indexes().first();
        QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        QStandardItem *item = m_model->itemFromIndex(sourceIndex);

        if (item) {
            isScene = (item->type() == SceneTreeItem::UserType + 2);
            isFolder = (item->type() == SceneFolderItem::UserType + 1);

            // Check if item can move up/down
            QStandardItem *parent = item->parent();
            if (!parent) parent = m_model->invisibleRootItem();

            int itemRow = item->row();
            canMoveUp = (itemRow > 0);
            canMoveDown = (itemRow < parent->rowCount() - 1);

            if (isScene) {
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Selection",
                    QString("Scene selected: %1").arg(item->text()).toUtf8().constData());
            }
        }
    }

    // Update toolbar button states (direct button control) - but respect lock state
    if (!m_isLocked) {
        if (m_removeButton) m_removeButton->setEnabled(hasSelection);
        if (m_filtersButton) m_filtersButton->setEnabled(isScene);
        if (m_moveUpButton) m_moveUpButton->setEnabled(hasSelection && canMoveUp);
        if (m_moveDownButton) m_moveDownButton->setEnabled(hasSelection && canMoveDown);
    }
}

void SceneOrganiserDock::updateToolbarState()
{
    if (!m_toolbar) {
        return;
    }

    // Update expand/collapse button state to reflect current folder states
    updateExpandCollapseButtonState();

    // This method can be used for additional toolbar state updates if needed
    // Currently, the selection-based state updates are handled in onSceneSelectionChanged
}

void SceneOrganiserDock::onItemClicked(const QModelIndex &index)
{
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    auto item = m_model->itemFromIndex(sourceIndex);
    if (!item) {
        m_lastClickedIndex = index;
        return;
    }

    // Get current settings for later use
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();

    // With extended selection a ctrl/shift click is part of building a multi-item
    // selection, not a request to go live. Switching on those would take the
    // stream somewhere the user never asked for, so only a click that leaves a
    // single row selected is allowed to change scene.
    if (m_treeView->selectionModel()->selectedRows().count() > 1) {
        m_lastClickedIndex = index;
        return;
    }

    // Check if studio mode is active - it overrides normal click behavior
    if (studioModeFor(m_canvasType) && item->type() == SceneTreeItem::UserType + 2) {
        // Check if preview switching is disabled in studio mode
        if (settings.sceneOrganiserDisablePreviewSwitchingInStudioMode) {
            // Preview switching is disabled in studio mode, do nothing
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "StudioMode",
                QString("Single-click: Preview switching disabled in studio mode").toUtf8().constData());
        } else {
            // Studio mode: single-click always sets preview scene
            obs_source_t *source = Canvas::FindScene(m_canvasType, item->text().toUtf8().constData());
            if (source) {
                obs_frontend_set_current_preview_scene(source);
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "StudioMode",
                    QString("Single-click: Set preview scene to '%1'").arg(item->text()).toUtf8().constData());
                obs_source_release(source);
            }
        }
    } else {
        // Normal mode: use settings to determine behavior
        // Handle single-click scene switching if enabled
        if (settings.sceneOrganiserSwitchMode == StreamUP::SettingsManager::SceneSwitchMode::SingleClick &&
            item->type() == SceneTreeItem::UserType + 2) {
            // Switch to scene on single-click
            obs_source_t *source = Canvas::FindScene(m_canvasType, item->text().toUtf8().constData());
            if (source) {
                Canvas::SetCurrentScene(m_canvasType, source);
                obs_source_release(source);
            }
        }
    }

    // Rename is intentionally NOT triggered by left-click. It was previously
    // started on a "second click on the same item", but that fired accidentally
    // during double-click scene switching. Rename is now only available via F2
    // or the right-click context menu > Rename.
}

void SceneOrganiserDock::onItemDoubleClicked(const QModelIndex &index)
{
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    auto item = m_model->itemFromIndex(sourceIndex);
    if (!item || item->type() != SceneTreeItem::UserType + 2) {
        return;
    }

    // Check if studio mode is active - it overrides normal double-click behavior
    if (studioModeFor(m_canvasType) && item->type() == SceneTreeItem::UserType + 2) {
        // Check if scene switching is disabled in studio mode
        StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
        if (settings.sceneOrganiserDisableTransitionInStudioMode) {
            // Transition is disabled in studio mode, do nothing
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "StudioMode",
                QString("Double-click: Transition disabled in studio mode").toUtf8().constData());
        } else {
            // Studio mode: double-click transitions preview to program (goes live)
            obs_source_t *source = Canvas::FindScene(m_canvasType, item->text().toUtf8().constData());
            if (source) {
                // First set as preview, then trigger transition
                obs_frontend_set_current_preview_scene(source);
                obs_frontend_preview_program_trigger_transition();
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "StudioMode",
                    QString("Double-click: Transitioned scene '%1' to program").arg(item->text()).toUtf8().constData());
                obs_source_release(source);
            }
        }
    } else {
        // Normal mode: use settings to determine behavior
        StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();

        // Only switch to scene on double-click if double-click mode is enabled
        if (settings.sceneOrganiserSwitchMode == StreamUP::SettingsManager::SceneSwitchMode::DoubleClick) {
            // Switch to scene on double-click
            obs_source_t *source = Canvas::FindScene(m_canvasType, item->text().toUtf8().constData());
            if (source) {
                Canvas::SetCurrentScene(m_canvasType, source);
                obs_source_release(source);
            }
        }
    }
}

void SceneOrganiserDock::onCustomContextMenuRequested(const QPoint &pos)
{
    QModelIndex index = m_treeView->indexAt(pos);

    if (index.isValid()) {
        QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        auto item = m_model->itemFromIndex(sourceIndex);
        if (item) {
            if (item->type() == SceneFolderItem::UserType + 1) {
                showFolderContextMenu(m_treeView->mapToGlobal(pos), index);
            } else if (item->type() == SceneTreeItem::UserType + 2) {
                showSceneContextMenu(m_treeView->mapToGlobal(pos), index);
            }
        }
    } else {
        showBackgroundContextMenu(m_treeView->mapToGlobal(pos));
    }
}

void SceneOrganiserDock::showFolderContextMenu(const QPoint &pos, const QModelIndex &index)
{
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    m_currentContextItem = m_model->itemFromIndex(sourceIndex);
    refreshColorMenuState();
    refreshIconMenuState();
    m_folderContextMenu->exec(pos);
}

void SceneOrganiserDock::showSceneContextMenu(const QPoint &pos, const QModelIndex &index)
{
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    m_currentContextItem = m_model->itemFromIndex(sourceIndex);

    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) {
        return;
    }

    // Update dynamic menu states
    QString sceneName = m_currentContextItem->text();
    obs_source_t *source = Canvas::FindScene(m_canvasType, sceneName.toUtf8().constData());

    refreshIconMenuState();
    populateAddToTabMenu();

    // The favourite action is a toggle, so it has to say which way it will go
    // for THIS scene.
    if (m_favouriteToggleAction) {
        m_favouriteToggleAction->setText(isFavourite(sceneName)
            ? obs_module_text("SceneOrganiser.Action.RemoveFavourite")
            : obs_module_text("SceneOrganiser.Action.AddFavourite"));
    }

    // Populate projector menu with current monitors
    populateProjectorMenu();

    // Transition override follows whichever scene was right clicked
    populateTransitionOverrideMenu(source);

    // Linked scenes follows the same rule
    populateLinkedScenesMenu(source);

    // Enable/disable "Paste Filters" based on whether we have copied filters
    QList<QAction*> actions = m_sceneContextMenu->actions();
    for (QAction *action : actions) {
        QString actionText = action->text();
        if (actionText.contains("Paste") && actionText.contains("Filter")) {
            action->setEnabled(m_copyFiltersSource && !obs_weak_source_expired(m_copyFiltersSource));
        }
        else if (actionText.contains("Copy") && actionText.contains("Filter")) {
            // Enable "Copy Filters" only if the scene has filters
            action->setEnabled(source && obs_source_filter_count(source) > 0);
        }
    }

    // Update "Show in Multiview" checkable state
    if (source) {
        obs_data_t *privateSettings = obs_source_get_private_settings(source);
        obs_data_set_default_bool(privateSettings, "show_in_multiview", true);
        bool showInMultiview = obs_data_get_bool(privateSettings, "show_in_multiview");

        for (QAction *action : actions) {
            if (action->text().contains("Multiview")) {
                action->setCheckable(true);
                action->setChecked(showInMultiview);
                break;
            }
        }

        obs_data_release(privateSettings);
        obs_source_release(source);
    }

    // Update hide/show scene action visibility based on current scene state
    bool sceneIsHidden = m_hiddenScenes.contains(sceneName);
    if (m_hideSceneAction) m_hideSceneAction->setVisible(!sceneIsHidden);
    if (m_showSceneAction) m_showSceneAction->setVisible(sceneIsHidden);

    // Only enable hide/show actions when unlocked
    if (m_hideSceneAction) m_hideSceneAction->setEnabled(!m_isLocked);
    if (m_showSceneAction) m_showSceneAction->setEnabled(!m_isLocked);

    m_sceneContextMenu->exec(pos);
}

void SceneOrganiserDock::showBackgroundContextMenu(const QPoint &pos)
{
    m_backgroundContextMenu->exec(pos);
}

void SceneOrganiserDock::onAddFolderClicked()
{
    // On a flat tab the add button fills the list instead of the tree.
    if (m_currentKind != QuickTabKind::Scenes) {
        showAddToTabMenu();
        return;
    }


    const QString layoutBefore = captureLayout();

    QPointer<SceneOrganiserDock> self(this);
    su::prompt(this,
        QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.AddFolder.Title")),
        QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.AddFolder.Text")),
        QString(),
        [self, layoutBefore](const QString &folderName) {
            if (!self) return;
            if (folderName.isEmpty()) return;
            auto folderItem = self->m_model->createFolderItem(folderName);
            self->m_model->invisibleRootItem()->appendRow(folderItem);
            self->m_treeView->expand(folderItem->index());
            self->applySortingIfEnabled();
            self->m_saveTimer->start();
            // Pushed here, not after su::prompt returns: the dialog is modeless,
            // so the folder does not exist yet at that point.
            self->pushLayoutUndo(QString::fromUtf8(obs_module_text("SceneOrganiser.Undo.AddFolder")), layoutBefore);
        });
}

void SceneOrganiserDock::onCreateSceneClicked()
{

    QPointer<SceneOrganiserDock> self(this);
    su::prompt(this,
        QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.CreateScene.Title")),
        QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.CreateScene.Text")),
        QString(),
        [self](const QString &sceneName) {
            if (!self) return;
            if (sceneName.isEmpty()) return;

            // Create a new scene in OBS
            obs_scene_t *scene = Canvas::CreateScene(self->GetCanvasType(), sceneName.toUtf8().constData());
            if (scene) {
                obs_source_t *scene_source = obs_scene_get_source(scene);

                // The scene will automatically appear in our tree view due to the OBS event system
                // We don't need to manually add it here

                StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
                if (settings.sceneOrganiserSwitchToNewScene) {
                    // In studio mode, only ever set the preview — never push the new scene to program
                    if (studioModeFor(self->GetCanvasType())) {
                        obs_frontend_set_current_preview_scene(scene_source);
                    } else {
                        Canvas::SetCurrentScene(self->GetCanvasType(), scene_source);
                    }
                }

                obs_scene_release(scene);

                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Scene Creation",
                    QString("Created new scene: %1").arg(sceneName).toUtf8().constData());
            }
        });
}

void SceneOrganiserDock::onRemoveClicked()
{
    // On a tab, remove means 'take out of this tab', never 'delete the scene
    // from OBS' - the scene itself is not this tab's to destroy. A folder goes
    // with its contents, which likewise only leave the tab.
    if (m_currentKind != QuickTabKind::Scenes) {
        if (m_quickTree && m_quickTree->selectionModel() && editableNodesForCurrentTab()) {
            const QModelIndexList selected = m_quickTree->selectionModel()->selectedRows();
            if (!selected.isEmpty()) {
                onRemoveFromTabClicked(m_quickModel->itemFromIndex(m_quickProxy->mapToSource(selected.first())));
            }
        }
        return;
    }


    // Extended selection means this can be a batch. Everything is resolved by
    // NAME up front and re-resolved at accept-time, because the confirm dialog
    // is modeless and the tree can be rebuilt by an OBS event while it is open.
    QModelIndexList selected = m_treeView->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;

    QStringList sceneNames;
    QStringList folderNames;
    for (const QModelIndex &proxyIndex : selected) {
        QStandardItem *item = m_model->itemFromIndex(m_proxyModel->mapToSource(proxyIndex));
        if (!item) continue;
        if (item->type() == SceneTreeItem::UserType + 2) {
            sceneNames.append(item->text());
        } else if (item->type() == SceneFolderItem::UserType + 1) {
            folderNames.append(item->text());
        }
    }

    const int total = sceneNames.size() + folderNames.size();
    if (total == 0) return;

    // One item keeps the original wording ("scene 'Intro'"); a batch names the
    // count instead, since listing forty scenes in a dialog helps nobody.
    QString itemType;
    QString itemName;
    if (total == 1) {
        itemType = folderNames.isEmpty() ? QString("scene") : QString("folder");
        itemName = folderNames.isEmpty() ? sceneNames.first() : folderNames.first();
    } else {
        itemType = QString("items");
        itemName = QString::number(total);
    }

    QPointer<SceneOrganiserDock> self(this);
    su::confirm(this,
        QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.Remove.Title")),
        QString(obs_module_text("SceneOrganiser.Dialog.Remove.Text")).arg(itemType, itemName),
        QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.Remove.Title")),
        "danger",
        [self, sceneNames, folderNames]() {
            if (!self) return;

            self->m_treeView->selectionModel()->clearSelection();

            for (const QString &sceneName : sceneNames) {
                obs_source_t *source = Canvas::FindScene(self->GetCanvasType(), sceneName.toUtf8().constData());
                if (!source) {
                    continue;
                }

                // Drop our own tracking and row before OBS removes the source,
                // so the tree never holds a row for a scene that is gone.
                if (QStandardItem *item = self->m_model->findSceneItemByName(sceneName)) {
                    if (item->type() == SceneTreeItem::UserType + 2) {
                        SceneTreeItem *sceneItem = static_cast<SceneTreeItem*>(item);
                        self->m_model->removeSceneFromTracking(sceneItem->getWeakSource());
                    }
                    QStandardItem *parent = item->parent();
                    if (!parent) parent = self->m_model->invisibleRootItem();
                    parent->removeRow(item->row());
                }

                obs_source_remove(source);
                obs_source_release(source);

                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Scene Removal",
                    QString("Removed scene '%1'").arg(sceneName).toUtf8().constData());
            }

            if (!sceneNames.isEmpty()) {
                self->m_model->cleanupEmptyItems();
            }

            for (const QString &folderName : folderNames) {
                if (QStandardItem *item = self->m_model->findFolderItemByName(folderName)) {
                    QStandardItem *parent = item->parent();
                    if (!parent) parent = self->m_model->invisibleRootItem();
                    parent->removeRow(item->row());
                }
            }

            self->SaveConfiguration(); // Immediate save on deletion
        });
}

void SceneOrganiserDock::onFiltersClicked()
{
    // Filters work off whichever scene is selected, on any tab.
    if (m_currentKind != QuickTabKind::Scenes) {
        const QString sceneName = selectedSceneOnCurrentTab();
        if (!sceneName.isEmpty()) {
            if (obs_source_t *source = Canvas::FindScene(m_canvasType, sceneName.toUtf8().constData())) {
                obs_frontend_open_source_filters(source);
                obs_source_release(source);
            }
        }
        return;
    }


    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;

    QModelIndex sourceIndex = m_proxyModel->mapToSource(selected.first());
    QStandardItem *item = m_model->itemFromIndex(sourceIndex);
    if (!item || item->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = item->text();
    obs_source_t *source = Canvas::FindScene(m_canvasType, sceneName.toUtf8().constData());
    if (source) {
        // Open scene filters using OBS frontend API
        obs_frontend_open_source_filters(source);
        obs_source_release(source);
    }
}

void SceneOrganiserDock::onMoveUpClicked()
{
    if (m_currentKind != QuickTabKind::Scenes) {
        moveWithinCurrentTab(-1);
        return;
    }


    const QString layoutBefore = captureLayout();

    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;

    QModelIndex sourceIndex = m_proxyModel->mapToSource(selected.first());
    QStandardItem *item = m_model->itemFromIndex(sourceIndex);
    if (!item) return;

    QStandardItem *parent = item->parent();
    if (!parent) parent = m_model->invisibleRootItem();

    int currentRow = item->row();
    if (currentRow > 0) {
        QList<QStandardItem*> items = parent->takeRow(currentRow);
        parent->insertRow(currentRow - 1, items);

        // Restore selection
        QModelIndex newIndex = m_model->indexFromItem(items.first());
        QModelIndex proxyIndex = m_proxyModel->mapFromSource(newIndex);
        m_treeView->selectionModel()->select(proxyIndex, QItemSelectionModel::ClearAndSelect);

        m_saveTimer->start();
    }

    pushLayoutUndo(QString::fromUtf8(obs_module_text("SceneOrganiser.Undo.Move")), layoutBefore);
}

void SceneOrganiserDock::onMoveDownClicked()
{
    if (m_currentKind != QuickTabKind::Scenes) {
        moveWithinCurrentTab(1);
        return;
    }


    const QString layoutBefore = captureLayout();

    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;

    QModelIndex sourceIndex = m_proxyModel->mapToSource(selected.first());
    QStandardItem *item = m_model->itemFromIndex(sourceIndex);
    if (!item) return;

    QStandardItem *parent = item->parent();
    if (!parent) parent = m_model->invisibleRootItem();

    int currentRow = item->row();
    if (currentRow < parent->rowCount() - 1) {
        QList<QStandardItem*> items = parent->takeRow(currentRow);
        parent->insertRow(currentRow + 1, items);

        // Restore selection
        QModelIndex newIndex = m_model->indexFromItem(items.first());
        QModelIndex proxyIndex = m_proxyModel->mapFromSource(newIndex);
        m_treeView->selectionModel()->select(proxyIndex, QItemSelectionModel::ClearAndSelect);

        m_saveTimer->start();
    }

    pushLayoutUndo(QString::fromUtf8(obs_module_text("SceneOrganiser.Undo.Move")), layoutBefore);
}

void SceneOrganiserDock::onExpandCollapseAllClicked()
{
    if (!m_expandCollapseButton) return;

    // On a tab this is the tab's own tree. Its collapsed state is remembered,
    // because the tab tree is rebuilt on every refresh and would otherwise
    // spring back open the moment anything changed.
    if (m_currentKind != QuickTabKind::Scenes) {
        if (!m_quickTree) return;

        m_quickTreeCollapsed = m_expandCollapseButton->isChecked();
        if (m_quickTreeCollapsed) {
            m_quickTree->collapseAll();
            m_expandCollapseButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.ExpandAll"));
        } else {
            m_quickTree->expandAll();
            m_expandCollapseButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.CollapseAll"));
        }
        return;
    }

    if (!m_treeView) return;

    // Note: In OBS convention, checked = collapsed, unchecked = expanded
    // The checkbox state has already been toggled by Qt, so we read the current state
    bool isCollapsed = m_expandCollapseButton->isChecked();

    if (isCollapsed) {
        // Collapse all folders
        m_treeView->collapseAll();
        syncFolderIcons();
        m_allExpanded = false;
        m_expandCollapseButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.ExpandAll"));
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "ExpandCollapse", "Collapsed all folders");
    } else {
        // Expand all folders
        m_treeView->expandAll();
        syncFolderIcons();
        m_allExpanded = true;
        m_expandCollapseButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.CollapseAll"));
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "ExpandCollapse", "Expanded all folders");
    }
}

void SceneOrganiserDock::updateExpandCollapseButtonState()
{
    if (!m_treeView || !m_expandCollapseButton || !m_proxyModel) return;

    // Count expanded and collapsed folders
    int totalFolders = 0;
    int expandedFolders = 0;

    std::function<void(const QModelIndex&)> countRecursive = [&](const QModelIndex& proxyIndex) {
        if (!proxyIndex.isValid()) return;

        // Convert to source index to check if it's a folder
        QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        QStandardItem *item = m_model->itemFromIndex(sourceIndex);

        if (item && item->type() == QStandardItem::UserType + 1) { // Folder item
            totalFolders++;
            if (m_treeView->isExpanded(proxyIndex)) {
                expandedFolders++;
            }
        }

        // Check children
        int rowCount = m_proxyModel->rowCount(proxyIndex);
        for (int i = 0; i < rowCount; ++i) {
            QModelIndex childIndex = m_proxyModel->index(i, 0, proxyIndex);
            countRecursive(childIndex);
        }
    };

    // Start from root items
    int rootRowCount = m_proxyModel->rowCount();
    for (int i = 0; i < rootRowCount; ++i) {
        QModelIndex rootIndex = m_proxyModel->index(i, 0);
        countRecursive(rootIndex);
    }

    // Block signals to prevent triggering the clicked handler
    m_expandCollapseButton->blockSignals(true);

    if (totalFolders == 0 || expandedFolders == 0) {
        // No folders or all folders collapsed - show collapsed state (expand icon)
        m_expandCollapseButton->setChecked(true);
        m_expandCollapseButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.ExpandAll"));
        m_allExpanded = false;
    } else {
        // At least one folder is expanded - show expanded state (collapse icon)
        m_expandCollapseButton->setChecked(false);
        m_expandCollapseButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.CollapseAll"));
        m_allExpanded = true;
    }

    m_expandCollapseButton->blockSignals(false);
}

void SceneOrganiserDock::onToggleIconsClicked()
{
    // Toggle the icons setting
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    settings.sceneOrganiserShowIcons = !settings.sceneOrganiserShowIcons;
    StreamUP::SettingsManager::UpdateSettings(settings);

    // Update all docks' icons immediately
    NotifySceneOrganiserIconsChanged();
}

// Builds the "Set Colour" submenu in the shape of OBS' native Sources menu:
// a checkable Clear entry, a checkable Custom Colour entry, then a grid of the
// eight preset swatches. Check state is refreshed on every show from the
// context item's stored colour, so no extra preset index has to be persisted.
QMenu *SceneOrganiserDock::createIconSubmenu()
{
    QMenu *menu = new QMenu(obs_module_text("SceneOrganiser.Action.SetIcon"), this);

    m_iconDefaultAction = menu->addAction(obs_module_text("SceneOrganiser.Icon.Default"), this, [this]() {
        applyIconSpec(QString());
    });
    m_iconDefaultAction->setCheckable(true);

    m_iconCustomAction = menu->addAction(obs_module_text("SceneOrganiser.Icon.CustomImage"), this,
                                         &SceneOrganiserDock::onSetCustomIconImageClicked);
    m_iconCustomAction->setCheckable(true);

    // Colour applies to whichever icon is in use, the default included, so it is
    // offered here rather than only alongside a custom icon.
    m_iconColorMenu = menu->addMenu(obs_module_text("SceneOrganiser.Menu.IconColour"));

    m_iconColorClearAction = m_iconColorMenu->addAction(
        QString::fromUtf8(obs_frontend_get_locale_string("Clear"), -1), this, [this]() { applyIconColor(QColor()); });
    m_iconColorClearAction->setCheckable(true);

    m_iconColorCustomAction = m_iconColorMenu->addAction(
        QString::fromUtf8(obs_frontend_get_locale_string("CustomColor"), -1), this,
        &SceneOrganiserDock::onSetCustomIconColorClicked);
    m_iconColorCustomAction->setCheckable(true);

    m_iconColorMenu->addSeparator();

    // The same 4x2 swatch grid as Set Colour, so the two menus read as one
    // idea rather than as two different ways of picking a colour. The presets
    // are the same eight, taken at full opacity: an icon is a small shape and
    // the translucent versions read as grey at that size.
    QWidget *iconSwatchWidget = new QWidget(m_iconColorMenu);
    QGridLayout *iconGrid = new QGridLayout(iconSwatchWidget);
    iconGrid->setContentsMargins(su::S(8), su::S(4), su::S(8), su::S(8));
    iconGrid->setSpacing(su::S(4));

    const QList<QColor> &iconPresets = PresetColors();
    m_iconColorSwatchButtons.clear();
    for (int i = 0; i < iconPresets.size(); ++i) {
        const QColor solid(iconPresets[i].red(), iconPresets[i].green(), iconPresets[i].blue());

        QPushButton *swatch = new QPushButton(iconSwatchWidget);
        swatch->setFlat(true);
        swatch->setFixedSize(su::S(26), su::S(22));
        swatch->setCursor(Qt::PointingHandCursor);
        connect(swatch, &QPushButton::clicked, this, [this, solid]() { applyIconColor(solid); });
        iconGrid->addWidget(swatch, i / 4, i % 4);
        m_iconColorSwatchButtons.append(swatch);
    }

    QWidgetAction *iconSwatchAction = new QWidgetAction(m_iconColorMenu);
    iconSwatchAction->setDefaultWidget(iconSwatchWidget);
    m_iconColorMenu->addAction(iconSwatchAction);

    connect(m_iconColorMenu, &QMenu::aboutToShow, this, &SceneOrganiserDock::refreshIconColorMenuState);

    menu->addSeparator();

    // Every icon the OBS theme provides, each shown with the icon itself so the
    // menu reads as a picker rather than a list of names.
    m_iconThemeActions.clear();
    for (const ObsThemeIcon &themeIcon : kObsThemeIcons) {
        const QString spec = QString::fromLatin1("obs:") + QString::fromLatin1(themeIcon.property);
        QAction *action = menu->addAction(QString::fromUtf8(themeIcon.label), this, [this, spec]() {
            applyIconSpec(spec);
        });
        action->setCheckable(true);
        // Preview icons are filled in by refreshIconMenuState() when the menu
        // opens, not here: this runs while the dock is being built, long before
        // the OBS theme is available to ask.
        m_iconThemeActions.insert(spec, action);
    }

    return menu;
}

// Ticks whichever entry matches the item the menu was opened on, so the current
// icon is visible without having to remember what was picked.
void SceneOrganiserDock::refreshIconMenuState()
{
    const QString spec = m_currentContextItem ? m_currentContextItem->data(CustomIconRole).toString() : QString();

    if (m_iconDefaultAction) m_iconDefaultAction->setChecked(spec.isEmpty());
    if (m_iconCustomAction) m_iconCustomAction->setChecked(spec.startsWith(QLatin1String("file:")));

    for (auto it = m_iconThemeActions.begin(); it != m_iconThemeActions.end(); ++it) {
        it.value()->setChecked(it.key() == spec);
        // Re-resolved on every open: the menu is built once, but the icons it
        // previews are themed and the theme can change under it.
        it.value()->setIcon(ResolveIconSpec(it.key()));
    }
}

void SceneOrganiserDock::applyIconSpec(const QString &spec)
{
    if (!m_currentContextItem) {
        return;
    }

    const QString layoutBefore = captureLayout();

    if (spec.isEmpty()) {
        m_currentContextItem->setData(QVariant(), CustomIconRole);
    } else {
        m_currentContextItem->setData(spec, CustomIconRole);
    }

    if (m_currentContextItem->type() == SceneFolderItem::UserType + 1) {
        static_cast<SceneFolderItem*>(m_currentContextItem)->updateIcon();
    } else if (m_currentContextItem->type() == SceneTreeItem::UserType + 2) {
        static_cast<SceneTreeItem*>(m_currentContextItem)->updateIcon();
    }

    forceTreeViewRepaint();
    pushLayoutUndo(QString::fromUtf8(obs_module_text("SceneOrganiser.Undo.Icon")), layoutBefore);
    SaveConfiguration();

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Icon",
        QString("Set icon '%1' on '%2'").arg(spec.isEmpty() ? QString("default") : spec,
                                             m_currentContextItem->text()).toUtf8().constData());
}

// Sets (or clears, with an invalid colour) the tint on the item the menu was
// opened on. Kept separate from applyIconSpec so a colour can be changed without
// re-picking the icon, and a colour survives changing the icon.
// Reflects the context item's current icon tint, exactly as the Set Colour menu
// reflects its row colour: Clear ticked when there is none, the matching swatch
// outlined when it is one of the presets, otherwise Custom Colour ticked.
void SceneOrganiserDock::refreshIconColorMenuState()
{
    QColor current;
    if (m_currentContextItem) {
        current = m_currentContextItem->data(CustomIconColorRole).value<QColor>();
    }

    const QList<QColor> &presets = PresetColors();
    int matchedPreset = -1;
    if (current.isValid()) {
        for (int i = 0; i < presets.size(); ++i) {
            if (QColor(presets[i].red(), presets[i].green(), presets[i].blue()) == current) {
                matchedPreset = i;
                break;
            }
        }
    }

    if (m_iconColorClearAction) {
        m_iconColorClearAction->setChecked(!current.isValid());
    }
    if (m_iconColorCustomAction) {
        m_iconColorCustomAction->setChecked(current.isValid() && matchedPreset < 0);
    }

    for (int i = 0; i < m_iconColorSwatchButtons.size() && i < presets.size(); ++i) {
        const QColor c(presets[i].red(), presets[i].green(), presets[i].blue());
        const QString border = (i == matchedPreset) ? QStringLiteral("2px solid black")
                                                    : QStringLiteral("1px solid rgba(0,0,0,60)");
        m_iconColorSwatchButtons[i]->setStyleSheet(
            QString("QPushButton{background-color:rgb(%1,%2,%3);border:%4;border-radius:%5px;}")
                .arg(c.red())
                .arg(c.green())
                .arg(c.blue())
                .arg(border)
                .arg(su::S(3)));
    }
}

void SceneOrganiserDock::applyIconColor(const QColor &color)
{
    if (!m_currentContextItem) {
        return;
    }

    const QString layoutBefore = captureLayout();

    if (color.isValid()) {
        m_currentContextItem->setData(color, CustomIconColorRole);
    } else {
        m_currentContextItem->setData(QVariant(), CustomIconColorRole);
    }

    if (m_currentContextItem->type() == SceneFolderItem::UserType + 1) {
        static_cast<SceneFolderItem *>(m_currentContextItem)->updateIcon();
    } else if (m_currentContextItem->type() == SceneTreeItem::UserType + 2) {
        static_cast<SceneTreeItem *>(m_currentContextItem)->updateIcon();
    }

    forceTreeViewRepaint();
    // The tabs copy their icons from the tree's items, so they need rebuilding.
    refreshQuickList();
    pushLayoutUndo(QString::fromUtf8(obs_module_text("SceneOrganiser.Undo.IconColour")), layoutBefore);
    SaveConfiguration();

    // A swatch is a click on a widget inside the menu, which does not dismiss it
    // the way choosing an action would.
    if (m_iconColorMenu) {
        m_iconColorMenu->close();
    }
    if (m_iconMenu) {
        m_iconMenu->close();
    }
}

void SceneOrganiserDock::onSetCustomIconColorClicked()
{
    if (!m_currentContextItem) {
        return;
    }

    const QColor current = m_currentContextItem->data(CustomIconColorRole).value<QColor>();

    auto sh = su::makeWindow(QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.IconColour.Title")),
                             "v" PROJECT_VERSION, this, /*brandFooter=*/false, "StreamUP");
    auto *cp = new su::ColorPicker();
    if (current.isValid()) {
        cp->setColor(current);
    }
    sh.content->setContentsMargins(su::S(16), su::S(16), su::S(16), su::S(8));
    sh.content->addWidget(cp);

    auto *cancel = new su::PillButton("Cancel", "outline");
    auto *ok = new su::PillButton("Select", "primary");
    sh.footerButtons->addWidget(cancel);
    sh.footerButtons->addWidget(ok);

    QObject::connect(cancel, &QPushButton::clicked, sh.dialog, &QDialog::close);

    QPointer<SceneOrganiserDock> self(this);
    QObject::connect(ok, &QPushButton::clicked, sh.dialog, [self, cp, dlg = sh.dialog]() {
        if (self) {
            const QColor chosen = cp->color();
            if (chosen.isValid()) {
                // Opaque: an icon is a small shape and a translucent tint just
                // reads as grey at that size.
                self->applyIconColor(QColor(chosen.red(), chosen.green(), chosen.blue()));
            }
        }
        dlg->close();
    });

    sh.dialog->resize(su::S(360), su::S(420));
    sh.dialog->show();
}

void SceneOrganiserDock::onSetCustomIconImageClicked()
{
    if (!m_currentContextItem) {
        return;
    }

    // Modal on purpose: it is the platform file dialog, and the item the menu
    // was opened on has to still be the item when the path comes back.
    const QString path = QFileDialog::getOpenFileName(this,
        QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.ChooseIcon")),
        QString(),
        QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.ChooseIcon.Filter")));

    if (path.isEmpty()) {
        return;
    }

    // The path is stored, not the pixels: an icon that is edited on disk then
    // follows, and the tree JSON stays small.
    applyIconSpec(QString::fromLatin1("file:") + path);
}

QMenu *SceneOrganiserDock::createColorSubmenu()
{
    QMenu *menu = new QMenu(obs_module_text("SceneOrganiser.Action.SetColor"), this);

    m_colorClearAction = menu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Clear"), -1),
                                        this, &SceneOrganiserDock::onClearCustomColorClicked);
    m_colorClearAction->setCheckable(true);

    m_colorCustomAction = menu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("CustomColor"), -1),
                                          this, &SceneOrganiserDock::onSetCustomColorClicked);
    m_colorCustomAction->setCheckable(true);

    menu->addSeparator();

    // Swatch grid (4x2, matching the native menu's layout).
    QWidget *swatchWidget = new QWidget(menu);
    QGridLayout *grid = new QGridLayout(swatchWidget);
    grid->setContentsMargins(su::S(8), su::S(4), su::S(8), su::S(8));
    grid->setSpacing(su::S(4));

    const QList<QColor> &presets = PresetColors();
    m_colorSwatchButtons.clear();
    for (int i = 0; i < presets.size(); ++i) {
        QPushButton *swatch = new QPushButton(swatchWidget);
        swatch->setFlat(true);
        swatch->setFixedSize(su::S(26), su::S(22));
        swatch->setCursor(Qt::PointingHandCursor);
        // Index is 0-based here; applyPresetColor takes the same 0-based index.
        connect(swatch, &QPushButton::clicked, this, [this, i]() { applyPresetColor(i); });
        grid->addWidget(swatch, i / 4, i % 4);
        m_colorSwatchButtons.append(swatch);
    }

    QWidgetAction *swatchAction = new QWidgetAction(menu);
    swatchAction->setDefaultWidget(swatchWidget);
    menu->addAction(swatchAction);

    connect(menu, &QMenu::aboutToShow, this, &SceneOrganiserDock::refreshColorMenuState);

    return menu;
}

// Reflects the context item's current colour: Clear ticked when there is none,
// the matching swatch outlined when it is one of the presets, otherwise Custom
// Colour ticked.
void SceneOrganiserDock::refreshColorMenuState()
{
    QColor current;
    if (m_currentContextItem) {
        QVariant colorData = m_currentContextItem->data(Qt::UserRole + 1);
        if (colorData.isValid()) {
            current = colorData.value<QColor>();
        }
    }

    const QList<QColor> &presets = PresetColors();
    int matchedPreset = -1;
    if (current.isValid()) {
        for (int i = 0; i < presets.size(); ++i) {
            if (presets[i] == current) {
                matchedPreset = i;
                break;
            }
        }
    }

    if (m_colorClearAction) {
        m_colorClearAction->setChecked(!current.isValid());
    }
    if (m_colorCustomAction) {
        m_colorCustomAction->setChecked(current.isValid() && matchedPreset < 0);
    }

    for (int i = 0; i < m_colorSwatchButtons.size() && i < presets.size(); ++i) {
        const QColor &c = presets[i];
        // The swatch is drawn at the preset's own alpha so it previews exactly
        // how the row will look; the selected one gets the native black outline.
        const QString border = (i == matchedPreset) ? QStringLiteral("2px solid black")
                                                    : QStringLiteral("1px solid rgba(0,0,0,60)");
        m_colorSwatchButtons[i]->setStyleSheet(
            QString("QPushButton{background-color:rgba(%1,%2,%3,%4);border:%5;border-radius:%6px;}")
                .arg(c.red())
                .arg(c.green())
                .arg(c.blue())
                .arg(c.alpha())
                .arg(border)
                .arg(su::S(3)));
    }
}

// Applies one of the eight preset colours to the context item and closes the
// menus, mirroring the native behaviour of clicking a swatch.
void SceneOrganiserDock::applyPresetColor(int presetIndex)
{
    const QString layoutBefore = captureLayout();

    const QList<QColor> &presets = PresetColors();
    if (!m_currentContextItem || presetIndex < 0 || presetIndex >= presets.size()) {
        return;
    }

    const QColor color = presets[presetIndex];
    m_currentContextItem->setData(color, Qt::UserRole + 1);
    applyCustomColorToItem(m_currentContextItem, color);
    m_saveTimer->start();
    forceTreeViewRepaint();

    if (m_colorMenu) {
        m_colorMenu->hide();
    }
    if (m_folderContextMenu) {
        m_folderContextMenu->hide();
    }
    if (m_sceneContextMenu) {
        m_sceneContextMenu->hide();
    }

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "CustomColor",
        QString("Set preset colour %1 for item '%2': %3")
        .arg(presetIndex + 1).arg(m_currentContextItem->text(), color.name(QColor::HexArgb)).toUtf8().constData());

    pushLayoutUndo(QString::fromUtf8(obs_module_text("SceneOrganiser.Undo.Colour")), layoutBefore);
}

void SceneOrganiserDock::onSetCustomColorClicked()
{
    const QString layoutBefore = captureLayout();

    if (!m_currentContextItem) {
        return;
    }

    // Get current color if any
    QVariant colorData = m_currentContextItem->data(Qt::UserRole + 1);
    QColor currentColor = colorData.isValid() ? colorData.value<QColor>() : QColor();

    // Open branded color picker dialog (replaces native QColorDialog; no alpha
    // was ever used here, so the SoT ColorPicker is a faithful replacement).
    auto sh = su::makeWindow(QString::fromUtf8("Choose Color"), "v" PROJECT_VERSION, this,
                             /*brandFooter=*/false, "StreamUP");
    auto *cp = new su::ColorPicker();
    if (currentColor.isValid()) {
        cp->setColor(currentColor);
    }
    sh.content->setContentsMargins(su::S(16), su::S(16), su::S(16), su::S(8));
    sh.content->addWidget(cp);

    auto *cancel = new su::PillButton("Cancel", "outline");
    auto *ok = new su::PillButton("Select", "primary");
    sh.footerButtons->addWidget(cancel);
    sh.footerButtons->addWidget(ok);

    QObject::connect(cancel, &QPushButton::clicked, sh.dialog, &QDialog::close);

    QPointer<SceneOrganiserDock> self(this);
    QObject::connect(ok, &QPushButton::clicked, sh.dialog, [self, cp, dlg = sh.dialog, layoutBefore]() {
        if (self) {
            // Re-read the context item the menu was opened on (same member the
            // synchronous code used). Guard against it having been cleared.
            QStandardItem *item = self->m_currentContextItem;
            if (item) {
                QColor selectedColor = cp->color();
                if (selectedColor.isValid()) {
                    // Store the color in the item
                    item->setData(selectedColor, Qt::UserRole + 1);

                    // Apply the color immediately
                    self->applyCustomColorToItem(item, selectedColor);
                    self->forceTreeViewRepaint();

                    // Save configuration
                    self->m_saveTimer->start();

                    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "CustomColor",
                        QString("Set custom color for item '%1': %2")
                        .arg(item->text(), selectedColor.name()).toUtf8().constData());

                    // Modeless dialog: the colour is only chosen here.
                    self->pushLayoutUndo(QString::fromUtf8(obs_module_text("SceneOrganiser.Undo.Colour")), layoutBefore);
                }
            }
        }
        dlg->close();
    });

    // makeWindow() only BUILDS the shell - it does not show it. Without this the
    // "Set Colour" action did nothing at all (the dialog is WA_DeleteOnClose and
    // unreferenced, so it was created and immediately discarded).
    sh.dialog->resize(su::S(360), su::S(420));
    sh.dialog->show();
}

void SceneOrganiserDock::onClearCustomColorClicked()
{
    const QString layoutBefore = captureLayout();

    if (!m_currentContextItem) {
        return;
    }

    // Remove the custom color
    m_currentContextItem->setData(QVariant(), Qt::UserRole + 1);

    // Clear the color styling
    clearCustomColorFromItem(m_currentContextItem);
    forceTreeViewRepaint();

    // Save configuration
    m_saveTimer->start();

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "CustomColor",
        QString("Cleared custom color for item '%1'").arg(m_currentContextItem->text()).toUtf8().constData());

    pushLayoutUndo(QString::fromUtf8(obs_module_text("SceneOrganiser.Undo.Colour")), layoutBefore);
}

void SceneOrganiserDock::applyCustomColorToItem(QStandardItem *item, const QColor &color)
{
    if (!item || !color.isValid()) {
        return;
    }

    // The colour itself lives on UserRole + 1; CustomColorDelegate reads it there
    // and paints the row. Nothing is set on Qt::BackgroundRole on purpose.
    //
    // Setting it as well would paint the colour twice: the style fills the whole
    // row square with the role brush, then the delegate draws its inset rounded
    // pill on top, leaving a darker border of the same colour around every
    // coloured row. Qt::ForegroundRole is the same story for the text — it is
    // reapplied inside QStyledItemDelegate::initStyleOption, which runs after the
    // delegate has set its own palette and so silently wins.
    //
    // These clears matter for items coloured by an earlier version of the plugin.
    item->setData(QVariant(), Qt::BackgroundRole);
    item->setData(QVariant(), Qt::ForegroundRole);
}

void SceneOrganiserDock::clearCustomColorFromItem(QStandardItem *item)
{
    if (!item) {
        return;
    }

    // Clear background and foreground colors to use default theme colors
    item->setData(QVariant(), Qt::BackgroundRole);
    item->setData(QVariant(), Qt::ForegroundRole);
}

void SceneOrganiserDock::applyAllCustomColors(QStandardItem *parent)
{
    // If no parent provided, start from root
    if (!parent) {
        parent = m_model->invisibleRootItem();
    }

    if (!parent) {
        return;
    }

    // Apply colors recursively to all items in the tree
    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *item = parent->child(i);
        if (!item) continue;

        // Check if this item has a custom color stored
        QVariant colorData = item->data(Qt::UserRole + 1);
        if (colorData.isValid()) {
            QColor color = colorData.value<QColor>();
            if (color.isValid()) {
                applyCustomColorToItem(item, color);
            }
        }

        // Recursively apply to children
        if (item->hasChildren()) {
            applyAllCustomColors(item);
        }
    }
}

QColor SceneOrganiserDock::getContrastTextColor(const QColor &backgroundColor)
{
    // Calculate luminance using the standard formula
    double r = backgroundColor.redF();
    double g = backgroundColor.greenF();
    double b = backgroundColor.blueF();

    // Apply gamma correction
    r = (r <= 0.03928) ? r / 12.92 : qPow((r + 0.055) / 1.055, 2.4);
    g = (g <= 0.03928) ? g / 12.92 : qPow((g + 0.055) / 1.055, 2.4);
    b = (b <= 0.03928) ? b / 12.92 : qPow((b + 0.055) / 1.055, 2.4);

    // Calculate relative luminance
    double luminance = 0.2126 * r + 0.7152 * g + 0.0722 * b;

    // Return white for dark backgrounds, black for light backgrounds
    return (luminance > 0.5) ? QColor(0, 0, 0) : QColor(255, 255, 255);
}

QColor SceneOrganiserDock::getDefaultThemeTextColor()
{
    if (m_treeView) {
        return m_treeView->palette().color(QPalette::Text);
    }
    // Fallback to a reasonable default if tree view is not available
    return QColor(255, 255, 255); // White text for dark themes (common in OBS)
}

QColor SceneOrganiserDock::adjustColorBrightness(const QColor &color, float factor)
{
    if (!color.isValid()) {
        return color;
    }

    // Convert to HSV to maintain hue and saturation while adjusting brightness
    int h, s, v;
    color.getHsv(&h, &s, &v);

    // Adjust the value (brightness) component
    v = qBound(0, static_cast<int>(v * factor), 255);

    return QColor::fromHsv(h, s, v);
}

// Guards against a theme whose highlight colour is barely distinguishable from
// the row background - the row would read as unpainted, which is the exact
// complaint this replaces. Nudges the fill away from the backdrop until it
// clears a modest luminance gap, preserving hue so themed accents survive.
QColor SceneOrganiserDock::ensureRowContrast(const QColor &bgColor)
{
    if (!bgColor.isValid()) {
        return bgColor;
    }

    const QColor backdrop = m_treeView ? m_treeView->palette().color(QPalette::Base) : QColor(30, 30, 30);

    auto luminance = [](const QColor &c) {
        return 0.2126 * c.redF() + 0.7152 * c.greenF() + 0.0722 * c.blueF();
    };

    const double backdropLum = luminance(backdrop);
    // Push away from the backdrop: lighten on a dark dock, darken on a light one.
    const float factor = (backdropLum < 0.5) ? 1.35f : 0.7f;

    QColor result = bgColor;
    for (int i = 0; i < 4 && qAbs(luminance(result) - backdropLum) < 0.12; ++i) {
        QColor next = adjustColorBrightness(result, factor);
        if (next == result) {
            // Pure black cannot be brightened by scaling value alone.
            next = (backdropLum < 0.5) ? QColor(80, 80, 80) : QColor(175, 175, 175);
        }
        result = next;
    }

    return result;
}

int SceneOrganiserDock::currentRowHeight() const
{
    int rowHeight = StreamUP::SettingsManager::GetCurrentSettings().sceneOrganiserItemHeight;
    if (rowHeight < 19) {
        rowHeight = 24;
    } else if (rowHeight > 48) {
        rowHeight = 48;
    }
    return rowHeight;
}

QColor SceneOrganiserDock::getSelectionColor(const QColor &baseColor)
{
    if (!baseColor.isValid()) {
        // No custom color, use default theme selection
        return m_treeView ? m_treeView->palette().color(QPalette::Highlight) : QColor(26, 127, 207);
    }

    // For custom colors, brighten them for selection
    return adjustColorBrightness(baseColor, 1.3f); // 30% brighter
}

QColor SceneOrganiserDock::getHoverColor(const QColor &baseColor)
{
    if (!baseColor.isValid()) {
        // No custom color, use default theme hover (slightly dimmed selection)
        QColor highlight = m_treeView ? m_treeView->palette().color(QPalette::Highlight) : QColor(26, 127, 207);
        return adjustColorBrightness(highlight, 0.7f);
    }

    // For custom colors, slightly brighten them for hover
    return adjustColorBrightness(baseColor, 1.1f); // 10% brighter
}

void SceneOrganiserDock::onToggleLockClicked()
{
    setLocked(!m_isLocked);
}

void SceneOrganiserDock::onSettingsClicked()
{
    // Open StreamUP settings dialog on Scene Organiser page (tab index 2)
    StreamUP::SettingsManager::ShowSettingsDialog(2);
}

void SceneOrganiserDock::onRenameSceneClicked()
{
    auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) return;

    QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
    auto item = m_model->itemFromIndex(sourceIndex);
    if (!item || item->type() != SceneTreeItem::UserType + 2) return;

    // Start inline editing
    m_treeView->edit(selectedIndexes.first());
}

void SceneOrganiserDock::onRenameFolderClicked()
{
    auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) return;

    QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
    auto item = m_model->itemFromIndex(sourceIndex);
    if (!item || item->type() != SceneFolderItem::UserType + 1) return;

    // Start inline editing. The rename lands when the editor closes, which is
    // handled by the model's itemChanged hook - see connectRenameUndo().
    m_renameLayoutBefore = captureLayout();
    m_treeView->edit(selectedIndexes.first());
}

void SceneOrganiserDock::setLocked(bool locked)
{
    m_isLocked = locked;

    // Update lock checkbox state and tooltip (matching OBS source dock)
    if (m_lockButton) {
        // Block signals to prevent infinite recursion when updating checkbox state
        m_lockButton->blockSignals(true);
        m_lockButton->setChecked(m_isLocked);
        m_lockButton->blockSignals(false);
        m_lockButton->setToolTip(m_isLocked ? obs_module_text("SceneOrganiser.Tooltip.Locked")
                                                : obs_module_text("SceneOrganiser.Tooltip.Unlocked"));
    }

    // Update UI enabled state
    updateUIEnabledState();

    // Apply scene visibility based on lock state
    applySceneVisibility();


    // Update lock action states in context menus
    updateLockActionStates();

    // Save lock state
    m_saveTimer->start();

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "LockState",
        QString("Scene organizer %1").arg(m_isLocked ? "locked" : "unlocked").toUtf8().constData());
}

void SceneOrganiserDock::updateUIEnabledState()
{
    bool unlocked = !m_isLocked;

    // Check current selection for selective enabling
    bool hasSelection = false;
    bool isScene = false;
    if (m_treeView && m_treeView->selectionModel()) {
        auto selected = m_treeView->selectionModel()->selectedIndexes();
        hasSelection = !selected.isEmpty();
        if (hasSelection) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selected.first());
            QStandardItem *item = m_model->itemFromIndex(sourceIndex);
            isScene = (item && item->type() == SceneTreeItem::UserType + 2);
        }
    }

    // NON-DESTRUCTIVE operations - always allowed when unlocked
    if (m_addButton) m_addButton->setEnabled(true); // Add scene/folder is always allowed
    if (m_filtersButton) m_filtersButton->setEnabled(isScene); // Scene filters are non-destructive

    // DESTRUCTIVE operations - only allowed when unlocked
    if (m_removeButton) m_removeButton->setEnabled(unlocked && hasSelection);
    if (m_moveUpButton) m_moveUpButton->setEnabled(unlocked && hasSelection);
    if (m_moveDownButton) m_moveDownButton->setEnabled(unlocked && hasSelection);

    // Tree view drag & drop - only allow when unlocked (moving is destructive)
    // and while no search filter is narrowing the tree. See updateDragEnabled().
    updateDragEnabled();

    // Context menus - enable but control individual destructive actions
    if (m_folderContextMenu) {
        m_folderContextMenu->setEnabled(true);
    }
    if (m_sceneContextMenu) {
        m_sceneContextMenu->setEnabled(true);
    }
    if (m_backgroundContextMenu) {
        m_backgroundContextMenu->setEnabled(true);
    }

    // Control individual destructive context menu actions
    if (m_deleteFolderAction) {
        m_deleteFolderAction->setEnabled(unlocked);
    }
    if (m_deleteSceneAction) {
        m_deleteSceneAction->setEnabled(unlocked);
    }
    // Scene visibility actions - only allowed when unlocked
    if (m_hideSceneAction) {
        m_hideSceneAction->setEnabled(unlocked);
    }
    if (m_showSceneAction) {
        m_showSceneAction->setEnabled(unlocked);
    }
    if (m_sceneMoveUpAction) {
        m_sceneMoveUpAction->setEnabled(unlocked);
    }
    if (m_sceneMoveDownAction) {
        m_sceneMoveDownAction->setEnabled(unlocked);
    }
    if (m_sceneMoveToTopAction) {
        m_sceneMoveToTopAction->setEnabled(unlocked);
    }
    if (m_sceneMoveToBottomAction) {
        m_sceneMoveToBottomAction->setEnabled(unlocked);
    }
}

void SceneOrganiserDock::onSettingsChanged()
{
    // Handle settings changes if needed
    LoadConfiguration();
    applySortingIfEnabled();

    // Apply the item height setting to the tree view
    if (m_treeView) {
        StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();

        // Item height is an absolute row height in pixels (19-48, default 24).
        // Derive icon + font from it; the delegate's sizeHint() enforces the row height.
        int rowHeight = settings.sceneOrganiserItemHeight;
        int iconSize = std::max(10, rowHeight - 8);   // 24px row -> 16px icon
        m_treeView->setIconSize(QSize(iconSize, iconSize));

        int fontSize = std::max(8, (11 * rowHeight) / 24);  // 24px row -> 11pt
        QFont font = m_treeView->font();
        font.setPointSize(fontSize);
        m_treeView->setFont(font);

        // Row height comes from the delegate's sizeHint, which Qt caches, so force a
        // relayout for the new height to take effect, then repaint.
        m_treeView->doItemsLayout();
        m_treeView->viewport()->update();

        // The flat tabs follow the tree's metrics.
        applyRowMetricsToQuickList();

        // Indent guides are painted per row, so a repaint is all they need.
        m_treeView->viewport()->update();
    }

    rebuildTabBar();
}

void SceneOrganiserDock::onIconsChanged()
{
    // Update all existing items to show/hide icons based on current setting
    if (m_model) {
        updateAllItemIcons(m_model->invisibleRootItem());
    }

    // The flat tabs copy their icons from the tree items, so they need rebuilding.
    refreshQuickList();

    // Update checkmarks in context menus
    updateToggleIconsState();
}

void SceneOrganiserDock::updateToggleIconsState()
{
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    bool iconsEnabled = settings.sceneOrganiserShowIcons;

    if (m_folderToggleIconsAction) {
        m_folderToggleIconsAction->setChecked(iconsEnabled);
    }
    if (m_sceneToggleIconsAction) {
        m_sceneToggleIconsAction->setChecked(iconsEnabled);
    }
    if (m_backgroundToggleIconsAction) {
        m_backgroundToggleIconsAction->setChecked(iconsEnabled);
    }
}

void SceneOrganiserDock::updateLockActionStates()
{
    QString lockText = m_isLocked ? "Unlock Scene Organiser" : "Lock Scene Organiser";

    if (m_folderLockAction) {
        m_folderLockAction->setText(lockText);
        m_folderLockAction->setChecked(m_isLocked);
    }
    if (m_sceneLockAction) {
        m_sceneLockAction->setText(lockText);
        m_sceneLockAction->setChecked(m_isLocked);
    }
    if (m_backgroundLockAction) {
        m_backgroundLockAction->setText(lockText);
        m_backgroundLockAction->setChecked(m_isLocked);
    }
}

void SceneOrganiserDock::updateActiveSceneHighlight()
{
    if (!m_model || !m_treeView) {
        return;
    }

    QString current_scene_name;
    QString preview_scene_name;

    // Get the current active (program) scene from OBS
    obs_source_t *current_scene = Canvas::GetCurrentScene(m_canvasType);
    if (current_scene) {
        current_scene_name = QString::fromUtf8(obs_source_get_name(current_scene));
        obs_source_release(current_scene);
    }

    // If in studio mode, also get the preview scene
    const bool studioMode = studioModeFor(m_canvasType);
    if (studioMode) {
        obs_source_t *preview_scene = obs_frontend_get_current_preview_scene();
        if (preview_scene) {
            preview_scene_name = QString::fromUtf8(obs_source_get_name(preview_scene));
            obs_source_release(preview_scene);
        }
    }

    // Mark the LIVE program scene throughout the tree with the dedicated
    // ProgramSceneRole. These rows are drawn with the theme's selected-row
    // look, INDEPENDENTLY of the tree's own selection, so the live scene still
    // reads as live in studio mode where the selection is the preview. This
    // tracks the real program scene no matter how it changed (Stream Deck,
    // hotkey, websocket, OBS scene list, etc.).
    updateActiveSceneHighlightRecursive(m_model->invisibleRootItem(), current_scene_name, preview_scene_name);

    // Whatever is live is by definition the most recent. Held back until the
    // initial load is done so replaying the tree at startup does not rewrite
    // the recents list before it has even been read from disk.
    if (m_initialLoadComplete) {
        noteRecentScene(current_scene_name);

        // noteRecentScene only rebuilds when the order actually changes, but the
        // programme marker moves whenever the live scene does - including
        // back to one already at the top of the list.
        if (m_currentKind != QuickTabKind::Scenes) {
            refreshQuickList();
        }
    }

    // In studio mode, keep the tree SELECTION (blue preview indicator) in sync
    // with OBS' current preview scene, so external preview changes are
    // reflected. Outside studio mode the selection is left entirely to the user
    // (clicks + arrow keys) and is never moved by program changes.
    if (studioMode && !preview_scene_name.isEmpty()) {
        selectSceneByName(preview_scene_name);
    }

    // Repaint so the program indicator refreshes immediately.
    if (m_treeView->viewport()) {
        m_treeView->viewport()->update();
    }

    QString debugMsg = QString("Updated active scene highlight - Program: %1").arg(current_scene_name);
    if (!preview_scene_name.isEmpty()) {
        debugMsg += QString(", Preview: %1").arg(preview_scene_name);
    }
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "ActiveScene", debugMsg.toUtf8().constData());
}

// Walks the tree and sets ProgramSceneRole=true on the scene item matching the
// live program scene, clearing it on every other scene item. Does NOT touch
// selection.
void SceneOrganiserDock::updateActiveSceneHighlightRecursive(QStandardItem *parent, const QString &activeSceneName, const QString &previewSceneName)
{
    Q_UNUSED(previewSceneName);

    if (!parent) {
        return;
    }

    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *item = parent->child(i);
        if (!item) {
            continue;
        }

        if (item->type() == SceneTreeItem::UserType + 2) {
            const bool isProgram = !activeSceneName.isEmpty() && item->text() == activeSceneName;
            // Only write when the state actually changes to avoid needless
            // dataChanged/repaint churn.
            if (item->data(ProgramSceneRole).toBool() != isProgram) {
                item->setData(isProgram, ProgramSceneRole);
            }
        }

        // Recurse into folders.
        if (item->rowCount() > 0) {
            updateActiveSceneHighlightRecursive(item, activeSceneName, previewSceneName);
        }
    }
}

// Moves the tree selection (blue preview/selected indicator) to the named
// scene. Programmatic selection only updates toolbar button state
// (onSceneSelectionChanged); it does NOT switch OBS scenes, so there is no
// feedback loop back to OBS.
void SceneOrganiserDock::selectSceneByName(const QString &sceneName)
{
    if (!m_model || !m_treeView || !m_proxyModel || sceneName.isEmpty()) {
        return;
    }

    QStandardItem *match = nullptr;
    std::function<void(QStandardItem *)> findItem = [&](QStandardItem *parent) {
        if (!parent || match) {
            return;
        }
        for (int i = 0; i < parent->rowCount(); ++i) {
            QStandardItem *item = parent->child(i);
            if (!item) {
                continue;
            }
            if (item->type() == SceneTreeItem::UserType + 2 && item->text() == sceneName) {
                match = item;
                return;
            }
            if (item->rowCount() > 0) {
                findItem(item);
            }
        }
    };
    findItem(m_model->invisibleRootItem());

    if (!match) {
        return;
    }

    QModelIndex proxyIndex = m_proxyModel->mapFromSource(match->index());
    if (!proxyIndex.isValid()) {
        return; // e.g. filtered out by the search box
    }

    QItemSelectionModel *selModel = m_treeView->selectionModel();
    if (selModel && selModel->currentIndex() != proxyIndex) {
        selModel->setCurrentIndex(
            proxyIndex,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
}

// Activates the currently selected scene (bound to Enter/Return in the tree).
// Studio mode: set it as preview and trigger the preview->program transition
// (matching OBS' own behaviour). Non-studio mode: set it as the current
// program scene. Arrow keys keep moving selection only; Enter is what commits.
void SceneOrganiserDock::triggerActivateSelectedScene()
{
    if (!m_model || !m_treeView || !m_proxyModel) {
        return;
    }

    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) {
        return;
    }

    QModelIndex sourceIndex = m_proxyModel->mapToSource(selected.first());
    activateSceneItem(m_model->itemFromIndex(sourceIndex));
}

// Takes a scene item live. In studio mode that means setting preview and
// triggering the transition; otherwise it cuts straight to program. Folders and
// nulls are ignored, so callers can hand over whatever the tree gave them.
void SceneOrganiserDock::activateSceneItem(QStandardItem *item)
{
    if (!item || item->type() != SceneTreeItem::UserType + 2) {
        return; // only scenes can be activated (folders ignored)
    }

    obs_source_t *source = Canvas::FindScene(m_canvasType, item->text().toUtf8().constData());
    if (!source) {
        return;
    }

    if (studioModeFor(m_canvasType)) {
        obs_frontend_set_current_preview_scene(source);
        obs_frontend_preview_program_trigger_transition();
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Activate",
            QString("Transitioned '%1' to program").arg(item->text()).toUtf8().constData());
    } else {
        Canvas::SetCurrentScene(m_canvasType, source);
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Activate",
            QString("Set program scene to '%1'").arg(item->text()).toUtf8().constData());
    }

    obs_source_release(source);
}

// Enter in the search box goes live with the first scene the filter left
// standing, reading the tree top to bottom exactly as drawn. Folder rows are
// skipped (they survive the filter to hold their matching children) as are
// hidden scenes, which are not switchable from this dock.
void SceneOrganiserDock::activateFirstSearchMatch()
{
    // On a tab, the first row still showing is the one to go live with.
    if (m_currentKind != QuickTabKind::Scenes) {
        if (m_quickProxy && m_quickProxy->rowCount() > 0) {
            std::function<bool(const QModelIndex &)> firstScene = [&](const QModelIndex &parent) {
                for (int i = 0; i < m_quickProxy->rowCount(parent); ++i) {
                    const QModelIndex proxyIndex = m_quickProxy->index(i, 0, parent);
                    QStandardItem *item = m_quickModel->itemFromIndex(m_quickProxy->mapToSource(proxyIndex));
                    if (item && (item->flags() & Qt::ItemIsEnabled) && !item->data(TabItemIsFolderRole).toBool()) {
                        if (QStandardItem *treeItem = m_model->findSceneItemByName(item->text())) {
                            activateSceneItem(treeItem);
                            return true;
                        }
                    }
                    if (firstScene(proxyIndex)) {
                        return true;
                    }
                }
                return false;
            };
            firstScene(QModelIndex());
        }
        return;
    }

    if (!m_treeView || !m_proxyModel || !m_model) {
        return;
    }

    QStandardItem *match = nullptr;
    std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &parent) {
        const int rows = m_proxyModel->rowCount(parent);
        for (int i = 0; i < rows && !match; ++i) {
            const QModelIndex proxyIndex = m_proxyModel->index(i, 0, parent);
            QStandardItem *item = m_model->itemFromIndex(m_proxyModel->mapToSource(proxyIndex));
            if (item && item->type() == SceneTreeItem::UserType + 2 && !m_hiddenScenes.contains(item->text())) {
                match = item;
                return;
            }
            walk(proxyIndex);
        }
    };
    walk(QModelIndex());

    if (!match) {
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Search", "Enter pressed with no switchable match");
        return;
    }

    activateSceneItem(match);
    selectSceneByName(match->text());
}

// Puts the cursor in this dock's search box, raising the dock first so the
// hotkey still works when it is tabbed behind another dock. Bound to a
// frontend hotkey per canvas - see hotkey-manager.cpp.
void SceneOrganiserDock::FocusSearchBox(CanvasType canvasType)
{
    for (SceneOrganiserDock *dock : s_dockInstances) {
        if (!dock || dock->GetCanvasType() != canvasType || !dock->m_searchEdit) {
            continue;
        }

        if (QWidget *dockWidget = dock->parentWidget()) {
            dockWidget->raise();
            dockWidget->show();
        }
        dock->m_searchEdit->setFocus(Qt::ShortcutFocusReason);
        dock->m_searchEdit->selectAll();
        return;
    }
}

// Writes a folder row's open/closed state onto the item and repaints its icon.
// Expansion belongs to the view, the icon belongs to the item, so this is the
// join between the two.
void SceneOrganiserDock::setFolderExpandedState(const QModelIndex &proxyIndex, bool expanded)
{
    if (!m_model || !m_proxyModel || !proxyIndex.isValid()) {
        return;
    }

    QStandardItem *item = m_model->itemFromIndex(m_proxyModel->mapToSource(proxyIndex));
    if (!item || item->type() != SceneFolderItem::UserType + 1) {
        return;
    }

    item->setData(expanded, FolderExpandedRole);
    static_cast<SceneFolderItem *>(item)->updateIcon();
}

// Walks the whole tree and brings every folder's icon in line with whether that
// row is actually open. Used after a load or a rebuild, when rows have been
// expanded without anyone having gone through the signal above.
void SceneOrganiserDock::syncFolderIcons(QStandardItem *parent)
{
    if (!m_model || !m_treeView || !m_proxyModel) {
        return;
    }
    if (!parent) {
        parent = m_model->invisibleRootItem();
    }

    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *child = parent->child(i);
        if (!child) {
            continue;
        }

        if (child->type() == SceneFolderItem::UserType + 1) {
            const QModelIndex proxyIndex = m_proxyModel->mapFromSource(m_model->indexFromItem(child));
            const bool expanded = proxyIndex.isValid() && m_treeView->isExpanded(proxyIndex);
            if (child->data(FolderExpandedRole).toBool() != expanded) {
                child->setData(expanded, FolderExpandedRole);
                static_cast<SceneFolderItem *>(child)->updateIcon();
            }
        }

        if (child->rowCount() > 0) {
            syncFolderIcons(child);
        }
    }
}

void SceneOrganiserDock::updateAllItemIcons(QStandardItem *parent)
{
    if (!parent) return;

    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *item = parent->child(i);
        if (!item) continue;

        // Update icon for this item
        if (item->type() == SceneFolderItem::UserType + 1) {
            // Folder item
            SceneFolderItem *folderItem = static_cast<SceneFolderItem*>(item);
            folderItem->updateIcon();
        } else if (item->type() == SceneTreeItem::UserType + 2) {
            // Scene item
            SceneTreeItem *sceneItem = static_cast<SceneTreeItem*>(item);
            sceneItem->updateIcon();
        }

        // Recursively update children
        updateAllItemIcons(item);
    }
}

// The one-time load: config, saved folder tree, scenes, colours, then enable
// saves. Normally driven by FINISHED_LOADING, but a dock created AFTER that
// event has already fired (the Vertical dock, which only exists once its canvas
// does) never sees it and has to be kicked directly - see
// CreateVerticalSceneOrganiserDock(). Guarded so a dock that gets both a direct
// kick and the event does not load twice.
void SceneOrganiserDock::performInitialLoad()
{
    if (m_initialLoadStarted) {
        return;
    }
    m_initialLoadStarted = true;

    QTimer::singleShot(100, this, [this]() {
        LoadConfiguration();
        m_model->loadSceneTree();
        refreshSceneList();
        applyAllCustomColors();

        // Restore folder expansion state after tree is fully loaded
        // and mark initial load as complete to allow saves
        QTimer::singleShot(500, this, [this]() {
            StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
            if (settings.sceneOrganiserRememberFolderState) {
                restoreFolderExpansionState();
            }
            // Mark initial load as complete - saves are now allowed
            m_initialLoadComplete = true;

            // Last word on the tab bar. LoadConfiguration builds it too, but at
            // that point the scene tree is still filling; rebuilding here means
            // the bar is built once from finished data.
            rebuildTabBar();
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Init",
                "Initial load complete - saves now enabled");
        });
    });
}

void SceneOrganiserDock::onFrontendEvent(enum obs_frontend_event event, void *private_data)
{
    auto dock = static_cast<SceneOrganiserDock*>(private_data);

    switch (event) {
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
        dock->performInitialLoad();
        break;
    case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
        // Skip during scene collection changes to avoid accessing stale scene data
        if (!dock->m_initialLoadComplete)
            break;
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Event", "Scene list changed event received");
        QTimer::singleShot(50, dock, [dock]() {
            if (!dock->m_initialLoadComplete)
                return;
            dock->refreshSceneList();
        });
        break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
        // Save current scene tree and settings before switching (only if initial load completed)
        if (dock->m_initialLoadComplete) {
            dock->m_model->saveSceneTree();
            dock->SaveConfiguration();
        }
        // Disable saves during collection switch to prevent race conditions
        dock->m_initialLoadComplete = false;
        break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
        QTimer::singleShot(100, dock, [dock]() {
            dock->LoadConfiguration();
            dock->m_model->loadSceneTree();
            dock->refreshSceneList();
            dock->applyAllCustomColors();

            // Restore folder expansion state after tree is fully loaded
            // and re-enable saves after load completes
            QTimer::singleShot(500, dock, [dock]() {
                StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
                if (settings.sceneOrganiserRememberFolderState) {
                    dock->restoreFolderExpansionState();
                }
                // Re-enable saves after collection load completes
                dock->m_initialLoadComplete = true;
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "CollectionChange",
                    "Scene collection load complete - saves now enabled");
            });
        });
        break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_RENAMED:
        QTimer::singleShot(100, dock, [dock]() {
            dock->m_model->saveSceneTree();
            dock->refreshSceneList();
        });
        break;
    case OBS_FRONTEND_EVENT_SCENE_CHANGED:
        if (!dock->m_initialLoadComplete)
            break;
        QTimer::singleShot(50, dock, [dock]() {
            if (!dock->m_initialLoadComplete)
                return;
            dock->updateActiveSceneHighlight();
        });
        break;
    case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
        // Update highlighting when preview scene changes in studio mode
        if (!dock->m_initialLoadComplete)
            break;
        QTimer::singleShot(50, dock, [dock]() {
            if (!dock->m_initialLoadComplete)
                return;
            dock->updateActiveSceneHighlight();
        });
        break;
    case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
    case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
        // Update highlighting when studio mode is toggled
        QTimer::singleShot(50, dock, [dock]() {
            dock->updateActiveSceneHighlight();
        });
        break;
    case OBS_FRONTEND_EVENT_THEME_CHANGED:
        // Handle theme changes - clear caches and update icons
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Theme", "Theme changed event received");
        QTimer::singleShot(50, dock, [dock]() {
            dock->onThemeChanged();
        });
        break;
    case OBS_FRONTEND_EVENT_EXIT:
        // Save configuration before OBS exits
        if (dock->m_initialLoadComplete) {
            dock->m_model->saveSceneTree();
            dock->SaveConfiguration();
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Exit",
                "Saved scene tree on OBS exit");
        }
        break;
    default:
        break;
    }
}

void SceneOrganiserDock::SaveConfiguration()
{
    // Prevent saving before initial load completes to avoid overwriting valid data
    if (!m_initialLoadComplete) {
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Save",
            "Skipping save - initial load not yet complete");
        return;
    }

    char *scene_collection = obs_frontend_get_current_scene_collection();
    if (!scene_collection) return;

    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) {
        bfree(scene_collection);
        return;
    }

    QString configDir = QString::fromUtf8(configPath);
    QString sceneCollectionName = QString::fromUtf8(scene_collection);
    bfree(configPath);
    bfree(scene_collection);

    // Ensure the directory exists
    QDir dir;
    if (!dir.mkpath(configDir)) {
        StreamUP::DebugLogger::LogInfo("SceneOrganiser",
            QString("Failed to create config directory: %1").arg(configDir).toUtf8().constData());
        return;
    }

    // Save lock state per scene collection
    QString lockStateFile = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_lock_state.txt";
    QFile file(lockStateFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << (m_isLocked ? "locked" : "unlocked");
        file.close();
    }

    // Save hidden scenes per scene collection
    QString hiddenScenesFile = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_hidden_scenes.txt";
    QFile hiddenFile(hiddenScenesFile);
    if (hiddenFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&hiddenFile);
        QStringList hiddenScenesList = QStringList(m_hiddenScenes.begin(), m_hiddenScenes.end());
        out << hiddenScenesList.join("\n");
        hiddenFile.close();
    }

    saveQuickTabs(configDir, sceneCollectionName);

    // Now using DigitOtter approach - save is handled by saveSceneTree()
    // which is called automatically on OBS frontend events
    m_model->saveSceneTree();

    // Save folder expansion state if setting is enabled
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    if (settings.sceneOrganiserRememberFolderState) {
        saveFolderExpansionState();
    }

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
        QString("Configuration saved for scene collection '%1' (lock state: %2)")
        .arg(sceneCollectionName)
        .arg(m_isLocked ? "locked" : "unlocked").toUtf8().constData());
}

void SceneOrganiserDock::LoadConfiguration()
{
    char *scene_collection = obs_frontend_get_current_scene_collection();
    if (!scene_collection) return;

    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) {
        bfree(scene_collection);
        return;
    }

    QString configDir = QString::fromUtf8(configPath);
    QString sceneCollectionName = QString::fromUtf8(scene_collection);
    bfree(configPath);
    bfree(scene_collection);

    // Load lock state per scene collection
    QString lockStateFile = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_lock_state.txt";
    QFile file(lockStateFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString lockState = in.readAll().trimmed();
        bool shouldBeLocked = (lockState == "locked");
        setLocked(shouldBeLocked);
        file.close();
    } else {
        // Default to unlocked if no saved state
        setLocked(false);
    }

    loadQuickTabs(configDir, sceneCollectionName);
    rebuildTabBar();
    updateControlsForTab();

    // Load hidden scenes per scene collection
    QString hiddenScenesFile = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_hidden_scenes.txt";
    QFile hiddenFile(hiddenScenesFile);
    if (hiddenFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&hiddenFile);
        QString hiddenScenesText = in.readAll().trimmed();
        if (!hiddenScenesText.isEmpty()) {
            QStringList hiddenScenesList = hiddenScenesText.split("\n", Qt::SkipEmptyParts);
            m_hiddenScenes = QSet<QString>(hiddenScenesList.begin(), hiddenScenesList.end());

            // Apply scene visibility after loading hidden scenes
            // Use longer delay to ensure scene tree is fully populated
            QTimer::singleShot(500, this, [this]() {
                updateHiddenScenesStyling();
                applySceneVisibility();
            });
        }
        hiddenFile.close();
    } else {
        // Clear hidden scenes if no saved state for this collection
        m_hiddenScenes.clear();
    }

    // Check if we need to prompt for migration
    // Check if migration is available even if lock file exists, to handle cases where
    // user previously used the plugin but hasn't migrated from obs_scene_tree_view yet
    // Check persistent migration history file to avoid re-prompting
    QString migrationHistoryFile = configDir + "/migration_prompted_collections.txt";
    QFile historyFile(migrationHistoryFile);
    QSet<QString> promptedCollections;

    // Load previously prompted collections from file
    if (historyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&historyFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                promptedCollections.insert(line);
            }
        }
        historyFile.close();
    }

    // Static set to track which collections are currently being prompted (to prevent race condition)
    // This prevents multiple dock instances from showing the import prompt simultaneously
    static QSet<QString> currentlyPrompting;

    // Check for migration availability if we haven't prompted yet for this collection
    // Also check if we're not already prompting for this collection (prevents duplicate prompts)
    if (m_model && !promptedCollections.contains(sceneCollectionName) && !currentlyPrompting.contains(sceneCollectionName)) {
        QString migrationConfigPath;
        if (SceneTreeModel::checkMigrationAvailable(sceneCollectionName, migrationConfigPath)) {
            // Mark this collection as currently being prompted (prevents other dock instances from showing the prompt)
            currentlyPrompting.insert(sceneCollectionName);

            // Mark this collection as prompted by saving to file immediately
            promptedCollections.insert(sceneCollectionName);
            if (historyFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&historyFile);
                for (const QString &collection : promptedCollections) {
                    out << collection << "\n";
                }
                historyFile.close();
            }

            // Use QTimer to ensure prompt appears after UI is fully loaded
            QTimer::singleShot(500, this, [this, sceneCollectionName]() {
                QPointer<SceneOrganiserDock> self(this);
                auto *dlg = su::confirm(this,
                    QString::fromUtf8("Import Scene Organization?"),
                    QString("Settings from the SceneTree plugin were found for '%1'.\n\n"
                            "Would you like to import your scene organization?\n\n"
                            "You can access the Scene Organizer from View → Docks → Scene Organizer.").arg(sceneCollectionName),
                    QString::fromUtf8("Import"),
                    "primary",
                    [self, sceneCollectionName]() {
                        if (!self) return;
                        if (self->m_model->migrateCurrentCollection()) {
                            // Reload configuration and tree for ALL dock instances
                            for (auto dock : SceneOrganiserDock::s_dockInstances) {
                                if (dock && dock->m_model) {
                                    dock->LoadConfiguration();
                                    dock->m_model->loadSceneTree();
                                    dock->m_model->updateTree();
                                    dock->applyAllCustomColors();
                                }
                            }

                            su::info(self,
                                QString::fromUtf8("Import Successful"),
                                QString("Successfully imported scene organization for '%1'!\n\n"
                                        "The Scene Organizer can be accessed from View → Docks → Scene Organizer.").arg(sceneCollectionName));
                        } else {
                            su::info(self,
                                QString::fromUtf8("Import Failed"),
                                QString::fromUtf8("Failed to import settings. Please check the log for details."));
                        }
                    });

                // Remove from currently prompting set once the (modeless) dialog
                // is dismissed — covers both the Import and Cancel paths.
                QObject::connect(dlg, &QObject::destroyed, qApp, [sceneCollectionName]() {
                    currentlyPrompting.remove(sceneCollectionName);
                });
            });
        }
    }

    // Now using the DigitOtter approach - this is handled by frontend events
    // No need to manually load configuration here
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
        QString("Configuration loaded for scene collection '%1' (lock state: %2)")
        .arg(sceneCollectionName)
        .arg(m_isLocked ? "locked" : "unlocked").toUtf8().constData());

    // Apply scene visibility after full initialization
    // Note: Folder expansion state is restored in the frontend event handlers
    // (FINISHED_LOADING and SCENE_COLLECTION_CHANGED) after the tree is fully refreshed
    QTimer::singleShot(1000, this, [this]() {
        updateHiddenScenesStyling();
        applySceneVisibility();
    });
}

// Search functionality implementation
// Drag and drop is allowed only when the tree is unlocked AND unfiltered.
// A drop while a search is active lands against the proxy's filtered rows, so
// the scene ends up somewhere the user never saw - the neighbours it appeared
// to land between are not its real neighbours in the unfiltered tree. Rather
// than try to translate the drop, we take dragging away for the duration.
void SceneOrganiserDock::updateDragEnabled()
{
    if (!m_treeView) {
        return;
    }

    const bool searching = m_proxyModel && !m_proxyModel->filterRegularExpression().pattern().isEmpty();
    const bool allowed = !m_isLocked && !searching;

    m_treeView->setDragEnabled(allowed);
    m_treeView->setAcceptDrops(allowed);
    m_treeView->setDragDropMode(allowed ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);
}

void SceneOrganiserDock::onSearchTextChanged(const QString &text)
{
    if (!m_proxyModel) return;

    // Check if we have search text for expansion logic
    bool hasText = !text.isEmpty();
    bool hadText = !m_proxyModel->filterRegularExpression().pattern().isEmpty();

    // Save expansion state when starting a search
    if (hasText && !hadText) {
        saveExpansionState();
    }

    // The filter follows whichever tree is on screen.
    if (m_currentKind != QuickTabKind::Scenes) {
        if (m_quickProxy) {
            m_quickProxy->setFilterWildcard(text);
            m_quickTree->expandAll();
        }
        return;
    }

    // Apply filter to proxy model
    m_proxyModel->setFilterWildcard(text);

    // Expand all items when searching to show matches
    if (hasText) {
        m_treeView->expandAll();
    } else {
        // Restore previous expansion state when search is cleared
        restoreExpansionState();
    }

    // Reordering against a filtered tree is not meaningful - see updateDragEnabled().
    updateDragEnabled();

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Search",
        QString("Search filter applied: '%1'").arg(text).toUtf8().constData());
}

void SceneOrganiserDock::onClearSearch()
{
    if (!m_searchEdit) return;

    m_searchEdit->clear();
    m_searchEdit->clearFocus();

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Search", "Search cleared");
}

void SceneOrganiserDock::saveExpansionState()
{
    if (!m_treeView || !m_proxyModel) return;

    m_savedExpansionState.clear();

    // Recursively save expansion state for all items
    std::function<void(const QModelIndex&)> saveRecursive = [&](const QModelIndex& proxyIndex) {
        if (!proxyIndex.isValid()) return;

        // Convert proxy index to source index for stable storage
        QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) return;

        // Save expansion state using source model index
        bool isExpanded = m_treeView->isExpanded(proxyIndex);
        m_savedExpansionState[QPersistentModelIndex(sourceIndex)] = isExpanded;

        // Recursively save children
        int rowCount = m_proxyModel->rowCount(proxyIndex);
        for (int i = 0; i < rowCount; ++i) {
            QModelIndex childIndex = m_proxyModel->index(i, 0, proxyIndex);
            saveRecursive(childIndex);
        }
    };

    // Start from root items
    int rootRowCount = m_proxyModel->rowCount();
    for (int i = 0; i < rootRowCount; ++i) {
        QModelIndex rootIndex = m_proxyModel->index(i, 0);
        saveRecursive(rootIndex);
    }

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Search",
        QString("Saved expansion state for %1 items").arg(m_savedExpansionState.size()).toUtf8().constData());
}

void SceneOrganiserDock::restoreExpansionState()
{
    if (!m_treeView || !m_proxyModel || m_savedExpansionState.isEmpty()) return;

    // Restore expansion state for all saved items
    for (auto it = m_savedExpansionState.begin(); it != m_savedExpansionState.end(); ++it) {
        QPersistentModelIndex sourceIndex = it.key();
        bool wasExpanded = it.value();

        if (sourceIndex.isValid()) {
            // Convert source index back to proxy index for tree view
            QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
            if (proxyIndex.isValid()) {
                m_treeView->setExpanded(proxyIndex, wasExpanded);
            }
        }
    }

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Search",
        QString("Restored expansion state for %1 items").arg(m_savedExpansionState.size()).toUtf8().constData());

    // Clear the saved state after restoring
    m_savedExpansionState.clear();
}

void SceneOrganiserDock::saveFolderExpansionState()
{
    if (!m_treeView || !m_model) return;

    char *scene_collection = obs_frontend_get_current_scene_collection();
    if (!scene_collection) return;

    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) {
        bfree(scene_collection);
        return;
    }

    QString configDir = QString::fromUtf8(configPath);
    QString sceneCollectionName = QString::fromUtf8(scene_collection);
    bfree(configPath);
    bfree(scene_collection);

    // Collect expanded folder names
    QStringList expandedFolders;
    std::function<void(const QModelIndex&)> collectExpanded = [&](const QModelIndex& index) {
        if (!index.isValid()) return;

        // Map proxy index to source index
        QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        QStandardItem *item = m_model->itemFromIndex(sourceIndex);

        if (item && item->type() == SceneFolderItem::UserType + 1) {
            if (m_treeView->isExpanded(index)) {
                expandedFolders.append(item->text());
            }
        }

        // Recursively check children
        int rowCount = m_proxyModel->rowCount(index);
        for (int i = 0; i < rowCount; ++i) {
            QModelIndex childIndex = m_proxyModel->index(i, 0, index);
            collectExpanded(childIndex);
        }
    };

    // Start from root items
    int rootRowCount = m_proxyModel->rowCount();
    for (int i = 0; i < rootRowCount; ++i) {
        QModelIndex rootIndex = m_proxyModel->index(i, 0);
        collectExpanded(rootIndex);
    }

    // Save to file
    QString expansionFile = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_expansion_state.txt";
    QFile file(expansionFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << expandedFolders.join("\n");
        file.close();

        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
            QString("Saved expansion state for %1 folders").arg(expandedFolders.size()).toUtf8().constData());
    }
}

void SceneOrganiserDock::restoreFolderExpansionState()
{
    if (!m_treeView || !m_model) return;

    char *scene_collection = obs_frontend_get_current_scene_collection();
    if (!scene_collection) return;

    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) {
        bfree(scene_collection);
        return;
    }

    QString configDir = QString::fromUtf8(configPath);
    QString sceneCollectionName = QString::fromUtf8(scene_collection);
    bfree(configPath);
    bfree(scene_collection);

    // Load from file
    QString expansionFile = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_expansion_state.txt";
    QFile file(expansionFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return; // No saved state, nothing to restore
    }

    QTextStream in(&file);
    QString expandedFoldersText = in.readAll().trimmed();
    file.close();

    if (expandedFoldersText.isEmpty()) {
        return;
    }

    QStringList expandedFoldersList = expandedFoldersText.split("\n", Qt::SkipEmptyParts);
    QSet<QString> expandedFolders = QSet<QString>(expandedFoldersList.begin(), expandedFoldersList.end());

    // Restore expansion state
    std::function<void(const QModelIndex&)> restoreExpanded = [&](const QModelIndex& index) {
        if (!index.isValid()) return;

        // Map proxy index to source index
        QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        QStandardItem *item = m_model->itemFromIndex(sourceIndex);

        if (item && item->type() == SceneFolderItem::UserType + 1) {
            if (expandedFolders.contains(item->text())) {
                m_treeView->setExpanded(index, true);
            }
        }

        // Recursively restore children
        int rowCount = m_proxyModel->rowCount(index);
        for (int i = 0; i < rowCount; ++i) {
            QModelIndex childIndex = m_proxyModel->index(i, 0, index);
            restoreExpanded(childIndex);
        }
    };

    // Start from root items
    int rootRowCount = m_proxyModel->rowCount();
    for (int i = 0; i < rootRowCount; ++i) {
        QModelIndex rootIndex = m_proxyModel->index(i, 0);
        restoreExpanded(rootIndex);
    }

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
        QString("Restored expansion state for %1 folders").arg(expandedFolders.size()).toUtf8().constData());

    // Update button state to reflect the restored expansion state
    updateExpandCollapseButtonState();
}

// Public methods for keyboard shortcuts
void SceneOrganiserDock::triggerRename()
{
    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();
    if (!selected.isEmpty()) {
        QModelIndex proxyIndex = selected.first();
        QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        QStandardItem *item = m_model->itemFromIndex(sourceIndex);
        if (item) {
            if (item->type() == SceneTreeItem::UserType + 2) {
                // Scene item - trigger scene rename
                onRenameSceneClicked();
            } else if (item->type() == SceneFolderItem::UserType + 1) {
                // Folder item - trigger folder rename
                onRenameFolderClicked();
            }
        }
    }
}

void SceneOrganiserDock::triggerRemove()
{
    onRemoveClicked();
}

// New OBS-compatible context menu actions

void SceneOrganiserDock::onDuplicateSceneClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString currentSceneName = m_currentContextItem->text();
    obs_source_t *currentSource = Canvas::FindScene(m_canvasType, currentSceneName.toUtf8().constData());
    if (!currentSource) return;

    obs_scene_t *currentScene = obs_scene_from_source(currentSource);
    if (!currentScene) {
        obs_source_release(currentSource);
        return;
    }

    // Generate unique name for duplicated scene
    QString format = currentSceneName + " %1";
    int i = 2;
    QString newName = format.arg(i);
    obs_source_t *existing = nullptr;
    while ((existing = Canvas::FindScene(m_canvasType, newName.toUtf8().constData())) != nullptr) {
        obs_source_release(existing);
        newName = format.arg(++i);
    }

    // Duplicate the scene
    obs_scene_t *duplicatedScene = obs_scene_duplicate(currentScene, newName.toUtf8().constData(), OBS_SCENE_DUP_REFS);
    if (duplicatedScene) {
        obs_source_t *newSource = obs_scene_get_source(duplicatedScene);

        // obs_scene_duplicate always creates on the main canvas, so a vertical
        // duplicate has to be moved across or it would land in the wrong dock.
        if (m_canvasType == CanvasType::Vertical) {
            if (obs_canvas_t *canvas = Canvas::Acquire(m_canvasType)) {
                obs_canvas_move_scene(duplicatedScene, canvas);
                obs_canvas_release(canvas);
            }
        }
        Canvas::SetCurrentScene(m_canvasType, newSource);

        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Scene Duplication",
            QString("Duplicated scene '%1' to '%2'").arg(currentSceneName, newName).toUtf8().constData());
        obs_scene_release(duplicatedScene);
    }

    obs_source_release(currentSource);
}

void SceneOrganiserDock::onDeleteSceneClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();
    obs_source_t *source = Canvas::FindScene(m_canvasType, sceneName.toUtf8().constData());
    if (!source) return;
    // The confirm dialog is modeless; don't hold the source ref across it.
    // Re-acquire by name inside the accept callback instead.
    obs_source_release(source);

    QPointer<SceneOrganiserDock> self(this);
    su::confirm(this,
        QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Title"), -1),
        QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Text"), -1).arg(sceneName),
        QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Title"), -1),
        "danger",
        [self, sceneName]() {
            if (!self) return;
            obs_source_t *source = Canvas::FindScene(self->GetCanvasType(), sceneName.toUtf8().constData());
            if (!source) return;
            obs_source_remove(source);
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Scene Deletion",
                QString("Deleted scene: %1").arg(sceneName).toUtf8().constData());
            obs_source_release(source);
        });
}

void SceneOrganiserDock::onHideSceneClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();

    // Add scene to hidden list
    m_hiddenScenes.insert(sceneName);

    // Apply visual styling when unlocked (grayed-out color to indicate it will be hidden when locked)
    QColor grayColor = getDefaultThemeTextColor();
    grayColor.setAlpha(100); // Make it semi-transparent for a grayed-out effect
    m_currentContextItem->setForeground(QBrush(grayColor));

    // If dock is locked, immediately hide the scene from the tree
    if (m_isLocked) {
        applySceneVisibility();
    }

    // Save configuration immediately to persist hidden scenes
    m_saveTimer->start(500);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Scene Visibility",
        QString("Hidden scene: %1").arg(sceneName).toUtf8().constData());
}

void SceneOrganiserDock::onShowSceneClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();

    // Remove scene from hidden list
    m_hiddenScenes.remove(sceneName);

    // Reset to default theme text color
    m_currentContextItem->setForeground(QBrush(getDefaultThemeTextColor()));

    // If dock is locked, immediately show the scene in the tree
    if (m_isLocked) {
        applySceneVisibility();
    }

    // Save configuration immediately to persist visible scenes
    m_saveTimer->start(500);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Scene Visibility",
        QString("Shown scene: %1").arg(sceneName).toUtf8().constData());
}

void SceneOrganiserDock::applySceneVisibility()
{
    if (!m_model || !m_treeView) return;

    // Recursively apply visibility to all scene items in the tree
    applySceneVisibilityRecursive(m_model->invisibleRootItem());

    // Force update of the proxy model to refresh the view
    if (m_proxyModel) {
        m_proxyModel->invalidate();
    }
}

void SceneOrganiserDock::applySceneVisibilityRecursive(QStandardItem *parent)
{
    if (!parent) return;

    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *child = parent->child(i);
        if (!child) continue;

        if (child->type() == SceneTreeItem::UserType + 2) { // Scene item
            QString sceneName = child->text();
            bool shouldHide = m_isLocked && m_hiddenScenes.contains(sceneName);

            // Get the model index for this item
            QModelIndex index = child->index();
            if (m_treeView) {
                // Map to proxy model index
                QModelIndex proxyIndex = m_proxyModel->mapFromSource(index);
                // Hide/show the row in the tree view
                m_treeView->setRowHidden(proxyIndex.row(), proxyIndex.parent(), shouldHide);
            }
        } else if (child->hasChildren()) {
            // Recursively check folder contents
            applySceneVisibilityRecursive(child);
        }
    }
}

void SceneOrganiserDock::updateHiddenScenesStyling()
{
    if (!m_model) return;
    updateHiddenScenesStylingRecursive(m_model->invisibleRootItem());
}

void SceneOrganiserDock::updateHiddenScenesStylingRecursive(QStandardItem *parent)
{
    // DISABLED - No custom styling
    Q_UNUSED(parent);
}

void SceneOrganiserDock::onCopyFiltersClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();
    obs_source_t *source = Canvas::FindScene(m_canvasType, sceneName.toUtf8().constData());
    if (!source) return;

    // Release previous copy source if any
    if (m_copyFiltersSource) {
        obs_weak_source_release(m_copyFiltersSource);
    }

    // Store weak reference to source for filter copying
    m_copyFiltersSource = obs_source_get_weak_source(source);
    obs_source_release(source);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Filter Copy",
        QString("Copied filters from scene: %1").arg(sceneName).toUtf8().constData());
}

void SceneOrganiserDock::onPasteFiltersClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;
    if (!m_copyFiltersSource || obs_weak_source_expired(m_copyFiltersSource)) return;

    QString targetSceneName = m_currentContextItem->text();
    obs_source_t *targetSource = Canvas::FindScene(m_canvasType, targetSceneName.toUtf8().constData());
    obs_source_t *sourceSource = obs_weak_source_get_source(m_copyFiltersSource);

    if (!targetSource || !sourceSource) {
        if (targetSource) obs_source_release(targetSource);
        if (sourceSource) obs_source_release(sourceSource);
        return;
    }

    // Copy all filters from source to target
    obs_source_copy_filters(targetSource, sourceSource);

    obs_source_release(targetSource);
    obs_source_release(sourceSource);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Filter Paste",
        QString("Pasted filters to scene: %1").arg(targetSceneName).toUtf8().constData());
}

void SceneOrganiserDock::onSceneFiltersClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();
    obs_source_t *source = Canvas::FindScene(m_canvasType, sceneName.toUtf8().constData());
    if (!source) return;

    obs_frontend_open_source_filters(source);
    obs_source_release(source);
}

void SceneOrganiserDock::onScreenshotSceneClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();
    obs_source_t *source = Canvas::FindScene(m_canvasType, sceneName.toUtf8().constData());
    if (!source) return;

    obs_frontend_take_source_screenshot(source);
    obs_source_release(source);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Screenshot",
        QString("Taking screenshot of scene: %1").arg(sceneName).toUtf8().constData());
}

void SceneOrganiserDock::onShowInMultiviewClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();
    obs_source_t *source = Canvas::FindScene(m_canvasType, sceneName.toUtf8().constData());
    if (!source) return;

    obs_data_t *privateSettings = obs_source_get_private_settings(source);
    obs_data_set_default_bool(privateSettings, "show_in_multiview", true);
    bool showInMultiview = obs_data_get_bool(privateSettings, "show_in_multiview");
    obs_data_set_bool(privateSettings, "show_in_multiview", !showInMultiview);
    obs_data_release(privateSettings);

    obs_source_release(source);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Multiview Toggle",
        QString("Toggled multiview for scene: %1 (now %2)").arg(sceneName, !showInMultiview ? "visible" : "hidden").toUtf8().constData());
}

void SceneOrganiserDock::onOpenProjectorClicked()
{
    // This would be called by monitor-specific projector actions
    // Implementation would involve creating projectors for specific monitors
}

void SceneOrganiserDock::onOpenProjectorOnMonitorClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QAction *action = qobject_cast<QAction*>(sender());
    if (!action) return;

    int monitorIndex = action->property("monitor").toInt();
    QString sceneName = m_currentContextItem->text();

    obs_frontend_open_projector("Scene", monitorIndex, nullptr, sceneName.toUtf8().constData());

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Projector Monitor",
        QString("Opened projector for scene '%1' on monitor %2").arg(sceneName).arg(monitorIndex).toUtf8().constData());
}

void SceneOrganiserDock::onOpenProjectorWindowClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();
    obs_source_t *source = Canvas::FindScene(m_canvasType, sceneName.toUtf8().constData());
    if (!source) return;

    obs_frontend_open_projector("Scene", -1, nullptr, obs_source_get_name(source));
    obs_source_release(source);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Projector Window",
        QString("Opened projector window for scene: %1").arg(sceneName).toUtf8().constData());
}

void SceneOrganiserDock::onSceneMoveUpClicked()
{
    const QString layoutBefore = captureLayout();

    if (!m_currentContextItem) return;

    QStandardItem *parent = m_currentContextItem->parent();
    if (!parent) parent = m_model->invisibleRootItem();

    int currentRow = m_currentContextItem->row();
    if (currentRow > 0) {
        // Take the entire row (this includes all columns)
        QList<QStandardItem*> items = parent->takeRow(currentRow);
        if (!items.isEmpty()) {
            parent->insertRow(currentRow - 1, items);
            QModelIndex sourceIndex = items[0]->index();
            QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
            m_treeView->setCurrentIndex(proxyIndex);
            m_saveTimer->start();
        }
    }

    pushLayoutUndo(QString::fromUtf8(obs_module_text("SceneOrganiser.Undo.Move")), layoutBefore);
}

void SceneOrganiserDock::onSceneMoveDownClicked()
{
    const QString layoutBefore = captureLayout();

    if (!m_currentContextItem) return;

    QStandardItem *parent = m_currentContextItem->parent();
    if (!parent) parent = m_model->invisibleRootItem();

    int currentRow = m_currentContextItem->row();
    if (currentRow < parent->rowCount() - 1) {
        // Take the entire row (this includes all columns)
        QList<QStandardItem*> items = parent->takeRow(currentRow);
        if (!items.isEmpty()) {
            parent->insertRow(currentRow + 1, items);
            QModelIndex sourceIndex = items[0]->index();
            QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
            m_treeView->setCurrentIndex(proxyIndex);
            m_saveTimer->start();
        }
    }

    pushLayoutUndo(QString::fromUtf8(obs_module_text("SceneOrganiser.Undo.Move")), layoutBefore);
}

void SceneOrganiserDock::onSceneMoveToTopClicked()
{
    const QString layoutBefore = captureLayout();

    if (!m_currentContextItem) return;

    QStandardItem *parent = m_currentContextItem->parent();
    if (!parent) parent = m_model->invisibleRootItem();

    int currentRow = m_currentContextItem->row();
    if (currentRow > 0) {
        // Take the entire row (this includes all columns)
        QList<QStandardItem*> items = parent->takeRow(currentRow);
        if (!items.isEmpty()) {
            parent->insertRow(0, items);
            QModelIndex sourceIndex = items[0]->index();
            QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
            m_treeView->setCurrentIndex(proxyIndex);
            m_saveTimer->start();
        }
    }

    pushLayoutUndo(QString::fromUtf8(obs_module_text("SceneOrganiser.Undo.Move")), layoutBefore);
}

void SceneOrganiserDock::onSceneMoveToBottomClicked()
{
    const QString layoutBefore = captureLayout();

    if (!m_currentContextItem) return;

    QStandardItem *parent = m_currentContextItem->parent();
    if (!parent) parent = m_model->invisibleRootItem();

    int currentRow = m_currentContextItem->row();
    int maxRow = parent->rowCount() - 1;
    if (currentRow < maxRow) {
        // Take the entire row (this includes all columns)
        QList<QStandardItem*> items = parent->takeRow(currentRow);
        if (!items.isEmpty()) {
            parent->insertRow(maxRow, items);
            QModelIndex sourceIndex = items[0]->index();
            QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
            m_treeView->setCurrentIndex(proxyIndex);
            m_saveTimer->start();
        }
    }

    pushLayoutUndo(QString::fromUtf8(obs_module_text("SceneOrganiser.Undo.Move")), layoutBefore);
}

void SceneOrganiserDock::populateTransitionOverrideMenu(obs_source_t *sceneSource)
{
    if (!m_sceneTransitionMenu) {
        return;
    }

    // Rebuilt from scratch each time. Transitions can be added or renamed while
    // the dock is open, and the tick has to follow whichever scene was clicked.
    m_sceneTransitionMenu->clear();

    if (!sceneSource) {
        m_sceneTransitionMenu->setEnabled(false);
        return;
    }
    m_sceneTransitionMenu->setEnabled(true);

    // OBS keeps the override on the scene's own private settings, and that is
    // where its transition code reads it back from. Using the same keys and the
    // same 300ms default means an override set here shows up in the OBS scene
    // list, and one set there shows up with a tick in here.
    obs_data_t *privateSettings = obs_source_get_private_settings(sceneSource);
    obs_data_set_default_int(privateSettings, "transition_duration", 300);
    const QString currentTransition = QString::fromUtf8(obs_data_get_string(privateSettings, "transition"));
    const int currentDuration = static_cast<int>(obs_data_get_int(privateSettings, "transition_duration"));
    obs_data_release(privateSettings);

    const QString sceneName = QString::fromUtf8(obs_source_get_name(sceneSource));
    QPointer<SceneOrganiserDock> self(this);

    // The scene is looked up again when an action fires rather than captured
    // here. The menu outlives this call, and the scene can be renamed or deleted
    // in between, so holding a source pointer would be holding a stale one.
    auto setOverride = [self, sceneName](const QString &transitionName) {
        if (!self) return;
        obs_source_t *scene = Canvas::FindScene(self->GetCanvasType(), sceneName.toUtf8().constData());
        if (!scene) return;
        obs_data_t *settings = obs_source_get_private_settings(scene);
        obs_data_set_string(settings, "transition", transitionName.toUtf8().constData());
        obs_data_release(settings);
        obs_source_release(scene);
    };

    // An empty transition name is how OBS records "no override".
    QAction *noneAction = m_sceneTransitionMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("None"), -1));
    noneAction->setCheckable(true);
    noneAction->setChecked(currentTransition.isEmpty());
    connect(noneAction, &QAction::triggered, this, [setOverride]() { setOverride(QString()); });

    struct obs_frontend_source_list transitions = {};
    obs_frontend_get_transitions(&transitions);
    for (size_t i = 0; i < transitions.sources.num; i++) {
        const char *name = obs_source_get_name(transitions.sources.array[i]);
        if (!name) continue;

        const QString transitionName = QString::fromUtf8(name);
        QAction *action = m_sceneTransitionMenu->addAction(transitionName);
        action->setCheckable(true);
        action->setChecked(transitionName == currentTransition);
        connect(action, &QAction::triggered, this, [setOverride, transitionName]() { setOverride(transitionName); });
    }
    obs_frontend_source_list_free(&transitions);

    // Duration sits in the menu as a spin box, the same as it does in OBS.
    QSpinBox *duration = new QSpinBox(m_sceneTransitionMenu);
    duration->setMinimum(50);
    duration->setMaximum(20000);
    duration->setSingleStep(50);
    duration->setSuffix(" ms");
    duration->setValue(currentDuration);
    connect(duration, &QSpinBox::valueChanged, this, [self, sceneName](int value) {
        if (!self) return;
        obs_source_t *scene = Canvas::FindScene(self->GetCanvasType(), sceneName.toUtf8().constData());
        if (!scene) return;
        obs_data_t *settings = obs_source_get_private_settings(scene);
        obs_data_set_int(settings, "transition_duration", value);
        obs_data_release(settings);
        obs_source_release(scene);
    });

    QWidgetAction *durationAction = new QWidgetAction(m_sceneTransitionMenu);
    durationAction->setDefaultWidget(duration);
    m_sceneTransitionMenu->addSeparator();
    m_sceneTransitionMenu->addAction(durationAction);
}

void SceneOrganiserDock::populateLinkedScenesMenu(obs_source_t *sceneSource)
{
    if (!m_sceneLinkedScenesMenu) {
        return;
    }

    // Rebuilt from scratch each time. Main scenes come and go while the dock is
    // open, and the ticks have to follow whichever vertical scene was clicked.
    m_sceneLinkedScenesMenu->clear();

    int canvasWidth = 0, canvasHeight = 0;
    if (!sceneSource || !Canvas::GetDimensions(m_canvasType, canvasWidth, canvasHeight)) {
        m_sceneLinkedScenesMenu->setEnabled(false);
        return;
    }
    m_sceneLinkedScenesMenu->setEnabled(true);

    const QString verticalSceneName = QString::fromUtf8(obs_source_get_name(sceneSource));

    // The link lives on the MAIN scene, not the vertical one: a "canvas" array
    // on its settings, one entry per canvas, keyed by the canvas dimensions.
    // Aitum reads exactly this when the main scene changes, so a link written
    // here works in its dock too, and one written there is ticked in here. The
    // dimensions are the key it uses, hence GetDimensions rather than a name.
    auto setLink = [canvasWidth, canvasHeight](obs_source_t *mainScene, const QString &verticalScene) {
        obs_data_t *settings = obs_source_get_settings(mainScene);
        obs_data_array_t *canvases = obs_data_get_array(settings, "canvas");

        obs_data_t *found = nullptr;
        const size_t count = canvases ? obs_data_array_count(canvases) : 0;
        for (size_t i = 0; i < count; i++) {
            obs_data_t *item = obs_data_array_item(canvases, i);
            if (!item) continue;
            if (obs_data_get_int(item, "width") == canvasWidth &&
                obs_data_get_int(item, "height") == canvasHeight) {
                found = item;
                // Clearing a link means dropping the entry entirely: an empty
                // scene name left behind would send Aitum looking for a scene
                // called "" every time that main scene goes live.
                if (verticalScene.isEmpty()) {
                    obs_data_array_erase(canvases, i);
                }
                break;
            }
            obs_data_release(item);
        }

        if (!verticalScene.isEmpty()) {
            if (!canvases) {
                canvases = obs_data_array_create();
                obs_data_set_array(settings, "canvas", canvases);
            }
            if (!found) {
                found = obs_data_create();
                obs_data_set_int(found, "width", canvasWidth);
                obs_data_set_int(found, "height", canvasHeight);
                obs_data_array_push_back(canvases, found);
            }
            obs_data_set_string(found, "scene", verticalScene.toUtf8().constData());
        }

        obs_data_release(found);
        obs_data_array_release(canvases);
        obs_data_release(settings);
    };

    struct obs_frontend_source_list mainScenes = {};
    obs_frontend_get_scenes(&mainScenes);
    for (size_t i = 0; i < mainScenes.sources.num; i++) {
        obs_source_t *mainScene = mainScenes.sources.array[i];
        const char *name = obs_source_get_name(mainScene);
        if (!name) continue;

        const QString mainSceneName = QString::fromUtf8(name);

        bool linkedToThis = false;
        obs_data_t *settings = obs_source_get_settings(mainScene);
        if (obs_data_array_t *canvases = obs_data_get_array(settings, "canvas")) {
            const size_t count = obs_data_array_count(canvases);
            for (size_t j = 0; j < count; j++) {
                obs_data_t *item = obs_data_array_item(canvases, j);
                if (!item) continue;
                if (obs_data_get_int(item, "width") == canvasWidth &&
                    obs_data_get_int(item, "height") == canvasHeight) {
                    linkedToThis = QString::fromUtf8(obs_data_get_string(item, "scene")) == verticalSceneName;
                }
                obs_data_release(item);
            }
            obs_data_array_release(canvases);
        }
        obs_data_release(settings);

        QAction *action = m_sceneLinkedScenesMenu->addAction(mainSceneName);
        action->setCheckable(true);
        action->setChecked(linkedToThis);

        // The main scene is looked up again when the action fires rather than
        // captured here: the menu outlives this call, and a scene can be renamed
        // or removed in between, so holding a source pointer would be stale.
        connect(action, &QAction::triggered, this,
                [setLink, mainSceneName, verticalSceneName](bool checked) {
            obs_source_t *scene = obs_get_source_by_name(mainSceneName.toUtf8().constData());
            if (!scene) return;
            setLink(scene, checked ? verticalSceneName : QString());
            obs_source_release(scene);
        });
    }
    obs_frontend_source_list_free(&mainScenes);
}

void SceneOrganiserDock::populateProjectorMenu()
{
    if (!m_sceneProjectorMenu) return;

    // Clear existing monitor actions (but keep the window projector action)
    QList<QAction*> actions = m_sceneProjectorMenu->actions();
    for (QAction *action : actions) {
        if (action->property("monitor").isValid()) {
            m_sceneProjectorMenu->removeAction(action);
            action->deleteLater();
        }
    }

    // Add monitor actions at the beginning
    QList<QScreen*> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        QScreen *screen = screens[i];
        QString monitorText;

        if (screens.size() > 1) {
            // Multiple monitors - show monitor number and name
            if (!screen->name().isEmpty()) {
                monitorText = QString(obs_module_text("SceneOrganiser.Projector.MonitorNamed"))
                                      .arg(i + 1)
                                      .arg(screen->name());
            } else {
                monitorText = QString(obs_module_text("SceneOrganiser.Projector.Monitor")).arg(i + 1);
            }
        } else {
            // Single monitor - just show "Fullscreen"
            monitorText = obs_module_text("SceneOrganiser.Projector.Fullscreen");
        }

        QAction *monitorAction = new QAction(monitorText, m_sceneProjectorMenu);
        monitorAction->setProperty("monitor", i);
        connect(monitorAction, &QAction::triggered, this, &SceneOrganiserDock::onOpenProjectorOnMonitorClicked);

        // Insert before the separator (which should be before "Windowed")
        QList<QAction*> currentActions = m_sceneProjectorMenu->actions();
        if (!currentActions.isEmpty() && currentActions.first()->isSeparator()) {
            m_sceneProjectorMenu->insertAction(currentActions.first(), monitorAction);
        } else {
            // If no separator found, add at the beginning
            if (!currentActions.isEmpty()) {
                m_sceneProjectorMenu->insertAction(currentActions.first(), monitorAction);
            } else {
                m_sceneProjectorMenu->addAction(monitorAction);
            }
        }
    }
}

//==============================================================================
// SceneTreeModel Implementation
//==============================================================================

SceneTreeModel::SceneTreeModel(CanvasType canvasType, QObject *parent)
    : QStandardItemModel(parent)
    , m_canvasType(canvasType)
{
    setupRootItem();
    // Don't call refreshFromObs() here - let the dock load configuration first
}

SceneTreeModel::~SceneTreeModel()
{
    // Clean up all weak source references
    cleanupSceneTree();
}

void SceneTreeModel::setupRootItem()
{
    setHorizontalHeaderLabels(QStringList() << "Scenes");
}

Qt::DropActions SceneTreeModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

Qt::ItemFlags SceneTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

    // Allow editing for scenes and folders
    QStandardItem *item = itemFromIndex(index);
    if (item && (item->type() == SceneTreeItem::UserType + 2 || item->type() == SceneFolderItem::UserType + 1)) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

bool SceneTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid())
        return false;

    QStandardItem *item = itemFromIndex(index);
    if (!item)
        return false;

    QString newName = value.toString().trimmed();
    QString oldName = item->text();

    // Validate the new name
    if (newName.isEmpty() || newName == oldName)
        return false;

    if (item->type() == SceneTreeItem::UserType + 2) {
        // Scene item - rename in OBS
        obs_source_t *source = Canvas::FindScene(m_canvasType, oldName.toUtf8().constData());
        if (source) {
            obs_source_set_name(source, newName.toUtf8().constData());
            obs_source_release(source);

            // Update the item text
            item->setText(newName);

            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Inline Rename",
                QString("Renamed scene from '%1' to '%2'").arg(oldName, newName).toUtf8().constData());

            emit modelChanged();

            // Immediately save after rename to ensure changes persist
            saveSceneTree();

            return true;
        }
    } else if (item->type() == SceneFolderItem::UserType + 1) {
        // Folder item - simple rename
        item->setText(newName);

        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Inline Rename",
            QString("Renamed folder from '%1' to '%2'").arg(oldName, newName).toUtf8().constData());

        emit modelChanged();

        // Immediately save after rename to ensure changes persist
        saveSceneTree();

        return true;
    }

    return false;
}

QStringList SceneTreeModel::mimeTypes() const
{
    return QStringList() << "application/x-streamup-sceneorganiser";
}

QMimeData *SceneTreeModel::mimeData(const QModelIndexList &indexes) const
{
    if (indexes.isEmpty())
        return nullptr;

    QMimeData *mimeData = new QMimeData();

    // Collect the items being dragged, one per row, discarding two things that
    // a multi-item selection can hand us and a single one never could:
    //
    //  - duplicates, since selectedIndexes() reports every column;
    //  - any item that sits inside a folder which is ALSO being dragged. The
    //    folder move re-creates its whole subtree and destroys the originals,
    //    so a separate entry for the child would be a dangling pointer by the
    //    time the drop loop reached it.
    QList<QStandardItem *> items;
    for (const QModelIndex &index : indexes) {
        if (index.column() != 0 || !index.isValid()) {
            continue;
        }
        if (QStandardItem *item = itemFromIndex(index)) {
            if (!items.contains(item)) {
                items.append(item);
            }
        }
    }

    QList<QStandardItem *> topLevel;
    for (QStandardItem *item : items) {
        bool insideDraggedFolder = false;
        for (QStandardItem *ancestor = item->parent(); ancestor; ancestor = ancestor->parent()) {
            if (items.contains(ancestor)) {
                insideDraggedFolder = true;
                break;
            }
        }
        if (!insideDraggedFolder) {
            topLevel.append(item);
        }
    }

    if (topLevel.isEmpty()) {
        delete mimeData;
        return nullptr;
    }

    // Use binary data format like DigitOtter plugin
    QByteArray mimeDataBytes;
    const int numIndexes = topLevel.size();

    mimeDataBytes.append(reinterpret_cast<const char*>(&numIndexes), sizeof(int));

    for (QStandardItem *item : topLevel) {
        // Store item pointer directly
        mimeDataBytes.append(reinterpret_cast<const char*>(&item), sizeof(QStandardItem*));
    }

    mimeData->setData("application/x-streamup-sceneorganiser", mimeDataBytes);
    return mimeData;
}

bool SceneTreeModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                  int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(action)
    Q_UNUSED(column)

    if (!data->hasFormat("application/x-streamup-sceneorganiser"))
        return false;

    // Get parent item - use root if none specified
    QStandardItem *parentItem = itemFromIndex(parent);
    if (!parentItem) {
        parentItem = invisibleRootItem();
    }

    // Prevent dropping scenes into scenes (DigitOtter approach)
    if (parentItem->type() == SceneTreeItem::UserType + 2) {
        // Dropping on a scene - place it beside the scene instead
        QStandardItem *sceneParent = parentItem->parent();
        if (!sceneParent) {
            sceneParent = invisibleRootItem();
        }
        row = parentItem->row() + 1;
        parentItem = sceneParent;
    }

    // Ensure valid row
    if (row < 0) {
        row = parentItem->rowCount();
    }

    QByteArray mimeDataBytes = data->data("application/x-streamup-sceneorganiser");
    if (mimeDataBytes.size() < static_cast<int>(sizeof(int))) {
        return false;
    }

    const char *dat = mimeDataBytes.constData();
    const int numIndexes = *reinterpret_cast<const int*>(dat);
    dat += sizeof(int);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "DragDrop",
        QString("Dropping %1 items at row %2 into '%3'")
        .arg(numIndexes).arg(row)
        .arg(parentItem == invisibleRootItem() ? "root" : parentItem->text()).toUtf8().constData());

    for (int i = 0; i < numIndexes; ++i) {
        if (dat + sizeof(QStandardItem*) > mimeDataBytes.constData() + mimeDataBytes.size()) {
            break; // Safety check
        }

        QStandardItem *originalItem = *reinterpret_cast<QStandardItem* const*>(dat);
        dat += sizeof(QStandardItem*);

        if (!originalItem) {
            continue;
        }

        if (originalItem->type() == SceneTreeItem::UserType + 2) {
            // Move scene item - create new item and update tracking
            SceneTreeItem *sceneItem = static_cast<SceneTreeItem*>(originalItem);
            obs_weak_source_t *weak_source = sceneItem->getWeakSource();

            // Add reference for the new item (since the old item will release its reference when destroyed)
            obs_weak_source_addref(weak_source);
            QStandardItem *newItem = new SceneTreeItem(originalItem->text(), weak_source);

            // Copy custom color from original item
            QVariant customColor = originalItem->data(Qt::UserRole + 1);
            if (customColor.isValid()) {
                newItem->setData(customColor, Qt::UserRole + 1);
                // Apply the color visually
                QColor color = customColor.value<QColor>();
                if (color.isValid()) {
                    // Get the dock instance to use its color helper methods
                    for (auto dock : SceneOrganiserDock::s_dockInstances) {
                        if (dock && dock->m_model == this) {
                            dock->applyCustomColorToItem(newItem, color);
                            break;
                        }
                    }
                }
            }

            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "DragDrop",
                QString("Inserting scene '%1' at row %2 (parent has %3 children)")
                .arg(newItem->text()).arg(row).arg(parentItem->rowCount()).toUtf8().constData());

            parentItem->insertRow(row, newItem);

            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "DragDrop",
                QString("After insert: scene '%1' is now at row %2 (parent has %3 children)")
                .arg(newItem->text()).arg(newItem->row()).arg(parentItem->rowCount()).toUtf8().constData());

            // Update tracking map with new item
            m_scenesInTree[weak_source] = newItem;

            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "DragDrop",
                QString("Updated tracking for scene '%1' - new item at %2")
                .arg(newItem->text()).arg(reinterpret_cast<uintptr_t>(newItem)).toUtf8().constData());

            // Check if this is the active scene and mark it for immediate update
            obs_source_t *current_scene = Canvas::GetCurrentScene(m_canvasType);
            if (current_scene) {
                QString current_scene_name = QString::fromUtf8(obs_source_get_name(current_scene));
                if (newItem->text() == current_scene_name) {
                    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "DragDrop",
                        QString("Moved scene '%1' is the active scene - forcing immediate repaint")
                        .arg(newItem->text()).toUtf8().constData());

                    // Force immediate repaint of this specific item
                    QModelIndex newIndex = indexFromItem(newItem);
                    if (newIndex.isValid()) {
                        emit dataChanged(newIndex, newIndex);
                    }
                }
                obs_source_release(current_scene);
            }
        } else if (originalItem->type() == SceneFolderItem::UserType + 1) {
            // Move folder item
            moveSceneFolder(originalItem, row, parentItem);
        }

        ++row; // Increment for next item
    }

    // Before anything else looks at the tree, and crucially before the save
    // below, take out any original the move left behind.
    removeUntrackedSceneDuplicates();

    emit modelChanged();

    // Immediately save after drag & drop to ensure changes persist
    saveSceneTree();

    return true;
}

// A scene is in the tree exactly once, and m_scenesInTree says which item that
// is. Anything else claiming the same scene is a leftover.
//
// Copies get left behind because a drop INSERTS the moved items and then relies
// on the view to remove the originals. That holds for a single row, but not for
// a folder dragged with its children also selected: the folder move already
// rebuilds the subtree, so the removal that follows is working from indexes that
// have moved underneath it, and an original survives. The survivor is not in the
// tracking map, so nothing afterwards notices it, and it gets saved.
//
// Rather than argue with the view about who removes what, the tree is measured
// against the map: an untracked scene row cannot be legitimate.
int SceneTreeModel::removeUntrackedSceneDuplicates(QStandardItem *parent)
{
    if (!parent) {
        parent = invisibleRootItem();
    }

    QSet<QStandardItem *> tracked;
    for (const auto &entry : m_scenesInTree) {
        if (entry.second) {
            tracked.insert(entry.second);
        }
    }

    int removed = 0;
    std::function<void(QStandardItem *)> sweep = [&](QStandardItem *node) {
        for (int i = node->rowCount() - 1; i >= 0; --i) {
            QStandardItem *child = node->child(i);
            if (!child) {
                continue;
            }

            if (child->rowCount() > 0) {
                sweep(child);
            }

            if (child->type() == SceneTreeItem::UserType + 2 && !tracked.contains(child)) {
                StreamUP::DebugLogger::LogInfo("SceneOrganiser",
                    QString("Removed a duplicate row for scene '%1'").arg(child->text()).toUtf8().constData());
                node->removeRow(i);
                ++removed;
            }
        }
    };
    sweep(parent);

    return removed;
}

void SceneTreeModel::updateTree(const QModelIndex &selectedIndex)
{
    // Get the scenes on this dock's canvas. obs_frontend_get_scenes only ever
    // returns main-canvas scenes, so a vertical dock has to go through the
    // canvas API to see anything at all.
    std::vector<obs_source_t *> scene_list = Canvas::GetScenes(m_canvasType);

    source_map_t new_scene_tree;

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "UpdateTree",
        QString("Processing %1 scenes from OBS").arg(scene_list.size()).toUtf8().constData());

    for (size_t i = 0; i < scene_list.size(); i++) {
        obs_source_t *source = scene_list[i];
        if (!source) continue;

        obs_scene_t *scene = obs_scene_from_source(source);
        if (!scene) continue;

        if (!isManagedScene(source)) continue;

        obs_weak_source_t *weak = obs_source_get_weak_source(source);
        source_map_t::iterator scene_it;

        // Check if scene already in tree by comparing actual sources, not weak pointers
        // Different weak references can point to the same source
        scene_it = m_scenesInTree.end();
        for (auto it = m_scenesInTree.begin(); it != m_scenesInTree.end(); ++it) {
            obs_source_t *existing_source = obs_weak_source_get_source(it->first);
            bool same_source = (existing_source == source);
            obs_source_release(existing_source);

            if (same_source) {
                scene_it = it;
                break;
            }
        }

        if (scene_it != m_scenesInTree.end()) {
            // Move existing scene to new tree
            auto new_scene_it = new_scene_tree.emplace(scene_it->first, scene_it->second).first;
            m_scenesInTree.erase(scene_it);
            scene_it = new_scene_it;
            obs_weak_source_release(weak); // Release the duplicate weak reference
        } else {
            // Add new scene
            scene_it = new_scene_tree.emplace(weak, nullptr).first;
        }

        if (!scene_it->second) {
            // Scene not yet in tree, add it
            const char *name = obs_source_get_name(source);
            if (name) {
                QString sceneName = QString::fromUtf8(name);
                QStandardItem *newSceneItem = new SceneTreeItem(sceneName, scene_it->first);

                // Determine where to add the scene
                QStandardItem *parent = invisibleRootItem();
                QStandardItem *selected = itemFromIndex(selectedIndex);
                if (selected) {
                    if (selected->type() == SceneFolderItem::UserType + 1) {
                        parent = selected;
                    } else if (selected->parent()) {
                        parent = selected->parent();
                    }
                }

                parent->appendRow(newSceneItem);
                scene_it->second = newSceneItem;

                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "UpdateTree",
                    QString("Added new scene: %1").arg(sceneName).toUtf8().constData());
            }
        } else {
            // Update existing scene name
            const char *name = obs_source_get_name(source);
            if (name) {
                scene_it->second->setText(QString::fromUtf8(name));
            }
        }
    }

    // Remove scenes that are no longer in OBS
    for (auto &old_scene : m_scenesInTree) {
        if (old_scene.second) {
            QStandardItem *parent = old_scene.second->parent();
            if (!parent) parent = invisibleRootItem();

            QString sceneName = old_scene.second->text();
            int row = old_scene.second->row();
            parent->removeRow(row);

            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "UpdateTree",
                QString("Removed scene: %1").arg(sceneName).toUtf8().constData());
        }
        // Release the weak source reference
        obs_weak_source_release(old_scene.first);
    }

    // Update our scene tree
    m_scenesInTree = std::move(new_scene_tree);

    // With the map settled, anything in the tree it does not know about is a
    // leftover copy from a move. Cheap, and it heals a tree that already has
    // some rather than only preventing new ones.
    removeUntrackedSceneDuplicates();

    Canvas::ReleaseScenes(scene_list);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "UpdateTree",
        QString("Tree updated - %1 scenes tracked").arg(m_scenesInTree.size()).toUtf8().constData());

    emit modelChanged();
}

bool SceneTreeModel::isValidSceneForCanvas(obs_scene_t *scene)
{
    if (!scene) return false;

    obs_source_t *source = obs_scene_get_source(scene);
    if (!source) return false;

    const char *scene_name = obs_source_get_name(source);
    if (!scene_name) return false;

    // Only show scenes that live on this dock's canvas, so vertical scenes stay
    // out of the Normal dock and main scenes stay out of the Vertical one.
    return Canvas::SceneBelongsTo(m_canvasType, source);
}


QStandardItem *SceneTreeModel::findSceneItem(obs_weak_source_t *weak_source)
{
    auto it = m_scenesInTree.find(weak_source);
    return (it != m_scenesInTree.end()) ? it->second : nullptr;
}

QStandardItem *SceneTreeModel::findFolderItem(const QString &folderName)
{
    return StreamUP::UIHelpers::FindItemRecursive(invisibleRootItem(), folderName, SceneFolderItem::UserType + 1);
}

QStandardItem *SceneTreeModel::createFolderItem(const QString &folderName)
{
    if (folderName.isEmpty() || folderName.trimmed().isEmpty()) {
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Warning",
            "Attempted to create folder item with empty name");
        return nullptr;
    }
    return new SceneFolderItem(folderName);
}

QStandardItem *SceneTreeModel::createSceneItem(const QString &sceneName, obs_weak_source_t *weak_source)
{
    if (sceneName.isEmpty() || sceneName.trimmed().isEmpty()) {
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Warning",
            "Attempted to create scene item with empty name");
        return nullptr;
    }
    return new SceneTreeItem(sceneName, weak_source);
}

void SceneTreeModel::moveSceneToFolder(obs_weak_source_t *weak_source, QStandardItem *folderItem)
{
    QStandardItem *sceneItem = findSceneItem(weak_source);
    if (sceneItem && folderItem) {
        QStandardItem *oldParent = sceneItem->parent();
        if (!oldParent) oldParent = invisibleRootItem();

        QStandardItem *movedItem = oldParent->takeChild(sceneItem->row());
        if (movedItem) {
            folderItem->appendRow(movedItem);
            emit modelChanged();
        }
    }
}

// Depth-first lookup by display name, used when an item has to be re-found
// after a modeless dialog rather than held across it as a raw pointer.
QStandardItem *SceneTreeModel::findItemByName(const QString &name, int itemType, QStandardItem *parent)
{
    if (!parent) {
        parent = invisibleRootItem();
    }

    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *item = parent->child(i);
        if (!item) continue;

        if (item->type() == itemType && item->text() == name) {
            return item;
        }
        if (QStandardItem *found = findItemByName(name, itemType, item)) {
            return found;
        }
    }

    return nullptr;
}

QStandardItem *SceneTreeModel::findSceneItemByName(const QString &name)
{
    return findItemByName(name, SceneTreeItem::UserType + 2, nullptr);
}

QStandardItem *SceneTreeModel::findFolderItemByName(const QString &name)
{
    return findItemByName(name, SceneFolderItem::UserType + 1, nullptr);
}

void SceneTreeModel::cleanupEmptyItems()
{
    cleanupEmptyItemsRecursive(invisibleRootItem());
}

void SceneTreeModel::cleanupEmptyItemsRecursive(QStandardItem *parent)
{
    if (!parent) return;

    // Iterate backwards to avoid index issues when removing items
    for (int i = parent->rowCount() - 1; i >= 0; --i) {
        QStandardItem *child = parent->child(i);
        if (!child) {
            // Remove null items
            parent->removeRow(i);
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cleanup", "Removed null item");
            continue;
        }

        // Check if item has empty or whitespace-only text
        QString itemText = child->text();
        if (itemText.isEmpty() || itemText.trimmed().isEmpty()) {
            parent->removeRow(i);
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cleanup",
                QString("Removed empty item at row %1").arg(i).toUtf8().constData());
            continue;
        }

        // For scene items, they are managed by updateTree now
        if (child->type() == SceneTreeItem::UserType + 2) {
            // Scene validation is handled in updateTree - skip here
            continue;
        }

        // Recursively clean up folders
        if (child->type() == SceneFolderItem::UserType + 1) {
            cleanupEmptyItemsRecursive(child);

            // After cleaning up children, check if folder is now empty and remove it if so
            if (child->rowCount() == 0) {
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cleanup",
                    QString("Folder '%1' is empty after cleanup - considering for removal").arg(itemText).toUtf8().constData());
                // Note: We don't auto-remove empty folders as user might want to keep them
            }
        } else if (child->type() != SceneTreeItem::UserType + 2) {
            // Handle items with unknown/invalid types
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cleanup",
                QString("Found item '%1' with unknown type %2, removing").arg(itemText).arg(child->type()).toUtf8().constData());
            parent->removeRow(i);
        }
    }
}

bool SceneTreeModel::isChildOf(QStandardItem *potentialChild, QStandardItem *potentialParent)
{
    if (!potentialChild || !potentialParent) {
        return false;
    }

    // Check if potentialParent is an ancestor of potentialChild
    QStandardItem *current = potentialChild->parent();
    while (current) {
        if (current == potentialParent) {
            return true;
        }
        current = current->parent();
    }
    return false;
}

void SceneTreeModel::moveSceneItem(QStandardItem *item, int row, QStandardItem *parentItem)
{
    // This function is now only used for internal moves, not drag & drop
    // Drag & drop is handled directly in dropMimeData
    if (!item || item->type() != SceneTreeItem::UserType + 2) {
        return;
    }

    SceneTreeItem *sceneItem = static_cast<SceneTreeItem*>(item);
    QString sceneName = item->text();
    obs_weak_source_t *weak = sceneItem->getWeakSource();

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Move",
        QString("Moving scene '%1'").arg(sceneName).toUtf8().constData());

    // For internal moves, we can safely move the item directly
    QStandardItem *oldParent = item->parent();
    if (!oldParent) oldParent = invisibleRootItem();

    QStandardItem *takenItem = oldParent->takeChild(item->row());
    if (takenItem) {
        parentItem->insertRow(row, takenItem);
        m_scenesInTree[weak] = takenItem;

        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Move",
            QString("Moved scene '%1' to row %2").arg(sceneName).arg(row).toUtf8().constData());
    }
}

void SceneTreeModel::moveSceneFolder(QStandardItem *item, int row, QStandardItem *parentItem)
{
    if (!item || item->type() != SceneFolderItem::UserType + 1) {
        return;
    }

    QString folderName = item->text();
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Move",
        QString("Moving folder '%1'").arg(folderName).toUtf8().constData());

    // Check if we're moving to the same parent - if so, keep original name
    QString uniqueName;
    QStandardItem *originalParent = item->parent();
    if (!originalParent) originalParent = invisibleRootItem();

    if (originalParent == parentItem) {
        // Moving within same parent - keep original name
        uniqueName = folderName;
    } else {
        // Moving to different parent - check for conflicts
        uniqueName = createUniqueFolderName(folderName, parentItem);
    }

    // Create new folder item
    QStandardItem *newFolder = createFolderItem(uniqueName);
    if (!newFolder) {
        return;
    }

    // Copy custom color from original folder
    QVariant customColor = item->data(Qt::UserRole + 1);
    if (customColor.isValid()) {
        newFolder->setData(customColor, Qt::UserRole + 1);
        // Apply the color visually
        QColor color = customColor.value<QColor>();
        if (color.isValid()) {
            // Get the dock instance to use its color helper methods
            for (auto dock : SceneOrganiserDock::s_dockInstances) {
                if (dock && dock->m_model == this) {
                    dock->applyCustomColorToItem(newFolder, color);
                    break;
                }
            }
        }
    }

    parentItem->insertRow(row, newFolder);

    // Recursively move child items
    for (int childRow = 0; childRow < item->rowCount(); ++childRow) {
        QStandardItem *childItem = item->child(childRow);
        if (!childItem) continue;

        if (childItem->type() == SceneFolderItem::UserType + 1) {
            moveSceneFolder(childItem, childRow, newFolder);
        } else if (childItem->type() == SceneTreeItem::UserType + 2) {
            moveSceneItem(childItem, childRow, newFolder);
        }
    }

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Move",
        QString("Moved folder '%1' to row %2 with %3 children")
        .arg(uniqueName).arg(row).arg(item->rowCount()).toUtf8().constData());
}

QString SceneTreeModel::createUniqueFolderName(const QString &baseName, QStandardItem *parentItem)
{
    QString uniqueName = baseName;
    int suffix = 1;

    // Check if name already exists in parent
    while (true) {
        bool nameExists = false;
        for (int i = 0; i < parentItem->rowCount(); ++i) {
            QStandardItem *child = parentItem->child(i);
            if (child && child->type() == SceneFolderItem::UserType + 1 &&
                child->text() == uniqueName) {
                nameExists = true;
                break;
            }
        }

        if (!nameExists) {
            break;
        }

        uniqueName = QString("%1 (%2)").arg(baseName).arg(suffix++);
    }

    return uniqueName;
}

bool SceneTreeModel::isManagedScene(obs_source_t *source)
{
    if (!source) return false;

    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) return false;

    return isValidSceneForCanvas(scene);
}

void SceneTreeModel::cleanupSceneTree()
{
    // Clear the model first - this will trigger SceneTreeItem destructors
    // which will release their own weak source references
    clear();
    setupRootItem();

    // Clear the tracking map (weak sources already released by item destructors)
    m_scenesInTree.clear();
}

void SceneTreeModel::saveSceneTree()
{
    char *scene_collection = obs_frontend_get_current_scene_collection();
    if (!scene_collection) return;

    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) {
        bfree(scene_collection);
        return;
    }

    QString configDir = QString::fromUtf8(configPath);
    QString configFile = configDir + "/" + sceneTreeFileName(m_canvasType);
    QString sceneCollectionName = QString::fromUtf8(scene_collection);

    bfree(configPath);

    // Load existing data first to preserve other scene collections
    obs_data_t *root_data = obs_data_create_from_json_file(configFile.toUtf8().constData());
    if (!root_data) {
        // File doesn't exist yet, create new data
        root_data = obs_data_create();
    }

    // Update only the current scene collection's data
    obs_data_array_t *folder_array = createFolderArray(*invisibleRootItem());
    obs_data_set_array(root_data, scene_collection, folder_array);
    obs_data_array_release(folder_array);

    // Save to file (now preserves other collections)
    obs_data_save_json(root_data, configFile.toUtf8().constData());
    obs_data_release(root_data);

    bfree(scene_collection);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Save",
        QString("Saved scene tree for collection '%1' to: %2").arg(sceneCollectionName, configFile).toUtf8().constData());
}

void SceneTreeModel::loadSceneTree()
{
    char *scene_collection = obs_frontend_get_current_scene_collection();
    if (!scene_collection) return;

    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) {
        bfree(scene_collection);
        return;
    }

    QString configDir = QString::fromUtf8(configPath);
    QString configFile = configDir + "/" + sceneTreeFileName(m_canvasType);
    QString sceneCollectionName = QString::fromUtf8(scene_collection);

    bfree(configPath);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Load",
        QString("Loading scene tree for collection: '%1'").arg(sceneCollectionName).toUtf8().constData());

    // Clean up previous tree
    cleanupSceneTree();

    // Load from file
    obs_data_t *root_data = obs_data_create_from_json_file(configFile.toUtf8().constData());
    if (root_data) {
        obs_data_array_t *folder_array = obs_data_get_array(root_data, scene_collection);
        if (folder_array) {
            size_t itemCount = obs_data_array_count(folder_array);
            loadFolderArray(folder_array, *invisibleRootItem());
            obs_data_array_release(folder_array);

            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Load",
                QString("Loaded %1 items from scene tree for collection '%2' from: %3")
                .arg(itemCount).arg(sceneCollectionName, configFile).toUtf8().constData());
        } else {
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Load",
                QString("No saved scene tree found for collection '%1' (key not in JSON)")
                .arg(sceneCollectionName).toUtf8().constData());
        }
        obs_data_release(root_data);
    } else {
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Load",
            QString("Config file does not exist or is invalid: %1").arg(configFile).toUtf8().constData());
    }

    bfree(scene_collection);
}

bool SceneTreeModel::migrateFromOriginalPlugin(const QString &originalConfigPath)
{
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Migration",
        QString("Starting migration from: %1").arg(originalConfigPath).toUtf8().constData());

    // Load the original plugin's config file
    obs_data_t *original_data = obs_data_create_from_json_file(originalConfigPath.toUtf8().constData());
    if (!original_data) {
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Migration",
            "Failed to load original config file");
        return false;
    }

    // Get our config path
    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) {
        obs_data_release(original_data);
        return false;
    }

    QString configDir = QString::fromUtf8(configPath);
    QString configFile = configDir + "/" + sceneTreeFileName(m_canvasType);
    bfree(configPath);

    // Load existing data (if any) to preserve what we have
    obs_data_t *root_data = obs_data_create_from_json_file(configFile.toUtf8().constData());
    if (!root_data) {
        root_data = obs_data_create();
    }

    int migratedCollections = 0;

    // Iterate through all scene collections in the original file
    obs_data_item_t *item = obs_data_first(original_data);
    while (item) {
        const char *collection_name = obs_data_item_get_name(item);
        obs_data_type type = obs_data_item_gettype(item);

        if (type == OBS_DATA_ARRAY) {
            obs_data_array_t *original_array = obs_data_item_get_array(item);
            if (original_array) {
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Migration",
                    QString("Migrating collection: %1").arg(collection_name).toUtf8().constData());

                // Check if we already have data for this collection
                obs_data_array_t *existing_array = obs_data_get_array(root_data, collection_name);
                if (existing_array) {
                    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Migration",
                        QString("Collection '%1' already exists, skipping").arg(collection_name).toUtf8().constData());
                    obs_data_array_release(existing_array);
                } else {
                    // Convert and migrate this collection
                    obs_data_set_array(root_data, collection_name, original_array);
                    migratedCollections++;
                }

                obs_data_array_release(original_array);
            }
        }

        obs_data_item_next(&item);
    }

    // Save the migrated data
    obs_data_save_json(root_data, configFile.toUtf8().constData());
    obs_data_release(root_data);
    obs_data_release(original_data);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Migration",
        QString("Migration complete. Migrated %1 scene collections.").arg(migratedCollections).toUtf8().constData());

    return migratedCollections > 0;
}

bool SceneTreeModel::checkMigrationAvailable(const QString &sceneCollectionName, QString &outConfigPath)
{
    // Get the streamup plugin config path to navigate to obs_scene_tree_view
    char *streamup_config_path = obs_module_get_config_path(obs_current_module(), "");
    if (!streamup_config_path) {
        return false;
    }

    QDir streamupConfigDir(QString::fromUtf8(streamup_config_path));
    streamupConfigDir.cdUp(); // Go to plugin_config directory

    QString sceneTreeConfigPath = streamupConfigDir.filePath("obs_scene_tree_view/scene_tree.json");
    bfree(streamup_config_path);

    // Check if the file exists
    if (!QFile::exists(sceneTreeConfigPath)) {
        return false;
    }

    // Load the file and check if this scene collection exists in it
    obs_data_t *original_data = obs_data_create_from_json_file(sceneTreeConfigPath.toUtf8().constData());
    if (!original_data) {
        return false;
    }

    // Check if this scene collection has data
    obs_data_array_t *collection_array = obs_data_get_array(original_data, sceneCollectionName.toUtf8().constData());
    bool hasData = (collection_array != nullptr && obs_data_array_count(collection_array) > 0);

    if (collection_array) {
        obs_data_array_release(collection_array);
    }
    obs_data_release(original_data);

    if (hasData) {
        outConfigPath = sceneTreeConfigPath;
    }

    return hasData;
}

// Convert obs_scene_tree_view format to StreamUP format
// obs_scene_tree_view: {"name": "X", "folder": [...], "is_expanded": bool} for folders, {"name": "X"} for scenes
// StreamUP: {"name": "X", "type": "folder", "children": [...]} for folders, {"name": "X", "type": "scene"} for scenes
obs_data_array_t* SceneTreeModel::convertSceneTreeViewFormat(obs_data_array_t *original_array)
{
    if (!original_array) return nullptr;

    obs_data_array_t *converted_array = obs_data_array_create();
    size_t count = obs_data_array_count(original_array);

    for (size_t i = 0; i < count; i++) {
        obs_data_t *original_item = obs_data_array_item(original_array, i);
        if (!original_item) continue;

        obs_data_t *converted_item = obs_data_create();
        const char *name = obs_data_get_string(original_item, "name");
        obs_data_set_string(converted_item, "name", name);

        // Check if this is a folder (has "folder" array) or a scene
        obs_data_array_t *folder_contents = obs_data_get_array(original_item, "folder");
        if (folder_contents) {
            // This is a folder
            obs_data_set_string(converted_item, "type", "folder");
            bool is_expanded = obs_data_get_bool(original_item, "is_expanded");
            obs_data_set_bool(converted_item, "expanded", is_expanded);

            // Recursively convert folder contents
            obs_data_array_t *converted_children = convertSceneTreeViewFormat(folder_contents);
            obs_data_set_array(converted_item, "children", converted_children);
            obs_data_array_release(converted_children);
            obs_data_array_release(folder_contents);
        } else {
            // This is a scene
            obs_data_set_string(converted_item, "type", "scene");
        }

        obs_data_array_push_back(converted_array, converted_item);
        obs_data_release(converted_item);
        obs_data_release(original_item);
    }

    return converted_array;
}

bool SceneTreeModel::migrateCurrentCollection()
{
    char *scene_collection = obs_frontend_get_current_scene_collection();
    if (!scene_collection) {
        return false;
    }

    QString collectionName = QString::fromUtf8(scene_collection);
    bfree(scene_collection);

    QString configPath;
    if (!checkMigrationAvailable(collectionName, configPath)) {
        return false;
    }

    // Load the original plugin's config file
    obs_data_t *original_data = obs_data_create_from_json_file(configPath.toUtf8().constData());
    if (!original_data) {
        return false;
    }

    // Get the data for this specific collection
    obs_data_array_t *collection_array = obs_data_get_array(original_data, collectionName.toUtf8().constData());
    if (!collection_array) {
        obs_data_release(original_data);
        return false;
    }

    // Convert from obs_scene_tree_view format to StreamUP format
    obs_data_array_t *converted_array = convertSceneTreeViewFormat(collection_array);
    if (!converted_array) {
        obs_data_array_release(collection_array);
        obs_data_release(original_data);
        return false;
    }

    // Get our config path
    char *our_config_path = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!our_config_path) {
        obs_data_array_release(converted_array);
        obs_data_array_release(collection_array);
        obs_data_release(original_data);
        return false;
    }

    QString configDir = QString::fromUtf8(our_config_path);
    QString configFile = configDir + "/" + sceneTreeFileName(m_canvasType);
    bfree(our_config_path);

    // Load existing data to preserve other collections
    obs_data_t *root_data = obs_data_create_from_json_file(configFile.toUtf8().constData());
    if (!root_data) {
        root_data = obs_data_create();
    }

    // Set only this collection's converted data
    obs_data_set_array(root_data, collectionName.toUtf8().constData(), converted_array);

    // Save
    obs_data_save_json(root_data, configFile.toUtf8().constData());

    obs_data_array_release(converted_array);
    obs_data_array_release(collection_array);
    obs_data_release(root_data);
    obs_data_release(original_data);

    return true;
}

void SceneTreeModel::loadOriginalFolderArray(obs_data_array_t *folder_array, QStandardItem &parent)
{
    // This loads the original plugin's format which is already compatible
    // The format is the same, so we can use the existing loadFolderArray
    loadFolderArray(folder_array, parent);
}

void SceneTreeModel::removeSceneFromTracking(obs_weak_source_t *weak_source)
{
    auto it = m_scenesInTree.find(weak_source);
    if (it != m_scenesInTree.end()) {
        m_scenesInTree.erase(it);
        // Note: Don't release weak_source here - it will be released by the SceneTreeItem destructor

        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cleanup",
            "Removed scene from tracking (weak reference managed by item)");
    }
}

obs_data_array_t *SceneTreeModel::createFolderArray(QStandardItem &parent)
{
    obs_data_array_t *folder_array = obs_data_array_create();

    for (int i = 0; i < parent.rowCount(); ++i) {
        QStandardItem *child = parent.child(i);
        if (!child) continue;

        obs_data_t *item_data = obs_data_create();

        if (child->type() == SceneFolderItem::UserType + 1) {
            // This is a folder
            obs_data_set_string(item_data, "name", child->text().toUtf8().constData());
            obs_data_set_string(item_data, "type", "folder");
            // Written for the format's sake and never read back: folders always
            // open expanded. Restoring the real state would mean asking the view,
            // which the model has no handle on. Not a TODO until someone asks for
            // collapsed folders to stay collapsed.
            obs_data_set_bool(item_data, "expanded", true);

            // Save custom color if set
            QVariant colorData = child->data(Qt::UserRole + 1);
            if (colorData.isValid()) {
                QColor color = colorData.value<QColor>();
                // HexArgb: the preset colours carry alpha, which QColor::name() would drop.
                obs_data_set_string(item_data, "custom_color", color.name(QColor::HexArgb).toUtf8().constData());
            }

            // Save custom icon spec if set
            const QString folderIconSpec = child->data(CustomIconRole).toString();
            if (!folderIconSpec.isEmpty()) {
                obs_data_set_string(item_data, "custom_icon", folderIconSpec.toUtf8().constData());
            }

            const QColor folderIconColor = child->data(CustomIconColorRole).value<QColor>();
            if (folderIconColor.isValid()) {
                obs_data_set_string(item_data, "custom_icon_color",
                                    folderIconColor.name(QColor::HexArgb).toUtf8().constData());
            }

            // Recursively save children
            obs_data_array_t *children = createFolderArray(*child);
            obs_data_set_array(item_data, "children", children);
            obs_data_array_release(children);

        } else if (child->type() == SceneTreeItem::UserType + 2) {
            // This is a scene
            obs_data_set_string(item_data, "name", child->text().toUtf8().constData());
            obs_data_set_string(item_data, "type", "scene");

            // Save custom color if set
            QVariant colorData = child->data(Qt::UserRole + 1);
            if (colorData.isValid()) {
                QColor color = colorData.value<QColor>();
                // HexArgb: the preset colours carry alpha, which QColor::name() would drop.
                obs_data_set_string(item_data, "custom_color", color.name(QColor::HexArgb).toUtf8().constData());
            }

            // Save custom icon spec if set
            const QString sceneIconSpec = child->data(CustomIconRole).toString();
            if (!sceneIconSpec.isEmpty()) {
                obs_data_set_string(item_data, "custom_icon", sceneIconSpec.toUtf8().constData());
            }

            const QColor sceneIconColor = child->data(CustomIconColorRole).value<QColor>();
            if (sceneIconColor.isValid()) {
                obs_data_set_string(item_data, "custom_icon_color",
                                    sceneIconColor.name(QColor::HexArgb).toUtf8().constData());
            }
        }

        obs_data_array_push_back(folder_array, item_data);
        obs_data_release(item_data);
    }

    return folder_array;
}

void SceneTreeModel::loadFolderArray(obs_data_array_t *folder_array, QStandardItem &parent)
{
    size_t count = obs_data_array_count(folder_array);
    for (size_t i = 0; i < count; i++) {
        obs_data_t *item_data = obs_data_array_item(folder_array, i);
        if (!item_data) continue;

        const char *name = obs_data_get_string(item_data, "name");
        const char *type = obs_data_get_string(item_data, "type");

        if (!name || !type) {
            obs_data_release(item_data);
            continue;
        }

        QString itemName = QString::fromUtf8(name);
        QString itemType = QString::fromUtf8(type);

        if (itemType == "folder") {
            // Create folder
            QStandardItem *folderItem = createFolderItem(itemName);
            if (folderItem) {
                parent.appendRow(folderItem);

                // Load custom color if present
                const char *colorName = obs_data_get_string(item_data, "custom_color");
                if (colorName && strlen(colorName) > 0) {
                    QColor color(colorName);
                    if (color.isValid()) {
                        folderItem->setData(color, Qt::UserRole + 1);
                    }
                }

                // Load custom icon spec and tint if present
                const char *iconSpec = obs_data_get_string(item_data, "custom_icon");
                const char *iconColor = obs_data_get_string(item_data, "custom_icon_color");
                if ((iconSpec && strlen(iconSpec) > 0) || (iconColor && strlen(iconColor) > 0)) {
                    if (iconSpec && strlen(iconSpec) > 0) {
                        folderItem->setData(QString::fromUtf8(iconSpec), CustomIconRole);
                    }
                    if (iconColor && strlen(iconColor) > 0) {
                        const QColor color(iconColor);
                        if (color.isValid()) {
                            folderItem->setData(color, CustomIconColorRole);
                        }
                    }
                    static_cast<SceneFolderItem*>(folderItem)->updateIcon();
                }

                // Load children
                obs_data_array_t *children = obs_data_get_array(item_data, "children");
                if (children) {
                    loadFolderArray(children, *folderItem);
                    obs_data_array_release(children);
                }
            }
        } else if (itemType == "scene") {
            // Create placeholder for scene - will be filled by updateTree
            // Find the actual scene source
            obs_source_t *source = Canvas::FindScene(m_canvasType, name);
            if (source) {
                obs_weak_source_t *weak = obs_source_get_weak_source(source);
                QStandardItem *sceneItem = new SceneTreeItem(itemName, weak);
                parent.appendRow(sceneItem);

                // Load custom color if present
                const char *colorName = obs_data_get_string(item_data, "custom_color");
                if (colorName && strlen(colorName) > 0) {
                    QColor color(colorName);
                    if (color.isValid()) {
                        sceneItem->setData(color, Qt::UserRole + 1);
                    }
                }

                // Load custom icon spec and tint if present
                const char *sceneIconSpec = obs_data_get_string(item_data, "custom_icon");
                const char *sceneIconColor = obs_data_get_string(item_data, "custom_icon_color");
                if ((sceneIconSpec && strlen(sceneIconSpec) > 0) || (sceneIconColor && strlen(sceneIconColor) > 0)) {
                    if (sceneIconSpec && strlen(sceneIconSpec) > 0) {
                        sceneItem->setData(QString::fromUtf8(sceneIconSpec), CustomIconRole);
                    }
                    if (sceneIconColor && strlen(sceneIconColor) > 0) {
                        const QColor color(sceneIconColor);
                        if (color.isValid()) {
                            sceneItem->setData(color, CustomIconColorRole);
                        }
                    }
                    static_cast<SceneTreeItem*>(sceneItem)->updateIcon();
                }

                // Add to our tracking map
                m_scenesInTree[weak] = sceneItem;

                obs_source_release(source);

                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "LoadConfig",
                    QString("Loaded scene '%1' from config").arg(itemName).toUtf8().constData());
            } else {
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "LoadConfig",
                    QString("Scene '%1' not found in OBS, skipping").arg(itemName).toUtf8().constData());
            }
        }

        obs_data_release(item_data);
    }
}


//==============================================================================
// SceneTreeView Implementation
//==============================================================================

SceneTreeView::SceneTreeView(QWidget *parent)
    : QTreeView(parent)
{
    setupView();
}

void SceneTreeView::setupView()
{
    // Do nothing - let the parent dock configure all settings
    // This ensures we don't interfere with OBS theme styling
}

void SceneTreeView::drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const
{
    // The branch column is painted here rather than by QTreeView, because the
    // base implementation hands the current hover and selection state to the
    // style, and an OBS theme styles ::branch for those states independently of
    // the row. That produced two separate faults: a small rounded block of the
    // item hover colour in the indent column, and a chevron that vanished in
    // whichever states the theme (or a fix to it) had rules for but no image.
    //
    // Drawing it with a clean state means only the plain ::branch:closed and
    // ::branch:open rules can match - the two that actually carry the chevron
    // images - so the arrow is drawn identically no matter what the mouse is
    // doing, and no state background is ever painted here at all.

    // The indent column is repainted with the dock's own background before
    // anything else goes into it. Some themes paint row state into this column
    // through the ::branch rules, which leaves a small rounded block of the
    // hover or selection colour floating to the left of the row - the row's pill
    // is the only thing that should express that state. Rather than chase which
    // rule in which theme is responsible, the column is simply cleared first.
    //
    // This applies to every row, theme-painted ones included. A theme's
    // ::branch:selected rule draws its own rounded block here, which on an
    // indented row is a second pill floating to the left of the real one with a
    // gap between them. The highlight belongs to the item, so the indent column
    // is cleared back to the dock background and the highlight simply starts
    // where the item does.
    painter->save();
    painter->fillRect(rect, palette().color(QPalette::Base));
    painter->restore();

    // Guides and chevron always sit on the cleared background, never on a
    // highlight, so they follow the plain text colour.
    const QPalette::ColorRole branchRole = QPalette::Text;

    int depth = 0;
    for (QModelIndex walk = index.parent(); walk.isValid(); walk = walk.parent()) {
        ++depth;
    }

    const int step = indentation();

    // A folder sitting inside another folder gets no guide drawn on its own row,
    // so the line breaks at each folder rather than running past it. The rows
    // inside it still get theirs, which is what makes a nested folder read as a
    // new heading rather than as another item in the list above it.
    const bool nestedFolderRow = (depth > 0) && index.data(TabItemIsFolderRole).toBool();

    if (StreamUP::SettingsManager::GetCurrentSettings().sceneOrganiserShowIndentGuides && depth > 0 && !nestedFolderRow) {
        // Derived from the theme's own text colour at low alpha rather than a
        // fixed grey, so the guides sit a consistent distance from the
        // background on a light theme and a dark one alike.
        QColor guide = palette().color(branchRole);
        guide.setAlpha(60);

        painter->save();
        painter->setPen(QPen(guide, 1));

        const int rowTop = rect.top();
        const int rowBottom = rect.bottom();
        const int rowMiddle = rowTop + (rect.height() / 2);
        const int iconHalf = iconSize().width() / 2;

        // A guide belongs to a FOLDER, and runs the height of that folder's
        // contents. Which means the question at each level is "is this row the
        // last thing inside that folder", not "does that folder have a sibling
        // after it" - the sibling rule is the classic one, and it drops the
        // outer line beside a nested folder's children whenever the outer folder
        // happens to be the last item in its own parent, leaving a gap in the
        // middle of a run of rows that are all still inside it.
        //
        // A row is the last thing inside an ancestor only if it is the last
        // child of its parent AND every folder between the two is likewise the
        // last child of its own parent, so the flag is carried outward.
        bool lastInside = index.row() == (model()->rowCount(index.parent()) - 1);

        QModelIndex ancestor = index.parent();
        for (int level = depth - 1; level >= 0 && ancestor.isValid(); --level) {
            // Lined up under the parent folder's ICON, not down the middle of the
            // indent step. The step's centre sits in the parent's chevron column,
            // which reads as a line beside the folder rather than one descending
            // from it. The parent's icon starts one step in from its own depth.
            const int x = rect.left() + ((level + 1) * step) + iconHalf;

            if (lastInside) {
                // The run ends here: stop halfway and turn into the row, so the
                // last item reads as attached rather than the line running on
                // past the end of the folder.
                painter->drawLine(x, rowTop, x, rowMiddle);
                const int elbowEnd = rect.left() + ((level + 2) * step) - 2;
                painter->drawLine(x, rowMiddle, elbowEnd, rowMiddle);
            } else {
                painter->drawLine(x, rowTop, x, rowBottom);
            }

            // Step outward: this ancestor's own position decides whether the
            // next level out is still running.
            const QModelIndex parentOfAncestor = ancestor.parent();
            lastInside = lastInside && (ancestor.row() == (model()->rowCount(parentOfAncestor) - 1));
            ancestor = parentOfAncestor;
        }

        painter->restore();
    }

    // The chevron is drawn here rather than handed to the style. Going through
    // PE_IndicatorBranch means a theme's ::branch rules decide what appears, and
    // those are matched per state: the closed rule drew fine while the open one
    // never matched at all, leaving an expanded folder with no arrow. Rather
    // than keep guessing which pseudo-state combination a given theme wants,
    // the dock draws its own - one shape, both states, every theme.
    if (model()->hasChildren(index)) {
        const QRect cell(rect.left() + (depth * step), rect.top(), step, rect.height());

        // Sized off the row so it keeps its proportions as the row height
        // setting changes, and kept small enough not to crowd the icon.
        const qreal size = qBound(5, cell.height() / 4, 9);

        // The guide line that runs past this row. A top level folder has none,
        // so it falls back to the middle of its own indent step.
        const qreal guideX = rect.left() + (depth * step) + (iconSize().width() / 2.0);
        const qreal centreX = (depth > 0) ? guideX : cell.center().x();
        const qreal centreY = cell.center().y() + 0.5;

        // Follows the theme through the palette, so it stays legible on a light
        // theme and a dark one without either being special-cased.
        QColor arrow = palette().color(branchRole);
        arrow.setAlpha(200);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(arrow);

        // Floating point, and a true apex rather than a point nudged a pixel
        // past the base: the old shape was a pixel wider on one side than the
        // other, which at this size reads as a blunt 2px tip instead of a point.
        QPolygonF triangle;
        if (isExpanded(index)) {
            // Open: pointing down, sitting centred on the line.
            triangle << QPointF(centreX - size, centreY - (size / 2.0))
                     << QPointF(centreX + size, centreY - (size / 2.0))
                     << QPointF(centreX, centreY + (size / 2.0));
        } else {
            // Closed: pointing right, with its long side ON the line rather than
            // straddling it, so the line reads as the edge the arrow grows from.
            triangle << QPointF(centreX, centreY - size)
                     << QPointF(centreX, centreY + size)
                     << QPointF(centreX + size, centreY);
        }

        painter->drawPolygon(triangle);
        painter->restore();
    }
}

void SceneTreeView::startDrag(Qt::DropActions supportedActions)
{
    // A folder carries its contents, so a selection holding both a folder and
    // something inside it describes the same rows twice. The payload already
    // drops the inner ones, but the view removes the ORIGINALS from the
    // selection, not from the payload - so it would go looking for rows the
    // folder move has already rebuilt, working from positions that have shifted
    // underneath it. Pruning the selection first keeps the two in step.
    if (QItemSelectionModel *selection = selectionModel()) {
        const QModelIndexList selected = selection->selectedRows();

        for (const QModelIndex &index : selected) {
            for (QModelIndex walk = index.parent(); walk.isValid(); walk = walk.parent()) {
                if (selected.contains(walk)) {
                    selection->select(index, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
                    break;
                }
            }
        }
    }

    QTreeView::startDrag(supportedActions);
}

void SceneTreeView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-streamup-sceneorganiser")) {
        event->acceptProposedAction();
    }
    QTreeView::dragEnterEvent(event);
}

void SceneTreeView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-streamup-sceneorganiser")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
    QTreeView::dragMoveEvent(event);
}

void SceneTreeView::dropEvent(QDropEvent *event)
{
    // Snapshot before the drop so the whole move - however many items it took,
    // and whatever folders it reshuffled - collapses into one undo entry.
    SceneOrganiserDock *dock = nullptr;
    QWidget *walk = parentWidget();
    while (walk && !dock) {
        dock = qobject_cast<SceneOrganiserDock*>(walk);
        walk = walk->parentWidget();
    }

    // Only the Scenes tree feeds the undo stack: a tab is an arrangement of its
    // own and its drops are saved directly, not recorded as scene-tree edits.
    const bool isScenesTree = dock && dock->m_treeView == this;
    const QString before = isScenesTree ? dock->captureLayout() : QString();

    QTreeView::dropEvent(event);

    if (isScenesTree) {
        dock->pushLayoutUndo(QString::fromUtf8(obs_module_text("SceneOrganiser.Undo.Move")), before);
    }
}

void SceneTreeView::contextMenuEvent(QContextMenuEvent *event)
{
    // This is handled by the custom context menu signal
    event->accept();
}

void SceneTreeView::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // QTreeView fills the whole row with the theme's selection colour (via
    // PE_PanelItemViewRow) before the delegate gets a look in. On a row that
    // CustomColorDelegate paints itself, that fill is a second background behind
    // the delegate's rounded pill — the darker band around a coloured row.
    //
    // The delegate's own copy of the state was already cleared, which is why an
    // unselected coloured row looked right and only the selected one did not:
    // this fill happens a level above the delegate and needed clearing too.
    //
    // Only rows the delegate actually paints need it suppressed. A row with no
    // custom colour is left entirely to the theme, so the theme's selection and
    // hover fill has to survive - that is what makes the dock match the Sources
    // list beside it. The live scene gets the theme's selected row fill by
    // asking the style for it here, matching what the delegate does above.
    const bool paintedByDelegate = index.data(Qt::UserRole + 1).isValid();
    if (!paintedByDelegate) {
        QStyleOptionViewItem themedOpt = option;
        if (index.data(ProgramSceneRole).toBool()) {
            themedOpt.state |= QStyle::State_Selected;
        }
        QTreeView::drawRow(painter, themedOpt, index);
        return;
    }

    QStyleOptionViewItem opt = option;
    opt.state &= ~QStyle::State_Selected;
    opt.state &= ~QStyle::State_MouseOver;
    QTreeView::drawRow(painter, opt, index);
}

void SceneTreeView::keyPressEvent(QKeyEvent *event)
{
    // Find the parent dock for all hotkey actions
    QWidget *parentWidget = this->parentWidget();
    SceneOrganiserDock *dock = nullptr;
    while (parentWidget && !dock) {
        dock = qobject_cast<SceneOrganiserDock*>(parentWidget);
        parentWidget = parentWidget->parentWidget();
    }

    if (!dock) {
        QTreeView::keyPressEvent(event);
        return;
    }

    // Handle hotkeys
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        // Delete/Remove hotkey
        dock->triggerRemove();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_F2) {
        // Rename hotkey (F2)
        dock->triggerRename();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        // Enter/Return activates the selected scene (go live / transition).
        // Arrow keys are intentionally NOT intercepted below so they keep
        // moving the selection (preview) only, without switching program.
        dock->triggerActivateSelectedScene();
        event->accept();
        return;
    }

    // Let the parent handle other keys (including Up/Down arrows, which move
    // selection only and never change the program scene).
    QTreeView::keyPressEvent(event);
}



//==============================================================================
// Layout undo / redo
//==============================================================================

// The tree layout already has a serialised form - the one written to disk - so
// undo is built on whole-layout snapshots rather than an inverse operation per
// command. One mechanism then covers drag and drop, the move actions, folder
// add/rename/delete and colour changes, and it cannot drift out of step with
// them the way hand-written inverses do.
//
// Scene DELETION is deliberately not covered: that removes the source from OBS
// itself, and no amount of restoring our tree brings the scene back.
QString SceneTreeModel::serialiseLayout()
{
    obs_data_t *root = obs_data_create();
    obs_data_array_t *folder_array = createFolderArray(*invisibleRootItem());
    obs_data_set_array(root, "layout", folder_array);
    obs_data_array_release(folder_array);

    const QString json = QString::fromUtf8(obs_data_get_json(root));
    obs_data_release(root);
    return json;
}

void SceneTreeModel::restoreLayout(const QString &json)
{
    obs_data_t *root = obs_data_create_from_json(json.toUtf8().constData());
    if (!root) {
        return;
    }

    obs_data_array_t *folder_array = obs_data_get_array(root, "layout");
    if (folder_array) {
        cleanupSceneTree();
        loadFolderArray(folder_array, *invisibleRootItem());
        obs_data_array_release(folder_array);

        // Reconcile against what OBS actually has, so a scene created or
        // renamed since the snapshot is not stranded outside the tree.
        updateTree();
        emit modelChanged();
        saveSceneTree();
    }

    obs_data_release(root);
}

QString SceneOrganiserDock::captureLayout()
{
    return m_model ? m_model->serialiseLayout() : QString();
}

void SceneOrganiserDock::pushLayoutUndo(const QString &name, const QString &before)
{
    // Nothing to record if the snapshot failed, the layout is unchanged, or we
    // are ourselves in the middle of applying an undo.
    if (m_applyingLayoutSnapshot || before.isEmpty()) {
        return;
    }

    const QString after = captureLayout();
    if (after.isEmpty() || after == before) {
        return;
    }

    auto pack = [this](const QString &layout) {
        obs_data_t *data = obs_data_create();
        obs_data_set_int(data, "canvas", static_cast<int>(m_canvasType));
        obs_data_set_string(data, "layout", layout.toUtf8().constData());
        const QString json = QString::fromUtf8(obs_data_get_json(data));
        obs_data_release(data);
        return json;
    };

    const QString undoData = pack(before);
    const QString redoData = pack(after);

    obs_frontend_add_undo_redo_action(name.toUtf8().constData(),
                                      SceneOrganiserDock::ApplyLayoutSnapshot,
                                      SceneOrganiserDock::ApplyLayoutSnapshot,
                                      undoData.toUtf8().constData(),
                                      redoData.toUtf8().constData(),
                                      false);
}

// Undo and redo are the same operation here - both just put a stored layout
// back - so one callback serves both directions.
void SceneOrganiserDock::ApplyLayoutSnapshot(const char *data)
{
    if (!data) {
        return;
    }

    obs_data_t *parsed = obs_data_create_from_json(data);
    if (!parsed) {
        return;
    }

    const CanvasType canvasType = static_cast<CanvasType>(obs_data_get_int(parsed, "canvas"));
    const QString layout = QString::fromUtf8(obs_data_get_string(parsed, "layout"));
    obs_data_release(parsed);

    for (SceneOrganiserDock *dock : s_dockInstances) {
        if (!dock || dock->GetCanvasType() != canvasType) {
            continue;
        }

        // Guard so restoring does not register an undo of its own, and so the
        // save timer does not fight the restore.
        dock->m_applyingLayoutSnapshot = true;
        dock->m_model->restoreLayout(layout);
        dock->applyAllCustomColors();
        dock->applySceneVisibility();
        dock->updateActiveSceneHighlight();
        dock->refreshQuickList();
        dock->m_applyingLayoutSnapshot = false;
        return;
    }
}

//==============================================================================
// QuickListDelegate Implementation
//==============================================================================

StreamUP::SceneOrganiser::QuickListDelegate::QuickListDelegate(SceneOrganiserDock *dock, QObject *parent)
    : QStyledItemDelegate(parent), m_dock(dock)
{
}

void StreamUP::SceneOrganiser::QuickListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                                        const QModelIndex &index) const
{
    if (!index.isValid() || !m_dock) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    const bool isProgram = index.data(ProgramSceneRole).toBool();
    const QVariant colorData = index.data(Qt::UserRole + 1);
    const QColor customColor = colorData.isValid() ? colorData.value<QColor>() : QColor();
    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isHovered = option.state & QStyle::State_MouseOver;

    if (!customColor.isValid()) {
        // Same rule as the tree: no custom colour means the theme owns the row,
        // and the live scene is shown with the theme's selected look.
        QStyleOptionViewItem themedOption = option;
        if (isProgram) {
            themedOption.state |= QStyle::State_Selected;
        }
        QStyledItemDelegate::paint(painter, themedOption, index);
        if (isProgram && ProgramRowNeedsOutline(option, index)) {
            DrawProgramOutline(painter, option.rect, option.palette.color(QPalette::HighlightedText));
        }
        return;
    }

    // Identical rules to the tree: a hand-set colour is brightened for
    // selection (and for the live scene), less so for hover.
    QColor bgColor = customColor;
    if (isSelected || isProgram) {
        bgColor = m_dock->getSelectionColor(customColor);
    } else if (isHovered) {
        bgColor = m_dock->getHoverColor(customColor);
    }

    if (!bgColor.isValid()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    bgColor = m_dock->ensureRowContrast(bgColor);

    const QColor textColor = m_dock->getContrastTextColor(bgColor);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QRect rect = option.rect;
    rect.adjust(2, 1, -2, -1);
    painter->setPen(Qt::NoPen);
    painter->setBrush(bgColor);
    painter->drawRoundedRect(rect, 4, 4);
    painter->restore();

    if (isProgram && ProgramRowNeedsOutline(option, index)) {
        DrawProgramOutline(painter, option.rect, textColor);
    }

    QStyleOptionViewItem modifiedOption = option;
    modifiedOption.palette.setColor(QPalette::Text, textColor);
    modifiedOption.palette.setColor(QPalette::HighlightedText, textColor);
    modifiedOption.backgroundBrush = QBrush(Qt::NoBrush);
    modifiedOption.state &= ~QStyle::State_HasFocus;
    modifiedOption.state &= ~QStyle::State_Selected;
    modifiedOption.state &= ~QStyle::State_MouseOver;

    QStyledItemDelegate::paint(painter, modifiedOption, index);
}

QSize StreamUP::SceneOrganiser::QuickListDelegate::sizeHint(const QStyleOptionViewItem &option,
                                                           const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(m_dock ? m_dock->currentRowHeight() : 24);
    return size;
}

//==============================================================================
// Tabs: Scenes, the built-in lists, and user-made custom tabs
//==============================================================================

// Tab bar entries carry their identity in tabData as a token, not a number tied
// to their position: tabs are reorderable, can be switched off in the settings,
// and custom ones come and go. A custom tab is identified by its name, so its
// place in the saved order survives its neighbours being renamed or deleted.
static const char *kTabTokenScenes = "scenes";
static const char *kTabTokenFavourites = "favourites";
static const char *kTabTokenRecent = "recent";
static const char *kTabTokenCustomPrefix = "custom:";

static QString CustomTabToken(const QString &name)
{
    return QString::fromLatin1(kTabTokenCustomPrefix) + name;
}

void SceneOrganiserDock::setupQuickTabs()
{
    m_quickTabs = new QTabBar(this);
    // Named so the theme can shape it. The global QTabBar rules are built for
    // the main window's preview tabs, which round the BOTTOM corners; a tab bar
    // sitting above a toolbar wants the opposite.
    m_quickTabs->setObjectName("SceneOrganiserTabBar");
    m_quickTabs->setExpanding(false);

    // Every tab can be dragged into whatever order suits, the built-in three
    // included - the order is saved by tab identity, so it survives tabs being
    // switched off in the settings and switched back on later.
    m_quickTabs->setMovable(true);
    m_quickTabs->setDrawBase(true);

    // Right-click is the route to creating and managing custom tabs.
    m_quickTabs->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_quickTabs, &QWidget::customContextMenuRequested, this, &SceneOrganiserDock::onQuickTabsContextMenu);
    connect(m_quickTabs, &QTabBar::currentChanged, this, &SceneOrganiserDock::onQuickTabChanged);
    connect(m_quickTabs, &QTabBar::tabMoved, this, &SceneOrganiserDock::onTabMoved);

    // One tree serves every non-Scenes tab, repopulated as tabs change. It is
    // the SAME view class as the Scenes tree - that is what gives it the same
    // painting - over a plain item model, because a tab holds an arrangement of
    // names rather than live scene sources.
    m_quickModel = new QStandardItemModel(this);
    m_quickProxy = new QSortFilterProxyModel(this);
    m_quickProxy->setSourceModel(m_quickModel);
    m_quickProxy->setRecursiveFilteringEnabled(true);
    m_quickProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_quickTree = new SceneTreeView(this);
    m_quickTree->setModel(m_quickProxy);
    m_quickTree->setHeaderHidden(true);
    m_quickTree->setFrameShape(QFrame::NoFrame);
    m_quickTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_quickTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_quickTree->setIndentation(20);
    m_quickTree->setRootIsDecorated(true);
    m_quickTree->setExpandsOnDoubleClick(false);
    m_quickTree->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Folders are arranged by dragging, exactly as in the Scenes tree.
    m_quickTree->setDragDropMode(QAbstractItemView::InternalMove);
    m_quickTree->setDefaultDropAction(Qt::MoveAction);
    m_quickTree->setDropIndicatorShown(true);
    m_quickTree->setDragEnabled(true);
    m_quickTree->setAcceptDrops(true);

    // No sideways scrolling: a long scene name is cut with an ellipsis.
    m_quickTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_quickTree->setTextElideMode(Qt::ElideRight);

    // Same delegate as the Scenes tree, so a row looks the same anywhere.
    m_quickTree->setItemDelegate(new CustomColorDelegate(this, m_quickTree, m_quickProxy, m_quickModel));
    applyRowMetricsToQuickList();

    // The quick list is built after the constructor ran, so it takes its copy
    // of the theme's row rules here.
    applyThemeRowStyling();

    connect(m_quickTree, &QAbstractItemView::clicked, this, &SceneOrganiserDock::onQuickTreeActivated);

    // Tab folders swap between the open and closed icon the same as the Scenes
    // tree's do.
    auto tabFolderIcon = [this](const QModelIndex &index, bool expanded) {
        if (!m_quickModel || !m_quickProxy) {
            return;
        }
        if (QStandardItem *item = m_quickModel->itemFromIndex(m_quickProxy->mapToSource(index))) {
            if (item->data(TabItemIsFolderRole).toBool()) {
                item->setIcon(GetFolderIcon(expanded, palette().color(QPalette::Text)));
            }
        }
    };
    connect(m_quickTree, &QTreeView::expanded, this, [tabFolderIcon](const QModelIndex &i) { tabFolderIcon(i, true); });
    connect(m_quickTree, &QTreeView::collapsed, this, [tabFolderIcon](const QModelIndex &i) { tabFolderIcon(i, false); });
    connect(m_quickTree, &QWidget::customContextMenuRequested, this, &SceneOrganiserDock::onQuickTreeContextMenu);

    // A drag inside the tree changes the tab's arrangement, so the widget is
    // read back into the tab whenever its rows move.
    connect(m_quickModel, &QAbstractItemModel::rowsMoved, this, [this]() { commitQuickTreeToTab(); });
    connect(m_quickModel, &QAbstractItemModel::rowsRemoved, this, [this]() {
        if (!m_populatingQuickTree) {
            commitQuickTreeToTab();
        }
    });

    m_viewStack = new QStackedWidget(this);
    m_viewStack->addWidget(m_treeView);   // index 0 - the organiser tree
    m_viewStack->addWidget(m_quickTree);  // index 1 - whichever tab is active

    rebuildTabBar();
}

// ---------------------------------------------------------------------------
// Node helpers
// ---------------------------------------------------------------------------

bool SceneOrganiserDock::nodesContainScene(const QVector<TabNode> &nodes, const QString &sceneName)
{
    for (const TabNode &node : nodes) {
        if (!node.isFolder && node.name == sceneName) {
            return true;
        }
        if (node.isFolder && nodesContainScene(node.children, sceneName)) {
            return true;
        }
    }
    return false;
}

bool SceneOrganiserDock::removeSceneFromNodes(QVector<TabNode> &nodes, const QString &sceneName)
{
    for (int i = 0; i < nodes.size(); ++i) {
        if (!nodes[i].isFolder && nodes[i].name == sceneName) {
            nodes.remove(i);
            return true;
        }
        if (nodes[i].isFolder && removeSceneFromNodes(nodes[i].children, sceneName)) {
            return true;
        }
    }
    return false;
}

void SceneOrganiserDock::collectSceneNames(const QVector<TabNode> &nodes, QStringList &out)
{
    for (const TabNode &node : nodes) {
        if (node.isFolder) {
            collectSceneNames(node.children, out);
        } else if (!out.contains(node.name)) {
            out << node.name;
        }
    }
}

QString SceneOrganiserDock::uniqueFolderName(const QVector<TabNode> &nodes, const QString &base)
{
    QStringList taken;
    for (const TabNode &node : nodes) {
        if (node.isFolder) {
            taken << node.name;
        }
    }

    if (!taken.contains(base)) {
        return base;
    }
    for (int suffix = 2; ; ++suffix) {
        const QString candidate = QString("%1 %2").arg(base).arg(suffix);
        if (!taken.contains(candidate)) {
            return candidate;
        }
    }
}

QVector<TabNode> *SceneOrganiserDock::editableNodesForCurrentTab()
{
    if (m_currentKind == QuickTabKind::Favourites) {
        return &m_favouriteNodes;
    }
    if (m_currentKind == QuickTabKind::Custom &&
        m_currentCustomTab >= 0 && m_currentCustomTab < m_customTabs.size()) {
        return &m_customTabs[m_currentCustomTab].nodes;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Populating the tab tree, and reading it back
// ---------------------------------------------------------------------------

void SceneOrganiserDock::refreshQuickList()
{
    if (!m_quickTree || !m_quickModel || m_currentKind == QuickTabKind::Scenes) {
        return;
    }

    // Guarded because populating inserts and removes rows, which is also how a
    // drag finishes - without this the rebuild would write itself back.
    m_populatingQuickTree = true;
    m_quickModel->clear();

    QString liveScene;
    if (obs_source_t *current = Canvas::GetCurrentScene(m_canvasType)) {
        liveScene = QString::fromUtf8(obs_source_get_name(current));
        obs_source_release(current);
    }

    auto makeSceneItem = [&](const QString &sceneName) -> QStandardItem * {
        // A name that no longer resolves to a scene on this canvas is skipped
        // rather than listed: a row that does nothing when clicked is worse than
        // an absent one. The node stays in the tab, so it returns if the scene does.
        QStandardItem *treeItem = m_model->findSceneItemByName(sceneName);
        if (!treeItem || m_hiddenScenes.contains(sceneName)) {
            return nullptr;
        }

        QStandardItem *item = new QStandardItem(sceneName);
        item->setIcon(treeItem->icon());
        item->setData(false, TabItemIsFolderRole);
        item->setEditable(false);
        item->setDropEnabled(false); // a scene is not a container
        item->setDragEnabled(true);

        // Colour and programme marker are passed as data, exactly as the Scenes
        // tree does it, so the delegate draws them the same way.
        const QVariant colorData = treeItem->data(Qt::UserRole + 1);
        if (colorData.isValid()) {
            item->setData(colorData, Qt::UserRole + 1);
        }
        item->setData(sceneName == liveScene, ProgramSceneRole);
        return item;
    };

    std::function<void(const QVector<TabNode> &, QStandardItem *)> build =
        [&](const QVector<TabNode> &nodes, QStandardItem *parent) {
            for (const TabNode &node : nodes) {
                if (node.isFolder) {
                    QStandardItem *folder = new QStandardItem(node.name);
                    // Tab folders are drawn expanded unless the tree is
                    // collapsed, which refreshQuickList applies just below.
                    folder->setIcon(GetFolderIcon(!m_quickTreeCollapsed,
                                                  palette().color(QPalette::Text)));
                    folder->setData(true, TabItemIsFolderRole);
                    folder->setEditable(false);
                    folder->setDropEnabled(true);
                    folder->setDragEnabled(true);

                    if (parent) {
                        parent->appendRow(folder);
                    } else {
                        m_quickModel->invisibleRootItem()->appendRow(folder);
                    }
                    build(node.children, folder);
                } else if (QStandardItem *item = makeSceneItem(node.name)) {
                    if (parent) {
                        parent->appendRow(item);
                    } else {
                        m_quickModel->invisibleRootItem()->appendRow(item);
                    }
                }
            }
        };

    if (m_currentKind == QuickTabKind::Recent) {
        // Recent has no arrangement to speak of - it is a plain, ordered list.
        for (const QString &sceneName : m_recentScenes) {
            if (QStandardItem *item = makeSceneItem(sceneName)) {
                m_quickModel->invisibleRootItem()->appendRow(item);
            }
        }
    } else if (QVector<TabNode> *nodes = editableNodesForCurrentTab()) {
        build(*nodes, nullptr);
    }

    if (m_quickModel->rowCount() == 0) {
        const char *emptyText = "SceneOrganiser.Tab.Custom.Empty";
        if (m_currentKind == QuickTabKind::Favourites) {
            emptyText = "SceneOrganiser.Tab.Favourites.Empty";
        } else if (m_currentKind == QuickTabKind::Recent) {
            emptyText = "SceneOrganiser.Tab.Recent.Empty";
        }

        QStandardItem *empty = new QStandardItem(obs_module_text(emptyText));
        empty->setFlags(Qt::NoItemFlags);
        m_quickModel->invisibleRootItem()->appendRow(empty);
    }

    // Folders come back the way the user left them.
    if (m_quickTreeCollapsed) {
        m_quickTree->collapseAll();
    } else {
        m_quickTree->expandAll();
    }

    // Recent is not rearranged by hand, so it does not accept drags.
    const bool arrangeable = (editableNodesForCurrentTab() != nullptr);
    m_quickTree->setDragDropMode(arrangeable ? QAbstractItemView::InternalMove
                                             : QAbstractItemView::NoDragDrop);
    m_quickTree->setDragEnabled(arrangeable);
    m_quickTree->setAcceptDrops(arrangeable);

    m_populatingQuickTree = false;
}

void SceneOrganiserDock::commitQuickTreeToTab()
{
    QVector<TabNode> *nodes = editableNodesForCurrentTab();
    if (!nodes || !m_quickModel || m_populatingQuickTree) {
        return;
    }

    std::function<QVector<TabNode>(QStandardItem *)> read = [&](QStandardItem *parent) {
        QVector<TabNode> out;
        QStandardItem *root = parent ? parent : m_quickModel->invisibleRootItem();
        for (int i = 0; i < root->rowCount(); ++i) {
            QStandardItem *item = root->child(i);
            if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
                continue; // the empty-state placeholder
            }

            TabNode node;
            node.isFolder = item->data(TabItemIsFolderRole).toBool();
            node.name = item->text();
            if (node.isFolder) {
                node.children = read(item);
            }
            out << node;
        }
        return out;
    };

    *nodes = read(nullptr);
    SaveConfiguration();
}

// The item behind a view index on the tab tree, mapping through its proxy.
static QStandardItem *TabItemFromIndex(QSortFilterProxyModel *proxy, QStandardItemModel *model,
                                       const QModelIndex &index)
{
    if (!proxy || !model || !index.isValid()) {
        return nullptr;
    }
    return model->itemFromIndex(proxy->mapToSource(index));
}

void SceneOrganiserDock::onQuickTreeActivated(const QModelIndex &index)
{
    QStandardItem *item = TabItemFromIndex(m_quickProxy, m_quickModel, index);
    if (!item || !(item->flags() & Qt::ItemIsEnabled) || item->data(TabItemIsFolderRole).toBool()) {
        return; // folders are for organising, not for going live
    }

    if (QStandardItem *treeItem = m_model->findSceneItemByName(item->text())) {
        activateSceneItem(treeItem);
        refreshQuickList();
    }
}

void SceneOrganiserDock::onQuickTreeContextMenu(const QPoint &pos)
{
    QStandardItem *item = TabItemFromIndex(m_quickProxy, m_quickModel, m_quickTree->indexAt(pos));
    const bool isFolder = item && item->data(TabItemIsFolderRole).toBool();
    const bool arrangeable = (editableNodesForCurrentTab() != nullptr);

    QMenu menu(this);

    if (arrangeable) {
        menu.addAction(obs_module_text("SceneOrganiser.Action.AddFolder"), this,
                       &SceneOrganiserDock::onAddTabFolderClicked);
        menu.addAction(obs_module_text("SceneOrganiser.Menu.AddScenes"), this,
                       &SceneOrganiserDock::showAddToTabMenu);
    }

    if (item && (item->flags() & Qt::ItemIsEnabled)) {
        menu.addSeparator();

        if (isFolder && arrangeable) {
            menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Rename"), -1), this, [this, item]() {
                onRenameTabFolderClicked(item);
            });
        }
        if (arrangeable) {
            menu.addAction(obs_module_text("SceneOrganiser.Action.RemoveFromTab"), this, [this, item]() {
                onRemoveFromTabClicked(item);
            });
        }

        if (!isFolder) {
            const QString sceneName = item->text();
            menu.addSeparator();
            menu.addAction(isFavourite(sceneName)
                               ? obs_module_text("SceneOrganiser.Action.RemoveFavourite")
                               : obs_module_text("SceneOrganiser.Action.AddFavourite"),
                           this, [this, sceneName]() {
                               if (isFavourite(sceneName)) {
                                   removeSceneFromNodes(m_favouriteNodes, sceneName);
                               } else {
                                   TabNode node;
                                   node.name = sceneName;
                                   m_favouriteNodes.append(node);
                               }
                               refreshQuickList();
                               SaveConfiguration();
                           });

            // Jumping back to the tree is the natural next step after finding a
            // scene here and wanting to see where it actually lives.
            menu.addAction(obs_module_text("SceneOrganiser.Action.ShowInTree"), this, [this, sceneName]() {
                if (m_quickTabs) {
                    m_quickTabs->setCurrentIndex(0);
                }
                selectSceneByName(sceneName);
            });
        }
    }

    if (menu.isEmpty()) {
        return;
    }
    menu.exec(m_quickTree->mapToGlobal(pos));
}

void SceneOrganiserDock::onAddTabFolderClicked()
{
    if (!editableNodesForCurrentTab()) {
        return;
    }

    QPointer<SceneOrganiserDock> self(this);
    su::prompt(this,
        QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.AddFolder.Title")),
        QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.AddFolder.Text")),
        QString(),
        [self](const QString &folderName) {
            if (!self || folderName.trimmed().isEmpty()) {
                return;
            }
            QVector<TabNode> *target = self->editableNodesForCurrentTab();
            if (!target) {
                return;
            }

            TabNode folder;
            folder.isFolder = true;
            folder.name = uniqueFolderName(*target, folderName.trimmed());
            target->append(folder);

            self->refreshQuickList();
            self->SaveConfiguration();
        });
}

void SceneOrganiserDock::onRenameTabFolderClicked(QStandardItem *item)
{
    if (!item || !item->data(TabItemIsFolderRole).toBool()) {
        return;
    }

    QPointer<SceneOrganiserDock> self(this);
    const QString oldName = item->text();

    su::prompt(this,
        QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.RenameFolder.Title")),
        QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.RenameFolder.Text")),
        oldName,
        [self, item, oldName](const QString &newName) {
            if (!self || newName.trimmed().isEmpty() || newName.trimmed() == oldName) {
                return;
            }
            item->setText(newName.trimmed());
            self->commitQuickTreeToTab();
        });
}

void SceneOrganiserDock::onRemoveFromTabClicked(QStandardItem *item)
{
    if (!item || !editableNodesForCurrentTab() || !m_quickModel) {
        return;
    }

    // Removing a folder takes its contents out of the tab with it. The scenes
    // themselves are untouched - a tab is an arrangement, not ownership.
    QStandardItem *parent = item->parent() ? item->parent() : m_quickModel->invisibleRootItem();
    parent->removeRow(item->row());

    commitQuickTreeToTab();
    refreshQuickList();
}

// ---------------------------------------------------------------------------
// Tab bar: building, ordering, switching
// ---------------------------------------------------------------------------

void SceneOrganiserDock::rebuildTabBar()
{
    if (!m_quickTabs) {
        return;
    }

    // Remember what was selected so the same tab can be picked back up. Doing
    // this by identity rather than by position is what lets a tab appear or
    // vanish beside the current one without moving the user somewhere else.
    const QString wantToken = currentTabToken();

    QSignalBlocker blocker(m_quickTabs);
    while (m_quickTabs->count() > 0) {
        m_quickTabs->removeTab(0);
    }

    QStringList available;
    available << QString::fromLatin1(kTabTokenScenes);

    const StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    if (settings.sceneOrganiserShowFavouritesTab) {
        available << QString::fromLatin1(kTabTokenFavourites);
    }
    if (settings.sceneOrganiserShowRecentTab) {
        available << QString::fromLatin1(kTabTokenRecent);
    }
    for (const CustomSceneTab &tab : m_customTabs) {
        available << CustomTabToken(tab.name);
    }

    // Saved order first, then anything new appended - so a tab switched back on
    // returns to where it was, and a tab just created lands at the end.
    QStringList ordered;
    for (const QString &token : m_tabOrder) {
        if (available.contains(token) && !ordered.contains(token)) {
            ordered << token;
        }
    }
    for (const QString &token : available) {
        if (!ordered.contains(token)) {
            ordered << token;
        }
    }
    m_tabOrder = ordered;

    for (const QString &token : ordered) {
        QString label;
        if (token == QLatin1String(kTabTokenScenes)) {
            label = obs_module_text("SceneOrganiser.Tab.Scenes");
        } else if (token == QLatin1String(kTabTokenFavourites)) {
            label = obs_module_text("SceneOrganiser.Tab.Favourites");
        } else if (token == QLatin1String(kTabTokenRecent)) {
            label = obs_module_text("SceneOrganiser.Tab.Recent");
        } else {
            label = token.mid(int(strlen(kTabTokenCustomPrefix)));
        }

        const int index = m_quickTabs->addTab(label);
        m_quickTabs->setTabData(index, token);
    }

    // With only the tree left there is nothing to switch between, so the bar
    // itself goes rather than sitting there as a single dead tab.
    m_quickTabs->setVisible(m_quickTabs->count() > 1);

    int restoreIndex = 0;
    for (int i = 0; i < m_quickTabs->count(); ++i) {
        if (m_quickTabs->tabData(i).toString() == wantToken) {
            restoreIndex = i;
            break;
        }
    }

    blocker.unblock();
    m_quickTabs->setCurrentIndex(restoreIndex);
    // setCurrentIndex is silent when the index has not changed (it is 0 here on
    // a rebuild that dropped the active tab), so the state is applied directly.
    onQuickTabChanged(restoreIndex);
}

QString SceneOrganiserDock::currentTabToken() const
{
    switch (m_currentKind) {
    case QuickTabKind::Favourites:
        return QString::fromLatin1(kTabTokenFavourites);
    case QuickTabKind::Recent:
        return QString::fromLatin1(kTabTokenRecent);
    case QuickTabKind::Custom:
        if (m_currentCustomTab >= 0 && m_currentCustomTab < m_customTabs.size()) {
            return CustomTabToken(m_customTabs.at(m_currentCustomTab).name);
        }
        return QString::fromLatin1(kTabTokenScenes);
    case QuickTabKind::Scenes:
    default:
        return QString::fromLatin1(kTabTokenScenes);
    }
}

// Any tab can be dragged anywhere, including the built-in three. The bar's new
// order simply becomes the saved order.
void SceneOrganiserDock::onTabMoved(int from, int to)
{
    Q_UNUSED(from);
    Q_UNUSED(to);

    if (!m_quickTabs) {
        return;
    }

    QStringList order;
    for (int i = 0; i < m_quickTabs->count(); ++i) {
        order << m_quickTabs->tabData(i).toString();
    }
    m_tabOrder = order;

    SaveConfiguration();
}

void SceneOrganiserDock::onQuickTabChanged(int index)
{
    const QString token = (m_quickTabs && index >= 0) ? m_quickTabs->tabData(index).toString()
                                                     : QString::fromLatin1(kTabTokenScenes);

    m_currentCustomTab = -1;
    if (token == QLatin1String(kTabTokenFavourites)) {
        m_currentKind = QuickTabKind::Favourites;
    } else if (token == QLatin1String(kTabTokenRecent)) {
        m_currentKind = QuickTabKind::Recent;
    } else if (token.startsWith(QLatin1String(kTabTokenCustomPrefix))) {
        m_currentKind = QuickTabKind::Custom;
        m_currentCustomTab = customTabIndexByName(token.mid(int(strlen(kTabTokenCustomPrefix))));
        if (m_currentCustomTab < 0) {
            m_currentKind = QuickTabKind::Scenes;
        }
    } else {
        m_currentKind = QuickTabKind::Scenes;
    }

    if (m_viewStack) {
        m_viewStack->setCurrentIndex(m_currentKind == QuickTabKind::Scenes ? 0 : 1);
    }

    // A search typed on one tab should not quietly hide rows on the next, so the
    // box is cleared on the way through.
    if (m_searchEdit && !m_searchEdit->text().isEmpty()) {
        m_searchEdit->clear();
    }
    if (m_quickProxy) {
        m_quickProxy->setFilterWildcard(QString());
    }

    refreshQuickList();
    updateControlsForTab();
    if (m_saveTimer) {
        m_saveTimer->start();
    }
}

void SceneOrganiserDock::onQuickTabsContextMenu(const QPoint &pos)
{
    QMenu menu(this);

    menu.addAction(obs_module_text("SceneOrganiser.Action.NewTab"), this, &SceneOrganiserDock::onCreateCustomTabClicked);

    // Rename and delete apply to the tab actually under the cursor, not the
    // selected one - right-clicking a tab and having it act on a different tab
    // would be indefensible.
    const int barIndex = m_quickTabs->tabAt(pos);
    const QString token = (barIndex >= 0) ? m_quickTabs->tabData(barIndex).toString() : QString();
    if (token.startsWith(QLatin1String(kTabTokenCustomPrefix))) {
        const int customIndex = customTabIndexByName(token.mid(int(strlen(kTabTokenCustomPrefix))));
        menu.addAction(obs_module_text("SceneOrganiser.Action.RenameTab"), this, [this, customIndex]() {
            onRenameCustomTabClicked(customIndex);
        });
        menu.addAction(obs_module_text("SceneOrganiser.Action.DeleteTab"), this, [this, customIndex]() {
            onDeleteCustomTabClicked(customIndex);
        });
    }

    // The built-in tabs are switched off from here as well as from the settings
    // page - right-clicking the thing you want rid of is the obvious gesture.
    if (token == QLatin1String(kTabTokenFavourites) || token == QLatin1String(kTabTokenRecent)) {
        const bool favourites = (token == QLatin1String(kTabTokenFavourites));
        menu.addAction(obs_module_text("SceneOrganiser.Action.HideTab"), this, [favourites]() {
            StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
            if (favourites) {
                settings.sceneOrganiserShowFavouritesTab = false;
            } else {
                settings.sceneOrganiserShowRecentTab = false;
            }
            StreamUP::SettingsManager::UpdateSettings(settings);
            SceneOrganiserDock::NotifyAllDocksSettingsChanged();
        });
    }

    menu.addSeparator();
    menu.addAction(obs_module_text("SceneOrganiser.Action.OpenSettings"), this, &SceneOrganiserDock::onSettingsClicked);

    menu.exec(m_quickTabs->mapToGlobal(pos));
}

// ---------------------------------------------------------------------------
// Toolbar behaviour per tab
// ---------------------------------------------------------------------------

void SceneOrganiserDock::updateControlsForTab()
{
    // Search stays on every tab: a tab with folders is just as worth filtering as
    // the Scenes tree, and a control that comes and goes makes the dock jump.
    if (m_searchEdit) {
        m_searchEdit->setPlaceholderText(obs_module_text("SceneOrganiser.Search.Placeholder"));
    }

    // The tab bar is built before the toolbar is, so the first call arrives with
    // no buttons to speak of.
    if (!m_toolbar) {
        return;
    }

    const bool onTree = (m_currentKind == QuickTabKind::Scenes);
    // Recent is maintained by what you go live with, so it is the one tab you do
    // not arrange by hand. Everything else is a tree the user owns.
    const bool arrangeable = (m_currentKind == QuickTabKind::Favourites ||
                              m_currentKind == QuickTabKind::Custom);

    // Rather than grey buttons out, each is relabelled for what it does here:
    // add puts scenes and folders into this tab, remove takes them out, and the
    // arrows order them. Only controls with no meaning at all are hidden.
    if (m_addButton) {
        m_addButton->setVisible(onTree || arrangeable);
        m_addButton->setToolTip(onTree ? obs_module_text("SceneOrganiser.Tooltip.Add")
                                       : obs_module_text("SceneOrganiser.Tooltip.AddToTab"));
    }
    if (m_removeButton) {
        m_removeButton->setToolTip(onTree ? obs_module_text("SceneOrganiser.Tooltip.Remove")
                                          : obs_module_text("SceneOrganiser.Tooltip.RemoveFromTab"));
    }
    if (m_moveUpButton) {
        m_moveUpButton->setVisible(onTree || arrangeable);
    }
    if (m_moveDownButton) {
        m_moveDownButton->setVisible(onTree || arrangeable);
    }
    if (m_expandCollapseButton) {
        // Tab trees have folders of their own, so expand/collapse applies there
        // too. Only Recent, a flat list, has nothing to expand.
        m_expandCollapseButton->setVisible(m_currentKind != QuickTabKind::Recent);

        // The button shows the state of the tree actually on screen.
        const bool collapsed = (m_currentKind == QuickTabKind::Scenes) ? !m_allExpanded : m_quickTreeCollapsed;
        m_expandCollapseButton->blockSignals(true);
        m_expandCollapseButton->setChecked(collapsed);
        m_expandCollapseButton->blockSignals(false);
    }
    if (m_lockButton) {
        // The lock protects the Scenes tree from edits; a tab is an arrangement
        // and has nothing of OBS's to protect.
        m_lockButton->setVisible(onTree);
    }

    updateUIEnabledState();
}

QString SceneOrganiserDock::selectedSceneOnCurrentTab() const
{
    if (m_currentKind == QuickTabKind::Scenes) {
        if (!m_treeView || !m_treeView->selectionModel()) {
            return QString();
        }
        const QModelIndexList selected = m_treeView->selectionModel()->selectedRows();
        if (selected.isEmpty()) {
            return QString();
        }
        QStandardItem *item = m_model->itemFromIndex(m_proxyModel->mapToSource(selected.first()));
        return (item && item->type() == SceneTreeItem::UserType + 2) ? item->text() : QString();
    }

    if (!m_quickTree || !m_quickTree->selectionModel()) {
        return QString();
    }
    const QModelIndexList selected = m_quickTree->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return QString();
    }
    QStandardItem *item = TabItemFromIndex(m_quickProxy, m_quickModel, selected.first());
    if (!item || !(item->flags() & Qt::ItemIsEnabled) || item->data(TabItemIsFolderRole).toBool()) {
        return QString();
    }
    return item->text();
}

void SceneOrganiserDock::applyRowMetricsToQuickList()
{
    if (!m_quickTree || !m_treeView) {
        return;
    }

    // Taken from the Scenes tree rather than recomputed, so there is exactly one
    // definition of what a row looks like in this dock.
    m_quickTree->setIconSize(m_treeView->iconSize());
    m_quickTree->setFont(m_treeView->font());
    m_quickTree->setIndentation(m_treeView->indentation());

    // Row height comes from the delegate's sizeHint, which Qt caches.
    m_quickTree->doItemsLayout();
    if (m_quickTree->viewport()) {
        m_quickTree->viewport()->update();
    }
}

// Add, on a tab: a checklist of every scene not already in it, so a tab can be
// filled in one pass rather than one scene per trip to the menu.
void SceneOrganiserDock::showAddToTabMenu()
{
    QVector<TabNode> *nodes = editableNodesForCurrentTab();
    if (!nodes) {
        return;
    }

    QStringList candidates;
    std::function<void(QStandardItem *)> collect = [&](QStandardItem *parent) {
        for (int i = 0; i < parent->rowCount(); ++i) {
            QStandardItem *item = parent->child(i);
            if (!item) continue;
            if (item->type() == SceneTreeItem::UserType + 2) {
                const QString sceneName = item->text();
                if (!nodesContainScene(*nodes, sceneName) && !m_hiddenScenes.contains(sceneName)) {
                    candidates << sceneName;
                }
            }
            if (item->rowCount() > 0) {
                collect(item);
            }
        }
    };
    collect(m_model->invisibleRootItem());

    if (candidates.isEmpty()) {
        su::info(this,
            QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.AddScenes.Title")),
            QString::fromUtf8(obs_module_text("SceneOrganiser.Menu.AllScenesAdded")));
        return;
    }

    auto shell = su::makeWindow(QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.AddScenes.Title")),
                                "v" PROJECT_VERSION, this, /*brandFooter=*/false, "StreamUP");

    auto *listWidget = new QListWidget();
    listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (const QString &sceneName : candidates) {
        QListWidgetItem *item = new QListWidgetItem(sceneName, listWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }

    // The whole row toggles, not just the little box. Aiming for a 13px target
    // in a list of scene names is a needless bit of precision.
    QObject::connect(listWidget, &QListWidget::itemClicked, [](QListWidgetItem *item) {
        item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    });

    // Wide enough for the longest scene name, so nothing is cut off or needs
    // scrolling sideways to read. Scene names here run long by nature.
    int widest = 0;
    const QFontMetrics metrics(listWidget->font());
    for (const QString &sceneName : candidates) {
        widest = std::max(widest, metrics.horizontalAdvance(sceneName));
    }
    // Text, plus the checkbox and its spacing, plus the dialog's own margins.
    const int contentWidth = widest + su::S(90);

    shell.content->setContentsMargins(su::S(16), su::S(16), su::S(16), su::S(8));
    shell.content->addWidget(listWidget);

    auto *cancel = new su::PillButton("Cancel", "outline");
    auto *add = new su::PillButton("Add", "primary");
    shell.footerButtons->addWidget(cancel);
    shell.footerButtons->addWidget(add);

    QObject::connect(cancel, &QPushButton::clicked, shell.dialog, &QDialog::close);

    QPointer<SceneOrganiserDock> self(this);
    QObject::connect(add, &QPushButton::clicked, shell.dialog, [self, listWidget, dlg = shell.dialog]() {
        if (self) {
            // Re-resolved on accept: the dialog is modeless, so the tab showing
            // now is the tab the scenes belong in.
            if (QVector<TabNode> *target = self->editableNodesForCurrentTab()) {
                for (int i = 0; i < listWidget->count(); ++i) {
                    QListWidgetItem *item = listWidget->item(i);
                    if (item->checkState() == Qt::Checked && !nodesContainScene(*target, item->text())) {
                        TabNode node;
                        node.name = item->text();
                        target->append(node);
                    }
                }
                self->refreshQuickList();
                self->SaveConfiguration();
            }
        }
        dlg->close();
    });

    // Bounded so a very long scene name cannot throw a dialog wider than the
    // screen, and a short list still gets a sensible minimum.
    const int dialogWidth = qBound(su::S(320), contentWidth, su::S(760));
    shell.dialog->resize(dialogWidth, su::S(460));
    shell.dialog->show();
}

// Move the selected row within its own level of the current tab's tree.
// Moving between folders is a drag; this is the toolbar equivalent for ordering
// within one.
void SceneOrganiserDock::moveWithinCurrentTab(int direction)
{
    if (!m_quickTree || !m_quickModel || !editableNodesForCurrentTab()) {
        return;
    }

    const QModelIndexList selected = m_quickTree->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }

    QStandardItem *item = TabItemFromIndex(m_quickProxy, m_quickModel, selected.first());
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
        return;
    }

    QStandardItem *parent = item->parent() ? item->parent() : m_quickModel->invisibleRootItem();
    const int from = item->row();
    const int to = from + direction;
    if (to < 0 || to >= parent->rowCount()) {
        return;
    }

    // Taken out and put back rather than swapped, so a folder keeps its children.
    m_populatingQuickTree = true;
    QList<QStandardItem *> row = parent->takeRow(from);
    parent->insertRow(to, row);
    m_populatingQuickTree = false;

    m_quickTree->expandAll();
    const QModelIndex moved = m_quickProxy->mapFromSource(m_quickModel->indexFromItem(row.first()));
    m_quickTree->selectionModel()->select(moved, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_quickTree->setCurrentIndex(moved);

    commitQuickTreeToTab();
}

// ---------------------------------------------------------------------------
// Custom tab management
// ---------------------------------------------------------------------------

int SceneOrganiserDock::customTabIndexByName(const QString &name) const
{
    for (int i = 0; i < m_customTabs.size(); ++i) {
        if (m_customTabs.at(i).name == name) {
            return i;
        }
    }
    return -1;
}

// Asks for a tab name, and keeps asking if the answer is one that is already
// taken. skipIndex is the tab being renamed, which is allowed to keep its own
// name; -1 when creating. Blank is treated as a cancel.
void SceneOrganiserDock::promptForTabName(const QString &title, const QString &fieldLabel,
                                          const QString &initial, int skipIndex,
                                          std::function<void(const QString &)> onAccept)
{
    QPointer<SceneOrganiserDock> self(this);

    su::prompt(this, title, fieldLabel, initial,
        [self, title, fieldLabel, skipIndex, onAccept](const QString &entered) {
            if (!self) {
                return;
            }

            const QString name = entered.trimmed();
            if (name.isEmpty()) {
                return;
            }

            const int clash = self->customTabIndexByName(name);
            if (clash >= 0 && clash != skipIndex) {
                // Say what went wrong, then hand the typed name straight back so
                // it can be edited rather than retyped from nothing.
                su::info(self,
                    QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.TabNameTaken.Title")),
                    QString(obs_module_text("SceneOrganiser.Dialog.TabNameTaken.Text")).arg(name));

                self->promptForTabName(title, fieldLabel, name, skipIndex, onAccept);
                return;
            }

            onAccept(name);
        });
}

void SceneOrganiserDock::onCreateCustomTabClicked()
{
    QPointer<SceneOrganiserDock> self(this);

    promptForTabName(QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.NewTab.Title")),
                     QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.NewTab.Text")),
                     QString(), -1,
        [self](const QString &name) {
            if (!self) {
                return;
            }

            CustomSceneTab tab;
            tab.name = name;
            self->m_customTabs.append(tab);

            // Land the user on the tab they just made - it is empty, and its
            // toolbar is how scenes and folders get into it.
            self->m_currentKind = QuickTabKind::Custom;
            self->m_currentCustomTab = self->m_customTabs.size() - 1;

            self->rebuildTabBar();
            self->SaveConfiguration();
        });
}

void SceneOrganiserDock::onRenameCustomTabClicked(int customIndex)
{
    if (customIndex < 0 || customIndex >= m_customTabs.size()) {
        return;
    }

    QPointer<SceneOrganiserDock> self(this);
    const QString oldName = m_customTabs.at(customIndex).name;

    promptForTabName(QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.RenameTab.Title")),
                     QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.RenameTab.Text")),
                     oldName, customIndex,
        [self, customIndex, oldName](const QString &name) {
            if (!self) {
                return;
            }
            // Re-check the index: the dialog is modeless and the tab list can
            // have changed underneath it.
            if (customIndex >= self->m_customTabs.size() || self->m_customTabs.at(customIndex).name != oldName) {
                return;
            }

            self->m_customTabs[customIndex].name = name;
            // The saved order refers to tabs by name, so it has to follow.
            const int orderIndex = self->m_tabOrder.indexOf(CustomTabToken(oldName));
            if (orderIndex >= 0) {
                self->m_tabOrder[orderIndex] = CustomTabToken(name);
            }

            self->rebuildTabBar();
            self->SaveConfiguration();
        });
}

void SceneOrganiserDock::onDeleteCustomTabClicked(int customIndex)
{
    if (customIndex < 0 || customIndex >= m_customTabs.size()) {
        return;
    }

    const QString tabName = m_customTabs.at(customIndex).name;
    QPointer<SceneOrganiserDock> self(this);

    su::confirm(this,
        QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.DeleteTab.Title")),
        QString(obs_module_text("SceneOrganiser.Dialog.DeleteTab.Text")).arg(tabName),
        QString::fromUtf8(obs_module_text("SceneOrganiser.Dialog.DeleteTab.Title")),
        "danger",
        [self, tabName]() {
            if (!self) {
                return;
            }
            // Resolved by name at accept-time; the list may have moved on.
            const int index = self->customTabIndexByName(tabName);
            if (index < 0) {
                return;
            }

            self->m_customTabs.remove(index);
            self->m_tabOrder.removeAll(CustomTabToken(tabName));

            // Deleting the tab you were looking at drops you back on the tree.
            if (self->m_currentKind == QuickTabKind::Custom && self->m_currentCustomTab == index) {
                self->m_currentKind = QuickTabKind::Scenes;
                self->m_currentCustomTab = -1;
            }

            self->rebuildTabBar();
            self->SaveConfiguration();
        });
}

void SceneOrganiserDock::addSceneToCustomTab(const QString &sceneName, int customIndex)
{
    if (customIndex < 0 || customIndex >= m_customTabs.size() || sceneName.isEmpty()) {
        return;
    }
    if (nodesContainScene(m_customTabs.at(customIndex).nodes, sceneName)) {
        return;
    }

    TabNode node;
    node.name = sceneName;
    m_customTabs[customIndex].nodes.append(node);

    refreshQuickList();
    SaveConfiguration();
}

void SceneOrganiserDock::removeSceneFromCustomTab(const QString &sceneName, int customIndex)
{
    if (customIndex < 0 || customIndex >= m_customTabs.size()) {
        return;
    }

    removeSceneFromNodes(m_customTabs[customIndex].nodes, sceneName);
    refreshQuickList();
    SaveConfiguration();
}

void SceneOrganiserDock::populateAddToTabMenu()
{
    if (!m_addToTabMenu) {
        return;
    }

    m_addToTabMenu->clear();

    const QString sceneName = m_currentContextItem ? m_currentContextItem->text() : QString();

    for (int i = 0; i < m_customTabs.size(); ++i) {
        const CustomSceneTab &tab = m_customTabs.at(i);
        const bool alreadyIn = nodesContainScene(tab.nodes, sceneName);

        QAction *action = m_addToTabMenu->addAction(tab.name, this, [this, sceneName, i, alreadyIn]() {
            if (alreadyIn) {
                removeSceneFromCustomTab(sceneName, i);
            } else {
                addSceneToCustomTab(sceneName, i);
            }
        });
        // Checked means "in this tab", and clicking it again takes the scene out,
        // so one menu covers both directions.
        action->setCheckable(true);
        action->setChecked(alreadyIn);
    }

    if (!m_customTabs.isEmpty()) {
        m_addToTabMenu->addSeparator();
    }
    m_addToTabMenu->addAction(obs_module_text("SceneOrganiser.Action.NewTab"), this,
                              &SceneOrganiserDock::onCreateCustomTabClicked);
}


// ---------------------------------------------------------------------------
// Tab persistence
//
// Tab trees are nested, so they are stored as JSON rather than the flat text
// the other per-collection files use. The previous flat format is still read
// once, so tabs built before folders existed come across intact.
// ---------------------------------------------------------------------------

static obs_data_array_t *TabNodesToArray(const QVector<TabNode> &nodes)
{
    obs_data_array_t *array = obs_data_array_create();

    for (const TabNode &node : nodes) {
        obs_data_t *item = obs_data_create();
        obs_data_set_string(item, "type", node.isFolder ? "folder" : "scene");
        obs_data_set_string(item, "name", node.name.toUtf8().constData());

        if (node.isFolder) {
            obs_data_array_t *children = TabNodesToArray(node.children);
            obs_data_set_array(item, "children", children);
            obs_data_array_release(children);
        }

        obs_data_array_push_back(array, item);
        obs_data_release(item);
    }

    return array;
}

static QVector<TabNode> TabNodesFromArray(obs_data_array_t *array)
{
    QVector<TabNode> nodes;
    if (!array) {
        return nodes;
    }

    const size_t count = obs_data_array_count(array);
    for (size_t i = 0; i < count; ++i) {
        obs_data_t *item = obs_data_array_item(array, i);
        if (!item) {
            continue;
        }

        TabNode node;
        node.isFolder = (strcmp(obs_data_get_string(item, "type"), "folder") == 0);
        node.name = QString::fromUtf8(obs_data_get_string(item, "name"));

        if (node.isFolder) {
            obs_data_array_t *children = obs_data_get_array(item, "children");
            node.children = TabNodesFromArray(children);
            obs_data_array_release(children);
        }

        if (!node.name.isEmpty()) {
            nodes << node;
        }
        obs_data_release(item);
    }

    return nodes;
}

void SceneOrganiserDock::saveQuickTabs(const QString &configDir, const QString &sceneCollectionName)
{
    const QString path = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_quick_tabs.json";

    obs_data_t *root = obs_data_create();
    obs_data_set_string(root, "current_tab", currentTabToken().toUtf8().constData());

    obs_data_array_t *order = obs_data_array_create();
    for (const QString &token : m_tabOrder) {
        obs_data_t *entry = obs_data_create();
        obs_data_set_string(entry, "token", token.toUtf8().constData());
        obs_data_array_push_back(order, entry);
        obs_data_release(entry);
    }
    obs_data_set_array(root, "order", order);
    obs_data_array_release(order);

    obs_data_array_t *favourites = TabNodesToArray(m_favouriteNodes);
    obs_data_set_array(root, "favourites", favourites);
    obs_data_array_release(favourites);

    obs_data_array_t *recent = obs_data_array_create();
    for (const QString &sceneName : m_recentScenes) {
        obs_data_t *entry = obs_data_create();
        obs_data_set_string(entry, "name", sceneName.toUtf8().constData());
        obs_data_array_push_back(recent, entry);
        obs_data_release(entry);
    }
    obs_data_set_array(root, "recent", recent);
    obs_data_array_release(recent);

    obs_data_array_t *tabs = obs_data_array_create();
    for (const CustomSceneTab &tab : m_customTabs) {
        obs_data_t *entry = obs_data_create();
        obs_data_set_string(entry, "name", tab.name.toUtf8().constData());

        obs_data_array_t *nodes = TabNodesToArray(tab.nodes);
        obs_data_set_array(entry, "nodes", nodes);
        obs_data_array_release(nodes);

        obs_data_array_push_back(tabs, entry);
        obs_data_release(entry);
    }
    obs_data_set_array(root, "tabs", tabs);
    obs_data_array_release(tabs);

    obs_data_save_json(root, path.toUtf8().constData());
    obs_data_release(root);
}

void SceneOrganiserDock::loadQuickTabs(const QString &configDir, const QString &sceneCollectionName)
{
    m_favouriteNodes.clear();
    m_recentScenes.clear();
    m_customTabs.clear();
    m_tabOrder.clear();

    const QString path = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_quick_tabs.json";

    obs_data_t *root = obs_data_create_from_json_file(path.toUtf8().constData());
    if (!root) {
        // Nothing in the new format: try the flat file this replaced, so tabs
        // made before folders existed are carried over rather than lost. It is
        // read once and then written back as JSON by the next save.
        const QString legacyPath = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_quick_tabs.txt";
        const bool migrated = loadLegacyQuickTabs(legacyPath);

        StreamUP::DebugLogger::LogInfo("SceneOrganiser",
            QString("No tab JSON at '%1' - legacy file %2")
                .arg(path, migrated ? "migrated" : "not found")
                .toUtf8()
                .constData());
        return;
    }

    m_currentKindToken = QString::fromUtf8(obs_data_get_string(root, "current_tab"));

    obs_data_array_t *order = obs_data_get_array(root, "order");
    if (order) {
        const size_t count = obs_data_array_count(order);
        for (size_t i = 0; i < count; ++i) {
            obs_data_t *entry = obs_data_array_item(order, i);
            if (entry) {
                m_tabOrder << QString::fromUtf8(obs_data_get_string(entry, "token"));
                obs_data_release(entry);
            }
        }
        obs_data_array_release(order);
    }

    obs_data_array_t *favourites = obs_data_get_array(root, "favourites");
    m_favouriteNodes = TabNodesFromArray(favourites);
    obs_data_array_release(favourites);

    obs_data_array_t *recent = obs_data_get_array(root, "recent");
    if (recent) {
        const size_t count = obs_data_array_count(recent);
        for (size_t i = 0; i < count; ++i) {
            obs_data_t *entry = obs_data_array_item(recent, i);
            if (entry) {
                m_recentScenes << QString::fromUtf8(obs_data_get_string(entry, "name"));
                obs_data_release(entry);
            }
        }
        obs_data_array_release(recent);
    }

    obs_data_array_t *tabs = obs_data_get_array(root, "tabs");
    if (tabs) {
        const size_t count = obs_data_array_count(tabs);
        for (size_t i = 0; i < count; ++i) {
            obs_data_t *entry = obs_data_array_item(tabs, i);
            if (!entry) {
                continue;
            }

            CustomSceneTab tab;
            tab.name = QString::fromUtf8(obs_data_get_string(entry, "name"));

            obs_data_array_t *nodes = obs_data_get_array(entry, "nodes");
            tab.nodes = TabNodesFromArray(nodes);
            obs_data_array_release(nodes);

            if (!tab.name.isEmpty()) {
                m_customTabs << tab;
            }
            obs_data_release(entry);
        }
        obs_data_array_release(tabs);
    }

    obs_data_release(root);

    // Restore the selected tab from its token, now that the tabs it may name
    // are loaded.
    m_currentKind = QuickTabKind::Scenes;
    m_currentCustomTab = -1;
    if (m_currentKindToken == QLatin1String(kTabTokenFavourites)) {
        m_currentKind = QuickTabKind::Favourites;
    } else if (m_currentKindToken == QLatin1String(kTabTokenRecent)) {
        m_currentKind = QuickTabKind::Recent;
    } else if (m_currentKindToken.startsWith(QLatin1String(kTabTokenCustomPrefix))) {
        const int index = customTabIndexByName(m_currentKindToken.mid(int(strlen(kTabTokenCustomPrefix))));
        if (index >= 0) {
            m_currentKind = QuickTabKind::Custom;
            m_currentCustomTab = index;
        }
    }

    StreamUP::DebugLogger::LogInfo("SceneOrganiser",
        QString("Loaded tabs from '%1': %2 custom, %3 favourite entries, %4 recent, order [%5]")
            .arg(path)
            .arg(m_customTabs.size())
            .arg(m_favouriteNodes.size())
            .arg(m_recentScenes.size())
            .arg(m_tabOrder.join(", "))
            .toUtf8()
            .constData());
}

bool SceneOrganiserDock::loadLegacyQuickTabs(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    bool headerRead = false;
    QStringList *plainTarget = nullptr;   // recent, or the order list
    QVector<TabNode> *nodeTarget = nullptr;

    while (!in.atEnd()) {
        const QString trimmed = in.readLine().trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        if (!headerRead) {
            headerRead = true;
            continue; // the old header held a visibility flag and tab index
        }

        if (trimmed == "[order]") {
            plainTarget = &m_tabOrder;
            nodeTarget = nullptr;
        } else if (trimmed == "[favourites]") {
            plainTarget = nullptr;
            nodeTarget = &m_favouriteNodes;
        } else if (trimmed == "[recent]") {
            plainTarget = &m_recentScenes;
            nodeTarget = nullptr;
        } else if (trimmed.startsWith("[tab]")) {
            CustomSceneTab tab;
            tab.name = trimmed.mid(5);
            m_customTabs.append(tab);
            plainTarget = nullptr;
            nodeTarget = &m_customTabs.last().nodes;
        } else if (plainTarget) {
            plainTarget->append(trimmed);
        } else if (nodeTarget) {
            // Everything in the old format was a flat scene entry.
            TabNode node;
            node.name = trimmed;
            nodeTarget->append(node);
        }
    }

    file.close();
    return true;
}

bool SceneOrganiserDock::isFavourite(const QString &sceneName) const
{
    return nodesContainScene(m_favouriteNodes, sceneName);
}

void SceneOrganiserDock::noteRecentScene(const QString &sceneName)
{
    if (sceneName.isEmpty() || (!m_recentScenes.isEmpty() && m_recentScenes.first() == sceneName)) {
        return;
    }

    m_recentScenes.removeAll(sceneName);
    m_recentScenes.prepend(sceneName);
    while (m_recentScenes.size() > kMaxRecentScenes) {
        m_recentScenes.removeLast();
    }

    refreshQuickList();
    m_saveTimer->start();
}

void SceneOrganiserDock::onToggleFavouriteClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) {
        return;
    }

    const QString sceneName = m_currentContextItem->text();
    if (isFavourite(sceneName)) {
        removeSceneFromNodes(m_favouriteNodes, sceneName);
    } else {
        // New favourites land at the top level; from there they can be filed
        // into whatever folders the Favourites tab has.
        TabNode node;
        node.name = sceneName;
        m_favouriteNodes.append(node);
    }

    refreshQuickList();
    SaveConfiguration();
}

//==============================================================================
// Item Classes Implementation
//==============================================================================

SceneFolderItem::SceneFolderItem(const QString &folderName)
    : QStandardItem(folderName)
{
    setupFolderItem();
    // Set creation timestamp to current time
    setCreationTimestamp(QDateTime::currentMSecsSinceEpoch());
}

void SceneFolderItem::setupFolderItem()
{
    // NO custom styling - pure OBS theme
    updateIcon();
    setDropEnabled(true);
    setDragEnabled(true);

    // The same marker the tab trees put on their folders, so the guide painting
    // can ask one question of either tree rather than knowing about item types.
    setData(true, TabItemIsFolderRole);
}

void SceneFolderItem::updateIcon()
{
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    if (!settings.sceneOrganiserShowIcons) {
        setIcon(QIcon());
        return;
    }

    // A custom icon wins; an unresolvable one (deleted file, property a theme
    // does not define) falls back to the default rather than showing nothing.
    // The default is tinted too, so a colour can be set without picking an icon.
    const QColor tint = data(CustomIconColorRole).value<QColor>();
    const QIcon custom = ResolveIconSpec(data(CustomIconRole).toString(), tint);
    if (!custom.isNull()) {
        // A folder given an icon of its own keeps it in both states. Picking an
        // icon deliberately and then having it change underneath you would be
        // the wrong kind of clever.
        setIcon(custom);
        return;
    }

    // Otherwise the icon follows the folder: open when the row is expanded,
    // closed when it is not.
    QColor iconTint = tint;
    if (!iconTint.isValid()) {
        // Follows the theme rather than being fixed white, since the shipped
        // folder icons are flat single-colour shapes.
        iconTint = QApplication::palette().color(QPalette::Text);
    }
    setIcon(GetFolderIcon(data(FolderExpandedRole).toBool(), iconTint));
}

qint64 SceneFolderItem::getCreationTimestamp() const
{
    return data(Qt::UserRole + 100).toLongLong();
}

void SceneFolderItem::setCreationTimestamp(qint64 timestamp)
{
    setData(timestamp, Qt::UserRole + 100);
}

SceneTreeItem::SceneTreeItem(const QString &sceneName, obs_weak_source_t *weak_source)
    : QStandardItem(sceneName), m_weakSource(weak_source)
{
    setupSceneItem();
    updateFromObs();
    // Set creation timestamp to current time
    setCreationTimestamp(QDateTime::currentMSecsSinceEpoch());
}

SceneTreeItem::~SceneTreeItem()
{
    // Release the weak source reference when this item is destroyed
    if (m_weakSource) {
        obs_weak_source_release(m_weakSource);
        m_weakSource = nullptr;
    }
}

void SceneTreeItem::setupSceneItem()
{
    // NO custom styling - pure OBS theme
    updateIcon();
    setDropEnabled(false);
    setDragEnabled(true);
}

void SceneTreeItem::updateIcon()
{
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    if (settings.sceneOrganiserShowIcons) {
        // A custom icon wins; an unresolvable one falls back to the default.
        // The default is tinted too, so a colour can be set on its own.
        const QColor tint = data(CustomIconColorRole).value<QColor>();
        const QIcon custom = ResolveIconSpec(data(CustomIconRole).toString(), tint);
        if (!custom.isNull()) {
            setIcon(custom);
        } else {
            const QIcon base = GetThemeIcon("sceneIcon");
            setIcon(tint.isValid() ? TintIcon(base, tint) : base);
        }

        // Could add special styling for current scene in the future
        // obs_source_t *current_scene = Canvas::GetCurrentScene(m_canvasType);
        // obs_source_t *this_scene = obs_weak_source_get_source(m_weakSource);
        // if (current_scene && this_scene && obs_source_get_ref(current_scene) == obs_source_get_ref(this_scene)) {
        //     // Current scene - could use different color or styling
        // }
        // if (current_scene) obs_source_release(current_scene);
        // if (this_scene) obs_source_release(this_scene);
    } else {
        setIcon(QIcon());
    }
}

qint64 SceneTreeItem::getCreationTimestamp() const
{
    return data(Qt::UserRole + 100).toLongLong();
}

void SceneTreeItem::setCreationTimestamp(qint64 timestamp)
{
    setData(timestamp, Qt::UserRole + 100);
}

void SceneTreeItem::updateFromObs()
{
    // Update icon in case current scene changed
    updateIcon();

    // Get scene info from OBS and update display
    obs_source_t *source = obs_get_source_by_name(text().toUtf8().constData());
    if (source) {
        // Could show additional info like scene item count, etc.
        obs_source_release(source);
    }
}

} // namespace SceneOrganiser
} // namespace StreamUP

//==============================================================================
// Tree View Helper Methods
//==============================================================================

void StreamUP::SceneOrganiser::SceneOrganiserDock::forceTreeViewRepaint()
{
    if (m_treeView) {
        m_treeView->viewport()->repaint();
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Repaint",
            "Forced tree view repaint after drag and drop");
    }
}

void StreamUP::SceneOrganiser::SceneOrganiserDock::scheduleOptimizedUpdate()
{
    if (!m_updatesPending) {
        m_updatesPending = true;
        m_updateBatchTimer->start();
    }
}

void StreamUP::SceneOrganiser::SceneOrganiserDock::processBatchedUpdates()
{
    if (!m_updatesPending) {
        return;
    }

    m_updatesPending = false;

    // Perform efficient batched updates
    if (m_treeView) {
        // Update the viewport instead of forcing a full repaint
        m_treeView->viewport()->update();
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Batch Update", "Processed batched tree view update");
    }
}

void StreamUP::SceneOrganiser::SceneOrganiserDock::clearIconCaches()
{
    ClearIconCaches();
}

void StreamUP::SceneOrganiser::SceneOrganiserDock::applyThemeRowStyling()
{
    // The colour a translucent row overlay has to be flattened against: the
    // view's own background, which is what the theme paints behind the rows.
    const QColor backdrop = m_treeView ? m_treeView->palette().color(QPalette::Base)
                                       : palette().color(QPalette::Base);
    const QString rowQss = BuildThemeRowQss(backdrop);

    // Set on the views themselves, so the rules reach nothing but our rows.
    for (QAbstractItemView *view : {static_cast<QAbstractItemView *>(m_treeView),
                                    static_cast<QAbstractItemView *>(m_quickTree)}) {
        if (view) {
            view->setStyleSheet(rowQss);
        }
    }
}


void StreamUP::SceneOrganiser::SceneOrganiserDock::onThemeChanged()
{
    // Clear caches and notify all dock instances to refresh
    OnThemeChanged();

    // Update all dock instances efficiently
    for (auto* dock : s_dockInstances) {
        if (dock && dock->m_model) {
            // Update theme state
            dock->currentThemeIsDark = StreamUP::UIHelpers::IsOBSThemeDark();
            // Update all icons with the new theme
            dock->updateAllItemIcons(dock->m_model->invisibleRootItem());
            // Re-lift the new theme's row rules (hover/selection) onto our views
            dock->applyThemeRowStyling();
            // Schedule viewport repaint
            dock->scheduleOptimizedUpdate();
        }
    }
}

//==============================================================================
// CustomColorDelegate Implementation
//==============================================================================

StreamUP::SceneOrganiser::CustomColorDelegate::CustomColorDelegate(SceneOrganiserDock *dock, QObject *parent,
                                                                   QSortFilterProxyModel *proxy,
                                                                   QStandardItemModel *model)
    : QStyledItemDelegate(parent), m_dock(dock), m_proxy(proxy), m_model(model)
{
}

void StreamUP::SceneOrganiser::CustomColorDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid() || !m_dock) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    // Whichever tree this delegate was built for.
    QStandardItemModel *model = m_model ? m_model : qobject_cast<QStandardItemModel*>(m_dock->m_model);
    QSortFilterProxyModel *proxy = m_proxy ? m_proxy : m_dock->m_proxyModel;
    if (!model) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    // Map from proxy model to source model if needed
    QModelIndex sourceIndex = index;
    if (proxy) {
        sourceIndex = proxy->mapToSource(index);
    }

    QStandardItem *item = model->itemFromIndex(sourceIndex);
    if (!item) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    // Is this the LIVE program scene? It is marked with the theme's own
    // selection colour, not a colour of ours, so the dock reads the same way as
    // the Scenes and Sources docks beside it in whatever theme is loaded.
    const bool isProgram = item->data(ProgramSceneRole).toBool();

    // Check if this item has a custom color
    QVariant colorData = item->data(Qt::UserRole + 1);
    QColor customColor = colorData.isValid() ? colorData.value<QColor>() : QColor();

    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isHovered = option.state & QStyle::State_MouseOver;

    if (!customColor.isValid()) {
        // No colour of our own to apply, so we do not paint at all - selection
        // and hover come from the active OBS theme. The live scene borrows the
        // theme's selected look by asking for it, rather than by us guessing at
        // a colour: painting our own pill here made the dock the odd one out in
        // every theme, which is what users saw.
        QStyleOptionViewItem themedOption = option;
        if (isProgram) {
            themedOption.state |= QStyle::State_Selected;
        }
        QStyledItemDelegate::paint(painter, themedOption, index);
        if (isProgram && ProgramRowNeedsOutline(option, index)) {
            DrawProgramOutline(painter, option.rect, option.palette.color(QPalette::HighlightedText));
        }
        return;
    }

    // From here the row has a colour the user set by hand, which is the only
    // case we paint ourselves.
    QColor bgColor = customColor;
    if (isSelected || isProgram) {
        bgColor = m_dock->getSelectionColor(customColor);
    } else if (isHovered) {
        bgColor = m_dock->getHoverColor(customColor);
    }

    // A plain row that is only selected/hovered has no base colour of its own,
    // so the two helpers above hand back the theme's palette Highlight. That is
    // the theme-agnostic part: a theme that only styles OBS' own SceneTree (by
    // class name, which a QTreeView in our namespace can never match) used to
    // leave this row painted by whatever generic rule it happened to have -
    // near-black in some themes, with no hover at all. We own the fill now.
    if (!bgColor.isValid()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    bgColor = m_dock->ensureRowContrast(bgColor);

    // Calculate contrasting text color
    QColor textColor = m_dock->getContrastTextColor(bgColor);

    // Save painter state
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Draw rounded rectangle background with custom color
    QRect rect = option.rect;
    // Add some padding to match theme styling
    rect.adjust(2, 1, -2, -1);

    // Draw rounded rectangle (radius 4 matches most OBS themes)
    painter->setPen(Qt::NoPen);
    painter->setBrush(bgColor);
    painter->drawRoundedRect(rect, 4, 4);

    if (isProgram && ProgramRowNeedsOutline(option, index)) {
        DrawProgramOutline(painter, option.rect, textColor);
    }

    // No selection outline on a merely selected row. One was tried here, drawn in the contrast colour so
    // it would read on light and dark rows alike, but on a bright row that means
    // a dark ring, which looks exactly like the row has been painted twice —
    // the very artefact this delegate exists to avoid. Selection is carried by
    // the brightened fill alone.

    painter->restore();

    // Now let the base class paint the content (icon, text) with custom text color
    QStyleOptionViewItem modifiedOption = option;
    modifiedOption.palette.setColor(QPalette::Text, textColor);
    modifiedOption.palette.setColor(QPalette::HighlightedText, textColor);

    // Tell the style not to draw the background (we already did it)
    modifiedOption.backgroundBrush = QBrush(Qt::NoBrush);

    // Draw without focus rect to avoid visual artifacts
    modifiedOption.state &= ~QStyle::State_HasFocus;

    // The background above is the finished article: bgColor already accounts for
    // selection and hover. Leaving those flags set makes the style paint the
    // theme's own highlight over the top, which washes a coloured row out into a
    // highlight-tinted blend and draws the theme's selection border around it.
    // Clearing them is what keeps a selected custom colour recognisably itself.
    modifiedOption.state &= ~QStyle::State_Selected;
    modifiedOption.state &= ~QStyle::State_MouseOver;

    // With State_Selected gone the style reads text from the Text role rather
    // than HighlightedText, so both are already pointed at the contrast colour.
    QStyledItemDelegate::paint(painter, modifiedOption, index);
}

QSize StreamUP::SceneOrganiser::CustomColorDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Enforce an explicit, absolute row height (in pixels) from the setting so the
    // whole row scales - not just the icon. Without this the row height is only the
    // implicit max(icon, text) height, which stops tracking once the icon dominates.
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(m_dock ? m_dock->currentRowHeight() : 24);
    return size;
}

#include "scene-organiser-dock.moc"
