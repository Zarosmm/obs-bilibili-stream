#include "http_client.hpp"
#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <exception>
#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QPointer>
#include "plugin_utils.hpp"
#include "util/base.h"

#ifdef _WIN32
#define strncasecmp _strnicmp
#endif
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
	// libcurl does not guarantee a NUL-terminated buffer and may deliver short
	// header lines (such as the terminating CRLF), so never read past realsize.
	if (realsize > strlen(set_cookie) && strncasecmp(buffer, set_cookie, strlen(set_cookie)) == 0) {
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

// All background work runs as opaque jobs on the single worker thread, which
// also serializes the libcurl calls made by HttpClient::get()/post().
using AsyncJob = std::function<void()>;

static std::queue<AsyncJob> async_queue;
static std::mutex queue_mutex;
static std::condition_variable queue_cv;
static bool stop_worker = false;
static std::thread worker_thread;

static bool workerStopping()
{
	std::lock_guard<std::mutex> lock(queue_mutex);
	return stop_worker;
}

static void enqueue(AsyncJob job)
{
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		if (stop_worker)
			return; // module is unloading: refuse new work
		async_queue.push(std::move(job));
	}
	queue_cv.notify_one();
}

// A completed job is handed back to the thread that owns its context object.
// QMetaObject::invokeMethod(context, functor, Qt::QueuedConnection) is the
// obvious tool for that, but its functor overload instantiates
// QtPrivate::qMetaTypeInterfaceForType<void>(), which the Qt builds shipped
// with the Windows and macOS OBS dependencies do not export (unresolved
// external at link time). Posting an event instead only relies on
// QCoreApplication::postEvent(), and posted events are still delivered by
// nested event loops such as QDialog::exec().
class JobEvent : public QEvent {
public:
	JobEvent(QPointer<QObject> context, std::function<void()> job)
		: QEvent(eventType()),
		  m_context(std::move(context)),
		  m_job(std::move(job))
	{
	}

	static QEvent::Type eventType()
	{
		static const QEvent::Type type = static_cast<QEvent::Type>(QEvent::registerEventType());
		return type;
	}

	const QPointer<QObject> &context() const { return m_context; }

	void run() const
	{
		if (m_job)
			m_job();
	}

private:
	QPointer<QObject> m_context;
	std::function<void()> m_job;
};

class UiThreadReceiver : public QObject {
protected:
	bool event(QEvent *event) override
	{
		if (event->type() == JobEvent::eventType()) {
			auto *job = static_cast<JobEvent *>(event);
			// A context that is already gone drops the job, matching what
			// Qt::QueuedConnection does for a destroyed receiver.
			if (job->context())
				job->run();
			return true;
		}
		return QObject::event(event);
	}
};

// Deliberately leaked: cleanup() detaches the worker thread, so a job can still
// be posted while the module unloads. The receiver has to stay valid for that
// post; the job itself is dropped, because its QPointer context is gone.
static QObject *uiThreadReceiver()
{
	static QObject *receiver = new UiThreadReceiver();
	return receiver;
}

static void deliverToUiThread(const QPointer<QObject> &context, std::function<void()> done)
{
	if (!done)
		return;
	QCoreApplication::postEvent(uiThreadReceiver(), new JobEvent(context, std::move(done)));
}

static void workerLoop()
{
	while (true) {
		std::unique_lock<std::mutex> lock(queue_mutex);
		queue_cv.wait(lock, [] { return !async_queue.empty() || stop_worker; });
		if (stop_worker) {
			// Shutting down: drop queued jobs and exit. Draining them here
			// could block module unload for the full timeout of every pending
			// request.
			while (!async_queue.empty())
				async_queue.pop();
			break;
		}
		AsyncJob job = async_queue.front();
		async_queue.pop();
		lock.unlock();

		try {
			job();
		} catch (const std::exception &error) {
			obs_log(LOG_ERROR, "异步任务执行失败: %s", error.what());
		} catch (...) {
			obs_log(LOG_ERROR, "异步任务执行失败: 未知异常");
		}
	}
}

void HttpClient::init()
{
	if (worker_thread.joinable())
		return;
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
	// Never block OBS shutdown: the worker exits by itself once the request
	// in flight (if any) returns. curl_global_cleanup() is skipped for the
	// same reason; it is optional at process exit.
	if (worker_thread.joinable())
		worker_thread.detach();
}

// Shared by get() and post(): post_data == nullptr makes it a GET request.
static HttpResponse perform(const std::string &url, const std::string *post_data,
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
	if (post_data) {
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data->c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(post_data->size()));
	}
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_cookies);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
	// Fail fast when the network is down instead of using the whole timeout
	// on the connection attempt.
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms < 5000L ? timeout_ms : 5000L);
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
	} else {
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
		response.data = std::move(response_data);
		response.cookies = std::move(response_cookies);
	}
	curl_slist_free_all(header_list);
	curl_easy_cleanup(curl);
	return response;
}

HttpResponse HttpClient::get(const std::string &url, const std::vector<std::string> &headers, long timeout_ms)
{
	return perform(url, nullptr, headers, timeout_ms);
}

HttpResponse HttpClient::post(const std::string &url, const std::string &data, const std::vector<std::string> &headers,
			      long timeout_ms)
{
	return perform(url, &data, headers, timeout_ms);
}

void HttpClient::getAsync(const std::string &url, const std::vector<std::string> &headers,
			  std::function<void(HttpResponse)> callback, long timeout_ms)
{
	enqueue([url, headers, callback = std::move(callback), timeout_ms]() {
		HttpResponse response = HttpClient::get(url, headers, timeout_ms);
		if (workerStopping())
			return; // unload in progress: skip callbacks that may touch destroyed state
		if (callback)
			callback(response);
	});
}

void HttpClient::postAsync(const std::string &url, const std::string &data, const std::vector<std::string> &headers,
			   std::function<void(HttpResponse)> callback, long timeout_ms)
{
	enqueue([url, data, headers, callback = std::move(callback), timeout_ms]() {
		HttpResponse response = HttpClient::post(url, data, headers, timeout_ms);
		if (workerStopping())
			return; // unload in progress: skip callbacks that may touch destroyed state
		if (callback)
			callback(response);
	});
}

void HttpClient::runAsync(QObject *context, std::function<void()> work, std::function<void()> done)
{
	// runAsync() is called from the UI thread, so priming the receiver here
	// gives it that thread's affinity before any worker can post to it.
	(void)uiThreadReceiver();

	QPointer<QObject> guard(context);
	enqueue([work = std::move(work), done = std::move(done), guard]() {
		try {
			work();
		} catch (const std::exception &error) {
			// Swallow the failure, but still deliver `done`: the callers rely
			// on it to clear their busy state. The continuation reports the
			// error through the shared result struct it captured.
			obs_log(LOG_ERROR, "异步任务执行失败: %s", error.what());
		} catch (...) {
			obs_log(LOG_ERROR, "异步任务执行失败: 未知异常");
		}
		if (workerStopping())
			return; // unload in progress: drop UI continuations
		deliverToUiThread(guard, std::move(done));
	});
}
} // namespace Http
