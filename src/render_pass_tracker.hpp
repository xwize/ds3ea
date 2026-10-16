#pragma once

#include "listeners/pipeline_listener.hpp"
#include "listeners/render_pass_listener.hpp"
#include "render_pass.hpp"
#include "reshade_forward.hpp"

#include <cstdint>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace ds3ea
{
/**
 * Knows which render pass is open on each command list, and tells the attached listeners as passes begin, draw and
 * end. DS3 records several command lists at once, each with its own open pass, so the tracker runs one
 * RenderPassStateMachine per command list, kept with the list's bound pass as ReShade private data on the list; the
 * per-draw path takes no lock. Addon forwards ReShade's events to it.
 *
 * Attach every listener before the events are registered: the list is read without a lock and must not change.
 */
class RenderPassTracker : public IPipelineListener
{
public:
	/**
	 * Adds a listener; earlier listeners get first claim on a draw.
	 * @param listener Not owned: it must outlive the tracker's last event.
	 */
	void attach(IRenderPassListener* listener);

	/** Records the render pass of the pixel shader the game created, worked out from its code. */
	void on_pipeline_created(std::uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects,
		std::uint64_t pipeline) override;

	/** Forgets the render pass of a shader the game destroyed. */
	void on_pipeline_destroyed(std::uint64_t pipeline) override;

	/** @param cmd_list A command list the game is destroying; its state goes with it. */
	void on_destroy_command_list(reshade::api::command_list* cmd_list);

	/**
	 * Remembers the pass of the pixel shader the game bound (bind_pipeline).
	 * @param cmd_list The command list it's bound on.
	 * @param stages The stages bound.
	 * @param pipeline The shader's handle.
	 */
	void on_bind_pipeline(reshade::api::command_list* cmd_list, reshade::api::pipeline_stage stages,
		std::uint64_t pipeline);

	/**
	 * Runs the pass state machine for a draw (draw, draw_indexed, draw_or_dispatch_indirect).
	 * @param cmd_list The command list recording it.
	 * @returns True if a listener replaced the draw and the game's must be skipped.
	 */
	[[nodiscard]] bool on_draw(reshade::api::command_list* cmd_list);

	/** @param cmd_list A deferred command list the game finished recording (FinishCommandList); its pass ends. */
	void on_close_command_list(reshade::api::command_list* cmd_list);

	/** @param queue The queue presenting; the immediate context's open pass ends, then listeners hear present. */
	void on_present(reshade::api::command_queue* queue);

	/**
	 * @param cmd_list A command list, on the thread recording it.
	 * @returns The render pass open on it, or RenderPass::unknown.
	 */
	[[nodiscard]] RenderPass get_open_render_pass(reshade::api::command_list* cmd_list) const;

private:
	/**
	 * @param shader A pixel shader's handle.
	 * @returns The render pass recorded for it when the game created it, or RenderPass::unknown.
	 */
	[[nodiscard]] RenderPass find_shader_render_pass(std::uint64_t shader) const;

	// A shader's code is only available when the game creates it, so its render pass is worked out then and kept
	// here by handle for every later bind. Shaders of no recognised pass aren't kept. Written on shader creation and
	// destruction (any thread, often loader threads), read on every pixel shader bind, hence the shared_mutex.
	std::unordered_map<std::uint64_t, RenderPass> m_shader_render_passes;
	mutable std::shared_mutex m_shader_render_passes_mutex;
	std::vector<IRenderPassListener*> m_attached;
};
} // namespace ds3ea
