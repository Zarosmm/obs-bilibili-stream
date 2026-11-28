#include "bilibili_api.hpp"
#include "http_client.hpp"
#include "md5.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <sstream>
#include <QUrl>
#include <QUrlQuery>
#include "plugin_utils.hpp"
#include "util/base.h"

namespace Bili {
// Response text, URLs and headers can contain credentials or account details.
static bool apiError(const char *operation, int code, std::string &message)
{
	obs_log(LOG_WARNING, "%s失败，Bilibili 错误码: %d", operation, code);
	message = std::string(operation) + "失败，Bilibili 错误码: " + std::to_string(code);
	return false;
}

static bool readResponse(const char *operation, const Http::HttpResponse &response, json11::Json &json,
			 std::string &message)
{
	if (response.status != 200) {
		obs_log(LOG_WARNING, "%s失败，HTTP 状态: %ld，超时: %d", operation, response.status,
			static_cast<int>(response.timeout));
		message = std::string(operation) + (response.timeout ? "超时，请重试"
						    : response.status == 0
							    ? "失败，请检查网络连接"
							    : "失败，HTTP 状态: " + std::to_string(response.status));
		return false;
	}

	std::string error;
	json = json11::Json::parse(response.data, error);
	if (!error.empty() || !json["code"].is_number()) {
		obs_log(LOG_WARNING, "%s失败，响应格式无效", operation);
		message = std::string(operation) + "失败，Bilibili 响应格式无效";
		return false;
	}
	return json["code"].int_value() == 0 || apiError(operation, json["code"].int_value(), message);
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

bool BiliApi::getQrCode(std::string &qr_data, std::string &qr_key, std::string &message)
{
	auto response = Http::HttpClient::get("https://passport.bilibili.com/x/passport-login/web/qrcode/generate",
					      default_headers);
	json11::Json json;
	if (!readResponse("获取登录二维码", response, json, message))
		return false;

	qr_data = json["data"]["url"].string_value();
	qr_key = json["data"]["qrcode_key"].string_value();
	if (qr_data.empty() || qr_key.empty()) {
		message = "获取登录二维码失败，Bilibili 响应缺少二维码数据";
		return false;
	}
	return true;
}

QrLoginStatus BiliApi::qrLogin(const std::string &qr_key, std::string &cookies, std::string &message)
{
	const auto url = "https://passport.bilibili.com/x/passport-login/web/qrcode/poll?qrcode_key=" + qr_key;
	auto response = Http::HttpClient::get(url, default_headers);
	json11::Json json;
	if (!readResponse("检查扫码状态", response, json, message))
		return QrLoginStatus::Error;

	const auto &data = json["data"];
	if (!data["code"].is_number()) {
		message = "检查扫码状态失败，Bilibili 响应格式无效";
		return QrLoginStatus::Error;
	}
	const int code = data["code"].int_value();
	switch (code) {
	case 86101:
		return QrLoginStatus::Waiting;
	case 86090:
		return QrLoginStatus::Scanned;
	case 86038:
		return QrLoginStatus::Expired;
	case 0:
		break;
	default:
		apiError("检查扫码状态", code, message);
		return QrLoginStatus::Error;
	}

	cookies = response.cookies;
	if (cookies.empty()) {
		const auto loginUrl = data["url"].string_value();
		const auto target = QUrl::fromEncoded(QByteArray::fromStdString(loginUrl), QUrl::StrictMode);
		const auto host = target.host();
		// The ticket contains login credentials. Restrict the secondary request to
		// known Bilibili passport hosts. HttpClient does not follow redirects.
		if (target.isValid() && target.scheme() == QStringLiteral("https") && target.userInfo().isEmpty() &&
		    target.port(443) == 443 &&
		    (host == QStringLiteral("passport.biligame.com") ||
		     host == QStringLiteral("passport.bilibili.com")) &&
		    target.path() == QStringLiteral("/x/passport-login/web/crossDomain")) {
			cookies = Http::HttpClient::get(target.toEncoded().toStdString(), default_headers).cookies;
		}

		// Older responses carry the cookies directly in the success URL.
		if (cookies.empty()) {
			const QUrlQuery query(target);
			const auto session = query.queryItemValue(QStringLiteral("SESSDATA"), QUrl::FullyEncoded);
			const auto csrf = query.queryItemValue(QStringLiteral("bili_jct"), QUrl::FullyEncoded);
			const auto user = query.queryItemValue(QStringLiteral("DedeUserID"), QUrl::FullyEncoded);
			if (!session.isEmpty() && !csrf.isEmpty() && !user.isEmpty())
				cookies = "SESSDATA=" + session.toStdString() + "; bili_jct=" + csrf.toStdString() +
					  "; DedeUserID=" + user.toStdString() + ";";
		}
	}
	if (cookies.empty()) {
		obs_log(LOG_WARNING, "扫码登录响应缺少登录凭据");
		message = "扫码登录失败，请关闭窗口后重新扫码";
		return QrLoginStatus::Error;
	}
	return QrLoginStatus::Success;
}

bool BiliApi::checkLoginStatus(const std::string &cookies, std::string &message, std::string &mid)
{
	auto headers = buildHeaders(cookies);
	auto response = Http::HttpClient::get("https://api.bilibili.com/x/web-interface/nav", headers);
	json11::Json json;
	if (!readResponse("检查登录状态", response, json, message))
		return false;

	bool is_login = json["data"]["isLogin"].bool_value();
	if (is_login) {
		mid = std::to_string(static_cast<long long>(json["data"]["mid"].number_value()));
	}
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

	size_t pos = cookies.find("DedeUserID=");
	if (pos == std::string::npos) {
		obs_log(LOG_ERROR, "无法从 Cookies 中提取 DedeUserID");
		message = "无法从 Cookies 中提取 DedeUserID";
		return false;
	}
	std::string dede_user_id = cookies.substr(pos + 11, cookies.find(';', pos) - pos - 11);

	std::string url = "https://api.live.bilibili.com/room/v2/Room/room_id_by_uid?uid=" + dede_user_id;
	auto headers = buildHeaders(cookies);
	auto response = Http::HttpClient::get(url, headers);
	json11::Json json;
	if (!readResponse("获取直播间", response, json, message))
		return false;

	const auto room = json["data"]["room_id"].number_value();
	pos = cookies.find("bili_jct=");
	if (room <= 0 || pos == std::string::npos) {
		message = "无法获取有效的直播间或登录凭据";
		return false;
	}
	csrf_token = cookies.substr(pos + 9, cookies.find(';', pos) - pos - 9);
	room_id = std::to_string(static_cast<long long>(room));
	return !csrf_token.empty();
}

json11::Json BiliApi::getPartitionList(std::string &message)
{
	auto response = Http::HttpClient::get("https://api.live.bilibili.com/room/v1/Area/getList", default_headers);
	json11::Json json;
	if (!readResponse("获取直播分区", response, json, message))
		return {};

	if (!json["data"].is_array() || json["data"].array_items().empty()) {
		message = "Bilibili 未返回可用的直播分区";
		return {};
	}
	return json["data"];
}

bool BiliApi::startLive(const Config &config, std::string &rtmp_addr, std::string &rtmp_code, std::string &message,
			std::string &face_qr)
{
	if (config.room_id.empty() || config.csrf_token.empty()) {
		message = "直播间尚未就绪，请重新扫码登录";
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
	json11::Json json;
	if (!readResponse("获取直播版本信息", version_response, json, message))
		return false;

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
	json = {};
	if (!readResponse("开始直播", response, json, message)) {
		if (json["code"].int_value() == 60024) {
			face_qr = json["data"]["qr"].string_value();
		} else if (json["code"].int_value() == 60043 && !config.mid.empty()) {
			face_qr =
				"https://www.bilibili.com/blackboard/live/face-auth-middle.html?source_event=400&mid=" +
				config.mid;
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

	return true;
}

bool BiliApi::stopLive(const Config &config, std::string &message)
{
	std::string data = "room_id=" + config.room_id + "&platform=pc_link&csrf_token=" + config.csrf_token +
			   "&csrf=" + config.csrf_token;
	auto headers = buildHeaders(config.cookies);
	auto response = Http::HttpClient::post("https://api.live.bilibili.com/room/v1/Room/stopLive", data, headers);
	json11::Json json;
	return readResponse("停止直播", response, json, message);
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
		if (!escaped) {
			message = "无法编码直播间标题";
			return false;
		}
		data += escaped;
		curl_free(escaped);
	}
	if (areaId >= 0) {
		data += "&area_id=" + std::to_string(areaId);
	}
	data += "&csrf_token=" + config.csrf_token + "&csrf=" + config.csrf_token;
	auto headers = buildHeaders(config.cookies);
	auto response = Http::HttpClient::post("https://api.live.bilibili.com/room/v1/Room/update", data, headers);
	json11::Json json;
	return readResponse("更新直播间信息", response, json, message);
}
} // namespace Bili
