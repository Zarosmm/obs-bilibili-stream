#pragma once
#include <QWidget>
#include <QString>
#include <string>
#include <functional>

namespace UI {
class DialogFactory {
public:
	static void message(const QString &msg, const QString &title, QWidget *parent = nullptr);
	static QDialog *qrLogin(QWidget *parent, const std::string &qrData, std::string &qrKey,
				 std::function<void(const std::string &cookies)> onSuccess);
	static QDialog *streamStarted(QWidget *parent, const std::string &rtmpAddr, const std::string &rtmpCode);
	static QDialog *faceAuth(QWidget *parent, const std::string &faceUrl);
	static QDialog *roomSettings(QWidget *parent, const std::string &roomUrl, const std::string &currentTitle,
				    int currentAreaId, int currentPartId,
				    std::function<void(const std::string &title, int areaId, int partId)> onApply);
};
} // namespace UI
