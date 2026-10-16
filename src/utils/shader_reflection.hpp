#pragma once

#include "utils/result.hpp"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ds3ea
{
// TEXCOORD indices a pixel shader can have: one per input register, of which D3D11 has 32
inline constexpr std::size_t k_max_texcoords = 32;

/** What a compiled pixel shader declares, as reflect_pixel_shader reads it back. */
struct ShaderReflectionInfo
{
	// Constant buffer names, e.g. "cbMatDynParam"
	std::vector<std::string> cbuffers = {};
	// Texture and buffer names, e.g. "atex3D_Texture"
	std::vector<std::string> resources = {};
	// Variables of $Globals the shader actually uses, e.g. "fParam_TonemapMaxMappingLuminance"
	std::vector<std::string> globals = {};
	// Render target outputs (SV_Target)
	std::uint32_t output_count = 0;
	// Bit n is set if the shader reads its TEXCOORDn input
	std::bitset<k_max_texcoords> texcoords_used = {};
};

/**
 * Reads back what a compiled pixel shader declares: the names of its constant buffers and textures, which of its
 * global variables and TEXCOORD inputs it uses, and how many render targets it writes. Compiled shader code keeps
 * these names, and D3D can list them (D3D calls this "reflection", hence D3DReflect); DS3's shaders were shipped with
 * them intact.
 * @param code The compiled shader, as the game passed it to CreatePixelShader.
 * @param size Its size in bytes.
 * @returns What the shader declares, or an invalid_data Error if D3D can't read the code.
 */
[[nodiscard]] Result<ShaderReflectionInfo> reflect_pixel_shader(const void* code, std::size_t size);
} // namespace ds3ea
