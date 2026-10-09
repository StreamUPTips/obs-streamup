#include "overlay-all-settings.hpp"
#include "overlay-ui-common.hpp"

#include <streamup/ui/color-picker.hpp>
#include <streamup/ui/dialogs.hpp>
#include <streamup/ui/glass.hpp>
#include <streamup/ui/ios-checkbox.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/section-card.hpp>
#include <streamup/ui/tree-widget.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
#include <streamup/ui/window-chrome.hpp>

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFontDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>

#include <cmath>
#include <memory>

namespace StreamUP {
namespace OverlayUI {

namespace oc = StreamUP::OverlayControls;

namespace {

QWidget *MainWindow()
{
	return static_cast<QWidget *>(obs_frontend_get_main_window());
}

QColor ObsToQColor(long long v)
{
	const quint32 u = quint32(v);
	return QColor(int(u & 0xff), int((u >> 8) & 0xff), int((u >> 16) & 0xff), int((u >> 24) & 0xff));
}

long long QColorToObs(const QColor &c)
{
	return (static_cast<long long>(c.alpha()) << 24) | (static_cast<long long>(c.blue()) << 16) |
	       (static_cast<long long>(c.green()) << 8) | static_cast<long long>(c.red());
}

QString SwatchQss(const QColor &c)
{
	const double luma = (0.299 * c.redF() + 0.587 * c.greenF() + 0.114 * c.blueF()) * c.alphaF() +
			    (1.0 - c.alphaF()) * 0.15;
	return su::scale_qss(
		QString("QPushButton{background:%1;color:%2;border:1px solid rgba(255,255,255,0.14);"
			"border-radius:8px;min-height:26px;padding:0 12px;font-size:12px;font-weight:700;"
			"font-family:Roboto,'Open Sans','.AppleSystemUIFont',Helvetica,Arial,sans-serif;}"
			"QPushButton:hover{border:1px solid %3;}")
			.arg(c.name(QColor::HexArgb), luma > 0.55 ? QStringLiteral("#11111b") : QStringLiteral("#ffffff"),
			     su::Colors::PRIMARY_LIGHT));
}

QString HexOf(const QColor &c, bool alpha)
{
	return alpha && c.alpha() < 255 ? QStringLiteral("#%1%2")
						  .arg(c.name(QColor::HexRgb).mid(1))
						  .arg(c.alpha(), 2, 16, QLatin1Char('0'))
						  .toUpper()
					: c.name(QColor::HexRgb).toUpper();
}

// Opens the StreamUP colour picker. onLive follows the picker; closing it any
// way but Select calls onCancel.
void PickColour(const QString &title, const QColor &initial, bool alpha, std::function<void(const QColor &)> onLive,
		std::function<void()> onCancel)
{
	auto sh = su::makeWindow(title, QString(), MainWindow(), false, "StreamUP", {}, true);
	auto *cp = new su::ColorPicker();
	cp->setColor(initial);
	sh.content->setContentsMargins(su::S(16), su::S(16), su::S(16), su::S(8));
	sh.content->addWidget(cp);
	auto *cancel = new su::PillButton(T("OverlayControls.Button.Cancel"), "outline");
	auto *ok = new su::PillButton(T("OverlayControls.Button.Select"), "primary");
	sh.footerButtons->addWidget(cancel);
	sh.footerButtons->addWidget(ok);
	auto accepted = std::make_shared<bool>(false);
	cp->onChanged = [onLive, alpha](const QColor &c) {
		QColor out = c;
		if (!alpha)
			out.setAlpha(255);
		onLive(out);
	};
	QObject::connect(cancel, &QPushButton::clicked, sh.dialog, &QDialog::close);
	QObject::connect(ok, &QPushButton::clicked, sh.dialog, [accepted, dlg = sh.dialog]() {
		*accepted = true;
		dlg->close();
	});
	QObject::connect(sh.dialog, &QObject::destroyed, qApp, [accepted, onCancel]() {
		if (!*accepted)
			onCancel();
	});
	sh.dialog->resize(su::S(360), su::S(430));
	sh.dialog->show();
}

int DecimalsFor(double step)
{
	if (step <= 0.0)
		return 3;
	return std::clamp(int(std::ceil(-std::log10(step) - 1e-9)), 0, 6);
}

// Runs fn on a scene item found by scene name and id (or source name).
template <typename Fn> bool WithSceneItem(const QString &scene, qint64 id, const QString &source, Fn fn)
{
	obs_source_t *ss = obs_get_source_by_name(scene.toUtf8().constData());
	if (!ss)
		return false;
	obs_scene_t *sc = obs_scene_from_source(ss);
	if (!sc)
		sc = obs_group_from_source(ss);
	obs_sceneitem_t *item = sc ? obs_scene_find_sceneitem_by_id(sc, id) : nullptr;
	if (item && QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(item))) != source)
		item = nullptr;
	if (!item && sc)
		item = obs_scene_find_source_recursive(sc, source.toUtf8().constData());
	if (item)
		fn(item);
	obs_source_release(ss);
	return item != nullptr;
}

QString ElidedName(const QString &s)
{
	return s.size() > 60 ? s.left(57) + "..." : s;
}

} // namespace

// ================================================================ PropertiesForm

PropertiesForm::PropertiesForm(obs_source_t *source, QWidget *parent) : QWidget(parent)
{
	m_source = obs_source_get_ref(source);
	m_root = new QVBoxLayout(this);
	m_root->setContentsMargins(0, 0, 0, 0);
	m_root->setSpacing(su::S(8));
	rebuild();
}

PropertiesForm::~PropertiesForm()
{
	obs_properties_destroy(m_props);
	obs_source_release(m_source);
}

obs_data_t *PropertiesForm::settings() const
{
	return obs_source_get_settings(m_source);
}

