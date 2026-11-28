#include "core/config_manager.hpp"
#include <obs-module.h>
#include <filesystem>
#include <QSaveFile>
#include <cstring>

namespace Core {
ConfigManager::ConfigManager() {}

char *ConfigManager::getConfigPath()
{
	return obs_module_config_path("config.json");
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

	m_config.cookies = obs_data_get_string(settings, "cookies");
	m_config.title = obs_data_get_string(settings, "title");
	obs_data_set_default_int(settings, "part_id", m_config.part_id);
	obs_data_set_default_int(settings, "area_id", m_config.area_id);
	m_config.part_id = static_cast<int>(obs_data_get_int(settings, "part_id"));
	m_config.area_id = static_cast<int>(obs_data_get_int(settings, "area_id"));
	const bool hasLegacyFields =
		obs_data_has_user_value(settings, "rtmp_addr") || obs_data_has_user_value(settings, "rtmp_code") ||
		obs_data_has_user_value(settings, "csrf_token") || obs_data_has_user_value(settings, "room_id") ||
		obs_data_has_user_value(settings, "mid");
	obs_data_release(settings);
	bfree(configFile);

	if (hasLegacyFields)
		save();
}

void ConfigManager::save()
{
	char *configFile = getConfigPath();
	if (!configFile)
		return;

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "cookies", m_config.cookies.c_str());
	obs_data_set_string(settings, "title", m_config.title.c_str());
	obs_data_set_int(settings, "part_id", m_config.part_id);
	obs_data_set_int(settings, "area_id", m_config.area_id);
	bool saved = false;
	try {
		std::error_code error;
		std::filesystem::create_directories(std::filesystem::u8path(configFile).parent_path(), error);
		if (!error) {
			QSaveFile file(QString::fromUtf8(configFile));
			const char *json = obs_data_get_json(settings);
			if (json && file.open(QIODevice::WriteOnly)) {
				const auto size = static_cast<qint64>(std::strlen(json));
				saved = file.write(json, size) == size && file.commit();
			}
		}
	} catch (const std::exception &) {
		// Avoid printing local paths and other system details to the OBS log.
	}
	if (!saved)
		blog(LOG_WARNING, "[obs-bilibili-stream] 配置保存失败");
	obs_data_release(settings);
	bfree(configFile);
}
} // namespace Core
