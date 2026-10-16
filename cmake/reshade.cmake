# Provides the header-only ReShade add-on API as the `reshade::headers` target, with the Dear ImGui header its overlay
# API is built against.
#
# Set DS3EA_RESHADE_DIR to a ReShade source checkout (e.g. ds3ea/repos/reshade) with its submodules to use local
# headers. Left empty, the pinned commits below are downloaded once per build tree, so a fresh worktree configures
# without any local setup.

set(DS3EA_RESHADE_DIR "" CACHE PATH "ReShade source checkout providing include/reshade.hpp (empty: download pinned)")

# Keep in step with the ReShade version DS3EA is tested against
set(DS3EA_RESHADE_COMMIT "7bf9de8b33bcc76c3177007e65d73c72dd0f34c0") # v6.8.0-28
set(DS3EA_RESHADE_SHA256 "94a9c98b5977dde5f3f644a6ea0e205998f1e0c36b9aca2a2ce6449a684ef192")
# ReShade's deps/imgui submodule at that commit (docking branch, 1.92.5). reshade_overlay.hpp refuses any other version.
# GitHub's archive of ReShade leaves submodules out, so it's downloaded on its own.
set(DS3EA_IMGUI_COMMIT "3912b3d9a9c1b3f17431aebafd86d2f40ee6e59c")
set(DS3EA_IMGUI_SHA256 "943c3c8f6ef87c2616ff5545d4c50d941f29f20d23d783a8ff2f46c8a3bcfdf6")

if(DS3EA_RESHADE_DIR)
	set(_reshade_root "${DS3EA_RESHADE_DIR}")
	set(_imgui_root "${DS3EA_RESHADE_DIR}/deps/imgui")
else()
	include(FetchContent)
	FetchContent_Declare(reshade
		URL "https://github.com/crosire/reshade/archive/${DS3EA_RESHADE_COMMIT}.zip"
		URL_HASH "SHA256=${DS3EA_RESHADE_SHA256}"
		DOWNLOAD_EXTRACT_TIMESTAMP ON
		# Only the headers are needed: include/ has no CMakeLists.txt, so ReShade's own project is never added
		SOURCE_SUBDIR include
	)
	# Only imgui.h is needed. The repository root has no CMakeLists.txt, so nothing is added to the build.
	FetchContent_Declare(imgui
		URL "https://github.com/ocornut/imgui/archive/${DS3EA_IMGUI_COMMIT}.zip"
		URL_HASH "SHA256=${DS3EA_IMGUI_SHA256}"
		DOWNLOAD_EXTRACT_TIMESTAMP ON
	)
	FetchContent_MakeAvailable(reshade imgui)
	set(_reshade_root "${reshade_SOURCE_DIR}")
	set(_imgui_root "${imgui_SOURCE_DIR}")
endif()

if(NOT EXISTS "${_reshade_root}/include/reshade.hpp")
	message(FATAL_ERROR "reshade.hpp not found under '${_reshade_root}/include'; check DS3EA_RESHADE_DIR")
endif()

if(NOT EXISTS "${_imgui_root}/imgui.h")
	message(FATAL_ERROR "imgui.h not found under '${_imgui_root}'; check out ReShade's submodules")
endif()

message(STATUS "ReShade headers: ${_reshade_root}/include")
message(STATUS "Dear ImGui header: ${_imgui_root}")

add_library(reshade_headers INTERFACE)
add_library(reshade::headers ALIAS reshade_headers)
# SYSTEM, so our warning level doesn't apply to ReShade's or ImGui's headers. reshade_overlay.hpp needs ImGui's texture
# IDs to be 64-bit.
target_include_directories(reshade_headers SYSTEM INTERFACE "${_reshade_root}/include" "${_imgui_root}")
target_compile_definitions(reshade_headers INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN ImTextureID=ImU64)
