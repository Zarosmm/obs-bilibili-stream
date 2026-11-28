#include <obs-data.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QDesktopServices>
#include <QUrl>
#include <memory>
#include "ui/menu_manager.hpp"
#include "ui/dialog_factory.hpp"
#include "core/config_manager.hpp"
#include "core/qr_generator.hpp"
#include "bilibili_api.hpp"
#include "http_client.hpp"
#include "plugin_utils.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

// Result of a background request. The worker thread only writes it, the
// continuation (which runs on the UI thread) only reads it, so no plugin state
// is touched from the worker.
struct LoginResult {
	bool loggedIn = false;
	bool roomOk = false;
	std::string message;
	std::string mid;
	std::string roomId;
	std::string csrfToken;
};

struct QrCodeResult {
	bool ok = false;
	std::string qrData;
	std::string qrKey;
	std::string message;
};

struct StreamResult {
	bool ok = false;
	std::string message;
	std::string rtmpAddr;
	std::string rtmpCode;
	std::string faceQr;
};

struct UpdateResult {
	bool ok = false;
	std::string message;
};

class BilibiliStreamPlugin : public QObject {
	Q_OBJECT
public:
	explicit BilibiliStreamPlugin(QMainWindow *parent);
	~BilibiliStreamPlugin();

private slots:
	void onScanQrcode();
	void onStreamToggle();
	void onOpenRoom();
	void onUpdateRoomInfo();

private:
	void updateActions();
	void openLiveRoom();
	void setBusy(bool busy);
	void refreshLoginState(const std::string &cookies, bool interactive = false);
	void applyRoomTitle(const std::string &title);
	void applyRoomPartition(int areaId, int partId);

	Core::ConfigManager m_config;
	UI::MenuManager *m_menu;
	bool m_busy = false;
	bool m_checkingLogin = false;
};

BilibiliStreamPlugin::BilibiliStreamPlugin(QMainWindow *parent)
	: QObject(nullptr),
	  m_menu(new UI::MenuManager(parent->menuBar(), this))
{
	m_config.load();

	connect(m_menu, &UI::MenuManager::scanQrcodeClicked, this, &BilibiliStreamPlugin::onScanQrcode);
	connect(m_menu, &UI::MenuManager::streamToggleClicked, this, &BilibiliStreamPlugin::onStreamToggle);
	connect(m_menu, &UI::MenuManager::openRoomClicked, this, &BilibiliStreamPlugin::onOpenRoom);
	connect(m_menu, &UI::MenuManager::updateRoomInfoClicked, this, &BilibiliStreamPlugin::onUpdateRoomInfo);

	const auto &cfg = m_config.config();
	if (!cfg.cookies.empty())
		refreshLoginState(cfg.cookies);
	else
		updateActions();
}

BilibiliStreamPlugin::~BilibiliStreamPlugin()
{
	obs_log(LOG_DEBUG, "释放 BilibiliStreamPlugin 资源");
}

void BilibiliStreamPlugin::updateActions()
{
	const auto &cfg = m_config.config();
	auto &actions = m_menu->actions();
	const bool roomReady = cfg.login_status && !cfg.room_id.empty() && !cfg.csrf_token.empty();
	QString status = QStringLiteral("未登录，请扫码登录");
	if (m_checkingLogin)
		status = QStringLiteral("正在检查登录状态…");
	else if (cfg.login_status)
		status = roomReady ? QStringLiteral("已登录") : QStringLiteral("已登录，直播间未就绪");
	else if (!cfg.cookies.empty())
		status = QStringLiteral("无法确认登录状态，请重新扫码");
	actions.loginStatus->setText(status);
	actions.scanQrcode->setEnabled(!m_busy);
	actions.streamToggle->setText(cfg.streaming ? QStringLiteral("停止直播") : QStringLiteral("开始直播"));
	actions.streamToggle->setEnabled(!m_busy && roomReady);
	actions.openRoom->setEnabled(!m_busy && roomReady);
	actions.updateRoomInfo->setEnabled(!m_busy && roomReady);
}