void PropertiesForm::rebuild()
{
	m_rebuildQueued = false;
	while (QLayoutItem *item = m_root->takeAt(0)) {
		if (QWidget *w = item->widget())
			w->deleteLater();
		else if (QLayout *l = item->layout()) {
			// Nested layouts own their widgets through their parent widget,
			// which is this form; clear them explicitly.
			while (QLayoutItem *sub = l->takeAt(0)) {
				if (QWidget *sw = sub->widget())
					sw->deleteLater();
				delete sub;
			}
		}
		delete item;
	}
	obs_properties_destroy(m_props);
	m_props = m_source ? obs_source_properties(m_source) : nullptr;
	if (!m_props) {
		m_root->addWidget(DimLabel(T("OverlayControls.All.NoSettings")));
		return;
	}
	// Runs every "modified" callback once, which is how a source hides the
	// settings that do not apply to its current mode.
	obs_data_t *s = settings();
	obs_properties_apply_settings(m_props, s);
	obs_data_release(s);
	build(m_root, m_props, 0);
}

void PropertiesForm::build(QVBoxLayout *into, obs_properties_t *props, int depth)
{
	auto newGrid = [into]() {
		auto *g = new QGridLayout();
		g->setContentsMargins(0, 0, 0, 0);
		g->setHorizontalSpacing(su::S(12));
		g->setVerticalSpacing(su::S(8));
		g->setColumnMinimumWidth(0, su::S(150));
		g->setColumnStretch(1, 1);
		into->addLayout(g);
		return g;
	};
	QGridLayout *grid = newGrid();
	int row = 0;
	bool any = false;
	for (obs_property_t *p = obs_properties_first(props); p; obs_property_next(&p)) {
		if (!obs_property_visible(p))
			continue;
		any = true;
		if (obs_property_get_type(p) == OBS_PROPERTY_GROUP) {
			const QString name = QString::fromUtf8(obs_property_name(p));
			auto *box = new QFrame();
			box->setObjectName("ocGroup");
			box->setStyleSheet(su::scale_qss(QString("QFrame#ocGroup{background:%1;border-radius:10px;}")
								 .arg(depth % 2 ? su::Colors::BG_PRIMARY : su::Colors::BG_SECONDARY)));
			auto *bv = new QVBoxLayout(box);
			bv->setContentsMargins(su::S(12), su::S(10), su::S(12), su::S(10));
			bv->setSpacing(su::S(8));
			const QString title = QString::fromUtf8(obs_property_description(p));
			if (obs_property_group_type(p) == OBS_GROUP_CHECKABLE) {
				auto *check = new su::IOSCheckBox(title);
				check->setStyleSheet(QString("QCheckBox{color:%1;font-weight:700;}").arg(su::Colors::TEXT_PRIMARY));
				obs_data_t *s = settings();
				check->setChecked(obs_data_get_bool(s, obs_property_name(p)));
				obs_data_release(s);
				QObject::connect(check, &QCheckBox::toggled, this, [this, name](bool on) {
					obs_data_t *u = obs_data_create();
					obs_data_set_bool(u, name.toUtf8().constData(), on);
					obs_source_update(m_source, u);
					obs_data_release(u);
					changed(name);
				});
				bv->addWidget(check);
			} else {
				auto *head = RowLabel(title);
				head->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(su::Colors::TAG_COLOR));
				bv->addWidget(head);
			}
			build(bv, obs_property_group_content(p), depth + 1);
			into->addWidget(box);
			grid = newGrid();
			row = 0;
			continue;
		}
		addProperty(grid, row++, p, into, depth);
	}
	if (!any && depth == 0)
		into->addWidget(DimLabel(T("OverlayControls.All.NoSettings")));
}

