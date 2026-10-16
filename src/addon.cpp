#include "addon.hpp"
#include "addon_info.hpp"
#include "listeners/device_listener.hpp"
#include "listeners/pipeline_listener.hpp"
#include "settings.hpp"
#include "utils/boundary.hpp"
#include "utils/build_config.hpp"
#include "utils/ini_settings_store.hpp"
#include "utils/logging.hpp"

#include <fmt/std.h>

#include <chrono>
#include <cstdint>
#include <memory>

namespace
{
using namespace ds3ea;

// Relative, so it resolves against the game's working directory
constexpr const wchar_t* k_ini_file_name = L"ds3ea.ini";

// How often changed settings are saved (and, in Debug builds, an edited ds3ea.ini is reloaded)
constexpr std::chrono::milliseconds k_settings_interval(500);

// How long to wait after a failed save before trying again, so a file that can't be written isn't reported twice a
// second
constexpr std::chrono::seconds k_settings_retry_interval(10);

// The instance the static ReShade handlers forward to. Set once the features are attached, before the handlers are
// registered, and cleared after the add-on is unregistered, so no handler sees it change.
Addon* g_instance = nullptr;
} // namespace

namespace ds3ea
{
struct Addon::Key
{
	explicit Key() = default;
};

Addon::Addon(HMODULE hmod, Key /*key*/)
	: m_module(hmod)
	, m_settings_file(k_ini_file_name)
	, m_settings(m_settings_file)
	, m_tonemapper(m_settings)
{
	// Every listener is attached before any event can arrive, so the lists never change while handlers read them. The
	// tracker is the first pipeline listener, so it has a new shader's render pass before any other one runs.
	m_pipeline_listeners.push_back(&m_tracker);
	m_pipeline_listeners.push_back(&m_tonemapper);
	m_tracker.attach(&m_tonemapper);
	m_device_listeners.push_back(&m_tonemapper);

	g_instance = this;
	reshade::register_event<reshade::addon_event::init_device>(&Addon::on_init_device);
	reshade::register_event<reshade::addon_event::destroy_device>(&Addon::on_destroy_device);
	reshade::register_event<reshade::addon_event::init_pipeline>(&Addon::on_init_pipeline);
	reshade::register_event<reshade::addon_event::destroy_pipeline>(&Addon::on_destroy_pipeline);
	reshade::register_event<reshade::addon_event::destroy_command_list>(&Addon::on_destroy_command_list);
	reshade::register_event<reshade::addon_event::bind_pipeline>(&Addon::on_bind_pipeline);
	reshade::register_event<reshade::addon_event::draw>(&Addon::on_draw);
	reshade::register_event<reshade::addon_event::draw_indexed>(&Addon::on_draw_indexed);
	reshade::register_event<reshade::addon_event::draw_or_dispatch_indirect>(&Addon::on_draw_or_dispatch_indirect);
	reshade::register_event<reshade::addon_event::close_command_list>(&Addon::on_close_command_list);
	reshade::register_event<reshade::addon_event::present>(&Addon::on_present);
	reshade::register_overlay(nullptr, &Addon::on_draw_settings);
	log::info("{} loaded, settings from {}", k_addon_name, m_settings_file.get_path());
}

Addon::~Addon()
{
	// First, so no handler touches the settings file while it's saved
	reshade::unregister_addon(m_module);
	g_instance = nullptr;

	// A destructor must not throw, so the save goes through call_and_catch like any other boundary
	call_and_catch("settings save at exit", [this]
	{
		if (m_settings.has_unsaved_changes())
			static_cast<void>(m_settings.save(m_settings_file));
	});
	log::set_sink(nullptr);
}

std::unique_ptr<Addon> Addon::create(HMODULE hmod)
{
	// ReShade refuses a module it has already registered, so a second create() for the same DLL returns nullptr
	if (!reshade::register_addon(hmod))
		return nullptr;

	// Before construction, so loading the settings can already log
	log::set_sink(&log::write_to_reshade);
	std::unique_ptr<Addon> addon;
	call_and_catch("add-on init", [&addon, hmod] { addon = std::make_unique<Addon>(hmod, Key()); });

	if (addon == nullptr)
	{
		log::set_sink(nullptr);
		reshade::unregister_addon(hmod);
	}

	return addon;
}

void Addon::on_init_device(reshade::api::device* device)
{
	call_and_catch("init_device", [device]
	{
		const auto api = static_cast<std::uint32_t>(device->get_api());
		log::trace("on_init_device called (device api {:#x})", api);
		for (IDeviceListener* const listener : g_instance->m_device_listeners)
			listener->on_device_created(device);
	});
}

void Addon::on_destroy_device(reshade::api::device* device)
{
	call_and_catch("destroy_device", [device]
	{
		for (IDeviceListener* const listener : g_instance->m_device_listeners)
			listener->on_device_destroyed(device);
	});
}

void Addon::on_init_pipeline(reshade::api::device* /*device*/, reshade::api::pipeline_layout /*layout*/,
	std::uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects,
	reshade::api::pipeline pipeline)
{
	call_and_catch("init_pipeline", [subobject_count, subobjects, pipeline]
	{
		for (IPipelineListener* const listener : g_instance->m_pipeline_listeners)
			listener->on_pipeline_created(subobject_count, subobjects, pipeline.handle);
	});
}

void Addon::on_destroy_pipeline(reshade::api::device* /*device*/, reshade::api::pipeline pipeline)
{
	call_and_catch("destroy_pipeline", [pipeline]
	{
		for (IPipelineListener* const listener : g_instance->m_pipeline_listeners)
			listener->on_pipeline_destroyed(pipeline.handle);
	});
}

void Addon::on_destroy_command_list(reshade::api::command_list* cmd_list)
{
	call_and_catch("destroy_command_list", [cmd_list] { g_instance->m_tracker.on_destroy_command_list(cmd_list); });
}

void Addon::on_bind_pipeline(reshade::api::command_list* cmd_list, reshade::api::pipeline_stage stages,
	reshade::api::pipeline pipeline)
{
	call_and_catch("bind_pipeline", [cmd_list, stages, pipeline]
		{ g_instance->m_tracker.on_bind_pipeline(cmd_list, stages, pipeline.handle); });
}

bool Addon::on_draw(reshade::api::command_list* cmd_list, std::uint32_t vertex_count, std::uint32_t instance_count,
	std::uint32_t first_vertex, std::uint32_t first_instance)
{
	const DrawCallArgs draw = {
		.kind = DrawKind::draw,
		.count = vertex_count,
		.instance_count = instance_count,
		.first = first_vertex,
		.vertex_offset = 0,
		.first_instance = first_instance,
	};
	bool skip = false;
	call_and_catch("draw", [cmd_list, &draw, &skip] { skip = g_instance->m_tracker.on_draw(cmd_list, draw); });
	return skip;
}

bool Addon::on_draw_indexed(reshade::api::command_list* cmd_list, std::uint32_t index_count,
	std::uint32_t instance_count, std::uint32_t first_index, std::int32_t vertex_offset, std::uint32_t first_instance)
{
	const DrawCallArgs draw = {
		.kind = DrawKind::draw_indexed,
		.count = index_count,
		.instance_count = instance_count,
		.first = first_index,
		.vertex_offset = vertex_offset,
		.first_instance = first_instance,
	};
	bool skip = false;
	call_and_catch("draw_indexed", [cmd_list, &draw, &skip] { skip = g_instance->m_tracker.on_draw(cmd_list, draw); });
	return skip;
}

bool Addon::on_draw_or_dispatch_indirect(reshade::api::command_list* cmd_list, reshade::api::indirect_command type,
	reshade::api::resource /*buffer*/, std::uint64_t /*offset*/, std::uint32_t /*draw_count*/,
	std::uint32_t /*stride*/)
{
	if (type == reshade::api::indirect_command::dispatch)
		return false;

	const DrawCallArgs draw = { .kind = DrawKind::indirect };
	bool skip = false;
	call_and_catch("draw_indirect", [cmd_list, &draw, &skip] { skip = g_instance->m_tracker.on_draw(cmd_list, draw); });
	return skip;
}

void Addon::on_close_command_list(reshade::api::command_list* cmd_list)
{
	call_and_catch("close_command_list", [cmd_list] { g_instance->m_tracker.on_close_command_list(cmd_list); });
}

void Addon::on_present(reshade::api::command_queue* queue, reshade::api::swapchain* /*swapchain*/,
	const reshade::api::rect* /*source_rect*/, const reshade::api::rect* /*dest_rect*/,
	std::uint32_t /*dirty_rect_count*/, const reshade::api::rect* /*dirty_rects*/)
{
	call_and_catch("present", [queue]
	{
		g_instance->m_tracker.on_present(queue);
		g_instance->update_settings_file();
	});
}

void Addon::on_draw_settings(reshade::api::effect_runtime* /*runtime*/)
{
	call_and_catch("settings overlay", [] { g_instance->draw_settings(); });
}

void Addon::update_settings_file()
{
	const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	if (now < m_next_settings_update)
		return;

	m_next_settings_update = now + k_settings_interval;

	// Before saving, so an edited file wins over unsaved changes
	if constexpr (k_debug_build)
		m_settings.reload_if_changed_on_disk(m_settings_file);

	// The store logs a failed save
	if (m_settings.has_unsaved_changes() && !m_settings.save(m_settings_file))
		m_next_settings_update = now + k_settings_retry_interval;
}
} // namespace ds3ea
