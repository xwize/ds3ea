#include "addon.hpp"
#include "settings.hpp"
#include "tonemapper.hpp"

#include <imgui.h>

namespace
{
using namespace ds3ea;

/**
 * The Tonemapper's widgets.
 * @param tonemapper Its settings, edited in place.
 * @returns True if any of them changed.
 */
bool draw_tonemapper_settings(Settings::TonemapperValues& tonemapper)
{
	bool changed = ImGui::Checkbox("Replace game tonemap", &tonemapper.replace_game_tonemap);
	ImGui::BeginDisabled(!tonemapper.replace_game_tonemap);
	changed |= ImGui::SliderFloat("Exposure", &tonemapper.exposure, k_exposure_range.min, k_exposure_range.max, "%.2f");
	changed |= ImGui::SliderFloat("White point", &tonemapper.white, k_white_range.min, k_white_range.max, "%.1f");
	changed |= ImGui::SliderFloat("Toe strength", &tonemapper.toe, k_toe_range.min, k_toe_range.max, "%.2f");
	changed |= ImGui::SliderFloat("Shoulder strength", &tonemapper.shoulder, k_shoulder_range.min, k_shoulder_range.max,
		"%.2f");
	changed |= ImGui::SliderFloat("Bloom strength", &tonemapper.bloom, k_bloom_range.min, k_bloom_range.max, "%.2f");
	changed |= ImGui::Checkbox("Dither", &tonemapper.dither);
	changed |= ImGui::Checkbox("Apply LUT", &tonemapper.apply_lut);

	if (ImGui::Button("Reset to defaults"))
	{
		const bool replace_game_tonemap = tonemapper.replace_game_tonemap;
		tonemapper = { .replace_game_tonemap = replace_game_tonemap };
		changed = true;
	}

	ImGui::EndDisabled();
	return changed;
}
} // namespace

namespace ds3ea
{
void Addon::draw_settings()
{
	Settings::Values values = m_settings.get();
	bool changed = false;

	if (ImGui::CollapsingHeader("Tonemapper", ImGuiTreeNodeFlags_DefaultOpen))
		changed |= draw_tonemapper_settings(values.tonemapper);

	if (changed)
		m_settings.set(values);
}
} // namespace ds3ea
