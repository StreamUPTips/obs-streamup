#include "overlay-layout-page.hpp"
#include "../core/canvas-lookup.hpp"
#include "overlay-ui-common.hpp"

#include <streamup/ui/glass.hpp>
#include <streamup/ui/ios-checkbox.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/section-card.hpp>
#include <streamup/ui/ui-scrollbar.hpp>

#include <obs.h>
#include <graphics/graphics.h>
#include <graphics/vec4.h>

#if !defined(_WIN32) && !defined(__APPLE__)
#include <obs-nix-platform.h>
#endif

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QKeyEvent>
#include <QListWidget>
#include <QApplication>
#include <QMouseEvent>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <utility>

namespace StreamUP {
namespace OverlayUI {

namespace oc = StreamUP::OverlayControls;

namespace {

// The native window OBS draws into, the same way OBS's own preview and
// projectors get theirs (OBSQTDisplay::QTToGSWindow).
bool ToGSWindow(QWindow *window, gs_window &gswindow)
{
#ifdef _WIN32
	gswindow.hwnd = (void *)window->winId();
	return true;
#elif defined(__APPLE__)
	gswindow.view = (id)window->winId();
	return true;
#else
	if (obs_get_nix_platform() != OBS_NIX_PLATFORM_X11_EGL)
		return false; // Wayland needs the platform interface OBS keeps to itself
	gswindow.id = uint32_t(window->winId());
	gswindow.display = obs_get_nix_platform_display();
	return true;
#endif
}

} // namespace

// ---------------------------------------------------------------- preview

// What the draw callback needs, shared with the graphics thread.
struct PreviewState {
	std::mutex lock;
	obs_weak_source_t *scene = nullptr;
	uint32_t cw = 1920;
	uint32_t ch = 1080;
	struct Box {
		QRectF rect;
		bool selected = false;
	};
	QList<Box> boxes;

	~PreviewState() { obs_weak_source_release(scene); }
};

// The Layout preview is a live OBS display, like OBS's own preview: the scene
// is drawn straight onto this widget's window by OBS's render thread every
// frame. The earlier preview rendered the scene off screen a few times a
// second and copied it back to the CPU, which was both choppy and soft, and
// stalled OBS while it waited for the copy.
//
// The part outlines are drawn in the same pass, in canvas pixels. Mouse
// handling stays in Qt.
class ScenePreview : public QWidget {
public:
	struct Frame {
		QString id;
		QString label;
		QRectF rect; // canvas pixels
		bool movable = true;
	};

	ScenePreview()
	{
		setAttribute(Qt::WA_PaintOnScreen);
		setAttribute(Qt::WA_StaticContents);
		setAttribute(Qt::WA_NoSystemBackground);
		setAttribute(Qt::WA_OpaquePaintEvent);
		setAttribute(Qt::WA_DontCreateNativeAncestors);
		setAttribute(Qt::WA_NativeWindow);
		setFocusPolicy(Qt::StrongFocus);
		setMouseTracking(true);
		setMinimumSize(su::S(420), su::S(236));
		setAccessibleName(T("OverlayControls.Layout.Preview"));
	}

	~ScenePreview() override { destroyDisplay(); }

	std::function<void(const QString &)> onSelect;
	std::function<void(double dx, double dy, bool finished)> onDrag; // canvas px from drag start
	std::function<void(double dx, double dy)> onNudge;

	// The scene to show and the canvas it draws on (vertical scenes are tall).
	void setScene(obs_source_t *scene, const QSize &canvas)
	{
		m_canvas = canvas;
		std::lock_guard<std::mutex> l(m_state->lock);
		obs_weak_source_release(m_state->scene);
		m_state->scene = scene ? obs_source_get_weak_source(scene) : nullptr;
		m_state->cw = uint32_t(canvas.width());
		m_state->ch = uint32_t(canvas.height());
	}

	void setFrames(const QList<Frame> &frames, const QString &selected)
	{
		m_frames = frames;
		m_selected = selected;
		pushBoxes();
	}

protected:
	QPaintEngine *paintEngine() const override { return nullptr; }

