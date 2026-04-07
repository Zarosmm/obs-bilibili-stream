#pragma once
#include "bilibili_api.hpp"

namespace Core {
class ConfigManager {
public:
	ConfigManager();
	void load();
	void save();
	Bili::Config &config() { return m_config; }
	const Bili::Config &config() const { return m_config; }

private:
	Bili::Config m_config;
	char *getConfigPath();
};
} // namespace Core
