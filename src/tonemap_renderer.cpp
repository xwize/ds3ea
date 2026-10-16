#include "gen/grading_passthrough.h"
#include "gen/tonemap.h"
#include "tonemap_renderer.hpp"
#include "listeners/render_pass_listener.hpp"
#include "tonemap_constants.hlsli"
#include "utils/error.hpp"
#include "utils/result.hpp"
#include "utils/runtime_error.hpp"

#include <fmt/format.h>

#include <d3d11.h>
#include <wrl/client.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace
{
using namespace ds3ea;
using Microsoft::WRL::ComPtr;

// Our registers, as declared in shaders/tonemap.hlsl; the game's tonemap uses none of them
constexpr UINT k_grading_globals_slot = 12;
constexpr UINT k_constants_slot = 13;
constexpr UINT k_lut_slot = 13;
constexpr UINT k_lut_sampler_slot = 13;

// Where the game's colour grading draw binds its inputs (docs: the game's grading pixel shader)
constexpr UINT k_game_grading_globals_slot = 0;
constexpr UINT k_game_lut_slot = 16;

/**
 * @param result An HRESULT.
 * @returns It as an unsigned number, for logging in hex.
 */
[[nodiscard]] std::uint32_t to_code(HRESULT result)
{
	return static_cast<std::uint32_t>(result);
}

/**
 * @param device The game's device.
 * @param bytecode A compiled pixel shader, from src/gen/.
 * @param name The shader's name, for the error.
 * @returns The shader.
 * @throws RuntimeError (device) if it can't be created.
 */
[[nodiscard]] ComPtr<ID3D11PixelShader> create_pixel_shader(ID3D11Device& device,
	std::span<const std::uint8_t> bytecode, std::string_view name)
{
	ComPtr<ID3D11PixelShader> shader;
	const HRESULT result = device.CreatePixelShader(bytecode.data(), bytecode.size(), nullptr, shader.GetAddressOf());

	if (FAILED(result))
		throw RuntimeError({
			.category = ErrorCategory::device,
			.message = fmt::format("can't create the {} pixel shader: {:#x}", name, to_code(result)),
		});

	return shader;
}

/**
 * Makes copy a texture the game's LUT can be copied into, recreating it (and its view) if the LUT's size or format
 * changed.
 * @param device The game's device.
 * @param game_lut The game's LUT.
 * @param[in,out] copy Our copy; created or replaced as needed.
 * @param[in,out] view Its shader resource view, replaced with it.
 * @returns Nothing, or a device Error.
 */
[[nodiscard]] Result<void> match_lut_copy(ID3D11Device& device, ID3D11Texture3D& game_lut,
	ComPtr<ID3D11Texture3D>& copy, ComPtr<ID3D11ShaderResourceView>& view)
{
	D3D11_TEXTURE3D_DESC desc = {};
	game_lut.GetDesc(&desc);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	if (copy != nullptr)
	{
		D3D11_TEXTURE3D_DESC current = {};
		copy->GetDesc(&current);

		if (std::memcmp(&current, &desc, sizeof(desc)) == 0)
			return {};
	}

	view.Reset();
	copy.Reset();
	HRESULT result = device.CreateTexture3D(&desc, nullptr, copy.GetAddressOf());

	if (SUCCEEDED(result))
		result = device.CreateShaderResourceView(copy.Get(), nullptr, view.GetAddressOf());

	if (FAILED(result))
	{
		copy.Reset();
		return make_error(ErrorCategory::device, "can't create the copy of the game's LUT: {:#x}", to_code(result));
	}

	return {};
}

/**
 * Makes copy a constant buffer the game's $Globals can be copied into, recreating it if their size changed.
 * @param device The game's device.
 * @param game_buffer The game's constant buffer.
 * @param[in,out] copy Our copy; created or replaced as needed.
 * @returns Nothing, or a device Error.
 */
[[nodiscard]] Result<void> match_buffer_copy(ID3D11Device& device, ID3D11Buffer& game_buffer,
	ComPtr<ID3D11Buffer>& copy)
{
	D3D11_BUFFER_DESC desc = {};
	game_buffer.GetDesc(&desc);

	if (copy != nullptr)
	{
		D3D11_BUFFER_DESC current = {};
		copy->GetDesc(&current);

		if (current.ByteWidth == desc.ByteWidth)
			return {};
	}

	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	copy.Reset();
	const HRESULT result = device.CreateBuffer(&desc, nullptr, copy.GetAddressOf());

	if (FAILED(result))
		return make_error(ErrorCategory::device, "can't create the copy of the grading $Globals: {:#x}",
			to_code(result));

	return {};
}

/**
 * Records the game's draw again, with whatever is bound now.
 * @param context The context recording it.
 * @param draw The game's draw.
 */
void repeat_draw(ID3D11DeviceContext& context, const DrawCallArgs& draw)
{
	assert(draw.kind != DrawKind::indirect && "Bug: repeating an indirect draw, whose arguments aren't known");

	if (draw.kind == DrawKind::draw_indexed)
		context.DrawIndexedInstanced(draw.count, draw.instance_count, draw.first, draw.vertex_offset,
			draw.first_instance);
	else
		context.DrawInstanced(draw.count, draw.instance_count, draw.first, draw.first_instance);
}

/**
 * The pixel shader state our draws change: saved when constructed and put back when destroyed, so the game's
 * following draws find what it bound.
 */
class SavedPixelShaderState
{
public:
	/** @param context The context to save the state of; must outlive this. */
	explicit SavedPixelShaderState(ID3D11DeviceContext& context)
		: m_context(&context)
	{
		context.PSGetShader(m_shader.GetAddressOf(), nullptr, nullptr);
		context.PSGetConstantBuffers(k_grading_globals_slot, 1, m_grading_globals.GetAddressOf());
		context.PSGetConstantBuffers(k_constants_slot, 1, m_constants.GetAddressOf());
		context.PSGetShaderResources(k_lut_slot, 1, m_lut_view.GetAddressOf());
		context.PSGetSamplers(k_lut_sampler_slot, 1, m_lut_sampler.GetAddressOf());
	}

	~SavedPixelShaderState()
	{
		m_context->PSSetShader(m_shader.Get(), nullptr, 0);
		m_context->PSSetConstantBuffers(k_grading_globals_slot, 1, m_grading_globals.GetAddressOf());
		m_context->PSSetConstantBuffers(k_constants_slot, 1, m_constants.GetAddressOf());
		m_context->PSSetShaderResources(k_lut_slot, 1, m_lut_view.GetAddressOf());
		m_context->PSSetSamplers(k_lut_sampler_slot, 1, m_lut_sampler.GetAddressOf());
	}

	/** Not copyable or movable: it puts the state back once. */
	SavedPixelShaderState(const SavedPixelShaderState&) = delete;
	SavedPixelShaderState& operator=(const SavedPixelShaderState&) = delete;
	SavedPixelShaderState(SavedPixelShaderState&&) = delete;
	SavedPixelShaderState& operator=(SavedPixelShaderState&&) = delete;

private:
	ID3D11DeviceContext* m_context;
	ComPtr<ID3D11PixelShader> m_shader;
	ComPtr<ID3D11Buffer> m_grading_globals;
	ComPtr<ID3D11Buffer> m_constants;
	ComPtr<ID3D11ShaderResourceView> m_lut_view;
	ComPtr<ID3D11SamplerState> m_lut_sampler;
};
} // namespace

