#include "addon.hpp"
#include "addon_info.hpp"
#include "utils/boundary.hpp"
#include "utils/build_config.hpp"
#include "utils/leak_check.hpp"

#include <Windows.h>

#include <array>
#include <filesystem>
#include <memory>

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
// Created on process attach, destroyed on process detach; only touched under the loader lock
std::unique_ptr<ds3ea::Addon> g_addon;

/**
 * @param hmod The add-on's module.
 * @returns Where the add-on's Debug leak report goes: ds3ea_leaks.log next to the add-on.
 */
std::filesystem::path get_leak_report_path(HMODULE hmod)
{
	std::array<wchar_t, MAX_PATH> module_path = {};
	GetModuleFileNameW(hmod, module_path.data(), static_cast<DWORD>(module_path.size()));
	return std::filesystem::path(module_path.data()).replace_filename(L"ds3ea_leaks.log");
}
} // namespace

// NOLINTNEXTLINE(readability-identifier-naming): the name Windows calls
BOOL APIENTRY DllMain(HMODULE hmod, DWORD reason, LPVOID /*reserved*/)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		// Release compiles this too, with <crtdbg.h>'s calls as no-op macros, and then discards it
		if constexpr (ds3ea::k_debug_build)
			ds3ea::call_and_catch("leak check", [hmod] { ds3ea::enable_leak_check(get_leak_report_path(hmod)); });

		ds3ea::call_and_catch("DllMain", [hmod] { g_addon = ds3ea::Addon::create(hmod); });
		return g_addon != nullptr;
	case DLL_PROCESS_DETACH:
		g_addon.reset();
		break;
	default:
		break;
	}

	return TRUE;
}
