#include "addon.hpp"
#include "addon_info.hpp"
#include "ini_settings_store.hpp"
#include "log.hpp"

#include <reshade.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace ds3ea
{
namespace
{
constexpr const wchar_t* k_ini_file_name = L"ds3ea.ini";

/**
 * Finds the settings file, which lives next to the add-on's DLL.
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

/**
 * The log sink while the add-on is loaded: forwards each message to ReShade's log.
 * @param level How serious the message is.
 * @param message The text.
 */
void log_to_reshade(LogLevel level, std::string_view message)
{
	reshade::log::level reshade_level = reshade::log::level::info;
	switch (level)
	{
	case LogLevel::error:
		reshade_level = reshade::log::level::error;
		break;
	case LogLevel::warning:
		reshade_level = reshade::log::level::warning;
		break;
	case LogLevel::info:
		reshade_level = reshade::log::level::info;
		break;
	case LogLevel::debug:
		reshade_level = reshade::log::level::debug;
		break;
	}

	reshade::log::message(reshade_level, std::string(message).c_str());
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
	set_log_sink(nullptr);
	reshade::unregister_addon(m_module);
}

std::unique_ptr<Addon> Addon::create(HMODULE hmod)
{
	// ReShade refuses a module it has already registered, so a second create() for the same DLL returns nullptr
	if (!reshade::register_addon(hmod))
		return nullptr;

	// Before construction, so loading the settings can already log
	set_log_sink(&log_to_reshade);
	return std::make_unique<Addon>(hmod, Key());
}

void Addon::on_init_device(reshade::api::device* device)
{
	log_info("{} hello world (device api {:#x})", k_addon_name, static_cast<std::uint32_t>(device->get_api()));
}
} // namespace ds3ea
