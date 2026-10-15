#include "ini_settings_store.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace
{
/** Gives each test its own empty folder under the temp directory and removes it afterwards. */
class IniSettingsStoreTest : public testing::Test
{
protected:
	void SetUp() override
	{
		const testing::TestInfo* const info = testing::UnitTest::GetInstance()->current_test_info();
		m_folder = std::filesystem::temp_directory_path() / "ds3ea_tests" / info->test_suite_name() / info->name();
		std::filesystem::remove_all(m_folder);
		std::filesystem::create_directories(m_folder);
	}

	void TearDown() override
	{
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

	std::filesystem::path m_folder;
};

TEST_F(IniSettingsStoreTest, MissingFileReadsDefaults)
{
	const ds3ea::IniSettingsStore store(get_ini_path());

	EXPECT_FLOAT_EQ(store.get_float("Tonemapper", "Exposure", 0.85f), 0.85f);
	EXPECT_EQ(store.get_int("Display", "WindowWidth", 1920), 1920);
	EXPECT_TRUE(store.get_bool("DLSS", "Enabled", true));
	EXPECT_EQ(store.get_string("DLSS", "Mode", "Quality"), "Quality");
	EXPECT_FALSE(std::filesystem::exists(get_ini_path()));
}

TEST_F(IniSettingsStoreTest, MissingKeyOrSectionReadsDefault)
{
	write_file("[Tonemapper]\nExposure=0.500\n");
	const ds3ea::IniSettingsStore store(get_ini_path());

	EXPECT_FLOAT_EQ(store.get_float("Tonemapper", "WhitePoint", 11.0f), 11.0f);
	EXPECT_FLOAT_EQ(store.get_float("Other", "Exposure", 2.0f), 2.0f);
}

TEST_F(IniSettingsStoreTest, GarbageValuesReadDefault)
{
	write_file("[Test]\nFloat=abc\nTrailing=1.5x\nInfinite=inf\nInt=12x\nTooBig=99999999999\nFraction=1.5\n"
			   "Bool=maybe\nEmpty=\n");
	const ds3ea::IniSettingsStore store(get_ini_path());

	EXPECT_FLOAT_EQ(store.get_float("Test", "Float", 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(store.get_float("Test", "Trailing", 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(store.get_float("Test", "Infinite", 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(store.get_float("Test", "Empty", 1.0f), 1.0f);
	EXPECT_EQ(store.get_int("Test", "Int", 7), 7);
	EXPECT_EQ(store.get_int("Test", "TooBig", 7), 7);
	EXPECT_EQ(store.get_int("Test", "Fraction", 7), 7);
	EXPECT_EQ(store.get_int("Test", "Empty", 7), 7);
	EXPECT_TRUE(store.get_bool("Test", "Bool", true));
	EXPECT_FALSE(store.get_bool("Test", "Empty", false));
}

TEST_F(IniSettingsStoreTest, BoolsAcceptIntegersAndWords)
{
	write_file("[Test]\nOne=1\nZero=0\nTwo=2\nUpper=TRUE\nLower=false\n");
	const ds3ea::IniSettingsStore store(get_ini_path());

	EXPECT_TRUE(store.get_bool("Test", "One", false));
	EXPECT_FALSE(store.get_bool("Test", "Zero", true));
	EXPECT_TRUE(store.get_bool("Test", "Two", false));
	EXPECT_TRUE(store.get_bool("Test", "Upper", false));
	EXPECT_FALSE(store.get_bool("Test", "Lower", true));
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
	EXPECT_FLOAT_EQ(store.get_float("Tonemapper", "Exposure", 0.0f), 1.25f);
	EXPECT_EQ(store.get_int("Display", "WindowWidth", 0), -2560);
	EXPECT_FALSE(store.get_bool("DLSS", "Enabled", true));
	EXPECT_EQ(store.get_string("DLSS", "Mode", ""), "Balanced");
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

TEST_F(IniSettingsStoreTest, BoolsAreWrittenAsIntegers)
{
	ds3ea::IniSettingsStore store(get_ini_path());
	store.set_bool("Tonemapper", "ApplyLUT", true);
	store.set_bool("Tonemapper", "Dither", false);
	ASSERT_TRUE(store.save());

	const std::string text = read_file();
	EXPECT_NE(text.find("ApplyLUT=1\n"), std::string::npos) << text;
	EXPECT_NE(text.find("Dither=0\n"), std::string::npos) << text;
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
	EXPECT_EQ(store.get_int("", "Version", 0), 2);
	EXPECT_EQ(store.get_string("Other", "Name", ""), "kept");
	EXPECT_FLOAT_EQ(store.get_float("Tonemapper", "Bloom", 0.0f), 0.5f);
	EXPECT_FLOAT_EQ(store.get_float("Tonemapper", "Exposure", 0.0f), 1.0f);
	EXPECT_EQ(read_file().find("[]"), std::string::npos);
}

TEST_F(IniSettingsStoreTest, LegacyFileReads)
{
	// As the legacy add-on's WritePrivateProfileString left it: CRLF line endings, spaces around = by hand
	write_file("[Tonemapper]\r\nExposure=0.850\r\nWhitePoint = 11.000\r\nApplyLUT=1\r\n\r\n"
			   "; A comment\r\n[DLSS]\r\nInjectionMethod=BeforeCustomTonemapper\r\n");
	const ds3ea::IniSettingsStore store(get_ini_path());

	EXPECT_FLOAT_EQ(store.get_float("Tonemapper", "Exposure", 0.0f), 0.85f);
	EXPECT_FLOAT_EQ(store.get_float("Tonemapper", "WhitePoint", 0.0f), 11.0f);
	EXPECT_TRUE(store.get_bool("Tonemapper", "ApplyLUT", false));
	EXPECT_EQ(store.get_string("DLSS", "InjectionMethod", ""), "BeforeCustomTonemapper");
}

TEST_F(IniSettingsStoreTest, ByteOrderMarkIsSkipped)
{
	write_file("\xEF\xBB\xBF[Tonemapper]\nExposure=0.500\n");
	const ds3ea::IniSettingsStore store(get_ini_path());

	EXPECT_FLOAT_EQ(store.get_float("Tonemapper", "Exposure", 0.0f), 0.5f);
}

TEST_F(IniSettingsStoreTest, LineBreaksInStringsDontAddKeys)
{
	{
		ds3ea::IniSettingsStore store(get_ini_path());
		store.set_string("Test", "Text", "one\nInjected=1");
		ASSERT_TRUE(store.save());
	}

	const ds3ea::IniSettingsStore store(get_ini_path());
	EXPECT_EQ(store.get_string("Test", "Text", ""), "one Injected=1");
	EXPECT_EQ(store.get_string("Test", "Injected", "none"), "none");
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
	EXPECT_EQ(store.get_int("Test", "Value", 0), 42);
	EXPECT_EQ(store.get_path(), path);
}

TEST_F(IniSettingsStoreTest, SaveIntoMissingFolderFails)
{
	ds3ea::IniSettingsStore store(m_folder / "missing" / "ds3ea.ini");
	store.set_int("Test", "Value", 1);

	EXPECT_FALSE(store.save());
}
} // namespace
