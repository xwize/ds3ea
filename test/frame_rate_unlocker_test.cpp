#include "frame_rate_unlocker.hpp"

#include <gtest/gtest.h>

namespace
{
TEST(SprintSpeedFactors, MatchTheGameAt60Fps)
{
	const ds3ea::SprintSpeedFactors factors = ds3ea::compute_sprint_speed_factors(60.0f, 1.0f / 60.0f);

	EXPECT_FLOAT_EQ(factors.stuck_scale, 30.0f);
	EXPECT_FLOAT_EQ(factors.slowdown, 0.8f);
	EXPECT_FLOAT_EQ(factors.recovery, 1.2f);
}

TEST(SprintSpeedFactors, GiveSlackAbove90Fps)
{
	const ds3ea::SprintSpeedFactors factors = ds3ea::compute_sprint_speed_factors(120.0f, 1.0f / 120.0f);

	EXPECT_FLOAT_EQ(factors.stuck_scale, 72.0f);
	EXPECT_NEAR(factors.slowdown, 0.894427f, 1e-5f);
	EXPECT_NEAR(factors.recovery, 1.224745f, 1e-5f);
}

TEST(SprintSpeedFactors, UseTheCapWhenTheMeasuredTimeIsOutOfRange)
{
	const ds3ea::SprintSpeedFactors factors = ds3ea::compute_sprint_speed_factors(144.0f, 0.0f);

	EXPECT_FLOAT_EQ(factors.stuck_scale, 0.5f * 144.0f * 1.2f);
}

TEST(SprintSpeedFactors, ClampTheCapToTheShortestFrame)
{
	const ds3ea::SprintSpeedFactors factors = ds3ea::compute_sprint_speed_factors(1000.0f, 0.0f);

	EXPECT_FLOAT_EQ(factors.stuck_scale, 0.5f * 480.0f * 1.2f);
}
} // namespace