void PropertiesForm::addProperty(QGridLayout *grid, int row, obs_property_t *p, QVBoxLayout *, int)
{
	const QString name = QString::fromUtf8(obs_property_name(p));
	const QByteArray key = name.toUtf8();
	const QString desc = QString::fromUtf8(obs_property_description(p));
	const QString help = QString::fromUtf8(obs_property_long_description(p));
	const bool enabled = obs_property_enabled(p);
	obs_data_t *s = settings();

	// Writes one key and lets OBS react (modified callbacks).
	auto set = [this, name](std::function<void(obs_data_t *, const char *)> fill) {
		obs_data_t *u = obs_data_create();
		fill(u, name.toUtf8().constData());
		obs_source_update(m_source, u);
		obs_data_release(u);
		changed(name);
	};

	QWidget *editor = nullptr;
	bool spanning = false;

	switch (obs_property_get_type(p)) {
	case OBS_PROPERTY_BOOL: {
		auto *check = new su::IOSCheckBox(desc);
		check->setStyleSheet(QString("QCheckBox{color:%1;}").arg(su::Colors::TEXT_PRIMARY));
		check->setChecked(obs_data_get_bool(s, key.constData()));
		QObject::connect(check, &QCheckBox::toggled, this,
				 [set](bool on) { set([on](obs_data_t *d, const char *k) { obs_data_set_bool(d, k, on); }); });
		editor = check;
		spanning = true;
		break;
	}
	case OBS_PROPERTY_INT: {
		const int min = obs_property_int_min(p);
		const int max = obs_property_int_max(p);
		const int step = std::max(1, obs_property_int_step(p));
		auto *spin = new su::MacSpinBox();
		spin->setOnCard(true);
		spin->setRange(min, max);
		spin->setSingleStep(step);
		spin->setKeyboardTracking(false);
		spin->setSuffix(QString::fromUtf8(obs_property_int_suffix(p)));
		spin->setValue(int(obs_data_get_int(s, key.constData())));
		spin->setAccessibleName(desc);
		if (obs_property_int_type(p) == OBS_NUMBER_SLIDER) {
			auto *w = new QWidget();
			auto *h = new QHBoxLayout(w);
			h->setContentsMargins(0, 0, 0, 0);
			h->setSpacing(su::S(10));
			auto *slider = new PanelSlider();
			slider->setRange(min, max);
			slider->setSingleStep(step);
			slider->setValue(spin->value());
			slider->setAccessibleName(desc);
			spin->setFixedWidth(su::S(110));
			h->addWidget(slider, 1);
			h->addWidget(spin);
			QObject::connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
			QObject::connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), slider, [slider](int v) {
				const QSignalBlocker b(slider);
				slider->setValue(v);
			});
			editor = w;
		} else {
			spin->setFixedWidth(su::S(140));
			editor = spin;
		}
		QObject::connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [set](int v) {
			set([v](obs_data_t *d, const char *k) { obs_data_set_int(d, k, v); });
		});
		break;
	}
	case OBS_PROPERTY_FLOAT: {
		const double min = obs_property_float_min(p);
		const double max = obs_property_float_max(p);
		const double step = obs_property_float_step(p) > 0 ? obs_property_float_step(p) : 0.01;
		auto *spin = new PanelDoubleSpin();
		spin->setRange(min, max);
		spin->setDecimals(DecimalsFor(step));
		spin->setSingleStep(step);
		spin->setSuffix(QString::fromUtf8(obs_property_float_suffix(p)));
		spin->setValue(obs_data_get_double(s, key.constData()));
		spin->setAccessibleName(desc);
		if (obs_property_float_type(p) == OBS_NUMBER_SLIDER) {
			auto *w = new QWidget();
			auto *h = new QHBoxLayout(w);
			h->setContentsMargins(0, 0, 0, 0);
			h->setSpacing(su::S(10));
			auto *slider = new PanelSlider();
			const int steps = std::clamp(int(std::lround((max - min) / step)), 1, 10000);
			slider->setRange(0, steps);
			auto toPos = [min, max, steps](double v) {
				return max > min ? int(std::lround((v - min) / (max - min) * steps)) : 0;
			};
			slider->setValue(toPos(spin->value()));
			slider->setAccessibleName(desc);
			spin->setFixedWidth(su::S(130));
			h->addWidget(slider, 1);
			h->addWidget(spin);
			QObject::connect(slider, &QSlider::valueChanged, spin, [spin, min, max, steps](int pos) {
				spin->setValue(min + (max - min) * pos / steps);
			});
			QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), slider,
					 [slider, toPos](double v) {
						 const QSignalBlocker b(slider);
						 slider->setValue(toPos(v));
					 });
			editor = w;
		} else {
			spin->setFixedWidth(su::S(150));
			editor = spin;
		}
		QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [set](double v) {
			set([v](obs_data_t *d, const char *k) { obs_data_set_double(d, k, v); });
		});
		break;
	}
	case OBS_PROPERTY_TEXT: {
		const enum obs_text_type tt = obs_property_text_type(p);
		if (tt == OBS_TEXT_INFO) {
			const char *colour = su::Colors::TEXT_SECONDARY;
			if (obs_property_text_info_type(p) == OBS_TEXT_INFO_WARNING)
				colour = su::Colors::COLOR_WARNING;
			else if (obs_property_text_info_type(p) == OBS_TEXT_INFO_ERROR)
				colour = su::Colors::COLOR_DANGER;
			const QString value = QString::fromUtf8(obs_data_get_string(s, key.constData()));
			auto *info = su::makeLabel(value.isEmpty() ? desc : desc + "  " + value, su::Sizes::FONT_SIZE_SMALL,
						   500, colour);
			info->setTextFormat(Qt::PlainText);
			editor = info;
			spanning = true;
			break;
		}
		if (tt == OBS_TEXT_MULTILINE) {
			auto *edit = new QPlainTextEdit(QString::fromUtf8(obs_data_get_string(s, key.constData())));
			// The debounce timer belongs to its editor, so a rebuild that
			// deletes the editor takes a pending write with it.
			auto *timer = new QTimer(edit);
			timer->setSingleShot(true);
			timer->setInterval(350);
			edit->setStyleSheet(su::plainTextStyle(true));
			edit->setMinimumHeight(su::S(96));
			edit->setMaximumHeight(su::S(220));
			edit->setAccessibleName(desc);
			if (obs_property_text_monospace(p))
				edit->setFont(QFont(QStringLiteral("Consolas")));
			QObject::connect(edit, &QPlainTextEdit::textChanged, timer, [timer]() { timer->start(); });
			QObject::connect(timer, &QTimer::timeout, edit, [set, edit]() {
				const QByteArray t = edit->toPlainText().toUtf8();
				set([t](obs_data_t *d, const char *k) { obs_data_set_string(d, k, t.constData()); });
			});
			editor = edit;
		} else {
			auto *edit = new QLineEdit(QString::fromUtf8(obs_data_get_string(s, key.constData())));
			edit->setStyleSheet(su::lineEditStyle(true));
			edit->setFixedHeight(su::S(28));
			edit->setAccessibleName(desc);
			auto *timer = new QTimer(edit);
			timer->setSingleShot(true);
			timer->setInterval(350);
			if (tt == OBS_TEXT_PASSWORD)
				edit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
			QObject::connect(edit, &QLineEdit::textEdited, timer, [timer]() { timer->start(); });
			QObject::connect(timer, &QTimer::timeout, edit, [set, edit]() {
				const QByteArray t = edit->text().toUtf8();
				set([t](obs_data_t *d, const char *k) { obs_data_set_string(d, k, t.constData()); });
			});
			editor = edit;
		}
		break;
	}
	case OBS_PROPERTY_PATH: {
		auto *w = new QWidget();
		auto *h = new QHBoxLayout(w);
		h->setContentsMargins(0, 0, 0, 0);
		h->setSpacing(su::S(8));
		auto *edit = new QLineEdit(QString::fromUtf8(obs_data_get_string(s, key.constData())));
		edit->setStyleSheet(su::lineEditStyle(true));
		edit->setFixedHeight(su::S(28));
		edit->setAccessibleName(desc);
		auto *browse = new su::PillButton(T("OverlayControls.All.Browse"), "neutral");
		h->addWidget(edit, 1);
		h->addWidget(browse);
		const enum obs_path_type pt = obs_property_path_type(p);
		const QString filter = QString::fromUtf8(obs_property_path_filter(p));
		const QString defaultPath = QString::fromUtf8(obs_property_path_default_path(p));
		QObject::connect(edit, &QLineEdit::editingFinished, this, [set, edit]() {
			const QByteArray t = edit->text().toUtf8();
			set([t](obs_data_t *d, const char *k) { obs_data_set_string(d, k, t.constData()); });
		});
		QObject::connect(browse, &QPushButton::clicked, this, [this, set, edit, pt, filter, defaultPath, desc]() {
			const QString start = edit->text().isEmpty() ? defaultPath : edit->text();
			QString picked;
			if (pt == OBS_PATH_DIRECTORY)
				picked = QFileDialog::getExistingDirectory(this, desc, start);
			else if (pt == OBS_PATH_FILE_SAVE)
				picked = QFileDialog::getSaveFileName(this, desc, start, filter);
			else
				picked = QFileDialog::getOpenFileName(this, desc, start, filter);
			if (picked.isEmpty())
				return;
			edit->setText(picked);
			const QByteArray t = picked.toUtf8();
			set([t](obs_data_t *d, const char *k) { obs_data_set_string(d, k, t.constData()); });
		});
		editor = w;
		break;
	}
	case OBS_PROPERTY_LIST: {
		const bool editable = obs_property_list_type(p) == OBS_COMBO_TYPE_EDITABLE;
		QComboBox *combo = nullptr;
		if (editable) {
			combo = EditableCombo(true);
		} else {
			auto *mac = new su::MacComboBox();
			mac->setOnCard(true);
			mac->setStyleSheet(su::comboStyle(true));
			combo = mac;
		}
		su::makeComboAnimated(combo);
		combo->setAccessibleName(desc);
		const enum obs_combo_format fmt = obs_property_list_format(p);
		const size_t count = obs_property_list_item_count(p);
		for (size_t i = 0; i < count; ++i) {
			QVariant data;
			switch (fmt) {
			case OBS_COMBO_FORMAT_INT:
				data = qlonglong(obs_property_list_item_int(p, i));
				break;
			case OBS_COMBO_FORMAT_FLOAT:
				data = obs_property_list_item_float(p, i);
				break;
			case OBS_COMBO_FORMAT_BOOL:
				data = obs_property_list_item_bool(p, i);
				break;
			default:
				data = QString::fromUtf8(obs_property_list_item_string(p, i));
				break;
			}
			combo->addItem(QString::fromUtf8(obs_property_list_item_name(p, i)), data);
			if (obs_property_list_item_disabled(p, i))
				if (auto *model = qobject_cast<QStandardItemModel *>(combo->model()))
					if (QStandardItem *it = model->item(int(i)))
						it->setEnabled(false);
		}
		QVariant current;
		switch (fmt) {
		case OBS_COMBO_FORMAT_INT:
			current = qlonglong(obs_data_get_int(s, key.constData()));
			break;
		case OBS_COMBO_FORMAT_FLOAT:
			current = obs_data_get_double(s, key.constData());
			break;
		case OBS_COMBO_FORMAT_BOOL:
			current = obs_data_get_bool(s, key.constData());
			break;
		default:
			current = QString::fromUtf8(obs_data_get_string(s, key.constData()));
			break;
		}
		int index = -1;
		for (int i = 0; i < combo->count(); ++i)
			if (combo->itemData(i) == current)
				index = i;
		if (editable) {
			if (index < 0)
				combo->setEditText(current.toString());
		}
		if (index >= 0)
			combo->setCurrentIndex(index);
		else if (!editable)
			combo->setCurrentIndex(-1);
		QObject::connect(combo, QOverload<int>::of(&QComboBox::activated), this, [set, combo, fmt](int i) {
			const QVariant v = combo->itemData(i);
			set([v, fmt](obs_data_t *d, const char *k) {
				switch (fmt) {
				case OBS_COMBO_FORMAT_INT:
					obs_data_set_int(d, k, v.toLongLong());
					break;
				case OBS_COMBO_FORMAT_FLOAT:
					obs_data_set_double(d, k, v.toDouble());
					break;
				case OBS_COMBO_FORMAT_BOOL:
					obs_data_set_bool(d, k, v.toBool());
					break;
				default:
					obs_data_set_string(d, k, v.toString().toUtf8().constData());
					break;
				}
			});
		});
		if (editable && fmt == OBS_COMBO_FORMAT_STRING) {
			auto *timer = new QTimer(combo);
			timer->setSingleShot(true);
			timer->setInterval(400);
			QObject::connect(combo, &QComboBox::editTextChanged, timer, [timer]() { timer->start(); });
			QObject::connect(timer, &QTimer::timeout, combo, [set, combo]() {
				const QByteArray t = combo->currentText().toUtf8();
				set([t](obs_data_t *d, const char *k) { obs_data_set_string(d, k, t.constData()); });
			});
		}
		editor = combo;
		break;
	}
	case OBS_PROPERTY_COLOR:
	case OBS_PROPERTY_COLOR_ALPHA: {
		const bool alpha = obs_property_get_type(p) == OBS_PROPERTY_COLOR_ALPHA;
		QColor c = ObsToQColor(obs_data_get_int(s, key.constData()));
		if (!alpha)
			c.setAlpha(255);
		auto *swatch = new QPushButton(HexOf(c, alpha));
		swatch->setStyleSheet(SwatchQss(c));
		swatch->setFixedWidth(su::S(132));
		swatch->setCursor(Qt::PointingHandCursor);
		swatch->setAccessibleName(desc);
		QPointer<QPushButton> guard(swatch);
		QObject::connect(swatch, &QPushButton::clicked, this, [this, set, guard, desc, alpha, key]() {
			obs_data_t *cur = settings();
			const long long before = obs_data_get_int(cur, key.constData());
			obs_data_release(cur);
			QColor initial = ObsToQColor(before);
			if (!alpha)
				initial.setAlpha(255);
			auto show = [guard, alpha](const QColor &c) {
				if (guard) {
					guard->setText(HexOf(c, alpha));
					guard->setStyleSheet(SwatchQss(c));
				}
			};
			// The picker can outlive this form (a rebuild, a closed window);
			// writes only go through while the form still exists.
			QPointer<PropertiesForm> self(this);
			PickColour(
				desc, initial, alpha,
				[self, set, show](const QColor &c) {
					if (!self)
						return;
					show(c);
					const long long v = QColorToObs(c);
					set([v](obs_data_t *d, const char *k) { obs_data_set_int(d, k, v); });
				},
				[self, set, show, before, alpha]() {
					if (!self)
						return;
					QColor c = ObsToQColor(before);
					if (!alpha)
						c.setAlpha(255);
					show(c);
					set([before](obs_data_t *d, const char *k) { obs_data_set_int(d, k, before); });
				});
		});
		auto *w = new QWidget();
		auto *h = new QHBoxLayout(w);
		h->setContentsMargins(0, 0, 0, 0);
		h->addWidget(swatch);
		h->addStretch();
		editor = w;
		break;
	}
	case OBS_PROPERTY_BUTTON: {
		auto *button = new su::PillButton(desc, "neutral");
		button->setAccessibleName(desc);
		const bool url = obs_property_button_type(p) == OBS_BUTTON_URL;
		const QString link = QString::fromUtf8(obs_property_button_url(p));
		QObject::connect(button, &QPushButton::clicked, this, [this, name, url, link]() {
			if (url) {
				QDesktopServices::openUrl(QUrl(link));
				return;
			}
			obs_property_t *prop = obs_properties_get(m_props, name.toUtf8().constData());
			if (prop && obs_property_button_clicked(prop, m_source) && !m_rebuildQueued) {
				m_rebuildQueued = true;
				QTimer::singleShot(0, this, [this]() { rebuild(); });
			}
		});
		auto *w = new QWidget();
		auto *h = new QHBoxLayout(w);
		h->setContentsMargins(0, 0, 0, 0);
		h->addWidget(button);
		h->addStretch();
		editor = w;
		spanning = true;
		break;
	}
	case OBS_PROPERTY_FONT: {
		obs_data_t *font = obs_data_get_obj(s, key.constData());
		const QString face = QString::fromUtf8(font ? obs_data_get_string(font, "face") : "");
		const int size = font ? int(obs_data_get_int(font, "size")) : 0;
		const QString style = QString::fromUtf8(font ? obs_data_get_string(font, "style") : "");
		const int flags = font ? int(obs_data_get_int(font, "flags")) : 0;
		obs_data_release(font);
		auto *button = new su::PillButton(QString("%1 %2 %3").arg(face, style).arg(size).simplified(), "neutral");
		button->setAccessibleName(desc);
		QObject::connect(button, &QPushButton::clicked, this, [this, set, face, size, style, flags, button]() {
			QFont current(face, size > 0 ? size : 12);
			current.setStyleName(style);
			current.setBold(flags & 1);
			current.setItalic(flags & 2);
			current.setUnderline(flags & 4);
			current.setStrikeOut(flags & 8);
			bool ok = false;
			const QFont f = QFontDialog::getFont(&ok, current, this);
			if (!ok)
				return;
			const QByteArray fFace = f.family().toUtf8();
			const QByteArray fStyle = f.styleName().toUtf8();
			const int fSize = f.pointSize() > 0 ? f.pointSize() : size;
			const int fFlags = (f.bold() ? 1 : 0) | (f.italic() ? 2 : 0) | (f.underline() ? 4 : 0) |
					   (f.strikeOut() ? 8 : 0);
			button->setText(QString("%1 %2 %3").arg(f.family(), f.styleName()).arg(fSize).simplified());
			set([fFace, fStyle, fSize, fFlags](obs_data_t *d, const char *k) {
				obs_data_t *obj = obs_data_create();
				obs_data_set_string(obj, "face", fFace.constData());
				obs_data_set_string(obj, "style", fStyle.constData());
				obs_data_set_int(obj, "size", fSize);
				obs_data_set_int(obj, "flags", fFlags);
				obs_data_set_obj(d, k, obj);
				obs_data_release(obj);
			});
		});
		auto *w = new QWidget();
		auto *h = new QHBoxLayout(w);
		h->setContentsMargins(0, 0, 0, 0);
		h->addWidget(button);
		h->addStretch();
		editor = w;
		break;
	}
	default: {
		// Editable lists, frame rates and anything newer: OBS's own dialog
		// handles these properly, so hand over rather than half-support them.
		auto *open = new su::PillButton(T("OverlayControls.All.EditInObs"), "outline");
		open->setAccessibleName(desc);
		QObject::connect(open, &QPushButton::clicked, this, [this]() {
			obs_source_t *parent = obs_filter_get_parent(m_source);
			if (parent)
				obs_frontend_open_source_filters(parent);
			else
				obs_frontend_open_source_properties(m_source);
		});
		auto *w = new QWidget();
		auto *h = new QHBoxLayout(w);
		h->setContentsMargins(0, 0, 0, 0);
		h->addWidget(open);
		h->addStretch();
		editor = w;
		break;
	}
	}
	obs_data_release(s);

	editor->setEnabled(enabled);
	editor->setToolTip(help);
	if (spanning) {
		grid->addWidget(editor, row, 0, 1, 2);
	} else {
		grid->addWidget(RowLabel(desc, help), row, 0, Qt::AlignVCenter);
		grid->addWidget(editor, row, 1);
	}
}

