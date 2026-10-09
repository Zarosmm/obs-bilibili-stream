#include "plugin_utils.hpp"
#include <obs-module.h>
#include <cstdarg>
#include <string>

const char *PLUGIN_NAME = "obs-bilibili-stream";
const char *PLUGIN_VERSION = PLUGIN_VERSION_STR;
const char *PLUGIN_COMMIT = GIT_COMMIT_HASH;

void obs_log(int log_level, const char *format, ...)
{
	std::string template_str = std::string("[") + PLUGIN_NAME + "] " + format;
	va_list args;
	va_start(args, format);
	blogva(log_level, template_str.c_str(), args);
	va_end(args);
}
