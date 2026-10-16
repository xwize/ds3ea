#pragma once

#include "utils/ini_settings_store.hpp"

#include <Windows.h>

#include <memory>

namespace reshade::api
{
// NOLINTNEXTLINE(readability-identifier-naming, cppcoreguidelines-virtual-class-destructor): ReShade's type
struct device;
} // namespace reshade::api

namespace ds3ea
{
/**
 * The add-on's registration with ReShade and the owner of its services. Constructing one through create() registers
 * the add-on and its event callbacks and loads ds3ea.ini; destroying it unregisters them. DllMain owns the single
 * instance for the life of the module.
 */
class Addon
{
	/** Passkey that limits construction to create(), while still letting it use std::make_unique. */
	struct Key;

public:
	/**
	 * Loads ds3ea.ini from the game's working directory, registers the event callbacks and logs the start-up. Only
	 * create() can call it, once ReShade has accepted the add-on.
	 * @param hmod Handle of this add-on's DLL.
	 * @param key Passkey only create() can make.
	 */
	Addon(HMODULE hmod, Key key);

	/** Unregisters the event callbacks and the add-on. */
	~Addon();

	/** Not copyable or movable: the instance owns the ReShade registration. */
	Addon(const Addon&) = delete;
	Addon& operator=(const Addon&) = delete;
	Addon(Addon&&) = delete;
	Addon& operator=(Addon&&) = delete;

	/**
	 * Registers the add-on and its event callbacks with ReShade.
	 * @param hmod Handle of this add-on's DLL.
	 * @returns The registered add-on, or nullptr if ReShade is missing, rejected the add-on API version or already
	 *          has this module registered.
	 */
	[[nodiscard]] static std::unique_ptr<Addon> create(HMODULE hmod);

private:
	/**
	 * Called once ReShade has created a device.
	 * @param device The device ReShade just initialised.
	 */
	static void on_init_device(reshade::api::device* device);

	HMODULE m_module;
	IniSettingsStore m_settings;
};
} // namespace ds3ea
