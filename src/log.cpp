#include "log.hpp"

#include <reshade.hpp>

#include <atomic>
#include <string>

namespace ds3ea::log
{
namespace
{
// Set by the add-on at load and cleared at unload; atomic so any thread can log while it changes
std::atomic<Sink> g_sink = nullptr;

/**
 * @param level Our log level.
 * @returns ReShade's matching log level.
 */
reshade::log::level to_reshade_level(Level level)
{
	switch (level)
	{
	case Level::error:
		return reshade::log::level::error;
	case Level::warning:
		return reshade::log::level::warning;
	case Level::info:
		return reshade::log::level::info;
	case Level::debug:
		return reshade::log::level::debug;
	}

	return reshade::log::level::info;
}
} // namespace

void set_sink(Sink sink)
{
	g_sink.store(sink);
}

void write_to_reshade(Level level, std::string_view message)
{
	reshade::log::message(to_reshade_level(level), std::string(message).c_str());
}

void write(Level level, std::string_view message)
{
	const Sink sink = g_sink.load();
	if (sink != nullptr)
		sink(level, message);
}
} // namespace ds3ea::log
