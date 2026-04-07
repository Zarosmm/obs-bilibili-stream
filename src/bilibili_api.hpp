#pragma once
#include <string>
#include <vector>
#include "json11/json11.hpp"

namespace Bili {
namespace {
const std::string APP_KEY = "aae92bc66f3edfab";
const std::string APP_SECRET = "af125a0d5279fd576c1b4418a3e8276d";
}

struct Config {
	std::string room_id;
	std::string csrf_token;
	std::string cookies;
	std::string mid;
	std::string title;
	bool login_status = false;
	bool streaming = false;
	std::string rtmp_addr;
	std::string rtmp_code;
	int part_id = 2;
	int area_id = 86;
};

class BiliApi {
public:
	static void init();
	static void cleanup();
	static bool getQrCode(const std::string &cookies, std::string &qr_data, std::string &qr_key,
			      std::string &message);
	static bool qrLogin(std::string &qr_key, std::string &cookies, std::string &message);
	static bool checkLoginStatus(const std::string &cookies, std::string &message, std::string &mid);
	static bool getRoomIdAndCsrf(const std::string &cookies, std::string &room_id, std::string &csrf_token,
				     std::string &message);
	static json11::Json getPartitionList(std::string &message);
	static bool startLive(Config &config, std::string &rtmp_addr, std::string &rtmp_code, std::string &message,
			      std::string &face_qr, std::string &mid);
	static bool stopLive(const Config &config, std::string &message);
	static bool updateRoomInfo(const Config &config, const std::string &title, std::string &message);

private:
	static std::vector<std::string> buildHeaders(const std::string &cookies);
	static std::string appsign(const std::vector<std::pair<std::string, std::string>> &params,
				   const std::string &app_key, const std::string &app_sec);
};
} // namespace Bili
