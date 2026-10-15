#include "utils/ini_settings_store.hpp"
#include "utils/logging.hpp"

#include <gtest/gtest.h>

#include <reshade.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace
{
/** One logged message, as the test sink received it. */
struct LoggedMessage
{
	reshade::log::level level; // How serious it was
	std::string text;          // The line as ReShade would get it
};

// Filled by capture_log while a test runs; tests run on one thread
std::vector<LoggedMessage> g_logged;

void capture_log(reshade::log::level level, std::string_view message)
{
	g_logged.push_back({ level, std::string(message) });
}

/**
 * Gives each test its own empty folder under the temp directory and removes it afterwards, and captures what the
 * store logs.
 */
class IniSettingsStoreTest : public testing::Test
{
protected:
	void SetUp() override
	{
		const testing::TestInfo* const info = testing::UnitTest::GetInstance()->current_test_info();
		m_folder = std::filesystem::temp_directory_path() / "ds3ea_tests" / info->test_suite_name() / info->name();
		std::filesystem::remove_all(m_folder);
		std::filesystem::create_directories(m_folder);
		g_logged.clear();
		ds3ea::log::set_sink(&capture_log);
	}

	void TearDown() override
	{
		ds3ea::log::set_sink(nullptr);
		std::error_code error;
		std::filesystem::remove_all(m_folder, error);
	}

	std::filesystem::path get_ini_path() const
	{
		return m_folder / "ds3ea.ini";
	}

	void write_file(std::string_view text) const
	{
		std::ofstream file(get_ini_path(), std::ios::binary);
		file << text;
	}

	std::string read_file() const
	{
		std::ifstream file(get_ini_path());
		return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}

	static bool has_logged(reshade::log::level level)
	{
		for (const LoggedMessage& message : g_logged)
			if (message.level == level)
				return true;

		return false;
	}

	std::filesystem::path m_folder;
};

TEST_F(IniSettingsStoreTest, MissingFileReadsDefaults)
{
	const ds3ea::IniSettingsStore store(get_ini_path());

	EXPECT_FLOAT_EQ(store.get_float_or("Tonemapper", "Exposure", 0.85f), 0.85f);
	EXPECT_EQ(store.get_int_or("Display", "WindowWidth", 1920), 1920);
	EXPECT_TRUE(store.get_bool_or("DLSS", "Enabled", true));
	EXPECT_EQ(store.get_string_or("DLSS", "Mode", "Quality"), "Quality");
	EXPECT_FALSE(std::filesystem::exists(get_ini_path()));
	EXPECT_TRUE(has_logged(reshade::log::level::info));
	EXPECT_FALSE(has_logged(reshade::log::level::error));
}

TEST_F(IniSettingsStoreTest, MissingKeyOrSectionReadsDefault)
{
	write_file("[Tonemapper]\nExposure=0.500\n");
	const ds3ea::IniSettingsStore store(get_ini_path());

	EXPECT_FLOAT_EQ(store.get_float_or("Tonemapper", "WhitePoint", 11.0f), 11.0f);
	EXPECT_FLOAT_EQ(store.get_float_or("Other", "Exposure", 2.0f), 2.0f);
}

TEST_F(IniSettingsStoreTest, GarbageValuesReadDefault)
{
	write_file("[Test]\nFloat=abc\nTrailing=1.5x\nInfinite=inf\nInt=12x\nTooBig=99999999999\nFraction=1.5\n"
			   "Bool=maybe\nEmpty=\n");
	const ds3ea::IniSettingsStore store(get_ini_path());

	EXPECT_FLOAT_EQ(store.get_float_or("Test", "Float", 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(store.get_float_or("Test", "Trailing", 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(store.get_float_or("Test", "Infinite", 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(store.get_float_or("Test", "Empty", 1.0f), 1.0f);
	EXPECT_EQ(store.get_int_or("Test", "Int", 7), 7);
	EXPECT_EQ(store.get_int_or("Test", "TooBig", 7), 7);
	EXPECT_EQ(store.get_int_or("Test", "Fraction", 7), 7);
	EXPECT_EQ(store.get_int_or("Test", "Empty", 7), 7);
	EXPECT_TRUE(store.get_bool_or("Test", "Bool", true));
	EXPECT_FALSE(store.get_bool_or("Test", "Empty", false));
}

TEST_F(IniSettingsStoreTest, BoolsAcceptWordsAndDigits)
{
	write_file("[Test]\nTrue=true\nFalse=false\nOne=1\nZero=0\nTwo=2\nUpper=TRUE\n");
	const ds3ea::IniSettingsStore store(get_ini_path());

	EXPECT_TRUE(store.get_bool_or("Test", "True", false));
	EXPECT_FALSE(store.get_bool_or("Test", "False", true));
	EXPECT_TRUE(store.get_bool_or("Test", "One", false));
	EXPECT_FALSE(store.get_bool_or("Test", "Zero", true));
	EXPECT_FALSE(store.get_bool_or("Test", "Two", false));
	EXPECT_FALSE(store.get_bool_or("Test", "Upper", false));
}

TEST_F(IniSettingsStoreTest, ValuesRoundTripThroughSave)
{
	{
		ds3ea::IniSettingsStore store(get_ini_path());
		store.set_float("Tonemapper", "Exposure", 1.25f);
		store.set_int("Display", "WindowWidth", -2560);
		store.set_bool("DLSS", "Enabled", false);
		store.set_string("DLSS", "Mode", "Balanced");
		ASSERT_TRUE(store.save());
	}

	const ds3ea::IniSettingsStore store(get_ini_path());
	EXPECT_FLOAT_EQ(store.get_float_or("Tonemapper", "Exposure", 0.0f), 1.25f);
	EXPECT_EQ(store.get_int_or("Display", "WindowWidth", 0), -2560);
	EXPECT_FALSE(store.get_bool_or("DLSS", "Enabled", true));
	EXPECT_EQ(store.get_string_or("DLSS", "Mode", ""), "Balanced");
	EXPECT_FALSE(std::filesystem::exists(m_folder / "ds3ea.ini.tmp"));
}

TEST_F(IniSettingsStoreTest, FloatsAreWrittenWithThreeDecimals)
{
	ds3ea::IniSettingsStore store(get_ini_path());
	store.set_float("Tonemapper", "Exposure", 0.85f);
	store.set_float("Tonemapper", "WhitePoint", 11.0f);
	ASSERT_TRUE(store.save());

	const std::string text = read_file();
	EXPECT_NE(text.find("Exposure=0.850\n"), std::string::npos) << text;
	EXPECT_NE(text.find("WhitePoint=11.000\n"), std::string::npos) << text;
}

TEST_F(IniSettingsStoreTest, BoolsAreWrittenAsWords)
{
	ds3ea::IniSettingsStore store(get_ini_path());
	store.set_bool("Tonemapper", "ApplyLUT", true);
	store.set_bool("Tonemapper", "Dither", false);
	ASSERT_TRUE(store.save());

	const std::string text = read_file();
	EXPECT_NE(text.find("ApplyLUT=true\n"), std::string::npos) << text;
	EXPECT_NE(text.find("Dither=false\n"), std::string::npos) << text;
}

TEST_F(IniSettingsStoreTest, SaveKeepsUntouchedSectionsAndKeys)
{
	write_file("Version=2\n[Other]\nName=kept\n[Tonemapper]\nBloom=0.500\nExposure=0.850\n");
	{
		ds3ea::IniSettingsStore store(get_ini_path());
		store.set_float("Tonemapper", "Exposure", 1.0f);
		ASSERT_TRUE(store.save());
	}

	const ds3ea::IniSettingsStore store(get_ini_path());
	EXPECT_EQ(store.get_int_or("", "Version", 0), 2);
	EXPECT_EQ(store.get_string_or("Other", "Name", ""), "kept");
	EXPECT_FLOAT_EQ(store.get_float_or("Tonemapper", "Bloom", 0.0f), 0.5f);
	EXPECT_FLOAT_EQ(store.get_float_or("Tonemapper", "Exposure", 0.0f), 1.0f);
	EXPECT_EQ(read_file().find("[]"), std::string::npos);
}

TEST_F(IniSettingsStoreTest, HandEditedFileReads)
{
	// As a text editor leaves it: CRLF line endings, spaces around =, a comment and a blank line
	write_file("[Tonemapper]\r\nExposure=0.850\r\nWhitePoint = 11.000\r\nApplyLUT=1\r\n\r\n"
			   "; A comment\r\n[DLSS]\r\nInjectionMethod=BeforeCustomTonemapper\r\n");
	const ds3ea::IniSettingsStore store(get_ini_path());

	EXPECT_FLOAT_EQ(store.get_float_or("Tonemapper", "Exposure", 0.0f), 0.85f);
	EXPECT_FLOAT_EQ(store.get_float_or("Tonemapper", "WhitePoint", 0.0f), 11.0f);
	EXPECT_TRUE(store.get_bool_or("Tonemapper", "ApplyLUT", false));
	EXPECT_EQ(store.get_string_or("DLSS", "InjectionMethod", ""), "BeforeCustomTonemapper");
	EXPECT_TRUE(g_logged.empty());
}

TEST_F(IniSettingsStoreTest, MalformedLinesAreLogged)
{
	write_file("[Test]\nNoEquals\n[Unclosed\nValue=1\n");
	const ds3ea::IniSettingsStore store(get_ini_path());

	EXPECT_EQ(store.get_int_or("Test", "Value", 0), 1);
	ASSERT_EQ(g_logged.size(), 2u);
	EXPECT_EQ(g_logged.at(0).level, reshade::log::level::warning);
	EXPECT_NE(g_logged.at(0).text.find("NoEquals"), std::string::npos) << g_logged.at(0).text;
}

TEST_F(IniSettingsStoreTest, NonAsciiPathWorks)
{
	const std::filesystem::path folder = m_folder / L"Dark Souls Ü";
	std::filesystem::create_directories(folder);
	const std::filesystem::path path = folder / "ds3ea.ini";
	{
		ds3ea::IniSettingsStore store(path);
		store.set_int("Test", "Value", 42);
		ASSERT_TRUE(store.save());
	}

	const ds3ea::IniSettingsStore store(path);
	EXPECT_EQ(store.get_int_or("Test", "Value", 0), 42);
	EXPECT_EQ(store.get_path(), path);
}

TEST_F(IniSettingsStoreTest, SaveIntoMissingFolderFails)
{
	ds3ea::IniSettingsStore store(m_folder / "missing" / "ds3ea.ini");
	store.set_int("Test", "Value", 1);

	EXPECT_FALSE(store.save());
	EXPECT_TRUE(has_logged(reshade::log::level::error));
}
} // namespace
