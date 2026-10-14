#include "addon_info.hpp"

#include <gtest/gtest.h>

#include <string_view>

namespace
{
TEST(AddonInfo, NameIsDs3ea)
{
	EXPECT_EQ(std::string_view(ds3ea::k_addon_name), "DS3EA");
}

TEST(AddonInfo, DescriptionIsNotEmpty)
{
	EXPECT_FALSE(std::string_view(ds3ea::k_addon_description).empty());
}
} // namespace
