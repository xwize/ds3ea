#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace ds3ea
{
/**
 * The add-on's settings file (ds3ea.ini): typed values by section and key, read once at construction and written
 * back by save(). A missing file reads as defaults and save() creates it. Failures to read or write the file are
 * logged. Keys the code never touches are kept, so a save never drops another feature's (or a newer version's)
 * settings. Comments and blank lines are not kept, and section and key names are case-sensitive.
 *
 * Not thread-safe: use it from one thread at a time.
 */
class IniSettingsStore
{
public:
	/**
	 * Loads the file. A missing or unreadable file leaves the store empty, so every getter returns its default.
	 * @param path Path of the ini file; also where save() writes.
	 */
	explicit IniSettingsStore(std::filesystem::path path);

	/** Discards unsaved changes. */
	~IniSettingsStore();

	/** Not copyable or movable: one store per file. */
	IniSettingsStore(const IniSettingsStore&) = delete;
	IniSettingsStore& operator=(const IniSettingsStore&) = delete;
	IniSettingsStore(IniSettingsStore&&) = delete;
	IniSettingsStore& operator=(IniSettingsStore&&) = delete;

	/**
	 * Reads a float.
	 * @param section Section name, without brackets.
	 * @param key Key within the section.
	 * @param fallback Returned when the key is missing or its value isn't a finite number.
	 * @returns The stored value, or fallback.
	 */
	[[nodiscard]] float get_float_or(std::string_view section, std::string_view key, float fallback) const;

	/**
	 * Reads an integer.
	 * @param section Section name, without brackets.
	 * @param key Key within the section.
	 * @param fallback Returned when the key is missing or its value isn't a whole decimal number that fits an int.
	 * @returns The stored value, or fallback.
	 */
	[[nodiscard]] int get_int_or(std::string_view section, std::string_view key, int fallback) const;

	/**
	 * Reads a bool, written as true, false, 1 or 0.
	 * @param section Section name, without brackets.
	 * @param key Key within the section.
	 * @param fallback Returned when the key is missing or its value is none of those.
	 * @returns The stored value, or fallback.
	 */
	[[nodiscard]] bool get_bool_or(std::string_view section, std::string_view key, bool fallback) const;

	/**
	 * Reads a string, with surrounding whitespace removed.
	 * @param section Section name, without brackets.
	 * @param key Key within the section.
	 * @param fallback Returned when the key is missing.
	 * @returns The stored value (possibly empty), or fallback.
	 */
	[[nodiscard]] std::string get_string_or(std::string_view section, std::string_view key, std::string_view fallback) const;

	/**
	 * Stores a float, written with 3 decimals (e.g. 0.850).
	 * @param section Section name, without brackets.
	 * @param key Key within the section.
	 * @param value Value to store.
	 */
	void set_float(std::string_view section, std::string_view key, float value);

	/**
	 * Stores an integer.
	 * @param section Section name, without brackets.
	 * @param key Key within the section.
	 * @param value Value to store.
	 */
	void set_int(std::string_view section, std::string_view key, int value);

	/**
	 * Stores a bool, written as true or false.
	 * @param section Section name, without brackets.
	 * @param key Key within the section.
	 * @param value Value to store.
	 */
	void set_bool(std::string_view section, std::string_view key, bool value);

	/**
	 * Stores a string. It must not contain line breaks (asserted), and surrounding whitespace doesn't survive a save
	 * and reload.
	 * @param section Section name, without brackets.
	 * @param key Key within the section.
	 * @param value Value to store.
	 */
	void set_string(std::string_view section, std::string_view key, std::string_view value);

	/**
	 * Writes every section and key to the file, through a temporary file so a failed write leaves the old file intact.
	 * @returns True if the file was written.
	 */
	bool save() const;

	/** @returns The path given at construction. */
	[[nodiscard]] const std::filesystem::path& get_path() const;

private:
	class Impl;

	std::unique_ptr<Impl> m_impl;
};
} // namespace ds3ea
