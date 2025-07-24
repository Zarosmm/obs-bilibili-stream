#include <obs-data.h>
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
#include <QComboBox>
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
        config.cookies = nullptr;
        config.title = nullptr;
        config.login_status = false;
        config.streaming = false;
        config.rtmp_addr = nullptr;
        config.rtmp_code = nullptr;
		config.part_id = 2;
		config.area_id = 86;
    }

    ~BilibiliStreamPlugin() {
        if (config.cookies) free(config.cookies);
        if (config.room_id) free(config.room_id);
        if (config.csrf_token) free(config.csrf_token);
        if (config.title) free(config.title);
        if (config.rtmp_code) free(config.rtmp_code);
        if (config.rtmp_addr) free(config.rtmp_addr);
    }

    BiliConfig config;
    QAction* loginStatusAction = nullptr;
    QAction* streamAction = nullptr;

private:
    static QPixmap generateQrCodePixmap(const char* qrcode_data) {
        if (!qrcode_data) {
            obs_log(LOG_ERROR, "二维码数据为空，无法生成二维码");
            return QPixmap();
        }

        obs_log(LOG_DEBUG, "qrcodegen 输入数据: %s", qrcode_data);

        using qrcodegen::QrCode;
        const QrCode::Ecc errCorLvl = QrCode::Ecc::LOW;
        const QrCode qr = QrCode::encodeText(qrcode_data, errCorLvl);
        if (qr.getSize() <= 0) {
            obs_log(LOG_ERROR, "qrcodegen 生成二维码失败");
            return QPixmap();
        }

        int scale = 5;
        int size = qr.getSize();
        QImage image(size * scale, size * scale, QImage::Format_RGB32);
        image.fill(Qt::white);
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
            obs_log(LOG_INFO, "Cookie 已保存: %s", config.cookies ? config.cookies : "无 cookies");

            if (bili_check_login_status(config.cookies)) {
                config.login_status = true;
                onLoginStatusTriggered();

                char* new_room_id = nullptr;
                char* new_csrf_token = nullptr;
                if (bili_get_room_id_and_csrf(config.cookies, &new_room_id, &new_csrf_token)) {
                    if (config.room_id) free(config.room_id);
                    if (config.csrf_token) free(config.csrf_token);
                    config.room_id = new_room_id;
                    config.csrf_token = new_csrf_token;
                    obs_log(LOG_INFO, "更新 room_id: %s, csrf_token: %s",
                            config.room_id ? config.room_id : "无",
                            config.csrf_token ? config.csrf_token : "无");
                } else {
                    obs_log(LOG_WARNING, "无法通过 cookies 获取 room_id 和 csrf_token，保留现有值");
                }
            } else {
                config.login_status = false;
                onLoginStatusTriggered();
                obs_log(LOG_WARNING, "登录失败，请检查Cookie是否正确");
            }

            char* config_file = nullptr;
		    config_file = obs_module_file("config.json");
		    if (config_file) {
		    	obs_data_t* settings = obs_data_create();
                obs_data_set_string(settings, "room_id", config.room_id ? config.room_id : "");
                obs_data_set_string(settings, "csrf_token", config.csrf_token ? config.csrf_token : "");
                obs_data_set_string(settings, "cookies", config.cookies ? config.cookies : "");
                obs_data_set_string(settings, "title", config.title ? config.title : "");
                obs_data_set_string(settings, "rtmp_addr", config.rtmp_addr ? config.rtmp_addr : "");
                obs_data_set_string(settings, "rtmp_code", config.rtmp_code ? config.rtmp_code : "");
				obs_data_set_int(settings, "part_id", config.part_id ? config.part_id : 2);
				obs_data_set_int(settings, "area_id", config.area_id ? config.area_id : 86);
                obs_data_save_json(settings, config_file);
                obs_data_release(settings);
                obs_log(LOG_INFO, "配置已保存到 OBS 数据库");
				bfree(config_file);
		    }
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
        if (!bili_get_qrcode(config.cookies, &qrcode_data, &qrcode_key)) {
            obs_log(LOG_ERROR, "获取二维码失败");
            free(qrcode_data);
            free(qrcode_key);
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
        QLabel* infoLabel = new QLabel("使用手机扫描二维码登录", qrDialog);
        layout->addWidget(infoLabel);
        qrDialog->setLayout(layout);

        QTimer* timer = new QTimer(qrDialog);
        QObject::connect(timer, &QTimer::timeout, [this, qrDialog, timer, &qrcode_key]() mutable {
            char* cookies = nullptr;
            if (bili_qr_login(&qrcode_key, &cookies)) {
				timer->stop();
                config.cookies = cookies;
                obs_log(LOG_INFO, "从 qr_login 获取 cookies: %s", config.cookies ? config.cookies : "无 cookies");
                obs_log(LOG_INFO, "二维码登录成功，检查登录状态");
                if (bili_check_login_status(config.cookies)) {
                    config.login_status = true;
                    onLoginStatusTriggered();

                    char* new_room_id = nullptr;
                    char* new_csrf_token = nullptr;
                    if (bili_get_room_id_and_csrf(config.cookies, &new_room_id, &new_csrf_token)) {
                        if (config.room_id) free(config.room_id);
                        if (config.csrf_token) free(config.csrf_token);
                        config.room_id = new_room_id;
                        config.csrf_token = new_csrf_token;
                        obs_log(LOG_INFO, "更新 room_id: %s, csrf_token: %s",
                                config.room_id ? config.room_id : "无",
                                config.csrf_token ? config.csrf_token : "无");
                    } else {
                        obs_log(LOG_WARNING, "无法通过 cookies 获取 room_id 和 csrf_token，保留现有值");
                    }

                    char* config_file = nullptr;
		            config_file = obs_module_file("config.json");
		            if (config_file) {
		            	obs_data_t* settings = obs_data_create();
                        obs_data_set_string(settings, "room_id", config.room_id ? config.room_id : "");
                        obs_data_set_string(settings, "csrf_token", config.csrf_token ? config.csrf_token : "");
                        obs_data_set_string(settings, "cookies", config.cookies ? config.cookies : "");
                        obs_data_set_string(settings, "title", config.title ? config.title : "");
                        obs_data_set_string(settings, "rtmp_addr", config.rtmp_addr ? config.rtmp_addr : "");
                        obs_data_set_string(settings, "rtmp_code", config.rtmp_code ? config.rtmp_code : "");
						obs_data_set_int(settings, "part_id", config.part_id ? config.part_id : 2);
						obs_data_set_int(settings, "area_id", config.area_id ? config.area_id : 86);
                        obs_data_save_json(settings, config_file);
                        obs_data_release(settings);
                        obs_log(LOG_INFO, "配置已保存到 OBS 数据库");
                        bfree(config_file);
		            }
                    qrDialog->accept();
                } else {
                    obs_log(LOG_WARNING, "登录状态检查失败");
                    config.login_status = false;
                    onLoginStatusTriggered();
                }
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
        if (config.login_status) {
            loginStatusAction->setText("登录状态: 已登录");
            loginStatusAction->setChecked(true);
            obs_log(LOG_INFO, "登录状态：已登录");
        } else {
            loginStatusAction->setText("登录状态: 未登录");
            loginStatusAction->setChecked(false);
            obs_log(LOG_INFO, "登录状态：未登录");
        }
    }

    void onstreamButtonTriggered() {
        if (config.streaming) {
            if (bili_stop_live(&config)) {
                streamAction->setText("开始直播");
                config.streaming = false;
                obs_log(LOG_INFO, "直播已停止");
            }
        } else {
            char* rtmp_addr = nullptr;
            char* rtmp_code = nullptr;
            if (bili_start_live(&config, &rtmp_addr, &rtmp_code)) {
                obs_log(LOG_INFO, "直播已启动，RTMP 地址: %s, 推流码: %s", rtmp_addr, rtmp_code);
                streamAction->setText("停止直播");
                config.streaming = true;
                config.rtmp_addr = rtmp_addr;
                config.rtmp_code = rtmp_code;
            }
            free(rtmp_addr);
            free(rtmp_code);
        }
		char* config_file = nullptr;
		config_file = obs_module_file("config.json");
		if (config_file) {
			obs_data_t* settings = obs_data_create();
            obs_data_set_string(settings, "room_id", config.room_id ? config.room_id : "");
            obs_data_set_string(settings, "csrf_token", config.csrf_token ? config.csrf_token : "");
            obs_data_set_string(settings, "cookies", config.cookies ? config.cookies : "");
            obs_data_set_string(settings, "title", config.title ? config.title : "");
            obs_data_set_string(settings, "rtmp_addr", config.rtmp_addr ? config.rtmp_addr : "");
            obs_data_set_string(settings, "rtmp_code", config.rtmp_code ? config.rtmp_code : "");
			obs_data_set_int(settings, "part_id", config.part_id ? config.part_id : 2);
			obs_data_set_int(settings, "area_id", config.area_id ? config.area_id : 86);
            obs_data_save_json(settings, config_file);
            obs_data_release(settings);
            obs_log(LOG_INFO, "配置已保存到 OBS 数据库");
            bfree(config_file);
		}
    }

    void onUpdateRoomInfoTriggered() {
        obs_log(LOG_INFO, "更新直播间信息菜单项被点击");

        // 创建对话框
        QDialog* dialog = new QDialog((QWidget*)obs_frontend_get_main_window());
        dialog->setWindowTitle("更新直播间信息");
        QVBoxLayout* layout = new QVBoxLayout(dialog);

        // 标题输入框
        QLabel* titleLabel = new QLabel("直播间标题:", dialog);
        QLineEdit* titleInput = new QLineEdit(config.title ? config.title : "我的直播", dialog);
        layout->addWidget(titleLabel);
        layout->addWidget(titleInput);

        // 分区下拉框
        QLabel* partLabel = new QLabel("分区:", dialog);
        QComboBox* partCombo = new QComboBox(dialog);
        layout->addWidget(partLabel);
        layout->addWidget(partCombo);

        // 子分区下拉框
        QLabel* areaLabel = new QLabel("子分区:", dialog);
        QComboBox* areaCombo = new QComboBox(dialog);
        layout->addWidget(areaLabel);
        layout->addWidget(areaCombo);

        // 确定和取消按钮
        QPushButton* confirmButton = new QPushButton("确定", dialog);
        QPushButton* cancelButton = new QPushButton("取消", dialog);
        layout->addWidget(confirmButton);
        layout->addWidget(cancelButton);

        // 读取 partition.json
        char* partition_file = obs_module_file("partition.json");
        obs_data_t* partition_data = nullptr;
        partition_data = obs_data_create_from_json_file(partition_file);
        if (!partition_data) {
            obs_log(LOG_ERROR, "无法解析 partition.json: %s", partition_file);
        }
        if (partition_file) bfree(partition_file);

        // 填充分区下拉框
        struct Part {
            int id;
            QString name;
            obs_data_t* list;
        };
        std::vector<Part> parts;
        if (partition_data) {
            obs_data_array_t* array = obs_data_get_array(partition_data, "");
            size_t count = obs_data_array_count(array);
            for (size_t i = 0; i < count; i++) {
                obs_data_t* item = obs_data_array_item(array, i);
                Part part;
                part.id = obs_data_get_int(item, "id");
                part.name = QString::fromUtf8(obs_data_get_string(item, "name"));
                part.list = obs_data_get_array(item, "list");
                parts.push_back(part);
                obs_data_release(item);
            }
            obs_data_array_release(array);
        }

        // 如果 partition.json 为空，使用默认分区
        if (parts.empty()) {
            parts.push_back({2, "网游", nullptr});
            obs_log(LOG_WARNING, "partition.json 为空，使用默认分区: 网游");
        }

        // 填充分区下拉框并设置默认值
        int selected_part_index = 0;
        for (size_t i = 0; i < parts.size(); i++) {
            partCombo->addItem(parts[i].name, parts[i].id);
            if (parts[i].id == config.part_id) {
                selected_part_index = i;
            }
        }
        partCombo->setCurrentIndex(selected_part_index);

        // 填充子分区下拉框
        auto updateAreaCombo = [&](int part_index) {
            areaCombo->clear();
            if (part_index >= 0 && part_index < (int)parts.size() && parts[part_index].list) {
                obs_data_array_t* areas = parts[part_index].list;
                size_t count = obs_data_array_count(areas);
                int selected_area_index = 0;
                for (size_t i = 0; i < count; i++) {
                    obs_data_t* area = obs_data_array_item(areas, i);
                    int id = obs_data_get_int(area, "id");
                    QString name = QString::fromUtf8(obs_data_get_string(area, "name"));
                    areaCombo->addItem(name, id);
                    if (id == config.area_id) {
                        selected_area_index = i;
                    }
                    obs_data_release(area);
                }
                areaCombo->setCurrentIndex(selected_area_index);
            } else {
                // 默认子分区（如果 part 没有 list 或无效）
                areaCombo->addItem("英雄联盟", 86);
                if (config.area_id == 86) {
                    areaCombo->setCurrentIndex(0);
                }
            }
        };

        // 初始化子分区下拉框
        updateAreaCombo(selected_part_index);

        // 分区选择改变时更新子分区
        QObject::connect(partCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
            updateAreaCombo(index);
        });

        // 确定按钮
        QObject::connect(confirmButton, &QPushButton::clicked, [=]() {
            // 更新 config
            QString new_title = titleInput->text().trimmed();
            if (!new_title.isEmpty()) {
                if (config.title) free(config.title);
                config.title = strdup(new_title.toUtf8().constData());
            }

            config.part_id = partCombo->currentData().toInt();
            config.area_id = areaCombo->currentData().toInt();

            // 调用 bili_update_room_info
            if (bili_update_room_info(&config, config.area_id)) {
                obs_log(LOG_INFO, "直播间信息更新成功: title=%s, part_id=%d, area_id=%d",
                        config.title, config.part_id, config.area_id);
            } else {
                obs_log(LOG_ERROR, "直播间信息更新失败");
            }

            // 保存配置
            char* config_file = obs_module_config_path("config.json");
            if (config_file) {
                obs_data_t* settings = obs_data_create();
                obs_data_set_string(settings, "room_id", config.room_id ? config.room_id : "");
                obs_data_set_string(settings, "csrf_token", config.csrf_token ? config.csrf_token : "");
                obs_data_set_string(settings, "cookies", config.cookies ? config.cookies : "");
                obs_data_set_string(settings, "title", config.title ? config.title : "");
                obs_data_set_string(settings, "rtmp_addr", config.rtmp_addr ? config.rtmp_addr : "");
                obs_data_set_string(settings, "rtmp_code", config.rtmp_code ? config.rtmp_code : "");
                obs_data_set_int(settings, "part_id", config.part_id);
                obs_data_set_int(settings, "area_id", config.area_id);
                obs_data_save_json(settings, config_file);
                obs_data_release(settings);
                obs_log(LOG_INFO, "配置已保存到 OBS 用户配置目录");
                bfree(config_file);
            }

            dialog->accept();
        });

        // 取消按钮
        QObject::connect(cancelButton, &QPushButton::clicked, [=]() {
            dialog->reject();
        });

        // 清理
        QObject::connect(dialog, &QDialog::finished, [=]() {
            if (partition_data) obs_data_release(partition_data);
            for (auto& part : parts) {
                if (part.list) obs_data_array_release(part.list);
            }
            dialog->deleteLater();
        });

        dialog->exec();
    }
};

static BilibiliStreamPlugin* plugin = nullptr;

bool obs_module_load(void) {
    bili_api_init();
    obs_log(LOG_DEBUG, "bili_api_init completed");

    auto main_window = (QMainWindow*)obs_frontend_get_main_window();
    if (!main_window) {
        obs_log(LOG_ERROR, "无法获取 OBS 主窗口");
        return false;
    }

    plugin = new BilibiliStreamPlugin(main_window);
    if (!plugin) {
        obs_log(LOG_ERROR, "无法创建 BilibiliStreamPlugin 对象");
        return false;
    }
	char* config_file = nullptr;
	config_file = obs_module_file("config.json"	);
	if (config_file) {
		obs_log(LOG_INFO, "配置文件路径: %s", config_file);
		obs_data_t* settings = obs_data_create_from_json_file(config_file);
        if (settings) {
            const char* room_id = obs_data_get_string(settings, "room_id");
            const char* csrf_token = obs_data_get_string(settings, "csrf_token");
            const char* cookies = obs_data_get_string(settings, "cookies");
            const char* title = obs_data_get_string(settings, "title");
			const char* rtmp_addr = obs_data_get_string(settings, "rtmp_addr");
			const char* rtmp_code = obs_data_get_string(settings, "rtmp_code");
            const int part_id = obs_data_get_int(settings, "part_id");
            const int area_id = obs_data_get_int(settings, "area_id");
			obs_data_release(settings);
		    obs_log(LOG_INFO, "从数据库加载配置");
		    obs_log(LOG_INFO, "cookies: %s", cookies && strlen(cookies) ? cookies : "");
		    obs_log(LOG_INFO, "csrf_token: %s", csrf_token && strlen(csrf_token) ? csrf_token : "");
		    obs_log(LOG_INFO, "room_id: %s", room_id && strlen(room_id) ? room_id : "");
		    obs_log(LOG_INFO, "title: %s", title && strlen(title) ? title : "");
			obs_log(LOG_INFO, "rtmp_addr: %s", rtmp_addr && strlen(rtmp_addr) ? rtmp_addr : "");
			obs_log(LOG_INFO, "rtmp_code: %s", rtmp_code && strlen(rtmp_code) ? rtmp_code : "");
			obs_log(LOG_INFO, "part_id: %d", part_id);
			obs_log(LOG_INFO, "area_id: %d", area_id);
            plugin->config.room_id = room_id && strlen(room_id) > 0 ? strdup(room_id) : nullptr;
            plugin->config.csrf_token = csrf_token && strlen(csrf_token) > 0 ? strdup(csrf_token) : nullptr;
            plugin->config.cookies = cookies && strlen(cookies) > 0 ? strdup(cookies) : nullptr;
            plugin->config.title = title && strlen(title) > 0 ? strdup(title) : nullptr;
			plugin->config.rtmp_addr = rtmp_addr && strlen(rtmp_addr) > 0 ? strdup(rtmp_addr) : nullptr;
			plugin->config.rtmp_code = rtmp_code && strlen(rtmp_code) > 0 ? strdup(rtmp_code) : nullptr;
            plugin->config.part_id = part_id;
            plugin->config.area_id = area_id;

		} else {
            obs_log(LOG_WARNING, "无法从 OBS 数据库加载配置，使用默认配置");
        }
        if (plugin->config.cookies && strlen(plugin->config.cookies) > 0) {
        	if (bili_check_login_status(plugin->config.cookies)) {
            	plugin->config.login_status = true;
            	char* new_room_id = nullptr;
            	char* new_csrf_token = nullptr;
            	if (bili_get_room_id_and_csrf(plugin->config.cookies, &new_room_id, &new_csrf_token)) {
                	if (plugin->config.room_id) free(plugin->config.room_id);
                	if (plugin->config.csrf_token) free(plugin->config.csrf_token);
                	plugin->config.room_id = new_room_id;
                	plugin->config.csrf_token = new_csrf_token;
                	obs_log(LOG_INFO, "更新 room_id: %s, csrf_token: %s",
                        	plugin->config.room_id ? plugin->config.room_id : "无",
                        	plugin->config.csrf_token ? plugin->config.csrf_token : "无");
            		} else {
                		obs_log(LOG_WARNING, "无法通过 cookies 获取 room_id 和 csrf_token，保留现有值");
            	}
        	}
        }

        if (!plugin->config.room_id) plugin->config.room_id = strdup("12345");
        if (!plugin->config.csrf_token) plugin->config.csrf_token = strdup("your_csrf_token");
        if (!plugin->config.title) plugin->config.title = strdup("我的直播");
		obs_log(LOG_INFO, "当前配置:");
		obs_log(LOG_INFO, "cookies: %s", plugin->config.cookies ? plugin->config.cookies : "无");
		obs_log(LOG_INFO, "csrf_token: %s", plugin->config.csrf_token ? plugin->config.csrf_token : "无");
		obs_log(LOG_INFO, "room_id: %s", plugin->config.room_id ? plugin->config.room_id : "无");
		obs_log(LOG_INFO, "title: %s", plugin->config.title ? plugin->config.title : "无");
		obs_log(LOG_INFO, "rtmp_addr: %s", plugin->config.rtmp_addr ? plugin->config.rtmp_addr : "无");
		obs_log(LOG_INFO, "rtmp_code: %s", plugin->config.rtmp_code ? plugin->config.rtmp_code : "无");
		obs_log(LOG_INFO, "part_id: %d", plugin->config.part_id);
		obs_log(LOG_INFO, "area_id: %d", plugin->config.area_id);
		bfree(config_file);
	}

    auto menuBar = main_window->menuBar();
    if (!menuBar) {
        obs_log(LOG_ERROR, "无法获取菜单栏");
        delete plugin;
        plugin = nullptr;
        return false;
    }
    auto bilibiliMenu = menuBar->addMenu("Bilibili直播");

    auto login = bilibiliMenu->addMenu("登录");
    QAction* manual = login->addAction("手动登录");
    QAction* scanQrcode = login->addAction("扫码登录");
    plugin->loginStatusAction = login->addAction("登录状态: 未登录");
    plugin->loginStatusAction->setCheckable(true);
	plugin->loginStatusAction->setEnabled(false);
    plugin->streamAction = bilibiliMenu->addAction("开始直播");
    QAction* updateRoomInfo = bilibiliMenu->addAction("更新直播间信息");

    QObject::connect(manual, &QAction::triggered, plugin, &BilibiliStreamPlugin::onManualTriggered);
    QObject::connect(scanQrcode, &QAction::triggered, plugin, &BilibiliStreamPlugin::onScanQrcodeTriggered);
    QObject::connect(plugin->loginStatusAction, &QAction::triggered, plugin, &BilibiliStreamPlugin::onLoginStatusTriggered);
    QObject::connect(plugin->streamAction, &QAction::triggered, plugin, &BilibiliStreamPlugin::onstreamButtonTriggered);
    QObject::connect(updateRoomInfo, &QAction::triggered, plugin, &BilibiliStreamPlugin::onUpdateRoomInfoTriggered);

    plugin->onLoginStatusTriggered();
    if (plugin->config.streaming) {
        plugin->streamAction->setText("停止直播");
    } else {
        plugin->streamAction->setText("开始直播");
    }

    obs_log(LOG_INFO, "插件加载成功，菜单已添加");
    return true;
}

void obs_module_unload(void) {
	bili_api_cleanup();
    if (plugin) {
		char* config_file = nullptr;
		config_file = obs_module_file("config.json");
		if (config_file) {
			obs_data_t* settings = obs_data_create();
            obs_data_set_string(settings, "room_id", plugin->config.room_id ? plugin->config.room_id : "");
            obs_data_set_string(settings, "csrf_token", plugin->config.csrf_token ? plugin->config.csrf_token : "");
            obs_data_set_string(settings, "cookies", plugin->config.cookies ? plugin->config.cookies : "");
            obs_data_set_string(settings, "title", plugin->config.title ? plugin->config.title : "");
            obs_data_set_string(settings, "rtmp_addr", plugin->config.rtmp_addr ? plugin->config.rtmp_addr : "");
            obs_data_set_string(settings, "rtmp_code", plugin->config.rtmp_code ? plugin->config.rtmp_code : "");
            obs_data_set_int(settings, "part_id", plugin->config.part_id ? plugin->config.part_id : 2);
            obs_data_set_int(settings, "area_id", plugin->config.area_id ? plugin->config.area_id : 86);
            obs_data_save_json(settings, config_file);
            obs_data_release(settings);
            obs_log(LOG_INFO, "配置已保存到 OBS 数据库");
			bfree(config_file);
		}
        plugin = nullptr;
    }
    obs_log(LOG_INFO, "插件已卸载");
}

#include "plugin-main.moc"
