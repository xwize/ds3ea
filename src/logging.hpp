#pragma once

#include <fmt/format.h>

#include <string_view>
#include <utility>

/**
 * @file
 * fmt-formatted logging for the whole add-on, instead of calling reshade::log::message directly. Lines go to a sink:
 * the add-on installs write_to_reshade() at load, and with no sink installed (as in the tests) they are dropped.
 * This header doesn't include reshade.hpp; logging.cpp is the only logging code that does.
 */

namespace reshade::log
{
enum class level;
} // namespace reshade::log

namespace ds3ea::log
{
/** Our levels: ReShade's four plus trace, which ReShade lacks. */
enum class Level
{
	error,   // Something failed and a feature or setting won't work
	warning, // Something unexpected that the add-on recovered from
	info,    // Normal progress worth a line in the log
	debug,   // Detail for developers
	trace,   // Finer detail than debug, e.g. per draw
};

/** Receives each line as ReShade gets it. Called from whichever thread logged, so it must be thread-safe. */
using Sink = void (*)(reshade::log::level level, std::string_view line);

/**
 * Installs the sink that receives every line from now on.
 * @param sink The new sink, or nullptr to drop lines.
 */
void set_sink(Sink sink);

/**
 * The add-on's sink: writes a line to ReShade's log. Only call it while ReShade has the add-on loaded.
 * @param level ReShade's level for the line.
 * @param line The text, without a trailing line break.
 */
void write_to_reshade(reshade::log::level level, std::string_view line);

/**
 * Maps a message onto ReShade's levels and sends it to the installed sink, if any.
 * @param level How serious the message is.
 * @param message The formatted text, without a trailing line break.
 */
void write(Level level, std::string_view message);

/**
 * Formats with fmt and logs one line.
 * @param level How serious the message is.
 * @param format fmt format string, checked at compile time.
 * @param args Values for the format string.
 */
template <typename... Args>
void message(Level level, fmt::format_string<Args...> format, Args&&... args)
{
	write(level, fmt::format(format, std::forward<Args>(args)...));
}

/**
 * Formats and logs an error.
 * @param format fmt format string, checked at compile time.
 * @param args Values for the format string.
 */
template <typename... Args>
void error(fmt::format_string<Args...> format, Args&&... args)
{
	message(Level::error, format, std::forward<Args>(args)...);
}

/**
 * Formats and logs a warning.
 * @param format fmt format string, checked at compile time.
 * @param args Values for the format string.
 */
template <typename... Args>
void warning(fmt::format_string<Args...> format, Args&&... args)
{
	message(Level::warning, format, std::forward<Args>(args)...);
}

/**
 * Formats and logs an informational message.
 * @param format fmt format string, checked at compile time.
 * @param args Values for the format string.
 */
template <typename... Args>
void info(fmt::format_string<Args...> format, Args&&... args)
{
	message(Level::info, format, std::forward<Args>(args)...);
}

/**
 * Formats and logs a debug message.
 * @param format fmt format string, checked at compile time.
 * @param args Values for the format string.
 */
template <typename... Args>
void debug(fmt::format_string<Args...> format, Args&&... args)
{
	message(Level::debug, format, std::forward<Args>(args)...);
}

/**
 * Formats and logs a trace message.
 * @param format fmt format string, checked at compile time.
 * @param args Values for the format string.
 */
template <typename... Args>
void trace(fmt::format_string<Args...> format, Args&&... args)
{
	message(Level::trace, format, std::forward<Args>(args)...);
}

/**
 * A message logged once, for call sites that would otherwise flood the log every frame. Declare it as a static
 * local: its constructor logs, and C++ constructs a static local only the first time control reaches it, so later
 * passes neither format nor log. That is thread-safe: when several threads arrive at once, one constructs it and
 * the others wait.
 *
 *     static const log::Once missing_shader(log::Level::warning, "tonemap shader {} not found", name);
 */
class Once
{
public:
	/**
	 * Formats and logs one line.
	 * @param level How serious the message is.
	 * @param format fmt format string, checked at compile time.
	 * @param args Values for the format string.
	 */
	template <typename... Args>
	Once(Level level, fmt::format_string<Args...> format, Args&&... args)
	{
		message(level, format, std::forward<Args>(args)...);
	}

	/** Not copyable or movable: each one stands for one call site. */
	Once(const Once&) = delete;
	Once& operator=(const Once&) = delete;
	Once(Once&&) = delete;
	Once& operator=(Once&&) = delete;
};
} // namespace ds3ea::log
