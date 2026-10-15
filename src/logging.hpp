#pragma once

#include <fmt/format.h>

#include <atomic>
#include <string>
#include <string_view>
#include <utility>

/**
 * @file
 * fmt-formatted logging for the whole add-on, instead of calling reshade::log::message directly. Messages go to a
 * sink: the add-on installs write_to_reshade() at load, and with no sink installed (as in the tests) messages are
 * dropped. This header doesn't include reshade.hpp; logging.cpp is the only logging code that does.
 */

namespace reshade::log
{
enum class level;
} // namespace reshade::log

namespace ds3ea::log
{
/** Our levels: ReShade's four plus trace, which ReShade lacks (written as debug with a "T: " prefix). */
enum class Level
{
	error,   // Something failed and a feature or setting won't work
	warning, // Something unexpected that the add-on recovered from
	info,    // Normal progress worth a line in the log
	debug,   // Detail for developers
	trace,   // Finer detail than debug, e.g. per draw
};

/** Receives each formatted message. Called from whichever thread logged, so it must be thread-safe. */
using Sink = void (*)(Level level, std::string_view message);

/** A message as ReShade receives it: its level and the text with any prefix for our extra levels. */
struct ReshadeLine
{
	reshade::log::level level; // ReShade's level (trace becomes debug)
	std::string text;          // The message, with "T: " in front for trace
};

/**
 * Installs the sink that receives every message from now on.
 * @param sink The new sink, or nullptr to drop messages.
 */
void set_sink(Sink sink);

/**
 * Turns one of our messages into what ReShade gets: levels map 1:1 except trace, which becomes debug with a "T: "
 * prefix.
 * @param level How serious the message is.
 * @param message The formatted text.
 * @returns ReShade's level and the line to write.
 */
ReshadeLine to_reshade_line(Level level, std::string_view message);

/**
 * The add-on's sink: forwards a message to ReShade's log. Only call it while ReShade has the add-on loaded.
 * @param level How serious the message is.
 * @param message The text, without a trailing line break.
 */
void write_to_reshade(Level level, std::string_view message);

/**
 * Sends a message to the installed sink, if any.
 * @param level How serious the message is.
 * @param message The text, without a trailing line break.
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
 * Logs its message the first time only; later calls do nothing, without formatting. Thread-safe: when several
 * threads call it at once, exactly one logs. For messages that would otherwise flood the log every frame:
 *
 *     static log::Once missing_shader;
 *     missing_shader.message(log::Level::warning, "tonemap shader {} not found", name);
 */
class Once
{
public:
	/**
	 * Formats and logs one line, unless this object has logged before.
	 * @param level How serious the message is.
	 * @param format fmt format string, checked at compile time.
	 * @param args Values for the format string.
	 */
	template <typename... Args>
	void message(Level level, fmt::format_string<Args...> format, Args&&... args)
	{
		if (m_logged.exchange(true))
			return;

		log::message(level, format, std::forward<Args>(args)...);
	}

private:
	std::atomic<bool> m_logged = false;
};
} // namespace ds3ea::log