void PropertiesForm::changed(const QString &name)
{
	obs_property_t *p = m_props ? obs_properties_get(m_props, name.toUtf8().constData()) : nullptr;
	if (!p)
		return;
	obs_data_t *s = settings();
	const bool reshape = obs_property_modified(p, s);
	obs_data_release(s);
	if (reshape && !m_rebuildQueued) {
		m_rebuildQueued = true;
		QTimer::singleShot(0, this, [this]() { rebuild(); });
	}
}

// ================================================================ AllSettingsPage

namespace {

// A filter card: its name, an on/off switch, and its settings, folded away
// until opened. The open state survives rebuilds.
QWidget *FilterCard(obs_source_t *filter, QSet<QString> &open, const QString &key)
{
	auto *card = new QFrame();
	card->setObjectName("ocFilter");
	card->setStyleSheet(
		su::scale_qss(QString("QFrame#ocFilter{background:%1;border-radius:12px;}").arg(su::Colors::BG_PRIMARY)));
	auto *v = new QVBoxLayout(card);
	v->setContentsMargins(su::S(12), su::S(8), su::S(12), su::S(8));
	v->setSpacing(su::S(8));

	auto *head = new QHBoxLayout();
	head->setSpacing(su::S(8));
	auto *fold = new QPushButton();
	fold->setCursor(Qt::PointingHandCursor);
	fold->setFlat(true);
	const QString name = QString::fromUtf8(obs_source_get_name(filter));
	const QString kind = QString::fromUtf8(obs_source_get_display_name(obs_source_get_unversioned_id(filter)));
	fold->setAccessibleName(name);
	fold->setStyleSheet(su::scale_qss(
		QString("QPushButton{color:%1;background:transparent;border:none;text-align:left;font-size:13px;"
			"font-weight:700;padding:4px 0;}QPushButton:hover{color:%2;}")
			.arg(su::Colors::TEXT_PRIMARY, su::Colors::PRIMARY_LIGHT)));
	head->addWidget(fold, 1);
	auto *kindLabel = DimLabel(kind);
	kindLabel->setWordWrap(false);
	head->addWidget(kindLabel);
	auto *on = new su::IOSCheckBox();
	on->setChecked(obs_source_enabled(filter));
	on->setAccessibleName(T("OverlayControls.All.FilterEnabled").arg(name));
	on->setToolTip(T("OverlayControls.All.FilterEnabledTip"));
	head->addWidget(on);
	v->addLayout(head);

	auto *body = new PropertiesForm(filter);
	v->addWidget(body);
	const bool isOpen = open.contains(key);
	body->setVisible(isOpen);
	fold->setText(QString::fromUtf8(isOpen ? "▾  " : "▸  ") + name);

	OBSWeakSource weak = OBSGetWeakRef(filter);
	QObject::connect(on, &QCheckBox::toggled, card, [weak](bool enabled) {
		OBSSourceAutoRelease f = OBSGetStrongRef(weak);
		if (f)
			obs_source_set_enabled(f, enabled);
	});
	QObject::connect(fold, &QPushButton::clicked, card, [fold, body, name, &open, key]() {
		const bool show = !body->isVisible();
		body->setVisible(show);
		fold->setText(QString::fromUtf8(show ? "▾  " : "▸  ") + name);
		if (show)
			open.insert(key);
		else
			open.remove(key);
	});
	return card;
}

QSet<QString> &OpenFilters()
{
	static QSet<QString> open;
	return open;
}

} // namespace

