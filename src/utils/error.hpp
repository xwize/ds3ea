#pragma once

#include <fmt/format.h>
#include <tl/expected.hpp>

#include <concepts>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

/**
 * @file
 * The one description of a failure that both ways of reporting it share: a failed Result<T> (result.hpp) and a
 * RuntimeError (runtime_error.hpp). Callers branch on its category the same way in both, and every logged
 * error says where it was raised.
 */

namespace ds3ea
{
/** What kind of failure this is, chosen by what a caller would do about it, not by where it happened. */
enum class ErrorCategory
{
	not_found,    // A file, shader, pattern or resource doesn't exist: fall back or skip it
	invalid_data, // It exists but can't be parsed or used: fall back to defaults
	unsupported,  // Game version, hardware or configuration we don't handle: disable the feature
	io,           // Reading or writing failed: report it and carry on without it
	device,       // D3D11 / GPU object creation or use failed: drop the GPU resources
	internal,     // Anything else that isn't a programmer error
};

/**
 * @param category The category to name.
 * @returns The category's name for logging, e.g. "not_found".
 */
[[nodiscard]] std::string_view get_category_name(ErrorCategory category);

/**
 * Why an operation failed, and where. A default member initializer's std::source_location::current() records where
 * the Error is initialised, so throw RuntimeError({ ErrorCategory::io, fmt::format("cannot write {}", path) });
 * records the throw.
 */
struct Error
{
	// What kind of failure, for callers that branch on it
	ErrorCategory category;
	// What went wrong, with context (file, shader name...)
	std::string message;
	// Where the Error was initialised
	std::source_location location = std::source_location::current();
};

/**
 * A fmt format string that also records where it was written. std::source_location::current() can't follow a
 * parameter pack as a default argument, so it defaults this constructor's argument instead; the constructor is
 * consteval like fmt::format_string's, which keeps fmt's compile-time format check.
 */
template <typename... Args>
struct FormatWithLocation
{
	/**
	 * @param text The format string.
	 * @param location Where the caller wrote it; leave it to the default.
	 */
	template <typename String>
		requires std::convertible_to<const String&, std::string_view>
	consteval explicit FormatWithLocation(const String& text,
			std::source_location location = std::source_location::current())
		: format(text)
		, location(location)
	{
	}

	fmt::format_string<Args...> format; // The checked format string
	std::source_location location;      // Where the error was raised
};

/**
 * Builds a failed Result: return make_error(ErrorCategory::not_found, "cannot open {}", path);
 * @param category What kind of failure this is.
 * @param format fmt format string for the message, checked at compile time; records the caller's location.
 * @param args Values for the format string.
 * @returns The Error, ready to return as a failed Result.
 */
template <typename... Args>
tl::unexpected<Error> make_error(ErrorCategory category, FormatWithLocation<std::type_identity_t<Args>...> format,
	Args&&... args)
{
	return tl::unexpected<Error>(
		Error{ category, fmt::format(format.format, std::forward<Args>(args)...), format.location });
}
} // namespace ds3ea

/** Formats an Error for the log: "[not_found] cannot open ds3ea.ini (ini_settings_store.cpp:42)". */
template <>
struct fmt::formatter<ds3ea::Error>
{
	/**
	 * Accepts only an empty format spec ("{}").
	 * @param ctx The format spec after the colon.
	 * @returns The end of the parsed spec.
	 */
	constexpr fmt::format_parse_context::iterator parse(fmt::format_parse_context& ctx) const
	{
		return ctx.begin();
	}

	/**
	 * @param error The error to write.
	 * @param ctx Where to write it.
	 * @returns The output position after the error.
	 */
	fmt::format_context::iterator format(const ds3ea::Error& error, fmt::format_context& ctx) const;
};
