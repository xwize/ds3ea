find_package(GTest REQUIRED)

add_executable(ds3ea_tests
	addon_info_test.cpp
)
target_include_directories(ds3ea_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(ds3ea_tests PRIVATE ds3ea_options GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(ds3ea_tests)
