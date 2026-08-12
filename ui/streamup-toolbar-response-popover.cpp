#include "streamup-toolbar-response-popover.hpp"

#include <streamup/ui/window-chrome.hpp>

#include <QApplication>
#include <QClipboard>
#include <QGridLayout>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QScreen>
#include <QVBoxLayout>

#include <obs-module.h>

using namespace StreamUP::UIStyles;

namespace StreamUP {

namespace {

// A screenshot response is a base64 image megabytes long. The clipboard gets
// all of it, but the popover only ever shows a readable amount.
constexpr int kMaxValueChars = 96;
constexpr int kMaxRows = 24;

QString formatValue(const QJsonValue &value)
{
	switch (value.type()) {
	case QJsonValue::Bool:
		return value.toBool() ? QStringLiteral("Yes") : QStringLiteral("No");
	case QJsonValue::Double: {
		const double d = value.toDouble();
		// Whole numbers read better without a trailing ".00", and the
		// fractional ones (CPU %, frame times) are noise past two places.
		if (qFuzzyCompare(d, qRound(d)))
			return QString::number(qRound(d));
		return QString::number(d, 'f', 2);
	}
	case QJsonValue::Null:
		return QStringLiteral("—");
	case QJsonValue::String:
		return value.toString();
	case QJsonValue::Array:
		return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
	case QJsonValue::Object:
		return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
	default:
		return QString();
	}
}

QString elide(const QString &text)
{
	QString flat = text;
	flat.replace('\n', ' ');
	if (flat.size() <= kMaxValueChars)
		return flat;
	return flat.left(kMaxValueChars) + QStringLiteral("…");
}

// obs-websocket keys are camelCase ("activeFps", "availableDiskSpace"). Split
// them so the popover reads like a readout rather than like source code.
QString humaniseKey(const QString &key)
{
	QString out;
	out.reserve(key.size() + 8);
	for (int i = 0; i < key.size(); ++i) {
		const QChar c = key.at(i);
		if (i > 0 && c.isUpper() && !key.at(i - 1).isUpper())
			out.append(' ');
		out.append(i == 0 ? c.toUpper() : c);
	}
	return out;
}

QLabel *makeLabel(QWidget *parent, const QString &text, const QString &colour, bool bold = false)
{
	QLabel *label = new QLabel(text, parent);
	label->setStyleSheet(QString("color: %1; %2").arg(colour, bold ? "font-weight: bold;" : ""));
	label->setTextInteractionFlags(Qt::TextSelectableByMouse);
	return label;
}

} // namespace

void showWebSocketResponsePopover(QWidget *anchor, const QString &title, const QString &responseJson,
				  const QString &errorText)
{
	if (!anchor)
		return;

	// Everything goes on the clipboard verbatim, indented, so it can be pasted
	// straight into a script or a bug report.
	QString clipboardText = responseJson;
	const QJsonDocument doc = QJsonDocument::fromJson(responseJson.toUtf8());
	if (doc.isObject())
		clipboardText = QString::fromUtf8(QJsonDocument(doc.object()).toJson(QJsonDocument::Indented));
	if (!errorText.isEmpty() && clipboardText.trimmed().isEmpty())
		clipboardText = errorText;
	if (!clipboardText.isEmpty())
		QGuiApplication::clipboard()->setText(clipboardText);

	QWidget *popup = new QWidget(anchor, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
	popup->setAttribute(Qt::WA_DeleteOnClose);
	popup->setAttribute(Qt::WA_TranslucentBackground);

	QVBoxLayout *outer = new QVBoxLayout(popup);
	outer->setContentsMargins(0, 0, 0, 0);

	RoundedContainer *card = new RoundedContainer(S(8), popup);
	outer->addWidget(card);

	QVBoxLayout *layout = new QVBoxLayout(card);
	layout->setContentsMargins(S(14), S(12), S(14), S(12));
	layout->setSpacing(S(8));

	layout->addWidget(makeLabel(card, title, Colors::TEXT_PRIMARY, /*bold=*/true));

	if (!errorText.isEmpty()) {
		QLabel *error = makeLabel(card, errorText, Colors::COLOR_DANGER);
		error->setWordWrap(true);
		layout->addWidget(error);
	}

	const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject();
	if (!obj.isEmpty()) {
		QGridLayout *grid = new QGridLayout();
		grid->setContentsMargins(0, 0, 0, 0);
		grid->setHorizontalSpacing(S(16));
		grid->setVerticalSpacing(S(4));

		int row = 0;
		const QStringList keys = obj.keys();
		for (const QString &key : keys) {
			if (row >= kMaxRows)
				break;
			grid->addWidget(makeLabel(card, humaniseKey(key), Colors::TEXT_MUTED), row, 0,
					Qt::AlignTop | Qt::AlignLeft);
			grid->addWidget(makeLabel(card, elide(formatValue(obj.value(key))), Colors::TEXT_PRIMARY),
					row, 1, Qt::AlignTop | Qt::AlignLeft);
			++row;
		}
		grid->setColumnStretch(1, 1);
		layout->addLayout(grid);

		if (keys.size() > row) {
			layout->addWidget(makeLabel(card,
						    QString(obs_module_text("StreamUP.Toolbar.WebSocket.Response.More"))
							    .arg(keys.size() - row),
						    Colors::TEXT_MUTED));
		}
	} else if (errorText.isEmpty()) {
		// A request that succeeded but returns nothing — most of the Set* and
		// Toggle* family. Saying so beats an empty box.
		layout->addWidget(makeLabel(card, obs_module_text("StreamUP.Toolbar.WebSocket.Response.NoData"),
					    Colors::TEXT_MUTED));
	}

	if (!clipboardText.isEmpty()) {
		layout->addWidget(makeLabel(card, obs_module_text("StreamUP.Toolbar.WebSocket.Response.Copied"),
					    Colors::TEXT_MUTED));
	}

	popup->adjustSize();

	// Sit under the button where there is room, above it where there is not,
	// then pull back inside the screen so a bar docked at an edge still works.
	const QRect anchorRect(anchor->mapToGlobal(QPoint(0, 0)), anchor->size());
	const QSize size = popup->sizeHint();
	QScreen *screen = anchor->screen() ? anchor->screen() : QGuiApplication::primaryScreen();
	const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

	int x = anchorRect.left();
	int y = anchorRect.bottom() + S(6);
	if (y + size.height() > available.bottom())
		y = anchorRect.top() - size.height() - S(6);

	x = qBound(available.left() + S(4), x, available.right() - size.width() - S(4));
	y = qBound(available.top() + S(4), y, available.bottom() - size.height() - S(4));

	popup->move(x, y);
	popup->show();
}

} // namespace StreamUP
