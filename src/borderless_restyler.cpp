#include "borderless_restyler.hpp"
#include "utils/build_config.hpp"
#include "utils/logging.hpp"

#include <fmt/format.h>

#include <Windows.h>
#include <CommCtrl.h>
#include <reshade.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace
{
using namespace ds3ea;

// Any value: it only tells our subclass apart from others on the same window
constexpr UINT_PTR k_subclass_id = 0xD53EA;

// A borderless top-level window; no caption, frame or system menu
constexpr LONG_PTR k_window_style = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

// Style bits the window's state owns (shown, minimised, maximised, disabled), kept from whatever is being set
constexpr LONG_PTR k_state_style_bits = WS_VISIBLE | WS_MINIMIZE | WS_MAXIMIZE | WS_DISABLED;

// Keeps the taskbar button; drops the edges and the topmost flag a full screen switch may have set
constexpr LONG_PTR k_window_ex_style = WS_EX_APPWINDOW;

// Where Windows puts a window's position while it's minimised
constexpr int k_minimized_position = -32000;

/**
 * @param window A window.
 * @returns The bounds of the monitor the window is mostly on, in screen coordinates.
 */
RECT get_monitor_rect(HWND window)
{
	MONITORINFO info = {};
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info);
	return info.rcMonitor;
}

/**
 * @param window A window.
 * @returns Its client area's size, e.g. "3840x2160".
 */
std::string describe_client_size(HWND window)
{
	RECT client = {};
	GetClientRect(window, &client);
	return fmt::format("{}x{}", client.right - client.left, client.bottom - client.top);
}

/**
 * Puts the window's style back to borderless when someone changes it.
 * @param window The game window.
 * @param which GWL_STYLE or GWL_EXSTYLE.
 * @param[in,out] change The style about to be set.
 * @returns True if the style was put back.
 */
bool keep_borderless_style(HWND window, WPARAM which, STYLESTRUCT& change)
{
	const LONG_PTR wanted = which == static_cast<WPARAM>(GWL_STYLE) ? make_borderless_style(change.styleNew)
																	: k_window_ex_style;

	if (static_cast<LONG_PTR>(change.styleNew) == wanted)
		return false;

	log::info("BorderlessRestyler: put back {} {:#x} -> {:#x} on window {}",
		which == static_cast<WPARAM>(GWL_STYLE) ? "style" : "extended style", change.styleNew, wanted,
		static_cast<const void*>(window));
	change.styleNew = static_cast<DWORD>(wanted);
	return true;
}

/**
 * The message take_over_window() posts to the window, so the restyle runs from the game's message loop rather than
 * inside CreateSwapChain: resizing the window there sends WM_SIZE to the game before it has its swap chain, and the
 * game crashes (0xc000041d).
 * @returns The message, unique to us.
 */
UINT get_restyle_message()
{
	static const UINT message = RegisterWindowMessageW(L"DS3EA.BorderlessRestyler.Restyle");
	return message;
}

/**
 * Takes the window's border off and puts it over its monitor.
 * @param window The game window.
 */
void restyle_window(HWND window)
{
	const LONG_PTR old_style = GetWindowLongPtrW(window, GWL_STYLE);
	const LONG_PTR old_ex_style = GetWindowLongPtrW(window, GWL_EXSTYLE);
	SetWindowLongPtrW(window, GWL_STYLE, make_borderless_style(old_style));
	SetWindowLongPtrW(window, GWL_EXSTYLE, k_window_ex_style);
	const RECT monitor = get_monitor_rect(window);
	SetWindowPos(window, HWND_TOP, monitor.left, monitor.top, monitor.right - monitor.left,
		monitor.bottom - monitor.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	log::info("BorderlessRestyler: window {} made borderless over its monitor ({}, {}) {}x{}, style {:#x} -> {:#x}, "
		"extended {:#x} -> {:#x}, client area {}", static_cast<const void*>(window), monitor.left, monitor.top,
		monitor.right - monitor.left, monitor.bottom - monitor.top, old_style, GetWindowLongPtrW(window, GWL_STYLE),
		old_ex_style, GetWindowLongPtrW(window, GWL_EXSTYLE), describe_client_size(window));
}
} // namespace

