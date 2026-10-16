#include "render_pass_tracker.hpp"
#include "render_pass.hpp"
#include "render_pass_state_machine.hpp"
#include "utils/logging.hpp"
#include "utils/reshade_with_imgui.hpp"
#include "utils/shader_reflection.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>

namespace ds3ea
{
/** Keyed in ReShade's private data by this type's uuid. */
struct __declspec(uuid("6b3f4c1e-2d5a-4e8b-9f17-0c2d4a6e8b31")) RenderPassTracker::CommandListPasses
{
	// Render pass of the pixel shader bound right now
	RenderPass pixel_pass = RenderPass::unknown;
	// Begin/end state of the run being recorded
	RenderPassStateMachine state_machine;
};

RenderPassTracker::RenderPassTracker() = default;

RenderPassTracker::~RenderPassTracker() = default;

void RenderPassTracker::attach(IRenderPassListener* listener)
{
	m_attached.push_back(listener);
}

void RenderPassTracker::on_pipeline_created(std::uint32_t subobject_count,
	const reshade::api::pipeline_subobject* subobjects, std::uint64_t pipeline)
{
	for (const reshade::api::pipeline_subobject& subobject : std::span(subobjects, subobject_count))
	{
		if (subobject.type != reshade::api::pipeline_subobject_type::pixel_shader || subobject.data == nullptr)
			continue;

		const auto* const desc = static_cast<const reshade::api::shader_desc*>(subobject.data);
		const Result<ShaderReflectionInfo> info = reflect_pixel_shader(desc->code, desc->code_size);

		if (!info)
		{
			static const log::Once failed(log::Level::warning, "Can't read a pixel shader: {}", info.error());
			continue;
		}

		const RenderPass pass = infer_render_pass_for_shader(*info);

		if (pass == RenderPass::unknown)
			continue;

		const std::unique_lock lock(m_shader_render_passes_mutex);
		m_shader_render_passes.insert_or_assign(pipeline, pass);
	}
}

void RenderPassTracker::on_pipeline_destroyed(std::uint64_t pipeline)
{
	const std::unique_lock lock(m_shader_render_passes_mutex);
	m_shader_render_passes.erase(pipeline);
}

void RenderPassTracker::on_destroy_command_list(reshade::api::command_list* cmd_list)
{
	const std::lock_guard lock(m_command_list_passes_mutex);
	m_command_list_passes.erase(cmd_list);
}

void RenderPassTracker::on_bind_pipeline(reshade::api::command_list* cmd_list, reshade::api::pipeline_stage stages,
	std::uint64_t pipeline)
{
	if ((stages & reshade::api::pipeline_stage::pixel_shader) == reshade::api::pipeline_stage::pixel_shader)
		get_passes(cmd_list).pixel_pass = find_shader_render_pass(pipeline);
}

bool RenderPassTracker::on_draw(reshade::api::command_list* cmd_list)
{
	CommandListPasses& passes = get_passes(cmd_list);
	return passes.state_machine.on_draw(passes.pixel_pass, cmd_list, m_attached) == DrawAction::skip;
}

void RenderPassTracker::on_close_command_list(reshade::api::command_list* cmd_list)
{
	get_passes(cmd_list).state_machine.end(cmd_list, m_attached);
}

void RenderPassTracker::on_present(reshade::api::command_queue* queue)
{
	reshade::api::command_list* const immediate = queue->get_immediate_command_list();

	if (immediate != nullptr)
		get_passes(immediate).state_machine.end(immediate, m_attached);

	for (IRenderPassListener* const listener : m_attached)
		listener->on_game_present();
}

RenderPass RenderPassTracker::get_open_render_pass(reshade::api::command_list* cmd_list) const
{
	const CommandListPasses* const passes = cmd_list->get_private_data<CommandListPasses>();
	return passes != nullptr ? passes->state_machine.get_open_pass() : RenderPass::unknown;
}

RenderPassTracker::CommandListPasses& RenderPassTracker::get_passes(reshade::api::command_list* cmd_list)
{
	CommandListPasses* const passes = cmd_list->get_private_data<CommandListPasses>();

	if (passes != nullptr)
		return *passes;

	const std::lock_guard lock(m_command_list_passes_mutex);
	std::unique_ptr<CommandListPasses>& owned = m_command_list_passes[cmd_list];
	owned = std::make_unique<CommandListPasses>();
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): ReShade's private data API takes the uuid as bytes
	const auto* const key = reinterpret_cast<const std::uint8_t*>(&__uuidof(CommandListPasses));
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): and the data as a 64-bit value
	cmd_list->set_private_data(key, reinterpret_cast<std::uintptr_t>(owned.get()));
	return *owned;
}

RenderPass RenderPassTracker::find_shader_render_pass(std::uint64_t shader) const
{
	const std::shared_lock lock(m_shader_render_passes_mutex);
	const auto entry = m_shader_render_passes.find(shader);
	return entry != m_shader_render_passes.end() ? entry->second : RenderPass::unknown;
}
} // namespace ds3ea
