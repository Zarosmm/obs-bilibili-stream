#include "http_client.hpp"
#include <curl/curl.h>
#include <cstring>
namespace Http {
static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	auto *response = static_cast<std::string *>(userp);
	response->append(static_cast<char *>(contents), realsize);
	return realsize;
}

static size_t headerCallback(char *buffer, size_t size, size_t nitems, void *userp)
{
	size_t realsize = size * nitems;
	auto *cookies = static_cast<std::string *>(userp);
	const char *set_cookie = "Set-Cookie: ";
	if (strncmp(buffer, set_cookie, strlen(set_cookie)) == 0) {
		std::string cookie(buffer + strlen(set_cookie), realsize - strlen(set_cookie));
		size_t end = cookie.find(';');
		if (end != std::string::npos)
			cookie = cookie.substr(0, end);
		if (cookie.empty() || cookie.find('=') == std::string::npos) {
			// obs_log(LOG_WARNING, "无效的 Cookie: %s", cookie.c_str());
			return realsize;
		}
		if (!cookies->empty())
			*cookies += "; ";
		*cookies += cookie;
	}
	return realsize;
}

void HttpClient::init()
{
	curl_global_init(CURL_GLOBAL_ALL);
}

void HttpClient::cleanup()
{
	curl_global_cleanup();
}

HttpResponse HttpClient::get(const std::string &url, const std::vector<std::string> &headers)
{
	HttpResponse response;
	response.status = 0;
	CURL *curl = curl_easy_init();
	if (!curl) {
		response.data = "CURL 初始化失败";
		return response;
	}

	std::string response_data;
	std::string response_cookies;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_cookies);

	struct curl_slist *header_list = nullptr;
	for (const auto &header : headers) {
		header_list = curl_slist_append(header_list, header.c_str());
	}
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
        const char* err_msg = curl_easy_strerror(res);
        
        response.data = std::string("网络错误: ") + err_msg;
        response.status = 0;

        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
        return response;
    }

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
	response.data = std::move(response_data);
	response.cookies = std::move(response_cookies);
	curl_slist_free_all(header_list);
	curl_easy_cleanup(curl);
	return response;
}

HttpResponse HttpClient::post(const std::string &url, const std::string &data, const std::vector<std::string> &headers)
{
	HttpResponse response;
	response.status = 0;
	CURL *curl = curl_easy_init();
	if (!curl) {
		response.data = "CURL 初始化失败";
		return response;
	}

	std::string response_data;
	std::string response_cookies;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_cookies);

	struct curl_slist *header_list = nullptr;
	for (const auto &header : headers) {
		header_list = curl_slist_append(header_list, header.c_str());
	}
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
        const char* err_msg = curl_easy_strerror(res);
        
        response.data = std::string("网络错误: ") + err_msg;
        response.status = 0;

        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
        return response;
    }

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
	response.data = std::move(response_data);
	response.cookies = std::move(response_cookies);
	curl_slist_free_all(header_list);
	curl_easy_cleanup(curl);
	return response;
}
} // namespace Http
