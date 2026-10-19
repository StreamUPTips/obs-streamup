#pragma once

// StreamUP Mixer: an audio mixer dock with user-made tabs.
//
// Each tab holds an ordered list of audio sources the user picked, arranged by
// drag and drop. A source can sit in more than one tab. Anything not in any tab
// shows in the "Unassigned" tab, so a new source never goes missing; that tab
// is worked out on the fly and can be moved but not renamed or deleted.
//
// The layout is saved inside the scene collection (one per collection) as a
// JSON string. Sources are tracked by UUID, so renames are followed for free.

#include "mixer-native-widgets.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <QFrame>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTabBar>
#include <QTimer>
#include <QVector>

class QBoxLayout;
class QLabel;
class QMenu;
class QScrollArea;
class QStackedWidget;
class QToolButton;

namespace StreamUP {
namespace Mixer {

class MixerStrip;

struct MixerTab {
	QString name;
	QStringList sources; // source UUIDs, in display order
	bool unassigned = false;
};

// The drag payload: which source, and which tab it was dragged out of.
struct StripDrag {
	QString uuid;
	int fromTab = -1;
	bool valid = false;
};

class MixerTabBar : public QTabBar {
	Q_OBJECT

public:
	explicit MixerTabBar(QWidget *parent = nullptr);

signals:
	void stripDroppedOnTab(int tabIndex, const QString &uuid, int fromTab, bool copy);

protected:
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void dropEvent(QDropEvent *event) override;

private:
	QTimer m_hoverSwitch;
	int m_hoverTab = -1;
};

class StripArea : public QWidget {
	Q_OBJECT

public:
	explicit StripArea(QWidget *parent = nullptr);

	void setVertical(bool vertical);
	QBoxLayout *stripLayout() const { return m_layout; }
	void setStrips(const QList<MixerStrip *> &strips) { m_strips = strips; }

signals:
	void stripDropped(int index, const QString &uuid, int fromTab, bool copy);

protected:
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void dropEvent(QDropEvent *event) override;

private:
	int dropIndexAt(const QPoint &pos) const;
	void showIndicator(int index);

	QBoxLayout *m_layout = nullptr;
	QWidget *m_indicator = nullptr;
	QList<MixerStrip *> m_strips;
	bool m_vertical = true;
};

// An AudioMixer by type name, so theme rules scoped to OBS's mixer (such as
// "AudioMixer QToolBar") reach this dock too.
class MixerDock : public ::AudioMixer {
	Q_OBJECT

public:
	explicit MixerDock(QWidget *parent = nullptr);
	~MixerDock() override;

	static MixerDock *instance();

	// Scene collection save/load (called from the frontend save callback).
	void saveTo(obs_data_t *moduleData);
	void loadFrom(obs_data_t *moduleData);

private slots:
	// OBS's mixer announces pin, hide and lock changes with this signal.
	void nativeMixerStatusChanged(QString uuid);

private:
	static void onFrontendEvent(enum obs_frontend_event event, void *data);
	static void onSourceCreate(void *data, calldata_t *cd);
	static void onSourceRemove(void *data, calldata_t *cd);

	void handleFrontendEvent(enum obs_frontend_event event);
	void connectGlobalSignals();
	void disconnectGlobalSignals();

	// Model
	QStringList allAudioSources() const;
	QStringList unassignedSources() const;
	QStringList sourcesForTab(int index) const;
	int unassignedIndex() const;
	bool isRegularTab(int index) const;
	void makeDefaultLayout();
	bool parseLayout(const QString &json);
	QString serialiseLayout() const;
	void ensureUnassignedTab();
	void pruneMissing();
	void applyPendingLoad();
	void layoutChanged(bool rebuild = true);

	// Edits
	void addTab(const QString &name);
	void renameTab(int index, const QString &name);
	void deleteTab(int index);
	void addToTab(int index, const QString &uuid, int position = -1);
	void removeFromTab(int index, const QString &uuid);
	void moveStrip(const QString &uuid, int fromTab, int toTab, int position, bool copy);
	void sourceRemoved(const QString &uuid);
	void sourceCreated(const QString &uuid);

	// UI
	void buildUi();
	void rebuildTabBar();
	void updateTabTexts();
	// Builds every strip of the open tab from scratch: for a tab switch,
	// an orientation or theme change, or a fresh load.
	void rebuildStrips();
	// Brings the open tab's strips in line with the model, reusing the
	// widgets already there. Every edit (drops, menus, removals) goes through
	// this, so a reorder only moves widgets and builds nothing.
	void syncStrips();
	MixerStrip *createStrip(const QString &uuid);
	void retireStrip(MixerStrip *strip);
	void layoutStrips();
	void clearStrips();
	void flushGraveyard();
	void startStripDrag(MixerStrip *strip);
	void showStripMenu(MixerStrip *strip, const QPoint &globalPos);
	void showTabMenu(const QPoint &pos);
	void showOptionsMenu();
	void showAddMenu();
	void promptNewTab(const QString &uuidToMove = QString(), int fromTab = -1);
	void promptRenameTab(int index);
	void confirmDeleteTab(int index);
	void setTabHiddenInNativeMixer(int index, bool hidden);
	void setVerticalStrips(bool vertical);
	void scheduleSave();
	void refreshTheme();
	void updatePreviewFlags();

	QVector<MixerTab> m_tabs;
	int m_current = 0;
	bool m_vertical = true;

	// Frozen while OBS loads, switches or closes a collection, when source
	// removals are OBS tearing down, not the user deleting sources.
	bool m_frozen = true;
	bool m_hasPendingLoad = true;
	QString m_pendingJson;
	bool m_signalsConnected = false;

	MixerTabBar *m_tabBar = nullptr;
	QStackedWidget *m_stack = nullptr;
	QScrollArea *m_scroll = nullptr;
	StripArea *m_area = nullptr;
	QLabel *m_empty = nullptr;
	QToolButton *m_addButton = nullptr;
	QToolButton *m_optionsButton = nullptr;
	// The open tab's strips, in display order.
	QList<MixerStrip *> m_strips;
	// Every strip built so far, by source UUID, shown or not. Switching tab
	// or moving a source between tabs reuses these rather than building new
	// ones. Each holds a source reference, so it is deleted as soon as its
	// source is removed or the collection unloads.
	QHash<QString, MixerStrip *> m_pool;
	bool m_updatingTabBar = false;

	// Strips replaced while a drag or a strip's context menu is running. That
	// nested event loop is running inside one of them, so they are deleted
	// once it has returned.
	int m_deferDeletes = 0;
	bool m_dragActive = false;
	QList<QPointer<MixerStrip>> m_graveyard;

	QTimer m_saveTimer;
};

// Creates the dock and hooks it into the frontend. Call from obs_module_load.
void RegisterMixerDock();

} // namespace Mixer
} // namespace StreamUP
