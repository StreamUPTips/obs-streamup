#include "websocket-button-config-dialog.hpp"
#include "icon-selector-dialog.hpp"
#include "ui-helpers.hpp"
#include <streamup/ui/window-chrome.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/gallery-style.hpp>
#include <streamup/ui/mac-inputs.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
#include "version.h"

#include <QDoubleValidator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPixmap>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QUuid>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs.h>

#include <vector>

using namespace StreamUP::UIStyles;

namespace StreamUP {

namespace {

// The obs-websocket 5.x requests worth putting on a toolbar. obs-websocket
// cannot be asked what it supports, so this list is shipped rather than queried.
const QStringList &obsWebSocketRequests()
{
	static const QStringList requests = {
		// General
		"GetVersion",
		"GetStats",
		"BroadcastCustomEvent",
		"TriggerHotkeyByName",
		"TriggerHotkeyByKeySequence",
		"Sleep",
		// Config
		"GetSceneCollectionList",
		"SetCurrentSceneCollection",
		"GetProfileList",
		"SetCurrentProfile",
		"GetVideoSettings",
		"GetStreamServiceSettings",
		// Sources
		"GetSourceActive",
		"GetSourceScreenshot",
		"SaveSourceScreenshot",
		// Scenes
		"GetSceneList",
		"GetGroupList",
		"GetCurrentProgramScene",
		"SetCurrentProgramScene",
		"GetCurrentPreviewScene",
		"SetCurrentPreviewScene",
		"CreateScene",
		"RemoveScene",
		"SetSceneName",
		// Inputs
		"GetInputList",
		"GetInputMute",
		"SetInputMute",
		"ToggleInputMute",
		"GetInputVolume",
		"SetInputVolume",
		"GetInputAudioBalance",
		"SetInputAudioBalance",
		"GetInputAudioSyncOffset",
		"SetInputAudioSyncOffset",
		"GetInputAudioMonitorType",
		"SetInputAudioMonitorType",
		"GetInputSettings",
		"SetInputSettings",
		"PressInputPropertiesButton",
		// Transitions
		"GetSceneTransitionList",
		"GetCurrentSceneTransition",
		"SetCurrentSceneTransition",
		"SetCurrentSceneTransitionDuration",
		"TriggerStudioModeTransition",
		// Filters
		"GetSourceFilterList",
		"GetSourceFilter",
		"SetSourceFilterEnabled",
		"SetSourceFilterIndex",
		"SetSourceFilterSettings",
		// Scene items
		"GetSceneItemList",
		"GetSceneItemId",
		"GetSceneItemEnabled",
		"SetSceneItemEnabled",
		"GetSceneItemLocked",
		"SetSceneItemLocked",
		"GetSceneItemIndex",
		"SetSceneItemIndex",
		"GetSceneItemTransform",
		"SetSceneItemTransform",
		"CreateSceneItem",
		"RemoveSceneItem",
		"DuplicateSceneItem",
		// Outputs
		"GetVirtualCamStatus",
		"ToggleVirtualCam",
		"StartVirtualCam",
		"StopVirtualCam",
		"GetReplayBufferStatus",
		"ToggleReplayBuffer",
		"StartReplayBuffer",
		"StopReplayBuffer",
		"SaveReplayBuffer",
		"GetLastReplayBufferReplay",
		"GetOutputList",
		"ToggleOutput",
		"StartOutput",
		"StopOutput",
		// Stream
		"GetStreamStatus",
		"ToggleStream",
		"StartStream",
		"StopStream",
		"SendStreamCaption",
		// Record
		"GetRecordStatus",
		"ToggleRecord",
		"StartRecord",
		"StopRecord",
		"ToggleRecordPause",
		"PauseRecord",
		"ResumeRecord",
		"SplitRecordFile",
		"CreateRecordChapter",
		// Media inputs
		"GetMediaInputStatus",
		"SetMediaInputCursor",
		"OffsetMediaInputCursor",
		"TriggerMediaInputAction",
		// UI
		"GetStudioModeEnabled",
		"SetStudioModeEnabled",
		"OpenInputPropertiesDialog",
		"OpenInputFiltersDialog",
		"OpenInputInteractDialog",
		"GetMonitorList",
		"OpenVideoMixProjector",
		"OpenSourceProjector",
	};
	return requests;
}

// ── Argument table ───────────────────────────────────────────────────────────
// Only the requests a toolbar button realistically needs arguments for. Anything
// missing here falls through to the raw JSON box, which is always available.

struct ArgSpec {
	const char *key;
	int kind; // WebSocketButtonConfigDialog::ArgKind, kept as int so the table stays POD
};

struct RequestArgs {
	const char *request;
	std::vector<ArgSpec> args;
};

enum {
	KText = 0,
	KNumber = 1,
	KDecimal = 2,
	KBoolean = 3,
	KScene = 4,
	KSource = 5,
	KInput = 6,
	KTransition = 7
};

const std::vector<RequestArgs> &friendlyArgumentTable()
{
	static const std::vector<RequestArgs> table = {
		{"SetCurrentProgramScene", {{"sceneName", KScene}}},
		{"SetCurrentPreviewScene", {{"sceneName", KScene}}},
		{"RemoveScene", {{"sceneName", KScene}}},
		{"SetSceneItemEnabled",
		 {{"sceneName", KScene}, {"sceneItemId", KNumber}, {"sceneItemEnabled", KBoolean}}},
		{"SetSceneItemLocked",
		 {{"sceneName", KScene}, {"sceneItemId", KNumber}, {"sceneItemLocked", KBoolean}}},
		{"GetSceneItemId", {{"sceneName", KScene}, {"sourceName", KSource}}},
		{"GetSceneItemList", {{"sceneName", KScene}}},
		{"SetSourceFilterEnabled",
		 {{"sourceName", KSource}, {"filterName", KText}, {"filterEnabled", KBoolean}}},
		{"GetSourceFilterList", {{"sourceName", KSource}}},
		{"GetSourceFilter", {{"sourceName", KSource}, {"filterName", KText}}},
		{"SetInputMute", {{"inputName", KInput}, {"inputMuted", KBoolean}}},
		{"ToggleInputMute", {{"inputName", KInput}}},
		{"GetInputMute", {{"inputName", KInput}}},
		{"GetInputVolume", {{"inputName", KInput}}},
		{"SetInputVolume", {{"inputName", KInput}, {"inputVolumeDb", KDecimal}}},
		{"SetInputAudioBalance", {{"inputName", KInput}, {"inputAudioBalance", KDecimal}}},
		{"SetInputAudioSyncOffset", {{"inputName", KInput}, {"inputAudioSyncOffset", KNumber}}},
		{"TriggerMediaInputAction", {{"inputName", KInput}, {"mediaAction", KText}}},
		{"GetMediaInputStatus", {{"inputName", KInput}}},
		{"OpenInputPropertiesDialog", {{"inputName", KInput}}},
		{"OpenInputFiltersDialog", {{"inputName", KInput}}},
		{"SetCurrentSceneTransition", {{"transitionName", KTransition}}},
		{"SetCurrentSceneTransitionDuration", {{"transitionDuration", KNumber}}},
		{"TriggerHotkeyByName", {{"hotkeyName", KText}}},
		{"SetStudioModeEnabled", {{"studioModeEnabled", KBoolean}}},
		{"SetCurrentSceneCollection", {{"sceneCollectionName", KText}}},
		{"SetCurrentProfile", {{"profileName", KText}}},
		{"SendStreamCaption", {{"captionText", KText}}},
		{"CreateRecordChapter", {{"chapterName", KText}}},
		{"OpenSourceProjector", {{"sourceName", KSource}, {"monitorIndex", KNumber}}},
		{"OpenVideoMixProjector", {{"videoMixType", KText}, {"monitorIndex", KNumber}}},
		{"SaveSourceScreenshot",
		 {{"sourceName", KSource}, {"imageFormat", KText}, {"imageFilePath", KText}}},
		{"GetSourceActive", {{"sourceName", KSource}}},
		{"Sleep", {{"sleepMillis", KNumber}}},
	};
	return table;
}

const std::vector<ArgSpec> *argsForRequest(const QString &request)
{
	for (const auto &entry : friendlyArgumentTable()) {
		if (request == QLatin1String(entry.request))
			return &entry.args;
	}
	return nullptr;
}

// ── OBS name lookups ─────────────────────────────────────────────────────────
// All of these run on the main thread, which is where the dialog lives.

QStringList sceneNames()
{
	QStringList names;
	char **raw = obs_frontend_get_scene_names();
	if (!raw)
		return names;
	for (char **it = raw; *it; ++it)
		names.append(QString::fromUtf8(*it));

	// ONE bfree, not one per string. obs_frontend_get_scene_names returns a
	// single bmalloc block holding the pointer array followed by the character
	// data, so every name is an interior pointer into it. Freeing them
	// individually corrupts the heap.
	bfree(raw);
	return names;
}

// obs_enum_sources hands out non-incremented references, so nothing is released here.
bool collectSourceName(void *data, obs_source_t *source)
{
	auto *out = static_cast<QStringList *>(data);
	const char *name = obs_source_get_name(source);
	if (name)
		out->append(QString::fromUtf8(name));
	return true;
}

bool collectInputName(void *data, obs_source_t *source)
{
	if (obs_source_get_type(source) != OBS_SOURCE_TYPE_INPUT)
		return true;
	return collectSourceName(data, source);
}

QStringList sourceNames()
{
	QStringList names;
	obs_enum_sources(collectSourceName, &names);
	names.sort(Qt::CaseInsensitive);
	return names;
}

QStringList inputNames()
{
	QStringList names;
	obs_enum_sources(collectInputName, &names);
	names.sort(Qt::CaseInsensitive);
	return names;
}

QStringList transitionNames()
{
	QStringList names;
	struct obs_frontend_source_list list = {};
	obs_frontend_get_transitions(&list);
	for (size_t i = 0; i < list.sources.num; i++) {
		const char *name = obs_source_get_name(list.sources.array[i]);
		if (name)
			names.append(QString::fromUtf8(name));
	}
	obs_frontend_source_list_free(&list);
	return names;
}

} // namespace

WebSocketButtonConfigDialog::WebSocketButtonConfigDialog(QWidget *parent)
	: ShadowDialog(parent), isEditMode(false)
{
	WindowShell chrome = applyChrome(this, obs_module_text("StreamUP.Toolbar.WebSocket.AddTitle"),
					 "v" PROJECT_VERSION, /*brandFooter=*/false, "StreamUP");
	chrome.content->setContentsMargins(S(20), S(16), S(20), S(16));
	mainLayout = chrome.content;
	footerButtons = chrome.footerButtons;
	setModal(true);
	resize(560 + 2 * ShadowDialog::kShadowMargin, 720 + 2 * ShadowDialog::kShadowMargin);

	setupUI();
	validateInput();
}

WebSocketButtonConfigDialog::WebSocketButtonConfigDialog(
	std::shared_ptr<StreamUP::ToolbarConfig::WebSocketButtonItem> existingItem, QWidget *parent)
	: ShadowDialog(parent), isEditMode(true)
{
	WindowShell chrome = applyChrome(this, obs_module_text("StreamUP.Toolbar.WebSocket.EditTitle"),
					 "v" PROJECT_VERSION, /*brandFooter=*/false, "StreamUP");
	chrome.content->setContentsMargins(S(20), S(16), S(20), S(16));
	mainLayout = chrome.content;
	footerButtons = chrome.footerButtons;
	setModal(true);
	resize(560 + 2 * ShadowDialog::kShadowMargin, 720 + 2 * ShadowDialog::kShadowMargin);

	setupUI();
	setExistingItem(existingItem);
	validateInput();
}

void WebSocketButtonConfigDialog::setupUI()
{
	setupRequestSection();
	setupArgumentsSection();
	setupIconSection();
	setupCustomisationSection();

	okButton = new PillButton(isEditMode ? obs_module_text("StreamUP.Toolbar.WebSocket.Button.Update")
					     : obs_module_text("StreamUP.Toolbar.WebSocket.Button.Add"),
				  "primary");
	cancelButton = new PillButton(obs_module_text("UI.Button.Cancel"), "outline");

	okButton->setDefault(true);
	okButton->setEnabled(false);

	footerButtons->addWidget(cancelButton);
	footerButtons->addWidget(okButton);

	connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
	connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

	rebuildArgumentFields();
	updateIconDisplay();
}

void WebSocketButtonConfigDialog::setupRequestSection()
{
	mainLayout->addWidget(sectionHeader(obs_module_text("StreamUP.Toolbar.WebSocket.Group.Request")));

	QStringList sources;
	sources << obs_module_text("StreamUP.Toolbar.WebSocket.Source.ObsWebSocket")
		<< obs_module_text("StreamUP.Toolbar.WebSocket.Source.StreamUP")
		<< obs_module_text("StreamUP.Toolbar.WebSocket.Source.Vendor");
	sourceSelector = new SegmentedControl(sources, this);
	mainLayout->addWidget(sourceSelector);

	requestStack = new QStackedWidget(this);
	requestStack->setMinimumHeight(S(180));

	// Page 0 — the shipped obs-websocket request list
	QWidget *obsPage = new QWidget(requestStack);
	QVBoxLayout *obsPageLayout = new QVBoxLayout(obsPage);
	obsPageLayout->setContentsMargins(0, 0, 0, 0);
	obsSearch = new SearchField(obsPage);
	obsSearch->setPlaceholderText(obs_module_text("StreamUP.Toolbar.WebSocket.Placeholder.Search"));
	obsRequestList = new QListWidget(obsPage);
	obsRequestList->setStyleSheet(listStyle());
	useScrollBars(obsRequestList);
	obsRequestList->addItems(obsWebSocketRequests());
	obsPageLayout->addWidget(obsSearch);
	obsPageLayout->addWidget(obsRequestList);
	requestStack->addWidget(obsPage);

	// Page 1 — StreamUP's own vendor requests
	QWidget *supPage = new QWidget(requestStack);
	QVBoxLayout *supPageLayout = new QVBoxLayout(supPage);
	supPageLayout->setContentsMargins(0, 0, 0, 0);
	streamUPSearch = new SearchField(supPage);
	streamUPSearch->setPlaceholderText(obs_module_text("StreamUP.Toolbar.WebSocket.Placeholder.Search"));
	streamUPRequestList = new QListWidget(supPage);
	streamUPRequestList->setStyleSheet(listStyle());
	useScrollBars(streamUPRequestList);
	streamUPRequestList->addItems(StreamUP::ToolbarConfig::streamUPVendorRequests());
	supPageLayout->addWidget(streamUPSearch);
	supPageLayout->addWidget(streamUPRequestList);
	requestStack->addWidget(supPage);

	// Page 2 — another plugin's vendor. Nothing can enumerate those, so it is free text.
	QWidget *vendorPage = new QWidget(requestStack);
	QFormLayout *vendorForm = new QFormLayout(vendorPage);
	vendorForm->setContentsMargins(0, 0, 0, 0);
	vendorNameEdit = new QLineEdit(vendorPage);
	vendorNameEdit->setPlaceholderText(obs_module_text("StreamUP.Toolbar.WebSocket.Placeholder.VendorName"));
	vendorNameEdit->setStyleSheet(lineEditStyle());
	vendorRequestEdit = new QLineEdit(vendorPage);
	vendorRequestEdit->setPlaceholderText(obs_module_text("StreamUP.Toolbar.WebSocket.Placeholder.RequestType"));
	vendorRequestEdit->setStyleSheet(lineEditStyle());
	vendorForm->addRow(QString(obs_module_text("StreamUP.Toolbar.WebSocket.Label.VendorName")), vendorNameEdit);
	vendorForm->addRow(QString(obs_module_text("StreamUP.Toolbar.WebSocket.Label.RequestType")), vendorRequestEdit);
	QLabel *vendorHint = new QLabel(obs_module_text("StreamUP.Toolbar.WebSocket.Message.VendorHint"), vendorPage);
	vendorHint->setWordWrap(true);
	vendorHint->setStyleSheet(dimLabelStyle());
	vendorForm->addRow("", vendorHint);
	requestStack->addWidget(vendorPage);

	mainLayout->addWidget(requestStack);

	sourceSelector->onChanged([this](int index) { onSourceChanged(index); });
	connect(obsSearch, &QLineEdit::textChanged, this,
		[this](const QString &text) { filterList(obsRequestList, text); });
	connect(streamUPSearch, &QLineEdit::textChanged, this,
		[this](const QString &text) { filterList(streamUPRequestList, text); });
	connect(obsRequestList, &QListWidget::currentRowChanged, this, [this](int) { onRequestChanged(); });
	connect(streamUPRequestList, &QListWidget::currentRowChanged, this, [this](int) { onRequestChanged(); });
	connect(vendorRequestEdit, &QLineEdit::textChanged, this, [this](const QString &) { onRequestChanged(); });
	connect(vendorNameEdit, &QLineEdit::textChanged, this, [this](const QString &) { validateInput(); });
}

void WebSocketButtonConfigDialog::setupArgumentsSection()
{
	mainLayout->addWidget(sectionHeader(obs_module_text("StreamUP.Toolbar.WebSocket.Group.Arguments")));

	rawJsonCheck = new IOSCheckBox(obs_module_text("StreamUP.Toolbar.WebSocket.Check.RawJson"), this);
	mainLayout->addWidget(rawJsonCheck);

	argsGroup = new QWidget(this);
	argsForm = new QFormLayout(argsGroup);
	argsForm->setContentsMargins(0, 0, 0, 0);
	mainLayout->addWidget(argsGroup);

	noArgumentsLabel = new QLabel(obs_module_text("StreamUP.Toolbar.WebSocket.Message.NoArguments"), this);
	noArgumentsLabel->setWordWrap(true);
	noArgumentsLabel->setStyleSheet(dimLabelStyle());
	mainLayout->addWidget(noArgumentsLabel);

	jsonEdit = new QPlainTextEdit(this);
	jsonEdit->setStyleSheet(plainTextStyle());
	jsonEdit->setPlaceholderText(obs_module_text("StreamUP.Toolbar.WebSocket.Placeholder.Json"));
	jsonEdit->setFixedHeight(S(96));
	mainLayout->addWidget(jsonEdit);

	jsonErrorLabel = new QLabel("", this);
	jsonErrorLabel->setWordWrap(true);
	jsonErrorLabel->setStyleSheet(QString("color: %1;").arg(Colors::COLOR_DANGER));
	jsonErrorLabel->hide();
	mainLayout->addWidget(jsonErrorLabel);

	connect(rawJsonCheck, &QCheckBox::toggled, this, [this](bool raw) { onRawJsonToggled(raw); });
	connect(jsonEdit, &QPlainTextEdit::textChanged, this, [this]() { validateInput(); });
}

void WebSocketButtonConfigDialog::setupIconSection()
{
	mainLayout->addWidget(sectionHeader(obs_module_text("StreamUP.Toolbar.WebSocket.Group.Icon")));

	QWidget *iconGroup = new QWidget(this);
	QHBoxLayout *iconLayout = new QHBoxLayout(iconGroup);
	iconLayout->setContentsMargins(0, 0, 0, 0);

	QLabel *previewCaption = new QLabel(obs_module_text("StreamUP.Toolbar.WebSocket.Label.Preview"), iconGroup);
	iconPreview = new QLabel(iconGroup);
	iconPreview->setFixedSize(S(32), S(32));
	iconPreview->setStyleSheet(scale_qss(QString("border: 1px solid %1;").arg(Colors::POPUP_BORDER)));
	iconPreview->setAlignment(Qt::AlignCenter);
	iconPreview->setScaledContents(true);

	selectIconButton = new PillButton(obs_module_text("StreamUP.Toolbar.WebSocket.Button.SelectIcon"), "primary");

	iconLayout->addWidget(previewCaption);
	iconLayout->addWidget(iconPreview);
	iconLayout->addWidget(selectIconButton);
	iconLayout->addStretch();

	mainLayout->addWidget(iconGroup);

	connect(selectIconButton, &QPushButton::clicked, this, &WebSocketButtonConfigDialog::onSelectIconClicked);
}

void WebSocketButtonConfigDialog::setupCustomisationSection()
{
	mainLayout->addWidget(sectionHeader(obs_module_text("StreamUP.Toolbar.WebSocket.Group.Customisation")));

	QWidget *group = new QWidget(this);
	QFormLayout *form = new QFormLayout(group);
	form->setContentsMargins(0, 0, 0, 0);

	displayNameEdit = new QLineEdit(group);
	displayNameEdit->setPlaceholderText(obs_module_text("StreamUP.Toolbar.WebSocket.Placeholder.DisplayName"));
	displayNameEdit->setStyleSheet(lineEditStyle());

	tooltipEdit = new QLineEdit(group);
	tooltipEdit->setPlaceholderText(obs_module_text("StreamUP.Toolbar.WebSocket.Placeholder.Tooltip"));
	tooltipEdit->setStyleSheet(lineEditStyle());

	form->addRow(QString(obs_module_text("StreamUP.Toolbar.WebSocket.Label.DisplayName")), displayNameEdit);
	form->addRow(QString(obs_module_text("StreamUP.Toolbar.WebSocket.Label.Tooltip")), tooltipEdit);

	mainLayout->addWidget(group);
	mainLayout->addStretch();
}

void WebSocketButtonConfigDialog::filterList(QListWidget *list, const QString &needle)
{
	for (int i = 0; i < list->count(); ++i) {
		QListWidgetItem *item = list->item(i);
		item->setHidden(!needle.isEmpty() && !item->text().contains(needle, Qt::CaseInsensitive));
	}
}

QString WebSocketButtonConfigDialog::currentRequestType() const
{
	switch (sourceSelector->currentIndex()) {
	case 0: {
		QListWidgetItem *item = obsRequestList->currentItem();
		return item ? item->text() : QString();
	}
	case 1: {
		QListWidgetItem *item = streamUPRequestList->currentItem();
		return item ? item->text() : QString();
	}
	default:
		return vendorRequestEdit->text().trimmed();
	}
}

void WebSocketButtonConfigDialog::onSourceChanged(int index)
{
	requestStack->setCurrentIndex(index);
	onRequestChanged();
}

void WebSocketButtonConfigDialog::onRequestChanged()
{
	rebuildArgumentFields();

	// Only ever a suggestion: once the user has typed a name of their own it stands.
	const QString request = currentRequestType();
	if (!request.isEmpty() && displayNameEdit->text().isEmpty())
		displayNameEdit->setPlaceholderText(request);

	validateInput();
}

void WebSocketButtonConfigDialog::clearArgumentFields()
{
	argFields.clear();
	while (argsForm->count() > 0) {
		QLayoutItem *item = argsForm->takeAt(0);
		if (QWidget *w = item->widget())
			w->deleteLater();
		delete item;
	}
}

QWidget *WebSocketButtonConfigDialog::makeArgEditor(ArgKind kind)
{
	switch (kind) {
	case ArgKind::Number: {
		auto *spin = new MacSpinBox(argsGroup);
		spin->setRange(-1000000, 1000000);
		return spin;
	}
	case ArgKind::Boolean:
		return new IOSCheckBox("", argsGroup);
	case ArgKind::Decimal: {
		auto *edit = new QLineEdit(argsGroup);
		edit->setStyleSheet(lineEditStyle());
		auto *validator = new QDoubleValidator(-1000000.0, 1000000.0, 3, edit);
		validator->setNotation(QDoubleValidator::StandardNotation);
		edit->setValidator(validator);
		return edit;
	}
	case ArgKind::SceneName:
	case ArgKind::SourceName:
	case ArgKind::InputName:
	case ArgKind::TransitionName: {
		auto *combo = new MacComboBox(argsGroup);
		combo->setStyleSheet(comboStyle());
		combo->addItem("");
		if (kind == ArgKind::SceneName)
			combo->addItems(sceneNames());
		else if (kind == ArgKind::SourceName)
			combo->addItems(sourceNames());
		else if (kind == ArgKind::InputName)
			combo->addItems(inputNames());
		else
			combo->addItems(transitionNames());
		return combo;
	}
	case ArgKind::Text:
	default: {
		auto *edit = new QLineEdit(argsGroup);
		edit->setStyleSheet(lineEditStyle());
		return edit;
	}
	}
}

void WebSocketButtonConfigDialog::rebuildArgumentFields()
{
	clearArgumentFields();

	const std::vector<ArgSpec> *specs = argsForRequest(currentRequestType());
	if (specs) {
		for (const ArgSpec &spec : *specs) {
			const ArgKind kind = static_cast<ArgKind>(spec.kind);
			QWidget *editor = makeArgEditor(kind);
			const QString labelKey =
				QString("StreamUP.Toolbar.WebSocket.Arg.%1").arg(QLatin1String(spec.key));
			argsForm->addRow(QString(obs_module_text(labelKey.toUtf8().constData())), editor);
			argFields.append({QString::fromLatin1(spec.key), kind, editor});
		}
	}

	const bool hasFriendly = !argFields.isEmpty();

	// With no friendly fields there is nothing to toggle between, so the raw box
	// is simply the only way in.
	{
		QSignalBlocker block(rawJsonCheck);
		rawJsonCheck->setVisible(hasFriendly);
		if (!hasFriendly)
			rawJsonCheck->setChecked(true);
	}

	const bool raw = rawJsonCheck->isChecked();
	argsGroup->setVisible(hasFriendly && !raw);
	jsonEdit->setVisible(raw);
	noArgumentsLabel->setVisible(!hasFriendly && !raw);
	if (!raw)
		jsonErrorLabel->hide();
}

void WebSocketButtonConfigDialog::onRawJsonToggled(bool raw)
{
	if (raw) {
		// Carry the friendly values across so the user edits what they already set.
		const QJsonObject built = collectArgumentFields();
		if (!built.isEmpty())
			jsonEdit->setPlainText(QString::fromUtf8(QJsonDocument(built).toJson(QJsonDocument::Indented)));
	} else {
		const QJsonDocument doc = QJsonDocument::fromJson(jsonEdit->toPlainText().trimmed().toUtf8());
		if (doc.isObject())
			applyJsonToFields(doc.object());
	}

	argsGroup->setVisible(!argFields.isEmpty() && !raw);
	jsonEdit->setVisible(raw);
	noArgumentsLabel->setVisible(argFields.isEmpty() && !raw);
	validateInput();
}

QJsonObject WebSocketButtonConfigDialog::collectArgumentFields() const
{
	QJsonObject obj;
	for (const ArgField &field : argFields) {
		switch (field.kind) {
		case ArgKind::Boolean:
			obj.insert(field.key, static_cast<IOSCheckBox *>(field.editor)->isChecked());
			break;
		case ArgKind::Number:
			obj.insert(field.key, static_cast<MacSpinBox *>(field.editor)->value());
			break;
		case ArgKind::Decimal: {
			const QString text = static_cast<QLineEdit *>(field.editor)->text().trimmed();
			if (!text.isEmpty())
				obj.insert(field.key, text.toDouble());
			break;
		}
		case ArgKind::SceneName:
		case ArgKind::SourceName:
		case ArgKind::InputName:
		case ArgKind::TransitionName: {
			const QString text = static_cast<QComboBox *>(field.editor)->currentText().trimmed();
			if (!text.isEmpty())
				obj.insert(field.key, text);
			break;
		}
		case ArgKind::Text:
		default: {
			const QString text = static_cast<QLineEdit *>(field.editor)->text().trimmed();
			if (!text.isEmpty())
				obj.insert(field.key, text);
			break;
		}
		}
	}
	return obj;
}

void WebSocketButtonConfigDialog::applyJsonToFields(const QJsonObject &obj)
{
	for (const ArgField &field : argFields) {
		if (!obj.contains(field.key))
			continue;
		const QJsonValue value = obj.value(field.key);
		switch (field.kind) {
		case ArgKind::Boolean:
			static_cast<IOSCheckBox *>(field.editor)->setChecked(value.toBool());
			break;
		case ArgKind::Number:
			static_cast<MacSpinBox *>(field.editor)->setValue(value.toInt());
			break;
		case ArgKind::Decimal:
			static_cast<QLineEdit *>(field.editor)
				->setText(QString::number(value.toDouble()));
			break;
		case ArgKind::SceneName:
		case ArgKind::SourceName:
		case ArgKind::InputName:
		case ArgKind::TransitionName: {
			auto *combo = static_cast<QComboBox *>(field.editor);
			const QString text = value.toString();
			int index = combo->findText(text);
			if (index < 0 && !text.isEmpty()) {
				// The named scene or source may not exist right now; keep it anyway.
				combo->addItem(text);
				index = combo->count() - 1;
			}
			combo->setCurrentIndex(index < 0 ? 0 : index);
			break;
		}
		case ArgKind::Text:
		default:
			static_cast<QLineEdit *>(field.editor)->setText(value.toString());
			break;
		}
	}
}

bool WebSocketButtonConfigDialog::jsonIsUsable(QString *errorOut) const
{
	if (!rawJsonCheck->isChecked())
		return true;

	const QString text = jsonEdit->toPlainText().trimmed();
	if (text.isEmpty())
		return true; // empty means no arguments

	QJsonParseError parseError{};
	const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		if (errorOut)
			*errorOut = QString("%1 %2")
					    .arg(obs_module_text("StreamUP.Toolbar.WebSocket.Error.InvalidJson"),
						 parseError.errorString());
		return false;
	}
	if (!doc.isObject()) {
		if (errorOut)
			*errorOut = obs_module_text("StreamUP.Toolbar.WebSocket.Error.NotJsonObject");
		return false;
	}
	return true;
}

