#include "logging.hpp"

#include <reshade.hpp>

#include <atomic>
#include <string>

namespace ds3ea::log
{
namespace
{
// Set by the add-on at load and cleared at unload; atomic so any thread can log while it changes
std::atomic<Sink> g_sink = nullptr;
} // namespace

void set_sink(Sink sink)
{
	g_sink.store(sink);
}

ReshadeLine to_reshade_line(Level level, std::string_view message)
{
	switch (level)
	{
	case Level::error:
		return { reshade::log::level::error, std::string(message) };
	case Level::warning:
		return { reshade::log::level::warning, std::string(message) };
	case Level::info:
		return { reshade::log::level::info, std::string(message) };
	case Level::debug:
		return { reshade::log::level::debug, std::string(message) };
	case Level::trace:
		return { reshade::log::level::debug, fmt::format("T: {}", message) };
	}

	return { reshade::log::level::info, std::string(message) };
}

void write_to_reshade(Level level, std::string_view message)
{
	const ReshadeLine line = to_reshade_line(level, message);
	reshade::log::message(line.level, line.text.c_str());
}

void write(Level level, std::string_view message)
{
	const Sink sink = g_sink.load();
	if (sink != nullptr)
		sink(level, message);
}
} // namespace ds3ea::log
