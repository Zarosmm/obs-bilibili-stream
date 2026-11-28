#pragma once
#include <string>
#include <vector>
#include "json11/json11.hpp"

namespace Bili {
namespace {
const std::string APP_KEY = "aae92bc66f3edfab";
const std::string APP_SECRET = "af125a0d5279fd576c1b4418a3e8276d";
} // namespace

struct Config {
	std::string room_id;
	std::string csrf_token;
	std::string cookies;
	std::string mid;
	std::string title;
	bool login_status = false;
	bool streaming = false;
	int part_id = 2;
	int area_id = 86;
};

enum class QrLoginStatus { Waiting, Scanned, Expired, Success, Error };

class BiliApi {
public:
	static void init();
	static void cleanup();
	static bool getQrCode(std::string &qr_data, std::string &qr_key, std::string &message);
	static QrLoginStatus qrLogin(const std::string &qr_key, std::string &cookies, std::string &message);
	static bool checkLoginStatus(const std::string &cookies, std::string &message, std::string &mid);
	static bool getRoomIdAndCsrf(const std::string &cookies, std::string &room_id, std::string &csrf_token,
				     std::string &message);
	static json11::Json getPartitionList(std::string &message);
	static bool startLive(const Config &config, std::string &rtmp_addr, std::string &rtmp_code,
			      std::string &message, std::string &face_qr);
	static bool stopLive(const Config &config, std::string &message);
	static bool updateRoomInfo(const Config &config, std::string &message, const std::string &title = "",
				   int areaId = -1);

private:
	static std::vector<std::string> buildHeaders(const std::string &cookies);
	static std::string appsign(const std::vector<std::pair<std::string, std::string>> &params,
				   const std::string &app_key, const std::string &app_sec);
};
} // namespace Bili
