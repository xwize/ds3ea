#include "addon.hpp"
#include "addon_info.hpp"

#include <Windows.h>

#include <memory>

/**
 * ReShade reads these exports for the Add-ons tab. The names are fixed by ReShade's add-on API, so they don't follow
 * our naming rules.
 */
extern "C" __declspec(dllexport) const char* NAME = ds3ea::k_addon_name;
extern "C" __declspec(dllexport) const char* DESCRIPTION = ds3ea::k_addon_description;

namespace
{
// Created on process attach, destroyed on process detach; only touched under the loader lock
std::unique_ptr<ds3ea::Addon> g_addon;
} // namespace

BOOL APIENTRY DllMain(HMODULE hmod, DWORD reason, LPVOID /*reserved*/)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		g_addon = ds3ea::Addon::create(hmod);
		return g_addon != nullptr;
	case DLL_PROCESS_DETACH:
		g_addon.reset();
		break;
	default:
		break;
	}

	return TRUE;
}
