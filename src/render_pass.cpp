#include "render_pass.hpp"
#include "utils/shader_reflection.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace
{
/**
 * @param names Names a shader declares.
 * @param name The name to look for.
 * @returns True if it's among them.
 */
[[nodiscard]] bool contains(const std::vector<std::string>& names, std::string_view name)
{
	return std::ranges::find(names, name) != names.end();
}
} // namespace

namespace ds3ea
{
RenderPass infer_render_pass_for_shader(const ShaderReflectionInfo& pixel_shader)
{
	// The YEBIS tonemap variants are the shaders that use the tonemap curve's parameters
	if (contains(pixel_shader.globals, "fParam_TonemapMaxMappingLuminance"))
		return RenderPass::tonemap;

	if (contains(pixel_shader.resources, "atex3D_Texture"))
		return RenderPass::color_grading;

	// Decals write the same render targets as the G-buffer materials, so they're matched first
	if (contains(pixel_shader.cbuffers, "cbDecalParam"))
		return RenderPass::decal;

	const bool material =
		contains(pixel_shader.cbuffers, "cbMatDynParam") || contains(pixel_shader.cbuffers, "cbMtdParam");

	if (pixel_shader.output_count >= 4 && material)
		return RenderPass::gbuffer;

	return RenderPass::unknown;
}
} // namespace ds3ea
