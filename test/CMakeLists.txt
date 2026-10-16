find_package(GTest REQUIRED)

add_executable(ds3ea_tests
	addon_info_test.cpp
	utils/error_test.cpp
	utils/ini_settings_store_test.cpp
	utils/logging_test.cpp
	utils/result_test.cpp
	utils/runtime_error_test.cpp
)
target_link_libraries(ds3ea_tests PRIVATE ds3ea_options ds3ea_core reshade::headers GTest::gtest_main)
ds3ea_clang_tidy(ds3ea_tests)
if(DS3EA_SANITIZE)
	ds3ea_copy_asan_runtime(ds3ea_tests)
endif()

include(GoogleTest)
gtest_discover_tests(ds3ea_tests)
