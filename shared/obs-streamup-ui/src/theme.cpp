#include "streamup/ui/theme.hpp"

#include <streamup/ui/gallery-style.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>
#include <util/config-file.h>

#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>

namespace StreamUP {
namespace Kit {

namespace su = StreamUP::UIStyles;

namespace Theme {
namespace {

using Vars = QHash<QString, QString>;

struct ThemeFile {
	QString id;
	QString extends;
	Vars vars;
};

// The StreamUP base theme plus The Andi variant (StreamUP_TheAndi.ovt), for
// when the active theme is not a StreamUP one (or its files cannot be read).
Vars BuiltIn()
{
	return {
		{"bg_darkest", "#0E0E0E"},
		{"bg_primary", "#1A1A1A"},
		{"bg_secondary", "#222222"},
		{"bg_tertiary", "#2E2E2E"},
		{"primary_color", "#D4A843"},
		{"primary_hover", "#E0B85A"},
		{"primary_light", "#EDD07E"},
		{"secondary_color", "#EDD07E"},
		{"text_primary", "#ffffff"},
		{"text_secondary", "rgba(255, 255, 255, 0.65)"},
		{"text_disabled", "rgba(255, 255, 255, 0.35)"},
		{"text_on_primary", "#1a1a1a"},
		{"color_warning", "#ff9f0a"},
		{"color_danger", "#ff453a"},
		{"border_subtle", "rgba(255, 255, 255, 0.07)"},
		{"border_soft", "rgba(255, 255, 255, 0.10)"},
		{"surface_subtle", "rgba(212, 168, 67, 0.08)"},
		{"menu_overlay", "rgba(16, 16, 16, 0.96)"},
		{"selection_bg", "rgba(212, 168, 67, 0.28)"},
	};
}

ThemeFile ReadThemeFile(const QString &path)
{
	ThemeFile t;
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
		return t;
	QString text = QString::fromUtf8(f.readAll());
	text.remove(QRegularExpression(QStringLiteral("/\\*.*?\\*/"), QRegularExpression::DotMatchesEverythingOption));
	static const QRegularExpression idRe(QStringLiteral("\\bid:\\s*'([^']+)'"));
	static const QRegularExpression extRe(QStringLiteral("\\bextends:\\s*'([^']+)'"));
	static const QRegularExpression varsRe(QStringLiteral("@OBSThemeVars\\s*\\{([^}]*)\\}"));
	static const QRegularExpression varRe(QStringLiteral("--([A-Za-z0-9_]+)\\s*:\\s*([^;]+);"));
	if (auto m = idRe.match(text); m.hasMatch())
		t.id = m.captured(1);
	if (auto m = extRe.match(text); m.hasMatch())
		t.extends = m.captured(1);
	auto blocks = varsRe.globalMatch(text);
	while (blocks.hasNext()) {
		auto vars = varRe.globalMatch(blocks.next().captured(1));
		while (vars.hasNext()) {
			auto v = vars.next();
			QString value = v.captured(2).trimmed();
			if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"'))
				value = value.mid(1, value.size() - 2);
			t.vars.insert(v.captured(1), value);
		}
	}
	return t;
}

QStringList ThemeDirs()
{
	QStringList dirs;
	const QString app = QCoreApplication::applicationDirPath();
	dirs << QDir(app).absoluteFilePath(QStringLiteral("../../data/obs-studio/themes"))
	     << QDir(app).absoluteFilePath(QStringLiteral("../Resources/data/obs-studio/themes"))
	     << QDir(app).absoluteFilePath(QStringLiteral("../share/obs/obs-studio/themes"))
	     << QStringLiteral("/usr/share/obs/obs-studio/themes");
	// User themes live beside plugin_config in OBS's own config folder.
	if (char *cfg = obs_module_config_path("")) {
		dirs << QDir(QString::fromUtf8(cfg)).absoluteFilePath(QStringLiteral("../../themes"));
		bfree(cfg);
	}
	return dirs;
}

QColor ParseColour(const QString &value)
{
	static const QRegularExpression rgbaRe(QStringLiteral(
		"rgba?\\(\\s*([\\d.]+)\\s*,\\s*([\\d.]+)\\s*,\\s*([\\d.]+)\\s*(?:,\\s*([\\d.]+)\\s*)?\\)"));
	if (auto m = rgbaRe.match(value); m.hasMatch()) {
		QColor c(int(m.captured(1).toDouble()), int(m.captured(2).toDouble()), int(m.captured(3).toDouble()));
		if (!m.captured(4).isEmpty())
			c.setAlphaF(float(m.captured(4).toDouble()));
		return c;
	}
	return QColor::fromString(value.trimmed());
}

QString Resolve(const Vars &vars, const QString &key, int depth = 0)
{
	const QString v = vars.value(key).trimmed();
	static const QRegularExpression ref(QStringLiteral("^var\\(--([A-Za-z0-9_]+)\\)$"));
	if (depth < 8)
		if (auto m = ref.match(v); m.hasMatch())
			return Resolve(vars, m.captured(1), depth + 1);
	return v;
}

// A translucent colour flattened onto a surface, as a solid #rrggbb.
QString Solid(const QColor &c, const QColor &over)
{
	const double a = c.alphaF();
	return QColor(int(std::lround(c.red() * a + over.red() * (1 - a))),
		      int(std::lround(c.green() * a + over.green() * (1 - a))),
		      int(std::lround(c.blue() * a + over.blue() * (1 - a))))
		.name();
}

QString Rgba(const QColor &c)
{
	return QStringLiteral("rgba(%1,%2,%3,%4)")
		.arg(c.red())
		.arg(c.green())
		.arg(c.blue())
		.arg(QString::number(c.alphaF(), 'f', 2));
}

QString g_source = QStringLiteral("built-in");
// The strings the shared tokens point at; kept alive for the session.
QHash<QString, QByteArray> g_store;

const char *Keep(const char *name, const QString &value)
{
	QByteArray &slot = g_store[QString::fromLatin1(name)];
	slot = value.toUtf8();
	return slot.constData();
}

} // namespace

QString TokenSource()
{
	return g_source;
}

void LoadTokens()
{
	Vars vars = BuiltIn();
	g_source = QStringLiteral("built-in");

	QString active;
	if (config_t *cfg = obs_frontend_get_user_config())
		active = QString::fromUtf8(config_get_string(cfg, "Appearance", "Theme"));

	if (active.startsWith(QStringLiteral("com.streamup."))) {
		QHash<QString, ThemeFile> themes;
		for (const QString &dir : ThemeDirs()) {
			const QDir d(dir);
			if (!d.exists())
				continue;
			for (const QString &name : d.entryList({QStringLiteral("*.obt"), QStringLiteral("*.ovt")}, QDir::Files)) {
				ThemeFile t = ReadThemeFile(d.absoluteFilePath(name));
				if (t.id.startsWith(QStringLiteral("com.streamup.")))
					themes.insert(t.id, t);
			}
		}
		// Walk from the variant up to the base, then apply the base first.
		QStringList chain;
		QString id = active;
		while (themes.contains(id) && chain.size() < 6 && !chain.contains(id)) {
			chain.prepend(id);
			id = themes.value(id).extends;
		}
		if (!chain.isEmpty()) {
			Vars merged;
			for (const QString &tid : chain) {
				const Vars &tv = themes[tid].vars;
				for (auto it = tv.cbegin(); it != tv.cend(); ++it)
					merged.insert(it.key(), it.value());
			}
			// Only take a variant whose surfaces and accent resolve.
			if (ParseColour(Resolve(merged, QStringLiteral("bg_primary"))).isValid() &&
			    ParseColour(Resolve(merged, QStringLiteral("primary_color"))).isValid()) {
				for (auto it = merged.cbegin(); it != merged.cend(); ++it)
					vars.insert(it.key(), it.value());
				g_source = active;
			}
		}
	}

	auto colour = [&vars](const char *key) {
		return ParseColour(Resolve(vars, QString::fromLatin1(key)));
	};
	const QColor darkest = colour("bg_darkest");
	const QColor primaryBg = colour("bg_primary");
	const QColor tertiary = colour("bg_tertiary");
	const QColor accent = colour("primary_color");
	QColor text = colour("text_primary");
	if (!text.isValid())
		text = QColor(Qt::white);

	namespace C = su::Colors;
	C::BG_DARKEST = Keep("BG_DARKEST", darkest.name());
	C::BG_PRIMARY = Keep("BG_PRIMARY", primaryBg.name());
	C::BG_SECONDARY = Keep("BG_SECONDARY", darkest.name());
	C::BG_TERTIARY = Keep("BG_TERTIARY", tertiary.isValid() ? tertiary.name() : primaryBg.lighter(130).name());
	C::SIDEBAR_BG = Keep("SIDEBAR_BG", darkest.name());
	C::INPUT_ON_BASE = Keep("INPUT_ON_BASE", primaryBg.name());
	C::INPUT_ON_CARD = Keep("INPUT_ON_CARD", darkest.name());

	C::PRIMARY_COLOR = Keep("PRIMARY_COLOR", accent.name());
	const QColor hover = colour("primary_hover");
	C::PRIMARY_HOVER = Keep("PRIMARY_HOVER", (hover.isValid() ? hover : accent.lighter(112)).name());
	C::PRIMARY_PRESSED = Keep("PRIMARY_PRESSED", accent.darker(115).name());
	const QColor light = colour("primary_light");
	C::PRIMARY_LIGHT = Keep("PRIMARY_LIGHT", (light.isValid() ? light : accent.lighter(130)).name());
	C::TAG_COLOR = C::PRIMARY_LIGHT;
	const QColor onAccent = colour("text_on_primary");
	C::TEXT_ON_PRIMARY = Keep("TEXT_ON_PRIMARY", (onAccent.isValid() ? onAccent : QColor(Qt::white)).name());

	if (const QColor w = colour("color_warning"); w.isValid())
		C::COLOR_WARNING = Keep("COLOR_WARNING", w.name());
	if (const QColor d = colour("color_danger"); d.isValid())
		C::COLOR_DANGER = Keep("COLOR_DANGER", d.name());

	C::TEXT_PRIMARY = Keep("TEXT_PRIMARY", text.name());
	QColor secondary = colour("text_secondary");
	if (!secondary.isValid()) {
		secondary = text;
		secondary.setAlphaF(0.65f);
	}
	C::TEXT_SECONDARY = Keep("TEXT_SECONDARY", Solid(secondary, primaryBg));
	QColor muted = text;
	muted.setAlphaF(0.45f);
	C::TEXT_MUTED = Keep("TEXT_MUTED", Solid(muted, primaryBg));
	QColor placeholder = colour("text_disabled");
	if (!placeholder.isValid()) {
		placeholder = text;
		placeholder.setAlphaF(0.35f);
	}
	C::TEXT_PLACEHOLDER = Keep("TEXT_PLACEHOLDER", Rgba(placeholder));

	QColor border = colour("border_subtle");
	if (!border.isValid())
		border = QColor(255, 255, 255, 20);
	C::BORDER_SUBTLE = Keep("BORDER_SUBTLE", Rgba(border));
	C::SEPARATOR = Keep("SEPARATOR", Solid(border, primaryBg));
	const QColor soft = colour("border_soft");
	C::POPUP_BORDER = Keep("POPUP_BORDER", Rgba(soft.isValid() ? soft : border));
	const QColor menu = colour("menu_overlay");
	C::POPUP_BG = Keep("POPUP_BG", menu.isValid() ? Solid(menu, darkest) : primaryBg.name());
	const QColor surface = colour("surface_subtle");
	C::HOVER_ROW = Keep("HOVER_ROW", Rgba(surface.isValid() ? surface : QColor(255, 255, 255, 13)));
	const QColor selection = colour("selection_bg");
	C::SELECTION = Keep("SELECTION", Rgba(selection.isValid() ? selection : accent));

	blog(LOG_INFO, "[StreamUP] UI colours from %s (accent %s, surface %s)", g_source.toUtf8().constData(),
	     C::PRIMARY_COLOR, C::BG_PRIMARY);
}

} // namespace Theme

} // namespace Kit
} // namespace StreamUP
