#include "utils/logging.hpp"

#include <reshade.hpp>

#include <atomic>
#include <string>
#include <utility>

namespace ds3ea::log
{
namespace
{
// Set by the add-on at load and cleared at unload; atomic so any thread can log while it changes
std::atomic<Sink> g_sink = nullptr;

/**
 * @param level How serious the message is.
 * @param message The formatted text.
 * @returns ReShade's level and the line to write.
 */
std::pair<reshade::log::level, std::string> to_reshade_line(Level level, std::string_view message)
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
} // namespace

void set_sink(Sink sink)
{
	g_sink.store(sink);
}

void write_to_reshade(reshade::log::level level, std::string_view line)
{
	reshade::log::message(level, std::string(line).c_str());
}

void write(Level level, std::string_view message)
{
	const Sink sink = g_sink.load();
	if (sink == nullptr)
		return;

	const auto [reshade_level, line] = to_reshade_line(level, message);
	sink(reshade_level, line);
}
} // namespace ds3ea::log