AllSettingsPage::AllSettingsPage(const QString &overlayId, const oc::OverlayInfo &info, QWidget *parent)
	: QWidget(parent), m_overlay(overlayId), m_info(info)
{
	auto *outer = new QHBoxLayout(this);
	outer->setContentsMargins(su::S(16), su::S(16), su::S(16), su::S(16));
	outer->setSpacing(su::S(14));

	auto *treeCard = new QFrame();
	treeCard->setObjectName("ocTreeCard");
	treeCard->setStyleSheet(
		su::scale_qss(QString("QFrame#ocTreeCard{background:%1;border-radius:14px;}").arg(su::Colors::BG_PRIMARY)));
	treeCard->setFixedWidth(su::S(270));
	auto *tv = new QVBoxLayout(treeCard);
	tv->setContentsMargins(su::S(6), su::S(8), su::S(6), su::S(8));
	auto *tree = new su::TreeWidget();
	tree->setAccessibleName(T("OverlayControls.All.Tree"));
	m_tree = tree;
	tv->addWidget(m_tree);
	outer->addWidget(treeCard);

	m_scroll = new QScrollArea();
	m_scroll->setWidgetResizable(true);
	m_scroll->setFrameShape(QFrame::NoFrame);
	m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_scroll->setStyleSheet("QScrollArea{background:transparent;border:none;}");
	su::useScrollBars(m_scroll);
	outer->addWidget(m_scroll, 1);

	QObject::connect(m_tree, &QTreeWidget::currentItemChanged, this,
			 [this](QTreeWidgetItem *item, QTreeWidgetItem *) { showItem(item); });
	// Screen readers select without moving the current item.
	QObject::connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
		const QList<QTreeWidgetItem *> sel = m_tree->selectedItems();
		if (!sel.isEmpty() && sel.first() != m_tree->currentItem())
			m_tree->setCurrentItem(sel.first());
	});
	fillTree();
}

