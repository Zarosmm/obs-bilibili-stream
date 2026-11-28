#include "ui/dialog_factory.hpp"
#include <obs-frontend-api.h>
#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QDeadlineTimer>
#include <QFormLayout>
#include <charconv>
#include "core/qr_generator.hpp"
#include "bilibili_api.hpp"
#include "http_client.hpp"
#include <memory>

namespace UI {

// The Bilibili API sometimes returns area ids as JSON strings and sometimes as
// numbers; accept both shapes and never throw, so a malformed payload cannot
// escape a Qt signal handler (an exception there terminates the process).
static bool parseAreaId(const json11::Json &value, int &id)
{
	const std::string text = value.is_string() ? value.string_value() : value.dump();
	if (text.empty())
		return false;

	int parsed = 0;
	auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
	if (result.ec != std::errc() || result.ptr != text.data() + text.size())
		return false;

	id = parsed;
	return true;
}

// Shared state of the asynchronous QR login poll: the timer callback only starts
// a new request when the previous one has finished, and stops for good once the
// dialog is closed.
struct QrPollState {
	std::string qrKey;
	QDeadlineTimer expires{180000};
	int consecutiveErrors = 0;
	bool inFlight = false;
	bool stopped = false;
};

struct QrLoginResult {
	Bili::QrLoginStatus status = Bili::QrLoginStatus::Error;
	std::string cookies;
	std::string message;
};

static QDialog *createBaseDialog(const QString &title, QWidget *parent)
{
	QDialog *dialog = new QDialog(parent);
	dialog->setWindowTitle(title);
	new QVBoxLayout(dialog);
	return dialog;
}

void DialogFactory::message(const QString &msg, const QString &title, QWidget *parent)
{
	QDialog *dialog = createBaseDialog(title, parent ? parent : (QWidget *)obs_frontend_get_main_window());
	QVBoxLayout *layout = (QVBoxLayout *)dialog->layout();
	auto *label = new QLabel(msg);
	label->setTextFormat(Qt::PlainText);
	label->setWordWrap(true);
	layout->addWidget(label);
	QPushButton *confirm = new QPushButton("确认");
	layout->addWidget(confirm);
	QObject::connect(confirm, &QPushButton::clicked, dialog, &QDialog::accept);
	QObject::connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
	dialog->exec();
}

QDialog *DialogFactory::qrLogin(QWidget *parent, const std::string &qrData, const std::string &qrKey,
				std::function<void(const std::string &cookies)> onSuccess)
{
	QDialog *dialog = createBaseDialog("Bilibili 登录二维码", parent);
	QVBoxLayout *layout = (QVBoxLayout *)dialog->layout();

	QLabel *qrLabel = new QLabel();
	QPixmap pixmap = Core::QrGenerator::generate(qrData);
	if (pixmap.isNull()) {
		dialog->deleteLater();
		message(QStringLiteral("无法生成二维码，请重新扫码"), QStringLiteral("扫码登录"), parent);
		return nullptr;
	}
	qrLabel->setPixmap(pixmap);
	qrLabel->setAlignment(Qt::AlignCenter);
	layout->addWidget(qrLabel);
	auto *status = new QLabel(QStringLiteral("请使用手机 Bilibili App 扫码"));
	status->setTextFormat(Qt::PlainText);
	status->setWordWrap(true);
	status->setAlignment(Qt::AlignCenter);
	layout->addWidget(status);

	auto *close = new QPushButton(QStringLiteral("关闭"));
	layout->addWidget(close);
	QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::reject);

	auto *timer = new QTimer(dialog);
	auto state = std::make_shared<QrPollState>();
	state->qrKey = qrKey;
	const auto stopPolling = [state, timer, qrLabel, status](const QString &text) {
		state->stopped = true;
		timer->stop();
		qrLabel->clear();
		status->setText(text);
	};
	const auto expired = QStringLiteral("二维码已过期，请关闭此窗口后重新扫码");

