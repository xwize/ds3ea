#include "settings.hpp"
#include "tonemap_model.hpp"
#include "utils/shader_reflection.hpp"

#include <gtest/gtest.h>

namespace
{
using ds3ea::TonemapVariant;

TEST(TonemapVariant, VignetteAndColorMatrixVariant)
{
	const ds3ea::ShaderReflectionInfo shader = {
		.globals = { "fParam_TonemapMaxMappingLuminance", "m44_ColorTransformMatrix" },
		.texcoords_read = 0b11111,
	};

	EXPECT_EQ(ds3ea::get_tonemap_variant(shader), (TonemapVariant{ .has_vignette = true, .has_color_matrix = true }));
}

TEST(TonemapVariant, PlainVariantDeclaresButDoesntReadTheVignetteCoordinate)
{
	const ds3ea::ShaderReflectionInfo shader = {
		.globals = { "fParam_TonemapMaxMappingLuminance" },
		.texcoords_read = 0b11011,
	};

	EXPECT_EQ(ds3ea::get_tonemap_variant(shader), TonemapVariant{});
}

TEST(TonemapConstants, DefaultsScaleExposureAndSetFlags)
{
	const ds3ea::shader::TonemapConstants constants = ds3ea::pack_tonemap_constants({},
		{ .has_vignette = true, .has_color_matrix = false }, true);

	EXPECT_FLOAT_EQ(constants.exposure, ds3ea::Settings::TonemapperValues::k_default_exposure * ds3ea::k_exposure_bias);
	EXPECT_FLOAT_EQ(constants.white, ds3ea::Settings::TonemapperValues::k_default_white);
	EXPECT_FLOAT_EQ(constants.bloom_strength, 1.0f);
	EXPECT_EQ(constants.has_vignette, 1u);
	EXPECT_EQ(constants.has_color_matrix, 0u);
	EXPECT_EQ(constants.is_graded, 1u);
	EXPECT_EQ(constants.apply_lut, 1u);
	EXPECT_EQ(constants.dither, 1u);
}

TEST(TonemapConstants, SettingsAreClampedToTheirRanges)
{
	const ds3ea::shader::TonemapConstants constants = ds3ea::pack_tonemap_constants(
		{ .exposure = 100.0f, .white = 0.0f, .toe = -1.0f, .shoulder = 9.0f, .bloom = 2.0f, .dither = false,
			.apply_lut = false },
		{}, false);

	EXPECT_FLOAT_EQ(constants.exposure, ds3ea::k_exposure_range.max * ds3ea::k_exposure_bias);
	EXPECT_FLOAT_EQ(constants.white, ds3ea::k_white_range.min);
	EXPECT_FLOAT_EQ(constants.toe, ds3ea::k_toe_range.min);
	EXPECT_FLOAT_EQ(constants.shoulder, ds3ea::k_shoulder_range.max);
	EXPECT_FLOAT_EQ(constants.bloom_strength, ds3ea::k_bloom_range.max);
	EXPECT_EQ(constants.dither, 0u);
	EXPECT_EQ(constants.apply_lut, 0u);
	EXPECT_EQ(constants.is_graded, 0u);
}
} // namespace