void BilibiliStreamPlugin::setBusy(bool busy)
{
	m_busy = busy;
	updateActions();
}

void BilibiliStreamPlugin::refreshLoginState(const std::string &cookies, bool interactive)
{
	if (m_busy)
		return;

	auto result = std::make_shared<LoginResult>();
	m_checkingLogin = true;
	setBusy(true);
	Http::HttpClient::runAsync(
		this,
		[cookies, result]() {
			result->loggedIn = Bili::BiliApi::checkLoginStatus(cookies, result->message, result->mid);
			if (result->loggedIn)
				result->roomOk = Bili::BiliApi::getRoomIdAndCsrf(cookies, result->roomId,
										 result->csrfToken, result->message);
		},
		[this, cookies, interactive, result]() {
			auto &cfg = m_config.config();
			if (interactive && result->loggedIn) {
				if (cfg.mid != result->mid)
					cfg.streaming = false;
				cfg.cookies = cookies;
				m_config.save();
			}
			if (!interactive || result->loggedIn) {
				cfg.login_status = result->loggedIn;
				cfg.mid = result->mid;
				cfg.room_id = result->roomOk ? result->roomId : std::string{};
				cfg.csrf_token = result->roomOk ? result->csrfToken : std::string{};
			}
			m_checkingLogin = false;
			setBusy(false);
			if (interactive && !result->roomOk) {
				const auto message = result->message.empty()
							     ? QStringLiteral("无法确认登录状态，请重新扫码")
							     : QString::fromUtf8(result->message);
				UI::DialogFactory::message(message, "登录");
			}
		});
}

void BilibiliStreamPlugin::applyRoomTitle(const std::string &title)
{
	if (m_busy)
		return;

	auto config = m_config.config();
	auto result = std::make_shared<UpdateResult>();
	setBusy(true);
	Http::HttpClient::runAsync(
		this,
		[config, title, result]() {
			result->ok = Bili::BiliApi::updateRoomInfo(config, result->message, title, -1);
		},
		[this, title, result]() {
			setBusy(false);
			if (result->ok) {
				m_config.config().title = title;
				m_config.save();
				UI::DialogFactory::message(QString::fromUtf8("直播间标题已更新"), "消息");
			} else {
				UI::DialogFactory::message(QString::fromUtf8(result->message), "错误");
			}
		});
}

void BilibiliStreamPlugin::applyRoomPartition(int areaId, int partId)
{
	if (m_busy)
		return;

	auto config = m_config.config();
	auto result = std::make_shared<UpdateResult>();
	setBusy(true);
	Http::HttpClient::runAsync(
		this,
		[config, areaId, result]() {
			result->ok = Bili::BiliApi::updateRoomInfo(config, result->message, "", areaId);
		},
		[this, areaId, partId, result]() {
			setBusy(false);
			if (result->ok) {
				auto &cfg = m_config.config();
				cfg.part_id = partId;
				cfg.area_id = areaId;
				m_config.save();
				UI::DialogFactory::message(QString::fromUtf8("直播间分区已更新"), "消息");
			} else {
				UI::DialogFactory::message(QString::fromUtf8(result->message), "错误");
			}
		});
}

void BilibiliStreamPlugin::openLiveRoom()
{
	auto &cfg = m_config.config();
	if (cfg.room_id.empty()) {
		UI::DialogFactory::message(QString::fromUtf8("room_id 为空，请先登录或更新直播间信息"), "消息");
		return;
	}
	const QString url = QString("https://live.bilibili.com/%1").arg(QString::fromStdString(cfg.room_id));
	if (!QDesktopServices::openUrl(QUrl(url))) {
		UI::DialogFactory::message(QStringLiteral("无法打开浏览器，请手动访问：\n") + url, "消息");
	}
}

void BilibiliStreamPlugin::onOpenRoom()
{
	openLiveRoom();
}

