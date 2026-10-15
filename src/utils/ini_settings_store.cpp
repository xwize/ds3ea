#include "utils/ini_settings_store.hpp"
#include "utils/logging.hpp"

#include <fmt/format.h>
#include <fmt/std.h>
#include <inipp.h>

#include <cassert>
#include <fstream>
#include <system_error>
#include <utility>

namespace ds3ea
{
namespace
{
using Ini = inipp::Ini<char>;

/**
 * Converts a value with inipp, which rejects text that isn't wholly one value of type T.
 * @param text The value as read from the file, or nullptr if the key is missing.
 * @param fallback Returned when text is missing or isn't a T.
 * @returns The converted value, or fallback.
 */
template <typename T>
T extract_or(const std::string* text, T fallback)
{
	T value = fallback;
	if (text != nullptr)
		inipp::extract(*text, value); // Leaves value alone when it fails

	return value;
}
} // namespace

/** The parsed file behind IniSettingsStore, kept out of the header so only this file sees inipp. */
class IniSettingsStore::Impl
{
public:
	explicit Impl(std::filesystem::path path)
		: m_path(std::move(path))
	{
		std::ifstream file(m_path);
		if (!file)
		{
			std::error_code error;
			if (std::filesystem::exists(m_path, error))
				log::error("Can't open {}; using default settings", m_path);
			else
				log::info("{} doesn't exist yet; using default settings", m_path);

			return;
		}

		m_ini.parse(file);
		for (const std::string& line : m_ini.errors)
			log::warning("Ignoring line in {}: {}", m_path, line);
	}

	/**
	 * @param section Section name.
	 * @param key Key within the section.
	 * @returns The value, or nullptr if the section or key is missing.
	 */
	const std::string* find(std::string_view section, std::string_view key) const
	{
		const auto section_it = m_ini.sections.find(std::string(section));
		if (section_it == m_ini.sections.end())
			return nullptr;

		const auto key_it = section_it->second.find(std::string(key));
		return key_it != section_it->second.end() ? &key_it->second : nullptr;
	}

	/**
	 * @param section Section name; created if missing.
	 * @param key Key within the section; created if missing.
	 * @param value Replaces any existing value.
	 */
	void set(std::string_view section, std::string_view key, std::string value)
	{
		m_ini.sections[std::string(section)][std::string(key)] = std::move(value);
	}

	/** @returns True if the file was written and moved over the old one. */
	bool save() const
	{
		std::filesystem::path temp_path = m_path;
		temp_path += L".tmp";
		if (!write(temp_path))
		{
			log::error("Can't write {}; settings not saved", temp_path);
			return false;
		}

		std::error_code error;
		std::filesystem::rename(temp_path, m_path, error);
		if (!error)
			return true;

		log::error("Can't replace {} with {}: {}; settings not saved", m_path, temp_path, error.message());
		std::filesystem::remove(temp_path, error);
		if (error)
			log::warning("Can't remove {}: {}", temp_path, error.message());

		return false;
	}

	/** @returns The file this store loads from and saves to. */
	const std::filesystem::path& get_path() const
	{
		return m_path;
	}

private:
	/**
	 * Writes the sections in name order. Keys before any section header (section "") go first, without a header,
	 * rather than under the "[]" that inipp's generate() would write.
	 * @param path File to create or overwrite.
	 * @returns True if every byte was written.
	 */
	bool write(const std::filesystem::path& path) const
	{
		std::ofstream file(path, std::ios::trunc);
		for (const auto& [name, section] : m_ini.sections)
		{
			if (!name.empty())
				file << Ini::char_section_start << name << Ini::char_section_end << '\n';

			for (const auto& [key, value] : section)
				file << key << Ini::char_assign << value << '\n';

			file << '\n';
		}

		file.close();
		return !file.fail();
	}

	std::filesystem::path m_path;
	Ini m_ini;
};

IniSettingsStore::IniSettingsStore(std::filesystem::path path)
	: m_impl(std::make_unique<Impl>(std::move(path)))
{
}

IniSettingsStore::~IniSettingsStore() = default;

float IniSettingsStore::get_float_or(std::string_view section, std::string_view key, float fallback) const
{
	return extract_or(m_impl->find(section, key), fallback);
}

int IniSettingsStore::get_int_or(std::string_view section, std::string_view key, int fallback) const
{
	return extract_or(m_impl->find(section, key), fallback);
}

bool IniSettingsStore::get_bool_or(std::string_view section, std::string_view key, bool fallback) const
{
	// inipp reads only true and false, so 1 and 0 are handled here
	const std::string* const text = m_impl->find(section, key);
	if (text != nullptr && *text == "1")
		return true;

	if (text != nullptr && *text == "0")
		return false;

	return extract_or(text, fallback);
}

std::string IniSettingsStore::get_string_or(std::string_view section, std::string_view key,
	std::string_view fallback) const
{
	const std::string* const text = m_impl->find(section, key);
	return text != nullptr ? *text : std::string(fallback);
}

void IniSettingsStore::set_float(std::string_view section, std::string_view key, float value)
{
	m_impl->set(section, key, fmt::format("{:.3f}", value));
}

void IniSettingsStore::set_int(std::string_view section, std::string_view key, int value)
{
	m_impl->set(section, key, fmt::format("{}", value));
}

void IniSettingsStore::set_bool(std::string_view section, std::string_view key, bool value)
{
	m_impl->set(section, key, fmt::format("{}", value));
}

void IniSettingsStore::set_string(std::string_view section, std::string_view key, std::string_view value)
{
	assert(value.find_first_of("\r\n") == std::string_view::npos && "Bug: Putting text with newlines into the ini");
	m_impl->set(section, key, std::string(value));
}

bool IniSettingsStore::save() const
{
	return m_impl->save();
}

const std::filesystem::path& IniSettingsStore::get_path() const
{
	return m_impl->get_path();
}
} // namespace ds3ea
