#include "settings.hpp"
#include "utils/ini_settings_store.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace
{
/** Gives each test its own empty folder under the temp directory, holding the ini, and removes it afterwards. */
class SettingsTest : public testing::Test
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

	[[nodiscard]] std::filesystem::path get_ini_path() const
	{
		return m_folder / "ds3ea.ini";
	}

	/** Writes the file as another program would, with a last-write time no earlier load or save can have seen. */
	void write_file(std::string_view text) const
	{
		{
			std::ofstream file(get_ini_path(), std::ios::binary);
			file << text;
		}

		const std::filesystem::file_time_type written = std::filesystem::last_write_time(get_ini_path());
		std::filesystem::last_write_time(get_ini_path(), written + std::chrono::hours(1));
	}

	[[nodiscard]] std::string read_file() const
	{
		std::ifstream file(get_ini_path());
		return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}

	std::filesystem::path m_folder;
};

TEST_F(SettingsTest, MissingFileGivesDefaults)
{
	const ds3ea::IniSettingsStore store(get_ini_path());
	const ds3ea::Settings settings(store);

	EXPECT_EQ(settings.get(), ds3ea::Settings::Values{});
	EXPECT_FALSE(settings.has_unsaved_changes());
}

TEST_F(SettingsTest, LoadsValuesFromTheFile)
{
	write_file("[Tonemapper]\nReplaceGameTonemap=false\nExposure=1.250\nApplyLUT=0\n");
	const ds3ea::IniSettingsStore store(get_ini_path());
	const ds3ea::Settings settings(store);

	EXPECT_FALSE(settings.get().tonemapper.replace_game_tonemap);
	EXPECT_FLOAT_EQ(settings.get().tonemapper.exposure, 1.25f);
	EXPECT_FALSE(settings.get().tonemapper.apply_lut);
}

TEST_F(SettingsTest, ChangesAreDetectedUntilSaved)
{
	ds3ea::IniSettingsStore store(get_ini_path());
	ds3ea::Settings settings(store);
	ds3ea::Settings::Values values = settings.get();

	settings.set(values);
	EXPECT_FALSE(settings.has_unsaved_changes());

	values.tonemapper.replace_game_tonemap = false;
	settings.set(values);
	EXPECT_TRUE(settings.has_unsaved_changes());

	ASSERT_TRUE(settings.save(store));
	EXPECT_FALSE(settings.has_unsaved_changes());
}

TEST_F(SettingsTest, SavedValuesLoadAgain)
{
	{
		ds3ea::IniSettingsStore store(get_ini_path());
		ds3ea::Settings settings(store);
		settings.set({ .tonemapper = { .replace_game_tonemap = false } });
		ASSERT_TRUE(settings.save(store));
	}

	const ds3ea::IniSettingsStore store(get_ini_path());
	const ds3ea::Settings settings(store);
	EXPECT_FALSE(settings.get().tonemapper.replace_game_tonemap);
}

TEST_F(SettingsTest, SaveKeepsUnknownKeys)
{
	write_file("[Tonemapper]\nContrast=0.250\n[Other]\nName=kept\n");
	ds3ea::IniSettingsStore store(get_ini_path());
	ds3ea::Settings settings(store);
	settings.set({ .tonemapper = { .replace_game_tonemap = false } });
	ASSERT_TRUE(settings.save(store));

	const std::string text = read_file();
	EXPECT_NE(text.find("Contrast=0.250\n"), std::string::npos) << text;
	EXPECT_NE(text.find("Name=kept\n"), std::string::npos) << text;
	EXPECT_NE(text.find("ReplaceGameTonemap=false\n"), std::string::npos) << text;
}

TEST_F(SettingsTest, ExternalEditIsReloaded)
{
	ds3ea::IniSettingsStore store(get_ini_path());
	ds3ea::Settings settings(store);
	write_file("[Tonemapper]\nReplaceGameTonemap=false\n");

	settings.reload_if_changed_on_disk(store);

	EXPECT_FALSE(settings.get().tonemapper.replace_game_tonemap);
	EXPECT_FALSE(settings.has_unsaved_changes());
	EXPECT_FALSE(store.is_changed_on_disk());
}

TEST_F(SettingsTest, OwnSaveIsNotAnExternalEdit)
{
	ds3ea::IniSettingsStore store(get_ini_path());
	ds3ea::Settings settings(store);
	settings.set({ .tonemapper = { .replace_game_tonemap = false } });
	ASSERT_TRUE(settings.save(store));

	EXPECT_FALSE(store.is_changed_on_disk());
}

TEST_F(SettingsTest, FileWinsOverUnsavedChanges)
{
	ds3ea::IniSettingsStore store(get_ini_path());
	ds3ea::Settings settings(store);
	settings.set({ .tonemapper = { .replace_game_tonemap = false } });
	write_file("[Tonemapper]\nReplaceGameTonemap=true\n");

	settings.reload_if_changed_on_disk(store);

	EXPECT_TRUE(settings.get().tonemapper.replace_game_tonemap);
	EXPECT_FALSE(settings.has_unsaved_changes());
}
} // namespace
