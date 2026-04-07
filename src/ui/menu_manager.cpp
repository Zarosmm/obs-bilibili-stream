#include "ui/menu_manager.hpp"
#include <QMenu>

namespace UI {
MenuManager::MenuManager(QMenuBar *menuBar, QObject *parent)
	: QObject(parent), m_menuBar(menuBar)
{
	setupMenu();
}

void MenuManager::setupMenu()
{
	QMenu *bilibiliMenu = m_menuBar->addMenu("Bilibili直播");
	QMenu *loginMenu = bilibiliMenu->addMenu("登录");

	m_actions.scanQrcode = loginMenu->addAction("扫码登录");
	m_actions.loginStatus = loginMenu->addAction("登录状态: 未登录");
	m_actions.loginStatus->setCheckable(true);
	m_actions.loginStatus->setEnabled(false);

	m_actions.streamToggle = bilibiliMenu->addAction("开始直播");
	m_actions.openRoom = bilibiliMenu->addAction("打开直播间");
	m_actions.updateRoomInfo = bilibiliMenu->addAction("更新直播间信息");

	connect(m_actions.scanQrcode, &QAction::triggered, this, &MenuManager::scanQrcodeClicked);
	connect(m_actions.streamToggle, &QAction::triggered, this, &MenuManager::streamToggleClicked);
	connect(m_actions.openRoom, &QAction::triggered, this, &MenuManager::openRoomClicked);
	connect(m_actions.updateRoomInfo, &QAction::triggered, this, &MenuManager::updateRoomInfoClicked);
}
} // namespace UI