	void paintEvent(QPaintEvent *) override { createDisplay(); }
	void showEvent(QShowEvent *e) override
	{
		QWidget::showEvent(e);
		createDisplay();
	}
	void resizeEvent(QResizeEvent *e) override
	{
		QWidget::resizeEvent(e);
		createDisplay();
		if (m_display) {
			const QSize px = pixelSize();
			obs_display_resize(m_display, uint32_t(px.width()), uint32_t(px.height()));
		}
	}

	// Same letterboxing as the draw callback, in widget (logical) pixels.
	QRectF target() const
	{
		const QRectF area = QRectF(rect());
		const double k = std::min(area.width() / m_canvas.width(), area.height() / m_canvas.height());
		const QSizeF size(m_canvas.width() * k, m_canvas.height() * k);
		return QRectF(area.center().x() - size.width() / 2, area.center().y() - size.height() / 2, size.width(),
			      size.height());
	}
	double scale() const { return target().width() / m_canvas.width(); }
	QPointF toCanvas(const QPointF &p) const
	{
		const QRectF t = target();
		return (p - t.topLeft()) / scale();
	}

	void mousePressEvent(QMouseEvent *e) override
	{
		setFocus();
		if (e->button() != Qt::LeftButton)
			return;
		const QPointF c = toCanvas(e->position());
		// The smallest frame under the pointer wins, so a nameplate inside
		// its camera panel can be picked on its own.
		const Frame *hit = nullptr;
		for (const Frame &fr : m_frames)
			if (fr.rect.contains(c) && (!hit || fr.rect.width() * fr.rect.height() < hit->rect.width() * hit->rect.height()))
				hit = &fr;
		if (hit) {
			m_selected = hit->id;
			if (onSelect)
				onSelect(hit->id);
			m_dragging = hit->movable;
			m_pressCanvas = c;
			setCursor(m_dragging ? Qt::ClosedHandCursor : Qt::ArrowCursor);
			pushBoxes();
		}
	}
	void mouseMoveEvent(QMouseEvent *e) override
	{
		const QPointF c = toCanvas(e->position());
		if (!m_dragging) {
			bool over = false;
			for (const Frame &fr : m_frames)
				over = over || (fr.rect.contains(c) && fr.movable);
			setCursor(over ? Qt::OpenHandCursor : Qt::ArrowCursor);
			return;
		}
		QPointF d = c - m_pressCanvas;
		if (e->modifiers() & Qt::ShiftModifier) {
			if (std::fabs(d.x()) > std::fabs(d.y()))
				d.setY(0);
			else
				d.setX(0);
		}
		if (onDrag)
			onDrag(std::round(d.x()), std::round(d.y()), false);
	}
	void mouseReleaseEvent(QMouseEvent *e) override
	{
		if (m_dragging && onDrag) {
			QPointF d = toCanvas(e->position()) - m_pressCanvas;
			if (e->modifiers() & Qt::ShiftModifier) {
				if (std::fabs(d.x()) > std::fabs(d.y()))
					d.setY(0);
				else
					d.setX(0);
			}
			onDrag(std::round(d.x()), std::round(d.y()), true);
		}
		m_dragging = false;
		setCursor(Qt::ArrowCursor);
	}
	void keyPressEvent(QKeyEvent *e) override
	{
		const double step = (e->modifiers() & Qt::ShiftModifier) ? 10.0 : 1.0;
		double dx = 0;
		double dy = 0;
		switch (e->key()) {
		case Qt::Key_Left:
			dx = -step;
			break;
		case Qt::Key_Right:
			dx = step;
			break;
		case Qt::Key_Up:
			dy = -step;
			break;
		case Qt::Key_Down:
			dy = step;
			break;
		default:
			QWidget::keyPressEvent(e);
			return;
		}
		if (onNudge && !m_selected.isEmpty())
			onNudge(dx, dy);
	}

private:
	QSize pixelSize() const { return size() * devicePixelRatioF(); }

	void createDisplay()
	{
		if (m_display || !windowHandle() || !windowHandle()->isExposed())
			return;
		const QSize px = pixelSize();
		gs_init_data info = {};
		info.cx = uint32_t(px.width());
		info.cy = uint32_t(px.height());
		info.format = GS_BGRA;
		info.zsformat = GS_ZS_NONE;
		if (!ToGSWindow(windowHandle(), info.window))
			return;
		const QColor bg(su::Colors::BG_DARKEST);
		const uint32_t abgr = 0xFF000000u | (uint32_t(bg.blue()) << 16) | (uint32_t(bg.green()) << 8) | uint32_t(bg.red());
		m_display = obs_display_create(&info, abgr);
		if (m_display)
			obs_display_add_draw_callback(m_display, Draw, m_state.get());
	}

