#include "bilibili_api.hpp"
#include "http_client.hpp"
#include "md5.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
#include "plugin_utils.hpp"
namespace Bili {

// 辅助函数：统一日志输出响应
void log_response(const std::string& url, const Http::HttpResponse& response) {
    obs_log(LOG_DEBUG, "BiliAPI Response: URL: %s, Status: %ld", url.c_str(), response.status);
    if (!response.data.empty()) {
        obs_log(LOG_DEBUG, "BiliAPI Response Data:\n%s", response.data.c_str());
    } else {
        obs_log(LOG_DEBUG, "BiliAPI Response Data: (empty)");
    }
}

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
    const std::string url = "https://passport.bilibili.com/x/passport-login/web/qrcode/generate";
	auto headers = buildHeaders(cookies);
	auto response = Http::HttpClient::get(url, headers);
    log_response(url, response); // 输出日志
    
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
    
    if (json["code"].int_value() != 0) {
        message = "API 返回错误，code: " + std::to_string(json["code"].int_value()) + 
                  ", message: " + json["message"].string_value();
        return false;
    }

	qr_data = json["data"]["url"].string_value();
	qr_key = json["data"]["qrcode_key"].string_value();
	if (qr_data.empty() || qr_key.empty()) {
		message = "无法提取二维码数据或密钥";
		return false;
	}

	message = "获取二维码成功";
	return true;
}

bool BiliApi::qrLogin(std::string &qr_key, std::string &cookies, std::string &message)
{
	std::string url = "https://passport.bilibili.com/x/passport-login/web/qrcode/poll?qrcode_key=" + qr_key;
	auto response = Http::HttpClient::get(url, default_headers);
    log_response(url, response); // 输出日志

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
		message = "JSON 解析失败: " + err;
		return false;
	}

	int code = json["data"]["code"].int_value();
	if (code != 0) {
		if (code == 86038) {
			message = "二维码已失效: " + json["message"].string_value();
		} else if (code == 86090) {
			message = "二维码已扫描，等待确认";
		} else {
			message = "API 返回错误，code: " + std::to_string(code) +
				  ", message: " + json["message"].string_value();
		}
		return false;
	}

	cookies = response.cookies;
	if (cookies.empty()) {
		message = "无法获取登录 Cookies";
		return false;
	}

	message = "二维码登录成功";
	return true;
}

bool BiliApi::checkLoginStatus(const std::string &cookies, std::string &message)
{
    const std::string url = "https://api.bilibili.com/x/web-interface/nav";
	auto headers = buildHeaders(cookies);
	auto response = Http::HttpClient::get(url, headers);
    log_response(url, response); // 输出日志

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
		message = "JSON 解析失败: " + err;
		return false;
	}

	bool is_login = json["data"]["isLogin"].bool_value();
    
    if (json["code"].int_value() != 0 || !is_login) {
        message = "登录状态检查失败: " + (is_login ? "未知错误" : "未登录") +
                  ", API Message: " + json["message"].string_value();
        return false;
    }

	message = "检查登录状态: 已登录";
	return is_login;
}

bool BiliApi::getRoomIdAndCsrf(const std::string &cookies, std::string &room_id, std::string &csrf_token,
			       std::string &message)
{
	if (cookies.empty()) {
		message = "Cookies 为空";
		return false;
	}

    // 尽量健壮地提取 DedeUserID
	size_t pos_start = cookies.find("DedeUserID=");
	if (pos_start == std::string::npos) {
		message = "无法从 Cookies 中提取 DedeUserID";
		return false;
	}
    pos_start += 11; // 跳过 "DedeUserID="
    size_t pos_end = cookies.find(';', pos_start);
    if (pos_end == std::string::npos) pos_end = cookies.length();
	std::string dede_user_id = cookies.substr(pos_start, pos_end - pos_start);

	std::string url = "https://api.live.bilibili.com/room/v2/Room/room_id_by_uid?uid=" + dede_user_id;
	auto headers = buildHeaders(cookies);
	auto response = Http::HttpClient::get(url, headers);
    log_response(url, response); // 输出日志

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
		message = "Json 解析失败: " + err;
		return false;
	}
	if (json["code"].int_value() != 0) {
		message = "API 返回错误， code: " + std::to_string(json["code"].int_value()) +
			  ", message: " + json["message"].string_value();
		return false;
	}

	room_id = std::to_string(json["data"]["room_id"].int_value());
    
    // 尽量健壮地提取 bili_jct
	pos_start = cookies.find("bili_jct=");
	if (pos_start == std::string::npos) {
		message = "无法从 Cookies 中提取 bili_jct";
		return false;
	}
    pos_start += 9; // 跳过 "bili_jct="
    pos_end = cookies.find(';', pos_start);
    if (pos_end == std::string::npos) pos_end = cookies.length();
	csrf_token = cookies.substr(pos_start, pos_end - pos_start);

	message = "获取 room_id 和 csrf_token 成功";
	return true;
}

