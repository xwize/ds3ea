#include "utils/shader_reflection.hpp"
#include "utils/error.hpp"

#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <bitset>
#include <cctype>
#include <cstddef>
#include <string_view>

namespace
{
using namespace ds3ea;

/**
 * @param reflection D3D's view of the compiled shader.
 * @param bound_resources How many constant buffers, textures and samplers the shader binds.
 * @param[in,out] info Gets the constant buffer and texture names added.
 */
void fill_bindings_into_info(ID3D11ShaderReflection& reflection, UINT bound_resources, ShaderReflectionInfo& info)
{
	for (UINT index = 0; index < bound_resources; ++index)
	{
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): filled by the call; = {} trips an enum check
		D3D11_SHADER_INPUT_BIND_DESC bind;

		if (FAILED(reflection.GetResourceBindingDesc(index, &bind)))
			continue;

		if (bind.Type == D3D_SIT_CBUFFER)
			info.cbuffers.emplace_back(bind.Name);
		else if (bind.Type != D3D_SIT_SAMPLER)
			info.resources.emplace_back(bind.Name);
	}
}

/**
 * @param reflection D3D's view of the compiled shader.
 * @param[in,out] info Gets the $Globals variables the shader uses added.
 */
void fill_used_globals_into_info(ID3D11ShaderReflection& reflection, ShaderReflectionInfo& info)
{
	ID3D11ShaderReflectionConstantBuffer* const buffer = reflection.GetConstantBufferByName("$Globals");
	D3D11_SHADER_BUFFER_DESC desc = {};

	if (buffer == nullptr || FAILED(buffer->GetDesc(&desc)))
		return;

	for (UINT index = 0; index < desc.Variables; ++index)
	{
		D3D11_SHADER_VARIABLE_DESC variable = {};

		if (SUCCEEDED(buffer->GetVariableByIndex(index)->GetDesc(&variable)) && (variable.uFlags & D3D_SVF_USED) != 0)
			info.globals.emplace_back(variable.Name);
	}
}

/**
 * @param name A semantic name from the compiled code, in the case it was written in.
 * @returns True if it's TEXCOORD; semantics are case-insensitive in HLSL.
 */
[[nodiscard]] bool is_texcoord(std::string_view name)
{
	constexpr std::string_view k_texcoord = "TEXCOORD";
	return std::ranges::equal(name, k_texcoord,
		[](char letter, char upper) { return std::toupper(static_cast<unsigned char>(letter)) == upper; });
}

/**
 * @param reflection D3D's view of the compiled shader.
 * @param input_count How many inputs its signature declares.
 * @returns Bit n set for each TEXCOORDn input the shader reads.
 */
[[nodiscard]] std::bitset<k_max_texcoords> determine_texcoords_used(ID3D11ShaderReflection& reflection,
	UINT input_count)
{
	std::bitset<k_max_texcoords> texcoords;

	for (UINT index = 0; index < input_count; ++index)
	{
		D3D11_SIGNATURE_PARAMETER_DESC input = {};
		const bool read = SUCCEEDED(reflection.GetInputParameterDesc(index, &input)) && input.ReadWriteMask != 0;

		if (read && is_texcoord(input.SemanticName) && input.SemanticIndex < k_max_texcoords)
			texcoords.set(input.SemanticIndex);
	}

	return texcoords;
}
} // namespace

namespace ds3ea
{
Result<ShaderReflectionInfo> reflect_pixel_shader(const void* code, std::size_t size)
{
	Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflection;

	if (code == nullptr || FAILED(D3DReflect(code, size, IID_PPV_ARGS(&reflection))))
		return make_error(ErrorCategory::invalid_data, "D3D can't read {} bytes of shader code", size);

	D3D11_SHADER_DESC desc = {};

	if (FAILED(reflection->GetDesc(&desc)))
		return make_error(ErrorCategory::invalid_data, "no shader description in {} bytes of shader code", size);

	ShaderReflectionInfo info;
	fill_bindings_into_info(*reflection.Get(), desc.BoundResources, info);

	if (std::ranges::find(info.cbuffers, "$Globals") != info.cbuffers.end())
		fill_used_globals_into_info(*reflection.Get(), info);

	for (UINT index = 0; index < desc.OutputParameters; ++index)
	{
		D3D11_SIGNATURE_PARAMETER_DESC output = {};

		if (SUCCEEDED(reflection->GetOutputParameterDesc(index, &output)) && output.SystemValueType == D3D_NAME_TARGET)
			++info.output_count;
	}

	info.texcoords_used = determine_texcoords_used(*reflection.Get(), desc.InputParameters);
	return info;
}
} // namespace ds3ea
