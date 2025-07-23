#include <obs-module.h>
#include <plugin-support.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QPainter>
#include <QLineEdit>
#include <QPushButton>
#include <windows.h>
#include "qrcodegen/qrcodegen.hpp"
#include "bili_api.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

class BilibiliStreamPlugin : public QObject {
    Q_OBJECT
public:
        explicit BilibiliStreamPlugin(QObject* parent = nullptr) : QObject(parent) {
                config.room_id = nullptr;
                config.csrf_token = nullptr;
                config.cookies = nullptr; // 初始化为 nullptr
                config.title = nullptr;
        }

	~BilibiliStreamPlugin() {
                if (config.cookies) {
                        free(config.cookies);
                        config.cookies = nullptr;
                }
                if (config.room_id) {
                        free(config.room_id);
                        config.room_id = nullptr;
                }
                if (config.csrf_token) {
                        free(config.csrf_token);
                        config.csrf_token = nullptr;
                }
                if (config.title) {
                        free(config.title);
                        config.title = nullptr;
                }
	}
	BiliConfig config;
private:

        static QPixmap generateQrCodePixmap(const char* qrcode_data) {
                if (!qrcode_data) {
                        obs_log(LOG_ERROR, "二维码数据为空，无法生成二维码");
                        return QPixmap();
                }

                obs_log(LOG_DEBUG, "qrcodegen 输入数据: %s", qrcode_data);

                // 使用 qrcodegen 生成二维码
                using qrcodegen::QrCode;

                const QrCode::Ecc errCorLvl = QrCode::Ecc::LOW; // 错误纠正级别
                const QrCode qr = QrCode::encodeText(qrcode_data, errCorLvl);
                if (qr.getSize() <= 0) {
                        obs_log(LOG_ERROR, "qrcodegen 生成二维码失败");
                        return QPixmap();
                }

                // 创建 QImage 绘制二维码
                int scale = 5; // 每个模块放大 5 倍
                int size = qr.getSize();
                QImage image(size * scale, size * scale, QImage::Format_RGB32);
                image.fill(Qt::white); // 白色背景

                QPainter painter(&image);
                painter.setPen(Qt::NoPen);
                painter.setBrush(Qt::black);
                for (int y = 0; y < size; y++) {
                        for (int x = 0; x < size; x++) {
                                if (qr.getModule(x, y)) {
					painter.drawRect(x * scale, y * scale, scale, scale);
                                }
                        }
                }

                obs_log(LOG_INFO, "qrcodegen 成功生成二维码图像，大小: %dx%d", image.width(), image.height());
                return QPixmap::fromImage(image);
        }

public slots:
	void onManualTriggered() {
		obs_log(LOG_INFO, "手动登录菜单项被点击");

                QDialog* loginDialog = new QDialog((QWidget*)obs_frontend_get_main_window());
                loginDialog->setWindowTitle("手动登录 - 输入Cookie");
                QVBoxLayout* layout = new QVBoxLayout(loginDialog);

                QLabel* label = new QLabel("请输入Bilibili Cookie:", loginDialog);
                layout->addWidget(label);

                QLineEdit* cookieInput = new QLineEdit(loginDialog);
                cookieInput->setPlaceholderText("在此输入Cookie (如: bili_jct=xxx; SESSDATA=yyy)");
                layout->addWidget(cookieInput);

                QPushButton* confirmButton = new QPushButton("确认", loginDialog);
                layout->addWidget(confirmButton);

                QObject::connect(confirmButton, &QPushButton::clicked, [=]() {
                        QString cookie = cookieInput->text().trimmed();
                        if (cookie.isEmpty()) {
                                obs_log(LOG_WARNING, "Cookie 输入为空，未保存");
                                loginDialog->reject();
                                return;
                        }

                        if (config.cookies) free(config.cookies);
                        config.cookies = strdup(cookie.toUtf8().constData());
                        obs_log(LOG_INFO, "Cookie 已保存: %s", config.cookies);

                        char* new_room_id = nullptr;
                        char* new_csrf_token = nullptr;
                        if (bili_get_room_id_and_csrf(config.cookies, &new_room_id, &new_csrf_token)) {
                                if (config.room_id) free((void*)config.room_id);
                                if (config.csrf_token) free((void*)config.csrf_token);
                                config.room_id = new_room_id;
                                config.csrf_token = new_csrf_token;
                        } else {
				obs_log(LOG_WARNING, "无法通过 cookies 获取 room_id 和 csrf_token，保留现有值");
                        }

                        setConfig(config);
                        loginDialog->accept();
                });

                QObject::connect(loginDialog, &QDialog::finished, [=]() {
			loginDialog->deleteLater();
                });

                loginDialog->exec();
        }

