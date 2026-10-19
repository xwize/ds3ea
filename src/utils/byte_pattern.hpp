#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ds3ea
{
/**
 * Bytes to look for in memory, where an empty entry matches any byte. Code signatures use them to find a function or
 * instruction whose address differs between game builds: the parts that vary between builds, such as call
 * displacements, are left as wildcards.
 */
using BytePattern = std::vector<std::optional<std::byte>>;

/**
 * Parses a pattern written as hex bytes separated by spaces, with ?? for any byte, e.g. "E8 ?? ?? ?? ?? 90".
 * Patterns are written in our code, so malformed text is a bug and asserts.
 * @param text The pattern.
 * @returns The parsed pattern.
 */
[[nodiscard]] BytePattern parse_byte_pattern(std::string_view text);

/**
 * Finds every place in memory where the pattern matches, overlapping matches included.
 * @param memory Where to look.
 * @param pattern What to look for; must not be empty or start with a wildcard.
 * @returns The offset of each match into memory, in order.
 */
[[nodiscard]] std::vector<std::size_t> find_byte_pattern(std::span<const std::byte> memory,
	const BytePattern& pattern);
} // namespace ds3ea
