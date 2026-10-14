#include "addon.hpp"
#include "addon_info.hpp"

#include <fmt/format.h>

#include <reshade.hpp>

#include <cstdint>
#include <string>

namespace ds3ea
{
struct Addon::Key
{
	explicit Key() = default;
};

Addon::Addon(HMODULE hmod, Key /*key*/)
	: m_module(hmod)
{
	reshade::register_event<reshade::addon_event::init_device>(&Addon::on_init_device);
}

Addon::~Addon()
{
	reshade::unregister_event<reshade::addon_event::init_device>(&Addon::on_init_device);
	reshade::unregister_addon(m_module);
}

std::unique_ptr<Addon> Addon::create(HMODULE hmod)
{
	// ReShade refuses a module it has already registered, so a second create() for the same DLL returns nullptr
	if (!reshade::register_addon(hmod))
		return nullptr;

	return std::make_unique<Addon>(hmod, Key());
}

void Addon::on_init_device(reshade::api::device* device)
{
	const std::string message = fmt::format("{} hello world (device api {:#x})", k_addon_name,
		static_cast<std::uint32_t>(device->get_api()));
	reshade::log::message(reshade::log::level::info, message.c_str());
}
} // namespace ds3ea
