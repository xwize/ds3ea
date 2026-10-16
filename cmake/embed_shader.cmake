# Writes a compiled shader into a C++ header as a std::array of its bytes (run by cmake/shaders.cmake).
# -DBYTECODE=<.cso file> -DHEADER=<.h to write> -DNAME=<variable name> -DSOURCE=<the .hlsl, for the comment>

file(READ "${BYTECODE}" hex HEX)
string(LENGTH "${hex}" hex_length)
math(EXPR size "${hex_length} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")

file(WRITE "${HEADER}" "// Generated from ${SOURCE} by cmake/shaders.cmake; don't edit\n"
	"#pragma once\n\n#include <array>\n#include <cstdint>\n\n"
	"namespace ds3ea\n{\n"
	"inline constexpr std::array<std::uint8_t, ${size}> ${NAME} = { ${bytes} };\n"
	"} // namespace ds3ea\n")