namespace ds3ea
{
LONG_PTR make_borderless_style(LONG_PTR style)
{
	return (style & k_state_style_bits) | k_window_style;
}

std::optional<SIZE> keep_covering_monitor(WINDOWPOS& pos, const RECT& monitor)
{
	const bool is_move_or_size = (pos.flags & SWP_NOMOVE) == 0 || (pos.flags & SWP_NOSIZE) == 0;

	if (!is_move_or_size || pos.x == k_minimized_position)
		return std::nullopt;

	const int width = monitor.right - monitor.left;
	const int height = monitor.bottom - monitor.top;
	const bool is_resize = (pos.flags & SWP_NOSIZE) == 0;
	const SIZE requested = { .cx = pos.cx, .cy = pos.cy };

	if (pos.x == monitor.left && pos.y == monitor.top && (!is_resize || (pos.cx == width && pos.cy == height)))
		return is_resize ? std::optional(requested) : std::nullopt;

	log::info("BorderlessRestyler: put back a move to ({}, {}) {}x{} (flags {:#x}) with ({}, {}) {}x{}", pos.x, pos.y,
		pos.cx, pos.cy, pos.flags, monitor.left, monitor.top, width, height);
	pos.x = monitor.left;
	pos.y = monitor.top;
	pos.cx = width;
	pos.cy = height;
	pos.flags &= ~static_cast<UINT>(SWP_NOMOVE | SWP_NOSIZE);
	return is_resize ? std::optional(requested) : std::nullopt;
}

BorderlessRestyler::BorderlessRestyler(bool is_enabled)
	: m_is_enabled(k_borderless_restyle_in_build && is_enabled)
{
	if (m_is_enabled)
		log::info("BorderlessRestyler: on");
	else if (!k_borderless_restyle_in_build)
		log::info("BorderlessRestyler: off, turned off in this build");
	else
		log::info("BorderlessRestyler: off ([Display] Borderless is false), the game decides");
}

BorderlessRestyler::~BorderlessRestyler()
{
	release_window();
}

bool BorderlessRestyler::on_create_swapchain(reshade::api::swapchain_desc& desc)
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access): ReShade's resource description is a union
	const auto& back_buffer = desc.back_buffer.texture;
	log::debug("BorderlessRestyler: swap chain {}x{}, swap effect {}, flags {:#x}, full screen {}", back_buffer.width,
		back_buffer.height, desc.present_mode, desc.present_flags, desc.fullscreen_state);

	if (!m_is_enabled)
		return false;

	bool changed = false;

	if (desc.fullscreen_state)
	{
		log::info("BorderlessRestyler: creating the swap chain windowed instead of full screen");
		desc.fullscreen_state = false;
		changed = true;
	}

	// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access): ReShade's resource description is a union
	if (m_game_size.cx == 0)
	{
		m_game_size = { .cx = static_cast<LONG>(back_buffer.width), .cy = static_cast<LONG>(back_buffer.height) };
	}
	else if (static_cast<LONG>(back_buffer.width) != m_game_size.cx ||
		static_cast<LONG>(back_buffer.height) != m_game_size.cy)
	{
		log::info("BorderlessRestyler: back buffer {}x{} kept at the game's {}x{}", back_buffer.width,
			back_buffer.height, m_game_size.cx, m_game_size.cy);
		desc.back_buffer.texture.width = static_cast<std::uint32_t>(m_game_size.cx);
		desc.back_buffer.texture.height = static_cast<std::uint32_t>(m_game_size.cy);
		changed = true;
	}
	// NOLINTEND(cppcoreguidelines-pro-type-union-access)

	return changed;
}

void BorderlessRestyler::apply_game_resize(HWND window, std::optional<SIZE> requested)
{
	if (!requested || (requested->cx == m_game_size.cx && requested->cy == m_game_size.cy))
		return;

	m_game_size = *requested;
	log::info("BorderlessRestyler: the game's resolution is now {}x{}", requested->cx, requested->cy);
	tell_game_resized(window);
}

