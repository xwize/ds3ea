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

option(DS3EA_SANITIZE "Build the unit tests with AddressSanitizer and UndefinedBehaviorSanitizer (clang-cl only)" OFF)

# Static CRT, same as ReShade, so the add-on has no VC redistributable dependency. Set before project() so Conan
# builds its packages with the matching runtime. ASan doesn't support the debug CRT, so sanitized builds use the
# release one.
if(DS3EA_SANITIZE)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
else()
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# C is enabled only so cmake-conan can put the C compiler in the Conan profile; all sources are C++
project(ds3ea VERSION 0.1.0 LANGUAGES C CXX)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "DS3EA builds only as a 64-bit MSVC target (Dark Souls 3 is x64)")
endif()

# With clang-cl, Conan builds its own compiler=clang packages from source (a few seconds each, once per configuration)
# rather than reusing the MSVC ones, so every package is built by the compiler that links it. cmake-conan passes the
# CRT on to Conan only for MSVC, so pass it here (with the MSVC toolset whose STL clang-cl uses), or the packages get
# the DLL CRT and fail to link.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT DS3EA_SANITIZE)
		set(_conan_runtime_type "Debug")
	else()
		set(_conan_runtime_type "Release")
	endif()
	list(APPEND CONAN_INSTALL_ARGS
		"-s:h=compiler.runtime=static"
		"-s:h=compiler.runtime_type=${_conan_runtime_type}"
		"-s:h=compiler.runtime_version=v${MSVC_TOOLSET_VERSION}"
	)
endif()

if(DS3EA_SANITIZE)
	include(cmake/sanitizers.cmake)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/reshade.cmake)
include(cmake/clang_tidy.cmake)

find_package(fmt REQUIRED)
find_package(inipp REQUIRED)
find_package(tl-expected REQUIRED)

# Compiler settings shared by the add-on and its tests
add_library(ds3ea_options INTERFACE)
target_compile_options(ds3ea_options INTERFACE /W4 /WX /permissive- /utf-8 /external:W0)
target_compile_definitions(ds3ea_options INTERFACE UNICODE _UNICODE)
if(DS3EA_SANITIZE)
	target_link_libraries(ds3ea_options INTERFACE ds3ea_sanitizers)
endif()

# Code shared by the add-on and its tests. inipp is linked PRIVATE so only ini_settings_store.cpp sees it;
# d3dcompiler provides D3DReflect for shader_reflection.cpp.
add_library(ds3ea_core STATIC
	src/listeners/device_listener.hpp
	src/listeners/pipeline_listener.hpp
	src/listeners/render_pass_listener.hpp
	src/render_pass.cpp
	src/render_pass.hpp
	src/render_pass_state_machine.cpp
	src/render_pass_state_machine.hpp
	src/reshade_forward.hpp
	src/shader_reflection.cpp
	src/shader_reflection.hpp
	src/utils/boundary.hpp
	src/utils/build_config.hpp
	src/utils/error.cpp
	src/utils/error.hpp
	src/utils/ini_settings_store.cpp
	src/utils/ini_settings_store.hpp
	src/utils/logging.cpp
	src/utils/logging.hpp
	src/utils/result.hpp
	src/utils/runtime_error.cpp
	src/utils/runtime_error.hpp
)
target_include_directories(ds3ea_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(ds3ea_core
	PUBLIC fmt::fmt tl::expected
	PRIVATE ds3ea_options d3dcompiler inipp::inipp reshade::headers
)
ds3ea_clang_tidy(ds3ea_core)

# A sanitized build is only for the unit tests: it links ds3ea_core built with the sanitizers, and an add-on built that
# way would need the ASan runtime loaded into the game. So the add-on isn't built.
if(NOT DS3EA_SANITIZE)
	add_library(ds3ea SHARED
		src/addon.cpp
		src/addon.hpp
		src/addon_info.hpp
		src/dll_main.cpp
		src/render_pass_tracker.cpp
		src/render_pass_tracker.hpp
		src/tonemapper.cpp
		src/tonemapper.hpp
	)
	target_link_libraries(ds3ea PRIVATE ds3ea_options ds3ea_core reshade::headers fmt::fmt)
	ds3ea_clang_tidy(ds3ea)
	# ReShade loads add-ons by extension, so the DLL is ds3ea.addon64
	set_target_properties(ds3ea PROPERTIES
		OUTPUT_NAME "ds3ea"
		SUFFIX ".addon64"
	)
endif()

# enable_testing() instead of include(CTest), which would add CDash targets to the IDE
option(BUILD_TESTING "Build the unit tests" ON)
if(BUILD_TESTING)
	enable_testing()
	add_subdirectory(test)
endif()
