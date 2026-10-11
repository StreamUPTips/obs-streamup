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

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonObject>
#include <QKeyEvent>
#include <QListWidget>
#include <QApplication>
#include <QMouseEvent>
#include <QPointer>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <cstring>
#include <memory>
#include <utility>

namespace StreamUP {
namespace OverlayUI {

namespace oc = StreamUP::OverlayControls;

namespace {

} // namespace

// Render target and two read-back surfaces, kept for as long as the page lives.
//
// The old preview made a fresh texture and staging surface every frame, then
// mapped the staging surface straight after staging it. That map waits for the
// GPU to finish the copy while holding OBS's graphics lock, so every preview
// frame stalled OBS's own render thread: the lag on the Layout page. Now each
// frame stages into one surface and reads back the OTHER one, staged a whole
// frame earlier and long since finished, so the map never waits.
struct PreviewGpu {
	gs_texrender_t *tex = nullptr;
	gs_stagesurf_t *stage[2] = {nullptr, nullptr};
	bool staged[2] = {false, false};
	int next = 0;
	uint32_t w = 0;
	uint32_t h = 0;

	// Graphics context must be held.
	void destroy()
	{
		gs_texrender_destroy(tex);
		tex = nullptr;
		for (int i = 0; i < 2; ++i) {
			gs_stagesurface_destroy(stage[i]);
			stage[i] = nullptr;
			staged[i] = false;
		}
		w = h = 0;
	}
};

namespace {

// Renders a scene into an image the same way obs-websocket's screenshot does,
// on the scene's own canvas size (a vertical scene is 1080x1920, not the main
// canvas). Returns the frame staged one call earlier; the first call returns
// nothing.
QImage RenderScene(const QString &sceneUuid, int width, PreviewGpu &gpu)
{
	obs_source_t *scene = obs_get_source_by_uuid(sceneUuid.toUtf8().constData());
	if (!scene)
		return QImage();
	uint32_t cw = 0;
	uint32_t ch = 0;
	if (!StreamUP::CanvasLookup::GetCanvasSize(scene, cw, ch)) {
		obs_source_release(scene);
		return QImage();
	}
	const uint32_t w = uint32_t(width);
	const uint32_t h = uint32_t(std::lround(double(width) * ch / cw));

	QImage img;
	obs_enter_graphics();
	if (gpu.w != w || gpu.h != h) {
		gpu.destroy();
		gpu.tex = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
		gpu.stage[0] = gs_stagesurface_create(w, h, GS_RGBA);
		gpu.stage[1] = gs_stagesurface_create(w, h, GS_RGBA);
		gpu.w = w;
		gpu.h = h;
	}
	gs_texrender_reset(gpu.tex);
	if (gs_texrender_begin(gpu.tex, w, h)) {
		struct vec4 clear;
		vec4_zero(&clear);
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		gs_ortho(0.0f, float(cw), 0.0f, float(ch), -100.0f, 100.0f);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
		obs_source_video_render(scene);
		gs_blend_state_pop();
		gs_texrender_end(gpu.tex);

		const int cur = gpu.next;
		const int prev = cur ^ 1;
		gs_stage_texture(gpu.stage[cur], gs_texrender_get_texture(gpu.tex));
		gpu.staged[cur] = true;
		gpu.next = prev;

		uint8_t *data = nullptr;
		uint32_t linesize = 0;
		if (gpu.staged[prev] && gs_stagesurface_map(gpu.stage[prev], &data, &linesize)) {
			img = QImage(int(w), int(h), QImage::Format_RGBA8888);
			for (uint32_t y = 0; y < h; ++y)
				std::memcpy(img.scanLine(int(y)), data + y * linesize, size_t(img.bytesPerLine()));
			gs_stagesurface_unmap(gpu.stage[prev]);
		}
	}
	obs_leave_graphics();
	obs_source_release(scene);
	if (img.isNull())
		return img;
	// A composited scene's alpha is not coverage (a text source can leave a
	// transparent box over an opaque background), so the preview shows the
	// colour and drops the alpha, like the program output does.
	return img.convertToFormat(QImage::Format_RGB888);
}

} // namespace

// ---------------------------------------------------------------- preview

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
		setFocusPolicy(Qt::StrongFocus);
		setMouseTracking(true);
		setMinimumSize(su::S(420), su::S(236));
		setAccessibleName(T("OverlayControls.Layout.Preview"));
	}

