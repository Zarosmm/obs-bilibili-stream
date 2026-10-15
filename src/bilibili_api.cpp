#include "bilibili_api.hpp"
#include "http_client.hpp"
#include "md5.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <sstream>
#include "plugin_utils.hpp"
#include "util/base.h"

namespace Bili {
// Bilibili puts the human readable error text in either "message" or "msg"
// depending on the endpoint. Accept both instead of showing an empty dialog.
static std::string jsonErrorMessage(const json11::Json &json)
{
	std::string message = json["message"].string_value();
	if (message.empty())
		message = json["msg"].string_value();
	return message;
}

static const std::string APP_KEY = "aae92bc66f3edfab";
static const std::string APP_SECRET = "af125a0d5279fd576c1b4418a3e8276d";

// API replies carry the stream key, the CSRF token, login ticket URLs and
// account details, and OBS logs are routinely pasted into bug reports. Log the
// verdict of a reply (code and message), never its raw body.
static void logReply(const char *what, const json11::Json &json)
{
	obs_log(LOG_INFO, "%s: code=%d, message=%s", what, json["code"].int_value(), jsonErrorMessage(json).c_str());
}

// Error text for a request that did not get an HTTP 200. Only a short prefix of
// the body is kept: an HTML error page would otherwise fill the whole dialog.
static std::string httpFailure(const std::string &what, const Http::HttpResponse &response)
{
	obs_log(LOG_ERROR, "%s失败，状态码: %ld", what.c_str(), response.status);
	std::string message = what + "失败，状态码: " + std::to_string(response.status);
	if (!response.data.empty()) {
		constexpr size_t max_body = 200;
		message += ", 数据: " + response.data.substr(0, max_body);
		if (response.data.size() > max_body)
			message += "...";
	}
	return message;
}

// Value of cookie `name` in a "k=v; k2=v2" string. The whole name has to match,
// so DedeUserID__ckMd5 is never mistaken for DedeUserID.
static std::string cookieValue(const std::string &cookies, const std::string &name)
{
	size_t pos = 0;
	while (pos < cookies.size()) {
		size_t end = cookies.find(';', pos);
		if (end == std::string::npos)
			end = cookies.size();
		size_t start = cookies.find_first_not_of(' ', pos);
		if (start < end) {
			size_t eq = cookies.find('=', start);
			if (eq < end && cookies.compare(start, eq - start, name) == 0)
				return cookies.substr(eq + 1, end - eq - 1);
		}
		pos = end + 1;
	}
	return "";
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
	auto headers = buildHeaders(cookies);
	auto response =
		Http::HttpClient::get("https://passport.bilibili.com/x/passport-login/web/qrcode/generate", headers);
	if (response.status != 200) {
		message = httpFailure("获取二维码", response);
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

	message = "获取二维码成功";
	return true;
}

bool BiliApi::qrLogin(const std::string &qr_key, std::string &cookies, std::string &message, int &poll_code)
{
	poll_code = QR_POLL_REQUEST_FAILED;
	std::string url = "https://passport.bilibili.com/x/passport-login/web/qrcode/poll?qrcode_key=" + qr_key;
	auto response = Http::HttpClient::get(url, default_headers);
	// This runs once a second while the QR dialog is open, and a successful
	// reply contains the login ticket URL: never log the body here.
	if (response.status != 200) {
		message = httpFailure("检查二维码登录状态", response);
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty()) {
		obs_log(LOG_ERROR, "JSON 解析失败: %s", err.c_str());
		message = "JSON 解析失败: " + err;
		return false;
	}

	if (json["code"].int_value() != 0 || !json["data"]["code"].is_number()) {
		// Without this check a missing data.code would read as 0 (success).
		logReply("检查二维码登录状态", json);
		message = "检查二维码登录状态失败: " + jsonErrorMessage(json);
		return false;
	}

	int code = json["data"]["code"].int_value();
	poll_code = code;
	if (code != QR_POLL_SUCCESS) {
		const std::string detail = json["data"]["message"].string_value();
		if (code == QR_POLL_EXPIRED) {
			obs_log(LOG_INFO, "二维码已失效");
			message = "二维码已失效，请关闭后重新扫码登录";
		} else if (code == QR_POLL_SCANNED) {
			message = "已扫描，请在手机上确认登录";
		} else if (code == QR_POLL_NOT_SCANNED) {
			message = "使用手机 Bilibili App 扫描二维码登录";
		} else {
			obs_log(LOG_ERROR, "二维码登录返回错误，code: %d, message: %s", code, detail.c_str());
			message = "二维码登录返回错误，code: " + std::to_string(code) + ", message: " + detail;
		}
		return false;
	}
	obs_log(LOG_INFO, "二维码已确认，正在获取登录 Cookies");

	cookies = response.cookies;
	if (cookies.empty()) {
		std::string loginUrl = json["data"]["url"].string_value();
		if (!loginUrl.empty()) {
			// bilibili changed the login flow (2026-08): the success URL is now
			// a crossDomain ticket URL (passport.biligame.com/x/passport-login/
			// web/crossDomain?ticket=...&gourl=...&first_domain=...) which no
			// longer embeds SESSDATA/bili_jct/DedeUserID. Following it sets the
			// login cookies via Set-Cookie response headers (the HTTP client
			// captures those but does not follow the redirect itself).
			obs_log(LOG_INFO, "从 crossDomain 跳转 URL 获取 Cookies...");
			auto ticketResp = Http::HttpClient::get(loginUrl, default_headers);
			if (!ticketResp.cookies.empty()) {
				cookies = ticketResp.cookies;
				obs_log(LOG_INFO, "crossDomain Cookies 获取成功");
			} else {
				obs_log(LOG_INFO, "从 URL 解析 Cookies...");

				auto extractParam = [&](const std::string &key) -> std::string {
					std::string search = key + "=";
					size_t start = loginUrl.find(search);
					if (start == std::string::npos)
						return "";
					start += search.length();
					size_t end = loginUrl.find("&", start);
					if (end == std::string::npos)
						end = loginUrl.length();
					return loginUrl.substr(start, end - start);
				};

				std::string sessData = extractParam("SESSDATA");
				std::string biliJct = extractParam("bili_jct"); // 即 csrf
				std::string dedeUserId = extractParam("DedeUserID");

				if (!sessData.empty() && !biliJct.empty()) {
					cookies = "SESSDATA=" + sessData + "; bili_jct=" + biliJct +
						  "; DedeUserID=" + dedeUserId + ";";
				}
			}
		}
	}
	if (cookies.empty()) {
		obs_log(LOG_ERROR, "无法获取登录 Cookies");
		message = "无法获取登录 Cookies";
		return false;
	}

	message = "二维码登录成功";
	return true;
}

bool BiliApi::checkLoginStatus(const std::string &cookies, std::string &message, std::string &mid)
{
	auto headers = buildHeaders(cookies);
	auto response = Http::HttpClient::get("https://api.bilibili.com/x/web-interface/nav", headers);
	if (response.status != 200) {
		message = httpFailure("检查登录状态", response);
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty()) {
		obs_log(LOG_ERROR, "JSON 解析失败: %s", err.c_str());
		message = "JSON 解析失败: " + err;
		return false;
	}

	bool is_login = json["data"]["isLogin"].bool_value();
	if (is_login) {
		mid = std::to_string(static_cast<long long>(json["data"]["mid"].number_value()));
	}
	obs_log(LOG_INFO, "检查登录状态: %s", is_login ? "已登录" : "未登录");
	message = "检查登录状态: " + std::string(is_login ? "已登录" : "未登录");
	return is_login;
}

bool BiliApi::getRoomIdAndCsrf(const std::string &cookies, std::string &room_id, std::string &csrf_token,
			       std::string &message)
{
	if (cookies.empty()) {
		obs_log(LOG_ERROR, "Cookies 为空");
		message = "Cookies 为空";
		return false;
	}

	const std::string dede_user_id = cookieValue(cookies, "DedeUserID");
	if (dede_user_id.empty()) {
		obs_log(LOG_ERROR, "无法从 Cookies 中提取 DedeUserID");
		message = "无法从 Cookies 中提取 DedeUserID，请重新扫码登录";
		return false;
	}
	const std::string bili_jct = cookieValue(cookies, "bili_jct");
	if (bili_jct.empty()) {
		obs_log(LOG_ERROR, "无法从 Cookies 中提取 bili_jct");
		message = "无法从 Cookies 中提取 bili_jct (csrf)，请重新扫码登录";
		return false;
	}

	std::string url = "https://api.live.bilibili.com/room/v2/Room/room_id_by_uid?uid=" + dede_user_id;
	auto headers = buildHeaders(cookies);
	auto response = Http::HttpClient::get(url, headers);
	if (response.status != 200) {
		message = httpFailure("获取房间号", response);
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty()) {
		obs_log(LOG_ERROR, "JSON 解析失败: %s", err.c_str());
		message = "Json 解析失败: " + err;
		return false;
	}
	if (json["code"].int_value() != 0) {
		const std::string detail = jsonErrorMessage(json);
		obs_log(LOG_ERROR, "API 返回错误，code: %d, message: %s", json["code"].int_value(), detail.c_str());
		message = "API 返回错误， code: " + std::to_string(json["code"].int_value()) + ", message: " + detail;
		return false;
	}

	const long long id = static_cast<long long>(json["data"]["room_id"].number_value());
	if (id <= 0) {
		// The account has never opened a live room: room_id comes back as 0.
		obs_log(LOG_WARNING, "该账号尚未开通直播间");
		message = "该账号尚未开通直播间，请先在 https://link.bilibili.com 开通后重新登录";
		return false;
	}
	room_id = std::to_string(id);
	csrf_token = bili_jct;
	obs_log(LOG_INFO, "获取房间号成功: %s", room_id.c_str());
	return true;
}

json11::Json BiliApi::getPartitionList(std::string &message)
{
	auto response = Http::HttpClient::get("https://api.live.bilibili.com/room/v1/Area/getList", default_headers);
	if (response.status != 200) {
		message = httpFailure("获取分区列表", response);
		return json11::Json();
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty() || !json["data"].is_array()) {
		message = "解析分区列表失败: " + std::string(err.empty() ? "无数据数组" : err);
		obs_log(LOG_ERROR, "%s", message.c_str());
		return json11::Json();
	}
	// The full list is tens of kilobytes; logging it used to flood the OBS log.
	obs_log(LOG_INFO, "获取分区列表成功，共 %d 个大分区", static_cast<int>(json["data"].array_items().size()));
	message = "获取分区列表成功";
	return json["data"];
}

bool BiliApi::startLive(Config &config, std::string &rtmp_addr, std::string &rtmp_code, std::string &message,
			std::string &face_qr, std::string &mid)
{
	if (config.room_id.empty() || config.csrf_token.empty()) {
		// Never echo the CSRF token: it ends up in the log and on screen.
		obs_log(LOG_ERROR, "配置无效: room_id=%s, csrf_token %s", config.room_id.c_str(),
			config.csrf_token.empty() ? "为空" : "已设置");
		message = "尚未登录或未获取到直播间信息，请先扫码登录";
		return false;
	}

	std::vector<std::pair<std::string, std::string>> version_params = {{"system_version", "2"},
									   {"ts", std::to_string(time(nullptr))}};
	std::string version_query = appsign(version_params, APP_KEY, APP_SECRET);
	std::string version_url =
		"https://api.live.bilibili.com/xlive/app-blink/v1/liveVersionInfo/getHomePageLiveVersion?" +
		version_query;

	auto headers = buildHeaders(config.cookies);
	auto version_response = Http::HttpClient::get(version_url, headers);
	if (version_response.status != 200) {
		message = httpFailure("获取直播版本信息", version_response);
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(version_response.data, err);
	if (!err.empty() || json["code"].int_value() != 0) {
		const std::string detail = err.empty() ? jsonErrorMessage(json) : err;
		obs_log(LOG_ERROR, "获取直播版本信息失败: %s", detail.c_str());
		message = "解析直播版本信息失败: " + detail;
		return false;
	}

	long build = json["data"]["build"].int_value();
	std::string curr_version = json["data"]["curr_version"].string_value();
	if (build == 0 || curr_version.empty()) {
		obs_log(LOG_ERROR, "无效的 build 或 curr_version");
		message = "无效的 build 或 curr_version";
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
	std::string start_data = appsign(start_params, APP_KEY, APP_SECRET);

	auto response =
		Http::HttpClient::post("https://api.live.bilibili.com/room/v1/Room/startLive", start_data, headers);
	if (response.status != 200) {
		message = httpFailure("启动直播", response);
		return false;
	}

	json = json11::Json::parse(response.data, err);
	if (!err.empty()) {
		obs_log(LOG_ERROR, "解析开播响应失败: %s", err.c_str());
		message = "解析开播响应失败: " + err;
		return false;
	}

	// The reply holds the RTMP address and the stream key: log the verdict only.
	logReply("启动直播", json);
	int code = json["code"].int_value();
	if (code != 0) {
		message = jsonErrorMessage(json);
		if (message.empty())
			message = "开播失败，错误码: " + std::to_string(code);

		// 如果是人脸识别
		if (code == 60024) {
			std::string face_url = json["data"]["qr"].string_value();
			obs_log(LOG_WARNING, "60024 需要人脸识别，URL: %s", face_url.c_str());
			// 这里可以弹出一个对话框或者在 UI 上显示二维码
			message = "需要人脸验证，请扫描二维码" + face_url;
			face_qr = face_url;
			return false;
		}
		if (code == 60043) {
			std::string face_url =
				"https://www.bilibili.com/blackboard/live/face-auth-middle.html?source_event=400&mid=" +
				mid;
			obs_log(LOG_WARNING, "60043 需要人脸识别，URL: %s", face_url.c_str());
			message = "需要人脸验证，请扫描二维码" + face_url;
			face_qr = face_url;
			return false;
		}
		return false;
	}

	rtmp_addr = json["data"]["rtmp"]["addr"].string_value();
	rtmp_code = json["data"]["rtmp"]["code"].string_value();
	if (rtmp_addr.empty() || rtmp_code.empty()) {
		obs_log(LOG_ERROR, "无法解析 RTMP 地址或推流码");
		message = "开播请求成功，但未获取到推流地址，请稍后重试";
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
		message = httpFailure("停止直播", response);
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty() || json["code"].int_value() != 0) {
		const std::string detail = err.empty() ? jsonErrorMessage(json) : err;
		obs_log(LOG_ERROR, "停止直播失败: %s", detail.c_str());
		message = "停止直播失败: " + detail;
		return false;
	}

	obs_log(LOG_INFO, "直播已停止");
	return true;
}

bool BiliApi::updateRoomInfo(const Config &config, std::string &message, const std::string &title, int areaId)
{
	if (title.empty() && areaId < 0) {
		message = "无可更新内容";
		return false;
	}

	std::string data = "room_id=" + config.room_id + "&platform=pc_link";
	if (!title.empty()) {
		char *escaped = curl_easy_escape(nullptr, title.c_str(), 0);
		data += "&title=";
		data += escaped ? escaped : title;
		if (escaped)
			curl_free(escaped);
	}
	if (areaId >= 0) {
		data += "&area_id=" + std::to_string(areaId);
	}
	data += "&csrf_token=" + config.csrf_token + "&csrf=" + config.csrf_token;
	auto headers = buildHeaders(config.cookies);
	auto response = Http::HttpClient::post("https://api.live.bilibili.com/room/v1/Room/update", data, headers);
	if (response.status != 200) {
		message = httpFailure("更新直播间信息", response);
		return false;
	}

	std::string err;
	json11::Json json = json11::Json::parse(response.data, err);
	if (!err.empty() || json["code"].int_value() != 0) {
		message = "更新直播间信息失败: " + (err.empty() ? jsonErrorMessage(json) : err);
		obs_log(LOG_ERROR, "%s", message.c_str());
		return false;
	}

	obs_log(LOG_INFO, "直播间信息更新成功: %s", title.c_str());
	return true;
}
} // namespace Bili
