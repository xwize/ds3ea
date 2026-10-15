#pragma once

#include "error.hpp"
#include "runtime_exception.hpp"

#include <tl/expected.hpp>

#include <functional>
#include <type_traits>
#include <utility>

/**
 * @file
 * Result<T>, the way to report failure from hot or simple paths (see GUIDELINES.md), and the bridges between it and
 * RuntimeException. Never call .value() on a Result: it throws tl::bad_expected_access, which our boundary catches
 * don't expect. Check and dereference it, chain it (and_then, map, or_else), or use get_or_throw().
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
		throw RuntimeException(std::move(result.error()));

	if constexpr (!std::is_void_v<T>)
		return std::move(*result);
}

/**
 * Runs func and turns a RuntimeException it throws into a failed Result with the same Error and location.
 * @param func The work to run; any other exception passes through.
 * @returns What func returned, or the Error it threw.
 */
template <typename Func>
Result<std::invoke_result_t<Func>> capture(Func&& func)
{
	try
	{
		if constexpr (std::is_void_v<std::invoke_result_t<Func>>)
		{
			std::invoke(std::forward<Func>(func));
			return {};
		}
		else
		{
			return std::invoke(std::forward<Func>(func));
		}
	}
	catch (const RuntimeException& exception)
	{
		return tl::unexpected<Error>(exception.get_error());
	}
}
} // namespace ds3ea