	std::function<void(const QString &)> onSelect;
	std::function<void(double dx, double dy, bool finished)> onDrag; // canvas px from drag start
	std::function<void(double dx, double dy)> onNudge;

	void setImage(const QImage &img)
	{
		m_image = img;
		update();
	}
	// The canvas the previewed scene draws on (vertical scenes are tall).
	void setCanvas(const QSize &canvas)
	{
		m_canvas = canvas;
		update();
	}
	void setFrames(const QList<Frame> &frames, const QString &selected)
	{
		m_frames = frames;
		m_selected = selected;
		update();
	}

protected:
	QRectF target() const
	{
		const QSize canvas = m_canvas;
		const QRectF area = QRectF(rect()).adjusted(1, 1, -1, -1);
		const double k = std::min(area.width() / canvas.width(), area.height() / canvas.height());
		const QSizeF size(canvas.width() * k, canvas.height() * k);
		return QRectF(area.center().x() - size.width() / 2, area.center().y() - size.height() / 2, size.width(),
			      size.height());
	}
	double scale() const { return target().width() / m_canvas.width(); }
	QRectF toWidget(const QRectF &r) const
	{
		const QRectF t = target();
		const double k = scale();
		return QRectF(t.x() + r.x() * k, t.y() + r.y() * k, r.width() * k, r.height() * k);
	}
	QPointF toCanvas(const QPointF &p) const
	{
		const QRectF t = target();
		return (p - t.topLeft()) / scale();
	}

	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF t = target();
		// Checkerboard-free dark backing: transparency in a scene reads
		// as the canvas background, like the OBS preview.
		p.fillRect(rect(), QColor(su::Colors::BG_DARKEST));
		p.fillRect(t, QColor(0, 0, 0));
		if (!m_image.isNull())
			p.drawImage(t, m_image);

		QFont f = font();
		f.setPixelSize(su::S(11));
		f.setBold(true);
		p.setFont(f);
		for (const Frame &fr : m_frames) {
			const bool sel = fr.id == m_selected;
			const QRectF r = toWidget(fr.rect);
			QPen pen(sel ? QColor(su::Colors::PRIMARY_LIGHT) : QColor(255, 255, 255, 110));
			pen.setWidthF(sel ? 2.0 : 1.0);
			if (!sel)
				pen.setStyle(Qt::DashLine);
			p.setPen(pen);
			// Not a ternary: Qt::NoBrush would convert to QColor(Qt::color0),
			// which is opaque black, and fill every frame.
			if (sel)
				p.setBrush(QColor(41, 151, 255, 28));
			else
				p.setBrush(Qt::NoBrush);
			p.drawRect(r);
			if (sel) {
				// Corner handles, so the selection reads at a glance.
				const double hs = su::S(6);
				p.setBrush(QColor(su::Colors::PRIMARY_LIGHT));
				p.setPen(Qt::NoPen);
				for (const QPointF &c : {r.topLeft(), r.topRight(), r.bottomLeft(), r.bottomRight()})
					p.drawRect(QRectF(c.x() - hs / 2, c.y() - hs / 2, hs, hs));
			}
			// Name tag inside the frame's top-left corner.
			const QString text = fr.label;
			const QFontMetrics fm(f);
			const QRectF tag(r.x() + 1, r.y() + 1, fm.horizontalAdvance(text) + su::S(10), fm.height() + su::S(4));
			p.setPen(Qt::NoPen);
			p.setBrush(sel ? QColor(su::Colors::PRIMARY_COLOR) : QColor(0, 0, 0, 150));
			p.drawRoundedRect(tag, su::S(3), su::S(3));
			p.setPen(QColor(255, 255, 255));
			p.drawText(tag, Qt::AlignCenter, text);
		}
		if (hasFocus()) {
			QPen fp{QColor(su::Colors::PRIMARY_COLOR)};
			fp.setWidthF(1.5);
			p.setPen(fp);
			p.setBrush(Qt::NoBrush);
			p.drawRect(t.adjusted(0.5, 0.5, -0.5, -0.5));
		}
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
		}
		update();
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
	void focusInEvent(QFocusEvent *e) override
	{
		update();
		QWidget::focusInEvent(e);
	}
	void focusOutEvent(QFocusEvent *e) override
	{
		update();
		QWidget::focusOutEvent(e);
	}

