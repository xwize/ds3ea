# clang-tidy on every build, with the checks in .clang-tidy at the repository root. Uses the clang-tidy that ships
# with Visual Studio's "C++ Clang tools" component. Turn it off for faster local builds with -DDS3EA_CLANG_TIDY=OFF.
option(DS3EA_CLANG_TIDY "Run clang-tidy on our sources during the build" ON)

if(DS3EA_CLANG_TIDY)
	# cl.exe lives in VC/Tools/MSVC/<version>/bin/..., clang-tidy in VC/Tools/Llvm/x64/bin
	string(REGEX REPLACE "/MSVC/.*$" "/Llvm/x64/bin" DS3EA_VS_LLVM_DIR "${CMAKE_CXX_COMPILER}")
	find_program(DS3EA_CLANG_TIDY_EXE clang-tidy HINTS "${DS3EA_VS_LLVM_DIR}")
	if(NOT DS3EA_CLANG_TIDY_EXE)
		message(FATAL_ERROR "clang-tidy not found. Install Visual Studio's \"C++ Clang tools for Windows\" component, "
			"or configure with -DDS3EA_CLANG_TIDY=OFF")
	endif()

	message(STATUS "clang-tidy: ${DS3EA_CLANG_TIDY_EXE}")
endif()

# Runs clang-tidy on a target's sources when it compiles. The naked new/delete checks in .clang-tidy are clang-query
# custom checks, which need --experimental-custom-checks.
function(ds3ea_clang_tidy target)
	if(NOT DS3EA_CLANG_TIDY)
		return()
	endif()

	if(CMAKE_GENERATOR MATCHES "Visual Studio")
		# CMake's CXX_CLANG_TIDY isn't implemented for the Visual Studio generators, so use MSBuild's code analysis
		cmake_path(GET DS3EA_CLANG_TIDY_EXE PARENT_PATH tidy_dir)
		cmake_path(GET DS3EA_CLANG_TIDY_EXE FILENAME tidy_exe)
		set_target_properties(${target} PROPERTIES
			VS_GLOBAL_RunCodeAnalysis true
			VS_GLOBAL_EnableClangTidyCodeAnalysis true
			VS_GLOBAL_EnableMicrosoftCodeAnalysis false
			VS_GLOBAL_ClangTidyToolPath "${tidy_dir}"
			VS_GLOBAL_ClangTidyToolExe "${tidy_exe}"
			VS_GLOBAL_ClangTidyToolExeAdditionalOptions "--quiet --experimental-custom-checks"
		)
	else()
		set_target_properties(${target} PROPERTIES
			CXX_CLANG_TIDY "${DS3EA_CLANG_TIDY_EXE};--quiet;--experimental-custom-checks"
		)
	endif()
endfunction()