	QObject::connect(timer, &QTimer::timeout, dialog, [=]() {
		if (state->expires.hasExpired() && !state->inFlight) {
			stopPolling(expired);
			return;
		}
		if (state->stopped || state->inFlight)
			return;
		state->inFlight = true;

		auto result = std::make_shared<QrLoginResult>();
		Http::HttpClient::runAsync(
			dialog,
			[state, result]() {
				result->status = Bili::BiliApi::qrLogin(state->qrKey, result->cookies, result->message);
			},
			[=]() {
				state->inFlight = false;
				if (state->stopped)
					return;

				using Bili::QrLoginStatus;
				if (result->status != QrLoginStatus::Error)
					state->consecutiveErrors = 0;

				switch (result->status) {
				case QrLoginStatus::Waiting:
					status->setText(QStringLiteral("请使用手机 Bilibili App 扫码"));
					break;
				case QrLoginStatus::Scanned:
					status->setText(QStringLiteral("已扫码，请在手机上确认登录"));
					break;
				case QrLoginStatus::Expired:
					stopPolling(expired);
					break;
				case QrLoginStatus::Success:
					state->stopped = true;
					timer->stop();
					if (onSuccess)
						onSuccess(result->cookies);
					dialog->accept();
					break;
				case QrLoginStatus::Error:
					if (++state->consecutiveErrors >= 3) {
						const auto error = result->message.empty()
									   ? QStringLiteral("无法检查扫码状态")
									   : QString::fromUtf8(result->message);
						stopPolling(error + QStringLiteral("。请关闭此窗口后重试"));
					} else {
						status->setText(QStringLiteral("暂时无法检查扫码状态，正在重试…"));
					}
					break;
				}
			});
	});

	QObject::connect(dialog, &QDialog::finished, dialog, [state, timer, dialog]() {
		state->stopped = true;
		timer->stop();
		dialog->deleteLater();
	});

	timer->start(2000);
	dialog->exec();
	return dialog;
}

QDialog *DialogFactory::streamStarted(QWidget *parent, const std::string &rtmpAddr, const std::string &rtmpCode)
{
	auto *dialog = createBaseDialog(QStringLiteral("Bilibili 推流设置"), parent);
	auto *layout = static_cast<QVBoxLayout *>(dialog->layout());
	auto *instructions = new QLabel(QStringLiteral("在 OBS 的「设置 → 推流 → 自定义」中分别粘贴服务器和推流码。"));
	instructions->setWordWrap(true);
	layout->addWidget(instructions);

	auto *form = new QFormLayout();
	layout->addLayout(form);
	auto addCopyRow = [dialog, form](const QString &label, const QString &value, bool secret) {
		auto *row = new QHBoxLayout();
		auto *field = new QLineEdit(value);
		field->setReadOnly(true);
		if (secret)
			field->setEchoMode(QLineEdit::Password);
		auto *copy = new QPushButton(QStringLiteral("复制"));
		copy->setAccessibleName(QStringLiteral("复制") + label);
		copy->setAutoDefault(false);
		row->addWidget(field, 1);
		row->addWidget(copy);
		form->addRow(label, row);
		QObject::connect(copy, &QPushButton::clicked, dialog, [copy, field]() {
			QApplication::clipboard()->setText(field->text());
			copy->setText(QStringLiteral("已复制"));
			QTimer::singleShot(1500, copy, [copy]() { copy->setText(QStringLiteral("复制")); });
		});
		return field;
	};

	addCopyRow(QStringLiteral("服务器地址"), QString::fromStdString(rtmpAddr), false);
	auto *keyField = addCopyRow(QStringLiteral("推流码"), QString::fromStdString(rtmpCode), true);
	auto *showKey = new QCheckBox(QStringLiteral("显示推流码"));
	QObject::connect(showKey, &QCheckBox::toggled, keyField, [keyField](bool visible) {
		keyField->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
	});
	layout->addWidget(showKey);

	auto *close = new QPushButton(QStringLiteral("关闭"));
	layout->addWidget(close);
	close->setFocus();
	QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::accept);
	QObject::connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
	dialog->setMinimumWidth(440);
	dialog->exec();
	return dialog;
}

QDialog *DialogFactory::faceAuth(QWidget *parent, const std::string &faceUrl)
{
	QDialog *dialog = createBaseDialog("实名认证（人脸识别）", parent);
	QVBoxLayout *layout = (QVBoxLayout *)dialog->layout();

	QLabel *qrLabel = new QLabel();
	QPixmap pixmap = Core::QrGenerator::generate(faceUrl);
	if (pixmap.isNull()) {
		qrLabel->setText("二维码生成失败，请检查网络或日志");
	} else {
		qrLabel->setPixmap(pixmap);
	}
	qrLabel->setAlignment(Qt::AlignCenter);

	QLabel *tipLabel = new QLabel("请使用<b>手机 Bilibili App</b> 扫描下方二维码完成人脸认证。<br>"
				      "认证完成后，请重新点击开始直播。");
	tipLabel->setWordWrap(true);
	tipLabel->setAlignment(Qt::AlignCenter);

	QPushButton *closeBtn = new QPushButton("我已完成认证");

	layout->addWidget(tipLabel);
	layout->addWidget(qrLabel);
	layout->addWidget(closeBtn);

	QObject::connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
	QObject::connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);

	dialog->exec();
	return dialog;
}