void AllSettingsPage::fillTree()
{
	m_tree->clear();
	QTreeWidgetItem *select = nullptr;
	std::function<void(QTreeWidgetItem *, obs_scene_t *, const QString &)> addItems =
		[&](QTreeWidgetItem *parent, obs_scene_t *scene, const QString &sceneName) {
			struct Ctx {
				QList<obs_sceneitem_t *> items;
			} ctx;
			obs_scene_enum_items(
				scene,
				[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
					static_cast<Ctx *>(param)->items.prepend(item); // top of the scene list first
					return true;
				},
				&ctx);
			for (obs_sceneitem_t *item : ctx.items) {
				obs_source_t *src = obs_sceneitem_get_source(item);
				const QString name = QString::fromUtf8(obs_source_get_name(src));
				auto *node = new QTreeWidgetItem(parent, QStringList{ElidedName(name)});
				node->setData(0, Qt::UserRole, sceneName);
				node->setData(0, Qt::UserRole + 1, name);
				node->setData(0, Qt::UserRole + 2, qlonglong(obs_sceneitem_get_id(item)));
				node->setToolTip(0, QString("%1\n%2").arg(
						  name, QString::fromUtf8(obs_source_get_display_name(obs_source_get_unversioned_id(src)))));
				if (!obs_sceneitem_visible(item))
					node->setForeground(0, QColor(su::Colors::TEXT_MUTED));
				if (sceneName == m_scene && name == m_source)
					select = node;
				if (obs_sceneitem_is_group(item))
					addItems(node, obs_group_from_source(src), name);
			}
		};
	for (const QString &sceneName : m_info.scenes) {
		obs_source_t *sceneSource = obs_get_source_by_name(sceneName.toUtf8().constData());
		if (!sceneSource)
			continue;
		auto *node = new QTreeWidgetItem(m_tree, QStringList{sceneName});
		node->setData(0, Qt::UserRole, QString());
		node->setData(0, Qt::UserRole + 1, sceneName);
		node->setData(0, Qt::UserRole + 2, qlonglong(0));
		QFont f = node->font(0);
		f.setBold(true);
		node->setFont(0, f);
		if (obs_scene_t *scene = obs_scene_from_source(sceneSource))
			addItems(node, scene, sceneName);
		if (m_scene.isEmpty() && m_source == sceneName)
			select = node;
		obs_source_release(sceneSource);
	}
	m_tree->expandToDepth(0);
	if (!select && m_tree->topLevelItemCount())
		select = m_tree->topLevelItem(0);
	if (select)
		m_tree->setCurrentItem(select);
	m_dirty = false;
}

