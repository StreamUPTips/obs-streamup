#pragma once

// Companion panel for toolbar edit mode.
//
// The toolbar itself is the editing surface (see StreamUP::ToolbarEditor); this
// floats beside it and carries the two things that cannot live on a toolbar: the
// palette you drag new elements from, and the settings for whatever is currently
// selected on the bar.
//
// It is a Qt::Tool so it stays above OBS without stealing activation, which
// matters because a drag out of the palette has to start while the toolbar
// underneath is still the thing being edited.
//
// The panel never writes the configuration to disk and never owns it. It mutates
// the selected item through the editor and then reports configurationChanged();
// deciding when to save is the owner's business, so a spacer resize drag is not
// a write per pixel.

#include <QPointer>
#include <QWidget>
#include <memory>

#include "streamup-toolbar-config.hpp"

class QCheckBox;
class QLabel;
class QTreeWidget;
class QVBoxLayout;

namespace StreamUP {

class ToolbarEditor;

namespace UIStyles {
class MacSpinBox;
}

class ToolbarEditPanel : public QWidget {
	Q_OBJECT

public:
	explicit ToolbarEditPanel(ToolbarEditor *editor, QWidget *parent = nullptr);

signals:
	// Something in the configuration changed by way of this panel. Same contract
	// as the editor's signal of the same name: the owner decides when to save.
	void configurationChanged();

	void resetRequested();
	void doneRequested();

	// A button was configured in a dialog and needs adding to the configuration.
	//
	// Every other palette entry goes in through ToolbarEditor::insertFromDescriptor,
	// but a hotkey button is not describable as a descriptor string: it only
	// exists once the user has picked a hotkey, an icon and a label in
	// HotkeyButtonConfigDialog. The editor deliberately exposes no raw "add this
	// item" call, and the panel does not hold the configuration pointer, so the
	// finished item is handed to the owner instead. THE OWNER MUST append it to
	// the configuration and call ToolbarEditor::rebuild(); nothing appears on the
	// toolbar otherwise.
	void itemCreated(std::shared_ptr<StreamUP::ToolbarConfig::ToolbarItem> item);

private:
	void buildUi();
	void populatePalette();
	void onSelectionChanged(const QString &itemId);
	void showHotkeyDialog();
	void showWebSocketDialog();

	QPointer<ToolbarEditor> editor_;

	QTreeWidget *palette_ = nullptr;

	QWidget *properties_ = nullptr;
	QVBoxLayout *propertiesLayout_ = nullptr;
	QLabel *propertiesIcon_ = nullptr;
	QLabel *propertiesTitle_ = nullptr;
	QLabel *propertiesHint_ = nullptr;
	QWidget *propertiesBody_ = nullptr;
	QCheckBox *visibleCheck_ = nullptr;
	QWidget *sizeRow_ = nullptr;
	QLabel *sizeLabel_ = nullptr;
	UIStyles::MacSpinBox *sizeSpin_ = nullptr;
	QCheckBox *flexibleCheck_ = nullptr;
	QCheckBox *showIconCheck_ = nullptr;
	QCheckBox *showHoursCheck_ = nullptr;
	QWidget *removeRow_ = nullptr;

	QString currentId_;
};

} // namespace StreamUP
