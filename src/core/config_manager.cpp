#include "core/config_manager.hpp"
#include <obs-module.h>

namespace Core {
ConfigManager::ConfigManager() {}

char *ConfigManager::getConfigPath()
{
	return obs_module_file("config.json");
}

void ConfigManager::load()
{
	m_config = Bili::Config();

	char *configFile = getConfigPath();
	if (!configFile)
		return;

	obs_data_t *settings = obs_data_create_from_json_file(configFile);
	if (!settings) {
		bfree(configFile);
		return;
	}

	m_config.room_id = obs_data_get_string(settings, "room_id");
	m_config.csrf_token = obs_data_get_string(settings, "csrf_token");
	m_config.mid = obs_data_get_string(settings, "mid");
	m_config.cookies = obs_data_get_string(settings, "cookies");
	m_config.title = obs_data_get_string(settings, "title");
	m_config.rtmp_addr = obs_data_get_string(settings, "rtmp_addr");
	m_config.rtmp_code = obs_data_get_string(settings, "rtmp_code");
	m_config.part_id = static_cast<int>(obs_data_get_int(settings, "part_id"));
	m_config.area_id = static_cast<int>(obs_data_get_int(settings, "area_id"));
	obs_data_release(settings);
	bfree(configFile);

	if (m_config.room_id.empty())
		m_config.room_id = "12345";
	if (m_config.csrf_token.empty())
		m_config.csrf_token = "your_csrf_token";
	if (m_config.title.empty())
		m_config.title = "我的直播";
}

void ConfigManager::save()
{
	char *configFile = getConfigPath();
	if (!configFile)
		return;

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "room_id", m_config.room_id.c_str());
	obs_data_set_string(settings, "csrf_token", m_config.csrf_token.c_str());
	obs_data_set_string(settings, "cookies", m_config.cookies.c_str());
	obs_data_set_string(settings, "mid", m_config.mid.c_str());
	obs_data_set_string(settings, "title", m_config.title.c_str());
	obs_data_set_string(settings, "rtmp_addr", m_config.rtmp_addr.c_str());
	obs_data_set_string(settings, "rtmp_code", m_config.rtmp_code.c_str());
	obs_data_set_int(settings, "part_id", m_config.part_id);
	obs_data_set_int(settings, "area_id", m_config.area_id);
	obs_data_save_json(settings, configFile);
	obs_data_release(settings);
	bfree(configFile);
}
} // namespace Core
