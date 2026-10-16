#include "tonemapper.hpp"
#include "utils/logging.hpp"
#include "utils/reshade_with_imgui.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>

namespace
{
using namespace ds3ea;

// What the tonemap pass draws while we replace it: a solid colour, proving our own GPU work lands where it ran
constexpr std::array<float, 4> k_replacement_colour = { 0.0f, 1.0f, 0.0f, 1.0f };

/**
 * @param cmd_list A command list ReShade gave us.
 * @returns The game's D3D11 context behind it (the deferred context recording, or the immediate one).
 */
[[nodiscard]] ID3D11DeviceContext* get_d3d11_context(reshade::api::command_list* cmd_list)
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr): ReShade's API
	return reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());
}

/**
 * Stands in for the game's tonemap draw: clears the render target it was about to draw into.
 * @param context The context recording the tonemap draw, with the game's render target still bound.
 */
void draw_replacement(ID3D11DeviceContext& context)
{
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
	context.OMGetRenderTargets(1, target.GetAddressOf(), nullptr);

	if (target != nullptr)
		context.ClearRenderTargetView(target.Get(), k_replacement_colour.data());
}
} // namespace

namespace ds3ea
{
Tonemapper::Tonemapper(const Settings& settings)
	: m_settings(&settings)
	, m_replacing(settings.get().tonemapper.replace_game_tonemap)
{
}

void Tonemapper::on_device_created(reshade::api::device* device)
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr): ReShade's API
	m_device = reinterpret_cast<ID3D11Device*>(device->get_native());
	log::info("Tonemapper: device created, D3D11 feature level {:#x}, {} the game's tonemap",
		static_cast<std::uint32_t>(m_device->GetFeatureLevel()), m_replacing ? "replacing" : "keeping");
}

void Tonemapper::on_device_destroyed(reshade::api::device* /*device*/)
{
	m_device.Reset();
}

void Tonemapper::on_tonemap_begin(reshade::api::command_list* cmd_list)
{
	static const log::Once first(log::Level::info, "Tonemapper: first tonemap begin, on command list {}",
		static_cast<const void*>(cmd_list));
}

DrawAction Tonemapper::on_tonemap_draw(reshade::api::command_list* cmd_list, bool claimed)
{
	const bool replace = m_settings->get().tonemapper.replace_game_tonemap;
	static const log::Once first(log::Level::info, "Tonemapper: first tonemap draw, claimed {}, {}", claimed,
		replace ? "replacing it" : "keeping it");

	if (m_replacing.exchange(replace) != replace)
		log::info("Tonemapper: now {} the game's tonemap", replace ? "replacing" : "keeping");

	if (!replace || claimed)
		return DrawAction::keep;

	draw_replacement(*get_d3d11_context(cmd_list));
	return DrawAction::skip;
}

void Tonemapper::on_tonemap_end(reshade::api::command_list* cmd_list)
{
	static const log::Once first(log::Level::info, "Tonemapper: first tonemap end, on command list {}",
		static_cast<const void*>(cmd_list));
}
} // namespace ds3ea