	void destroyDisplay()
	{
		if (!m_display)
			return;
		obs_display_remove_draw_callback(m_display, Draw, m_state.get());
		obs_display_destroy(m_display);
		m_display = nullptr;
	}

	void pushBoxes()
	{
		std::lock_guard<std::mutex> l(m_state->lock);
		m_state->boxes.clear();
		// Selected last so it draws on top.
		for (const Frame &fr : m_frames)
			if (fr.id != m_selected)
				m_state->boxes.append({fr.rect, false});
		for (const Frame &fr : m_frames)
			if (fr.id == m_selected)
				m_state->boxes.append({fr.rect, true});
	}

	// ---- graphics thread

	static void FillRect(gs_eparam_t *colour, const vec4 &c, float x, float y, float w, float h)
	{
		gs_effect_set_vec4(colour, &c);
		gs_matrix_push();
		gs_matrix_translate3f(x, y, 0.0f);
		gs_matrix_scale3f(w, h, 1.0f);
		gs_draw_sprite(nullptr, 0, 1, 1);
		gs_matrix_pop();
	}

	static void OutlineRect(gs_eparam_t *colour, const vec4 &c, const QRectF &r, float t)
	{
		const float x = float(r.x()), y = float(r.y()), w = float(r.width()), h = float(r.height());
		FillRect(colour, c, x, y, w, t);
		FillRect(colour, c, x, y + h - t, w, t);
		FillRect(colour, c, x, y, t, h);
		FillRect(colour, c, x + w - t, y, t, h);
	}

	static void Draw(void *param, uint32_t cx, uint32_t cy)
	{
		auto *st = static_cast<PreviewState *>(param);
		std::lock_guard<std::mutex> l(st->lock);
		obs_source_t *scene = obs_weak_source_get_source(st->scene);
		if (!scene || !st->cw || !st->ch) {
			obs_source_release(scene);
			return;
		}
		const float k = std::min(float(cx) / float(st->cw), float(cy) / float(st->ch));
		const int vw = int(float(st->cw) * k);
		const int vh = int(float(st->ch) * k);
		const int vx = (int(cx) - vw) / 2;
		const int vy = (int(cy) - vh) / 2;

		gs_viewport_push();
		gs_projection_push();
		gs_set_viewport(vx, vy, vw, vh);
		gs_ortho(0.0f, float(st->cw), 0.0f, float(st->ch), -100.0f, 100.0f);

		gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
		gs_eparam_t *colour = gs_effect_get_param_by_name(solid, "color");
		gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

		// The canvas reads black where the scene is transparent, like the
		// program output.
		vec4 black;
		vec4_set(&black, 0.0f, 0.0f, 0.0f, 1.0f);
		gs_technique_begin(tech);
		gs_technique_begin_pass(tech, 0);
		FillRect(colour, black, 0.0f, 0.0f, float(st->cw), float(st->ch));
		gs_technique_end_pass(tech);
		gs_technique_end(tech);

		obs_source_video_render(scene);

		// Outlines a constant on-screen width whatever the zoom.
		gs_blend_state_push();
		gs_enable_blending(true);
		gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);
		gs_technique_begin(tech);
		gs_technique_begin_pass(tech, 0);
		const float px = 1.0f / std::max(k, 0.0001f);
		vec4 dim, sel, fill;
		vec4_set(&dim, 1.0f, 1.0f, 1.0f, 0.45f);
		vec4_set(&sel, 0.25f, 0.62f, 1.0f, 1.0f);
		vec4_set(&fill, 0.16f, 0.59f, 1.0f, 0.12f);
		for (const PreviewState::Box &b : st->boxes) {
			if (b.selected) {
				FillRect(colour, fill, float(b.rect.x()), float(b.rect.y()), float(b.rect.width()),
					 float(b.rect.height()));
				OutlineRect(colour, sel, b.rect, 2.0f * px);
				const float hs = 7.0f * px;
				for (const QPointF &c : {b.rect.topLeft(), b.rect.topRight(), b.rect.bottomLeft(), b.rect.bottomRight()})
					FillRect(colour, sel, float(c.x()) - hs / 2, float(c.y()) - hs / 2, hs, hs);
			} else {
				OutlineRect(colour, dim, b.rect, 1.0f * px);
			}
		}
		gs_technique_end_pass(tech);
		gs_technique_end(tech);
		gs_blend_state_pop();

