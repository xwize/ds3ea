cmake_minimum_required(VERSION 3.25)

# Conan runs automatically during configure (cmake-conan), so IDEs need no separate install step.
# Pass -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES= with your own Conan toolchain to opt out.
# cmake/conan_provider.cmake is vendored from conan-io/cmake-conan, branch develop2, commit dff3768b. Later commits
# warn on every configure unless the conanfile uses the CMakeConfigDeps generator instead of CMakeDeps.
if(NOT DEFINED CMAKE_PROJECT_TOP_LEVEL_INCLUDES)
	set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES "${CMAKE_CURRENT_LIST_DIR}/cmake/conan_provider.cmake")
endif()
# Stop Conan writing a CMakeUserPresets.json into the source tree, which belongs to the developer
set(CONAN_INSTALL_ARGS "--build=missing;-c=tools.cmake.cmaketoolchain:user_presets=" CACHE STRING
	"Command line arguments for conan install")

# Static CRT, same as ReShade, so the add-on has no VC redistributable dependency. Set before project() so Conan
# builds its packages with the matching runtime.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# C is enabled only so cmake-conan can put the C compiler in the Conan profile; all sources are C++
project(ds3ea VERSION 0.1.0 LANGUAGES C CXX)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "DS3EA builds only as a 64-bit MSVC target (Dark Souls 3 is x64)")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/reshade.cmake)
include(cmake/clang_tidy.cmake)

find_package(fmt REQUIRED)
find_package(inipp REQUIRED)
find_package(scn REQUIRED)

# Compiler settings shared by the add-on and its tests
add_library(ds3ea_options INTERFACE)
target_compile_options(ds3ea_options INTERFACE /W4 /WX /permissive- /utf-8 /external:W0)
target_compile_definitions(ds3ea_options INTERFACE UNICODE _UNICODE)

# Code shared by the add-on and its tests. inipp is linked PRIVATE so only ini_settings_store.cpp sees it; scnlib is
# there for parsing text.
add_library(ds3ea_core STATIC
	src/ini_settings_store.cpp
	src/ini_settings_store.hpp
	src/logging.cpp
	src/logging.hpp
)
target_include_directories(ds3ea_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(ds3ea_core PUBLIC fmt::fmt PRIVATE ds3ea_options inipp::inipp reshade::headers scn::scn)
ds3ea_clang_tidy(ds3ea_core)

add_library(ds3ea SHARED
	src/addon.cpp
	src/addon.hpp
	src/addon_info.hpp
	src/dll_main.cpp
)
target_link_libraries(ds3ea PRIVATE ds3ea_options ds3ea_core reshade::headers fmt::fmt)
ds3ea_clang_tidy(ds3ea)
# ReShade loads add-ons by extension, so the DLL is ds3ea.addon64
set_target_properties(ds3ea PROPERTIES
	OUTPUT_NAME "ds3ea"
	SUFFIX ".addon64"
)

# enable_testing() instead of include(CTest), which would add CDash targets to the IDE
option(BUILD_TESTING "Build the unit tests" ON)
if(BUILD_TESTING)
	enable_testing()
	add_subdirectory(test)
endif()