private:
	QImage m_image;
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
	partsCard->setStyleSheet(su::scale_qss(QString("QFrame#ocPartsCard{background:%1;border-radius:14px;}")
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
	m_editor->setStyleSheet(su::scale_qss(QString("QFrame#ocPartEditor{background:%1;border-radius:14px;}")
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

	// Preview refresh: a few frames a second is plenty to see a part land,
	// and only while this page is on screen. Every frame renders the whole
	// scene, Aura glow shaders and all, so keep it modest.
	m_gpu = std::make_shared<PreviewGpu>();
	auto *timer = new QTimer(this);
	timer->setObjectName("ocPreviewTimer");
	timer->setInterval(250);
	QObject::connect(timer, &QTimer::timeout, this, [this]() {
		if (!m_scene.isEmpty() && isVisible())
			renderPreview();
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
	joinRender();
	flushWrite();
	showScene(false);
	if (m_gpu) {
		obs_enter_graphics();
		m_gpu->destroy();
		obs_leave_graphics();
	}
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
	m_sceneUuid.clear();
	if (m_scene.isEmpty())
		return;
	obs_source_t *scene = StreamUP::CanvasLookup::GetSourceByName(m_scene.toUtf8().constData());
	if (!scene)
		return;
	m_sceneUuid = QString::fromUtf8(obs_source_get_uuid(scene));
	uint32_t cw = 0;
	uint32_t ch = 0;
	if (StreamUP::CanvasLookup::GetCanvasSize(scene, cw, ch))
		m_preview->setCanvas(QSize(int(cw), int(ch)));
	if (show) {
		obs_source_inc_showing(scene);
		m_shown = scene;
	} else {
		obs_source_release(scene);
	}
}

void LayoutPage::joinRender()
{
	if (m_render.joinable())
		m_render.join();
}

void LayoutPage::renderPreview()
{
	// One frame in flight at a time; a slow frame just skips a tick.
	if (m_rendering->load())
		return;
	joinRender();
	if (m_sceneUuid.isEmpty())
		return;
	m_rendering->store(true);
	const QString scene = m_sceneUuid;
	// The preview is a small picture; rendering it bigger than it shows only
	// costs GPU time.
	const int width = std::clamp(m_preview->width(), 320, 640);
	QPointer<ScenePreview> preview(m_preview);
	auto busy = m_rendering;
	auto gpu = m_gpu;
	m_render = std::thread([scene, width, preview, busy, gpu]() {
		const QImage img = RenderScene(scene, width, *gpu);
		if (!img.isNull())
			QMetaObject::invokeMethod(
				qApp,
				[preview, img]() {
					if (preview)
						preview->setImage(img);
				},
				Qt::QueuedConnection);
		busy->store(false);
	});
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
	m_partTitle->setText(c->label.toUpper());
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
	joinRender();
	flushWrite();
	showScene(false);
	QWidget::hideEvent(e);
}

} // namespace OverlayUI
} // namespace StreamUP
