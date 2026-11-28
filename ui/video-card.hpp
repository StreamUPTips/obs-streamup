#pragma once

#include "../core/videos.hpp"

#include <QPushButton>

namespace StreamUP {

// A YouTube video as a card: its thumbnail with a play badge and its length,
// and the words beside it. A click opens it on YouTube. It is a button, so a
// Lightbox closes when it is clicked like any other button.
class VideoCard : public QPushButton {
public:
	enum class Style {
		Compact, // a small card, for Show me
		Hero,    // full width, for the top of What's new
	};

	VideoCard(const Videos::Video &video, Style style, QWidget *parent = nullptr);

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;
	int heightForWidth(int w) const override;

protected:
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	QRect thumbRect() const;
	void open();
	void pinHeight(int h);

	Videos::Video m_video;
	Style m_style;
};

// A small "Watch the video · 2:00" link with a red play mark, for a card's foot.
class WatchLink : public QPushButton {
public:
	WatchLink(const Videos::Video &video, QWidget *parent = nullptr);
	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *) override;

private:
	Videos::Video m_video;
};

namespace VideoPaint {
// The YouTube play badge, centred on c, w wide.
void PlayBadge(class QPainter &p, const QPointF &c, qreal w);
} // namespace VideoPaint

} // namespace StreamUP
