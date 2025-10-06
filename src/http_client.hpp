#pragma once
#include <string>
#include <vector>
#include <cstring>
namespace Http {
struct HttpResponse {
	long status;
	std::string data;
	std::string cookies;
};

class HttpClient {
public:
	static void init();
	static void cleanup();
	static HttpResponse get(const std::string &url, const std::vector<std::string> &headers = {});
	static HttpResponse post(const std::string &url, const std::string &data,
				 const std::vector<std::string> &headers = {});
};
} // namespace Http
