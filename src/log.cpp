#include "log.hpp"

#include <atomic>

namespace ds3ea
{
namespace
{
// Set by the add-on at load and cleared at unload; atomic so any thread can log while it changes
std::atomic<LogSink> g_sink = nullptr;
} // namespace

void set_log_sink(LogSink sink)
{
	g_sink.store(sink);
}

void write_log(LogLevel level, std::string_view message)
{
	const LogSink sink = g_sink.load();
	if (sink != nullptr)
		sink(level, message);
}
} // namespace ds3ea
