#pragma once

/**
 * @file
 * ReShade's add-on API with its overlay's Dear ImGui functions. Include this instead of reshade.hpp: register_addon()
 * only fetches ReShade's ImGui function table when imgui.h came first, and as an inline function it must be the same
 * in every file that includes it.
 */

// IWYU pragma: begin_exports
#include <imgui.h>

#include <reshade.hpp>
// IWYU pragma: end_exports
