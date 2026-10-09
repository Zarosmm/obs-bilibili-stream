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
#include "ui/async_task.hpp"

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
	void setBusy(bool busy);
	void validateLogin();
	bool m_busy = false;

	Core::ConfigManager m_config;
	UI::MenuManager *m_menu;
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
	if (!cfg.cookies.empty())
		validateLogin();

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

void BilibiliStreamPlugin::setBusy(bool busy)
{
	m_busy = busy;
	m_menu->actions().scanQrcode->setEnabled(!busy);
	m_menu->actions().streamToggle->setEnabled(!busy);
	m_menu->actions().updateRoomInfo->setEnabled(!busy);
}

void BilibiliStreamPlugin::validateLogin()
{
	setBusy(true);
	auto cfg = m_config.config();
	UI::runAsync(this, [this, cfg]() mutable {
		std::string message;
		cfg.login_status = Bili::BiliApi::checkLoginStatus(cfg.cookies, message, cfg.mid);
		bool roomOk = cfg.login_status &&
			      Bili::BiliApi::getRoomIdAndCsrf(cfg.cookies, cfg.room_id, cfg.csrf_token, message);
		return [this, cfg, roomOk] {
			auto &current = m_config.config();
			current.login_status = cfg.login_status;
			current.mid = cfg.mid;
			if (roomOk) {
				current.room_id = cfg.room_id;
				current.csrf_token = cfg.csrf_token;
			}
			m_config.save();
			updateLoginStatus();
			setBusy(false);
		};
	});
}

void BilibiliStreamPlugin::onScanQrcode()
{
	if (m_busy)
		return;
	setBusy(true);
	auto cookies = m_config.config().cookies;
	UI::runAsync(this, [this, cookies] {
		std::string data, key, message;
		bool ok = Bili::BiliApi::getQrCode(cookies, data, key, message);
		return [this, ok, data, key, message]() mutable {
			if (!ok) {
				setBusy(false);
				UI::DialogFactory::message(QString::fromStdString(message), "消息");
				return;
			}
			bool loggedIn = false;
			UI::DialogFactory::qrLogin((QWidget *)obs_frontend_get_main_window(), data, key,
						   [this, &loggedIn](const std::string &cookies) {
							   loggedIn = true;
							   m_config.config().cookies = cookies;
							   validateLogin();
						   });
			if (!loggedIn)
				setBusy(false);
		};
	});
}

void BilibiliStreamPlugin::onStreamToggle()
{
	if (m_busy)
		return;
	auto cfg = m_config.config();
	if (!cfg.streaming && !cfg.area_id) {
		UI::DialogFactory::message("请更新直播间分区", "消息");
		return;
	}
	setBusy(true);
	UI::runAsync(this, [this, cfg]() mutable {
		std::string message, addr, code, face;
		bool ok = cfg.streaming ? Bili::BiliApi::stopLive(cfg, message)
					: Bili::BiliApi::startLive(cfg, addr, code, message, face, cfg.mid);
		return [this, cfg, ok, message, addr, code, face] {
			setBusy(false);
			if (!ok) {
				if (!face.empty())
					UI::DialogFactory::faceAuth((QWidget *)obs_frontend_get_main_window(), face);
				else
					UI::DialogFactory::message(QString::fromStdString(message), "直播操作失败");
				return;
			}
			auto &current = m_config.config();
			current.streaming = !cfg.streaming;
			if (current.streaming) {
				current.rtmp_addr = addr;
				current.rtmp_code = code;
			}
			m_menu->actions().streamToggle->setText(current.streaming ? "停止直播" : "开始直播");
			m_config.save();
			if (current.streaming)
				UI::DialogFactory::streamStarted((QWidget *)obs_frontend_get_main_window(), addr, code);
			else
				UI::DialogFactory::message("直播已停止", "消息");
		};
	});
}

void BilibiliStreamPlugin::onUpdateRoomInfo()
{
	if (m_busy)
		return;
	auto cfg = m_config.config();
	auto apply = [this](std::string title, int areaId, int partId) {
		if (m_busy)
			return;
		setBusy(true);
		auto snapshot = m_config.config();
		UI::runAsync(this, [this, snapshot, title, areaId, partId] {
			std::string message;
			bool ok = Bili::BiliApi::updateRoomInfo(snapshot, message, title, areaId);
			return [this, ok, message, title, areaId, partId] {
				setBusy(false);
				if (ok) {
					auto &current = m_config.config();
					if (areaId >= 0) {
						current.area_id = areaId;
						current.part_id = partId;
					} else
						current.title = title;
					m_config.save();
				}
				UI::DialogFactory::message(ok ? QStringLiteral("直播间信息已更新")
							      : QString::fromStdString(message),
							   "消息");
			};
		});
	};
	UI::DialogFactory::roomSettings((QWidget *)obs_frontend_get_main_window(), cfg.room_id, cfg.title, cfg.area_id,
					cfg.part_id, [apply](const std::string &title) { apply(title, -1, -1); },
					[apply](int area, int part) { apply("", area, part); });
}

static BilibiliStreamPlugin *plugin = nullptr;

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "插件版本: %s, commit: %s", PLUGIN_VERSION, PLUGIN_COMMIT);
	Bili::BiliApi::init();
	auto mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow) {
		Bili::BiliApi::cleanup();
		return false;
	}
	plugin = new BilibiliStreamPlugin(mainWindow);
	obs_log(LOG_INFO, "插件加载成功");
	return true;
}

void obs_module_unload(void)
{
	Bili::BiliApi::cleanup();
	UI::shutdownAsync();
	delete plugin;
	plugin = nullptr;
	obs_log(LOG_INFO, "插件已卸载");
}

#include "plugin-main.moc"
