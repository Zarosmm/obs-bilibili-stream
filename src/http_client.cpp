#include "http_client.hpp"
#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

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
			return realsize;
		}
		if (!cookies->empty())
			*cookies += "; ";
		*cookies += cookie;
	}
	return realsize;
}

struct AsyncRequest {
	std::string url;
	std::string data;
	std::vector<std::string> headers;
	std::function<void(HttpResponse)> callback;
	bool is_post;
	long timeout_ms;
};

static std::queue<AsyncRequest> async_queue;
static std::mutex queue_mutex;
static std::condition_variable queue_cv;
static bool stop_worker = false;
static std::thread worker_thread;

static void workerLoop()
{
	while (true) {
		std::unique_lock<std::mutex> lock(queue_mutex);
		queue_cv.wait(lock, [] { return !async_queue.empty() || stop_worker; });
		if (stop_worker && async_queue.empty())
			break;
		AsyncRequest req = async_queue.front();
		async_queue.pop();
		lock.unlock();

		HttpResponse response;
		if (req.is_post) {
			response = HttpClient::post(req.url, req.data, req.headers, req.timeout_ms);
		} else {
			response = HttpClient::get(req.url, req.headers, req.timeout_ms);
		}
		req.callback(response);
	}
}

void HttpClient::init()
{
	curl_global_init(CURL_GLOBAL_ALL);
	stop_worker = false;
	worker_thread = std::thread(workerLoop);
}

void HttpClient::cleanup()
{
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		stop_worker = true;
	}
	queue_cv.notify_all();
	if (worker_thread.joinable())
		worker_thread.join();
	curl_global_cleanup();
}

HttpResponse HttpClient::get(const std::string &url, const std::vector<std::string> &headers, long timeout_ms)
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
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

	struct curl_slist *header_list = nullptr;
	for (const auto &header : headers) {
		header_list = curl_slist_append(header_list, header.c_str());
	}
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		response.timeout = (res == CURLE_OPERATION_TIMEDOUT);
		response.data = std::string("网络错误: ") + curl_easy_strerror(res);
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

HttpResponse HttpClient::post(const std::string &url, const std::string &data,
			      const std::vector<std::string> &headers, long timeout_ms)
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
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

	struct curl_slist *header_list = nullptr;
	for (const auto &header : headers) {
		header_list = curl_slist_append(header_list, header.c_str());
	}
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		response.timeout = (res == CURLE_OPERATION_TIMEDOUT);
		response.data = std::string("网络错误: ") + curl_easy_strerror(res);
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

void HttpClient::getAsync(const std::string &url, const std::vector<std::string> &headers,
			  std::function<void(HttpResponse)> callback, long timeout_ms)
{
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		async_queue.push({url, "", headers, callback, false, timeout_ms});
	}
	queue_cv.notify_one();
}

void HttpClient::postAsync(const std::string &url, const std::string &data,
			   const std::vector<std::string> &headers, std::function<void(HttpResponse)> callback,
			   long timeout_ms)
{
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		async_queue.push({url, data, headers, callback, true, timeout_ms});
	}
	queue_cv.notify_one();
}
} // namespace Http
