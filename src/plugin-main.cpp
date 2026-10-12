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
	std::string mid;
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
	void updateLoginStatus();
	void openLiveRoom();
	void setBusy(bool busy);
	void refreshLoginState(const std::string &cookies);
	void applyRoomTitle(const std::string &title);
	void applyRoomPartition(int areaId, int partId);

	Core::ConfigManager m_config;
	UI::MenuManager *m_menu;
	bool m_busy = false;
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

	auto &cfg = m_config.config();
	if (!cfg.cookies.empty()) {
		// Refresh the cached login state and room id in the background: doing
		// this on the UI thread used to freeze OBS for up to 10 seconds.
		refreshLoginState(cfg.cookies);
	} else {
		updateLoginStatus();
	}

	if (cfg.streaming) {
		m_menu->actions().streamToggle->setText("停止直播");
	}
}

BilibiliStreamPlugin::~BilibiliStreamPlugin()
{
	obs_log(LOG_DEBUG, "释放 BilibiliStreamPlugin 资源");
}

void BilibiliStreamPlugin::updateLoginStatus()
{
	auto &cfg = m_config.config();
	m_menu->actions().loginStatus->setText(cfg.login_status ? "登录状态: 已登录" : "登录状态: 未登录");
	m_menu->actions().loginStatus->setChecked(cfg.login_status);
}

// Disables the actions that perform network requests while one is in flight, so
// a double click can no longer start a second, overlapping request.
void BilibiliStreamPlugin::setBusy(bool busy)
{
	m_busy = busy;
	auto &actions = m_menu->actions();
	actions.scanQrcode->setEnabled(!busy);
	actions.streamToggle->setEnabled(!busy);
	actions.updateRoomInfo->setEnabled(!busy);
}

void BilibiliStreamPlugin::refreshLoginState(const std::string &cookies)
{
	if (m_busy)
		return;

	auto result = std::make_shared<LoginResult>();
	setBusy(true);
	Http::HttpClient::runAsync(
		this,
		[cookies, result]() {
			result->loggedIn = Bili::BiliApi::checkLoginStatus(cookies, result->message, result->mid);
			if (result->loggedIn)
				result->roomOk = Bili::BiliApi::getRoomIdAndCsrf(cookies, result->roomId,
										 result->csrfToken, result->message);
		},
		[this, cookies, result]() {
			setBusy(false);
			auto &cfg = m_config.config();
			cfg.cookies = cookies;
			cfg.login_status = result->loggedIn;
			if (result->loggedIn)
				cfg.mid = result->mid;
			if (result->roomOk) {
				cfg.room_id = result->roomId;
				cfg.csrf_token = result->csrfToken;
			}
			m_config.save();
			updateLoginStatus();
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

	const std::string cookies = m_config.config().cookies;
	auto result = std::make_shared<QrCodeResult>();
	setBusy(true);
	Http::HttpClient::runAsync(
		this,
		[cookies, result]() {
			result->ok = Bili::BiliApi::getQrCode(cookies, result->qrData, result->qrKey, result->message);
		},
		[this, result]() {
			setBusy(false);
			if (!result->ok) {
				obs_log(LOG_ERROR, "获取二维码失败");
				UI::DialogFactory::message(QString::fromUtf8(result->message), "消息");
				return;
			}

			auto parent = (QWidget *)obs_frontend_get_main_window();
			UI::DialogFactory::qrLogin(parent, result->qrData, result->qrKey,
						   [this](const std::string &cookies) { refreshLoginState(cookies); });
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
					m_menu->actions().streamToggle->setText("开始直播");
					m_config.config().streaming = false;
					m_config.save();
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
		[config, result]() mutable {
			result->mid = config.mid;
			result->ok = Bili::BiliApi::startLive(config, result->rtmpAddr, result->rtmpCode,
							      result->message, result->faceQr, result->mid);
		},
		[this, result]() {
			setBusy(false);
			if (result->ok) {
				auto &cfg = m_config.config();
				m_menu->actions().streamToggle->setText("停止直播");
				cfg.streaming = true;
				cfg.rtmp_addr = result->rtmpAddr;
				cfg.rtmp_code = result->rtmpCode;
				m_config.save();
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
			if (!partitionData->is_array())
				UI::DialogFactory::message(QString::fromUtf8(result->message), "消息");

			auto &cfg = m_config.config();
			UI::DialogFactory::roomSettings(
				parent, cfg.room_id, cfg.title, cfg.area_id, cfg.part_id, *partitionData,
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
