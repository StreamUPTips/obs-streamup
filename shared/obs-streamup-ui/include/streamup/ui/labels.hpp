#pragma once

// StreamUP label helpers — the branded text primitives every custom window uses.
//
// These were previously trapped as file-static helpers inside the gallery's
// obs-ui-gallery.cpp (and re-invented per-plugin as GetLabelStyle /
// GetDimLabelStyle / GetFormLabelStyle). They live here so the shared
// obs-streamup-ui library owns the one true version. Header-only.

#include "gallery-style.hpp"

#include <QLabel>
#include <QString>

namespace StreamUP {
namespace UIStyles {

// A plain branded label: colour + size + weight, transparent background, word
// wrap on. px/weight are design pixels — scaled through scale_qss().
inline QLabel *makeLabel(const QString &text, int px, int weight,
			 const char *color = Colors::TEXT_PRIMARY)
{
	auto *l = new QLabel(text);
	l->setStyleSheet(scale_qss(
		QString("QLabel{color:%1;font-size:%2px;font-weight:%3;background:transparent;}")
			.arg(color)
			.arg(px)
			.arg(weight)));
	l->setWordWrap(true);
	return l;
}

// Accent display-font section header (all-caps, Bebas/condensed). Use above a
// group of rows inside a custom window — the StreamUP house "section" title.
inline QLabel *sectionHeader(const QString &text)
{
	auto *l = new QLabel(text.toUpper());
	l->setFont(displayFont(16, 700));
	l->setStyleSheet(
		QString("QLabel{color:%1;background:transparent;}").arg(Colors::PRIMARY_COLOR));
	l->setContentsMargins(0, S(10), 0, S(2));
	l->setWordWrap(true);
	return l;
}

// ── Bare QSS strings (apply to an existing QLabel) ───────────────────────────
// Replacements for the per-plugin GetLabelStyle / GetDimLabelStyle /
// GetFormLabelStyle helpers. Prefer makeLabel() for new code; these exist so a
// label built elsewhere (e.g. a form row) can be styled in place.

// Body label — 13 / 500, primary text.
inline QString labelStyle(const char *color = Colors::TEXT_PRIMARY)
{
	return scale_qss(
		QString("color:%1;font-size:13px;font-weight:500;background:transparent;").arg(color));
}

// Dim caption — 12, muted.
inline QString dimLabelStyle()
{
	return scale_qss(
		QString("color:%1;font-size:12px;background:transparent;").arg(Colors::TEXT_MUTED));
}

// Form field label — 11 / bold, uppercase, muted. The little caption above an
// input inside a form.
inline QString formLabelStyle()
{
	return scale_qss(QString("color:%1;font-size:11px;font-weight:700;"
				 "text-transform:uppercase;background:transparent;")
				 .arg(Colors::TEXT_MUTED));
}

} // namespace UIStyles
} // namespace StreamUP
