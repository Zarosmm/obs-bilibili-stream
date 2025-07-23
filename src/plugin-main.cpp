/*
Plugin Name
Copyright (C) 2025 <Developer> <Email Address>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

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
#include <windows.h>
#include <QZXing.h>
#include "bili_api.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("bilibili-stream-for-obs", "zh-CN")

class BilibiliStreamPlugin : public QObject {
    Q_OBJECT
public:
    explicit BilibiliStreamPlugin(QObject* parent = nullptr) : QObject(parent) {
        // 初始化 Bilibili 配置（示例值，需替换为实际配置）
        config.room_id = "12345";
        config.csrf_token = "your_csrf_token";
        config.cookies = "your_cookies";
        config.title = "我的直播";
    }

private:
    BiliConfig config;

    // 使用 QZXing 生成二维码图像
    QPixmap generateQrCodePixmap(const char* qrcode_data) {
        if (!qrcode_data) {
            obs_log(LOG_ERROR, "二维码数据为空，无法生成二维码");
            return QPixmap();
        }

        // 使用 QZXing 生成二维码
        QZXing encoder;
        encoder.setEncoder(QZXing::EncoderFormat_QR_CODE);
        QImage qrImage = encoder.encodeData(QString(qrcode_data));
        if (qrImage.isNull()) {
            obs_log(LOG_ERROR, "QZXing 生成二维码失败");
            return QPixmap();
        }

        return QPixmap::fromImage(qrImage);
    }

public slots:
    void onScanQrcodeTriggered() {
        obs_log(LOG_INFO, "扫码登录菜单项被点击");
        char* qrcode_data = nullptr;
        char* qrcode_key = nullptr;
        if (bili_get_qrcode(&qrcode_data, &qrcode_key)) {
            obs_log(LOG_INFO, "二维码数据: %s, 二维码密钥: %s",
                    qrcode_data ? qrcode_data : "无数据",
                    qrcode_key ? qrcode_key : "无数据");

            // 创建对话框显示二维码
            QDialog* qrDialog = new QDialog((QWidget*)obs_frontend_get_main_window());
            qrDialog->setWindowTitle("Bilibili 登录二维码");
            QVBoxLayout* layout = new QVBoxLayout(qrDialog);
            QLabel* qrLabel = new QLabel(qrDialog);
            qrLabel->setPixmap(generateQrCodePixmap(qrcode_data));
            layout->addWidget(qrLabel);
            QLabel* infoLabel = new QLabel("请使用Bilibili手机客户端扫描二维码登录", qrDialog);
            layout->addWidget(infoLabel);
            qrDialog->setLayout(layout);
            qrDialog->exec();

            // TODO: 使用 qrcode_key 检查登录状态
        }
        free(qrcode_data);
        free(qrcode_key);
    }

    void onLoginStatusTriggered() {
        obs_log(LOG_INFO, "登录状态菜单项被点击");
        char* status_data = NULL;
        if (bili_check_login_status(&status_data)) {
            obs_log(LOG_INFO, "登录状态数据: %s", status_data ? status_data : "无数据");
        }
        free(status_data);
    }

    void onPushStreamTriggered() {
        obs_log(LOG_INFO, "开始直播菜单项被点击");
        char* rtmp_addr = NULL;
        char* rtmp_code = NULL;
        if (bili_start_live(&config, 624, &rtmp_addr, &rtmp_code)) {
            obs_log(LOG_INFO, "直播已启动，RTMP 地址: %s, 推流码: %s", rtmp_addr, rtmp_code);
            // TODO: 将 rtmp_addr 和 rtmp_code 设置到 OBS 输出设置中
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
    QAction* scanQrcode = bilibiliMenu->addAction("扫码登录");
    QAction* loginStatus = bilibiliMenu->addAction("登录状态");
    loginStatus->setCheckable(true);
    QAction* pushStream = bilibiliMenu->addAction("开始直播");
    QAction* stopStream = bilibiliMenu->addAction("停止直播");
    QAction* updateRoomInfo = bilibiliMenu->addAction("更新直播间信息");

    // 连接信号与槽
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
