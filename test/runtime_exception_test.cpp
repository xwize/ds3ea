#include "error.hpp"
#include "runtime_exception.hpp"

#include <gtest/gtest.h>

#include <source_location>
#include <string>
#include <string_view>

namespace
{
TEST(RuntimeException, ThrowErrorCarriesCategoryAndMessage)
{
	bool caught = false;
	try
	{
		ds3ea::throw_error(ds3ea::ErrorCategory::unsupported, "game version {} not supported", "1.08");
	}
	catch (const ds3ea::RuntimeException& exception)
	{
		caught = true;
		EXPECT_EQ(exception.get_error().category, ds3ea::ErrorCategory::unsupported);
		EXPECT_EQ(exception.get_error().message, "game version 1.08 not supported");
		EXPECT_EQ(std::string(exception.what()), "game version 1.08 not supported");
	}

	EXPECT_TRUE(caught);
}

TEST(RuntimeException, ThrowErrorRecordsTheCallersLocation)
{
	const std::source_location here = std::source_location::current();
	bool caught = false;
	try
	{
		ds3ea::throw_error(ds3ea::ErrorCategory::internal, "broken");
	}
	catch (const ds3ea::RuntimeException& exception)
	{
		caught = true;
		EXPECT_EQ(std::string_view(exception.get_error().location.file_name()), here.file_name());
		EXPECT_EQ(exception.get_error().location.line(), here.line() + 4);
	}

	EXPECT_TRUE(caught);
}
} // namespace
