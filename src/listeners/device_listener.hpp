#pragma once

#include "reshade_forward.hpp"

namespace ds3ea
{
/** Told when the game's D3D11 device is created and destroyed, to make and release GPU resources. */
class IDeviceListener
{
public:
	IDeviceListener() = default;
	virtual ~IDeviceListener() = default;

	IDeviceListener(const IDeviceListener&) = delete;
	IDeviceListener& operator=(const IDeviceListener&) = delete;
	IDeviceListener(IDeviceListener&&) = delete;
	IDeviceListener& operator=(IDeviceListener&&) = delete;

	/** @param device The game's new device. */
	virtual void on_device_created(reshade::api::device* /*device*/) {}

	/** @param device The device about to be destroyed; release everything made from it. */
	virtual void on_device_destroyed(reshade::api::device* /*device*/) {}
};
} // namespace ds3ea
