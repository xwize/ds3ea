/*
 * The patch is based on DarkSouls3RemoveIntroScreens (https://github.com/bladecoding/DarkSouls3RemoveIntroScreens),
 * the speedrun-legal "No-Logo" mod, under its MIT licence:
 *
 * Copyright (c) 2017 Bladecoding
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of
 * the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "game_image.hpp"
#include "intro_skipper.hpp"
#include "utils/build_config.hpp"
#include "utils/byte_pattern.hpp"
#include "utils/logging.hpp"
#include "utils/memory_patch.hpp"
#include "utils/result.hpp"

#include <fmt/format.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using namespace ds3ea;

/**
 * The game builds its intro as a chain of four calls that each append a screen. This is the last two, which append
 * the logos (call; nop; mov r8,r15; mov rdx,r12; mov rcx,rax; call), followed by the mov that tells this chain apart
 * from a near-identical one elsewhere in the game.
 */
constexpr std::string_view k_logo_calls_pattern =
	"E8 ?? ?? ?? ?? 90 4D 8B C7 49 8B D4 48 8B C8 E8 ?? ?? ?? ?? C7 44 24 50 01 00 00 00";

// The patch covers both calls, not the mov after them
constexpr std::size_t k_patch_size = 20;

/**
 * Drops both calls and writes the null screens they'd have returned into their output arguments, which the code after
 * them already handles: xor rax,rax; mov [rdx],rax; mov [r12],rax; then nops.
 */
constexpr std::array<std::byte, k_patch_size> k_skip_logos = {
	std::byte { 0x48 }, std::byte { 0x31 }, std::byte { 0xC0 }, std::byte { 0x48 }, std::byte { 0x89 },
	std::byte { 0x02 }, std::byte { 0x49 }, std::byte { 0x89 }, std::byte { 0x04 }, std::byte { 0x24 },
	std::byte { 0x90 }, std::byte { 0x90 }, std::byte { 0x90 }, std::byte { 0x90 }, std::byte { 0x90 },
	std::byte { 0x90 }, std::byte { 0x90 }, std::byte { 0x90 }, std::byte { 0x90 }, std::byte { 0x90 },
};

// Where the pattern is in 1.15.0, only to log whether this build has moved it
constexpr std::uintptr_t k_rva_1_15_0 = 0xBE9D0F;

/**
 * Finds the logo calls in the game's code.
 * @param image The game's exe as mapped in memory.
 * @returns The RVA of the 20 bytes to patch, or nothing (logged) if the pattern doesn't match exactly once.
 */
std::optional<std::size_t> find_logo_calls(std::span<const std::byte> image)
{
	const auto started = std::chrono::steady_clock::now();
	const std::vector<std::size_t> matches = find_in_code(image, parse_byte_pattern(k_logo_calls_pattern));
	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - started);

	if (matches.size() != 1)
	{
		log::warning("IntroSkipper: not skipping the intro, the logo code matched {} times instead of once",
			matches.size());
		return std::nullopt;
	}

	log::debug("IntroSkipper: logo code found at DarkSoulsIII.exe+{:#x} in {} ms{}", matches.front(),
		elapsed.count(), matches.front() == k_rva_1_15_0 ? "" : " (not where 1.15.0 has it)");
	return matches.front();
}

/**
 * Patches the game's intro, if this version can be patched.
 * @returns The patch, or nothing (logged) if the game was left alone.
 */
std::optional<MemoryPatch> patch_intro()
{
	const std::optional<GameVersion> version = read_game_version();

	if (!version || !is_supported_game_version())
	{
		log::warning("IntroSkipper: not skipping the intro, game version {} isn't 1.15.x",
			version ? fmt::format("{}.{}.{}.{}", version->major, version->minor, version->patch, version->build)
					: "unknown");
		return std::nullopt;
	}

	const std::span<std::byte> image = get_game_image();
	const std::optional<std::size_t> rva = find_logo_calls(image);

	if (!rva)
		return std::nullopt;

	// The pattern already checked the bytes, so the patch is written against what's there
	const std::span<std::byte> target = image.subspan(*rva, k_patch_size);
	const std::vector<std::byte> original(target.begin(), target.end());
	Result<MemoryPatch> patch = MemoryPatch::apply(target, original, k_skip_logos);

	if (!patch)
	{
		log::warning("IntroSkipper: not skipping the intro, {}", patch.error());
		return std::nullopt;
	}

	log::info("IntroSkipper: skipping the intro (game version {}.{}.{}.{})", version->major, version->minor,
		version->patch, version->build);
	return std::move(*patch);
}
} // namespace

namespace ds3ea
{
IntroSkipper::IntroSkipper()
{
	if constexpr (k_skip_intro_in_build)
		m_patch = patch_intro();
	else
		log::info("IntroSkipper: not skipping the intro, turned off in this build");
}
} // namespace ds3ea
