#pragma once
#include "http_client.hpp"
#include <QApplication>
#include <QPointer>
#include <QMetaObject>
namespace UI {
inline QObject *asyncDispatcher = nullptr;
inline void shutdownAsync()
{
	delete asyncDispatcher;
	asyncDispatcher = nullptr;
}
// Only the completion closure may access UI or shared configuration.
template<typename Work> void runAsync(QObject *context, Work work)
{
	if (!asyncDispatcher)
		asyncDispatcher = new QObject(qApp);
	auto dispatcher = asyncDispatcher;
	QPointer<QObject> guard(context);
	Http::HttpClient::enqueue([guard, dispatcher, work = std::move(work)]() mutable {
		auto complete = work();
		QMetaObject::invokeMethod(
			dispatcher,
			[guard, complete = std::move(complete)]() mutable {
				if (guard)
					complete();
			},
			Qt::QueuedConnection);
	});
}
} // namespace UI
