#include "tonemapper.hpp"
#include "render_pass.hpp"
#include "settings.hpp"
#include "tonemap_model.hpp"
#include "tonemap_renderer.hpp"
#include "utils/error.hpp"
#include "utils/logging.hpp"
#include "utils/result.hpp"
#include "utils/runtime_error.hpp"
#include "utils/shader_reflection.hpp"

#include <d3d11.h>
#include <reshade.hpp>
#include <wrl/client.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>

namespace
{
using namespace ds3ea;

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

DrawAction Tonemapper::on_tonemap_draw(reshade::api::command_list* cmd_list, const DrawCall& draw, bool claimed)
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

DrawAction Tonemapper::on_color_grading_draw(reshade::api::command_list* cmd_list, const DrawCall& draw, bool claimed)
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
