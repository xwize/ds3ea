#include "boundary.hpp"
#include "error.hpp"
#include "logging.hpp"
#include "runtime_exception.hpp"

#include <gtest/gtest.h>

#include <reshade.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
// Filled by capture_line while a test runs; tests run on one thread
std::vector<std::string> g_logged;

void capture_line(reshade::log::level /*level*/, std::string_view line)
{
	g_logged.emplace_back(line);
}

/** Captures every line logged while a test runs. */
class BoundaryTest : public testing::Test
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

TEST_F(BoundaryTest, ReturnsTrueWhenNothingThrows)
{
	bool ran = false;

	EXPECT_TRUE(ds3ea::call_at_boundary("test", [&ran] { ran = true; }));
	EXPECT_TRUE(ran);
	EXPECT_TRUE(g_logged.empty());
}

TEST_F(BoundaryTest, LogsOurExceptionWithItsError)
{
	EXPECT_FALSE(ds3ea::call_at_boundary("init_device", []
		{ ds3ea::throw_error(ds3ea::ErrorCategory::device, "no swap chain"); }));

	ASSERT_EQ(g_logged.size(), 1u);
	EXPECT_EQ(g_logged.at(0).find("init_device: [device] no swap chain (boundary_test.cpp:"), 0u) << g_logged.at(0);
}

TEST_F(BoundaryTest, LogsOtherExceptionsWithWhat)
{
	EXPECT_FALSE(ds3ea::call_at_boundary("present", [] { throw std::out_of_range("bad index"); }));

	ASSERT_EQ(g_logged.size(), 1u);
	EXPECT_EQ(g_logged.at(0), "present: bad index");
}

TEST_F(BoundaryTest, LogsUnknownExceptions)
{
	EXPECT_FALSE(ds3ea::call_at_boundary("present", [] { throw 42; }));

	ASSERT_EQ(g_logged.size(), 1u);
	EXPECT_EQ(g_logged.at(0), "present: unknown exception");
}
} // namespace