void BilibiliStreamPlugin::onScanQrcode()
{
	if (m_busy)
		return;

	auto result = std::make_shared<QrCodeResult>();
	setBusy(true);
	Http::HttpClient::runAsync(
		this,
		[result]() { result->ok = Bili::BiliApi::getQrCode(result->qrData, result->qrKey, result->message); },
		[this, result]() {
			setBusy(false);
			if (!result->ok) {
				UI::DialogFactory::message(QString::fromUtf8(result->message), "消息");
				return;
			}

			auto parent = (QWidget *)obs_frontend_get_main_window();
			UI::DialogFactory::qrLogin(parent, result->qrData, result->qrKey,
						   [this](const std::string &cookies) {
							   refreshLoginState(cookies, true);
						   });
		});
}

void BilibiliStreamPlugin::onStreamToggle()
{
	if (m_busy)
		return;

	const auto &cfg = m_config.config();
	if (cfg.streaming) {
		auto config = m_config.config();
		auto result = std::make_shared<UpdateResult>();
		setBusy(true);
		Http::HttpClient::runAsync(
			this, [config, result]() { result->ok = Bili::BiliApi::stopLive(config, result->message); },
			[this, result]() {
				setBusy(false);
				if (result->ok) {
					m_config.config().streaming = false;
					updateActions();
					UI::DialogFactory::message(QString::fromUtf8("直播已停止"), "消息");
				} else {
					UI::DialogFactory::message(QString::fromUtf8(result->message), "消息");
				}
			});
		return;
	}

	if (!cfg.area_id) {
		UI::DialogFactory::message(QString::fromUtf8("请更新直播间分区"), "消息");
		return;
	}

	// startLive() performs two blocking requests; run them on the worker thread
	// and apply the result (RTMP address, menu text, config) on the UI thread.
	auto config = m_config.config();
	auto result = std::make_shared<StreamResult>();
	setBusy(true);
	Http::HttpClient::runAsync(
		this,
		[config, result]() {
			result->ok = Bili::BiliApi::startLive(config, result->rtmpAddr, result->rtmpCode,
							      result->message, result->faceQr);
		},
		[this, result]() {
			setBusy(false);
			if (result->ok) {
				m_config.config().streaming = true;
				updateActions();
				UI::DialogFactory::streamStarted((QWidget *)obs_frontend_get_main_window(),
								 result->rtmpAddr, result->rtmpCode);
			} else if (!result->faceQr.empty()) {
				UI::DialogFactory::faceAuth((QWidget *)obs_frontend_get_main_window(), result->faceQr);
			} else {
				UI::DialogFactory::message(QString::fromUtf8(result->message.c_str()), "开播失败");
			}
		});
}

void BilibiliStreamPlugin::onUpdateRoomInfo()
{
	if (m_busy)
		return;

	auto parent = (QWidget *)obs_frontend_get_main_window();
	// The partition list is needed to build the dialog: fetch it on the worker
	// thread first (the dialog used to do this synchronously while opening) and
	// only then show the dialog.
	auto partitionData = std::make_shared<json11::Json>();
	auto result = std::make_shared<UpdateResult>();
	setBusy(true);
	Http::HttpClient::runAsync(
		this, [partitionData, result]() { *partitionData = Bili::BiliApi::getPartitionList(result->message); },
		[this, parent, partitionData, result]() {
			setBusy(false);
			auto &cfg = m_config.config();
			UI::DialogFactory::roomSettings(
				parent, cfg.room_id, cfg.title, cfg.area_id, cfg.part_id, *partitionData,
				QString::fromUtf8(result->message),
				[this](const std::string &title) { applyRoomTitle(title); },
				[this](int areaId, int partId) { applyRoomPartition(areaId, partId); });
		});
}

static BilibiliStreamPlugin *plugin = nullptr;

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "插件版本: %s, commit: %s", PLUGIN_VERSION, PLUGIN_COMMIT);
	Bili::BiliApi::init();
	auto mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow)
		return false;
	plugin = new BilibiliStreamPlugin(mainWindow);
	obs_log(LOG_INFO, "插件加载成功");
	return true;
}

void obs_module_unload(void)
{
	Bili::BiliApi::cleanup();
	delete plugin;
	plugin = nullptr;
	obs_log(LOG_INFO, "插件已卸载");
}

#include "plugin-main.moc"
