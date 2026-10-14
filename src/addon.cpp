#include "addon.hpp"
#include "addon_info.hpp"

#include <reshade.hpp>

#include <fmt/format.h>

#include <cstdint>
#include <string>

namespace ds3ea
{
	std::unique_ptr<Addon> Addon::create(HMODULE module)
	{
		if (!reshade::register_addon(module))
			return nullptr;

		return std::make_unique<Addon>(module, Key());
	}

	Addon::Addon(HMODULE module, Key /*key*/) : m_module(module)
	{
		reshade::register_event<reshade::addon_event::init_device>(&Addon::on_init_device);
	}

	Addon::~Addon()
	{
		reshade::unregister_event<reshade::addon_event::init_device>(&Addon::on_init_device);
		reshade::unregister_addon(m_module);
	}

	void Addon::on_init_device(reshade::api::device* device)
	{
		const std::string message = fmt::format("{} hello world (device api {:#x})", k_addon_name,
			static_cast<std::uint32_t>(device->get_api()));
		reshade::log::message(reshade::log::level::info, message.c_str());
	}
} // namespace ds3ea
