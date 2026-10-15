#pragma once

#include <fmt/format.h>

#include <string_view>
#include <utility>

/**
 * @file
 * fmt-formatted logging for code that must not depend on ReShade, such as ds3ea_core, which the tests link without
 * it. Messages go to a sink: the add-on installs one that writes to ReShade's log; with no sink installed (as in the
 * tests) messages are dropped.
 */

namespace ds3ea
{
enum class LogLevel
{
	error,   // Something failed and a feature or setting won't work
	warning, // Something unexpected that the add-on recovered from
	info,    // Normal progress worth a line in the log
	debug,   // Detail for developers
};

/** Receives each formatted message. Called from whichever thread logged, so it must be thread-safe. */
using LogSink = void (*)(LogLevel level, std::string_view message);

/**
 * Installs the sink that receives every message from now on.
 * @param sink The new sink, or nullptr to drop messages.
 */
void set_log_sink(LogSink sink);

/**
 * Sends a message to the installed sink, if any.
 * @param level How serious the message is.
 * @param message The text, without a trailing line break.
 */
void write_log(LogLevel level, std::string_view message);

/**
 * Formats and logs an error.
 * @param format fmt format string.
 * @param args Values for the format string.
 */
template <typename... Args>
void log_error(fmt::format_string<Args...> format, Args&&... args)
{
	write_log(LogLevel::error, fmt::format(format, std::forward<Args>(args)...));
}

/**
 * Formats and logs a warning.
 * @param format fmt format string.
 * @param args Values for the format string.
 */
template <typename... Args>
void log_warning(fmt::format_string<Args...> format, Args&&... args)
{
	write_log(LogLevel::warning, fmt::format(format, std::forward<Args>(args)...));
}

/**
 * Formats and logs an informational message.
 * @param format fmt format string.
 * @param args Values for the format string.
 */
template <typename... Args>
void log_info(fmt::format_string<Args...> format, Args&&... args)
{
	write_log(LogLevel::info, fmt::format(format, std::forward<Args>(args)...));
}
} // namespace ds3ea
