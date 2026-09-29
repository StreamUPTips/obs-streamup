#include "overlay-platforms-editor.hpp"
#include "overlay-ui-common.hpp"

#include <streamup/ui/glass.hpp>
#include <streamup/ui/ios-checkbox.hpp>
#include <streamup/ui/pill-button.hpp>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLineEdit>

namespace StreamUP {
namespace OverlayUI {

namespace oc = StreamUP::OverlayControls;

PlatformsEditor::PlatformsEditor(const oc::ControlInfo &info, QWidget *parent) : QWidget(parent), m_defs(info.platforms)
{
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(0, 0, 0, 0);
	v->setSpacing(su::S(8));

	// Column captions line up with the row grid below.
	auto *head = new QGridLayout();
	head->setContentsMargins(0, 0, 0, 0);
	head->setHorizontalSpacing(su::S(8));
	head->addWidget(DimLabel(T("OverlayControls.Platforms.Platform")), 0, 1);
	head->addWidget(DimLabel(T("OverlayControls.Platforms.Name")), 0, 2);
	head->addWidget(DimLabel(T("OverlayControls.Platforms.Nameplate")), 0, 3);
	head->addWidget(DimLabel(T("OverlayControls.Platforms.Chat")), 0, 4);
	head->setColumnMinimumWidth(0, su::S(56));
	head->setColumnMinimumWidth(1, su::S(130));
	head->setColumnStretch(2, 1);
	head->setColumnMinimumWidth(3, su::S(80));
	head->setColumnMinimumWidth(4, su::S(56));
	head->setColumnMinimumWidth(5, su::S(26));
	v->addLayout(head);

	auto *listHost = new QWidget();
	m_list = new QVBoxLayout(listHost);
	m_list->setContentsMargins(0, 0, 0, 0);
	m_list->setSpacing(su::S(6));
	v->addWidget(listHost);

	auto *add = new su::PillButton(T("OverlayControls.Platforms.Add"), "neutral");
	add->setAccessibleName(T("OverlayControls.Platforms.Add"));
	auto *addRow = new QHBoxLayout();
	addRow->setContentsMargins(0, su::S(2), 0, 0);
	addRow->addWidget(add);
	addRow->addStretch();
	v->addLayout(addRow);

	QObject::connect(add, &QPushButton::clicked, this, [this]() {
		// The first platform not already listed, so adding twice does
		// not give two identical rows.
		QString pick = m_defs.first().id;
		for (const oc::PlatformDef &d : m_defs) {
			bool used = false;
			for (const QJsonValue &r : m_rows)
				used = used || r.toObject().value("platform").toString() == d.id;
			if (!used) {
				pick = d.id;
				break;
			}
		}
		m_rows.append(QJsonObject{{"platform", pick}, {"username", ""}, {"nameplate", true}, {"chat", false}});
		rebuild();
		commit();
	});

	m_typing = new QTimer(this);
	m_typing->setSingleShot(true);
	m_typing->setInterval(450);
	QObject::connect(m_typing, &QTimer::timeout, this, [this]() { commit(); });

	setValue(info.value.toArray());
}

const oc::PlatformDef *PlatformsEditor::def(const QString &id) const
{
	for (const oc::PlatformDef &d : m_defs)
		if (d.id == id)
			return &d;
	return nullptr;
}

void PlatformsEditor::setValue(const QJsonArray &rows)
{
	if (rows == m_rows && m_list->count() == rows.size())
		return;
	m_rows = rows;
	rebuild();
}

void PlatformsEditor::commit(bool typing)
{
	if (typing) {
		m_typing->start();
		return;
	}
	m_typing->stop();
	if (onChanged)
		onChanged(m_rows);
}

void PlatformsEditor::rebuild()
{
	while (QLayoutItem *item = m_list->takeAt(0)) {
		if (QWidget *w = item->widget())
			w->deleteLater();
		delete item;
	}

	for (int i = 0; i < m_rows.size(); ++i) {
		const QJsonObject row = m_rows.at(i).toObject();
		const QString platform = row.value("platform").toString();
		const oc::PlatformDef *d = def(platform);

		auto *w = new QWidget();
		auto *g = new QGridLayout(w);
		g->setContentsMargins(0, 0, 0, 0);
		g->setHorizontalSpacing(su::S(8));
		g->setColumnMinimumWidth(0, su::S(56));
		g->setColumnMinimumWidth(1, su::S(130));
		g->setColumnStretch(2, 1);
		g->setColumnMinimumWidth(3, su::S(80));
		g->setColumnMinimumWidth(4, su::S(56));

		auto *order = new QHBoxLayout();
		order->setSpacing(0);
		auto *up = IconButton(QString::fromUtf8("▲"), T("OverlayControls.Platforms.Up"),
				      T("OverlayControls.Platforms.Up"));
		auto *down = IconButton(QString::fromUtf8("▼"), T("OverlayControls.Platforms.Down"),
					T("OverlayControls.Platforms.Down"));
		up->setEnabled(i > 0);
		down->setEnabled(i < m_rows.size() - 1);
		order->addWidget(up);
		order->addWidget(down);
		g->addLayout(order, 0, 0);

		auto *combo = new su::MacComboBox();
		combo->setOnCard(true);
		combo->setStyleSheet(su::comboStyle(true));
		su::makeComboAnimated(combo);
		for (const oc::PlatformDef &pd : m_defs)
			combo->addItem(pd.label, pd.id);
		combo->setCurrentIndex(std::max(0, combo->findData(platform)));
		combo->setAccessibleName(T("OverlayControls.Platforms.Platform"));
		g->addWidget(combo, 0, 1);

		auto *name = new QLineEdit(row.value("username").toString());
		name->setStyleSheet(su::lineEditStyle(true));
		name->setFixedHeight(su::S(28));
		name->setPlaceholderText(d ? d->placeholder : QString());
		name->setAccessibleName(T("OverlayControls.Platforms.Name"));
		g->addWidget(name, 0, 2);

		auto *plate = new su::IOSCheckBox();
		plate->setChecked(row.value("nameplate").toBool(true));
		plate->setAccessibleName(T("OverlayControls.Platforms.Nameplate"));
		g->addWidget(plate, 0, 3, Qt::AlignCenter);

		auto *chat = new su::IOSCheckBox();
		chat->setChecked(row.value("chat").toBool(false));
		chat->setEnabled(d && d->chat);
		chat->setToolTip(d && d->chat ? QString() : T("OverlayControls.Platforms.NoChat"));
		chat->setAccessibleName(T("OverlayControls.Platforms.Chat"));
		g->addWidget(chat, 0, 4, Qt::AlignCenter);

		auto *remove = IconButton(QString::fromUtf8("✕"), T("OverlayControls.Platforms.Remove"),
					  T("OverlayControls.Platforms.Remove"), su::Colors::COLOR_DANGER);
		g->addWidget(remove, 0, 5);

		m_list->addWidget(w);

		// Rows are rebuilt from m_rows on every structural change, so each
		// handler edits m_rows by index and then commits.
		QObject::connect(up, &QToolButton::clicked, this, [this, i]() {
			const QJsonValue a = m_rows.at(i);
			m_rows[i] = m_rows.at(i - 1);
			m_rows[i - 1] = a;
			rebuild();
			commit();
		});
		QObject::connect(down, &QToolButton::clicked, this, [this, i]() {
			const QJsonValue a = m_rows.at(i);
			m_rows[i] = m_rows.at(i + 1);
			m_rows[i + 1] = a;
			rebuild();
			commit();
		});
		QObject::connect(remove, &QToolButton::clicked, this, [this, i]() {
			m_rows.removeAt(i);
			rebuild();
			commit();
		});
		QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i, combo](int) {
			QJsonObject r = m_rows.at(i).toObject();
			r["platform"] = combo->currentData().toString();
			const oc::PlatformDef *nd = def(r.value("platform").toString());
			if (!nd || !nd->chat)
				r["chat"] = false;
			m_rows[i] = r;
			rebuild(); // the chat switch and placeholder follow the platform
			commit();
		});
		QObject::connect(name, &QLineEdit::textEdited, this, [this, i](const QString &text) {
			QJsonObject r = m_rows.at(i).toObject();
			r["username"] = text;
			m_rows[i] = r;
			commit(true);
		});
		QObject::connect(name, &QLineEdit::editingFinished, this, [this]() {
			if (m_typing->isActive())
				commit();
		});
		QObject::connect(plate, &QCheckBox::toggled, this, [this, i](bool on) {
			QJsonObject r = m_rows.at(i).toObject();
			r["nameplate"] = on;
			m_rows[i] = r;
			commit();
		});
		QObject::connect(chat, &QCheckBox::toggled, this, [this, i](bool on) {
			QJsonObject r = m_rows.at(i).toObject();
			r["chat"] = on;
			m_rows[i] = r;
			commit();
		});
	}
}

} // namespace OverlayUI
} // namespace StreamUP
