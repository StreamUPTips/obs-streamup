#include "my-products-window.hpp"

#include "../core/file-manager.hpp"
#include "../core/overlay-controls.hpp"
#include "../core/product-registry.hpp"
#include "overlay-controls-window.hpp"
#include "overlay-ui-common.hpp"
#include "version.h"

#include <streamup/ui/labels.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/section-card.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
#include <streamup/ui/window-chrome.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QPointer>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace su = StreamUP::UIStyles;
namespace oc = StreamUP::OverlayControls;
namespace pr = StreamUP::Products;
using StreamUP::OverlayUI::T;

namespace StreamUP {
namespace MyProductsWindow {

namespace {

QWidget *MainWindow()
{
	return static_cast<QWidget *>(obs_frontend_get_main_window());
}

class MyProductsDialog : public su::ShadowDialog {
public:
	explicit MyProductsDialog(QWidget *parent) : su::ShadowDialog(parent)
	{
		setAttribute(Qt::WA_DeleteOnClose, true);
		auto shell = su::applyChrome(this, T("MyProducts.Title"), "v" PROJECT_VERSION, /*brandFooter=*/true,
					     "StreamUP");

		auto *scroll = new QScrollArea();
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scroll->setStyleSheet("QScrollArea{background:transparent;border:none;}");
		su::useScrollBars(scroll);
		auto *page = new QWidget();
		page->setObjectName("mpPage");
		page->setStyleSheet(QString("QWidget#mpPage{background:%1;}").arg(su::Colors::BG_DARKEST));
		m_list = new QVBoxLayout(page);
		m_list->setContentsMargins(su::S(16), su::S(16), su::S(16), su::S(16));
		m_list->setSpacing(su::S(12));
		scroll->setWidget(page);
		shell.content->addWidget(scroll, 1);

#ifdef _WIN32
		auto *install = new su::PillButton(T("MyProducts.Install"), "outline");
		shell.footerButtons->addWidget(install);
		QObject::connect(install, &QPushButton::clicked, this,
				 []() { StreamUP::FileManager::LoadStreamupFileWithWarning(); });
#endif
		auto *done = new su::PillButton(T("OverlayControls.Button.Done"), "primary");
		shell.footerButtons->addWidget(done);
		QObject::connect(done, &QPushButton::clicked, this, &QDialog::close);

		// Installs, removals and collection switches all change the list.
		// Several can arrive at once, so they share one rebuild.
		m_rebuild = new QTimer(this);
		m_rebuild->setSingleShot(true);
		m_rebuild->setInterval(150);
		QObject::connect(m_rebuild, &QTimer::timeout, this, [this]() { rebuild(); });
		QObject::connect(oc::GetNotifier(), &oc::Notifier::overlaysChanged, this, [this]() { m_rebuild->start(); });
		obs_frontend_add_event_callback(OnEvent, this);

		rebuild();
		const int m = su::S(su::ShadowDialog::kShadowMargin);
		resize(su::S(640) + 2 * m, su::S(560) + 2 * m);
	}

	~MyProductsDialog() override { obs_frontend_remove_event_callback(OnEvent, this); }

private:
	static void OnEvent(enum obs_frontend_event event, void *param)
	{
		if (event != OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED && event != OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED)
			return;
		auto *self = static_cast<MyProductsDialog *>(param);
		QMetaObject::invokeMethod(self->m_rebuild, qOverload<>(&QTimer::start), Qt::QueuedConnection);
	}

	void clearList()
	{
		while (QLayoutItem *item = m_list->takeAt(0)) {
			if (QWidget *w = item->widget())
				w->deleteLater();
			delete item;
		}
	}

	void rebuild()
	{
		clearList();
		const QList<pr::ProductEntry> products = pr::ListProducts();

		QLabel *intro = su::makeLabel(products.isEmpty() ? T("MyProducts.Empty") : T("MyProducts.Intro"),
					      su::CardText::kBody, 400, su::Colors::TEXT_SECONDARY);
		intro->setWordWrap(true);
		m_list->addWidget(intro);

		for (const pr::ProductEntry &p : products)
			addCard(p);
		m_list->addStretch(1);
	}

	void addCard(const pr::ProductEntry &p)
	{
		QVBoxLayout *card = su::sectionCard(m_list);
		auto *row = new QHBoxLayout();
		row->setContentsMargins(0, 0, 0, 0);
		row->setSpacing(su::S(16));
		auto *text = new QVBoxLayout();
		text->setContentsMargins(0, 0, 0, 0);
		text->setSpacing(su::S(3));
		text->addWidget(su::makeLabel(p.name, su::CardText::kTitle, 600, su::Colors::TEXT_PRIMARY));

		QStringList facts;
		if (!p.version.isEmpty())
			facts.append("v" + p.version);
		facts.append(p.isTransition ? T("MyProducts.Kind.Transition") : T("MyProducts.Kind.Overlay"));
		facts.append(p.scenes.size() == 1 ? T("MyProducts.OneScene") : T("MyProducts.Scenes").arg(p.scenes.size()));
		text->addWidget(su::makeLabel(facts.join(QString::fromUtf8("  ·  ")), su::CardText::kCaption, 400,
					      su::Colors::TEXT_SECONDARY));
		if (!p.scenes.isEmpty()) {
			QLabel *scenes = su::makeLabel(p.scenes.join(", "), su::Sizes::FONT_SIZE_TINY, 500, su::Colors::TEXT_MUTED);
			scenes->setWordWrap(true);
			text->addWidget(scenes);
		}
		if (p.overlayId.isEmpty()) {
			QLabel *note = su::makeLabel(T("MyProducts.NoControls"), su::Sizes::FONT_SIZE_TINY, 500,
						     su::Colors::COLOR_WARNING);
			note->setWordWrap(true);
			text->addWidget(note);
		}
		row->addLayout(text, 1);

		if (!p.overlayId.isEmpty()) {
			auto *adjust = new su::PillButton(T("MyProducts.Adjust"), "primary");
			adjust->setAccessibleName(T("MyProducts.AdjustAccessible").arg(p.name));
			const QString id = p.overlayId;
			QObject::connect(adjust, &QPushButton::clicked, this,
					 [id]() { StreamUP::OverlayControlsWindow::Show(id); });
			row->addWidget(adjust, 0, Qt::AlignVCenter);
		} else {
			auto *attach = new su::PillButton(T("MyProducts.AddControls"), "neutral");
			QObject::connect(attach, &QPushButton::clicked, this,
					 []() { StreamUP::OverlayControlsWindow::AttachFromFile(); });
			row->addWidget(attach, 0, Qt::AlignVCenter);
		}
		card->addLayout(row);
	}

	QVBoxLayout *m_list = nullptr;
	QTimer *m_rebuild = nullptr;
};

QPointer<MyProductsDialog> &Open()
{
	static QPointer<MyProductsDialog> dialog;
	return dialog;
}

void ShowOnUiThread()
{
	QPointer<MyProductsDialog> &existing = Open();
	if (existing) {
		existing->showNormal();
		existing->raise();
		existing->activateWindow();
		return;
	}
	auto *dlg = new MyProductsDialog(MainWindow());
	existing = dlg;
	dlg->show();
	dlg->raise();
	dlg->activateWindow();
}

} // namespace

void Show()
{
	QMetaObject::invokeMethod(qApp, []() { ShowOnUiThread(); }, Qt::QueuedConnection);
}

} // namespace MyProductsWindow
} // namespace StreamUP
