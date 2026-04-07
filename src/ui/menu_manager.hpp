#pragma once
#include <QMenuBar>
#include <QAction>
#include <QObject>

namespace UI {
class MenuManager : public QObject {
	Q_OBJECT
public:
	struct Actions {
		QAction *scanQrcode = nullptr;
		QAction *loginStatus = nullptr;
		QAction *streamToggle = nullptr;
		QAction *openRoom = nullptr;
		QAction *updateRoomInfo = nullptr;
	};

	explicit MenuManager(QMenuBar *menuBar, QObject *parent = nullptr);
	Actions &actions() { return m_actions; }

signals:
	void scanQrcodeClicked();
	void streamToggleClicked();
	void openRoomClicked();
	void updateRoomInfoClicked();

private:
	void setupMenu();
	Actions m_actions;
	QMenuBar *m_menuBar;
};
} // namespace UI
