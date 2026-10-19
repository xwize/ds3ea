#pragma once

#include "utils/reshade_forward.hpp"

namespace ds3ea
{
/**
 * Told when the game creates, resizes and destroys its swap chain, and when it switches between full screen and
 * windowed, so a feature can change how the game's picture reaches the screen.
 */
class ISwapchainListener
{
public:
	ISwapchainListener() = default;
	virtual ~ISwapchainListener() = default;

	ISwapchainListener(const ISwapchainListener&) = delete;
	ISwapchainListener& operator=(const ISwapchainListener&) = delete;
	ISwapchainListener(ISwapchainListener&&) = delete;
	ISwapchainListener& operator=(ISwapchainListener&&) = delete;

	/**
	 * The game is about to create its swap chain, or resize its buffers.
	 * @param[in,out] desc What the game asked for; a listener may change it.
	 * @returns True if the listener changed desc.
	 */
	virtual bool on_create_swapchain(reshade::api::swapchain_desc& /*desc*/) { return false; }

	/**
	 * The game created its swap chain, or resized its buffers.
	 * @param swapchain The swap chain.
	 * @param resize True if the buffers were resized rather than the swap chain created.
	 */
	virtual void on_swapchain_created(reshade::api::swapchain* /*swapchain*/, bool /*resize*/) {}

	/**
	 * The game is about to destroy its swap chain, or resize its buffers.
	 * @param swapchain The swap chain.
	 * @param resize True if only the buffers are about to be resized.
	 */
	virtual void on_swapchain_destroyed(reshade::api::swapchain* /*swapchain*/, bool /*resize*/) {}

	/**
	 * The game is switching into or out of exclusive full screen.
	 * @param swapchain The swap chain.
	 * @param fullscreen True when switching into full screen.
	 * @returns True to stop the switch; the game is still told it succeeded.
	 */
	virtual bool on_set_fullscreen_state(reshade::api::swapchain* /*swapchain*/, bool /*fullscreen*/) { return false; }
};
} // namespace ds3ea
