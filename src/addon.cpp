#include "addon.hpp"
#include "addon_info.hpp"
#include "ini_settings_store.hpp"

#include <fmt/format.h>

#include <reshade.hpp>

#include <cstdint>
#include <filesystem>
#include <string>

namespace ds3ea
{
namespace
{
constexpr const wchar_t* k_ini_file_name = L"ds3ea.ini";

/**
 * Finds the settings file, which lives next to the add-on's DLL like the legacy add-on's did.
 * @param hmod Handle of this add-on's DLL.
 * @returns The ini path, or just the file name (the game's working directory) if the DLL path can't be read.
 */
std::filesystem::path get_ini_path(HMODULE hmod)
{
	std::wstring module_path(MAX_PATH, L'\0');
	while (true)
	{
		const DWORD length = GetModuleFileNameW(hmod, module_path.data(), static_cast<DWORD>(module_path.size()));
		if (length == 0)
			return k_ini_file_name;

		// A full buffer means the path was truncated
		if (length < module_path.size())
		{
			module_path.resize(length);
			return std::filesystem::path(module_path).replace_filename(k_ini_file_name);
		}

		module_path.resize(module_path.size() * 2);
	}
}
} // namespace

struct Addon::Key
{
	explicit Key() = default;
};

Addon::Addon(HMODULE hmod, Key /*key*/)
	: m_module(hmod)
	, m_settings(get_ini_path(hmod))
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
