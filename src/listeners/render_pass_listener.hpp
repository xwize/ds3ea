#pragma once

#include "utils/reshade_forward.hpp"

namespace ds3ea
{
/** What should happen to the game's own draw. */
enum class DrawAction
{
	keep, // The game's draw runs
	skip, // A listener replaced the draw (or hides it), so the game's doesn't run
};

/**
 * Told when the game's render passes begin and end, per command list. A pass begins at its first draw and ends when
 * a draw of another recognised pass is recorded on the same command list, or the list is closed, or at present.
 * Called from whichever thread records the command list, so implementations must be thread-safe. Every hook does
 * nothing by default, so a listener overrides only the passes it cares about.
 */
class IRenderPassListener
{
public:
	IRenderPassListener() = default;
	virtual ~IRenderPassListener() = default;

	IRenderPassListener(const IRenderPassListener&) = delete;
	IRenderPassListener& operator=(const IRenderPassListener&) = delete;
	IRenderPassListener(IRenderPassListener&&) = delete;
	IRenderPassListener& operator=(IRenderPassListener&&) = delete;

	/**
	 * The first G-buffer draw of a run is about to be recorded.
	 * @param cmd_list The command list recording it.
	 */
	virtual void on_gbuffer_begin(reshade::api::command_list* /*cmd_list*/) {}

	/**
	 * The G-buffer run on a command list has ended. Other command lists may still be in theirs.
	 * @param cmd_list The command list it was recorded on.
	 */
	virtual void on_gbuffer_end(reshade::api::command_list* /*cmd_list*/) {}

	/**
	 * The tonemap pass begins on a command list; on_tonemap_draw follows for each of its draws.
	 * @param cmd_list The command list recording it.
	 */
	virtual void on_tonemap_begin(reshade::api::command_list* /*cmd_list*/) {}

	/**
	 * A tonemap draw is about to be recorded. Listeners are asked in attach order; the first to return skip replaces
	 * the draw, and later listeners are told it's claimed.
	 * @param cmd_list The command list recording it, with the game's shaders and resources still bound.
	 * @param claimed True if an earlier listener already replaces this draw.
	 * @returns skip to replace the game's draw, keep to let it run.
	 */
	[[nodiscard]] virtual DrawAction on_tonemap_draw(reshade::api::command_list* /*cmd_list*/, bool /*claimed*/)
	{
		return DrawAction::keep;
	}

	/**
	 * The tonemap pass on that command list has ended.
	 * @param cmd_list The command list it was recorded on.
	 */
	virtual void on_tonemap_end(reshade::api::command_list* /*cmd_list*/) {}

	/** The game presented a frame; every pass of the frame has ended. */
	virtual void on_game_present() {}
};
} // namespace ds3ea
