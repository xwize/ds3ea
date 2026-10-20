#pragma once

#include "utils/ini_settings_store.hpp"

#include <mutex>

namespace ds3ea
{
/**
 * The add-on's runtime settings, loaded from and saved to an IniSettingsStore. It knows whether the values changed
 * since they were last loaded or saved, by comparing them with a copy of what was saved.
 *
 * Thread-safe: the overlay and reloads replace the values on the render thread while features read them from
 * whichever thread records a draw, so the values are behind a mutex. Readers take a copy with get(); writers edit a
 * copy and put it back with set().
 */
class Settings
{
public:
	/**
	 * Loads every setting from the store, with its default where the store doesn't have it.
	 * @param store The loaded settings file.
	 */
	explicit Settings(const IniSettingsStore& store);

	/**
	 * The Tonemapper's settings, in ds3ea.ini's [Tonemapper] section. The curve's defaults (tuned in game) look very
	 * close to the game's own tonemap, without its banding and black crush.
	 */
	struct TonemapperValues
	{
		static constexpr float k_default_exposure = 0.85f;
		static constexpr float k_default_white = 11.0f;
		static constexpr float k_default_toe = 0.60f;
		static constexpr float k_default_shoulder = 0.15f;

		// Replace the game's tonemap with ours
		bool replace_game_tonemap = true;
		// Input scale before the curve, on top of the game's own exposure
		float exposure = k_default_exposure;
		// Input level that maps to white (Hable's W)
		float white = k_default_white;
		// Toe strength (Hable's D): higher lifts and flattens the darks
		float toe = k_default_toe;
		// Shoulder strength (Hable's A): higher compresses the highlights sooner
		float shoulder = k_default_shoulder;
		// Bloom strength: 0 none, 1 the game's
		float bloom = 1.0f;
		// Triangular dither, which hides banding in the 8-bit output
		bool dither = true;
		// Apply the game's colour grading LUT, at full precision
		bool apply_lut = true;

		bool operator==(const TonemapperValues&) const = default;
	};

	/** The frame limiter's settings, in ds3ea.ini's [FrameLimiter] section. */
	struct FrameLimiterValues
	{
		static constexpr float k_vanilla_frame_rate_limit = 60.0f;

		// The game's frame rate cap in fps: 60 leaves the game's own cap alone, 0 removes it
		float frame_rate_limit = k_vanilla_frame_rate_limit;
		// Keep sprinting at full speed above 60 fps
		bool fix_sprint_slowdown = true;

		bool operator==(const FrameLimiterValues&) const = default;
	};

	/** Every setting the add-on changes at runtime, one struct per feature. Each field's default is its initialiser. */
	struct Values
	{
		// [Tonemapper]
		TonemapperValues tonemapper{};
		// [Display] Borderless: a borderless window over the monitor instead of exclusive full screen. Read once, at
		// add-on load.
		bool borderless = false;
		// [FrameLimiter]
		FrameLimiterValues frame_limiter{};

		bool operator==(const Values&) const = default;
	};

	/**
	 * Reads every setting from the store, keeping a setting's last saved value when the store doesn't have it, and
	 * marks the result as saved.
	 * @param store The loaded settings file.
	 */
	void load(const IniSettingsStore& store);

	/**
	 * Writes every setting into the store and saves the file. On success the values are marked as saved.
	 * @param store The settings file to write.
	 * @returns True if the file was saved.
	 */
	[[nodiscard]] bool save(IniSettingsStore& store);

	/**
	 * Debug builds' live editing: if someone else wrote the file since our last load or save, reloads it and loads
	 * the settings from it. The file wins over unsaved changes, which are dropped with a warning. A file that can't
	 * be read keeps the current values.
	 * @param store The settings file.
	 */
	void reload_if_changed_on_disk(IniSettingsStore& store);

	/** @returns A copy of the current values. */
	[[nodiscard]] Values get() const;

	/** @param values Replaces the current values. */
	void set(const Values& values);

	/** @returns True if the values differ from the ones last loaded or saved. */
	[[nodiscard]] bool has_unsaved_changes() const;

private:
	mutable std::mutex m_mutex;
	// Guarded by m_mutex
	Values m_values;
	// Guarded by m_mutex: the values as last loaded or saved
	Values m_saved;
};
} // namespace ds3ea
