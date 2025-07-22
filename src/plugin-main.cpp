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
#include <QDockWidget>
#include <QMenuBar>
#include <QMenu>
#include <QVBoxLayout>
#include <QLabel>
#include <QThread>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")


static void on_menu_action_triggered(void *data)
{
	obs_log(LOG_INFO, "菜单项被点击");
}

bool obs_module_load(void)
{
	// 获取 OBS 主窗口
	auto main_window = (QMainWindow *)obs_frontend_get_main_window();
	if (main_window == nullptr)
		return false;


        // 创建菜单栏
        auto menuBar = main_window->menuBar();

	auto bilibiliStream = menuBar->addMenu("Bilibili Stream");

        // 添加菜单
        // 菜单 1: 扫码

	auto scanQrcode = bilibiliStream1->addMenu("扫码登录");


        // 菜单 2: 登录状态
        auto loginStatus1 = bilibiliStream1->addMenu("未登录");

        // 菜单 3: 帮助
        auto pushStream = bilibiliStream1->addMenu("开始直播");

        // 连接菜单动作
        //QObject::connect(actionSave, &QAction::triggered, [contentWidget]() {
        //    contentWidget->SaveConfig();
        //    blog(LOG_INFO, "保存配置");
        //});
        //QObject::connect(actionLoad, &QAction::triggered, [contentWidget]() {
        //    contentWidget->LoadConfig();
        //    blog(LOG_INFO, "加载配置");
        //});
        //QObject::connect(actionStartAll, &QAction::triggered, [contentWidget]() {
        //    for (auto x : contentWidget->GetAllPushWidgets())
        //        x->StartStreaming();
        //    blog(LOG_INFO, "全部开始推流");
        //});
        //QObject::connect(actionStopAll, &QAction::triggered, [contentWidget]() {
        //    for (auto x : contentWidget->GetAllPushWidgets())
        //        x->StopStreaming();
        //    blog(LOG_INFO, "全部停止推流");
        //});
        //QObject::connect(actionAbout, &QAction::triggered, []() {
        //    blog(LOG_INFO, "关于菜单被点击");
        //    // 可以弹出关于对话框
        //});

	obs_log(LOG_INFO, "插件加载成功，菜单已添加");
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