namespace ds3ea
{
TonemapRenderer::TonemapRenderer(ID3D11Device& device)
	: m_device(&device)
	, m_tonemap_shader(create_pixel_shader(device, k_tonemap, "tonemap"))
	, m_passthrough_shader(create_pixel_shader(device, k_grading_passthrough, "grading pass-through"))
{
	const D3D11_BUFFER_DESC constants_desc = {
		.ByteWidth = sizeof(shader::TonemapConstants),
		.Usage = D3D11_USAGE_DYNAMIC,
		.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
		.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
		.MiscFlags = 0,
		.StructureByteStride = 0,
	};
	HRESULT result = device.CreateBuffer(&constants_desc, nullptr, m_constants.GetAddressOf());

	if (FAILED(result))
		throw RuntimeError({
			.category = ErrorCategory::device,
			.message = fmt::format("can't create the tonemap constant buffer: {:#x}", to_code(result)),
		});

	// Linear and clamped, so the LUT's scale and offset land on its texel centres as at the game's own lookup
	const D3D11_SAMPLER_DESC sampler_desc = {
		.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
		.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
		.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
		.MipLODBias = 0.0f,
		.MaxAnisotropy = 1,
		.ComparisonFunc = D3D11_COMPARISON_NEVER,
		.BorderColor = {},
		.MinLOD = 0.0f,
		.MaxLOD = D3D11_FLOAT32_MAX,
	};
	result = device.CreateSamplerState(&sampler_desc, m_lut_sampler.GetAddressOf());

	if (FAILED(result))
		throw RuntimeError({
			.category = ErrorCategory::device,
			.message = fmt::format("can't create the LUT sampler: {:#x}", to_code(result)),
		});
}

Result<void> TonemapRenderer::capture_grading(ID3D11DeviceContext& context)
{
	ComPtr<ID3D11ShaderResourceView> game_lut_view;
	context.PSGetShaderResources(k_game_lut_slot, 1, game_lut_view.GetAddressOf());
	ComPtr<ID3D11Buffer> game_globals;
	context.PSGetConstantBuffers(k_game_grading_globals_slot, 1, game_globals.GetAddressOf());

	if (game_lut_view == nullptr || game_globals == nullptr)
		return make_error(ErrorCategory::not_found, "the colour grading draw has no LUT or $Globals bound");

	ComPtr<ID3D11Resource> game_lut_resource;
	game_lut_view->GetResource(game_lut_resource.GetAddressOf());
	ComPtr<ID3D11Texture3D> game_lut;

	if (FAILED(game_lut_resource.As(&game_lut)))
		return make_error(ErrorCategory::invalid_data, "the colour grading LUT isn't a 3D texture");

	return match_lut_copy(*m_device.Get(), *game_lut.Get(), m_lut, m_lut_view)
		.and_then([this, &game_globals]
			{ return match_buffer_copy(*m_device.Get(), *game_globals.Get(), m_grading_globals); })
		.map([this, &context, &game_lut, &game_globals]
		{
			context.CopyResource(m_lut.Get(), game_lut.Get());
			context.CopyResource(m_grading_globals.Get(), game_globals.Get());
		});
}

bool TonemapRenderer::has_grading() const
{
	return m_lut_view != nullptr && m_grading_globals != nullptr;
}

Result<void> TonemapRenderer::draw_tonemap(ID3D11DeviceContext& context, const DrawCallArgs& draw,
	const shader::TonemapConstants& constants)
{
	assert((constants.is_graded == 0 || has_grading()) && "Bug: grading the tonemap before any grading was captured");

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	const HRESULT result = context.Map(m_constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	if (FAILED(result))
		return make_error(ErrorCategory::device, "can't write the tonemap constants: {:#x}", to_code(result));

	std::memcpy(mapped.pData, &constants, sizeof(constants));
	context.Unmap(m_constants.Get(), 0);

	const SavedPixelShaderState saved(context);
	context.PSSetShader(m_tonemap_shader.Get(), nullptr, 0);
	context.PSSetConstantBuffers(k_constants_slot, 1, m_constants.GetAddressOf());
	context.PSSetConstantBuffers(k_grading_globals_slot, 1, m_grading_globals.GetAddressOf());
	context.PSSetShaderResources(k_lut_slot, 1, m_lut_view.GetAddressOf());
	context.PSSetSamplers(k_lut_sampler_slot, 1, m_lut_sampler.GetAddressOf());
	repeat_draw(context, draw);
	return {};
}

void TonemapRenderer::draw_grading_passthrough(ID3D11DeviceContext& context, const DrawCallArgs& draw)
{
	const SavedPixelShaderState saved(context);
	context.PSSetShader(m_passthrough_shader.Get(), nullptr, 0);
	repeat_draw(context, draw);
}
} // namespace ds3ea
