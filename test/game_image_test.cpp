#include "game_image.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

namespace
{
/** A fake image with a 32-bit displacement written at an offset. */
std::array<std::byte, 64> make_image(std::size_t offset, std::int32_t displacement)
{
	std::array<std::byte, 64> image = {};
	std::memcpy(&image.at(offset), &displacement, sizeof(displacement));
	return image;
}

TEST(ResolveRipRelative, AddsTheDisplacementToTheInstructionEnd)
{
	const auto image = make_image(3, 20);

	EXPECT_EQ(ds3ea::resolve_rip_relative(image, 3, 7), std::optional<std::size_t>(27));
}

TEST(ResolveRipRelative, HandlesNegativeDisplacements)
{
	const auto image = make_image(10, -10);

	EXPECT_EQ(ds3ea::resolve_rip_relative(image, 10, 14), std::optional<std::size_t>(4));
}

TEST(ResolveRipRelative, RefusesATargetOutsideTheImage)
{
	EXPECT_EQ(ds3ea::resolve_rip_relative(make_image(3, 100), 3, 7), std::nullopt);
	EXPECT_EQ(ds3ea::resolve_rip_relative(make_image(3, -8), 3, 7), std::nullopt);
}

TEST(ResolveRipRelative, RefusesADisplacementPastTheEnd)
{
	EXPECT_EQ(ds3ea::resolve_rip_relative(make_image(0, 0), 62, 66), std::nullopt);
}
} // namespace
