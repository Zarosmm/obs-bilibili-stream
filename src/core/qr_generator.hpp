#pragma once
#include <QPixmap>
#include <QString>
#include <string>

namespace Core {
class QrGenerator {
public:
	static QPixmap generate(const std::string &data);
	static QPixmap generate(const QString &data);
};
} // namespace Core