        void onScanQrcodeTriggered() {
                obs_log(LOG_INFO, "扫码登录菜单项被点击");
		char* qrcode_data = nullptr;
		char* qrcode_key = nullptr;
		if (!bili_get_qrcode(getConfig().cookies, &qrcode_data, &qrcode_key)) {
                        obs_log(LOG_ERROR, "获取二维码失败");
                        return;
		}

		obs_log(LOG_INFO, "二维码数据: %s, 二维码密钥: %s",
		        qrcode_data ? qrcode_data : "无数据",
		        qrcode_key ? qrcode_key : "无数据");

		QDialog* qrDialog = new QDialog((QWidget*)obs_frontend_get_main_window());
		qrDialog->setWindowTitle("Bilibili 登录二维码");
		QVBoxLayout* layout = new QVBoxLayout(qrDialog);
		QLabel* qrLabel = new QLabel(qrDialog);
		QPixmap qrPixmap = generateQrCodePixmap(qrcode_data);
		if (qrPixmap.isNull()) {
		        obs_log(LOG_ERROR, "二维码图像为空，无法显示");
		        qrLabel->setText("无法生成二维码");
		} else {
			qrLabel->setPixmap(qrPixmap);
		}
		layout->addWidget(qrLabel);
		QLabel* infoLabel = new QLabel("请使用Bilibili手机客户端扫描二维码登录", qrDialog);
		layout->addWidget(infoLabel);
		qrDialog->setLayout(layout);

		QTimer* timer = new QTimer(qrDialog);
		QObject::connect(timer, &QTimer::timeout, [this, qrDialog, timer, &qrcode_key]() mutable {
		        if (bili_qr_login(getConfig().cookies, &qrcode_key)) {
		                obs_log(LOG_INFO, "二维码登录成功，检查登录状态以获取 cookies");
		                char* new_cookies = nullptr;
		                if (bili_check_login_status(getConfig().cookies, &new_cookies)) {
		                        if (new_cookies) {
		                                if (config.cookies) free(config.cookies);
		                                config.cookies = new_cookies;

		                                char* new_room_id = nullptr;
		                                char* new_csrf_token = nullptr;
						if (bili_get_room_id_and_csrf(config.cookies, &new_room_id, &new_csrf_token)) {
		                                    if (config.room_id) free((void*)config.room_id);
		                                    if (config.csrf_token) free((void*)config.csrf_token);
		                                    config.room_id = new_room_id;
		                                    config.csrf_token = new_csrf_token;
						} else {
							obs_log(LOG_WARNING, "无法通过 cookies 获取 room_id 和 csrf_token，保留现有值");
						}

		                        }
		                }
		                timer->stop();
		                qrDialog->accept();
		        } else {
				obs_log(LOG_DEBUG, "二维码登录检查：尚未登录");
		        }
		});
		timer->start(1000);

		QObject::connect(qrDialog, &QDialog::finished, [=]() {
                        timer->stop();
                        free(qrcode_data);
                        free(qrcode_key);
                        qrDialog->deleteLater();
		});

		qrDialog->exec();
    }

    void onLoginStatusTriggered() {
            obs_log(LOG_INFO, "登录状态菜单项被点击");
            char* new_cookies = nullptr;
            if (bili_check_login_status(config.cookies, &new_cookies)) {
                    obs_log(LOG_INFO, "登录状态：已登录");
                    if (new_cookies) {
                            if (config.cookies) {
					free(config.cookies);
                            }
                            config.cookies = new_cookies;
                            obs_log(LOG_INFO, "更新 cookies: %s", config.cookies ? config.cookies : "无 cookies");
                    } else {
				obs_log(LOG_WARNING, "未获取到新的 cookies");
                    }
            } else {
                    obs_log(LOG_WARNING, "登录状态：未登录");
                    if (new_cookies) {
				free(new_cookies);
                    }
            }
    }

