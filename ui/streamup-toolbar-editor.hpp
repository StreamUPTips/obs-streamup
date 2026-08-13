#pragma once

// The toolbar, in editable form.
//
// There is exactly one of these and it is the real run: when edit mode is on,
// the toolbar swaps its live content for this and swaps back when you finish.
// Nothing is previewed or mirrored, so there is no second copy to disagree with
// the first, which is what made the last attempt so hard to pin down.
//
// The buttons it lays out are inert lookalikes with no connections, so a drag
// cannot start a stream. All interaction is handled here rather than by the
// children, so there is no route for a press to reach a button at all.
//
// Every measurement goes through ToolbarGeom::Axis, so horizontal and vertical
// run the same code.

#include <QList>
#include <QStringList>
#include <QVBoxLayout>
#include <QRect>
#include <QString>
#include <QWidget>
#include <memory>

#include "streamup-toolbar-config.hpp"
#include "streamup-toolbar-geometry.hpp"
#include "settings-manager.hpp"

namespace StreamUP {

// Mime type for a drag out of the palette. Payload is a descriptor string,
// see ToolbarEditor::createItemFromDescriptor().
extern const char *const kToolbarPaletteMimeType;

class ToolbarEditor : public QWidget {
	Q_OBJECT

public:
	explicit ToolbarEditor(QWidget *parent = nullptr);

	// The configuration being edited, mutated in place. Not owned.
	void setConfiguration(ToolbarConfig::ToolbarConfiguration *config);
	void setAxis(const ToolbarGeom::Axis &axis);
	void setAlignment(SettingsManager::ToolbarAlignment alignment);

	void rebuild();

	QString selectedItemId() const { return selectedId_; }
	void setSelectedItemId(const QString &id);
	std::shared_ptr<ToolbarConfig::ToolbarItem> selectedItem() const;

	// Insert a new item built from a palette descriptor. Index -1 appends.
	// Returns the new item's id, or an empty string if the descriptor was not
	// understood.
	QString insertFromDescriptor(const QString &descriptor, int configIndex = -1);
	void removeItem(const QString &id);

	// Descriptors the palette can offer.
	static QStringList paletteDescriptors();
	static std::shared_ptr<ToolbarConfig::ToolbarItem> createItemFromDescriptor(const QString &descriptor);

signals:
	void itemSelected(const QString &itemId);
	// Something about the configuration changed. The owner decides when to
	// save, so a drag is not a write per pixel.
	void configurationChanged();

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	struct Slot {
		QString id;
		ToolbarConfig::ItemType type = ToolbarConfig::ItemType::Button;
		// What is drawn.
		QRect rect;
		// What can be clicked. A separator is a hairline, so its visual rect
		// is far too thin to hit. Widened along the flow for those.
		QRect hitRect;
	};

	void captureSlots();
	void resizeSpacerLive(const QString &id, int size);
	int slotAt(const QPoint &pos) const;
	int insertionIndexAt(const QPoint &pos) const;
	QRect insertionMarkerRect(int index) const;
	bool isSpacerGrip(int slotIndex, const QPoint &pos) const;
	// Config index for a drop landing in gap `slotGap`.
	int configIndexForGap(int slotGap) const;

	ToolbarConfig::ToolbarConfiguration *config_ = nullptr;
	ToolbarGeom::Axis axis_{false};
	SettingsManager::ToolbarAlignment alignment_ = SettingsManager::ToolbarAlignment::Start;

	QVBoxLayout *hostLayout_ = nullptr;
	QWidget *content_ = nullptr;
	QList<Slot> slots_;

	QString selectedId_;
	int hoverSlot_ = -1;

	bool pressed_ = false;
	QPoint pressPos_;
	int pressSlot_ = -1;
	bool dragging_ = false;
	bool draggingOut_ = false;
	int dropGap_ = -1;

	// Spacer resize. Held by id: a resize re-lays the run out underneath the
	// drag, so any index captured at press time goes stale immediately.
	bool resizingSpacer_ = false;
	QString resizeId_;
	int resizeStartSize_ = 0;

	bool paletteDragActive_ = false;
};

} // namespace StreamUP