		gs_projection_pop();
		gs_viewport_pop();
		obs_source_release(scene);
	}

	std::shared_ptr<PreviewState> m_state = std::make_shared<PreviewState>();
	obs_display_t *m_display = nullptr;
	QSize m_canvas{1920, 1080};
	QList<Frame> m_frames;
	QString m_selected;
	bool m_dragging = false;
	QPointF m_pressCanvas;
};

// ---------------------------------------------------------------- page

LayoutPage::LayoutPage(const QString &overlayId, const oc::OverlayInfo &info, QWidget *parent)
	: QWidget(parent), m_overlay(overlayId), m_info(info)
{
	auto *outer = new QHBoxLayout(this);
	outer->setContentsMargins(su::S(16), su::S(16), su::S(16), su::S(16));
	outer->setSpacing(su::S(14));

	// Left: scene picker and the parts in it.
	auto *left = new QWidget();
	left->setFixedWidth(su::S(230));
	auto *lv = new QVBoxLayout(left);
	lv->setContentsMargins(0, 0, 0, 0);
	lv->setSpacing(su::S(8));
	lv->addWidget(DimLabel(T("OverlayControls.Layout.Scene")));
	auto *scenes = new su::MacComboBox();
	scenes->setStyleSheet(su::comboStyle(false));
	su::makeComboAnimated(scenes);
	scenes->setAccessibleName(T("OverlayControls.Layout.Scene"));
	m_scenes = scenes;
	lv->addWidget(m_scenes);
	lv->addSpacing(su::S(6));
	lv->addWidget(DimLabel(T("OverlayControls.Layout.Parts")));

	auto *partsCard = new QFrame();
	partsCard->setObjectName("ocPartsCard");
	partsCard->setStyleSheet(su::scale_qss(QString("QFrame#ocPartsCard{background:%1;border:1px solid rgba(255,255,255,0.06);border-radius:12px;}")
						       .arg(su::Colors::BG_PRIMARY)));
	auto *pc = new QVBoxLayout(partsCard);
	pc->setContentsMargins(su::S(4), su::S(4), su::S(4), su::S(4));
	m_parts = new QListWidget();
	m_parts->setStyleSheet(NavListStyle());
	m_parts->setAccessibleName(T("OverlayControls.Layout.Parts"));
	su::useScrollBars(m_parts);
	pc->addWidget(m_parts);
	lv->addWidget(partsCard, 1);
	auto *hint = DimLabel(T("OverlayControls.Layout.Hint"));
	hint->setWordWrap(true);
	lv->addWidget(hint);
	outer->addWidget(left);

	// Right: preview above the selected part's fields.
	auto *right = new QVBoxLayout();
	right->setSpacing(su::S(12));
	m_preview = new ScenePreview();
	right->addWidget(m_preview, 1);

	m_editor = new QFrame();
	m_editor->setObjectName("ocPartEditor");
	m_editor->setStyleSheet(su::scale_qss(QString("QFrame#ocPartEditor{background:%1;border-radius:12px;}")
						      .arg(su::Colors::BG_PRIMARY)));
	auto *eg = new QGridLayout(m_editor);
	eg->setContentsMargins(su::S(16), su::S(12), su::S(16), su::S(12));
	eg->setHorizontalSpacing(su::S(10));
	eg->setVerticalSpacing(su::S(8));
	m_partTitle = su::sectionHeader(QString());
	m_partTitle->setContentsMargins(0, 0, 0, 0);
	eg->addWidget(m_partTitle, 0, 0, 1, 7);

	m_x = new PanelDoubleSpin();
	m_y = new PanelDoubleSpin();
	for (PanelDoubleSpin *s : {m_x, m_y}) {
		// Typing "1250" is one move, not four.
		s->setKeyboardTracking(false);
		s->setRange(-4000, 4000);
		s->setDecimals(0);
		s->setSuffix(" px");
		s->setMinimumWidth(su::S(96));
	}
	m_x->setAccessibleName(T("OverlayControls.Layout.X"));
	m_y->setAccessibleName(T("OverlayControls.Layout.Y"));
	eg->addWidget(RowLabel(T("OverlayControls.Layout.X")), 1, 0);
	eg->addWidget(m_x, 1, 1);
	eg->addWidget(RowLabel(T("OverlayControls.Layout.Y")), 1, 2);
	eg->addWidget(m_y, 1, 3);

	m_visible = new su::IOSCheckBox(T("OverlayControls.Layout.Visible"));
	m_visible->setStyleSheet(QString("QCheckBox{color:%1;}").arg(su::Colors::TEXT_PRIMARY));
	m_visible->setAccessibleName(T("OverlayControls.Layout.Visible"));
	eg->addWidget(m_visible, 1, 4);

	m_reset = new su::PillButton(T("OverlayControls.Layout.ResetPart"), "outline");
	m_reset->setAccessibleName(T("OverlayControls.Layout.ResetPart"));
	eg->addWidget(m_reset, 1, 6, Qt::AlignRight);

	eg->addWidget(RowLabel(T("OverlayControls.Layout.Scale")), 2, 0);
	m_scale = new PanelSlider();
	m_scale->setAccessibleName(T("OverlayControls.Layout.Scale"));
	eg->addWidget(m_scale, 2, 1, 1, 4);
	m_scaleValue = su::makeLabel(QString(), su::Sizes::FONT_SIZE_SMALL, 600, su::Colors::TEXT_SECONDARY);
	m_scaleValue->setWordWrap(false);
	eg->addWidget(m_scaleValue, 2, 5);
	eg->setColumnStretch(5, 1);
	right->addWidget(m_editor, 0);
	outer->addLayout(right, 1);

	// The preview draws itself every frame; this only keeps the part
	// outlines in step with the live scene (a part moved elsewhere shows
	// here), which is a few cheap lookups.
	auto *timer = new QTimer(this);
	timer->setObjectName("ocPreviewTimer");
	timer->setInterval(100);
	QObject::connect(timer, &QTimer::timeout, this, [this]() {
		// Frames follow the live scene: a part moved elsewhere shows here.
		QList<ScenePreview::Frame> frames;
		for (const oc::ControlInfo *c : partsInScene(m_scene)) {
			const QRectF r = oc::LayoutFrame(m_overlay, c->id, m_scene);
			if (!r.isEmpty())
				frames.append(ScenePreview::Frame{c->id, c->label, r, c->layout.movable});
		}
		m_preview->setFrames(frames, m_selected);
	});

	m_flushTimer = new QTimer(this);
	m_flushTimer->setSingleShot(true);
	m_flushTimer->setInterval(33);
	QObject::connect(m_flushTimer, &QTimer::timeout, this, [this]() {
		if (flushWrite())
			m_flushTimer->start();
	});

	QObject::connect(m_scenes, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
		m_scene = m_scenes->currentData().toString();
		showScene(isVisible());
		fillParts();
	});
	QObject::connect(m_parts, &QListWidget::currentRowChanged, this, [this](int row) {
		if (m_updating || row < 0)
			return;
		selectPart(m_parts->item(row)->data(Qt::UserRole).toString());
	});
	// Screen readers select without moving the current row.
	QObject::connect(m_parts, &QListWidget::itemSelectionChanged, this, [this]() {
		const QList<QListWidgetItem *> sel = m_parts->selectedItems();
		if (m_updating || sel.isEmpty())
			return;
		const QString id = sel.first()->data(Qt::UserRole).toString();
		if (id != m_selected)
			selectPart(id);
	});

