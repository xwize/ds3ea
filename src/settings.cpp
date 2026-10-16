#include "settings.hpp"
#include "utils/ini_settings_store.hpp"
#include "utils/logging.hpp"

#include <fmt/std.h>

#include <mutex>
#include <string_view>

namespace
{
using namespace ds3ea;

/**
 * The ini section and key of every setting, written once for both loading and saving.
 * @param values The settings (const when saving).
 * @param field Called with the section, the key and a reference to the field.
 */
template <typename Values, typename Field>
void for_each_setting(Values& values, const Field& field)
{
	field("Tonemapper", "ReplaceGameTonemap", values.tonemapper.replace_game_tonemap);
}

/**
 * @param store The settings file.
 * @param section Section name.
 * @param key Key within the section.
 * @param value Set from the file; kept if the file doesn't have it.
 */
void load_value(const IniSettingsStore& store, std::string_view section, std::string_view key, bool& value)
{
	value = store.get_bool_or(section, key, value);
}

/**
 * @param store The settings file.
 * @param section Section name.
 * @param key Key within the section.
 * @param value Written to the file.
 */
void save_value(IniSettingsStore& store, std::string_view section, std::string_view key, bool value)
{
	store.set_bool(section, key, value);
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
	SettingsValues values = m_saved;
	for_each_setting(values, [&store](std::string_view section, std::string_view key, auto& value)
		{ load_value(store, section, key, value); });
	m_values = values;
	m_saved = values;
}

bool Settings::save(IniSettingsStore& store)
{
	const SettingsValues values = get();
	for_each_setting(values, [&store](std::string_view section, std::string_view key, auto value)
		{ save_value(store, section, key, value); });

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

SettingsValues Settings::get() const
{
	const std::lock_guard lock(m_mutex);
	return m_values;
}

void Settings::set(const SettingsValues& values)
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
