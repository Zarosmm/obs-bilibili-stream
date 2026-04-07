#include <obs-data.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QDesktopServices>
#include <QUrl>
#include "ui/menu_manager.hpp"
#include "ui/dialog_factory.hpp"
#include "core/config_manager.hpp"
#include "core/qr_generator.hpp"
#include "bilibili_api.hpp"
#include "plugin_utils.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

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

	Core::ConfigManager m_config;
	UI::MenuManager *m_menu;
};

BilibiliStreamPlugin::BilibiliStreamPlugin(QMainWindow *parent)
	: QObject(nullptr), m_menu(new UI::MenuManager(parent->menuBar(), this))
{
	m_config.load();

	connect(m_menu, &UI::MenuManager::scanQrcodeClicked, this, &BilibiliStreamPlugin::onScanQrcode);
	connect(m_menu, &UI::MenuManager::streamToggleClicked, this, &BilibiliStreamPlugin::onStreamToggle);
	connect(m_menu, &UI::MenuManager::openRoomClicked, this, &BilibiliStreamPlugin::onOpenRoom);
	connect(m_menu, &UI::MenuManager::updateRoomInfoClicked, this, &BilibiliStreamPlugin::onUpdateRoomInfo);

	auto &cfg = m_config.config();
	if (!cfg.cookies.empty()) {
		std::string message;
		if (Bili::BiliApi::checkLoginStatus(cfg.cookies, message, cfg.mid)) {
			cfg.login_status = true;
			std::string newRoomId, newCsrfToken;
			if (Bili::BiliApi::getRoomIdAndCsrf(cfg.cookies, newRoomId, newCsrfToken, message)) {
				cfg.room_id = newRoomId;
				cfg.csrf_token = newCsrfToken;
				m_config.save();
			}
		}
	}

	updateLoginStatus();
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
	auto &cfg = m_config.config();
	std::string qrData, qrKey, message;
	if (!Bili::BiliApi::getQrCode(cfg.cookies, qrData, qrKey, message)) {
		obs_log(LOG_ERROR, "获取二维码失败");
		UI::DialogFactory::message(QString::fromUtf8(message), "消息");
		return;
	}

	auto parent = (QWidget *)obs_frontend_get_main_window();
	UI::DialogFactory::qrLogin(parent, qrData, qrKey, [this, &cfg](const std::string &cookies) {
		std::string msg;
		cfg.cookies = cookies;
		cfg.login_status = Bili::BiliApi::checkLoginStatus(cfg.cookies, msg, cfg.mid);
		updateLoginStatus();

		std::string newRoomId, newCsrfToken;
		if (Bili::BiliApi::getRoomIdAndCsrf(cfg.cookies, newRoomId, newCsrfToken, msg)) {
			cfg.room_id = newRoomId;
			cfg.csrf_token = newCsrfToken;
		}
		m_config.save();
	});
}

void BilibiliStreamPlugin::onStreamToggle()
{
	auto &cfg = m_config.config();
	std::string message;

	if (cfg.streaming) {
		if (Bili::BiliApi::stopLive(cfg, message)) {
			m_menu->actions().streamToggle->setText("开始直播");
			cfg.streaming = false;
			m_config.save();
			UI::DialogFactory::message(QString::fromUtf8("直播已停止"), "消息");
		} else {
			UI::DialogFactory::message(QString::fromUtf8(message), "消息");
		}
	} else if (!cfg.area_id) {
		UI::DialogFactory::message(QString::fromUtf8("请更新直播间分区"), "消息");
	} else {
		std::string rtmpAddr, rtmpCode, faceQr;
		if (Bili::BiliApi::startLive(cfg, rtmpAddr, rtmpCode, message, faceQr, cfg.mid)) {
			m_menu->actions().streamToggle->setText("停止直播");
			cfg.streaming = true;
			cfg.rtmp_addr = rtmpAddr;
			cfg.rtmp_code = rtmpCode;
			m_config.save();
			UI::DialogFactory::streamStarted((QWidget *)obs_frontend_get_main_window(), rtmpAddr, rtmpCode);
		} else {
			if (!faceQr.empty()) {
				UI::DialogFactory::faceAuth((QWidget *)obs_frontend_get_main_window(), faceQr);
			} else {
				UI::DialogFactory::message(QString::fromUtf8(message.c_str()), "开播失败");
			}
		}
	}
}

void BilibiliStreamPlugin::onUpdateRoomInfo()
{
	auto &cfg = m_config.config();
	auto parent = (QWidget *)obs_frontend_get_main_window();

	UI::DialogFactory::roomSettings(parent, cfg.room_id, cfg.title, cfg.area_id, cfg.part_id,
					[this, &cfg](const std::string &title, int areaId, int partId) {
						std::string message;
						if (!title.empty() && Bili::BiliApi::updateRoomInfo(cfg, title, message)) {
							cfg.title = title;
							m_config.save();
							UI::DialogFactory::message(QString::fromUtf8("直播间标题已更新"), "消息");
						}
						if (areaId != cfg.area_id || partId != cfg.part_id) {
							cfg.part_id = partId;
							cfg.area_id = areaId;
							m_config.save();
							UI::DialogFactory::message(QString::fromUtf8("直播间分区已更新"), "消息");
						}
					});
}

static BilibiliStreamPlugin *plugin = nullptr;

bool obs_module_load(void)
{
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
