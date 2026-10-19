#include "utils/byte_pattern.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
template <std::size_t Size>
std::array<std::byte, Size> to_bytes(const std::array<std::uint8_t, Size>& values)
{
	std::array<std::byte, Size> bytes = {};
	for (std::size_t i = 0; i < Size; ++i)
		bytes.at(i) = static_cast<std::byte>(values.at(i));

	return bytes;
}

TEST(BytePattern, ParsesHexBytesAndWildcards)
{
	const ds3ea::BytePattern pattern = ds3ea::parse_byte_pattern("E8 ?? 0d  90");

	const ds3ea::BytePattern expected = { std::byte { 0xE8 }, std::nullopt, std::byte { 0x0D }, std::byte { 0x90 } };
	EXPECT_EQ(pattern, expected);
}

TEST(BytePattern, WildcardsMatchAnyByte)
{
	const auto memory = to_bytes<8>({ 0x00, 0xE8, 0x11, 0x22, 0x90, 0xE8, 0x33, 0x44 });

	EXPECT_EQ(ds3ea::find_byte_pattern(memory, ds3ea::parse_byte_pattern("E8 ?? ??")),
		(std::vector<std::size_t> { 1, 5 }));
}

TEST(BytePattern, FixedBytesMustMatch)
{
	const auto memory = to_bytes<6>({ 0xE8, 0x11, 0x90, 0xE8, 0x11, 0x91 });

	EXPECT_EQ(ds3ea::find_byte_pattern(memory, ds3ea::parse_byte_pattern("E8 ?? 91")),
		(std::vector<std::size_t> { 3 }));
}

TEST(BytePattern, FindsOverlappingMatches)
{
	const auto memory = to_bytes<4>({ 0xAA, 0xAA, 0xAA, 0xAA });

	EXPECT_EQ(ds3ea::find_byte_pattern(memory, ds3ea::parse_byte_pattern("AA AA")),
		(std::vector<std::size_t> { 0, 1, 2 }));
}

TEST(BytePattern, MatchAtTheVeryEndIsFound)
{
	const auto memory = to_bytes<4>({ 0x00, 0x00, 0xE8, 0x01 });

	EXPECT_EQ(ds3ea::find_byte_pattern(memory, ds3ea::parse_byte_pattern("E8 ??")),
		(std::vector<std::size_t> { 2 }));
}

TEST(BytePattern, PatternLongerThanMemoryFindsNothing)
{
	const auto memory = to_bytes<2>({ 0xE8, 0x00 });

	EXPECT_TRUE(ds3ea::find_byte_pattern(memory, ds3ea::parse_byte_pattern("E8 00 00")).empty());
}
} // namespace