	m_preview->onSelect = [this](const QString &id) { selectPart(id); };
	// Drags are relative to where the part was when the press started.
	auto dragStart = std::make_shared<QPointF>();
	auto dragging = std::make_shared<bool>(false);
	m_preview->onDrag = [this, dragStart, dragging](double dx, double dy, bool finished) {
		const oc::ControlInfo *c = part(m_selected);
		if (!c)
			return;
		if (!*dragging) {
			const QJsonObject v = c->value.toObject();
			*dragStart = QPointF(v.value("x").toDouble(), v.value("y").toDouble());
			*dragging = true;
		}
		write(QJsonObject{{"x", dragStart->x() + dx}, {"y", dragStart->y() + dy}});
		if (finished) {
			*dragging = false;
			flushWrite(); // the drop lands where it was let go
		}
	};
	m_preview->onNudge = [this](double dx, double dy) {
		const oc::ControlInfo *c = part(m_selected);
		if (!c || !c->layout.movable)
			return;
		const QJsonObject v = c->value.toObject();
		write(QJsonObject{{"x", v.value("x").toDouble() + dx}, {"y", v.value("y").toDouble() + dy}});
	};

	QObject::connect(m_x, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
		if (!m_updating)
			write(QJsonObject{{"x", v}});
	});
	QObject::connect(m_y, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
		if (!m_updating)
			write(QJsonObject{{"y", v}});
	});
	QObject::connect(m_scale, &QSlider::valueChanged, this, [this](int pos) {
		if (m_updating)
			return;
		const double s = pos / 100.0;
		m_scaleValue->setText(QString::number(s, 'f', 2) + QString::fromUtf8("×"));
		write(QJsonObject{{"scale", s}});
	});
	QObject::connect(m_visible, &QCheckBox::toggled, this, [this](bool on) {
		if (!m_updating)
			write(QJsonObject{{"visible", on}});
	});
	QObject::connect(m_reset, &QPushButton::clicked, this, [this]() {
		if (!m_selected.isEmpty())
			oc::ResetControl(m_overlay, m_selected);
	});

	fillScenes();
}

