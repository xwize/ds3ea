#include "settings.hpp"
#include "utils/ini_settings_store.hpp"
#include "utils/logging.hpp"

#include <fmt/std.h>

#include <mutex>
#include <string_view>
#include <type_traits>

namespace
{
using namespace ds3ea;

/**
 * The one list of where each setting lives in the ini, shared by load() and save() so they can't drift apart. Calls
 * read_or_write(section, key, field) for every setting: load() passes a function that reads the field from the
 * store, save() one that writes it.
 * @param values The settings (const when saving).
 * @param read_or_write Reads or writes one field, given its section and key.
 */
template <typename Values, typename ReadOrWrite>
void for_each_setting(Values& values, const ReadOrWrite& read_or_write)
{
	read_or_write("Tonemapper", "ReplaceGameTonemap", values.tonemapper.replace_game_tonemap);
	read_or_write("Tonemapper", "Exposure", values.tonemapper.exposure);
	read_or_write("Tonemapper", "WhitePoint", values.tonemapper.white);
	read_or_write("Tonemapper", "Toe", values.tonemapper.toe);
	read_or_write("Tonemapper", "Shoulder", values.tonemapper.shoulder);
	read_or_write("Tonemapper", "Bloom", values.tonemapper.bloom);
	read_or_write("Tonemapper", "Dither", values.tonemapper.dither);
	read_or_write("Tonemapper", "ApplyLUT", values.tonemapper.apply_lut);
}
} // namespace

namespace ds3ea
{
Settings::Settings(const IniSettingsStore& store)
{
	load(store);
}

void Settings::load(const IniSettingsStore& store)
{
	const std::lock_guard lock(m_mutex);
	Values values = m_saved;
	for_each_setting(values, [&store](std::string_view section, std::string_view key, auto& value)
	{
		if constexpr (std::is_same_v<std::remove_cvref_t<decltype(value)>, bool>)
			value = store.get_bool_or(section, key, value);
		else
			value = store.get_float_or(section, key, value);
	});
	m_values = values;
	m_saved = values;
}

bool Settings::save(IniSettingsStore& store)
{
	const Values values = get();
	for_each_setting(values, [&store](std::string_view section, std::string_view key, auto value)
	{
		if constexpr (std::is_same_v<decltype(value), bool>)
			store.set_bool(section, key, value);
		else
			store.set_float(section, key, value);
	});

	if (!store.save())
		return false;

	const std::lock_guard lock(m_mutex);
	m_saved = values;
	return true;
}

void Settings::reload_if_changed_on_disk(IniSettingsStore& store)
{
	if (!store.is_changed_on_disk() || !store.reload())
		return;

	if (has_unsaved_changes())
		log::warning("{} changed on disk; dropping the settings changed since the last save", store.get_path());

	load(store);
	log::info("Reloaded settings from {}", store.get_path());
}

Settings::Values Settings::get() const
{
	const std::lock_guard lock(m_mutex);
	return m_values;
}

void Settings::set(const Values& values)
{
	const std::lock_guard lock(m_mutex);
	m_values = values;
}

bool Settings::has_unsaved_changes() const
{
	const std::lock_guard lock(m_mutex);
	return m_values != m_saved;
}
} // namespace ds3ea
