#include "ini_settings_store.hpp"

#include <fmt/format.h>
#include <inipp.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <optional>
#include <system_error>
#include <type_traits>
#include <utility>

namespace ds3ea
{
namespace
{
using Ini = inipp::Ini<char>;

constexpr std::string_view k_utf8_bom = "\xEF\xBB\xBF";

/**
 * Parses the whole of text as a decimal number.
 * @param text The value as read from the file.
 * @returns The number, or nothing if text has anything else in it, is out of range or (for floats) isn't finite.
 */
template <typename T>
std::optional<T> parse_number(const std::string& text)
{
	T value = {};
	const char* const last = text.data() + text.size();
	const auto [end, error] = std::from_chars(text.data(), last, value);
	if (error != std::errc() || end != last)
		return std::nullopt;

	if constexpr (std::is_floating_point_v<T>)
	{
		if (!std::isfinite(value))
			return std::nullopt;
	}

	return value;
}

/**
 * Parses a bool the way the legacy add-on wrote it (an integer) or as true/false in any case.
 * @param text The value as read from the file.
 * @returns The bool, or nothing if text is neither.
 */
std::optional<bool> parse_bool(const std::string& text)
{
	if (const std::optional<int> number = parse_number<int>(text))
		return *number != 0;

	std::string lower = text;
	std::transform(lower.begin(), lower.end(), lower.begin(),
		[](unsigned char chr) { return static_cast<char>(std::tolower(chr)); });
	if (lower == "true")
		return true;

	if (lower == "false")
		return false;

	return std::nullopt;
}

/**
 * Skips a UTF-8 byte order mark, which Notepad may add, so it doesn't become part of the first line.
 * @param file The freshly opened file.
 */
void skip_bom(std::ifstream& file)
{
	std::string head(k_utf8_bom.size(), '\0');
	file.read(head.data(), static_cast<std::streamsize>(head.size()));
	if (file && head == k_utf8_bom)
		return;

	file.clear();
	file.seekg(0);
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
			return;

		skip_bom(file);
		m_ini.parse(file);
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
			return false;

		std::error_code error;
		std::filesystem::rename(temp_path, m_path, error);
		if (!error)
			return true;

		std::filesystem::remove(temp_path, error);
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

float IniSettingsStore::get_float(std::string_view section, std::string_view key, float fallback) const
{
	const std::string* const text = m_impl->find(section, key);
	return text != nullptr ? parse_number<float>(*text).value_or(fallback) : fallback;
}

int IniSettingsStore::get_int(std::string_view section, std::string_view key, int fallback) const
{
	const std::string* const text = m_impl->find(section, key);
	return text != nullptr ? parse_number<int>(*text).value_or(fallback) : fallback;
}

bool IniSettingsStore::get_bool(std::string_view section, std::string_view key, bool fallback) const
{
	const std::string* const text = m_impl->find(section, key);
	return text != nullptr ? parse_bool(*text).value_or(fallback) : fallback;
}

std::string IniSettingsStore::get_string(std::string_view section, std::string_view key,
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
	m_impl->set(section, key, value ? "1" : "0");
}

void IniSettingsStore::set_string(std::string_view section, std::string_view key, std::string_view value)
{
	// A line break would end the line on save and turn the rest of the value into a line of its own
	std::string line(value);
	std::replace_if(line.begin(), line.end(), [](char chr) { return chr == '\r' || chr == '\n'; }, ' ');
	m_impl->set(section, key, std::move(line));
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
