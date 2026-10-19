#include "borderless_restyler.hpp"

#include <gtest/gtest.h>

#include <Windows.h>

#include <optional>

namespace
{
constexpr RECT k_monitor = { .left = 0, .top = 0, .right = 3840, .bottom = 2160 };

WINDOWPOS make_pos(int x, int y, int width, int height, UINT flags = 0)
{
	WINDOWPOS pos = {};
	pos.x = x;
	pos.y = y;
	pos.cx = width;
	pos.cy = height;
	pos.flags = flags;
	return pos;
}

TEST(KeepCoveringMonitor, PutsBackAResizeAndReturnsTheSizeAskedFor)
{
	WINDOWPOS pos = make_pos(0, 0, 1920, 1080, SWP_NOMOVE | SWP_NOZORDER);

	const std::optional<SIZE> requested = ds3ea::keep_covering_monitor(pos, k_monitor);

	EXPECT_TRUE(requested.has_value());
	EXPECT_EQ(requested.value_or(SIZE {}).cx, 1920);
	EXPECT_EQ(requested.value_or(SIZE {}).cy, 1080);
	EXPECT_EQ(pos.cx, 3840);
	EXPECT_EQ(pos.cy, 2160);
	EXPECT_EQ(pos.flags & (SWP_NOMOVE | SWP_NOSIZE), 0U);
}

TEST(KeepCoveringMonitor, PutsBackAMoveWithoutReportingASize)
{
	WINDOWPOS pos = make_pos(100, 50, 0, 0, SWP_NOSIZE);

	const std::optional<SIZE> requested = ds3ea::keep_covering_monitor(pos, k_monitor);

	EXPECT_FALSE(requested.has_value());
	EXPECT_EQ(pos.x, 0);
	EXPECT_EQ(pos.y, 0);
	EXPECT_EQ(pos.cx, 3840);
	EXPECT_EQ(pos.cy, 2160);
}

TEST(KeepCoveringMonitor, ReportsAResizeToTheMonitorsSize)
{
	WINDOWPOS pos = make_pos(0, 0, 3840, 2160, SWP_NOMOVE);

	const std::optional<SIZE> requested = ds3ea::keep_covering_monitor(pos, k_monitor);

	EXPECT_EQ(requested.value_or(SIZE {}).cx, 3840);
	EXPECT_EQ(pos.flags, static_cast<UINT>(SWP_NOMOVE));
}

TEST(KeepCoveringMonitor, CoversAMonitorThatIsntAtTheOrigin)
{
	const RECT second_monitor = { .left = -1920, .top = 0, .right = 0, .bottom = 1080 };
	WINDOWPOS pos = make_pos(-1900, 20, 800, 600);

	static_cast<void>(ds3ea::keep_covering_monitor(pos, second_monitor));

	EXPECT_EQ(pos.x, -1920);
	EXPECT_EQ(pos.y, 0);
	EXPECT_EQ(pos.cx, 1920);
	EXPECT_EQ(pos.cy, 1080);
}

TEST(KeepCoveringMonitor, LeavesMinimisingAndZOrderChangesAlone)
{
	WINDOWPOS minimise = make_pos(-32000, -32000, 160, 28);
	WINDOWPOS z_order = make_pos(5, 5, 10, 10, SWP_NOMOVE | SWP_NOSIZE);

	EXPECT_FALSE(ds3ea::keep_covering_monitor(minimise, k_monitor).has_value());
	EXPECT_FALSE(ds3ea::keep_covering_monitor(z_order, k_monitor).has_value());
	EXPECT_EQ(minimise.x, -32000);
	EXPECT_EQ(z_order.x, 5);
}
} // namespace
