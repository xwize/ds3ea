#include "gen/tonemap.h"
#include "utils/shader_reflection.hpp"
#include "render_pass.hpp"
#include "tonemap_constants.hlsli"
#include "utils/result.hpp"

#include <gtest/gtest.h>

#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
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
bool contains(const std::vector<std::string>& names, std::string_view name)
{
	return std::ranges::find(names, name) != names.end();
}

TEST(ShaderReflection, ReadsOurTonemapShader)
{
	const ds3ea::Result<ds3ea::ShaderReflectionInfo> info =
		ds3ea::reflect_pixel_shader(ds3ea::k_tonemap.data(), ds3ea::k_tonemap.size());

	ASSERT_TRUE(info.has_value());
	EXPECT_TRUE(contains(info->cbuffers, "GameGlobals"));
	EXPECT_TRUE(contains(info->cbuffers, "GradingGlobals"));
	EXPECT_TRUE(contains(info->cbuffers, "Tonemapper"));
	EXPECT_TRUE(contains(info->resources, "t_scene"));
	EXPECT_TRUE(contains(info->resources, "t_lut"));
	EXPECT_EQ(info->output_count, 1u);
	// TEXCOORD0-3 are read; TEXCOORD4 is declared only to line up with the game's vertex shader
	EXPECT_EQ(info->texcoords_used, std::bitset<ds3ea::k_max_texcoords>(0b01111));
}

TEST(ShaderReflection, OurTonemapShaderIsNotTakenForTheGames)
{
	const ds3ea::Result<ds3ea::ShaderReflectionInfo> info =
		ds3ea::reflect_pixel_shader(ds3ea::k_tonemap.data(), ds3ea::k_tonemap.size());

	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(ds3ea::infer_render_pass_for_shader(*info), ds3ea::RenderPass::unknown);
}

TEST(ShaderReflection, CodeThatIsNotAShaderIsAnError)
{
	const std::array<std::uint8_t, 4> garbage = { 1, 2, 3, 4 };
	const ds3ea::Result<ds3ea::ShaderReflectionInfo> info = ds3ea::reflect_pixel_shader(garbage.data(), garbage.size());

	ASSERT_FALSE(info.has_value());
	EXPECT_EQ(info.error().category, ds3ea::ErrorCategory::invalid_data);
}

TEST(ShaderReflection, TonemapConstantsAreTheSizeOfTheShadersBuffer)
{
	Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflection;
	ASSERT_TRUE(SUCCEEDED(
		D3DReflect(ds3ea::k_tonemap.data(), ds3ea::k_tonemap.size(), IID_PPV_ARGS(reflection.GetAddressOf()))));
	D3D11_SHADER_BUFFER_DESC desc = {};

	ASSERT_TRUE(SUCCEEDED(reflection->GetConstantBufferByName("Tonemapper")->GetDesc(&desc)));
	EXPECT_EQ(desc.Size, sizeof(ds3ea::shader::TonemapConstants));
}
} // namespace
