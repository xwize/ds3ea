/**
 * $Globals of the game's YEBIS post-processing shaders (tonemap, colour grading), as our shaders read them. It's one
 * large buffer they all share, bound at b0; each shader uses a few of its members. Declared up to the colour matrix,
 * with register numbers in the comments (Dark Souls III 1.15.x).
 */
struct YebisGlobals
{
	// 0-153: depth of field, lens distortion, texture coordinate tables
	float4 unused_0[154];
	// 154-185: [0] exposure (x) and bloom weight (y), or the screen fade at the grading draw; [1] alpha weights
	float4 afRGBA_Modulate[32];
	// 186-201: [0..3] the tint remap into a bloom tint and a scene tint, or the LUT's scale and offset at the grading
	// draw
	float4 afRGBA_Offset[16];
	// 202: gamma (x)
	float4 fParam_GammaCorrection;
	// 203-207: fRGBA_Constant, afRGBA_Constant[4]
	float4 unused_203[5];
	// 208
	float4 fParam_TonemapMaxMappingLuminance;
	// 209
	float4 fParam_BrightPassRemapFactor;
	// 210-213: one row per register, applied as mul(m44_ColorTransformMatrix, colour)
	row_major float4x4 m44_ColorTransformMatrix;
};
