#pragma once

#include "shader_reflection.hpp"

namespace ds3ea
{
/** The game's render passes we recognise, from the pixel shader bound for a draw. */
enum class RenderPass
{
	unknown,       // No recognised pass; doesn't end the open pass
	gbuffer,       // Material shaders writing the G-buffer render targets
	decal,         // Decals over the G-buffer
	tonemap,       // YEBIS tonemap: HDR scene + bloom to the 8-bit frame
	color_grading, // 3D LUT colour grading of the 8-bit frame
};

/**
 * Works out which render pass a game pixel shader draws for. Every one of DS3's pixel shaders belongs to one pass, so
 * the pass a draw belongs to is the pass of the pixel shader it's drawn with. The shader is recognised by the names
 * it was written with (rules from the legacy add-on's frame analysis).
 * @param pixel_shader What the game's pixel shader declares.
 * @returns Its render pass, or RenderPass::unknown.
 */
[[nodiscard]] RenderPass infer_render_pass_for_shader(const ShaderReflectionInfo& pixel_shader);
} // namespace ds3ea
