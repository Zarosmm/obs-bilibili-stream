#include "plugin_utils.hpp"
#include <obs-module.h>
#include <cstdarg>
#include <string>

const char* PLUGIN_NAME = "@CMAKE_PROJECT_NAME@";
const char* PLUGIN_VERSION = "@CMAKE_PROJECT_VERSION@";

void obs_log(int log_level, const char* format, ...) {
	std::string template_str = std::string("[") + PLUGIN_NAME + "] " + format;
	va_list args;
	va_start(args, format);
	blogva(log_level, template_str.c_str(), args);
	va_end(args);
}