LayoutPage::~LayoutPage()
{
	flushWrite();
	showScene(false);
}

// Keeps the previewed scene showing while the page is up, so its sources
// render properly, without toggling it every frame.
void LayoutPage::showScene(bool show)
{
	if (m_shown) {
		obs_source_dec_showing(m_shown);
		obs_source_release(m_shown);
		m_shown = nullptr;
	}
	m_preview->setScene(nullptr, QSize(1920, 1080));
	if (m_scene.isEmpty())
		return;
	obs_source_t *scene = StreamUP::CanvasLookup::GetSourceByName(m_scene.toUtf8().constData());
	if (!scene)
		return;
	uint32_t cw = 0;
	uint32_t ch = 0;
	if (!StreamUP::CanvasLookup::GetCanvasSize(scene, cw, ch)) {
		cw = 1920;
		ch = 1080;
	}
	m_preview->setScene(scene, QSize(int(cw), int(ch)));
	if (show) {
		obs_source_inc_showing(scene);
		m_shown = scene;
	} else {
		obs_source_release(scene);
	}
}

const oc::ControlInfo *LayoutPage::part(const QString &id) const
{
	for (const oc::GroupInfo &g : m_info.groups)
		for (const oc::ControlInfo &c : g.controls)
			if (c.id == id && c.type == oc::ControlType::Layout)
				return &c;
	return nullptr;
}

QList<const oc::ControlInfo *> LayoutPage::partsInScene(const QString &scene) const
{
	QList<const oc::ControlInfo *> out;
	for (const oc::GroupInfo &g : m_info.groups)
		for (const oc::ControlInfo &c : g.controls)
			if (c.type == oc::ControlType::Layout && c.layout.scenes.contains(scene))
				out.append(&c);
	return out;
}

void LayoutPage::fillScenes()
{
	m_updating = true;
	m_scenes->clear();
	for (const QString &scene : m_info.scenes)
		if (!partsInScene(scene).isEmpty())
			m_scenes->addItem(scene, scene);
	m_updating = false;
	m_scene = m_scenes->currentData().toString();
	fillParts();
}

