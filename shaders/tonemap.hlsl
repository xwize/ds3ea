/**
 * Our tonemap, drawn by TonemapRenderer in place of the game's YEBIS tonemap draw (Tonemapper::on_tonemap_draw). The
 * game's vertex shader, fullscreen quad, inputs and $Globals stay bound; only this pixel shader and our registers
 * (b12, b13, t13, s13, which the game's tonemap doesn't use) are added, and the game's draw is repeated.
 *
 * The same inputs and steps as the game's (tint remap, bloom, exposure, the vignette and colour matrix of its
 * variant, gamma), with Hable's filmic curve in place of the game's (1 - e^-kx)(1 - m e^-kx)^2, which crushes the
 * darks. Then the game's colour grading (screen fade, 3D LUT), copied from its grading draw, at full precision rather
 * than from this pass's 8-bit output, and a triangular dither last, in place of the game's 4x4 pattern.
 */

#include "tonemap_constants.hlsli"
#include "yebis_globals.hlsli"

// The game's tonemap inputs, as it binds them
Texture2D<float4> t_scene : register(t0);
Texture2D<float4> t_glare : register(t1);
Texture2D<float4> t_tint : register(t2);
SamplerState s_scene : register(s0);
SamplerState s_glare : register(s1);
SamplerState s_tint : register(s2);

cbuffer GameGlobals : register(b0)
{
	YebisGlobals game;
}

// $Globals at the game's last grading draw: afRGBA_Modulate[0] is the screen fade, afRGBA_Offset[0..1] the LUT's scale
// and offset onto its texel centres
cbuffer GradingGlobals : register(b12)
{
	YebisGlobals grading;
}

cbuffer Tonemapper : register(b13)
{
	TonemapConstants settings;
}

// The game's colour grading LUT (16x16x16), copied at its grading draw, with a linear clamping sampler
Texture3D<float4> t_lut : register(t13);
SamplerState s_lut : register(s13);

// Hable's curve constants: linear strength, linear angle, toe numerator and denominator
static const float k_linear_strength = 0.50;
static const float k_linear_angle = 0.10;
static const float k_toe_numerator = 0.02;
static const float k_toe_denominator = 0.30;

// The 8-bit target's step, which the dither spreads each value over
static const float k_target_step = 1.0 / 255.0;

// Where the dither's second, independent hash is taken: the sum of two uniform values is triangular
static const uint2 k_second_hash_offset = uint2(1913u, 7919u);

/**
 * Hable's filmic curve, with adjustable shoulder (A) and toe (D) strength.
 * @param x Linear input, >= 0.
 * @returns The curve's output, not yet normalised to white.
 */
float3 hable(float3 x)
{
	const float a = settings.shoulder;
	const float d = settings.toe;
	return (x * (a * x + k_linear_angle * k_linear_strength) + d * k_toe_numerator) /
		(x * (a * x + k_linear_strength) + d * k_toe_denominator) - k_toe_numerator / k_toe_denominator;
}

/**
 * An integer hash of a pixel position.
 * @param pixel The pixel.
 * @returns A value in [0, 1].
 */
float hash(uint2 pixel)
{
	uint h = pixel.x * 1973u + pixel.y * 9277u + 26699u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return float((h ^ (h >> 16)) & 0xFFFFu) / 65535.0;
}

/**
 * The game's tint texture remapped, as the game does, into an additive bloom tint and a scene multiplier.
 * @param tint The tint texture's colour.
 * @param bloom_tint Gets the bloom tint.
 * @param scene_tint Gets the scene multiplier.
 */
void remap_tint(float3 tint, out float3 bloom_tint, out float3 scene_tint)
{
	bloom_tint = tint * game.afRGBA_Offset[0].x + game.afRGBA_Offset[0].y;
	float average = dot(bloom_tint, 1.0 / 3.0);
	bloom_tint = saturate(game.afRGBA_Offset[1].x * (bloom_tint - average) + average) * game.afRGBA_Offset[0].z +
		game.afRGBA_Offset[0].w;

	scene_tint = tint * game.afRGBA_Offset[2].x + game.afRGBA_Offset[2].y;
	average = dot(scene_tint, 1.0 / 3.0);
	scene_tint = (game.afRGBA_Offset[3].x * (scene_tint - average) + average) * game.afRGBA_Offset[2].z +
		game.afRGBA_Offset[2].w;
}

/**
 * The inputs are float4, one register each, as the game's vertex shader writes them: with float2 inputs the compiler
 * would pack two per register, which no longer lines up with it. TEXCOORD4 (the game's dither pattern coordinate) is
 * declared only to keep that line-up.
 */
float4 main(float4 uv_scene : TEXCOORD0, float4 uv_glare : TEXCOORD1, float4 vignette_coord : TEXCOORD2,
	float4 uv_tint : TEXCOORD3, float4 uv_dither : TEXCOORD4, float4 position : SV_Position) : SV_Target
{
	float3 bloom_tint;
	float3 scene_tint;
	remap_tint(t_tint.Sample(s_tint, uv_tint.xy).rgb, bloom_tint, scene_tint);

	const float3 glare = t_glare.Sample(s_glare, uv_glare.xy).rgb;
	const float3 bloom = saturate((glare * scene_tint + bloom_tint) * game.afRGBA_Modulate[0].y);
	const float4 scene = t_scene.Sample(s_scene, uv_scene.xy);
	const float alpha = saturate(dot(scene, game.afRGBA_Modulate[1]));

	// The game's radial vignette: (1 - r^2)^4, from the centred coordinate
	float falloff = 1.0;
	if (settings.has_vignette != 0)
	{
		falloff = saturate(1.0 - dot(vignette_coord.xy, vignette_coord.xy));
		falloff *= falloff;
		falloff *= falloff;
	}

	const float3 hdr = max(scene.rgb * scene_tint * game.afRGBA_Modulate[0].x * falloff, 0.0);
	float3 color = saturate(hable(hdr * settings.exposure) / hable(settings.white));

	// The bloom screen-blended, then the variant's colour matrix and the game's gamma
	color = color + (1.0 - color) * (bloom * settings.bloom_strength);
	if (settings.has_color_matrix != 0)
		color = transform(game.m44_ColorTransformMatrix, float4(color, 1.0)).rgb;

	float4 result = float4(pow(saturate(color), game.fParam_GammaCorrection.x), alpha);

	if (settings.is_graded != 0)
	{
		result *= grading.afRGBA_Modulate[0];
		if (settings.apply_lut != 0)
		{
			const float3 lut_coord = saturate(result.rgb) * grading.afRGBA_Offset[0].xyz + grading.afRGBA_Offset[1].xyz;
			result.rgb = t_lut.SampleLevel(s_lut, lut_coord, 0).rgb;
		}
	}

	// A triangular dither of +-1 step of the 8-bit target hides banding without visible noise
	if (settings.dither != 0)
	{
		const uint2 pixel = uint2(position.xy);
		result.rgb += (hash(pixel) + hash(pixel + k_second_hash_offset) - 1.0) * k_target_step;
	}

	return result;
}
