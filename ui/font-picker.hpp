#pragma once

// The Overlay Controls font picker: every installed font and the Google Fonts
// catalogue in one searchable list, each name drawn in its own face. Picking a
// Google font downloads and installs it for the user first (in the
// background), so OBS's text sources can use it.

#include <QPushButton>
#include <QString>
#include <QStringList>

#include <functional>

namespace StreamUP {
namespace FontPicker {

struct Picked {
	QString family;
	bool installed = false; // a Google font was installed just now
	QString file;           // where it was installed
};

// Opens the picker over parent. sourceKinds are the text source types the
// control drives (see OverlayControls::BoundSourceKinds), so it can say
// whether a new font shows up straight away. onPicked runs on the UI thread.
void Open(QWidget *parent, const QString &current, const QStringList &suggested, const QStringList &sourceKinds,
	  std::function<void(const Picked &)> onPicked);

// The field a font control shows: the current family, drawn in its own face,
// with a chevron, styled like the other inputs. Click opens the picker.
class FontButton : public QPushButton {
public:
	FontButton();
	void setFamily(const QString &family);
	QString family() const { return m_family; }
	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *) override;

private:
	QString m_family;
};

} // namespace FontPicker
} // namespace StreamUP
