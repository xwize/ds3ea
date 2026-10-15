#include "error.hpp"
#include "result.hpp"
#include "runtime_exception.hpp"

#include <gtest/gtest.h>

#include <source_location>
#include <string>

namespace
{
ds3ea::Result<int> parse_positive(int value)
{
	if (value <= 0)
		return ds3ea::make_error(ds3ea::ErrorCategory::invalid_data, "{} is not positive", value);

	return value;
}

ds3ea::Result<void> check_positive(int value)
{
	if (value <= 0)
		return ds3ea::make_error(ds3ea::ErrorCategory::invalid_data, "{} is not positive", value);

	return {};
}

TEST(Result, SuccessHoldsTheValue)
{
	const ds3ea::Result<int> result = parse_positive(5);

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, 5);
}

TEST(Result, FailureHoldsCategoryAndMessage)
{
	const ds3ea::Result<int> result = parse_positive(-1);

	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().category, ds3ea::ErrorCategory::invalid_data);
	EXPECT_EQ(result.error().message, "-1 is not positive");
}

TEST(Result, VoidSucceedsAndFails)
{
	EXPECT_TRUE(check_positive(1).has_value());

	const ds3ea::Result<void> failed = check_positive(0);
	ASSERT_FALSE(failed.has_value());
	EXPECT_EQ(failed.error().message, "0 is not positive");
}

TEST(Result, ChainsWithAndThenAndMap)
{
	const auto describe = [](int value)
	{
		return parse_positive(value)
			.and_then([](int positive) { return parse_positive(positive - 10); })
			.map([](int remainder) { return std::to_string(remainder); });
	};

	EXPECT_EQ(describe(15).value_or(""), "5");
	ASSERT_FALSE(describe(3).has_value());
	EXPECT_EQ(describe(3).error().message, "-7 is not positive");
}

TEST(Result, GetOrThrowReturnsTheValue)
{
	EXPECT_EQ(ds3ea::get_or_throw(parse_positive(7)), 7);
	EXPECT_NO_THROW(ds3ea::get_or_throw(check_positive(7)));
}

TEST(Result, GetOrThrowThrowsTheSameErrorAndLocation)
{
	const std::source_location here = std::source_location::current();
	const ds3ea::Result<int> failed = ds3ea::make_error(ds3ea::ErrorCategory::not_found, "missing");
	bool caught = false;
	try
	{
		ds3ea::get_or_throw(failed);
	}
	catch (const ds3ea::RuntimeException& exception)
	{
		caught = true;
		EXPECT_EQ(exception.get_error().category, ds3ea::ErrorCategory::not_found);
		EXPECT_EQ(exception.get_error().message, "missing");
		EXPECT_EQ(exception.get_error().location.line(), here.line() + 1);
	}

	EXPECT_TRUE(caught);
}

TEST(Result, GetOrThrowThrowsForVoid)
{
	EXPECT_THROW(ds3ea::get_or_throw(check_positive(0)), ds3ea::RuntimeException);
}

TEST(Result, CapturePassesSuccessThrough)
{
	const ds3ea::Result<int> result = ds3ea::capture([] { return 3; });
	const ds3ea::Result<void> void_result = ds3ea::capture([] {});

	EXPECT_EQ(result.value_or(0), 3);
	EXPECT_TRUE(void_result.has_value());
}

TEST(Result, CaptureTurnsTheExceptionIntoTheSameErrorAndLocation)
{
	const std::source_location here = std::source_location::current();
	const ds3ea::Result<int> result = ds3ea::capture([]() -> int
		{ ds3ea::throw_error(ds3ea::ErrorCategory::device, "no device"); });
	const ds3ea::Result<void> void_result = ds3ea::capture([]
		{ ds3ea::throw_error(ds3ea::ErrorCategory::io, "no file"); });

	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().category, ds3ea::ErrorCategory::device);
	EXPECT_EQ(result.error().message, "no device");
	EXPECT_EQ(result.error().location.line(), here.line() + 2);
	ASSERT_FALSE(void_result.has_value());
	EXPECT_EQ(void_result.error().category, ds3ea::ErrorCategory::io);
}
} // namespace
