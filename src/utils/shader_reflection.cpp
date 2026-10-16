#include "utils/shader_reflection.hpp"
#include "utils/error.hpp"

#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace
{
using namespace ds3ea;

/**
 * @param reflection D3D's view of the compiled shader.
 * @param bound_resources How many constant buffers, textures and samplers the shader binds.
 * @param info Gets the constant buffer and texture names.
 */
void read_bindings(ID3D11ShaderReflection& reflection, UINT bound_resources, ShaderReflectionInfo& info)
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
 * @param info Gets the $Globals variables the shader uses.
 */
void read_used_globals(ID3D11ShaderReflection& reflection, ShaderReflectionInfo& info)
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
 * @param reflection D3D's view of the compiled shader.
 * @param input_count How many inputs its signature declares.
 * @returns Bit n set for each TEXCOORDn input the shader reads.
 */
[[nodiscard]] std::uint32_t read_texcoords_read(ID3D11ShaderReflection& reflection, UINT input_count)
{
	// One bit per TEXCOORD index in the result
	constexpr UINT k_max_texcoords = 32;
	std::uint32_t texcoords = 0;

	for (UINT index = 0; index < input_count; ++index)
	{
		D3D11_SIGNATURE_PARAMETER_DESC input = {};
		const bool read = SUCCEEDED(reflection.GetInputParameterDesc(index, &input)) && input.ReadWriteMask != 0;

		// Semantics are case-insensitive in HLSL, and the compiled code keeps the case they were written in
		if (read && _stricmp(input.SemanticName, "TEXCOORD") == 0 && input.SemanticIndex < k_max_texcoords)
			texcoords |= 1u << input.SemanticIndex;
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
	read_bindings(*reflection.Get(), desc.BoundResources, info);

	if (std::ranges::find(info.cbuffers, "$Globals") != info.cbuffers.end())
		read_used_globals(*reflection.Get(), info);

	for (UINT index = 0; index < desc.OutputParameters; ++index)
	{
		D3D11_SIGNATURE_PARAMETER_DESC output = {};

		if (SUCCEEDED(reflection->GetOutputParameterDesc(index, &output)) && output.SystemValueType == D3D_NAME_TARGET)
			++info.output_count;
	}

	info.texcoords_read = read_texcoords_read(*reflection.Get(), desc.InputParameters);
	return info;
}
} // namespace ds3ea
