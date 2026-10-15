#include "utils/error.hpp"
#include "utils/result.hpp"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <source_location>
#include <string>
#include <string_view>

namespace
{
TEST(Error, MakeErrorFormatsTheMessage)
{
	const ds3ea::Result<int> failed =
		ds3ea::make_error(ds3ea::ErrorCategory::not_found, "cannot open {} ({})", "a.ini", 2);

	EXPECT_EQ(failed.error().category, ds3ea::ErrorCategory::not_found);
	EXPECT_EQ(failed.error().message, "cannot open a.ini (2)");
}

TEST(Error, MakeErrorRecordsTheCallersLocation)
{
	const std::source_location here = std::source_location::current();
	const ds3ea::Result<int> failed = ds3ea::make_error(ds3ea::ErrorCategory::io, "cannot write");

	EXPECT_EQ(std::string_view(failed.error().location.file_name()), here.file_name());
	EXPECT_EQ(failed.error().location.line(), here.line() + 1);
}

TEST(Error, FormatterShowsCategoryMessageAndFileLine)
{
	const std::source_location here = std::source_location::current();
	const ds3ea::Result<int> failed = ds3ea::make_error(ds3ea::ErrorCategory::not_found, "cannot open {}", "a.ini");

	const std::string expected = fmt::format("[not_found] cannot open a.ini (error_test.cpp:{})", here.line() + 1);
	EXPECT_EQ(fmt::format("{}", failed.error()), expected);
}
} // namespace
