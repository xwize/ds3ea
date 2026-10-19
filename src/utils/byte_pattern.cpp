#include "utils/byte_pattern.hpp"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace ds3ea
{
BytePattern parse_byte_pattern(std::string_view text)
{
	BytePattern pattern;
	std::size_t next = text.find_first_not_of(' ');

	while (next != std::string_view::npos)
	{
		const std::size_t end = std::min(text.find(' ', next), text.size());
		const std::string_view token = text.substr(next, end - next);

		if (token == "??")
		{
			pattern.emplace_back();
		}
		else
		{
			std::uint8_t value = 0;
			const char* const token_begin = std::to_address(token.begin());
			const char* const token_end = std::to_address(token.end());
			[[maybe_unused]] const auto [last, error] = std::from_chars(token_begin, token_end, value, 16);
			assert(error == std::errc() && last == token_end && token.size() == 2 &&
				"Bug: a byte pattern token is not two hex digits or ??");
			pattern.emplace_back(static_cast<std::byte>(value));
		}

		next = text.find_first_not_of(' ', end);
	}

	return pattern;
}

std::vector<std::size_t> find_byte_pattern(std::span<const std::byte> memory, const BytePattern& pattern)
{
	assert(!pattern.empty() && pattern.front().has_value() && "Bug: a byte pattern must start with a fixed byte");

	std::vector<std::size_t> offsets;

	if (memory.size() < pattern.size())
		return offsets;

	// Where a match can start. Searching for the first byte keeps a scan of the game's code to milliseconds.
	const std::span<const std::byte> starts = memory.first(memory.size() - pattern.size() + 1);
	for (auto start = std::ranges::find(starts, *pattern.front()); start != starts.end();
		 start = std::find(start + 1, starts.end(), *pattern.front()))
	{
		const auto offset = static_cast<std::size_t>(start - starts.begin());
		const bool is_match = std::ranges::equal(pattern, memory.subspan(offset, pattern.size()),
			[](std::optional<std::byte> wanted, std::byte actual) { return !wanted || *wanted == actual; });

		if (is_match)
			offsets.push_back(offset);
	}

	return offsets;
}
} // namespace ds3ea
