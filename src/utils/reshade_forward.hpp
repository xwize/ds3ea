#pragma once

#include <cstdint>

/**
 * @file
 * Forward declarations of the ReShade types our headers pass around, so they needn't include reshade.hpp. The names
 * are ReShade's and the types are defined there, hence the NOLINT.
 */

// NOLINTBEGIN(readability-identifier-naming, cppcoreguidelines-virtual-class-destructor)
namespace reshade::api
{
struct command_list;
struct command_queue;
struct device;
struct pipeline_subobject;
enum class pipeline_stage : std::uint32_t;
} // namespace reshade::api
// NOLINTEND(readability-identifier-naming, cppcoreguidelines-virtual-class-destructor)
