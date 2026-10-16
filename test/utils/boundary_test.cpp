#include "utils/boundary.hpp"
#include "utils/error.hpp"
#include "utils/logging.hpp"
#include "utils/runtime_error.hpp"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <reshade.hpp>

#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
/** One line, as the test sink received it. */
struct LoggedLine
{
	// ReShade's level for the line
	reshade::log::level level;
	// The line as ReShade would get it
	std::string text;
};

// Filled by capture_line while a test runs; tests run on one thread
std::vector<LoggedLine> g_logged;

void capture_line(reshade::log::level level, std::string_view line)
{
	g_logged.push_back({ .level = level, .text = std::string(line) });
}

/** Captures every line logged while a test runs. */
class CallAndCatchTest : public testing::Test
{
protected:
	void SetUp() override
	{
		g_logged.clear();
		ds3ea::log::set_sink(&capture_line);
	}

	void TearDown() override
	{
		ds3ea::log::set_sink(nullptr);
	}
};

TEST_F(CallAndCatchTest, ReturnsTrueWhenFuncReturns)
{
	bool ran = false;

	EXPECT_TRUE(ds3ea::call_and_catch("test", [&ran] { ran = true; }));
	EXPECT_TRUE(ran);
	EXPECT_TRUE(g_logged.empty());
}

TEST_F(CallAndCatchTest, LogsRuntimeErrorWithCategoryAndLocation)
{
	const std::source_location here = std::source_location::current();
	const bool returned = ds3ea::call_and_catch("init_device",
		[] { throw ds3ea::RuntimeError({ .category = ds3ea::ErrorCategory::device, .message = "no device" }); });

	EXPECT_FALSE(returned);
	ASSERT_EQ(g_logged.size(), 1u);
	EXPECT_EQ(g_logged.at(0).level, reshade::log::level::error);
	EXPECT_EQ(g_logged.at(0).text,
		fmt::format("init_device: [device] no device (boundary_test.cpp:{})", here.line() + 2));
}

TEST_F(CallAndCatchTest, LogsStandardExceptionMessage)
{
	EXPECT_FALSE(ds3ea::call_and_catch("test", [] { throw std::out_of_range("index 9"); }));
	ASSERT_EQ(g_logged.size(), 1u);
	EXPECT_EQ(g_logged.at(0).text, "test: index 9");
}

TEST_F(CallAndCatchTest, LogsUnknownException)
{
	EXPECT_FALSE(ds3ea::call_and_catch("test", [] { throw 42; }));
	ASSERT_EQ(g_logged.size(), 1u);
	EXPECT_EQ(g_logged.at(0).text, "test: unknown exception");
}
} // namespace
