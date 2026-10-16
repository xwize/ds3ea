#pragma once

#include "reshade_forward.hpp"

#include <cstdint>

namespace ds3ea
{
/**
 * Told when the game creates and destroys shaders. A shader's code is only available when it's created, so anything
 * worked out from the code is worked out then; per-shader data must be forgotten when the shader is destroyed,
 * because the game reuses handles.
 */
class IPipelineListener
{
public:
	IPipelineListener() = default;
	virtual ~IPipelineListener() = default;

	IPipelineListener(const IPipelineListener&) = delete;
	IPipelineListener& operator=(const IPipelineListener&) = delete;
	IPipelineListener(IPipelineListener&&) = delete;
	IPipelineListener& operator=(IPipelineListener&&) = delete;

	/**
	 * The game created a shader (in D3D11 every shader is its own pipeline).
	 * @param subobject_count How many subobjects the pipeline has.
	 * @param subobjects Its subobjects; in D3D11 one shader, with its compiled code.
	 * @param pipeline The new shader's handle.
	 */
	virtual void on_pipeline_created(std::uint32_t /*subobject_count*/,
		const reshade::api::pipeline_subobject* /*subobjects*/, std::uint64_t /*pipeline*/)
	{
	}

	/** @param pipeline The destroyed shader's handle. */
	virtual void on_pipeline_destroyed(std::uint64_t /*pipeline*/) {}
};
} // namespace ds3ea
