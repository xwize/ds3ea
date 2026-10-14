# Provides the header-only ReShade add-on API as the `reshade::headers` target.
#
# Set DS3EA_RESHADE_DIR to a ReShade source checkout (e.g. ds3ea/repos/reshade) to use local headers. Left empty, the
# pinned commit below is downloaded once per build tree, so a fresh worktree configures without any local setup.

set(DS3EA_RESHADE_DIR "" CACHE PATH "ReShade source checkout providing include/reshade.hpp (empty: download pinned)")

# Keep in step with the ReShade version DS3EA is tested against
set(DS3EA_RESHADE_COMMIT "7bf9de8b33bcc76c3177007e65d73c72dd0f34c0") # v6.8.0-28
set(DS3EA_RESHADE_SHA256 "94a9c98b5977dde5f3f644a6ea0e205998f1e0c36b9aca2a2ce6449a684ef192")

if(DS3EA_RESHADE_DIR)
	set(_reshade_root "${DS3EA_RESHADE_DIR}")
else()
	include(FetchContent)
	FetchContent_Declare(reshade
		URL "https://github.com/crosire/reshade/archive/${DS3EA_RESHADE_COMMIT}.zip"
		URL_HASH "SHA256=${DS3EA_RESHADE_SHA256}"
		DOWNLOAD_EXTRACT_TIMESTAMP ON
		# Only the headers are needed: include/ has no CMakeLists.txt, so ReShade's own project is never added
		SOURCE_SUBDIR include
	)
	FetchContent_MakeAvailable(reshade)
	set(_reshade_root "${reshade_SOURCE_DIR}")
endif()

if(NOT EXISTS "${_reshade_root}/include/reshade.hpp")
	message(FATAL_ERROR "reshade.hpp not found under '${_reshade_root}/include'; check DS3EA_RESHADE_DIR")
endif()

message(STATUS "ReShade headers: ${_reshade_root}/include")

add_library(reshade_headers INTERFACE)
add_library(reshade::headers ALIAS reshade_headers)
# SYSTEM, so our warning level doesn't apply to ReShade's headers
target_include_directories(reshade_headers SYSTEM INTERFACE "${_reshade_root}/include")
target_compile_definitions(reshade_headers INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN)
