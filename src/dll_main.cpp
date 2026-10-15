#include "addon.hpp"
#include "addon_info.hpp"
#include "utils/boundary.hpp"

#include <Windows.h>

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
} // namespace

// NOLINTNEXTLINE(readability-identifier-naming): the name Windows calls
BOOL APIENTRY DllMain(HMODULE hmod, DWORD reason, LPVOID /*reserved*/)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
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
