find_package(GTest REQUIRED)

add_executable(ds3ea_tests
	addon_info_test.cpp
	ini_settings_store_test.cpp
	logging_test.cpp
)
target_link_libraries(ds3ea_tests PRIVATE ds3ea_options ds3ea_core reshade::headers GTest::gtest_main)
ds3ea_clang_tidy(ds3ea_tests)

include(GoogleTest)
gtest_discover_tests(ds3ea_tests)
