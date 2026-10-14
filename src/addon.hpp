#pragma once

#include <Windows.h>

#include <memory>

namespace reshade::api
{
	struct device;
} // namespace reshade::api

namespace ds3ea
{
	/**
	 * The add-on's registration with ReShade. Constructing one through create() registers the add-on and its event
	 * callbacks; destroying it unregisters them. DllMain owns the single instance for the life of the module.
	 */
	class Addon
	{
		/** Passkey that limits construction to create(), while still letting it use std::make_unique. */
		struct Key
		{
			explicit Key() = default;
		};

	public:
		/**
		 * Registers the add-on and its event callbacks with ReShade.
		 * @param module Handle of this add-on's DLL.
		 * @returns The registered add-on, or nullptr if ReShade is missing or rejected the add-on API version.
		 */
		static std::unique_ptr<Addon> create(HMODULE module);

		/**
		 * Registers the event callbacks. Only create() can call it, once ReShade has accepted the add-on.
		 * @param module Handle of this add-on's DLL.
		 * @param key Passkey only create() can make.
		 */
		Addon(HMODULE module, Key key);

		/** Unregisters the event callbacks and the add-on. */
		~Addon();

		/** Not copyable or movable: the instance owns the ReShade registration. */
		Addon(const Addon&) = delete;
		Addon& operator=(const Addon&) = delete;
		Addon(Addon&&) = delete;
		Addon& operator=(Addon&&) = delete;

	private:
		/**
		 * Logs the hello-world line once ReShade has created a device.
		 * @param device The device ReShade just initialised.
		 */
		static void on_init_device(reshade::api::device* device);

		HMODULE m_module;
	};
} // namespace ds3ea
