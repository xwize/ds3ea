#include "render_pass_state_machine.hpp"

#include <span>

namespace
{
using namespace ds3ea;

/**
 * @param pass The pass beginning.
 * @param cmd_list Its command list.
 * @param listeners Who to tell.
 */
void begin_pass(RenderPass pass, reshade::api::command_list* cmd_list, std::span<IRenderPassListener* const> listeners)
{
	for (IRenderPassListener* const listener : listeners)
	{
		if (pass == RenderPass::gbuffer)
			listener->on_gbuffer_begin(cmd_list);
		else if (pass == RenderPass::tonemap)
			listener->on_tonemap_begin(cmd_list);
	}
}

/**
 * @param pass The pass of the draw.
 * @param cmd_list Its command list.
 * @param listeners Who to ask, in priority order.
 * @returns skip if a listener replaced the draw.
 */
[[nodiscard]] DrawAction draw(RenderPass pass, reshade::api::command_list* cmd_list, std::span<IRenderPassListener* const> listeners)
{
	DrawAction action = DrawAction::keep;

	if (pass != RenderPass::tonemap)
		return action;

	for (IRenderPassListener* const listener : listeners)
	{
		const bool claimed = action == DrawAction::skip;
		if (listener->on_tonemap_draw(cmd_list, claimed) == DrawAction::skip)
			action = DrawAction::skip;
	}

	return action;
}

/**
 * @param pass The pass ending.
 * @param cmd_list Its command list.
 * @param listeners Who to tell.
 */
void end_pass(RenderPass pass, reshade::api::command_list* cmd_list, std::span<IRenderPassListener* const> listeners)
{
	for (IRenderPassListener* const listener : listeners)
	{
		if (pass == RenderPass::gbuffer)
			listener->on_gbuffer_end(cmd_list);
		else if (pass == RenderPass::tonemap)
			listener->on_tonemap_end(cmd_list);
	}
}
} // namespace

namespace ds3ea
{
DrawAction RenderPassStateMachine::on_draw(RenderPass pass, reshade::api::command_list* cmd_list,
	std::span<IRenderPassListener* const> listeners)
{
	if (pass == RenderPass::unknown)
		return DrawAction::keep;

	if (pass != m_open)
	{
		end(cmd_list, listeners);
		m_open = pass;
		begin_pass(pass, cmd_list, listeners);
	}

	return draw(pass, cmd_list, listeners);
}

void RenderPassStateMachine::end(reshade::api::command_list* cmd_list, std::span<IRenderPassListener* const> listeners)
{
	end_pass(m_open, cmd_list, listeners);
	m_open = RenderPass::unknown;
}

RenderPass RenderPassStateMachine::get_open_pass() const
{
	return m_open;
}
} // namespace ds3ea
