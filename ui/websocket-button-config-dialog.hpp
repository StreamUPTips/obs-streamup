#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QString>
#include <QVector>
#include <memory>
#include "streamup-toolbar-config.hpp"
#include <streamup/ui/window-chrome.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/search-field.hpp>
#include <streamup/ui/segmented-control.hpp>
#include <streamup/ui/ios-checkbox.hpp>

namespace StreamUP {

// Configures a toolbar button that fires an obs-websocket request. Same shape as
// HotkeyButtonConfigDialog: pick a target, name it, choose an icon, hand back an item.
class WebSocketButtonConfigDialog : public StreamUP::UIStyles::ShadowDialog {
	Q_OBJECT

public:
	explicit WebSocketButtonConfigDialog(QWidget *parent = nullptr);
	explicit WebSocketButtonConfigDialog(std::shared_ptr<StreamUP::ToolbarConfig::WebSocketButtonItem> existingItem,
					     QWidget *parent = nullptr);

	// Get the configured websocket button item
	std::shared_ptr<StreamUP::ToolbarConfig::WebSocketButtonItem> getWebSocketButtonItem() const;

private slots:
	void onSelectIconClicked();
	void validateInput();

private:
	// How an argument is presented. The name-flavoured kinds are populated from
	// OBS so the user picks rather than spells a scene or source name wrong.
	enum class ArgKind { Text, Number, Decimal, Boolean, SceneName, SourceName, InputName, TransitionName };

	struct ArgField {
		QString key;
		ArgKind kind;
		QWidget *editor;
	};

	void setupUI();
	void setupRequestSection();
	void setupArgumentsSection();
	void setupIconSection();
	void setupCustomisationSection();

	void onSourceChanged(int index);
	void onRequestChanged();
	void onRawJsonToggled(bool raw);

	void rebuildArgumentFields();
	void clearArgumentFields();
	QWidget *makeArgEditor(ArgKind kind);
	QJsonObject collectArgumentFields() const;
	void applyJsonToFields(const QJsonObject &obj);

	QString currentRequestType() const;
	QString currentRequestData() const;
	bool jsonIsUsable(QString *errorOut) const;

	void updateIconDisplay();
	void setExistingItem(std::shared_ptr<StreamUP::ToolbarConfig::WebSocketButtonItem> item);

	static void filterList(QListWidget *list, const QString &needle);

	// Chrome
	QVBoxLayout *mainLayout;    // chrome.content
	QHBoxLayout *footerButtons; // chrome.footerButtons (right-anchored action slot)

	// Request selection
	StreamUP::UIStyles::SegmentedControl *sourceSelector;
	QStackedWidget *requestStack;
	StreamUP::UIStyles::SearchField *obsSearch;
	QListWidget *obsRequestList;
	StreamUP::UIStyles::SearchField *streamUPSearch;
	QListWidget *streamUPRequestList;
	QLineEdit *vendorNameEdit;
	QLineEdit *vendorRequestEdit;

	// Arguments
	QWidget *argsGroup;
	QFormLayout *argsForm;
	StreamUP::UIStyles::IOSCheckBox *rawJsonCheck;
	QPlainTextEdit *jsonEdit;
	QLabel *jsonErrorLabel;
	QLabel *noArgumentsLabel;
	QVector<ArgField> argFields;

	// Icon
	QLabel *iconPreview;
	StreamUP::UIStyles::PillButton *selectIconButton;

	// Customisation
	QLineEdit *displayNameEdit;
	QLineEdit *tooltipEdit;

	// Dialog buttons
	StreamUP::UIStyles::PillButton *okButton;
	StreamUP::UIStyles::PillButton *cancelButton;

	// Data
	QString selectedIconPath;
	bool isEditMode;
	QString originalItemId; // For edit mode
};

} // namespace StreamUP