    void onPushStreamTriggered() {
            obs_log(LOG_INFO, "开始直播菜单项被点击");
            char* rtmp_addr = nullptr;
            char* rtmp_code = nullptr;
            if (bili_start_live(&config, 624, &rtmp_addr, &rtmp_code)) {
                    obs_log(LOG_INFO, "直播已启动，RTMP 地址: %s, 推流码: %s", rtmp_addr, rtmp_code);
                    obs_data_t* settings = obs_data_create();
                    obs_data_set_string(settings, "server", rtmp_addr);
                    obs_data_set_string(settings, "key", rtmp_code);
                    obs_output_t* output = obs_output_create("rtmp_output", "bilibili_stream", settings, nullptr);
                    obs_output_start(output);
                    obs_data_release(settings);
            }
            free(rtmp_addr);
            free(rtmp_code);
    }

    void onStopStreamTriggered() {
            obs_log(LOG_INFO, "停止直播菜单项被点击");
            if (bili_stop_live(&config)) {
			obs_log(LOG_INFO, "直播已停止");
            }
    }

    void onUpdateRoomInfoTriggered() {
            obs_log(LOG_INFO, "更新直播间信息菜单项被点击");
            if (bili_update_room_info(&config, 642)) {
			obs_log(LOG_INFO, "直播间信息更新成功");
            }
    }
};

static BilibiliStreamPlugin* plugin = nullptr;

