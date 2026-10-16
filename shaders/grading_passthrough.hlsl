/**
 * Replaces the game's colour grading draw (Tonemapper::on_color_grading_draw) once our tonemap has already applied
 * the grading at full precision: copies the tonemap's output to the grading pass's target unchanged. The game's
 * vertex shader, quad and input (t0, s0) stay bound, and the game's draw is repeated.
 */

Texture2D<float4> t_frame : register(t0);
SamplerState s_frame : register(s0);

float4 main(float4 uv : TEXCOORD0) : SV_Target
{
	return t_frame.Sample(s_frame, uv.xy);
}
