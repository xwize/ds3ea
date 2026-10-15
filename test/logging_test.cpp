#include "logging.hpp"

#include <gtest/gtest.h>

#include <reshade.hpp>

#include <latch>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
/** One logged message, as the test sink received it. */
struct LoggedMessage
{
	ds3ea::log::Level level; // How serious it was
	std::string text;        // The formatted message
};

// Filled by capture_log, which threads may call at once; guarded by g_logged_mutex
std::vector<LoggedMessage> g_logged;
std::mutex g_logged_mutex;

void capture_log(ds3ea::log::Level level, std::string_view message)
{
	const std::lock_guard lock(g_logged_mutex);
	g_logged.push_back({ level, std::string(message) });
}

/** Captures everything logged while a test runs. */
class LoggingTest : public testing::Test
{
protected:
	void SetUp() override
	{
		g_logged.clear();
		ds3ea::log::set_sink(&capture_log);
	}

	void TearDown() override
	{
		ds3ea::log::set_sink(nullptr);
	}
};

TEST(LoggingReshadeLine, TraceIsDebugWithPrefix)
{
	const ds3ea::log::ReshadeLine line = ds3ea::log::to_reshade_line(ds3ea::log::Level::trace, "draw 12");

	EXPECT_EQ(line.level, reshade::log::level::debug);
	EXPECT_EQ(line.text, "T: draw 12");
}

TEST(LoggingReshadeLine, OtherLevelsMapOneToOneWithoutPrefix)
{
	EXPECT_EQ(ds3ea::log::to_reshade_line(ds3ea::log::Level::error, "text").level, reshade::log::level::error);
	EXPECT_EQ(ds3ea::log::to_reshade_line(ds3ea::log::Level::warning, "text").level, reshade::log::level::warning);
	EXPECT_EQ(ds3ea::log::to_reshade_line(ds3ea::log::Level::info, "text").level, reshade::log::level::info);
	EXPECT_EQ(ds3ea::log::to_reshade_line(ds3ea::log::Level::debug, "text").level, reshade::log::level::debug);

	for (const ds3ea::log::Level level :
		{ ds3ea::log::Level::error, ds3ea::log::Level::warning, ds3ea::log::Level::info, ds3ea::log::Level::debug })
		EXPECT_EQ(ds3ea::log::to_reshade_line(level, "text").text, "text");
}

TEST_F(LoggingTest, MessageFormatsWithFmt)
{
	ds3ea::log::message(ds3ea::log::Level::info, "{} hello world (device api {:#x})", "DS3EA", 0xb000u);
	ds3ea::log::trace("draw {}", 3);

	ASSERT_EQ(g_logged.size(), 2u);
	EXPECT_EQ(g_logged.at(0).level, ds3ea::log::Level::info);
	EXPECT_EQ(g_logged.at(0).text, "DS3EA hello world (device api 0xb000)");
	EXPECT_EQ(g_logged.at(1).level, ds3ea::log::Level::trace);
	EXPECT_EQ(g_logged.at(1).text, "draw 3");
}

TEST_F(LoggingTest, NoSinkDropsMessages)
{
	ds3ea::log::set_sink(nullptr);
	ds3ea::log::error("dropped");

	EXPECT_TRUE(g_logged.empty());
}

TEST_F(LoggingTest, OnceLogsFirstCallOnly)
{
	ds3ea::log::Once once;
	for (int call = 0; call < 3; ++call)
		once.message(ds3ea::log::Level::warning, "shader {} not found", call);

	ASSERT_EQ(g_logged.size(), 1u);
	EXPECT_EQ(g_logged.at(0).level, ds3ea::log::Level::warning);
	EXPECT_EQ(g_logged.at(0).text, "shader 0 not found");
}

TEST_F(LoggingTest, OnceLogsOnceAcrossThreads)
{
	constexpr int k_thread_count = 16;
	ds3ea::log::Once once;
	std::latch start(k_thread_count);
	std::vector<std::thread> threads;
	for (int index = 0; index < k_thread_count; ++index)
	{
		threads.emplace_back([&once, &start, index]
		{
			start.arrive_and_wait(); // Release every thread together so they race
			once.message(ds3ea::log::Level::info, "thread {}", index);
		});
	}

	for (std::thread& thread : threads)
		thread.join();

	EXPECT_EQ(g_logged.size(), 1u);
}
} // namespace
