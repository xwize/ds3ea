#include "tonemapper.hpp"
#include "render_pass.hpp"
#include "settings.hpp"
#include "tonemap_constants.hlsli"
#include "tonemap_renderer.hpp"
#include "utils/error.hpp"
#include "utils/logging.hpp"
#include "utils/result.hpp"
#include "utils/runtime_error.hpp"
#include "utils/shader_reflection.hpp"

#include <d3d11.h>
#include <reshade.hpp>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>

namespace
{
using namespace ds3ea;

/**
 * The curve's input scale at exposure 1: puts the game's mid-grey (0.5 after its exposure) at about the same output
 * level as the game's curve (0.49 before gamma).
 */
constexpr float k_exposure_bias = 4.0f;

// The game's vignette coordinate, centred on the screen
constexpr std::size_t k_vignette_texcoord = 2;

/**
 * @param game_shader What a game tonemap pixel shader declares.
 * @returns Its variant: the vignette if it reads TEXCOORD2, the colour matrix if it uses m44_ColorTransformMatrix.
 */
[[nodiscard]] TonemapVariant get_tonemap_variant(const ShaderReflectionInfo& game_shader)
{
	const auto& globals = game_shader.globals;
	return {
		.has_vignette = game_shader.texcoords_used.test(k_vignette_texcoord),
		.has_color_matrix = std::ranges::find(globals, "m44_ColorTransformMatrix") != globals.end(),
	};
}

/**
 * @param settings The tonemap settings, clamped to their ranges.
 * @param variant The variant of the game's tonemap being replaced.
 * @param is_graded True if the game's grading inputs (fade, LUT) are bound for the shader to apply.
 * @returns The constants for shaders/tonemap.hlsl.
 */
[[nodiscard]] shader::TonemapConstants pack_tonemap_constants(const Settings::TonemapperValues& settings,
	TonemapVariant variant, bool is_graded)
{
	return {
		.exposure = std::clamp(settings.exposure, k_exposure_range.min, k_exposure_range.max) * k_exposure_bias,
		.white = std::clamp(settings.white, k_white_range.min, k_white_range.max),
		.toe = std::clamp(settings.toe, k_toe_range.min, k_toe_range.max),
		.shoulder = std::clamp(settings.shoulder, k_shoulder_range.min, k_shoulder_range.max),
		.bloom_strength = std::clamp(settings.bloom, k_bloom_range.min, k_bloom_range.max),
		.has_vignette = static_cast<shader::uint>(variant.has_vignette),
		.has_color_matrix = static_cast<shader::uint>(variant.has_color_matrix),
		.is_graded = static_cast<shader::uint>(is_graded),
		.apply_lut = static_cast<shader::uint>(settings.apply_lut),
		.dither = static_cast<shader::uint>(settings.dither),
		.padding_0 = 0,
		.padding_1 = 0,
	};
}

/**
 * @param cmd_list A command list ReShade gave us.
 * @returns The game's D3D11 context behind it (the deferred context recording, or the immediate one).
 */
[[nodiscard]] ID3D11DeviceContext& get_d3d11_context(reshade::api::command_list* cmd_list)
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr): ReShade's API
	return *reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());
}
} // namespace

namespace ds3ea
{
Tonemapper::Tonemapper(const Settings& settings)
	: m_settings(&settings)
	, m_is_replacing(settings.get().tonemapper.replace_game_tonemap)
{
}

void Tonemapper::on_device_created(reshade::api::device* device)
{
	const std::lock_guard lock(m_renderer_mutex);
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr): ReShade's API
	m_device = reinterpret_cast<ID3D11Device*>(device->get_native());

	try
	{
		m_renderer.emplace(*m_device.Get());
		log::info("Tonemapper: ready, {} the game's tonemap", m_is_replacing ? "replacing" : "keeping");
	}
	catch (const RuntimeError& exception)
	{
		if (exception.get_error().category != ErrorCategory::device)
			throw;

		log::error("Tonemapper: {}; the game's tonemap stays", exception.get_error());
	}
}

void Tonemapper::on_device_destroyed(reshade::api::device* /*device*/)
{
	const std::lock_guard lock(m_renderer_mutex);
	m_renderer.reset();
	m_device.Reset();
}

