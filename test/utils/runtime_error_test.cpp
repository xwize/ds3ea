#include "utils/error.hpp"
#include "utils/runtime_error.hpp"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <source_location>
#include <string>
#include <string_view>

namespace
{
TEST(RuntimeError, CarriesCategoryAndMessage)
{
	bool caught = false;
	try
	{
		throw ds3ea::RuntimeError({ ds3ea::ErrorCategory::unsupported, fmt::format("version {} unsupported", "1.08") });
	}
	catch (const ds3ea::RuntimeError& exception)
	{
		caught = true;
		EXPECT_EQ(exception.get_error().category, ds3ea::ErrorCategory::unsupported);
		EXPECT_EQ(exception.get_error().message, "version 1.08 unsupported");
		EXPECT_EQ(std::string(exception.what()), "version 1.08 unsupported");
	}

	EXPECT_TRUE(caught);
}

TEST(RuntimeError, RecordsWhereItWasThrown)
{
	const std::source_location here = std::source_location::current();
	bool caught = false;
	try
	{
		throw ds3ea::RuntimeError({ ds3ea::ErrorCategory::internal, "broken" });
	}
	catch (const ds3ea::RuntimeError& exception)
	{
		caught = true;
		EXPECT_EQ(std::string_view(exception.get_error().location.file_name()), here.file_name());
		EXPECT_EQ(exception.get_error().location.line(), here.line() + 4);
	}

	EXPECT_TRUE(caught);
}
} // namespace
