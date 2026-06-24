// Shared StreamUP in-app feedback dialog. Generalized from the Chat plugin's
// feedback dialog so every plugin's window-chrome feedback button posts to the
// StreamUP feedback API instead of opening the website. Product name and
// version are supplied by the caller (the window chrome passes the window's
// brandName + version), so one implementation serves every plugin.

#include <streamup/ui/feedback.hpp>

#include <streamup/ui/window-chrome.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/mac-inputs.hpp>
#include <streamup/ui/glass.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/ui-scrollbar.hpp>

#include <QApplication>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QVBoxLayout>

#include <thread>

using namespace StreamUP::UIStyles;

namespace StreamUP {
namespace UIStyles {

// ---------------------------------------------------------------------------
// Singleton guard (one feedback window across all plugin windows)
// ---------------------------------------------------------------------------
static ShadowDialog *g_feedback_dialog = nullptr;

// ---------------------------------------------------------------------------
// curl POST helper (runs curl process, returns HTTP status code)
// ---------------------------------------------------------------------------
static int curl_post_json(const QString &url, const QByteArray &json, QByteArray &response_out)
{
	QProcess proc;
	QStringList args;
	args << "-s" << "-L" << "--max-time" << "15"
	     << "-X" << "POST"
	     << "-H" << "Content-Type: application/json"
	     << "-w" << "\n%{http_code}"
	     << "-d" << QString::fromUtf8(json)
	     << url;

	proc.start("curl", args);
	if (!proc.waitForStarted(5000))
		return -1;
	if (!proc.waitForFinished(20000))
		return -1;
	if (proc.exitCode() != 0)
		return -1;

	QByteArray raw = proc.readAllStandardOutput();
	int last_nl = raw.lastIndexOf('\n');
	if (last_nl < 0)
		return -1;

	bool ok = false;
	int status = raw.mid(last_nl + 1).trimmed().toInt(&ok);
	response_out = raw.left(last_nl);
	return ok ? status : -1;
}

// ---------------------------------------------------------------------------
// Build JSON payload (minimal manual construction, no external JSON lib)
// ---------------------------------------------------------------------------
static QByteArray escape_json_string(const QString &s)
{
	QByteArray out;
	out.reserve(s.size() + 16);
	for (QChar c : s) {
		if (c == '\\')
			out.append("\\\\");
		else if (c == '"')
			out.append("\\\"");
		else if (c == '\n')
			out.append("\\n");
		else if (c == '\r')
			out.append("\\r");
		else if (c == '\t')
			out.append("\\t");
		else
			out.append(c.unicode() < 0x20 ? ' ' : c.toLatin1());
	}
	return out;
}

// StreamUP product numbers (Notion Product Database). The shared feedback
// dialog is keyed by the window's brand name, so map brand name -> number.
// Returns an empty string for an unknown product (submission still attributed
// by productName).
static QString product_number_for(const QString &productName)
{
	static const QHash<QString, QString> kNumbers = {
		{QStringLiteral("StreamUP"), QStringLiteral("obs001")},
		{QStringLiteral("StreamUP Chapter Marker Manager"), QStringLiteral("obs002")},
		{QStringLiteral("StreamUP ProText"), QStringLiteral("obs003")},
		{QStringLiteral("StreamUP Source Explorer"), QStringLiteral("obs004")},
		{QStringLiteral("StreamUP Hotkey Display"), QStringLiteral("obs005")},
		{QStringLiteral("StreamUP Chat"), QStringLiteral("obs007")},
	};
	return kNumbers.value(productName);
}

static QByteArray build_feedback_json(const QString &name, const QString &description,
				      const QString &type, const QString &productName,
				      const QString &productVersion)
{
	QByteArray json;
	json.append("{");
	json.append("\"name\":\"");
	json.append(escape_json_string(name));
	json.append("\",\"description\":\"");
	json.append(escape_json_string(description));
	json.append("\",\"productName\":\"");
	json.append(escape_json_string(productName));
	json.append("\",\"productNumber\":\"");
	json.append(escape_json_string(product_number_for(productName)));
	json.append("\",\"productVersion\":\"");
	json.append(escape_json_string(productVersion));
	json.append("\",\"type\":\"");
	json.append(escape_json_string(type));
	json.append("\"}");
	return json;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void show_feedback_dialog(QWidget *parent, const QString &productName, const QString &productVersion)
{
	if (g_feedback_dialog) {
		g_feedback_dialog->raise();
		g_feedback_dialog->activateWindow();
		return;
	}

	const QString brandName = productName.isEmpty() ? QStringLiteral("StreamUP") : productName;
	// Display version keeps the caller's form (e.g. "v1.0.1"); the API wants the
	// bare number, so strip a single leading "v".
	QString apiVersion = productVersion;
	if (apiVersion.startsWith(QLatin1Char('v')) || apiVersion.startsWith(QLatin1Char('V')))
		apiVersion = apiVersion.mid(1);

	auto *dlg = new ShadowDialog(parent);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	// No feedback button on the feedback window itself (showFeedback=false).
	WindowShell shell = applyChrome(dlg, QStringLiteral("Feedback"), productVersion,
					/*brandFooter=*/true, brandName,
					/*onFeedback=*/{}, /*popup=*/false, /*showFeedback=*/false);

	const int sm = S(ShadowDialog::kShadowMargin);
	dlg->resize(S(520) + 2 * sm, S(540) + 2 * sm);
	dlg->setMinimumSize(S(400) + 2 * sm, S(470) + 2 * sm);

	QObject::connect(dlg, &QDialog::destroyed, []() { g_feedback_dialog = nullptr; });
	g_feedback_dialog = dlg;

	// === Body (form fields) ===
	shell.content->setContentsMargins(S(20), S(16), S(20), S(20));
	shell.content->setSpacing(S(14));

	auto *intro = new QLabel("Let us know what you think! Submit a feature request, bug report, or general feedback.");
	intro->setWordWrap(true);
	intro->setStyleSheet(scale_qss(QString("color: %1; font-size: 12px;").arg(Colors::TEXT_MUTED)));
	shell.content->addWidget(intro);

	auto *typeLabel = new QLabel("FEEDBACK TYPE");
	typeLabel->setStyleSheet(scale_qss(QString("color: %1; font-size: 10px; font-weight: bold;").arg(Colors::TEXT_MUTED)));
	shell.content->addWidget(typeLabel);

	auto *typeCombo = new MacComboBox();
	typeCombo->addItem("Feature Request");
	typeCombo->addItem("Bug Report");
	typeCombo->addItem("General Feedback");
	typeCombo->setCurrentIndex(-1);
	typeCombo->setPlaceholderText("Select feedback type...");
	typeCombo->setOnCard(false);
	typeCombo->setStyleSheet(comboStyle(false));
	typeCombo->setFixedHeight(S(28));
	makeComboAnimated(typeCombo);
	useScrollBars(typeCombo->view());
	shell.content->addWidget(typeCombo);

	auto *nameLabel = new QLabel("TITLE");
	nameLabel->setStyleSheet(scale_qss(QString("color: %1; font-size: 10px; font-weight: bold;").arg(Colors::TEXT_MUTED)));
	shell.content->addWidget(nameLabel);

	auto *nameEdit = new QLineEdit();
	nameEdit->setPlaceholderText("Brief summary of your feedback...");
	nameEdit->setStyleSheet(lineEditStyle(false));
	nameEdit->setFixedHeight(S(28));
	shell.content->addWidget(nameEdit);

	auto *descLabel = new QLabel("DESCRIPTION");
	descLabel->setStyleSheet(scale_qss(QString("color: %1; font-size: 10px; font-weight: bold;").arg(Colors::TEXT_MUTED)));
	shell.content->addWidget(descLabel);

	auto *descEdit = new QPlainTextEdit();
	descEdit->setPlaceholderText("Describe your feedback in detail...");
	descEdit->setStyleSheet(plainTextStyle(false));
	descEdit->setFixedHeight(S(120));
	shell.content->addWidget(descEdit);

	shell.content->addStretch();

	// === Footer: status line (extra row) + action buttons ===
	auto *statusLabel = new QLabel();
	statusLabel->setStyleSheet(scale_qss(QString("color: %1; font-size: 11px;").arg(Colors::TEXT_MUTED)));
	statusLabel->setVisible(false);
	shell.footer->addWidget(statusLabel);

	auto *cancelBtn = new PillButton("Cancel", "outline");
	auto *sendBtn = new PillButton("Send", "primary");
	shell.footerButtons->addWidget(cancelBtn);
	shell.footerButtons->addWidget(sendBtn);

	// Connections
	QObject::connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::close);

	QObject::connect(sendBtn, &QPushButton::clicked, [=]() {
		QString typeStr = typeCombo->currentText();

		if (typeCombo->currentIndex() < 0) {
			statusLabel->setStyleSheet(scale_qss(QString("color: %1; font-size: 11px;").arg(Colors::COLOR_DANGER)));
			statusLabel->setText("Please select a feedback type.");
			statusLabel->setVisible(true);
			return;
		}
		if (nameEdit->text().trimmed().isEmpty()) {
			statusLabel->setStyleSheet(scale_qss(QString("color: %1; font-size: 11px;").arg(Colors::COLOR_DANGER)));
			statusLabel->setText("Please enter a title.");
			statusLabel->setVisible(true);
			return;
		}
		if (descEdit->toPlainText().trimmed().isEmpty()) {
			statusLabel->setStyleSheet(scale_qss(QString("color: %1; font-size: 11px;").arg(Colors::COLOR_DANGER)));
			statusLabel->setText("Please enter a description.");
			statusLabel->setVisible(true);
			return;
		}

		// Disable controls while submitting
		sendBtn->setEnabled(false);
		sendBtn->setText("Sending...");
		nameEdit->setEnabled(false);
		descEdit->setEnabled(false);
		typeCombo->setEnabled(false);
		statusLabel->setStyleSheet(scale_qss(QString("color: %1; font-size: 11px;").arg(Colors::TEXT_MUTED)));
		statusLabel->setText("Submitting feedback...");
		statusLabel->setVisible(true);

		QByteArray json = build_feedback_json(nameEdit->text().trimmed(),
						      descEdit->toPlainText().trimmed(), typeStr,
						      brandName, apiVersion);

		QPointer<QDialog> dlgPtr = dlg;
		QPointer<PillButton> sendPtr = sendBtn;
		QPointer<QLabel> statusPtr = statusLabel;
		QPointer<QLineEdit> namePtr = nameEdit;
		QPointer<QPlainTextEdit> descPtr = descEdit;
		QPointer<MacComboBox> typePtr = typeCombo;

		// Submit in a background thread; UI updates marshalled back.
		std::string json_std = json.toStdString();
		std::thread([json_std, dlgPtr, sendPtr, statusPtr, namePtr, descPtr, typePtr]() {
			QByteArray response;
			QByteArray payload = QByteArray::fromStdString(json_std);
			int status = curl_post_json("https://api.streamup.tips/feedback/submit", payload, response);
			bool success = (status == 200);

			QMetaObject::invokeMethod(
				qApp,
				[success, dlgPtr, sendPtr, statusPtr, namePtr, descPtr, typePtr]() {
					if (!statusPtr || !sendPtr)
						return;
					if (success) {
						statusPtr->setStyleSheet(scale_qss(QString("color: %1; font-size: 11px;").arg(Colors::COLOR_SUCCESS)));
						statusPtr->setText("Feedback submitted successfully! Thank you.");
						sendPtr->setText("Sent!");
						if (dlgPtr)
							QTimer::singleShot(1500, dlgPtr, &QDialog::close);
					} else {
						statusPtr->setStyleSheet(scale_qss(QString("color: %1; font-size: 11px;").arg(Colors::COLOR_DANGER)));
						statusPtr->setText("Failed to submit feedback. Please try again.");
						sendPtr->setEnabled(true);
						sendPtr->setText("Send");
						if (namePtr)
							namePtr->setEnabled(true);
						if (descPtr)
							descPtr->setEnabled(true);
						if (typePtr)
							typePtr->setEnabled(true);
					}
				},
				Qt::QueuedConnection);
		}).detach();
	});

	dlg->show();
}

} // namespace UIStyles
} // namespace StreamUP
