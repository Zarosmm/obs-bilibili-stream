#include "bilibili_api.hpp"
#include "http_client.hpp"
#include "md5.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
namespace Bili {
static const std::vector<std::string> default_headers = {
	"Accept: application/json, text/plain, */*",
	"Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
	"Content-Type: application/x-www-form-urlencoded; charset=UTF-8",
	"Origin: https://link.bilibili.com",
	"Referer: https://link.bilibili.com/p/center/index",
	"Sec-Ch-Ua: \"Microsoft Edge\";v=\"129\", \"Not=A?Brand\";v=\"8\", \"Chromium\";v=\"129\"",
	"Sec-Ch-Ua-Mobile: ?0",
	"Sec-Ch-Ua-Platform: \"Windows\"",
	"Sec-Fetch-Dest: empty",
	"Sec-Fetch-Mode: cors",
	"Sec-Fetch-Site: same-site",
	"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/129.0.0.0 Safari/537.36"};

std::vector<std::string> BiliApi::buildHeaders(const std::string &cookies)
{
	std::vector<std::string> headers = default_headers;
	if (!cookies.empty()) {
		headers.push_back("Cookie: " + cookies);
	}
	return headers;
}

std::string BiliApi::appsign(const std::vector<std::pair<std::string, std::string>> &params, const std::string &app_key,
			     const std::string &app_sec)
{
	std::vector<std::pair<std::string, std::string>> sorted_params = params;
	sorted_params.emplace_back("appkey", app_key);
	std::sort(sorted_params.begin(), sorted_params.end());

	std::ostringstream query;
	for (size_t i = 0; i < sorted_params.size(); ++i) {
		if (i > 0)
			query << "&";
		query << sorted_params[i].first << "=" << sorted_params[i].second;
	}
	std::string query_str = query.str() + app_sec;

	unsigned char digest[16];
	Crypto::MD5Context ctx;
	Crypto::md5Init(&ctx);
	Crypto::md5Update(&ctx, reinterpret_cast<const unsigned char *>(query_str.c_str()), query_str.length());
	Crypto::md5Final(digest, &ctx);

	char md5_hex[33];
	for (int i = 0; i < 16; ++i) {
		snprintf(&md5_hex[i * 2], 3, "%02x", digest[i]);
	}
	md5_hex[32] = '\0';
	sorted_params.emplace_back("sign", md5_hex);

	query.str("");
	for (size_t i = 0; i < sorted_params.size(); ++i) {
		if (i > 0)
			query << "&";
		query << sorted_params[i].first << "=" << sorted_params[i].second;
	}
	return query.str();
}

void BiliApi::init()
{
	Http::HttpClient::init();
}

void BiliApi::cleanup()
{
	Http::HttpClient::cleanup();
}

bool BiliApi::getQrCode(const std::string &cookies, std::string &qr_data, std::string &qr_key, std::string &message)
{
	auto headers = buildHeaders(cookies);
	auto response =
		Http::HttpClient::get("https://passport.bilibili.com/x/passport-login/web/qrcode/generate", headers);
	if (response.status != 200) {
		message = "获取二维码失败，状态码: " + std::to_string(response.status);
		if (!response.data.empty()) {
			message += ", 数据: " + response.data;
		}
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty()) {
		message = "Json 解析失败: " + err;
		return false;
	}

	qr_data = json["data"]["url"].string_value();
	qr_key = json["data"]["qrcode_key"].string_value();
	if (qr_data.empty() || qr_key.empty()) {
		message = "无法提取二维码数据或密钥";
		return false;
	}

	message = "获取二维码成功，URL: " + qr_data + ", Key: " + qr_key;
	return true;
}

bool BiliApi::qrLogin(std::string &qr_key, std::string &cookies, std::string &message)
{
	std::string url = "https://passport.bilibili.com/x/passport-login/web/qrcode/poll?qrcode_key=" + qr_key;
	auto response = Http::HttpClient::get(url, default_headers);
	if (response.status != 200) {
		message = "检查二维码登录状态失败，状态码: " + std::to_string(response.status);
		if (!response.data.empty()) {
			message += ", 数据: " + response.data;
		}
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty()) {
		// obs_log(LOG_ERROR, "JSON 解析失败: %s", err.c_str());
		message = "JSON 解析失败: " + err;
		return false;
	}

	int code = json["data"]["code"].int_value();
	if (code != 0) {
		if (code == 86038) {
			// obs_log(LOG_ERROR, "二维码已失效: %s", json["message"].string_value().c_str());
			message = "二维码已失效: " + json["message"].string_value();
		} else if (code == 86090) {
			// obs_log(LOG_INFO, "二维码已扫描，等待确认");
			message = "二维码已扫描，等待确认";
		} else {
			// obs_log(LOG_ERROR, "API 返回错误，code: %d, message: %s", code, json["message"].string_value().c_str());
			message = "API 返回错误，code: " + std::to_string(code) +
				  ", message: " + json["message"].string_value();
		}
		return false;
	}

	cookies = response.cookies;
	if (cookies.empty()) {
		// obs_log(LOG_ERROR, "无法获取登录 Cookies");
		message = "无法获取登录 Cookies";
		return false;
	}

	// obs_log(LOG_INFO, "二维码登录成功");
	message = "二维码登录成功";
	return true;
}

bool BiliApi::checkLoginStatus(const std::string &cookies, std::string &message)
{
	auto headers = buildHeaders(cookies);
	auto response = Http::HttpClient::get("https://api.bilibili.com/x/web-interface/nav", headers);
	if (response.status != 200) {
		message = "检查登录状态失败，状态码: " + std::to_string(response.status);
		if (!response.data.empty()) {
			message += ", 数据: " + response.data;
		}
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty()) {
		// obs_log(LOG_ERROR, "JSON 解析失败: %s", err.c_str());
		message = "JSON 解析失败: " + err;
		return false;
	}

	bool is_login = json["data"]["isLogin"].bool_value();
	//obs_log(LOG_INFO, "检查登录状态: %s", is_login ? "已登录" : "未登录");
	message = "检查登录状态: " + std::string(is_login ? "已登录" : "未登录");
	return is_login;
}

bool BiliApi::getRoomIdAndCsrf(const std::string &cookies, std::string &room_id, std::string &csrf_token,
			       std::string &message)
{
	if (cookies.empty()) {
		//obs_log(LOG_ERROR, "Cookies 为空");
		return false;
	}

	size_t pos = cookies.find("DedeUserID=");
	if (pos == std::string::npos) {
		//obs_log(LOG_ERROR, "无法从 Cookies 中提取 DedeUserID");
		return false;
	}
	std::string dede_user_id = cookies.substr(pos + 11, cookies.find(';', pos) - pos - 11);

	std::string url = "https://api.live.bilibili.com/room/v2/Room/room_id_by_uid?uid=" + dede_user_id;
	auto headers = buildHeaders(cookies);
	auto response = Http::HttpClient::get(url, headers);
	if (response.status != 200) {
		message = "获取房间号失败，状态码: " + std::to_string(response.status);
		if (!response.data.empty()) {
			message += ", 数据: " + response.data;
		}
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty()) {
		//obs_log(LOG_ERROR, "JSON 解析失败: %s", err.c_str());
		message = "Json 解析失败: " + err;
		return false;
	}
	if (json["code"].int_value() != 0) {
		//obs_log(LOG_ERROR, "API 返回错误，code: %d, message: %s",
		//json["code"].int_value(), json["message"].string_value().c_str());
		message = "API 返回错误， code: " + std::to_string(json["code"].int_value()) +
			  ", message: " + json["message"].string_value();
		return false;
	}

	room_id = std::to_string(json["data"]["room_id"].int_value());
	pos = cookies.find("bili_jct=");
	if (pos == std::string::npos) {
		//obs_log(LOG_ERROR, "无法从 Cookies 中提取 bili_jct");
		return false;
	}
	csrf_token = cookies.substr(pos + 9, cookies.find(';', pos) - pos - 9);

	//obs_log(LOG_INFO, "获取 room_id 和 csrf_token 成功: room_id=%s, csrf_token=%s", room_id.c_str(), csrf_token.c_str());
	return true;
}

json11::Json BiliApi::getPartitionList(std::string &message)
{
	auto response = Http::HttpClient::get("https://api.live.bilibili.com/room/v1/Area/getList", default_headers);
	if (response.status != 200) {
		message = "获取分区列表失败，状态码: " + std::to_string(response.status);
		if (!response.data.empty()) {
			message += ", 数据: " + response.data;
		}
		return json11::Json();
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty() || !json["data"].is_array()) {
		message = "解析分区列表失败: " + std::string(err.empty() ? "无数据数组" : err);
		return json11::Json();
	}
	message = "获取分区列表成功";
	return json["data"];
}

bool BiliApi::startLive(Config &config, std::string &rtmp_addr, std::string &rtmp_code, std::string &message)
{
	if (config.room_id.empty() || config.csrf_token.empty()) {
		//obs_log(LOG_ERROR, "配置无效: room_id=%s, csrf_token=%s, title=%s",
		//config.room_id.c_str(), config.csrf_token.c_str(), config.title.c_str());
		message = "配置无效: 房间号=" + config.room_id + ", csrf_token=" + config.csrf_token;
		return false;
	}

	const std::string app_key = "aae92bc66f3edfab";
	const std::string app_sec = "af125a0d5279fd576c1b4418a3e8276d";

	std::vector<std::pair<std::string, std::string>> version_params = {{"system_version", "2"},
									   {"ts", std::to_string(time(nullptr))}};
	std::string version_query = appsign(version_params, app_key, app_sec);
	std::string version_url =
		"https://api.live.bilibili.com/xlive/app-blink/v1/liveVersionInfo/getHomePageLiveVersion?" +
		version_query;

	auto headers = buildHeaders(config.cookies);
	auto version_response = Http::HttpClient::get(version_url, headers);
	if (version_response.status != 200) {
		//obs_log(LOG_ERROR, "获取直播版本信息失败，状态码: %ld", version_response.status);
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(version_response.data, err);
	if (!err.empty() || json["code"].int_value() != 0) {
		message = "解析直播版本信息失败: " + (err.empty() ? json["message"].string_value() : err);
		return false;
	}

	long build = json["data"]["build"].int_value();
	std::string curr_version = json["data"]["curr_version"].string_value();
	if (build == 0 || curr_version.empty()) {
		//obs_log(LOG_ERROR, "无效的 build 或 curr_version");
		return false;
	}

	std::vector<std::pair<std::string, std::string>> start_params = {{"room_id", config.room_id},
									 {"platform", "pc_link"},
									 {"area_v2", std::to_string(config.area_id)},
									 {"backup_stream", "0"},
									 {"csrf_token", config.csrf_token},
									 {"csrf", config.csrf_token},
									 {"build", std::to_string(build)},
									 {"version", curr_version},
									 {"ts", std::to_string(time(nullptr))}};
	std::string start_data = appsign(start_params, app_key, app_sec);

	auto response =
		Http::HttpClient::post("https://api.live.bilibili.com/room/v1/Room/startLive", start_data, headers);
	if (response.status != 200) {
		//obs_log(LOG_ERROR, "启动直播失败，状态码: %ld", response.status);
		return false;
	}
	if (response.status != 200) {
		message = "获取二维码失败，状态码: " + std::to_string(response.status);
		if (!response.data.empty()) {
			message += ", 数据: " + response.data;
		}
		return false;
	}

	json = json11::Json::parse(response.data, err);
	if (!err.empty() || json["code"].int_value() != 0) {
		//obs_log(LOG_ERROR, "启动直播失败: %s", err.empty() ? json["message"].string_value().c_str() : err.c_str());
		return false;
	}

	rtmp_addr = json["data"]["rtmp"]["addr"].string_value();
	rtmp_code = json["data"]["rtmp"]["code"].string_value();
	if (rtmp_addr.empty() || rtmp_code.empty()) {
		//obs_log(LOG_ERROR, "无法解析 RTMP 地址或推流码");
		return false;
	}

	//obs_log(LOG_INFO, "直播启动成功，RTMP 地址: %s, 推流码: %s", rtmp_addr.c_str(), rtmp_code.c_str());
	return true;
}

bool BiliApi::stopLive(const Config &config, std::string &message)
{
	std::string data = "room_id=" + config.room_id + "&platform=pc_link&csrf_token=" + config.csrf_token +
			   "&csrf=" + config.csrf_token;
	auto headers = buildHeaders(config.cookies);
	auto response = Http::HttpClient::post("https://api.live.bilibili.com/room/v1/Room/stopLive", data, headers);
	if (response.status != 200) {
		//obs_log(LOG_ERROR, "停止直播失败，状态码: %ld", response.status);
		return false;
	}
	if (response.status != 200) {
		message = "获取二维码失败，状态码: " + std::to_string(response.status);
		if (!response.data.empty()) {
			message += ", 数据: " + response.data;
		}
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty() || json["code"].int_value() != 0) {
		//obs_log(LOG_ERROR, "停止直播失败: %s", err.empty() ? json["message"].string_value().c_str() : err.c_str());
		return false;
	}

	//obs_log(LOG_INFO, "直播已停止");
	return true;
}

bool BiliApi::updateRoomInfo(const Config &config, const std::string &title, std::string &message)
{
	std::string data = "room_id=" + config.room_id + "&platform=pc_link&title=" + title +
			   "&csrf_token=" + config.csrf_token + "&csrf=" + config.csrf_token;
	auto headers = buildHeaders(config.cookies);
	auto response = Http::HttpClient::post("https://api.live.bilibili.com/room/v1/Room/update", data, headers);
	if (response.status != 200) {
		message = "获取更新房间信息失败，状态码: " + std::to_string(response.status);
		if (!response.data.empty()) {
			message += ", 数据: " + response.data;
		}
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty() || json["code"].int_value() != 0) {
		//obs_log(LOG_ERROR, "更新直播间信息失败: %s", err.empty() ? json["message"].string_value().c_str() : err.c_str());
		message = "更新直播间信息失败: " + (err.empty() ? json["message"].string_value() : err);
		return false;
	}

	//obs_log(LOG_INFO, "直播间信息更新成功: %s", title.c_str());
	return true;
}
} // namespace Bili
