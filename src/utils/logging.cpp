#include "utils/logging.hpp"

#include <reshade.hpp>

#include <atomic>
#include <string>
#include <utility>

namespace
{
using namespace ds3ea;

// Set by the add-on at load and cleared at unload; atomic so any thread can log while it changes
std::atomic<log::Sink> g_sink = nullptr;

/**
 * @param level How serious the message is.
 * @param message The formatted text.
 * @returns ReShade's level and the line to write.
 */
[[nodiscard]] std::pair<reshade::log::level, std::string> to_reshade_line(log::Level level, std::string_view message)
{
	switch (level)
	{
	case log::Level::error:
		return { reshade::log::level::error, std::string(message) };
	case log::Level::warning:
		return { reshade::log::level::warning, std::string(message) };
	case log::Level::info:
		return { reshade::log::level::info, std::string(message) };
	case log::Level::debug:
		return { reshade::log::level::debug, std::string(message) };
	case log::Level::trace:
		return { reshade::log::level::debug, fmt::format("T: {}", message) };
	}

	return { reshade::log::level::info, std::string(message) };
}
} // namespace

namespace ds3ea::log
{
void set_sink(Sink sink) noexcept
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