void AllSettingsPage::showItem(QTreeWidgetItem *item)
{
	if (!item)
		return;
	const int keepScroll = (m_scene == item->data(0, Qt::UserRole).toString() &&
				m_source == item->data(0, Qt::UserRole + 1).toString())
				       ? m_scroll->verticalScrollBar()->value()
				       : 0;
	m_scene = item->data(0, Qt::UserRole).toString();
	m_source = item->data(0, Qt::UserRole + 1).toString();
	m_itemId = item->data(0, Qt::UserRole + 2).toLongLong();

	auto *page = new QWidget();
	page->setObjectName("ocAllPage");
	page->setStyleSheet(QString("QWidget#ocAllPage{background:%1;}").arg(su::Colors::BG_DARKEST));
	auto *v = new QVBoxLayout(page);
	v->setContentsMargins(0, 0, su::S(6), 0);
	v->setSpacing(su::S(12));

	obs_source_t *src = obs_get_source_by_name(m_source.toUtf8().constData());
	if (!src) {
		v->addWidget(DimLabel(T("OverlayControls.All.Gone")));
		v->addStretch();
		m_scroll->setWidget(page);
		return;
	}
	const bool isScene = obs_scene_from_source(src) || obs_group_from_source(src);

	// Header: name, kind and the escape hatches.
	auto *head = new QHBoxLayout();
	auto *titles = new QVBoxLayout();
	titles->setSpacing(su::S(2));
	auto *title = su::makeLabel(m_source, su::Sizes::FONT_SIZE_LARGE, 700, su::Colors::TEXT_PRIMARY);
	title->setWordWrap(true);
	titles->addWidget(title);
	titles->addWidget(DimLabel(QString::fromUtf8(obs_source_get_display_name(obs_source_get_unversioned_id(src)))));
	head->addLayout(titles, 1);
	auto *openObs = new su::PillButton(T("OverlayControls.All.OpenInObs"), "neutral");
	auto *reset = new su::PillButton(T("OverlayControls.All.ResetSource"), "outline");
	reset->setEnabled(oc::HasSnapshot(m_overlay, m_source));
	reset->setToolTip(T("OverlayControls.All.ResetSourceTip"));
	head->addWidget(openObs, 0, Qt::AlignTop);
	head->addWidget(reset, 0, Qt::AlignTop);
	v->addLayout(head);

	OBSWeakSource weak = OBSGetWeakRef(src);
	QObject::connect(openObs, &QPushButton::clicked, page, [weak, isScene]() {
		OBSSourceAutoRelease s = OBSGetStrongRef(weak);
		if (!s)
			return;
		if (isScene)
			obs_frontend_open_source_filters(s);
		else
			obs_frontend_open_source_properties(s);
	});
	const QString sourceName = m_source;
	QObject::connect(reset, &QPushButton::clicked, page, [this, sourceName]() {
		QPointer<AllSettingsPage> self(this);
		su::confirm(this, T("OverlayControls.All.ResetSource"),
			    T("OverlayControls.All.ResetSourceMessage").arg(sourceName.toHtmlEscaped()),
			    T("OverlayControls.All.ResetSource"), "danger", [self, sourceName]() {
				    if (!self)
					    return;
				    oc::ResetSource(self->m_overlay, sourceName);
				    if (self->onStatus)
					    self->onStatus(T("OverlayControls.All.ResetDone").arg(sourceName));
			    });
	});

	// Position and size of this item in its scene.
	if (!m_scene.isEmpty()) {
		QVBoxLayout *card = su::sectionCard(v, T("OverlayControls.All.Transform"));
		auto *grid = new QGridLayout();
		grid->setContentsMargins(0, su::S(6), 0, 0);
		grid->setHorizontalSpacing(su::S(10));
		grid->setVerticalSpacing(su::S(8));
		card->addLayout(grid);

		struct obs_transform_info info = {};
		struct obs_sceneitem_crop crop = {};
		bool visible = true;
		QSizeF size;
		WithSceneItem(m_scene, m_itemId, m_source, [&](obs_sceneitem_t *it) {
			obs_sceneitem_get_info2(it, &info);
			obs_sceneitem_get_crop(it, &crop);
			visible = obs_sceneitem_visible(it);
			obs_source_t *s = obs_sceneitem_get_source(it);
			size = QSizeF((int(obs_source_get_width(s)) - crop.left - crop.right) * info.scale.x,
				      (int(obs_source_get_height(s)) - crop.top - crop.bottom) * info.scale.y);
		});

		const QString scene = m_scene;
		const qint64 id = m_itemId;
		auto withItem = [scene, id, sourceName](std::function<void(obs_sceneitem_t *)> fn) {
			WithSceneItem(scene, id, sourceName, fn);
		};
		auto addSpin = [&](int row, int col, const QString &label, double value, double min, double max, int decimals,
				   const QString &suffix, std::function<void(obs_sceneitem_t *, double)> apply) {
			auto *spin = new PanelDoubleSpin();
			spin->setRange(min, max);
			spin->setDecimals(decimals);
			spin->setSuffix(suffix);
			spin->setValue(value);
			spin->setAccessibleName(label);
			grid->addWidget(RowLabel(label), row, col);
			grid->addWidget(spin, row, col + 1);
			QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), page,
					 [withItem, apply](double v) { withItem([&](obs_sceneitem_t *it) { apply(it, v); }); });
		};
		auto setInfo = [](obs_sceneitem_t *it, const std::function<void(obs_transform_info &)> &edit) {
			struct obs_transform_info i;
			obs_sceneitem_get_info2(it, &i);
			edit(i);
			obs_sceneitem_set_info2(it, &i);
		};
		addSpin(0, 0, T("OverlayControls.Layout.X"), info.pos.x, -10000, 10000, 1, " px",
			[setInfo](obs_sceneitem_t *it, double v) { setInfo(it, [v](obs_transform_info &i) { i.pos.x = float(v); }); });
		addSpin(0, 2, T("OverlayControls.Layout.Y"), info.pos.y, -10000, 10000, 1, " px",
			[setInfo](obs_sceneitem_t *it, double v) { setInfo(it, [v](obs_transform_info &i) { i.pos.y = float(v); }); });
		addSpin(1, 0, T("OverlayControls.All.ScaleX"), info.scale.x, -20, 20, 4, QString(),
			[setInfo](obs_sceneitem_t *it, double v) { setInfo(it, [v](obs_transform_info &i) { i.scale.x = float(v); }); });
		addSpin(1, 2, T("OverlayControls.All.ScaleY"), info.scale.y, -20, 20, 4, QString(),
			[setInfo](obs_sceneitem_t *it, double v) { setInfo(it, [v](obs_transform_info &i) { i.scale.y = float(v); }); });
		addSpin(2, 0, T("OverlayControls.All.Rotation"), info.rot, -360, 360, 1, QString::fromUtf8("°"),
			[setInfo](obs_sceneitem_t *it, double v) { setInfo(it, [v](obs_transform_info &i) { i.rot = float(v); }); });
		auto setCrop = [](obs_sceneitem_t *it, int which, int v) {
			struct obs_sceneitem_crop c;
			obs_sceneitem_get_crop(it, &c);
			(which == 0 ? c.left : which == 1 ? c.top : which == 2 ? c.right : c.bottom) = v;
			obs_sceneitem_set_crop(it, &c);
		};
		const QStringList cropLabels = {T("OverlayControls.All.CropLeft"), T("OverlayControls.All.CropTop"),
						T("OverlayControls.All.CropRight"), T("OverlayControls.All.CropBottom")};
		const int cropValues[4] = {crop.left, crop.top, crop.right, crop.bottom};
		for (int i = 0; i < 4; ++i)
			addSpin(3 + i / 2, (i % 2) * 2, cropLabels[i], cropValues[i], 0, 10000, 0, " px",
				[setCrop, i](obs_sceneitem_t *it, double v) { setCrop(it, i, int(std::lround(v))); });

		auto *vis = new su::IOSCheckBox(T("OverlayControls.Layout.Visible"));
		vis->setStyleSheet(QString("QCheckBox{color:%1;}").arg(su::Colors::TEXT_PRIMARY));
		vis->setChecked(visible);
		vis->setAccessibleName(T("OverlayControls.Layout.Visible"));
		grid->addWidget(vis, 2, 2, 1, 2);
		QObject::connect(vis, &QCheckBox::toggled, page, [withItem](bool on) {
			withItem([on](obs_sceneitem_t *it) { obs_sceneitem_set_visible(it, on); });
		});
		grid->setColumnStretch(1, 1);
		grid->setColumnStretch(3, 1);
		auto *sizeNote = DimLabel(T("OverlayControls.All.Size")
						  .arg(std::fabs(size.width()), 0, 'f', 0)
						  .arg(std::fabs(size.height()), 0, 'f', 0));
		card->addWidget(sizeNote);
		card->addWidget(DimLabel(T("OverlayControls.All.TransformNote")));
	}

	// The source's own settings.
	if (!isScene) {
		QVBoxLayout *card = su::sectionCard(v, T("OverlayControls.All.Settings"));
		card->addSpacing(su::S(4));
		card->addWidget(new PropertiesForm(src));
	}

	// Its filters, in order.
	struct FilterList {
		QList<obs_source_t *> filters;
	} list;
	obs_source_enum_filters(
		src, [](obs_source_t *, obs_source_t *f, void *param) { static_cast<FilterList *>(param)->filters.append(f); },
		&list);
	QVBoxLayout *filtersCard = su::sectionCard(v, T("OverlayControls.All.Filters"));
	if (list.filters.isEmpty())
		filtersCard->addWidget(DimLabel(T("OverlayControls.All.NoFilters")));
	for (obs_source_t *f : list.filters)
		filtersCard->addWidget(FilterCard(f, OpenFilters(), m_source + "\n" + QString::fromUtf8(obs_source_get_name(f))));
	v->addStretch();
	obs_source_release(src);

	m_scroll->setWidget(page);
	QTimer::singleShot(0, m_scroll, [scroll = QPointer<QScrollArea>(m_scroll), keepScroll]() {
		if (scroll)
			scroll->verticalScrollBar()->setValue(keepScroll);
	});
}

void AllSettingsPage::refresh()
{
	if (!isVisible()) {
		m_dirty = true;
		return;
	}
	fillTree();
}

void AllSettingsPage::showEvent(QShowEvent *e)
{
	QWidget::showEvent(e);
	if (m_dirty)
		fillTree();
}

} // namespace OverlayUI
} // namespace StreamUP
