#pragma once

#include "listeners/device_listener.hpp"
#include "listeners/pipeline_listener.hpp"
#include "render_pass_tracker.hpp"
#include "tonemapper.hpp"
#include "utils/ini_settings_store.hpp"

#include <Windows.h>
// Not utils/reshade_forward.hpp: the handlers take ReShade's handle types (pipeline, resource) by value, which are
// typedefs of unnamed structs and can't be forward-declared
#include <reshade.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace ds3ea
{
/**
 * The add-on's registration with ReShade and the owner of its services and features. create() registers the add-on;
 * the constructor builds and attaches the features, then registers the ReShade event handlers, which forward to the
 * one instance. DllMain owns that instance for the life of the module.
 */
class Addon
{
	/** Passkey that limits construction to create(), while still letting it use std::make_unique. */
	struct Key;

public:
	/**
	 * Loads ds3ea.ini from the game's working directory, builds and attaches the features, then registers the event
	 * handlers and logs the start-up. Only create() can call it, once ReShade has accepted the add-on.
	 * @param hmod Handle of this add-on's DLL.
	 * @param key Passkey only create() can make.
	 */
	Addon(HMODULE hmod, Key key);

	/** Unregisters the add-on, which removes every event handler it registered. */
	~Addon();

	/** Not copyable or movable: the instance owns the ReShade registration and the handlers point at it. */
	Addon(const Addon&) = delete;
	Addon& operator=(const Addon&) = delete;
	Addon(Addon&&) = delete;
	Addon& operator=(Addon&&) = delete;

	/**
	 * Registers the add-on with ReShade and builds the instance.
	 * @param hmod Handle of this add-on's DLL.
	 * @returns The add-on, or nullptr if ReShade is missing, rejected the add-on or the add-on failed to start.
	 */
	[[nodiscard]] static std::unique_ptr<Addon> create(HMODULE hmod);

private:
	/**
	 * ReShade created a device; tells the device listeners.
	 * @param device The device ReShade just initialised.
	 */
	static void on_init_device(reshade::api::device* device);

	/**
	 * ReShade is destroying a device; tells the device listeners.
	 * @param device The device about to be destroyed.
	 */
	static void on_destroy_device(reshade::api::device* device);

	/**
	 * The game created a pipeline (in D3D11, one shader); tells the pipeline listeners.
	 * @param device The device.
	 * @param layout Unused in D3D11.
	 * @param subobject_count How many subobjects the pipeline has.
	 * @param subobjects Its subobjects; in D3D11 one shader.
	 * @param pipeline The new pipeline's handle.
	 */
	static void on_init_pipeline(reshade::api::device* device, reshade::api::pipeline_layout layout,
		std::uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects,
		reshade::api::pipeline pipeline);

	/**
	 * The game is destroying a pipeline; tells the pipeline listeners.
	 * @param device The device.
	 * @param pipeline The pipeline being destroyed.
	 */
	static void on_destroy_pipeline(reshade::api::device* device, reshade::api::pipeline pipeline);

	/**
	 * The game is destroying a command list; the tracker drops its state.
	 * @param cmd_list The command list being destroyed.
	 */
	static void on_destroy_command_list(reshade::api::command_list* cmd_list);

	/**
	 * The game bound a pipeline; the tracker notes a pixel shader's render pass.
	 * @param cmd_list The command list it's bound on.
	 * @param stages The stages bound.
	 * @param pipeline The pipeline bound.
	 */
	static void on_bind_pipeline(reshade::api::command_list* cmd_list, reshade::api::pipeline_stage stages,
		reshade::api::pipeline pipeline);

	/**
	 * A draw; the tracker runs the command list's render pass state machine.
	 * @param cmd_list The command list recording the draw.
	 * @param vertex_count Unused.
	 * @param instance_count Unused.
	 * @param first_vertex Unused.
	 * @param first_instance Unused.
	 * @returns True to skip the game's draw.
	 */
	static bool on_draw(reshade::api::command_list* cmd_list, std::uint32_t vertex_count,
		std::uint32_t instance_count, std::uint32_t first_vertex, std::uint32_t first_instance);

	/**
	 * An indexed draw; the tracker runs the command list's render pass state machine.
	 * @param cmd_list The command list recording the draw.
	 * @param index_count Unused.
	 * @param instance_count Unused.
	 * @param first_index Unused.
	 * @param vertex_offset Unused.
	 * @param first_instance Unused.
	 * @returns True to skip the game's draw.
	 */
	static bool on_draw_indexed(reshade::api::command_list* cmd_list, std::uint32_t index_count,
		std::uint32_t instance_count, std::uint32_t first_index, std::int32_t vertex_offset,
		std::uint32_t first_instance);

	/**
	 * An indirect draw or dispatch; for a draw, the tracker runs the command list's render pass state machine.
	 * @param cmd_list The command list recording the draw or dispatch.
	 * @param type Draw or dispatch.
	 * @param buffer Unused.
	 * @param offset Unused.
	 * @param draw_count Unused.
	 * @param stride Unused.
	 * @returns True to skip the game's draw.
	 */
	static bool on_draw_or_dispatch_indirect(reshade::api::command_list* cmd_list, reshade::api::indirect_command type,
		reshade::api::resource buffer, std::uint64_t offset, std::uint32_t draw_count, std::uint32_t stride);

	/**
	 * The game finished recording a deferred command list; the tracker ends its open pass.
	 * @param cmd_list The command list.
	 */
	static void on_close_command_list(reshade::api::command_list* cmd_list);

	/**
	 * The game presented a frame; the tracker ends the immediate context's pass and tells the listeners.
	 * @param queue The queue presenting.
	 * @param swapchain Unused.
	 * @param source_rect Unused.
	 * @param dest_rect Unused.
	 * @param dirty_rect_count Unused.
	 * @param dirty_rects Unused.
	 */
	static void on_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain,
		const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, std::uint32_t dirty_rect_count,
		const reshade::api::rect* dirty_rects);

	HMODULE m_module;
	IniSettingsStore m_settings;
	// The features, declared before the tracker and the lists that point at them so they're destroyed after
	Tonemapper m_tonemapper;
	RenderPassTracker m_tracker;
	std::vector<IDeviceListener*> m_device_listeners;
	std::vector<IPipelineListener*> m_pipeline_listeners;
};
} // namespace ds3ea