json11::Json BiliApi::getPartitionList(std::string &message)
{
    const std::string url = "https://api.live.bilibili.com/room/v1/Area/getList";
	auto response = Http::HttpClient::get(url, default_headers);
    log_response(url, response); // 输出日志
    
	if (response.status != 200) {
		message = "获取分区列表失败，状态码: " + std::to_string(response.status);
		if (!response.data.empty()) {
			message += ", 数据: " + response.data;
		}
		return json11::Json();
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty()) {
        message = "解析分区列表失败: " + err;
		return json11::Json();
    }
    
    if (json["code"].int_value() != 0 || !json["data"].is_array()) {
		message = "解析分区列表失败: API返回错误或无数据数组: " + json["message"].string_value();
		return json11::Json();
	}
    
	message = "获取分区列表成功";
	return json["data"];
}

bool BiliApi::startLive(Config &config, std::string &rtmp_addr, std::string &rtmp_code, std::string &message)
{
	if (config.room_id.empty() || config.csrf_token.empty()) {
		message = "配置无效: 房间号=" + config.room_id + ", csrf_token=" + config.csrf_token;
		return false;
	}

	const std::string app_key = "aae92bc66f3edfab";
	const std::string app_sec = "af125a0d5279fd576c1b4418a3e8276d";
    
    // 1. 获取直播版本信息
    const std::string version_url_base = "https://api.live.bilibili.com/xlive/app-blink/v1/liveVersionInfo/getHomePageLiveVersion";
	std::vector<std::pair<std::string, std::string>> version_params = {{"system_version", "2"},
									   {"ts", std::to_string(time(nullptr))}};
	std::string version_query = appsign(version_params, app_key, app_sec);
	std::string version_url = version_url_base + "?" + version_query;

	auto headers = buildHeaders(config.cookies);
	auto version_response = Http::HttpClient::get(version_url, headers);
    log_response(version_url, version_response); // 输出日志

	if (version_response.status != 200) {
        message = "获取直播版本信息失败，状态码: " + std::to_string(version_response.status);
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(version_response.data, err);
	if (!err.empty()) {
        message = "解析直播版本信息失败: " + err;
        return false;
    }
    
    if (json["code"].int_value() != 0) {
		message = "获取直播版本信息API返回错误: " + json["message"].string_value();
        return false;
    }

	long build = json["data"]["build"].int_value();
	std::string curr_version = json["data"]["curr_version"].string_value();
	if (build == 0 || curr_version.empty()) {
		message = "无效的 build 或 curr_version";
		return false;
	}

    // 2. 启动直播
    const std::string start_url = "https://api.live.bilibili.com/room/v1/Room/startLive";
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

	auto response = Http::HttpClient::post(start_url, start_data, headers);
    log_response(start_url, response); // 输出日志

	if (response.status != 200) {
		message = "启动直播失败，网络错误或服务器状态码: " + std::to_string(response.status);
		if (!response.data.empty()) {
			message += ", 数据: " + response.data;
		}
		return false;
	}
    
	json = json11::Json::parse(response.data, err);
	if (!err.empty()) {
        message = "解析启动直播响应失败: " + err;
        return false;
    }
    
    if (json["code"].int_value() != 0) {
		message = "启动直播失败: " + json["message"].string_value();
		return false;
	}

	rtmp_addr = json["data"]["rtmp"]["addr"].string_value();
	rtmp_code = json["data"]["rtmp"]["code"].string_value();
	if (rtmp_addr.empty() || rtmp_code.empty()) {
		message = "无法解析 RTMP 地址或推流码";
		return false;
	}

	message = "直播启动成功";
	return true;
}

bool BiliApi::stopLive(const Config &config, std::string &message)
{
    const std::string url = "https://api.live.bilibili.com/room/v1/Room/stopLive";
	std::string data = "room_id=" + config.room_id + "&platform=pc_link&csrf_token=" + config.csrf_token +
			   "&csrf=" + config.csrf_token;
	auto headers = buildHeaders(config.cookies);
	auto response = Http::HttpClient::post(url, data, headers);
    log_response(url, response); // 输出日志

	if (response.status != 200) {
		message = "停止直播失败，网络错误或服务器状态码: " + std::to_string(response.status);
		if (!response.data.empty()) {
			message += ", 数据: " + response.data;
		}
		return false;
	}
    
	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty()) {
        message = "解析停止直播响应失败: " + err;
        return false;
    }
    
    if (json["code"].int_value() != 0) {
		message = "停止直播失败: " + json["message"].string_value();
		return false;
	}

	message = "直播已停止";
	return true;
}

bool BiliApi::updateRoomInfo(const Config &config, const std::string &title, std::string &message)
{
    const std::string url = "https://api.live.bilibili.com/room/v1/Room/update";
	std::string data = "room_id=" + config.room_id + "&platform=pc_link&title=" + title +
			   "&csrf_token=" + config.csrf_token + "&csrf=" + config.csrf_token;
	auto headers = buildHeaders(config.cookies);
	auto response = Http::HttpClient::post(url, data, headers);
    log_response(url, response); // 输出日志

	if (response.status != 200) {
		message = "更新房间信息失败，网络错误或服务器状态码: " + std::to_string(response.status);
		if (!response.data.empty()) {
			message += ", 数据: " + response.data;
		}
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty()) {
        message = "解析更新房间信息响应失败: " + err;
		return false;
	}
    
    if (json["code"].int_value() != 0) {
		message = "更新直播间信息失败: " + json["message"].string_value();
		return false;
	}

	message = "直播间信息更新成功";
	return true;
}
} // namespace Bili
