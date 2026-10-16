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
 * @param listener Who to ask.
 * @param pass The pass of the draw; tonemap or color_grading, the passes with a draw hook.
 * @param cmd_list Its command list.
 * @param draw Its arguments.
 * @param claimed True if an earlier listener already replaces the draw.
 * @returns The listener's answer.
 */
[[nodiscard]] DrawAction ask(IRenderPassListener& listener, RenderPass pass, reshade::api::command_list* cmd_list,
	const DrawCall& draw, bool claimed)
{
	if (pass == RenderPass::tonemap)
		return listener.on_tonemap_draw(cmd_list, draw, claimed);

	return listener.on_color_grading_draw(cmd_list, draw, claimed);
}

/**
 * @param pass The pass of the draw.
 * @param cmd_list Its command list.
 * @param draw Its arguments.
 * @param listeners Who to ask, in priority order.
 * @returns skip if a listener replaced the draw.
 */
[[nodiscard]] DrawAction draw_pass(RenderPass pass, reshade::api::command_list* cmd_list, const DrawCall& draw,
	std::span<IRenderPassListener* const> listeners)
{
	DrawAction action = DrawAction::keep;

	if (pass != RenderPass::tonemap && pass != RenderPass::color_grading)
		return action;

	for (IRenderPassListener* const listener : listeners)
	{
		const bool claimed = action == DrawAction::skip;

		if (ask(*listener, pass, cmd_list, draw, claimed) == DrawAction::skip)
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
DrawAction RenderPassStateMachine::on_draw(RenderPass pass, reshade::api::command_list* cmd_list, const DrawCall& draw,
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

	return draw_pass(pass, cmd_list, draw, listeners);
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
