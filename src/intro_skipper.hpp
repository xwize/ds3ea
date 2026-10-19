#pragma once

#include "utils/memory_patch.hpp"

#include <optional>

namespace ds3ea
{
/**
 * Patches the game so it starts at the title screen, and returns to it after a quit, without the Bandai Namco and
 * FromSoftware logos. The game queues its intro screens about 6 seconds after it creates its D3D11 device, which is
 * when ReShade loads the add-on, so patching on construction is in time. On a game version other than 1.15.x, or when
 * the code to patch can't be found exactly once, it logs why and leaves the game alone. Turned on and off for the
 * whole build by k_skip_intro_in_build.
 */
class IntroSkipper
{
public:
	/** Patches the game, if this build skips the intro and the game can be patched; logs the outcome. */
	IntroSkipper();

	/** Puts the game's code back. */
	~IntroSkipper() = default;

	/** Not copyable or movable: Addon owns the one instance for the life of the add-on. */
	IntroSkipper(const IntroSkipper&) = delete;
	IntroSkipper& operator=(const IntroSkipper&) = delete;
	IntroSkipper(IntroSkipper&&) = delete;
	IntroSkipper& operator=(IntroSkipper&&) = delete;

private:
	// Empty when the game wasn't patched
	std::optional<MemoryPatch> m_patch;
};
} // namespace ds3ea
