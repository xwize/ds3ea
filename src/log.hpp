#pragma once

#include <fmt/format.h>

#include <string_view>
#include <utility>

/**
 * @file
 * fmt-formatted logging. Messages go to a sink: the add-on installs write_to_reshade() at load, and with no sink
 * installed (as in the tests) messages are dropped.
 */

namespace ds3ea::log
{
enum class Level
{
	error,   // Something failed and a feature or setting won't work
	warning, // Something unexpected that the add-on recovered from
	info,    // Normal progress worth a line in the log
	debug,   // Detail for developers
};

/** Receives each formatted message. Called from whichever thread logged, so it must be thread-safe. */
using Sink = void (*)(Level level, std::string_view message);

/**
 * Installs the sink that receives every message from now on.
 * @param sink The new sink, or nullptr to drop messages.
 */
void set_sink(Sink sink);

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
 * Formats and logs an error.
 * @param format fmt format string.
 * @param args Values for the format string.
 */
template <typename... Args>
void error(fmt::format_string<Args...> format, Args&&... args)
{
	write(Level::error, fmt::format(format, std::forward<Args>(args)...));
}

/**
 * Formats and logs a warning.
 * @param format fmt format string.
 * @param args Values for the format string.
 */
template <typename... Args>
void warning(fmt::format_string<Args...> format, Args&&... args)
{
	write(Level::warning, fmt::format(format, std::forward<Args>(args)...));
}

/**
 * Formats and logs an informational message.
 * @param format fmt format string.
 * @param args Values for the format string.
 */
template <typename... Args>
void info(fmt::format_string<Args...> format, Args&&... args)
{
	write(Level::info, fmt::format(format, std::forward<Args>(args)...));
}
} // namespace ds3ea::log
