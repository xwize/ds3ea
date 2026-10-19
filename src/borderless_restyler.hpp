#pragma once

#include "listeners/swapchain_listener.hpp"
#include "utils/reshade_forward.hpp"

#include <Windows.h>

#include <optional>

namespace ds3ea
{
/**
 * @param style A window style being set on the game window.
 * @returns That style without its border (a popup window), keeping the bits its state owns: shown, minimised,
 *     maximised and disabled.
 */
[[nodiscard]] LONG_PTR make_borderless_style(LONG_PTR style);

/**
 * Keeps the game window over its monitor when someone moves or resizes it. A move to the minimised position is let
 * through.
 * @param[in,out] pos The position and size about to be set, from WM_WINDOWPOSCHANGING.
 * @param monitor The bounds of the window's monitor, in screen coordinates.
 * @returns The size that was asked for, if pos is a resize, even one to the monitor's size: the game resizes its
 *     window to its resolution.
 */
[[nodiscard]] std::optional<SIZE> keep_covering_monitor(WINDOWPOS& pos, const RECT& monitor);

/**
 * Borderless full screen: keeps the game's swap chain windowed and turns its window into a borderless one that covers
 * the monitor it's on, so it looks like full screen without the mode switch. The game asks for exclusive full screen
 * after creating a windowed swap chain; that switch is stopped, and the game believes it happened. The window is
 * subclassed, so when the game or DXGI later restores a border or moves the window, the change is put back as it's
 * made. Built in by k_borderless_restyle_in_build, then turned on by [Display] Borderless in ds3ea.ini.
 */
class BorderlessRestyler : public ISwapchainListener
{
public:
	/**
	 * @param is_enabled Whether ds3ea.ini turns borderless on; read once, as it acts when the swap chain is made.
	 *     Ignored when k_borderless_restyle_in_build is false.
	 */
	explicit BorderlessRestyler(bool is_enabled);

	/** Removes the window subclass, if it's still there. */
	~BorderlessRestyler() override;

	/** Not copyable or movable: the window subclass points at this instance. */
	BorderlessRestyler(const BorderlessRestyler&) = delete;
	BorderlessRestyler& operator=(const BorderlessRestyler&) = delete;
	BorderlessRestyler(BorderlessRestyler&&) = delete;
	BorderlessRestyler& operator=(BorderlessRestyler&&) = delete;

	bool on_create_swapchain(reshade::api::swapchain_desc& desc) override;
	void on_swapchain_created(reshade::api::swapchain* swapchain, bool resize) override;
	void on_swapchain_destroyed(reshade::api::swapchain* swapchain, bool resize) override;
	bool on_set_fullscreen_state(reshade::api::swapchain* swapchain, bool fullscreen) override;

private:
	/**
	 * The subclass's window procedure (SUBCLASSPROC): puts back style, size and position changes to the game window.
	 * Windows fixes its signature.
	 * @param window The game window.
	 * @param message The message.
	 * @param wparam The message's wParam.
	 * @param lparam The message's lParam.
	 * @param subclass_id Our subclass id.
	 * @param ref_data The BorderlessRestyler.
	 * @returns The message's result.
	 */
	static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
		UINT_PTR subclass_id, DWORD_PTR ref_data);

	/** Subclasses the window and posts it the restyle, the first time it's seen. */
	void take_over_window(HWND window);

	/**
	 * The game asked for a window size (its resolution), which the window keeps at the monitor's: if it's a new
	 * resolution, keeps the back buffer at that size from now on and tells the game its window resized.
	 * @param window The game window.
	 * @param requested The size asked for, or nothing if it wasn't a resize.
	 */
	void apply_game_resize(HWND window, std::optional<SIZE> requested);

	/**
	 * Posts the game the WM_SIZE it would have had for its resolution, so it resizes its buffers.
	 * @param window The game window.
	 */
	void tell_game_resized(HWND window) const;

	/** Removes the subclass. */
	void release_window();

	bool m_is_enabled;
	// The game window once taken over, else nullptr; only touched on the game window's thread
	HWND m_window = nullptr;
	// Whether the posted restyle has run; until then the subclass leaves the window alone
	bool m_is_restyled = false;
	// The game's resolution, which its back buffer is kept at whatever the window's size; zero until its swap chain
	// is created
	SIZE m_game_size = {};
};
} // namespace ds3ea
