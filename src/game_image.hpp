#pragma once

#include "utils/byte_pattern.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ds3ea
{
/**
 * The game's exe as mapped in memory, and how features find code and globals in it. Addresses inside the image are
 * RVAs (offsets from its base), so the pure parts work on any buffer and can be tested against fake ones.
 */

/** The game's exe file version, e.g. 1.15.0.0. */
struct GameVersion
{
	std::uint16_t major = 0;
	std::uint16_t minor = 0;
	std::uint16_t patch = 0;
	std::uint16_t build = 0;
};

/** @returns The running exe's file version, or nothing if it has none. */
[[nodiscard]] std::optional<GameVersion> read_game_version();

/** @returns True if the running exe is 1.15.x, the only version our patches are known for. */
[[nodiscard]] bool is_supported_game_version();

/** @returns The game's exe as mapped in memory. */
[[nodiscard]] std::span<std::byte> get_game_image();

/**
 * Finds every match of a pattern in the image's executable sections.
 * @param image A module as mapped in memory.
 * @param pattern What to look for.
 * @returns The RVA of each match, in order.
 */
[[nodiscard]] std::vector<std::size_t> find_in_code(std::span<const std::byte> image, const BytePattern& pattern);

/**
 * Resolves an x64 RIP-relative operand: the 32-bit displacement at displacement_rva, relative to the end of its
 * instruction.
 * @param image A module as mapped in memory.
 * @param displacement_rva Where the displacement is.
 * @param instruction_end_rva Where the instruction that holds it ends.
 * @returns The RVA it points at, or nothing if the displacement or that RVA is outside the image.
 */
[[nodiscard]] std::optional<std::size_t> resolve_rip_relative(std::span<const std::byte> image,
	std::size_t displacement_rva, std::size_t instruction_end_rva);
} // namespace ds3ea