void Tonemapper::on_pipeline_created(std::uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects,
	std::uint64_t pipeline)
{
	for (const reshade::api::pipeline_subobject& subobject : std::span(subobjects, subobject_count))
	{
		if (subobject.type != reshade::api::pipeline_subobject_type::pixel_shader || subobject.data == nullptr)
			continue;

		// The tracker logs shaders it can't read, so they're only skipped here
		const auto* const desc = static_cast<const reshade::api::shader_desc*>(subobject.data);
		const Result<ShaderReflectionInfo> info = reflect_pixel_shader(desc->code, desc->code_size);

		if (!info || infer_render_pass_for_shader(*info) != RenderPass::tonemap)
			continue;

		const TonemapVariant variant = get_tonemap_variant(*info);
		log::debug("Tonemapper: game tonemap shader {:#x}, vignette {}, colour matrix {}", pipeline,
			variant.has_vignette, variant.has_color_matrix);
		const std::unique_lock lock(m_variants_mutex);
		m_variants.insert_or_assign(pipeline, variant);
	}
}

void Tonemapper::on_pipeline_destroyed(std::uint64_t pipeline)
{
	const std::unique_lock lock(m_variants_mutex);
	m_variants.erase(pipeline);
}

void Tonemapper::on_tonemap_begin(reshade::api::command_list* cmd_list)
{
	static const log::Once first(log::Level::info, "Tonemapper: first tonemap begin, on command list {}",
		static_cast<const void*>(cmd_list));
}

DrawAction Tonemapper::on_tonemap_draw(reshade::api::command_list* cmd_list, const DrawCallArgs& draw, bool claimed)
{
	const Settings::TonemapperValues settings = m_settings->get().tonemapper;
	const bool should_replace = settings.replace_game_tonemap && !claimed && draw.kind != DrawKind::indirect;

	if (m_is_replacing.exchange(settings.replace_game_tonemap) != settings.replace_game_tonemap)
		log::info("Tonemapper: now {} the game's tonemap", settings.replace_game_tonemap ? "replacing" : "keeping");

	m_is_graded_this_frame = false;

	if (!should_replace)
		return DrawAction::keep;

	ID3D11DeviceContext& context = get_d3d11_context(cmd_list);
	const std::optional<TonemapVariant> variant = find_bound_variant(context);

	if (!variant)
	{
		static const log::Once unknown(log::Level::warning, "Tonemapper: the bound tonemap shader wasn't seen created; "
			"the game's tonemap runs");
		return DrawAction::keep;
	}

	const std::lock_guard lock(m_renderer_mutex);

	if (!m_renderer)
		return DrawAction::keep;

	const bool is_graded = m_renderer->has_grading();
	const Result<void> drawn =
		m_renderer->draw_tonemap(context, draw, pack_tonemap_constants(settings, *variant, is_graded));

	if (!drawn)
	{
		static const log::Once failed(log::Level::error, "Tonemapper: {}; the game's tonemap runs", drawn.error());
		return DrawAction::keep;
	}

	static const log::Once first(log::Level::info, "Tonemapper: first tonemap drawn, vignette {}, colour matrix {}, "
		"graded {}", variant->has_vignette, variant->has_color_matrix, is_graded);
	m_is_graded_this_frame = is_graded;
	return DrawAction::skip;
}

void Tonemapper::on_tonemap_end(reshade::api::command_list* cmd_list)
{
	static const log::Once first(log::Level::info, "Tonemapper: first tonemap end, on command list {}",
		static_cast<const void*>(cmd_list));
}

DrawAction Tonemapper::on_color_grading_draw(reshade::api::command_list* cmd_list, const DrawCallArgs& draw,
	bool claimed)
{
	const bool is_graded = m_is_graded_this_frame.exchange(false);

	if (!m_settings->get().tonemapper.replace_game_tonemap || claimed || draw.kind == DrawKind::indirect)
		return DrawAction::keep;

	ID3D11DeviceContext& context = get_d3d11_context(cmd_list);
	const std::lock_guard lock(m_renderer_mutex);

	if (!m_renderer)
		return DrawAction::keep;

	const Result<void> captured = m_renderer->capture_grading(context);

	if (!captured)
	{
		static const log::Once failed(log::Level::warning, "Tonemapper: {}; the game grades its own frames",
			captured.error());
	}

	if (!is_graded)
		return DrawAction::keep;

	m_renderer->draw_grading_passthrough(context, draw);
	return DrawAction::skip;
}

std::optional<TonemapVariant> Tonemapper::find_bound_variant(ID3D11DeviceContext& context) const
{
	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
	context.PSGetShader(shader.GetAddressOf(), nullptr, nullptr);
	// ReShade's handle of a D3D11 shader is its address
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): ReShade's handle convention
	const auto handle = reinterpret_cast<std::uint64_t>(shader.Get());
	const std::shared_lock lock(m_variants_mutex);
	const auto found = m_variants.find(handle);
	return found != m_variants.end() ? std::optional(found->second) : std::nullopt;
}
} // namespace ds3ea
