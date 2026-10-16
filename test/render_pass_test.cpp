#include "render_pass.hpp"

#include <gtest/gtest.h>

namespace
{
using ds3ea::infer_render_pass_for_shader;
using ds3ea::RenderPass;

TEST(InferRenderPass, TonemapUsesTheTonemapCurveGlobal)
{
	EXPECT_EQ(infer_render_pass_for_shader({ .globals = { "fParam_TonemapMaxMappingLuminance" } }),
		RenderPass::tonemap);
}

TEST(InferRenderPass, ColorGradingReadsThe3DLut)
{
	EXPECT_EQ(infer_render_pass_for_shader({ .resources = { "atex3D_Texture" } }), RenderPass::color_grading);
}

TEST(InferRenderPass, DecalWinsOverGBufferMaterial)
{
	EXPECT_EQ(infer_render_pass_for_shader({ .cbuffers = { "cbDecalParam", "cbMatDynParam" }, .output_count = 4 }),
		RenderPass::decal);
}

TEST(InferRenderPass, GBufferIsAMaterialWritingFourTargets)
{
	EXPECT_EQ(infer_render_pass_for_shader({ .cbuffers = { "cbMatDynParam" }, .output_count = 4 }),
		RenderPass::gbuffer);
	EXPECT_EQ(infer_render_pass_for_shader({ .cbuffers = { "cbMtdParam" }, .output_count = 5 }), RenderPass::gbuffer);
}

TEST(InferRenderPass, AnythingElseIsUnknown)
{
	EXPECT_EQ(infer_render_pass_for_shader({ .cbuffers = { "cbMatDynParam" }, .output_count = 3 }),
		RenderPass::unknown);
	EXPECT_EQ(infer_render_pass_for_shader({ .output_count = 4 }), RenderPass::unknown);
}
} // namespace
