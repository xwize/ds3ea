#pragma once

#include "settings.hpp"
#include "utils/memory_patch.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace ds3ea
{
/**
 * The constants the game's sprint "stuck" check multiplies by, which assume 60 fps. Above about 80 fps the
 * character moves too little per frame to pass the check, so sprinting slows down.
 */
struct SprintSpeedFactors
{
	// Distance per frame times this must reach 1 (the game's 30.0, i.e. 0.5 / (1/60))
	float stuck_scale = 0.0f;
	// Per-frame slowdown while stuck (the game's 0.8)
	float slowdown = 0.0f;
	// Per-frame recovery once moving again (the game's 1.2)
	float recovery = 0.0f;
};

/**
 * Scales the sprint check's 60 fps constants to a frame time, as DS3DebugFPS does: the per-frame rates become
 * exponents of the frame time, and above 90 fps the check gets some slack and a faster recovery.
 * @param frame_rate_limit The cap in fps.
 * @param measured_frame_time The game's last frame time in seconds; clamped to 1/480..1/20, and replaced by
 *     1/frame_rate_limit when it's outside that range.
 * @returns The factors to use this frame.
 */
[[nodiscard]] SprintSpeedFactors compute_sprint_speed_factors(float frame_rate_limit, float measured_frame_time);

/**
 * Raises or removes the game's 60 fps cap by turning on its own debug frame limiter at a custom rate, in its frame
 * pacing object (SprjFlipper), and keeps sprinting at full speed above 60 fps. The object already exists when
 * ReShade loads the add-on. Based on 0dm's DS3DebugFPS. Turned on and off for the whole build by
 * k_unlock_frame_rate_in_build.
 */
class FrameRateUnlocker
{
public:
	/**
	 * Patches the game if the settings ask for a cap other than 60 and the game can be patched; logs the outcome.
	 * @param settings The add-on's settings, read once.
	 */
	explicit FrameRateUnlocker(const Settings& settings);

	/** Puts the sprint check's code back. The cap is left, since the game is exiting. */
	~FrameRateUnlocker();

	/** Not copyable or movable: Addon owns the one instance for the life of the add-on. */
	FrameRateUnlocker(const FrameRateUnlocker&) = delete;
	FrameRateUnlocker& operator=(const FrameRateUnlocker&) = delete;
	FrameRateUnlocker(FrameRateUnlocker&&) = delete;
	FrameRateUnlocker& operator=(FrameRateUnlocker&&) = delete;

	/** The game presented a frame: updates the sprint factors and, for the spike, logs the frame rate. */
	void on_present();

private:
	/** Frees the page the sprint check reads its factors from. */
	struct PageDeleter
	{
		void operator()(SprintSpeedFactors* page) const noexcept;
	};

	/** Frame times over one measuring window. */
	struct FrameStats
	{
		std::chrono::steady_clock::time_point started;
		std::chrono::steady_clock::time_point last_present;
		std::uint32_t frames = 0;
		std::chrono::steady_clock::duration longest {};
	};

	/** Finds SprjFlipper and writes the cap into it; logs the outcome. */
	void patch_frame_rate();

	/** Points the sprint check at factors we update each frame; logs the outcome. */
	void patch_sprint_check();

	/** Counts the frame and logs the window's frame rate and the player's speed. */
	void measure_frame();

	float m_frame_rate_limit;
	// The object, once patched
	std::uintptr_t m_flipper = 0;
	// RVA of the WorldChrMan global, only to measure the player's speed
	std::optional<std::size_t> m_world_chr_man_slot;
	// Declared before the patches that point the game at it, so it's freed after they're put back
	std::unique_ptr<SprintSpeedFactors, PageDeleter> m_sprint_factors;
	std::vector<MemoryPatch> m_sprint_patches;
	FrameStats m_stats;
	std::optional<std::array<float, 3>> m_last_position;
};
} // namespace ds3ea
