find_package(GTest REQUIRED)

add_executable(ds3ea_tests
	addon_info_test.cpp
	borderless_restyler_test.cpp
	render_pass_state_machine_test.cpp
	render_pass_test.cpp
	settings_test.cpp
	utils/byte_pattern_test.cpp
	utils/error_test.cpp
	utils/ini_settings_store_test.cpp
	utils/logging_test.cpp
	utils/memory_patch_test.cpp
	utils/result_test.cpp
	utils/runtime_error_test.cpp
	utils/shader_reflection_test.cpp
)
target_link_libraries(ds3ea_tests PRIVATE ds3ea_options ds3ea_core reshade::headers GTest::gtest_main)
add_dependencies(ds3ea_tests ds3ea_shaders)
ds3ea_clang_tidy(ds3ea_tests)
if(DS3EA_SANITIZE)
	ds3ea_copy_asan_runtime(ds3ea_tests)
endif()

include(GoogleTest)
gtest_discover_tests(ds3ea_tests)
