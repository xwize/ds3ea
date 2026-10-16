#pragma once

#include "listeners/render_pass_listener.hpp"
#include "render_pass.hpp"

#include <span>

namespace ds3ea
{
/**
 * Tracks which render pass is open on one command list. Each draw arrives with the render pass of its pixel shader,
 * and the state machine calls the listeners' begin, draw and end hooks as passes open and close. Pure apart from
 * calling the listeners, so it's tested with fake draws and a mock listener.
 *
 * - A recognised pass begins at its first draw.
 * - It ends when a draw of another recognised pass arrives, or end() is called (command list closed, present).
 * - Draws of no recognised pass neither begin nor end anything: other draws are interleaved with every pass.
 * - Every draw of a pass with a draw hook asks the listeners whether the game's draw runs.
 */
class RenderPassStateMachine
{
public:
	/**
	 * @param pass The render pass of the draw's pixel shader.
	 * @param cmd_list The command list recording the draw, passed on to the listeners.
	 * @param listeners Who to tell, in priority order.
	 * @returns Whether the game's draw runs.
	 */
	[[nodiscard]] DrawAction on_draw(RenderPass pass, reshade::api::command_list* cmd_list,
		std::span<IRenderPassListener* const> listeners);

	/**
	 * Ends the open pass, if any.
	 * @param cmd_list The command list it was recorded on.
	 * @param listeners Who to tell.
	 */
	void end(reshade::api::command_list* cmd_list, std::span<IRenderPassListener* const> listeners);

	/** @returns The render pass that's open, or RenderPass::unknown. */
	[[nodiscard]] RenderPass get_open_pass() const;

private:
	RenderPass m_open = RenderPass::unknown;
};
} // namespace ds3ea
