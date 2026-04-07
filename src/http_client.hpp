#pragma once
#include <string>
#include <vector>
#include <cstring>
#include <functional>

namespace Http {
struct HttpResponse {
	long status;
	std::string data;
	std::string cookies;
	bool timeout = false;
};

class HttpClient {
public:
	static void init();
	static void cleanup();
	static HttpResponse get(const std::string &url, const std::vector<std::string> &headers = {},
			       long timeout_ms = 10000);
	static HttpResponse post(const std::string &url, const std::string &data,
				 const std::vector<std::string> &headers = {}, long timeout_ms = 10000);
	static void getAsync(const std::string &url, const std::vector<std::string> &headers,
			     std::function<void(HttpResponse)> callback, long timeout_ms = 10000);
	static void postAsync(const std::string &url, const std::string &data,
			      const std::vector<std::string> &headers,
			      std::function<void(HttpResponse)> callback, long timeout_ms = 10000);
};
} // namespace Http
