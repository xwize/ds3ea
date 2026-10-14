#pragma once

/**
 * @file
 * Identity of the add-on as ReShade shows it in its Add-ons tab. Kept free of ReShade and Win32 so tests can use it.
 */

namespace ds3ea
{
	inline constexpr const char* k_addon_name = "DS3EA";
	inline constexpr const char* k_addon_description = "Dark Souls 3 Enhancement Add-on";
} // namespace ds3ea
