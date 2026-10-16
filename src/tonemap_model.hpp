#pragma once

#include "settings.hpp"
#include "tonemap_constants.hlsli"
#include "utils/shader_reflection.hpp"

/**
 * @file
 * The tonemap's model: which variant of the game's tonemap a shader is, and the constants our tonemap shader
 * (shaders/tonemap.hlsl) gets from the settings and the variant. Pure, so it's unit-tested; TonemapRenderer draws with
 * it.
 */

namespace ds3ea
{
/**
 * What a game tonemap shader applies besides its curve. The game has a variant with the radial vignette and the colour
 * matrix, and one with neither; our shader stands in for both, told which by its constants.
 */
struct TonemapVariant
{
	// Reads the centred vignette coordinate (TEXCOORD2) and darkens towards the edges
	bool has_vignette = false;
	// Applies m44_ColorTransformMatrix
	bool has_color_matrix = false;

	bool operator==(const TonemapVariant&) const = default;
};

/** The range a tonemap setting is kept in: the overlay's slider, and a clamp on values read from the ini. */
struct SettingRange
{
	float min;
	float max;
};

inline constexpr SettingRange k_exposure_range = { .min = 0.25f, .max = 4.0f };
inline constexpr SettingRange k_white_range = { .min = 2.0f, .max = 30.0f };
inline constexpr SettingRange k_toe_range = { .min = 0.05f, .max = 1.0f };
inline constexpr SettingRange k_shoulder_range = { .min = 0.05f, .max = 0.5f };
inline constexpr SettingRange k_bloom_range = { .min = 0.0f, .max = 1.0f };

/**
 * The curve's input scale at exposure 1: puts the game's mid-grey (0.5 after its exposure) at about the same output
 * level as the game's curve (0.49 before gamma).
 */
inline constexpr float k_exposure_bias = 4.0f;

/**
 * @param game_shader What a game tonemap pixel shader declares.
 * @returns Its variant: the vignette if it reads TEXCOORD2, the colour matrix if it uses m44_ColorTransformMatrix.
 */
[[nodiscard]] TonemapVariant get_tonemap_variant(const ShaderReflectionInfo& game_shader);

/**
 * @param settings The tonemap settings, clamped to their ranges.
 * @param variant The variant of the game's tonemap being replaced.
 * @param is_graded True if the game's grading inputs (fade, LUT) are bound for the shader to apply.
 * @returns The constants for shaders/tonemap.hlsl.
 */
[[nodiscard]] shader::TonemapConstants pack_tonemap_constants(const Settings::TonemapperValues& settings,
	TonemapVariant variant, bool is_graded);
} // namespace ds3ea