void LayoutPage::fillParts()
{
	m_updating = true;
	m_parts->clear();
	// Parents first, each followed by the parts that ride on it.
	const QList<const oc::ControlInfo *> parts = partsInScene(m_scene);
	std::function<void(const QString &, int)> add = [&](const QString &parent, int depth) {
		for (const oc::ControlInfo *c : parts) {
			const bool top = c->layout.parent.isEmpty() || !part(c->layout.parent) ||
					 !part(c->layout.parent)->layout.scenes.contains(m_scene);
			if ((parent.isEmpty() && top) || (!parent.isEmpty() && c->layout.parent == parent)) {
				auto *item = new QListWidgetItem(QString(depth * 4, QChar(' ')) + c->label);
				item->setData(Qt::UserRole, c->id);
				item->setToolTip(c->help);
				m_parts->addItem(item);
				add(c->id, depth + 1);
			}
		}
	};
	add(QString(), 0);
	m_updating = false;

	QString keep = m_selected;
	bool found = false;
	for (int i = 0; i < m_parts->count(); ++i)
		found = found || m_parts->item(i)->data(Qt::UserRole).toString() == keep;
	if (!found)
		keep = m_parts->count() ? m_parts->item(0)->data(Qt::UserRole).toString() : QString();
	selectPart(keep);
}

void LayoutPage::selectPart(const QString &id)
{
	// A write still waiting on the throttle belongs to the part it was made on.
	flushWrite();
	m_selected = id;
	m_updating = true;
	for (int i = 0; i < m_parts->count(); ++i)
		if (m_parts->item(i)->data(Qt::UserRole).toString() == id)
			m_parts->setCurrentRow(i);
	m_updating = false;
	showSelected();
}

void LayoutPage::showSelected()
{
	const oc::ControlInfo *c = part(m_selected);
	m_editor->setEnabled(c != nullptr);
	if (!c) {
		m_partTitle->setText(QString());
		return;
	}
	m_updating = true;
	m_partTitle->setText(c->label);
	const QJsonObject v = c->value.toObject();
	m_x->setValue(v.value("x").toDouble());
	m_y->setValue(v.value("y").toDouble());
	m_x->setEnabled(c->layout.movable);
	m_y->setEnabled(c->layout.movable);
	const bool scalable = c->layout.scaleMax > c->layout.scaleMin;
	m_scale->setEnabled(scalable);
	m_scale->setRange(int(std::lround(c->layout.scaleMin * 100)), int(std::lround(c->layout.scaleMax * 100)));
	m_scale->setValue(int(std::lround(v.value("scale").toDouble(1.0) * 100)));
	m_scaleValue->setText(QString::number(v.value("scale").toDouble(1.0), 'f', 2) + QString::fromUtf8("×"));
	m_scale->setToolTip(scalable ? QString() : T("OverlayControls.Layout.NoScale"));
	m_visible->setChecked(v.value("visible").toBool(true));
	m_reset->setEnabled(c->applied);
	m_updating = false;
}

// A drag reports every mouse move. Each write moves every item of the part,
// recomputes its ring cut-outs and tells the window, so they are throttled to
// ~30 a second: the first lands at once, later ones merge and land on the
// timer, the last always lands.
void LayoutPage::write(const QJsonObject &partial)
{
	for (auto it = partial.begin(); it != partial.end(); ++it)
		m_pending[it.key()] = it.value();
	if (!m_flushTimer->isActive()) {
		flushWrite();
		m_flushTimer->start();
	}
	// The window refreshes every page from the notifier; this page's own
	// fields follow on that refresh.
}

bool LayoutPage::flushWrite()
{
	if (m_pending.isEmpty() || m_selected.isEmpty())
		return false;
	const QJsonObject partial = std::exchange(m_pending, QJsonObject());
	QString error;
	if (!oc::SetControlValue(m_overlay, m_selected, partial, &error) && onStatus)
		onStatus(error);
	return true;
}

void LayoutPage::refresh(const oc::OverlayInfo &info)
{
	m_info = info;
	showSelected();
}

void LayoutPage::showEvent(QShowEvent *e)
{
	QWidget::showEvent(e);
	showScene(true);
	if (auto *t = findChild<QTimer *>("ocPreviewTimer"))
		t->start();
}

void LayoutPage::hideEvent(QHideEvent *e)
{
	if (auto *t = findChild<QTimer *>("ocPreviewTimer"))
		t->stop();
	flushWrite();
	showScene(false);
	QWidget::hideEvent(e);
}

} // namespace OverlayUI
} // namespace StreamUP
