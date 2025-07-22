/*
Plugin Name
Copyright (C) <Year> <Developer> <Email Address>

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

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

class BilibiliStreamPlugin : public QObject {
    Q_OBJECT
public:
    explicit BilibiliStreamPlugin(QObject* parent = nullptr) : QObject(parent) {}

public slots:
    void onScanQrcodeTriggered() {
        obs_log(LOG_INFO, "扫码登录菜单项被点击");
    }

    void onLoginStatusTriggered() {
        obs_log(LOG_INFO, "登录状态菜单项被点击");
    }

    void onPushStreamTriggered() {
        obs_log(LOG_INFO, "开始直播菜单项被点击");
    }
};

static BilibiliStreamPlugin* plugin = nullptr;

bool obs_module_load(void)
{
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

    // 连接信号与槽
    QObject::connect(scanQrcode, &QAction::triggered, plugin, &BilibiliStreamPlugin::onScanQrcodeTriggered);
    QObject::connect(loginStatus, &QAction::triggered, plugin, &BilibiliStreamPlugin::onLoginStatusTriggered);
    QObject::connect(pushStream, &QAction::triggered, plugin, &BilibiliStreamPlugin::onPushStreamTriggered);

    obs_log(LOG_INFO, "插件加载成功，菜单已添加");
    return true;
}

void obs_module_unload(void)
{
    if (plugin) {
        delete plugin;
        plugin = nullptr;
    }
    obs_log(LOG_INFO, "插件已卸载");
}