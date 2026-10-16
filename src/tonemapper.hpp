#pragma once

#include "listeners/device_listener.hpp"
#include "listeners/render_pass_listener.hpp"
#include "reshade_forward.hpp"

#include <d3d11.h>
#include <wrl/client.h>

namespace ds3ea
{
/**
 * The tonemapping feature, for now only its plumbing. With replace_game_tonemap it claims the game's tonemap draw and
 * clears the render target to green instead: a stand-in showing that our own GPU work lands where the game's tonemap
 * ran. Without it the game's tonemap runs unchanged.
 */
class Tonemapper : public IRenderPassListener, public IDeviceListener
{
public:
	/** @param replace_game_tonemap Claim the game's tonemap draw ([Tonemapper] ReplaceGameTonemap). */
	explicit Tonemapper(bool replace_game_tonemap);

	void on_device_created(reshade::api::device* device) override;
	void on_device_destroyed(reshade::api::device* device) override;
	void on_tonemap_begin(reshade::api::command_list* cmd_list) override;
	[[nodiscard]] DrawAction on_tonemap_draw(reshade::api::command_list* cmd_list, bool claimed) override;
	void on_tonemap_end(reshade::api::command_list* cmd_list) override;

private:
	bool m_replace_game_tonemap;
	// The game's device, for creating our GPU resources. Also released when the add-on unloads, since DS3 exits
	// without destroying its device.
	Microsoft::WRL::ComPtr<ID3D11Device> m_device;
};
} // namespace ds3ea
