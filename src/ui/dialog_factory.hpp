#pragma once
#include <QWidget>
#include <QString>
#include <string>
#include <functional>
#include "json11/json11.hpp"

namespace UI {
class DialogFactory {
public:
	static void message(const QString &msg, const QString &title, QWidget *parent = nullptr);
	static QDialog *qrLogin(QWidget *parent, const std::string &qrData, const std::string &qrKey,
				std::function<void(const std::string &cookies)> onSuccess);
	static QDialog *streamStarted(QWidget *parent, const std::string &rtmpAddr, const std::string &rtmpCode);
	static QDialog *faceAuth(QWidget *parent, const std::string &faceUrl);
	static QDialog *roomSettings(QWidget *parent, const std::string &roomUrl, const std::string &currentTitle,
				     int currentAreaId, int currentPartId, const json11::Json &partitionData,
				     std::function<void(const std::string &title)> onTitleApply,
				     std::function<void(int areaId, int partId)> onPartitionApply);
};
} // namespace UI
