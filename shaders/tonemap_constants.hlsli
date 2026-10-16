/**
 * The constant buffer of our tonemap shader (tonemap.hlsl, cbuffer Tonemapper at b13), declared once for both HLSL and
 * C++: tonemap.hlsl and src/tonemap_model.hpp include this file. Only 4-byte scalars, so HLSL's packing rules and C++
 * lay it out identically, padded to a whole 16-byte register. The uint fields are flags: 0 off, 1 on.
 */
#ifdef __cplusplus
#pragma once

#include <cstdint>

namespace ds3ea::shader
{
using uint = std::uint32_t;
#endif

struct TonemapConstants
{
	// Input scale before the curve: the exposure setting times its bias, on top of the game's own exposure
	float exposure;
	// Input level that maps to white (Hable's W)
	float white;
	// Toe strength (Hable's D): higher lifts and flattens the darks
	float toe;
	// Shoulder strength (Hable's A): higher compresses the highlights sooner
	float shoulder;
	// Bloom strength: 0 none, 1 the game's
	float bloom_strength;
	// The game's variant applies the radial vignette
	uint has_vignette;
	// The game's variant applies its colour matrix
	uint has_color_matrix;
	// The game's grading inputs from the last grading draw are bound: apply its screen fade (and with apply_lut its
	// LUT) here, at full precision, instead of at its 8-bit grading draw
	uint is_graded;
	// Apply the game's colour grading LUT
	uint apply_lut;
	// Add the triangular dither, last
	uint dither;
	uint padding_0;
	uint padding_1;
};

#ifdef __cplusplus
static_assert(sizeof(TonemapConstants) % 16 == 0, "a constant buffer is a whole number of 16-byte registers");
} // namespace ds3ea::shader
#endif
