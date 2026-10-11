#pragma once

// The Socials list in the Overlay Controls window: which platforms the overlay
// shows, the handle for each, and whether it rotates on the nameplate and/or
// feeds the chat.

#include "../core/overlay-controls.hpp"

#include <QJsonArray>
#include <QList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>

namespace StreamUP {
namespace OverlayUI {

class PlatformsEditor : public QWidget {
public:
	explicit PlatformsEditor(const OverlayControls::ControlInfo &info, QWidget *parent = nullptr);

	// Replaces the list without calling onChanged.
	void setValue(const QJsonArray &rows);
	QJsonArray value() const { return m_rows; }

	std::function<void(const QJsonArray &)> onChanged;

private:
	void rebuild();
	void commit(bool typing = false);
	const OverlayControls::PlatformDef *def(const QString &id) const;

	QList<OverlayControls::PlatformDef> m_defs;
	QJsonArray m_rows;
	QVBoxLayout *m_list = nullptr;
	QTimer *m_typing = nullptr;
};

} // namespace OverlayUI
} // namespace StreamUP
