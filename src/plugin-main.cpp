#include <obs-data.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QAction>
#include <QMenuBar>
#include <QMenu>
#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTimer>
#include <QClipboard>
#include <QApplication>
#include <QPainter>
#include <thread> // 引入线程
#include <functional> // 引入 std::bind, std::function

#include "qrcodegen/qrcodegen.hpp"
#include "bilibili_api.hpp"
#include "plugin_utils.hpp"

// 假设 Config 结构体在 bilibili_api.hpp 中定义，且包含 streaming 状态
// struct Config { ... bool streaming = false; ... }; 

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

class BilibiliStreamPlugin : public QObject {
    Q_OBJECT
public:
    explicit BilibiliStreamPlugin(QMainWindow* parent) : QObject(parent) {
        menuBar = parent->menuBar();
        setupMenu();
        loadConfig();
        // 初始时检查登录状态，使用异步
        std::thread([this]() {
            std::string message;
            if (!config.cookies.empty() && Bili::BiliApi::checkLoginStatus(config.cookies, message)) {
                config.login_status = true;
                std::string new_room_id, new_csrf_token;
                if (Bili::BiliApi::getRoomIdAndCsrf(config.cookies, new_room_id, new_csrf_token, message)) {
                    config.room_id = new_room_id;
                    config.csrf_token = new_csrf_token;
                }
            } else {
                obs_log(LOG_INFO, "初始登录检查失败: %s", message.c_str());
            }
            // 异步完成后，回主线程更新 UI
            QMetaObject::invokeMethod(this, "onLoginStatusTriggered", Qt::QueuedConnection);
            QMetaObject::invokeMethod(this, "updateStreamButtonText", Qt::QueuedConnection);
        }).detach();

    }

    ~BilibiliStreamPlugin() {
        obs_log(LOG_DEBUG, "释放 BilibiliStreamPlugin 资源");
    }

private:
    Bili::Config config;
    QMenuBar* menuBar;
    QAction* loginStatusAction = nullptr;
    QAction* streamAction = nullptr;
    
    // 异步回调方法，用于更新开始/停止直播按钮的文本
    Q_SLOT void updateStreamButtonText() {
        if (config.streaming) {
            streamAction->setText("停止直播");
        } else {
            streamAction->setText("开始直播");
        }
    }

    void setupMenu() {
        QMenu* bilibiliMenu = menuBar->addMenu("Bilibili直播");
        QMenu* loginMenu = bilibiliMenu->addMenu("登录");
        QAction* scanQrcode = loginMenu->addAction("扫码登录");
        loginStatusAction = loginMenu->addAction("登录状态: 未登录");
        loginStatusAction->setCheckable(true);
        loginStatusAction->setEnabled(false);
        streamAction = bilibiliMenu->addAction("开始直播");
        QAction* updateRoomInfo = bilibiliMenu->addAction("更新直播间信息");

        connect(scanQrcode, &QAction::triggered, this, &BilibiliStreamPlugin::onScanQrcodeTriggered);
        // connect(loginStatusAction, &QAction::triggered, this, &BilibiliStreamPlugin::onLoginStatusTriggered); // 登录状态不应手动触发
        connect(streamAction, &QAction::triggered, this, &BilibiliStreamPlugin::onStreamButtonTriggered);
        connect(updateRoomInfo, &QAction::triggered, this, &BilibiliStreamPlugin::onUpdateRoomInfoTriggered);
    }