QDialog *DialogFactory::roomSettings(QWidget *parent, const std::string &roomUrl, const std::string &currentTitle,
				     int currentAreaId, int currentPartId, const json11::Json &partitionData,
				     const QString &partitionError,
				     std::function<void(const std::string &title)> onTitleApply,
				     std::function<void(int areaId, int partId)> onPartitionApply)
{
	QDialog *dialog = createBaseDialog("更新直播间信息", parent);
	QVBoxLayout *layout = (QVBoxLayout *)dialog->layout();

	layout->addWidget(
		new QLabel(QString("直播间: https://live.bilibili.com/%1").arg(QString::fromStdString(roomUrl))));

	QLineEdit *titleInput = new QLineEdit(QString::fromStdString(currentTitle));
	QPushButton *confirmTitle = new QPushButton("确认");
	QHBoxLayout *titleRow = new QHBoxLayout();
	titleRow->addWidget(new QLabel("直播间标题:"));
	titleRow->addWidget(titleInput);
	titleRow->addWidget(confirmTitle);
	layout->addLayout(titleRow);

	QComboBox *partCombo = new QComboBox();
	QComboBox *areaCombo = new QComboBox();
	QPushButton *confirmPartition = new QPushButton("确认");
	QHBoxLayout *partitionRow = new QHBoxLayout();
	partitionRow->addWidget(new QLabel("直播间分区:"));
	partitionRow->addWidget(partCombo);
	partitionRow->addWidget(areaCombo);
	partitionRow->addWidget(confirmPartition);
	layout->addLayout(partitionRow);

	struct Part {
		int id;
		std::string name;
		std::vector<json11::Json> list;
	};
	std::vector<Part> parts;
	if (partitionData.is_array()) {
		for (const auto &item : partitionData.array_items()) {
			parts.push_back(
				{item["id"].int_value(), item["name"].string_value(), item["list"].array_items()});
		}
	}

	size_t selectedPartIndex = 0;
	for (size_t i = 0; i < parts.size(); ++i) {
		partCombo->addItem(QString::fromStdString(parts[i].name), parts[i].id);
		if (parts[i].id == currentPartId)
			selectedPartIndex = i;
	}
	partCombo->setCurrentIndex(static_cast<int>(selectedPartIndex));

	auto updateAreaCombo = [=](int partIndex) {
		areaCombo->clear();
		if (partIndex >= 0 && static_cast<size_t>(partIndex) < parts.size() && !parts[partIndex].list.empty()) {
			size_t selectedAreaIndex = 0;
			for (size_t i = 0; i < parts[partIndex].list.size(); ++i) {
				int id = 0;
				if (!parseAreaId(parts[partIndex].list[i]["id"], id) || id <= 0)
					continue;
				QString name = QString::fromStdString(parts[partIndex].list[i]["name"].string_value());
				areaCombo->addItem(name, id);
				if (id == currentAreaId)
					selectedAreaIndex = static_cast<size_t>(areaCombo->count() - 1);
			}
			areaCombo->setCurrentIndex(static_cast<int>(selectedAreaIndex));
		}
		areaCombo->setEnabled(areaCombo->count() > 0);
		confirmPartition->setEnabled(areaCombo->count() > 0);
	};
	updateAreaCombo(partCombo->currentIndex());
	partCombo->setEnabled(partCombo->count() > 0);
	if (!partitionError.isEmpty()) {
		auto *error = new QLabel(partitionError);
		error->setTextFormat(Qt::PlainText);
		error->setWordWrap(true);
		layout->addWidget(error);
	}
	confirmTitle->setEnabled(!titleInput->text().trimmed().isEmpty());
	QObject::connect(titleInput, &QLineEdit::textChanged, confirmTitle,
			 [confirmTitle](const QString &text) { confirmTitle->setEnabled(!text.trimmed().isEmpty()); });
	QObject::connect(partCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), updateAreaCombo);

	QObject::connect(confirmTitle, &QPushButton::clicked, [=]() {
		std::string newTitle = titleInput->text().trimmed().toUtf8().constData();
		if (!newTitle.empty() && onTitleApply) {
			onTitleApply(newTitle);
		}
	});

	QObject::connect(confirmPartition, &QPushButton::clicked, [=]() {
		if (onPartitionApply && areaCombo->currentIndex() >= 0) {
			onPartitionApply(areaCombo->currentData().toInt(), partCombo->currentData().toInt());
		}
	});

	QPushButton *closeBtn = new QPushButton("关闭");
	layout->addWidget(closeBtn);
	QObject::connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);

	QObject::connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
	dialog->exec();
	return dialog;
}

} // namespace UI
