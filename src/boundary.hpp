#pragma once

#include "error.hpp"
#include "logging.hpp"
#include "runtime_error.hpp"

#include <exception>
#include <functional>
#include <string_view>
#include <utility>

namespace ds3ea
{
/**
 * Runs func where our code is called from ReShade, D3D11, the game or Windows (an event callback, DllMain, add-on
 * init). Unwinding through their frames can crash or corrupt state, so no exception gets past this: each one is
 * logged and swallowed.
 * @param boundary What is calling us, for the log line, e.g. "init_device".
 * @param func The work to run.
 * @returns True if func returned normally, false if it threw.
 */
template <typename Func>
bool call_and_catch(std::string_view boundary, Func&& func) noexcept
{
	try
	{
		std::invoke(std::forward<Func>(func));
		return true;
	}
	catch (const RuntimeError& exception)
	{
		log::error("{}: {}", boundary, exception.get_error());
	}
	catch (const std::exception& exception)
	{
		log::error("{}: {}", boundary, exception.what());
	}
	catch (...)
	{
		log::error("{}: unknown exception", boundary);
	}

	return false;
}
} // namespace ds3ea