QString WebSocketButtonConfigDialog::currentRequestData() const
{
	QJsonObject obj;
	if (rawJsonCheck->isChecked()) {
		const QString text = jsonEdit->toPlainText().trimmed();
		if (text.isEmpty())
			return QString();
		const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
		if (!doc.isObject())
			return QString();
		obj = doc.object();
	} else {
		obj = collectArgumentFields();
	}

	if (obj.isEmpty())
		return QString();
	return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void WebSocketButtonConfigDialog::onSelectIconClicked()
{
	const bool isCustomIcon = QFileInfo(selectedIconPath).isAbsolute();

	IconSelectorDialog dialog(isCustomIcon ? QString() : selectedIconPath,
				  isCustomIcon ? selectedIconPath : QString(), isCustomIcon, this);

	if (dialog.exec() == QDialog::Accepted) {
		const QString newIconPath = dialog.getSelectedIcon();
		if (!newIconPath.isEmpty()) {
			selectedIconPath = newIconPath;
			updateIconDisplay();
		}
	}
}

void WebSocketButtonConfigDialog::updateIconDisplay()
{
	iconPreview->clear();

	if (selectedIconPath.isEmpty()) {
		iconPreview->setText(obs_module_text("StreamUP.Toolbar.WebSocket.Message.NoIcon"));
		return;
	}

	QPixmap pixmap;
	if (QFileInfo(selectedIconPath).isAbsolute() && QFileInfo(selectedIconPath).exists())
		pixmap.load(selectedIconPath);
	else
		pixmap.load(StreamUP::UIHelpers::GetThemedIconPath(selectedIconPath));

	if (!pixmap.isNull())
		iconPreview->setPixmap(pixmap.scaled(S(32), S(32), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	else
		iconPreview->setText(obs_module_text("StreamUP.Toolbar.WebSocket.Message.InvalidIcon"));
}

void WebSocketButtonConfigDialog::setExistingItem(std::shared_ptr<StreamUP::ToolbarConfig::WebSocketButtonItem> item)
{
	if (!item)
		return;

	originalItemId = item->id;

	int sourceIndex = 0;
	switch (item->source) {
	case StreamUP::ToolbarConfig::WebSocketButtonItem::Source::StreamUP:
		sourceIndex = 1;
		break;
	case StreamUP::ToolbarConfig::WebSocketButtonItem::Source::Vendor:
		sourceIndex = 2;
		break;
	default:
		sourceIndex = 0;
		break;
	}
	sourceSelector->setCurrentIndex(sourceIndex);
	requestStack->setCurrentIndex(sourceIndex);

	if (sourceIndex == 2) {
		vendorNameEdit->setText(item->vendorName);
		vendorRequestEdit->setText(item->requestType);
	} else {
		QListWidget *list = (sourceIndex == 1) ? streamUPRequestList : obsRequestList;
		QList<QListWidgetItem *> matches = list->findItems(item->requestType, Qt::MatchExactly);
		if (matches.isEmpty() && !item->requestType.isEmpty()) {
			// A request from a newer obs-websocket than the list we ship.
			list->addItem(item->requestType);
			matches = list->findItems(item->requestType, Qt::MatchExactly);
		}
		if (!matches.isEmpty())
			list->setCurrentItem(matches.first());
	}

	rebuildArgumentFields();

	if (!item->requestData.isEmpty()) {
		const QJsonDocument doc = QJsonDocument::fromJson(item->requestData.toUtf8());
		const std::vector<ArgSpec> *specs = argsForRequest(item->requestType);
		bool coveredByFields = doc.isObject() && specs != nullptr;
		if (coveredByFields) {
			// Anything the friendly fields cannot represent has to stay as JSON,
			// otherwise saving would silently drop it.
			const QJsonObject obj = doc.object();
			for (const QString &key : obj.keys()) {
				bool known = false;
				for (const ArgField &field : argFields)
					known = known || (field.key == key);
				coveredByFields = coveredByFields && known;
			}
			if (coveredByFields)
				applyJsonToFields(obj);
		}
		if (!coveredByFields) {
			rawJsonCheck->setChecked(true);
			jsonEdit->setPlainText(
				doc.isObject()
					? QString::fromUtf8(QJsonDocument(doc.object()).toJson(QJsonDocument::Indented))
					: item->requestData);
		}
	}

	selectedIconPath = item->customIconPath.isEmpty() ? item->iconPath : item->customIconPath;
	updateIconDisplay();

	displayNameEdit->setText(item->displayName);
	tooltipEdit->setText(item->tooltip);
}

void WebSocketButtonConfigDialog::validateInput()
{
	const bool hasRequest = !currentRequestType().isEmpty();
	const bool hasVendor = sourceSelector->currentIndex() != 2 || !vendorNameEdit->text().trimmed().isEmpty();

	QString jsonError;
	const bool jsonOk = jsonIsUsable(&jsonError);
	if (jsonOk) {
		jsonErrorLabel->hide();
	} else {
		jsonErrorLabel->setText(jsonError);
		jsonErrorLabel->show();
	}

	okButton->setEnabled(hasRequest && hasVendor && jsonOk);

	if (!hasRequest)
		okButton->setToolTip(obs_module_text("StreamUP.Toolbar.WebSocket.Tooltip.SelectRequestFirst"));
	else if (!hasVendor)
		okButton->setToolTip(obs_module_text("StreamUP.Toolbar.WebSocket.Tooltip.VendorNameRequired"));
	else if (!jsonOk)
		okButton->setToolTip(jsonError);
	else
		okButton->setToolTip("");
}

std::shared_ptr<StreamUP::ToolbarConfig::WebSocketButtonItem>
WebSocketButtonConfigDialog::getWebSocketButtonItem() const
{
	const QString requestType = currentRequestType();
	if (requestType.isEmpty())
		return nullptr;

	const QString itemId =
		isEditMode ? originalItemId
			   : QString("websocket_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

	auto item = std::make_shared<StreamUP::ToolbarConfig::WebSocketButtonItem>(itemId, requestType);

	switch (sourceSelector->currentIndex()) {
	case 1:
		item->source = StreamUP::ToolbarConfig::WebSocketButtonItem::Source::StreamUP;
		break;
	case 2:
		item->source = StreamUP::ToolbarConfig::WebSocketButtonItem::Source::Vendor;
		item->vendorName = vendorNameEdit->text().trimmed();
		break;
	default:
		item->source = StreamUP::ToolbarConfig::WebSocketButtonItem::Source::ObsWebSocket;
		break;
	}

	item->requestData = currentRequestData();

	if (QFileInfo(selectedIconPath).isAbsolute() && QFileInfo(selectedIconPath).exists()) {
		item->useCustomIcon = true;
		item->customIconPath = selectedIconPath;
		item->iconPath = "";
	} else {
		item->useCustomIcon = false;
		item->iconPath = selectedIconPath;
		item->customIconPath = "";
	}

	const QString displayName = displayNameEdit->text().trimmed();
	const QString tooltip = tooltipEdit->text().trimmed();

	item->displayName = displayName.isEmpty() ? requestType : displayName;
	item->tooltip = tooltip.isEmpty() ? item->displayName : tooltip;

	return item;
}

} // namespace StreamUP

#include "websocket-button-config-dialog.moc"
