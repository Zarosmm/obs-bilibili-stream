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

private:
    BiliConfig config;

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

        // 创建手动登录对话框
        QDialog* loginDialog = new QDialog((QWidget*)obs_frontend_get_main_window());
        loginDialog->setWindowTitle("手动登录 - 输入Cookie");
        QVBoxLayout* layout = new QVBoxLayout(loginDialog);

        // 添加提示标签
        QLabel* label = new QLabel("请输入Bilibili Cookie:", loginDialog);
        layout->addWidget(label);

        // 添加Cookie输入框
        QLineEdit* cookieInput = new QLineEdit(loginDialog);
        cookieInput->setPlaceholderText("在此输入Cookie (如: bili_jct=xxx; SESSDATA=yyy)");
        layout->addWidget(cookieInput);

        // 添加确认按钮
        QPushButton* confirmButton = new QPushButton("确认", loginDialog);
        layout->addWidget(confirmButton);

        // 连接确认按钮的点击事件
        QObject::connect(confirmButton, &QPushButton::clicked, [=]() {
            QString cookie = cookieInput->text().trimmed();
            if (cookie.isEmpty()) {
                obs_log(LOG_WARNING, "Cookie 输入为空，未保存");
                loginDialog->reject();
                return;
            }

            // 释放旧的 config.cookies 内存
            if (config.cookies) {
                free(config.cookies);
            }

            // 将 QString 转换为 char* 并存储到 config.cookies
            config.cookies = strdup(cookie.toUtf8().constData());
            obs_log(LOG_INFO, "Cookie 已保存: %s", config.cookies);

            loginDialog->accept();
        });

        // 对话框关闭时清理资源
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

        // 创建 QTimer 每秒检查登录状态
        QTimer* timer = new QTimer(qrDialog);
        QObject::connect(timer, &QTimer::timeout, [this, qrDialog, timer, &qrcode_key]() mutable {
            if (bili_qr_login(config.cookies, &qrcode_key)) {
                obs_log(LOG_INFO, "二维码登录成功，检查登录状态以获取 cookies");
                char* new_cookies = nullptr;
                if (bili_check_login_status(config.cookies, &new_cookies)) {
                    if (new_cookies) {
                        if (config.cookies) {
                            free(config.cookies);
                        }
                        config.cookies = new_cookies;
                        obs_log(LOG_INFO, "二维码登录后更新 cookies: %s", config.cookies);
                    }
                }
                timer->stop();
                qrDialog->accept();
            } else {
                obs_log(LOG_DEBUG, "二维码登录检查：尚未登录");
            }
        });
        timer->start(1000); // 每1000ms（1秒）检查一次

        // 对话框关闭时清理资源
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

    // 获取 OBS 主窗口
    auto main_window = (QMainWindow*)obs_frontend_get_main_window();
    if (!main_window) {
        obs_log(LOG_ERROR, "无法获取 OBS 主窗口");
        return false;
    }

    // 创建菜单
    auto menuBar = main_window->menuBar();
    auto bilibiliMenu = menuBar->addMenu("Bilibili直播");

    // 创建插件对象
    plugin = new BilibiliStreamPlugin(main_window);

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
        delete plugin;
        plugin = nullptr;
    }
    // Cleanup Bilibili API
    bili_api_cleanup();
    obs_log(LOG_INFO, "插件已卸载");
}

#include "plugin-main.moc"
