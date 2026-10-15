#pragma once

#include "utils/error.hpp"
#include "utils/runtime_error.hpp"

#include <tl/expected.hpp>

#include <type_traits>
#include <utility>

/**
 * @file
 * Result<T>, the way to report failure from hot or simple paths (see GUIDELINES.md), and get_or_throw() to turn a
 * failed one into a RuntimeError. Never call .value() on a Result: it throws tl::bad_expected_access, which our
 * boundary catches don't expect. Check and dereference it, chain it (and_then, map, or_else), or use get_or_throw().
 */

namespace ds3ea
{
/** A T, or the Error that stopped it being made. */
template <typename T>
using Result = tl::expected<T, Error>;

/**
 * Unwraps a Result, turning a failure into an exception that keeps the original Error and location.
 * @param result The Result to unwrap.
 * @returns The value (nothing for Result<void>).
 */
template <typename T>
T get_or_throw(Result<T> result)
{
	if (!result)
		throw RuntimeError(std::move(result.error()));

	if constexpr (!std::is_void_v<T>)
		return std::move(*result);
}
} // namespace ds3ea
