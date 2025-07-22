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

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static void on_menu_action_triggered(void *data)
{
	obs_log(LOG_INFO, "菜单项被点击")
}

bool obs_module_load(void)
{
	// 获取 OBS 前端主窗口
	QMainWindow *main_window = (QMainWindow *)obs_frontend_get_main_window();

	// 创建 QAction（菜单项）
	QAction *menu_action = new QAction("BiliBili直播码", main_window);

	// 连接 QAction 的触发信号到回调函数
	QObject::connect(menu_action, &QAction::triggered, [](bool) {
	    on_menu_action_triggered(nullptr);
	});

	// 将 QAction 添加到 OBS 的“工具”菜单
	obs_frontend_add_tools_menu_qaction(menu_action);

	blog(LOG_INFO, "插件加载成功，菜单已添加");
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