    void loadConfig() {
	    std::string message;
        char* config_file = obs_module_file("config.json");
        if (config_file) {
            obs_data_t* settings = obs_data_create_from_json_file(config_file);
            if (settings) {
                config.room_id = obs_data_get_string(settings, "room_id");
                config.csrf_token = obs_data_get_string(settings, "csrf_token");
                config.cookies = obs_data_get_string(settings, "cookies");
                config.title = obs_data_get_string(settings, "title");
                config.rtmp_addr = obs_data_get_string(settings, "rtmp_addr");
                config.rtmp_code = obs_data_get_string(settings, "rtmp_code");
                config.part_id = obs_data_get_int(settings, "part_id");
                config.area_id = obs_data_get_int(settings, "area_id");
                config.streaming = obs_data_get_bool(settings, "streaming"); // 加载 streaming 状态
                obs_data_release(settings);
            }
            bfree(config_file);
        }
        if (config.room_id.empty()) config.room_id = "12345";
        if (config.csrf_token.empty()) config.csrf_token = "your_csrf_token";
        if (config.title.empty()) config.title = "我的直播";
    }

    void saveConfig() {
        char* config_file = obs_module_file("config.json");
        if (config_file) {
            obs_data_t* settings = obs_data_create();
            obs_data_set_string(settings, "room_id", config.room_id.c_str());
            obs_data_set_string(settings, "csrf_token", config.csrf_token.c_str());
            obs_data_set_string(settings, "cookies", config.cookies.c_str());
            obs_data_set_string(settings, "title", config.title.c_str());
            obs_data_set_string(settings, "rtmp_addr", config.rtmp_addr.c_str());
            obs_data_set_string(settings, "rtmp_code", config.rtmp_code.c_str());
            obs_data_set_int(settings, "part_id", config.part_id);
            obs_data_set_int(settings, "area_id", config.area_id);
            obs_data_set_bool(settings, "streaming", config.streaming); // 保存 streaming 状态
            obs_data_save_json(settings, config_file);
            obs_data_release(settings);
            bfree(config_file);
        }
    }

    static QPixmap generateQrCode(const std::string& qr_data) {
        // ... (二维码生成代码不变)
        if (qr_data.empty()) {
            obs_log(LOG_ERROR, "二维码数据为空");
            return QPixmap();
        }
        using qrcodegen::QrCode;
        const QrCode qr = QrCode::encodeText(qr_data.c_str(), QrCode::Ecc::LOW);
        if (qr.getSize() <= 0) {
            obs_log(LOG_ERROR, "生成二维码失败");
            return QPixmap();
        }
        const int scale = 5;
        const int size = qr.getSize();
        QImage image(size * scale, size * scale, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                if (qr.getModule(x, y)) {
                    painter.drawRect(x * scale, y * scale, scale, scale);
                }
            }
        }
        return QPixmap::fromImage(image);
    }

