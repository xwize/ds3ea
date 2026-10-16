#include "addon.hpp"
#include "settings.hpp"
#include "utils/reshade_with_imgui.hpp"

namespace ds3ea
{
void Addon::draw_settings()
{
	SettingsValues values = m_settings.get();
	bool changed = false;

	if (ImGui::CollapsingHeader("Tonemapper", ImGuiTreeNodeFlags_DefaultOpen))
		changed |= ImGui::Checkbox("Replace game tonemap", &values.tonemapper.replace_game_tonemap);

	if (changed)
		m_settings.set(values);
}
} // namespace ds3ea
