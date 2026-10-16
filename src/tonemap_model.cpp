#include "tonemap_model.hpp"
#include "settings.hpp"
#include "tonemap_constants.hlsli"
#include "utils/shader_reflection.hpp"

#include <algorithm>
#include <cstdint>

namespace
{
using namespace ds3ea;

// The game's vignette coordinate, centred on the screen
constexpr std::uint32_t k_vignette_texcoord = 2;

/**
 * @param value A setting.
 * @param range Its range.
 * @returns The value, clamped to the range.
 */
[[nodiscard]] float clamp_to(float value, SettingRange range)
{
	return std::clamp(value, range.min, range.max);
}

/**
 * @param flag A setting or variant flag.
 * @returns The flag as the shader's uint: 1 on, 0 off.
 */
[[nodiscard]] shader::uint to_shader_flag(bool flag)
{
	return flag ? 1u : 0u;
}
} // namespace

namespace ds3ea
{
TonemapVariant get_tonemap_variant(const ShaderReflectionInfo& game_shader)
{
	const auto& globals = game_shader.globals;
	return {
		.has_vignette = (game_shader.texcoords_read & (1u << k_vignette_texcoord)) != 0,
		.has_color_matrix = std::ranges::find(globals, "m44_ColorTransformMatrix") != globals.end(),
	};
}

shader::TonemapConstants pack_tonemap_constants(const Settings::TonemapperValues& settings, TonemapVariant variant,
	bool is_graded)
{
	return {
		.exposure = clamp_to(settings.exposure, k_exposure_range) * k_exposure_bias,
		.white = clamp_to(settings.white, k_white_range),
		.toe = clamp_to(settings.toe, k_toe_range),
		.shoulder = clamp_to(settings.shoulder, k_shoulder_range),
		.bloom_strength = clamp_to(settings.bloom, k_bloom_range),
		.has_vignette = to_shader_flag(variant.has_vignette),
		.has_color_matrix = to_shader_flag(variant.has_color_matrix),
		.is_graded = to_shader_flag(is_graded),
		.apply_lut = to_shader_flag(settings.apply_lut),
		.dither = to_shader_flag(settings.dither),
		.padding_0 = 0,
		.padding_1 = 0,
	};
}
} // namespace ds3ea
