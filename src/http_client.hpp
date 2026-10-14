#pragma once
#include <string>
#include <vector>
#include <cstring>
#include <functional>

class QObject;

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
	// NOTE: the getAsync/postAsync callbacks still run on the HTTP worker thread;
	// use runAsync() below when the result has to reach the UI thread.
	static void getAsync(const std::string &url, const std::vector<std::string> &headers,
			     std::function<void(HttpResponse)> callback, long timeout_ms = 10000);
	static void postAsync(const std::string &url, const std::string &data, const std::vector<std::string> &headers,
			      std::function<void(HttpResponse)> callback, long timeout_ms = 10000);

	// Runs `work` on the HTTP worker thread, so a blocking network call can never
	// stall the Qt event loop, and delivers `done` to the thread that owns
	// `context` through a queued event, so a modal dialog's nested event loop still
	// runs it. `done` is dropped once the module unloads or `context` has been
	// destroyed, so it is safe to touch widgets from it.
	// `work` must not touch UI objects: capture what it needs by value instead of
	// reading plugin state.
	static void runAsync(QObject *context, std::function<void()> work, std::function<void()> done);
};
} // namespace Http
