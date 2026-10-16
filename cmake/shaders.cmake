# Compiles the HLSL shaders in shaders/ at build time with FXC, the Windows SDK's compiler for Direct3D 11 bytecode,
# and embeds each one in src/gen/<name>.h as ds3ea::k_<name> (GUIDELINES.md). A shader that doesn't compile, or
# compiles with a warning, fails the build. The ds3ea_shaders target builds them all; targets that include a
# generated header depend on it.

# FXC isn't on PATH outside a developer prompt (the vs presets), so look in the newest Windows SDK first
set(_program_files_x86 "ProgramFiles(x86)")
file(GLOB _sdk_bin_dirs LIST_DIRECTORIES true "$ENV{${_program_files_x86}}/Windows Kits/10/bin/10.*/x64")
list(SORT _sdk_bin_dirs COMPARE NATURAL ORDER DESCENDING)
find_program(DS3EA_FXC fxc HINTS ${_sdk_bin_dirs} REQUIRED)
message(STATUS "FXC: ${DS3EA_FXC}")

file(GLOB _shader_includes CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/shaders/*.hlsli")
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/shaders")

# Compiles the pixel shader shaders/<name>.hlsl (entry point main, shader model 5) into src/gen/<name>.h
function(ds3ea_pixel_shader name)
	set(source "${PROJECT_SOURCE_DIR}/shaders/${name}.hlsl")
	set(bytecode "${PROJECT_BINARY_DIR}/shaders/${name}.cso")
	set(header "${PROJECT_SOURCE_DIR}/src/gen/${name}.h")
	add_custom_command(OUTPUT "${header}"
		COMMAND "${DS3EA_FXC}" /nologo /WX /O3 /T ps_5_0 /E main /Fo "${bytecode}" "${source}"
		COMMAND "${CMAKE_COMMAND}" "-DBYTECODE=${bytecode}" "-DHEADER=${header}" "-DNAME=k_${name}"
			"-DSOURCE=shaders/${name}.hlsl" -P "${PROJECT_SOURCE_DIR}/cmake/embed_shader.cmake"
		DEPENDS "${source}" ${_shader_includes} "${PROJECT_SOURCE_DIR}/cmake/embed_shader.cmake"
		COMMENT "Compiling shaders/${name}.hlsl"
		VERBATIM
	)
	set_property(GLOBAL APPEND PROPERTY DS3EA_SHADER_HEADERS "${header}")
endfunction()

ds3ea_pixel_shader(grading_passthrough)
ds3ea_pixel_shader(tonemap)

get_property(_shader_headers GLOBAL PROPERTY DS3EA_SHADER_HEADERS)
add_custom_target(ds3ea_shaders DEPENDS ${_shader_headers})
