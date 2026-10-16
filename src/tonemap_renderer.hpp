#pragma once

#include "listeners/render_pass_listener.hpp"
#include "tonemap_constants.hlsli"
#include "utils/result.hpp"

#include <d3d11.h>
#include <wrl/client.h>

namespace ds3ea
{
/**
 * Draws our tonemap (shaders/tonemap.hlsl) in place of the game's tonemap draw and, once that has applied the game's
 * colour grading, a pass-through (shaders/grading_passthrough.hlsl) in place of the game's grading draw. Each draw
 * keeps the game's vertex shader, quad and inputs bound, binds our pixel shader and registers, repeats the game's draw
 * and puts back what it changed, so the game's following draws find their own state.
 *
 * Owns its D3D objects, released with it: DS3 exits without destroying its device, so the add-on's unload is when.
 * Not thread-safe: Tonemapper uses it under a lock.
 */
class TonemapRenderer
{
public:
	/**
	 * Creates the shaders, the constant buffer and the LUT's sampler.
	 * @param device The game's device.
	 * @throws RuntimeError (device) if one can't be created.
	 */
	explicit TonemapRenderer(ID3D11Device& device);

	/**
	 * Copies the game's colour grading inputs (its LUT and $Globals) at its grading draw, for the next tonemap draw.
	 * @param context The context recording the grading draw, with the game's inputs bound.
	 * @returns Nothing, or a not_found Error if they aren't bound, or a device Error if the copies can't be created.
	 */
	[[nodiscard]] Result<void> capture_grading(ID3D11DeviceContext& context);

	/** @returns True once grading inputs have been captured, so draw_tonemap can apply them. */
	[[nodiscard]] bool has_grading() const;

	/**
	 * Draws our tonemap in place of the game's tonemap draw.
	 * @param context The context recording the game's tonemap draw, with its inputs bound.
	 * @param draw The game's draw, repeated with our shader; not indirect.
	 * @param constants Our shader's constants; is_graded only if has_grading().
	 * @returns Nothing, or a device Error if the constants can't be written (nothing is drawn).
	 */
	[[nodiscard]] Result<void> draw_tonemap(ID3D11DeviceContext& context, const DrawCall& draw,
		const shader::TonemapConstants& constants);

	/**
	 * Draws the pass-through in place of the game's colour grading draw.
	 * @param context The context recording the game's grading draw, with its input bound.
	 * @param draw The game's draw, repeated with our shader; not indirect.
	 */
	void draw_grading_passthrough(ID3D11DeviceContext& context, const DrawCall& draw);

private:
	Microsoft::WRL::ComPtr<ID3D11Device> m_device;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_tonemap_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_passthrough_shader;
	// Dynamic, rewritten for every tonemap draw
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_constants;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> m_lut_sampler;
	// Copies of the game's grading inputs from its last grading draw; created at the first one
	Microsoft::WRL::ComPtr<ID3D11Texture3D> m_lut;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_lut_view;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_grading_globals;
};
} // namespace ds3ea
