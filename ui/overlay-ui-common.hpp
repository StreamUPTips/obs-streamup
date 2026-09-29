#pragma once

// Small widgets shared by the Overlay Controls window and its pages.

#include <streamup/ui/gallery-style.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/mac-inputs.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPainter>
#include <QPainterPath>
#include <QSlider>
#include <QToolButton>
#include <QWheelEvent>

#include <obs-module.h>

namespace StreamUP {
namespace OverlayUI {

namespace su = StreamUP::UIStyles;

inline QString T(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

// A slider inside a scrolling page must not grab the wheel as the pointer
// passes over it, or scrolling the page changes the overlay. It only takes the
// wheel once it has focus.
class PanelSlider : public QSlider {
public:
	PanelSlider() : QSlider(Qt::Horizontal)
	{
		setFocusPolicy(Qt::StrongFocus);
		setStyleSheet(su::sliderStyle());
	}

protected:
	void wheelEvent(QWheelEvent *e) override
	{
		if (!hasFocus()) {
			e->ignore();
			return;
		}
		QSlider::wheelEvent(e);
	}
};

// The decimal twin of the kit's MacSpinBox: same field, same painted accent
// stepper. Ignores the wheel unless focused, for the same reason as above.
class PanelDoubleSpin : public QDoubleSpinBox {
public:
	explicit PanelDoubleSpin(bool onCard = true)
	{
		setFocusPolicy(Qt::StrongFocus);
		setMinimumHeight(su::S(28));
		setKeyboardTracking(false);
		const char *bg = onCard ? su::Colors::INPUT_ON_CARD : su::Colors::INPUT_ON_BASE;
		setStyleSheet(su::scale_qss(
			QString("QDoubleSpinBox{background:%1;color:%2;border:none;"
				"border-radius:8px;min-height:28px;max-height:28px;padding:0 28px 0 10px;font-size:13px;}"
				"QDoubleSpinBox::up-button{subcontrol-origin:border;subcontrol-position:top right;"
				"width:22px;border:none;background:transparent;}"
				"QDoubleSpinBox::down-button{subcontrol-origin:border;subcontrol-position:bottom right;"
				"width:22px;border:none;background:transparent;}"
				"QDoubleSpinBox::up-arrow,QDoubleSpinBox::down-arrow{width:0;height:0;image:none;}")
				.arg(bg, su::Colors::TEXT_PRIMARY)));
	}
	QSize sizeHint() const override { return {QDoubleSpinBox::sizeHint().width(), su::S(28)}; }
	QSize minimumSizeHint() const override { return {QDoubleSpinBox::minimumSizeHint().width(), su::S(28)}; }

protected:
	void wheelEvent(QWheelEvent *e) override
	{
		if (!hasFocus()) {
			e->ignore();
			return;
		}
		QDoubleSpinBox::wheelEvent(e);
	}
	void paintEvent(QPaintEvent *e) override
	{
		QDoubleSpinBox::paintEvent(e);
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		const qreal rad = su::S(8);
		const qreal bw = su::S(22);
		QPainterPath fp;
		fp.addRoundedRect(r, rad, rad);
		const QRectF col(r.right() - bw, r.top(), bw + 1, r.height());
		p.save();
		p.setClipPath(fp);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(su::Colors::PRIMARY_COLOR));
		p.drawRect(col);
		QPen dv(QColor(255, 255, 255, 90));
		dv.setWidthF(std::max(1.6, su::S(2) * 0.9));
		p.setPen(dv);
		p.drawLine(QPointF(col.left(), r.center().y()), QPointF(col.right(), r.center().y()));
		p.restore();
		QPen cp{QColor(255, 255, 255)};
		cp.setWidthF(std::max(1.2, su::S(2) * 0.75));
		cp.setCapStyle(Qt::RoundCap);
		cp.setJoinStyle(Qt::RoundJoin);
		p.setPen(cp);
		const qreal cw = su::S(6);
		const qreal cx = col.center().x();
		su::detail::drawChevron(p, QPointF(cx, r.top() + r.height() * 0.25), cw, /*up=*/true);
		su::detail::drawChevron(p, QPointF(cx, r.top() + r.height() * 0.75), cw, /*up=*/false);
		if (hasFocus()) {
			QPen fpen{QColor(su::Colors::PRIMARY_COLOR)};
			fpen.setWidthF(2);
			p.setPen(fpen);
			p.setBrush(Qt::NoBrush);
			p.drawRoundedRect(r, rad, rad);
		}
	}
};

// An editable combo box. The kit's MacComboBox paints its own field text, and
// an editable combo's line edit draws the text again on top, so the two read
// as doubled. This one is styled entirely by QSS, to match the kit's inputs.
inline QComboBox *EditableCombo(bool onCard = true)
{
	auto *combo = new QComboBox();
	combo->setEditable(true);
	combo->setInsertPolicy(QComboBox::NoInsert);
	combo->setFixedHeight(su::S(28));
	const char *bg = onCard ? su::Colors::INPUT_ON_CARD : su::Colors::INPUT_ON_BASE;
	combo->setStyleSheet(
		su::scale_qss(QString("QComboBox{background:%1;color:%2;border:1px solid transparent;border-radius:8px;"
				      "padding:0 6px 0 10px;font-size:13px;}"
				      "QComboBox:focus{border:1px solid rgba(0,118,223,0.6);}"
				      "QComboBox::drop-down{border:none;width:24px;}"
				      "QComboBox QLineEdit{background:transparent;border:none;color:%2;padding:0;}")
					  .arg(bg, su::Colors::TEXT_PRIMARY)) +
		su::comboStyle(onCard));
	return combo;
}

// A small round icon-text button: row resets, list reorder and remove.
inline QToolButton *IconButton(const QString &glyph, const QString &tooltip, const QString &accessibleName,
			       const char *hover = su::Colors::TEXT_PRIMARY)
{
	auto *b = new QToolButton();
	b->setText(glyph);
	b->setToolTip(tooltip);
	b->setAccessibleName(accessibleName);
	b->setCursor(Qt::PointingHandCursor);
	b->setFixedSize(su::S(26), su::S(26));
	b->setStyleSheet(su::scale_qss(
		QString("QToolButton{color:%1;background:transparent;border:none;border-radius:13px;font-size:14px;}"
			"QToolButton:hover{color:%2;background:rgba(255,255,255,0.08);}"
			"QToolButton:disabled{color:transparent;}")
			.arg(su::Colors::TEXT_MUTED, hover)));
	return b;
}

inline QLabel *RowLabel(const QString &text, const QString &tooltip = QString())
{
	auto *l = su::makeLabel(text, su::Sizes::FONT_SIZE_SMALL, 600, su::Colors::TEXT_PRIMARY);
	l->setWordWrap(false);
	l->setToolTip(tooltip);
	return l;
}

inline QLabel *DimLabel(const QString &text)
{
	auto *l = su::makeLabel(text, su::Sizes::FONT_SIZE_TINY, 500, su::Colors::TEXT_MUTED);
	return l;
}

// The rounded list used as a navigation rail, matching StreamUP Settings.
inline QString NavListStyle()
{
	return su::scale_qss(QString("QListWidget{background-color:transparent;border:none;"
				     "font-family:Roboto,'Open Sans','.AppleSystemUIFont',Helvetica,Arial,sans-serif;"
				     "font-size:14px;outline:none;}"
				     "QListWidget::item{padding:11px 16px;border:none;color:%1;background:transparent;}"
				     "QListWidget::item:selected{background-color:%2;color:%3;border-radius:14px;}"
				     "QListWidget::item:hover:!selected{background-color:%4;color:%3;border-radius:14px;}")
				     .arg(su::Colors::TEXT_SECONDARY)
				     .arg(su::Colors::PRIMARY_COLOR)
				     .arg(su::Colors::TEXT_PRIMARY)
				     .arg(su::Colors::HOVER_ROW));
}

} // namespace OverlayUI
} // namespace StreamUP