public slots:
    void onScanQrcodeTriggered() {
        std::string qr_data, qr_key, message;
        if (!Bili::BiliApi::getQrCode(config.cookies, qr_data, qr_key, message)) {
		    showMessageDialog(QString::fromUtf8(message), "消息");
            return;
        }

        QDialog* qrDialog = new QDialog((QWidget*)obs_frontend_get_main_window());
        qrDialog->setWindowTitle("Bilibili 登录二维码");
        QVBoxLayout* layout = new QVBoxLayout(qrDialog);
        QLabel* qrLabel = new QLabel(qrDialog);
        QPixmap qrPixmap = generateQrCode(qr_data);
        if (qrPixmap.isNull()) {
            qrLabel->setText("无法生成二维码");
        } else {
            qrLabel->setPixmap(qrPixmap);
        }
        layout->addWidget(qrLabel);
        layout->addWidget(new QLabel("使用手机扫描二维码登录", qrDialog));
        qrDialog->setLayout(layout);

        // 使用 QTimer 进行轮询检查，但网络请求放在线程中
        QTimer* timer = new QTimer(qrDialog);
        
        // Lambda 捕获 qrDialog 指针，并使用 std::function 存储异步任务
        std::function<void()> pollTask = [=]() mutable {
            std::string cookies_result, message_result;
            // 网络请求必须在非主线程进行
            bool success = Bili::BiliApi::qrLogin(qr_key, cookies_result, message_result);
            
            // 异步完成后，回主线程更新 UI
            QMetaObject::invokeMethod(this, [=]() mutable {
                if (success) {
                    timer->stop();
                    config.cookies = cookies_result;
                    // 再次异步检查，确保获取 room_id 和 csrf_token
                    std::thread([this, cookies_result, qrDialog]() {
                        std::string message_check;
                        config.login_status = Bili::BiliApi::checkLoginStatus(cookies_result, message_check);
                        std::string new_room_id, new_csrf_token;
                        if (config.login_status && Bili::BiliApi::getRoomIdAndCsrf(config.cookies, new_room_id, new_csrf_token, message_check)) {
                            config.room_id = new_room_id;
                            config.csrf_token = new_csrf_token;
                        } else {
                            // 登录失败，可能被登出或数据获取失败
                            config.login_status = false;
                        }
                        
                        QMetaObject::invokeMethod(this, [this, qrDialog]() {
                            saveConfig();
                            onLoginStatusTriggered();
                            qrDialog->accept();
                        }, Qt::QueuedConnection);
                        
                    }).detach();
                } else if (message_result.find("二维码已扫描") == std::string::npos) {
                    // 仅在失效或API错误时停止轮询并弹框
                    timer->stop();
                    // 显示失败消息
                    showMessageDialog(QString::fromUtf8(message_result), "消息");
                    qrDialog->reject(); 
                }
                // 如果是"等待确认"，则不弹框，继续轮询
            }, Qt::QueuedConnection);
        };

        connect(timer, &QTimer::timeout, [pollTask]() { 
            // 在定时器触发时，启动一个新的线程来执行 pollTask
            std::thread(pollTask).detach();
        });
        timer->start(1000); // 每秒检查一次
        connect(qrDialog, &QDialog::finished, [=]() { 
            timer->stop(); // 窗口关闭时停止计时器
            qrDialog->deleteLater(); 
        });
        qrDialog->exec();
    }

    Q_SLOT void onLoginStatusTriggered() {
        loginStatusAction->setText(config.login_status ? "登录状态: 已登录" : "登录状态: 未登录");
        loginStatusAction->setChecked(config.login_status);
    }

    void onStreamButtonTriggered() {
	    std::string message;
        
        streamAction->setEnabled(false); // 禁用按钮防止重复点击

        auto streamTask = [this]() {
            std::string message_result;
            if (config.streaming) {
                // 停止直播逻辑 (在线程中)
                bool success = Bili::BiliApi::stopLive(config, message_result);
                QMetaObject::invokeMethod(this, [this, success, message_result]() {
                    streamAction->setEnabled(true);
                    if (success) {
                        config.streaming = false;
                        saveConfig();
                        updateStreamButtonText();
                        showMessageDialog("直播已停止", "消息");
                    } else {
                        showMessageDialog(QStringLiteral("停止直播失败\n") + QString::fromUtf8(message_result.c_str()), "消息");
                    }
                }, Qt::QueuedConnection);
            } else if (!config.area_id) {
                 QMetaObject::invokeMethod(this, [this]() {
                     streamAction->setEnabled(true);
                     showMessageDialog("请更新直播间分区", "消息");
                 }, Qt::QueuedConnection);
            } else {
                // 开始直播逻辑 (在线程中)
                std::string rtmp_addr, rtmp_code;
                bool success = Bili::BiliApi::startLive(config, rtmp_addr, rtmp_code, message_result);
                
                QMetaObject::invokeMethod(this, [this, success, rtmp_addr, rtmp_code, message_result]() {
                    streamAction->setEnabled(true);
                    if (success) {
                        config.streaming = true;
                        config.rtmp_addr = rtmp_addr;
                        config.rtmp_code = rtmp_code;
                        saveConfig();
                        updateStreamButtonText();

                        // 成功弹窗 (在主线程)
                        QDialog* dialog = new QDialog((QWidget*)obs_frontend_get_main_window());
                        dialog->setWindowTitle("消息");
                        QVBoxLayout* layout = new QVBoxLayout(dialog);
                        layout->addWidget(new QLabel(QString("Bilibili已开始直播请复制以下内容进行推流\n"
                                                             "若自定义推流失败请使用预设的B站推流更换推流地址尝试\n"
                                                             "RTMP 地址: %1\n推流码: %2").arg(rtmp_addr.c_str(), rtmp_code.c_str()), dialog));
                        QPushButton* copy = new QPushButton("复制", dialog);
                        QPushButton* confirm = new QPushButton("确认", dialog);
                        layout->addWidget(copy);
                        layout->addWidget(confirm);
                        connect(copy, &QPushButton::clicked, [=]() {
                            QApplication::clipboard()->setText(QString("推流地址: %1\n推流码: %2").arg(rtmp_addr.c_str(), rtmp_code.c_str()));
                        });
                        connect(confirm, &QPushButton::clicked, [=]() { dialog->accept(); });
                        connect(dialog, &QDialog::finished, [=]() { dialog->deleteLater(); });
                        dialog->exec();
                    } else {
                        showMessageDialog(QStringLiteral("启动直播失败\n") + QString::fromUtf8(message_result.c_str()), "消息");
                    }
                }, Qt::QueuedConnection);
            }
        };
        
        // 启动线程执行任务
        std::thread(streamTask).detach();
    }

    void onUpdateRoomInfoTriggered() {
	    std::string message;
        QDialog* dialog = new QDialog((QWidget*)obs_frontend_get_main_window());
        dialog->setWindowTitle("更新直播间信息");
        QVBoxLayout* layout = new QVBoxLayout(dialog);

        QHBoxLayout* titleLayout = new QHBoxLayout();
        QLineEdit* titleInput = new QLineEdit(config.title.c_str(), dialog);
        QPushButton* confirmTitle = new QPushButton("确认", dialog);
        titleLayout->addWidget(new QLabel("直播间标题:", dialog));
        titleLayout->addWidget(titleInput);
        titleLayout->addWidget(confirmTitle);
        layout->addLayout(titleLayout);

        QHBoxLayout* partitionLayout = new QHBoxLayout();
        QComboBox* partCombo = new QComboBox(dialog);
        QComboBox* areaCombo = new QComboBox(dialog);
        QPushButton* confirmPartition = new QPushButton("确认", dialog);
        partitionLayout->addWidget(new QLabel("直播间分区:", dialog));
        partitionLayout->addWidget(partCombo);
        partitionLayout->addWidget(areaCombo);
        partitionLayout->addWidget(confirmPartition);
        layout->addLayout(partitionLayout);

        // 异步获取分区列表
        confirmTitle->setEnabled(false);
        confirmPartition->setEnabled(false);
        std::thread([this, dialog, partCombo, areaCombo, confirmTitle, confirmPartition]() {
            std::string message_result;
            struct Part { int id; QString name; std::vector<json11::Json> list; };
            std::vector<Part> parts;
            auto partition_data = Bili::BiliApi::getPartitionList(message_result);
            if (partition_data.is_array()) {
                for (const auto& item : partition_data.array_items()) {
                    parts.push_back({item["id"].int_value(), QString::fromUtf8(item["name"].string_value().c_str()), item["list"].array_items()});
                }
            }
            
            // 回到主线程更新 UI
            QMetaObject::invokeMethod(this, [=]() {
                confirmTitle->setEnabled(true);
                confirmPartition->setEnabled(true);

                if (parts.empty()) {
                    showMessageDialog(QStringLiteral("获取分区列表失败\n") + QString::fromUtf8(message_result), "消息");
                    return;
                }

                size_t selected_part_index = 0;
                for (size_t i = 0; i < parts.size(); ++i) {
                    partCombo->addItem(parts[i].name, parts[i].id);
                    if (parts[i].id == config.part_id) selected_part_index = i;
                }
                partCombo->setCurrentIndex(static_cast<int>(selected_part_index));

                auto updateAreaCombo = [=](int part_index) {
                    areaCombo->clear();
                    if (part_index >= 0 && static_cast<size_t>(part_index) < parts.size() && !parts[part_index].list.empty()) {
                        size_t selected_area_index = 0;
                        for (size_t i = 0; i < parts[part_index].list.size(); ++i) {
                            int id = std::stoi(parts[part_index].list[i]["id"].string_value());
                            QString name = QString::fromUtf8(parts[part_index].list[i]["name"].string_value().c_str());
                            areaCombo->addItem(name, id);
                            if (id == config.area_id) selected_area_index = i;
                        }
                        areaCombo->setCurrentIndex(static_cast<int>(selected_area_index));
                    } else {
                        // 默认值，以防列表为空
                        areaCombo->addItem("英雄联盟", 86);
                        if (config.area_id == 86) areaCombo->setCurrentIndex(0);
                    }
                };
                updateAreaCombo(static_cast<int>(selected_part_index));
                
                // 连接信号
                connect(partCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) { updateAreaCombo(index); });

            }, Qt::QueuedConnection);
        }).detach();

        // 标题更新（异步）
        connect(confirmTitle, &QPushButton::clicked, [=]() {
            confirmTitle->setEnabled(false);
            std::string new_title = titleInput->text().trimmed().toUtf8().constData();
		    std::thread([this, new_title, confirmTitle]() {
		        std::string update_message;
		        bool success = !new_title.empty() && Bili::BiliApi::updateRoomInfo(config, new_title, update_message);
		        
		        QMetaObject::invokeMethod(this, [this, success, new_title, update_message, confirmTitle]() {
		            confirmTitle->setEnabled(true);
		            if (success) {
		                config.title = new_title;
		                saveConfig();
		                showMessageDialog("直播间标题已更新", "消息");
		            } else {
		                showMessageDialog(QStringLiteral("直播间标题更新失败\n") + QString::fromUtf8(update_message.c_str()), "消息");
		            }
		        }, Qt::QueuedConnection);
		    }).detach();
        });

        // 分区更新（同步，因为只是本地保存配置）
        connect(confirmPartition, &QPushButton::clicked, [=]() {
            config.part_id = partCombo->currentData().toInt();
            config.area_id = areaCombo->currentData().toInt();
            saveConfig();
            showMessageDialog("直播间分区已更新", "消息");
        });

        connect(dialog, &QDialog::finished, [=]() { dialog->deleteLater(); });
        dialog->exec();
    }

    void showMessageDialog(const QString& message, const QString& title) {
        QDialog* dialog = new QDialog((QWidget*)obs_frontend_get_main_window());
        dialog->setWindowTitle(title);
        QVBoxLayout* layout = new QVBoxLayout(dialog);
        layout->addWidget(new QLabel(message, dialog));
        QPushButton* confirm = new QPushButton("确认", dialog);
        layout->addWidget(confirm);
        connect(confirm, &QPushButton::clicked, [=]() { dialog->accept(); });
        connect(dialog, &QDialog::finished, [=]() { dialog->deleteLater(); });
        dialog->exec();
    }
};

static BilibiliStreamPlugin* plugin = nullptr;

bool obs_module_load(void) {
    Bili::BiliApi::init();
    auto main_window = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (!main_window) return false;
    plugin = new BilibiliStreamPlugin(main_window);
    obs_log(LOG_INFO, "Bilibili Stream Plugin loaded successfully.");
    return true;
}

void obs_module_unload(void) {
    Bili::BiliApi::cleanup();
    delete plugin;
    plugin = nullptr;
    obs_log(LOG_INFO, "Bilibili Stream Plugin unloaded.");
}

#include "plugin-main.moc" // 确保这行在你修改的文件中存在
