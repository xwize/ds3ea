#pragma once

#include "listeners/device_listener.hpp"
#include "listeners/pipeline_listener.hpp"
#include "listeners/render_pass_listener.hpp"
#include "settings.hpp"
#include "tonemap_renderer.hpp"
#include "utils/reshade_forward.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

namespace ds3ea
{
/**
 * What a game tonemap shader applies besides its curve. The game has a variant with the radial vignette and the colour
 * matrix, and one with neither; our shader stands in for both, told which by its constants.
 */
struct TonemapVariant
{
	// Reads the centred vignette coordinate (TEXCOORD2) and darkens towards the edges
	bool has_vignette = false;
	// Applies m44_ColorTransformMatrix
	bool has_color_matrix = false;
};

/** The range a tonemap setting is kept in: the overlay's slider, and a clamp on values read from the ini. */
struct SettingRange
{
	float min;
	float max;
};

inline constexpr SettingRange k_exposure_range = { .min = 0.25f, .max = 4.0f };
inline constexpr SettingRange k_white_range = { .min = 2.0f, .max = 30.0f };
inline constexpr SettingRange k_toe_range = { .min = 0.05f, .max = 1.0f };
inline constexpr SettingRange k_shoulder_range = { .min = 0.05f, .max = 0.5f };
inline constexpr SettingRange k_bloom_range = { .min = 0.0f, .max = 1.0f };

/**
 * The tonemapping feature: with the replace_game_tonemap setting, our tonemap (Hable's filmic curve, the game's
 * colour grading at full precision, a triangular dither) replaces the game's, drawn in place by TonemapRenderer.
 * Without it, or if our tonemap can't run, the game's runs unchanged. Settings are read at every draw, so changes
 * show on the next frame.
 *
 * The game's grading draw comes after its tonemap draw in a frame. The grading is applied in our tonemap draw, from
 * the copy taken at the previous frame's grading draw, so it works on our full-precision colour instead of this
 * pass's 8-bit output; the grading draw then becomes a pass-through. Until a first copy exists, the game's grading
 * draw runs as usual.
 *
 * Draw hooks run on the game's recording threads, device and pipeline hooks on others; the renderer is used under a
 * lock, and the variants map has its own.
 */
class Tonemapper : public IRenderPassListener, public IDeviceListener, public IPipelineListener
{
public:
	/** @param settings The add-on's settings; not owned, and must outlive the Tonemapper. */
	explicit Tonemapper(const Settings& settings);

	void on_device_created(reshade::api::device* device) override;
	void on_device_destroyed(reshade::api::device* device) override;
	void on_pipeline_created(std::uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects,
		std::uint64_t pipeline) override;
	void on_pipeline_destroyed(std::uint64_t pipeline) override;
	void on_tonemap_begin(reshade::api::command_list* cmd_list) override;
	[[nodiscard]] DrawAction on_tonemap_draw(reshade::api::command_list* cmd_list, const DrawCallArgs& draw,
		bool claimed) override;
	void on_tonemap_end(reshade::api::command_list* cmd_list) override;
	[[nodiscard]] DrawAction on_color_grading_draw(reshade::api::command_list* cmd_list, const DrawCallArgs& draw,
		bool claimed) override;

private:
	/**
	 * @param context The context recording a tonemap draw.
	 * @returns The variant of the game's tonemap shader bound on it, or nothing if it isn't one we've seen created.
	 */
	[[nodiscard]] std::optional<TonemapVariant> find_bound_variant(ID3D11DeviceContext& context) const;

	const Settings* m_settings;
	// The replace_game_tonemap value of the last tonemap draw, to log when it changes; tonemap draws only
	std::atomic<bool> m_is_replacing;
	// Set by a tonemap draw that applied the game's grading, consumed by the grading draw that follows it
	std::atomic<bool> m_is_graded_this_frame = false;
	// The variant of each game tonemap shader, by handle: written when the game creates or destroys shaders (any
	// thread), read at every tonemap draw
	std::unordered_map<std::uint64_t, TonemapVariant> m_variants;
	mutable std::shared_mutex m_variants_mutex;
	// The game's device and our renderer, from device creation until it's destroyed or the add-on unloads (DS3 never
	// destroys its device); guarded by m_renderer_mutex
	Microsoft::WRL::ComPtr<ID3D11Device> m_device;
	std::optional<TonemapRenderer> m_renderer;
	std::mutex m_renderer_mutex;
};
} // namespace ds3ea
