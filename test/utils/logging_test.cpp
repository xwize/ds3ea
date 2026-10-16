#include "utils/logging.hpp"

#include <gtest/gtest.h>

#include <reshade.hpp>

#include <array>
#include <cstddef>
#include <latch>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
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

// Filled by capture_line, which threads may call at once; guarded by g_logged_mutex
std::vector<LoggedLine> g_logged;
std::mutex g_logged_mutex;

void capture_line(reshade::log::level level, std::string_view line)
{
	const std::lock_guard lock(g_logged_mutex);
	g_logged.push_back({ .level = level, .text = std::string(line) });
}

/** Captures every line logged while a test runs. */
class LoggingTest : public testing::Test
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

TEST_F(LoggingTest, TraceIsDebugWithPrefix)
{
	ds3ea::log::trace("draw {}", 12);

	ASSERT_EQ(g_logged.size(), 1u);
	EXPECT_EQ(g_logged.at(0).level, reshade::log::level::debug);
	EXPECT_EQ(g_logged.at(0).text, "T: draw 12");
}

TEST_F(LoggingTest, OtherLevelsMapOneToOneWithoutPrefix)
{
	const std::array<std::pair<ds3ea::log::Level, reshade::log::level>, 4> levels = { {
		{ ds3ea::log::Level::error, reshade::log::level::error },
		{ ds3ea::log::Level::warning, reshade::log::level::warning },
		{ ds3ea::log::Level::info, reshade::log::level::info },
		{ ds3ea::log::Level::debug, reshade::log::level::debug },
	} };

	for (const auto& entry : levels)
		ds3ea::log::message(entry.first, "text");

	ASSERT_EQ(g_logged.size(), levels.size());
	for (std::size_t index = 0; index < levels.size(); ++index)
	{
		EXPECT_EQ(g_logged.at(index).level, levels.at(index).second);
		EXPECT_EQ(g_logged.at(index).text, "text");
	}
}

TEST_F(LoggingTest, MessageFormatsWithFmt)
{
	ds3ea::log::message(ds3ea::log::Level::info, "{} loaded (api {:#x})", "DS3EA", 0xb000u);

	ASSERT_EQ(g_logged.size(), 1u);
	EXPECT_EQ(g_logged.at(0).text, "DS3EA loaded (api 0xb000)");
}

TEST_F(LoggingTest, NoSinkDropsMessages)
{
	ds3ea::log::set_sink(nullptr);
	ds3ea::log::error("dropped");

	EXPECT_TRUE(g_logged.empty());
}

TEST_F(LoggingTest, OnceLogsFirstPassOnly)
{
	const auto log_missing = [](int call)
	{
		static const ds3ea::log::Once missing_shader(ds3ea::log::Level::warning, "shader {} not found", call);
	};
	for (int call = 0; call < 3; ++call)
		log_missing(call);

	ASSERT_EQ(g_logged.size(), 1u);
	EXPECT_EQ(g_logged.at(0).level, reshade::log::level::warning);
	EXPECT_EQ(g_logged.at(0).text, "shader 0 not found");
}

TEST_F(LoggingTest, OnceLogsOnceAcrossThreads)
{
	constexpr int k_thread_count = 16;
	std::latch start(k_thread_count);
	std::vector<std::thread> threads;
	threads.reserve(k_thread_count);

	for (int index = 0; index < k_thread_count; ++index)
	{
		threads.emplace_back([&start, index]
		{
			// Release every thread together so they race
			start.arrive_and_wait();
			static const ds3ea::log::Once once(ds3ea::log::Level::info, "thread {}", index);
		});
	}

	for (std::thread& thread : threads)
		thread.join();

	EXPECT_EQ(g_logged.size(), 1u);
}
} // namespace
