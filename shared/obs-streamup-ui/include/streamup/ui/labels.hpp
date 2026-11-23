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
#include <QStringList>

namespace StreamUP {
namespace UIStyles {

// A plain branded label: colour + size + weight, transparent background, word
// wrap on. px/weight are design pixels — scaled through scale_qss().
inline QLabel *makeLabel(const QString &text, int px, int weight,
			 const char *color = Colors::TEXT_PRIMARY)
{
	auto *l = new QLabel(text);
	// Snapped onto the type scale, so no window invents its own size.
	l->setStyleSheet(scale_qss(
		QString("QLabel{color:%1;font-size:%2px;font-weight:%3;background:transparent;}")
			.arg(color)
			.arg(Type::size(px))
			.arg(Type::weight(weight))));
	l->setWordWrap(true);
	return l;
}

// "WHAT'S NEW IN 3.0" / "Docks & Layout" -> "What's new in 3.0" / "Docks & layout".
// Words that are names (StreamUP, OBS, a capital in the middle) keep their case.
inline QString sentenceCase(const QString &text)
{
	static const QStringList keep = {"StreamUP", "OBS", "Google", "Aitum", "Streamer.bot", "WebSocket", "Multi-Dock",
					 "Scene", "Organiser", "ProText", "I"};
	const QStringList words = text.split(QLatin1Char(' '));
	QStringList out;
	for (int i = 0; i < words.size(); ++i) {
		const QString &w = words[i];
		QString k;
		for (const QString &name : keep)
			if (w.compare(name, Qt::CaseInsensitive) == 0)
				k = name;
		if (!k.isEmpty() && ((k != "Scene" && k != "Organiser") || text.contains("Scene Organiser", Qt::CaseInsensitive))) {
			out << k;
			continue;
		}
		bool innerCap = false;
		for (int c = 1; c < w.size(); ++c)
			innerCap = innerCap || (w[c].isUpper() && w[c - 1].isLower());
		if (innerCap) {
			out << w;
			continue;
		}
		QString lw = w.toLower();
		if (i == 0 && !lw.isEmpty())
			lw[0] = lw[0].toUpper();
		out << lw;
	}
	return out.join(QLatin1Char(' '));
}

// Accent display-font section header (all-caps, Bebas/condensed). Use above a
// group of rows inside a custom window — the StreamUP house "section" title.
inline QLabel *sectionHeader(const QString &text)
{
	// A quiet label above a group: sentence case, small, semibold, muted. The
	// accent colour is kept for actions, so it is not spent on labels.
	auto *l = new QLabel(sentenceCase(text));
	l->setStyleSheet(scale_qss(QString("QLabel{color:%1;font-size:%2px;font-weight:%3;background:transparent;}")
					   .arg("#a6adc8")
					   .arg(Type::Small)
					   .arg(Type::Semibold)));
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
