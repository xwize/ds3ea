#include "addon.hpp"
#include "addon_info.hpp"
#include "boundary.hpp"
#include "ini_settings_store.hpp"
#include "logging.hpp"

#include <reshade.hpp>

#include <cstdint>

namespace ds3ea
{
namespace
{
// Relative, so it resolves against the game's working directory
constexpr const wchar_t* k_ini_file_name = L"ds3ea.ini";
} // namespace

struct Addon::Key
{
	explicit Key() = default;
};

Addon::Addon(HMODULE hmod, Key /*key*/)
	: m_module(hmod)
	, m_settings(k_ini_file_name)
{
	reshade::register_event<reshade::addon_event::init_device>(&Addon::on_init_device);
}

Addon::~Addon()
{
	reshade::unregister_event<reshade::addon_event::init_device>(&Addon::on_init_device);
	log::set_sink(nullptr);
	reshade::unregister_addon(m_module);
}

std::unique_ptr<Addon> Addon::create(HMODULE hmod)
{
	// ReShade refuses a module it has already registered, so a second create() for the same DLL returns nullptr
	if (!reshade::register_addon(hmod))
		return nullptr;

	// Before construction, so loading the settings can already log
	log::set_sink(&log::write_to_reshade);
	std::unique_ptr<Addon> addon;
	call_at_boundary("add-on init", [&addon, hmod] { addon = std::make_unique<Addon>(hmod, Key()); });
	if (addon == nullptr)
	{
		log::set_sink(nullptr);
		reshade::unregister_addon(hmod);
	}

	return addon;
}

void Addon::on_init_device(reshade::api::device* device)
{
	call_at_boundary("init_device", [device]
	{
		const auto api = static_cast<std::uint32_t>(device->get_api());
		log::info("{} hello world (device api {:#x})", k_addon_name, api);
	});
}
} // namespace ds3ea
