# AddressSanitizer and UndefinedBehaviorSanitizer for the unit tests (DS3EA_SANITIZE, the clang-sanitize preset), as
# the ds3ea_sanitizers target that ds3ea_options links. Include before the first find_package, since it also sets up
# the Conan packages.
#
# MSVC's ASan can't be combined with /RTC1 (in CMake's Debug flags) or incremental linking, but clang-cl ignores /RTC1
# and lld-link never links incrementally, so neither needs removing.

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	message(FATAL_ERROR "DS3EA_SANITIZE needs clang-cl (MSVC has no UndefinedBehaviorSanitizer); use the clang-sanitize "
		"preset")
endif()

# CMake links with lld-link directly, which doesn't add the sanitizer runtime the way the clang-cl driver does. With
# both sanitizers the ASan runtime DLL holds UBSan's too; the thunk connects it to the static CRT. lld-link finds both in
# the compiler's resource directory.
set(_sanitizer_link_options
	clang_rt.asan_dynamic-x86_64.lib
	/wholearchive:clang_rt.asan_static_runtime_thunk-x86_64.lib
)

# UBSan reports and carries on by default, so -fno-sanitize-recover=undefined makes its first report end the test run
# with a failure
add_library(ds3ea_sanitizers INTERFACE)
target_compile_options(ds3ea_sanitizers INTERFACE -fsanitize=address,undefined -fno-sanitize-recover=undefined)
target_link_options(ds3ea_sanitizers INTERFACE ${_sanitizer_link_options})

# With ASan the MSVC STL marks the unused capacity of std::string and std::vector, so ASan catches accesses past size()
# but within capacity(). Every library linked together must agree on this (the linker checks), so the Conan packages
# are built with ASan too, and the flag is part of their package ID so they're kept apart from the plain clang-cl ones.
# The alternative, _DISABLE_STRING_ANNOTATION / _DISABLE_VECTOR_ANNOTATION, links the plain packages but loses that
# check. The link options are for the executables CMake builds to test the compiler.
list(JOIN _sanitizer_link_options "\",\"" _conan_link_flags)
list(APPEND CONAN_INSTALL_ARGS
	"-c:h=tools.build:cxxflags=[\"-fsanitize=address\"]"
	"-c:h=tools.build:exelinkflags=[\"${_conan_link_flags}\"]"
	"-c:h=tools.info.package_id:confs=[\"tools.build:cxxflags\"]"
)

# The ASan runtime DLL, which sanitized executables load at start
execute_process(
	COMMAND "${CMAKE_CXX_COMPILER}" /clang:-print-resource-dir
	OUTPUT_VARIABLE _clang_resource_dir
	OUTPUT_STRIP_TRAILING_WHITESPACE
	COMMAND_ERROR_IS_FATAL ANY
)
find_file(DS3EA_ASAN_RUNTIME clang_rt.asan_dynamic-x86_64.dll
	HINTS "${_clang_resource_dir}/lib/windows"
	NO_DEFAULT_PATH
	REQUIRED
)
message(STATUS "ASan runtime: ${DS3EA_ASAN_RUNTIME}")

# Copies the ASan runtime next to a sanitized executable after it links, so it runs (and gtest_discover_tests can list
# its tests) without the LLVM folder on PATH. Call before gtest_discover_tests, which runs the executable at build time.
function(ds3ea_copy_asan_runtime target)
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${DS3EA_ASAN_RUNTIME}" "$<TARGET_FILE_DIR:${target}>"
	)
endfunction()
