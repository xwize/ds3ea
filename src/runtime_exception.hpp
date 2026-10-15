#pragma once

#include "error.hpp"

#include <fmt/format.h>

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ds3ea
{
/**
 * Our one exception type: it carries an Error, and what() returns the Error's message. Catch it by const reference
 * and switch on get_error().category to handle some categories and rethrow (throw;) the rest.
 */
class RuntimeException : public std::runtime_error
{
public:
	/** @param error Why it failed and where it was raised. */
	explicit RuntimeException(Error error);

	/** @returns Why it failed and where it was raised. */
	const Error& get_error() const;

private:
	Error m_error;
};

/**
 * Throws a RuntimeException: throw_error(ErrorCategory::io, "cannot write {}", path);
 * @param category What kind of failure this is.
 * @param format fmt format string for the message, checked at compile time; records the caller's location.
 * @param args Values for the format string.
 */
template <typename... Args>
[[noreturn]] void throw_error(ErrorCategory category, FormatWithLocation<std::type_identity_t<Args>...> format,
	Args&&... args)
{
	throw RuntimeException(Error{ category, fmt::format(format.format, std::forward<Args>(args)...), format.location });
}
} // namespace ds3ea
