#include "addon.hpp"
#include "addon_info.hpp"
#include "utils/boundary.hpp"
#include "utils/build_config.hpp"
#include "utils/error.hpp"
#include "utils/runtime_error.hpp"

#include <fmt/format.h>

#include <Windows.h>
#include <crtdbg.h>

#include <memory>
#include <system_error>

/**
 * ReShade reads these exports for the Add-ons tab. The names are fixed by ReShade's add-on API, so they don't follow
 * our naming rules.
 */
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" __declspec(dllexport) const char* NAME = ds3ea::k_addon_name;
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" __declspec(dllexport) const char* DESCRIPTION = ds3ea::k_addon_description;

namespace
{
using namespace ds3ea;

// Created on process attach, destroyed on process detach; only touched under the loader lock
std::unique_ptr<Addon> g_addon;

/**
 * Turns on the debug CRT's heap tracking and its leak report, written to ds3ea_leaks.log in the game's working
 * directory when the add-on unloads. We link the CRT statically, so it sees only our own heap, not ReShade's or the
 * game's.
 *
 * The file is emptied here, and the CRT lists the leaks in it on DLL_PROCESS_DETACH, after our static objects are
 * destroyed. That needs a normal exit (Alt+F4 ends in ExitProcess): if the game is killed (TerminateProcess), no
 * detach runs and the file stays empty too. So an empty report means "no leaks" only after a normal exit.
 * @throws RuntimeError (io) if the file can't be created; leaks then go to the debugger output, the CRT's default.
 * maybe_unused: in Release the only call is in a discarded if constexpr branch.
 */
[[maybe_unused]] void enable_leak_check()
{
	// A Win32 handle rather than a stream or our logger: the report is written during CRT shutdown, when those may be
	// gone. It is never closed, since the CRT writes to it after everything of ours has been destroyed.
	const HANDLE report = CreateFileW(L"ds3ea_leaks.log", GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);

	if (report == INVALID_HANDLE_VALUE)
	{
		const std::error_code error(static_cast<int>(GetLastError()), std::system_category());
		throw RuntimeError({
			.category = ErrorCategory::io,
			.message = fmt::format("cannot create ds3ea_leaks.log: {}", error.message()),
		});
	}

	// The leak list is reported as _CRT_WARN; _CRT_ERROR and _CRT_ASSERT keep their usual reporting
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, report);
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
}
} // namespace

// NOLINTNEXTLINE(readability-identifier-naming): the name Windows calls
BOOL APIENTRY DllMain(HMODULE hmod, DWORD reason, LPVOID /*reserved*/)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		// First, so start-up allocations are tracked too. Release compiles this, with <crtdbg.h>'s calls as no-op
		// macros, and then discards it.
		if constexpr (k_debug_build)
			call_and_catch("leak check", enable_leak_check);

		call_and_catch("DllMain", [hmod] { g_addon = Addon::create(hmod); });
		return g_addon != nullptr;
	case DLL_PROCESS_DETACH:
		g_addon.reset();
		break;
	default:
		break;
	}

	return TRUE;
}