bool obs_module_load(void)
{
        // Initialize Bilibili API
        bili_api_init();
        obs_log(LOG_DEBUG, "bili_api_init completed");

        // 获取 OBS 主窗口
        auto main_window = (QMainWindow*)obs_frontend_get_main_window();
        if (!main_window) {
	            obs_log(LOG_ERROR, "无法获取 OBS 主窗口");
	            return false;
        }

        // 创建插件对象
        plugin = new BilibiliStreamPlugin(main_window);
        if (!plugin) {
                obs_log(LOG_ERROR, "无法创建 BilibiliStreamPlugin 对象");
                return false;
        }

        // 从 OBS 数据库加载配置
        obs_data_t* settings = obs_get_private_data();
        if (settings) {
                const char* room_id = obs_data_get_string(settings, "bilibili_room_id");
                const char* csrf_token = obs_data_get_string(settings, "bilibili_csrf_token");
                const char* cookies = obs_data_get_string(settings, "bilibili_cookies");
                const char* title = obs_data_get_string(settings, "bilibili_title");

                plugin->config.room_id = room_id && strlen(room_id) > 0 ? strdup(room_id) : nullptr;
                plugin->config.csrf_token = csrf_token && strlen(csrf_token) > 0 ? strdup(csrf_token) : nullptr;
                plugin->config.cookies = cookies && strlen(cookies) > 0 ? strdup(cookies) : nullptr;
                plugin->config.title = title && strlen(title) > 0 ? strdup(title) : nullptr;

		// 如果有 cookies，尝试获取 room_id 和 csrf_token
                if (plugin->config.cookies && strlen(plugin->config.cookies) > 0) {
                        char* new_room_id = nullptr;
                        char* new_csrf_token = nullptr;
                        if (bili_get_room_id_and_csrf(plugin->config.cookies, &new_room_id, &new_csrf_token)) {
                                if (plugin->config.room_id) free((void*)plugin->config.room_id);
                                if (plugin->config.csrf_token) free((void*)plugin->config.csrf_token);
                                plugin->config.room_id = new_room_id;
                                plugin->config.csrf_token = new_csrf_token;
                        } else {
				obs_log(LOG_WARNING, "无法通过 cookies 获取 room_id 和 csrf_token，使用数据库值或默认值");
                        }
                }
		// Use default values for missing fields
                if (!plugin->config.room_id) plugin->config.room_id = strdup("12345");
                if (!plugin->config.csrf_token) plugin->config.csrf_token = strdup("your_csrf_token");
                if (!plugin->config.title) plugin->config.title = strdup("我的直播");

                plugin->setConfig(plugin->config);
                // Free temporary config strings
                if (plugin->config.room_id) free(plugin->config.room_id);
                if (plugin->config.csrf_token) free(plugin->config.csrf_token);
                if (plugin->config.cookies) free(plugin->config.cookies);
                if (plugin->config.title) free(plugin->config.title);

                obs_log(LOG_INFO, "从 OBS 数据库加载配置: room_id=%s, csrf_token=%s, cookies=%s, title=%s",
                        plugin->getConfig().room_id ? plugin->getConfig().room_id : "无",
                        plugin->getConfig().csrf_token ? plugin->getConfig().csrf_token : "无",
                        plugin->getConfig().cookies ? plugin->getConfig().cookies : "无",
                        plugin->getConfig().title ? plugin->getConfig().title : "无");

                obs_data_release(settings);
        } else {
                obs_log(LOG_WARNING, "无法从 OBS 数据库加载配置，使用默认配置");
                // 默认配置已在 BilibiliStreamPlugin 构造函数中设置
        }

        // 创建菜单
        auto menuBar = main_window->menuBar();
        if (!menuBar) {
                obs_log(LOG_ERROR, "无法获取菜单栏");
                delete plugin;
                plugin = nullptr;
                return false;
        }
        auto bilibiliMenu = menuBar->addMenu("Bilibili直播");

        // 添加菜单项
        auto login = bilibiliMenu->addMenu("登录");
        QAction* manual = login->addAction("手动登录");
        QAction* scanQrcode = login->addAction("扫码登录");
        QAction* loginStatus = login->addAction("登录状态");
        loginStatus->setCheckable(true);
        QAction* pushStream = bilibiliMenu->addAction("开始直播");
        QAction* stopStream = bilibiliMenu->addAction("停止直播");
        QAction* updateRoomInfo = bilibiliMenu->addAction("更新直播间信息");

        // 连接信号与槽
        QObject::connect(manual, &QAction::triggered, plugin, &BilibiliStreamPlugin::onManualTriggered);
        QObject::connect(scanQrcode, &QAction::triggered, plugin, &BilibiliStreamPlugin::onScanQrcodeTriggered);
        QObject::connect(loginStatus, &QAction::triggered, plugin, &BilibiliStreamPlugin::onLoginStatusTriggered);
        QObject::connect(pushStream, &QAction::triggered, plugin, &BilibiliStreamPlugin::onPushStreamTriggered);
        QObject::connect(stopStream, &QAction::triggered, plugin, &BilibiliStreamPlugin::onStopStreamTriggered);
        QObject::connect(updateRoomInfo, &QAction::triggered, plugin, &BilibiliStreamPlugin::onUpdateRoomInfoTriggered);

        obs_log(LOG_INFO, "插件加载成功，菜单已添加");
        return true;
}

void obs_module_unload(void)
{
        if (plugin) {
        	// 保存配置到 OBS 数据库
        	obs_data_t* settings = obs_data_create();
        	obs_data_set_string(settings, "bilibili_room_id", plugin->config.room_id ? plugin->config.room_id : "");
        	obs_data_set_string(settings, "bilibili_csrf_token", plugin->config.csrf_token ? plugin->config.csrf_token : "");
        	obs_data_set_string(settings, "bilibili_cookies", plugin->config.cookies ? plugin->config.cookies : "");
        	obs_data_set_string(settings, "bilibili_title", plugin->config.title ? plugin->config.title : "");
        	obs_set_private_data(settings);
        	obs_data_release(settings);
        	obs_log(LOG_INFO, "配置已保存到 OBS 数据库");
                delete plugin;
                plugin = nullptr;
        }
        // Cleanup Bilibili API
        bili_api_cleanup();
        obs_log(LOG_INFO, "插件已卸载");
}

#include "plugin-main.moc"