void BorderlessRestyler::tell_game_resized(HWND window) const
{
	// The game waits for WM_SIZE to resize its buffers, which the window doesn't get when its size doesn't change
	PostMessageW(window, WM_SIZE, SIZE_RESTORED, MAKELPARAM(m_game_size.cx, m_game_size.cy));
	log::info("BorderlessRestyler: told the game its window is {}x{}", m_game_size.cx, m_game_size.cy);
}

void BorderlessRestyler::on_swapchain_created(reshade::api::swapchain* swapchain, bool resize)
{
	const auto window = static_cast<HWND>(swapchain->get_hwnd());
	const reshade::api::resource_desc back_buffer =
		swapchain->get_device()->get_resource_desc(swapchain->get_current_back_buffer());
	// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access): ReShade's resource description is a union
	log::debug("BorderlessRestyler: swap chain {}, back buffer {}x{}, client area {}", resize ? "resized" : "created",
		back_buffer.texture.width, back_buffer.texture.height, describe_client_size(window));
	// NOLINTEND(cppcoreguidelines-pro-type-union-access)

	if (m_is_enabled && m_window == nullptr)
		take_over_window(window);
}

void BorderlessRestyler::on_swapchain_destroyed(reshade::api::swapchain* swapchain, bool resize)
{
	if (!resize && static_cast<HWND>(swapchain->get_hwnd()) == m_window)
		release_window();
}

bool BorderlessRestyler::on_set_fullscreen_state(reshade::api::swapchain* /*swapchain*/, bool fullscreen)
{
	if (!m_is_enabled || !fullscreen)
		return false;

	log::info("BorderlessRestyler: stopped the switch to exclusive full screen");
	return true;
}

LRESULT CALLBACK BorderlessRestyler::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
	UINT_PTR subclass_id, DWORD_PTR ref_data)
{
	// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr): Windows passes these as
	// integers
	auto* const self = reinterpret_cast<BorderlessRestyler*>(ref_data);

	if (message == get_restyle_message())
	{
		restyle_window(window);
		self->m_is_restyled = true;
		return 0;
	}

	switch (message)
	{
	case WM_STYLECHANGING:
		// A border would have shrunk the client area, which the game waits to be told about
		if (self->m_is_restyled && keep_borderless_style(window, wparam, *reinterpret_cast<STYLESTRUCT*>(lparam)))
			self->tell_game_resized(window);

		break;
	case WM_WINDOWPOSCHANGING:
		// Minimising is left alone
		if (self->m_is_restyled && IsIconic(window) == FALSE)
		{
			auto& pos = *reinterpret_cast<WINDOWPOS*>(lparam);
			self->apply_game_resize(window, keep_covering_monitor(pos, get_monitor_rect(window)));
		}

		break;
	case WM_NCDESTROY:
		RemoveWindowSubclass(window, &BorderlessRestyler::window_proc, subclass_id);
		self->m_window = nullptr;
		break;
	default:
		break;
	}
	// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)

	return DefSubclassProc(window, message, wparam, lparam);
}

void BorderlessRestyler::take_over_window(HWND window)
{
	const DWORD owner_thread = GetWindowThreadProcessId(window, nullptr);
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): Windows passes the instance as an integer
	const auto ref_data = reinterpret_cast<DWORD_PTR>(this);
	const bool is_subclassed =
		SetWindowSubclass(window, &BorderlessRestyler::window_proc, k_subclass_id, ref_data) != FALSE;
	const bool is_posted = PostMessageW(window, get_restyle_message(), 0, 0) != FALSE;
	m_window = window;
	log::info("BorderlessRestyler: window {} subclassed {}, restyle posted {} (window thread {}, this thread {})",
		static_cast<const void*>(window), is_subclassed, is_posted, owner_thread, GetCurrentThreadId());
}

void BorderlessRestyler::release_window()
{
	if (m_window == nullptr)
		return;

	const bool is_removed = RemoveWindowSubclass(m_window, &BorderlessRestyler::window_proc, k_subclass_id) != FALSE;
	log::info("BorderlessRestyler: subclass {}removed from window {}", is_removed ? "" : "NOT ",
		static_cast<const void*>(m_window));
	m_window = nullptr;
	m_is_restyled = false;
}
} // namespace ds3ea
