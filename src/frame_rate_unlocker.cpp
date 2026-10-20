/*
 * Based on DS3DebugFPS (https://github.com/0dm/DS3DebugFPS) by 0dm, under the Apache License, Version 2.0
 * (http://www.apache.org/licenses/LICENSE-2.0). The patterns, field offsets and sprint factor maths come from it.
 */

#include "frame_rate_unlocker.hpp"
#include "game_image.hpp"
#include "settings.hpp"
#include "utils/build_config.hpp"
#include "utils/byte_pattern.hpp"
#include "utils/logging.hpp"
#include "utils/memory_patch.hpp"
#include "utils/result.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using namespace ds3ea;

// SprjFlipper's construction: mov ecx, 0x368 (the object's size); call (the allocator)
constexpr std::string_view k_flipper_alloc_pattern = "B9 68 03 00 00 E8";

// How far after the allocation the constructor stores the object in its global: mov [rip+global], rax
constexpr std::size_t k_flipper_store_search = 64;
constexpr std::string_view k_flipper_store_pattern = "48 89 05";

// Where a mov's [rip+x] displacement is, and the mov's size
constexpr std::size_t k_mov_displacement = 3;
constexpr std::size_t k_mov_size = 7;

// mov rbx, [rip+WorldChrMan]
constexpr std::string_view k_world_chr_man_pattern =
	"48 8B 1D ?? ?? ?? 04 48 8B F9 48 85 DB ?? ?? 8B 11 85 D2 ?? ?? 8D";

// The debug frame limiter's rate (float) and switch (byte), and the measured frame time (float, seconds)
constexpr std::uintptr_t k_frame_rate_offset = 0x354;
constexpr std::uintptr_t k_use_debug_limiter_offset = 0x358;
constexpr std::uintptr_t k_frame_time_offset = 0x26C;

// What FrameRateLimit = 0 asks for: effectively uncapped, as DS3DebugFPS's default
constexpr float k_uncapped_frame_rate = 1000.0f;

/**
 * The sprint check (addss; shufps; sqrtss; mulss xmm, [rip+stuck_scale]; comiss), then two more mulss [rip+x] at
 * fixed distances for the slowdown and recovery rates.
 */
constexpr std::string_view k_sprint_check_pattern =
	"F3 0F 58 ?? 0F C6 ?? ?? 0F 51 ?? F3 0F 59 05 ?? ?? ?? ?? 0F 2F";
constexpr std::string_view k_mulss_rip_pattern = "F3 0F 59 05";
// Where each mulss starts, relative to the pattern, in SprintSpeedFactors' field order
constexpr std::array<std::size_t, 3> k_sprint_mulss_offsets = { 0x0B, 0x29, 0x54 };
constexpr std::size_t k_mulss_displacement = 4;
constexpr std::size_t k_mulss_size = 8;

// The factors' page must be within a rel32 of the code: searched upwards from the image's end in allocation steps
constexpr std::size_t k_allocation_granularity = 0x10000;
constexpr std::size_t k_near_page_search = 0x70000000;

constexpr std::chrono::seconds k_stats_interval(1);

/**
 * Reads memory that may not be mapped, without faulting.
 * @param address Where to read.
 * @returns The value, or nothing if it can't be read.
 */
template <typename T>
std::optional<T> read_memory(std::uintptr_t address)
{
	T value {};
	SIZE_T read = 0;

	// NOLINTNEXTLINE(performance-no-int-to-ptr,cppcoreguidelines-pro-type-reinterpret-cast): an address in the game
	if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), &value, sizeof(T), &read) ==
			FALSE ||
		read != sizeof(T))
		return std::nullopt;

	return value;
}

/** @returns A pointer's address, for arithmetic with the game's addresses. */
std::uintptr_t address_of(const void* pointer)
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): only an integer converts a pointer to an address
	return reinterpret_cast<std::uintptr_t>(pointer);
}

/** @returns The address of an RVA in the game's image. */
std::uintptr_t to_address(std::size_t rva)
{
	return address_of(get_game_image().data()) + rva;
}

/**
 * @param object A pointer read from a global.
 * @returns True if it looks like a game object: aligned, readable, and its vtable inside the exe.
 */
bool is_game_object(std::uintptr_t object)
{
	const std::span<std::byte> image = get_game_image();
	const std::uintptr_t image_start = address_of(image.data());
	const std::optional<std::uintptr_t> vtable = object % alignof(void*) == 0 ? read_memory<std::uintptr_t>(object)
																			  : std::nullopt;
	return object != 0 && vtable && *vtable >= image_start && *vtable < image_start + image.size();
}

/**
 * Finds SprjFlipper through the global its constructor stores it in.
 * @param image The game's exe.
 * @returns The object, or 0 (logged) if no global holds one.
 */
std::uintptr_t find_flipper(std::span<const std::byte> image)
{
	const BytePattern store = parse_byte_pattern(k_flipper_store_pattern);
	const std::vector<std::size_t> allocs = find_in_code(image, parse_byte_pattern(k_flipper_alloc_pattern));

	for (const std::size_t alloc : allocs)
	{
		const std::vector<std::size_t> stores = find_byte_pattern(image.subspan(alloc, k_flipper_store_search), store);

		if (stores.empty())
			continue;

		const std::size_t instruction = alloc + stores.front();
		const std::optional<std::size_t> global = resolve_rip_relative(image, instruction + k_mov_displacement,
			instruction + k_mov_size);
		const std::uintptr_t object = global ? read_memory<std::uintptr_t>(to_address(*global)).value_or(0) : 0;

		if (is_game_object(object))
		{
			log::debug("FrameRateUnlocker: SprjFlipper {:#x} in global DarkSoulsIII.exe+{:#x}", object, *global);
			return object;
		}
	}

	log::warning("FrameRateUnlocker: not unlocking the frame rate, none of the {} SprjFlipper constructor matches "
				 "leads to the object",
		allocs.size());
	return 0;
}

/**
 * Reserves a page within a rel32 of the image, for data the game's code addresses RIP-relative.
 * @param image The game's exe.
 * @returns The page, or nullptr if none is free.
 */
void* allocate_near(std::span<const std::byte> image)
{
	const std::uintptr_t image_end = address_of(image.data()) + image.size();
	const std::uintptr_t first = (image_end + k_allocation_granularity - 1) & ~(k_allocation_granularity - 1);

	for (std::uintptr_t hint = first; hint < image_end + k_near_page_search; hint += k_allocation_granularity)
	{
		// NOLINTNEXTLINE(performance-no-int-to-ptr,cppcoreguidelines-pro-type-reinterpret-cast): VirtualAlloc's hint
		void* const page = VirtualAlloc(reinterpret_cast<void*>(hint), sizeof(SprintSpeedFactors),
			MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

		if (page != nullptr)
			return page;
	}

	return nullptr;
}
} // namespace

namespace ds3ea
{
SprintSpeedFactors compute_sprint_speed_factors(float frame_rate_limit, float measured_frame_time)
{
	constexpr float k_shortest_frame = 1.0f / 480.0f;
	constexpr float k_longest_frame = 0.05f;

	constexpr float k_vanilla_frame_rate = 60.0f;
	constexpr float k_high_frame_rate = 90.0f;

	// Below this speed in m/s the check counts the character as stuck (the game's 30.0 = 1 / (2 m/s * 1/60 s))
	constexpr float k_stuck_speed = 2.0f;

	constexpr float k_vanilla_slowdown = 0.8f;
	constexpr float k_vanilla_recovery = 1.2f;

	constexpr float k_high_slack = 1.2f;
	constexpr float k_high_recovery = 1.5f;

	const bool is_measured_valid = measured_frame_time >= k_shortest_frame &&
		measured_frame_time <= k_longest_frame;

	const float frame_time = std::clamp(is_measured_valid ? measured_frame_time : 1.0f / frame_rate_limit,
		k_shortest_frame, k_longest_frame);

	const bool is_high = frame_rate_limit >= k_high_frame_rate;
	const float slack = is_high ? k_high_slack : 1.0f;
	const float recovery = is_high ? k_high_recovery : k_vanilla_recovery;

	return SprintSpeedFactors {
		.stuck_scale = slack / (k_stuck_speed * frame_time),
		.slowdown = std::pow(k_vanilla_slowdown, frame_time * k_vanilla_frame_rate),
		.recovery = std::pow(recovery, frame_time * k_vanilla_frame_rate),
	};
}

FrameRateUnlocker::FrameRateUnlocker(const Settings& settings)
	: m_frame_rate_limit(settings.get().frame_limiter.frame_rate_limit)
{
	if (m_frame_rate_limit <= 0.0f)
		m_frame_rate_limit = k_uncapped_frame_rate;

	if constexpr (!k_unlock_frame_rate_in_build)
	{
		log::info("FrameRateUnlocker: not unlocking the frame rate, turned off in this build");
		return;
	}

	if (m_frame_rate_limit == Settings::FrameLimiterValues::k_vanilla_frame_rate_limit)
	{
		log::info("FrameRateUnlocker: not unlocking the frame rate, [FrameLimiter] FrameRateLimit is 60");
		return;
	}

	if (!is_supported_game_version())
	{
		log::warning("FrameRateUnlocker: not unlocking the frame rate, the game version isn't 1.15.x");
		return;
	}

	patch_frame_rate();

	if (m_flipper != 0 && settings.get().frame_limiter.fix_sprint_slowdown)
		patch_sprint_check();

	m_world_chr_man_slot = []() -> std::optional<std::size_t>
	{
		const std::span<std::byte> image = get_game_image();
		const std::vector<std::size_t> matches = find_in_code(image, parse_byte_pattern(k_world_chr_man_pattern));
		return matches.size() == 1
			? resolve_rip_relative(image, matches.front() + k_mov_displacement, matches.front() + k_mov_size)
			: std::nullopt;
	}();
}

FrameRateUnlocker::~FrameRateUnlocker() = default;

void FrameRateUnlocker::PageDeleter::operator()(SprintSpeedFactors* page) const noexcept
{
	VirtualFree(page, 0, MEM_RELEASE);
}

void FrameRateUnlocker::on_present()
{
	if (m_sprint_factors)
	{
		const float frame_time = read_memory<float>(m_flipper + k_frame_time_offset).value_or(0.0f);
		*m_sprint_factors = compute_sprint_speed_factors(m_frame_rate_limit, frame_time);
	}

	if (m_flipper != 0)
		measure_frame();
}

void FrameRateUnlocker::patch_frame_rate()
{
	const std::uintptr_t flipper = find_flipper(get_game_image());

	if (flipper == 0)
		return;

	// NOLINTBEGIN(performance-no-int-to-ptr,cppcoreguidelines-pro-type-reinterpret-cast): read from the game's global
	auto* const frame_rate = reinterpret_cast<float*>(flipper + k_frame_rate_offset);
	auto* const use_debug_limiter = reinterpret_cast<std::uint8_t*>(flipper + k_use_debug_limiter_offset);
	// NOLINTEND(performance-no-int-to-ptr,cppcoreguidelines-pro-type-reinterpret-cast)
	log::debug("FrameRateUnlocker: limiter was rate {}, debug {}", *frame_rate, *use_debug_limiter);
	*frame_rate = m_frame_rate_limit;
	*use_debug_limiter = 1;
	m_flipper = flipper;
	log::info("FrameRateUnlocker: frame rate limit set to {} fps", m_frame_rate_limit);
}

void FrameRateUnlocker::patch_sprint_check()
{
	const std::span<std::byte> image = get_game_image();
	const std::vector<std::size_t> matches = find_in_code(image, parse_byte_pattern(k_sprint_check_pattern));

	if (matches.size() != 1)
	{
		log::warning("FrameRateUnlocker: not fixing the sprint slowdown, its check matched {} times instead of once",
			matches.size());
		return;
	}

	const BytePattern mulss = parse_byte_pattern(k_mulss_rip_pattern);
	const auto is_mulss = [&image, &mulss](std::size_t rva)
	{ return !find_byte_pattern(image.subspan(rva, mulss.size()), mulss).empty(); };

	if (!std::ranges::all_of(k_sprint_mulss_offsets, [&](std::size_t offset) { return is_mulss(matches.front() + offset); }))
	{
		log::warning("FrameRateUnlocker: not fixing the sprint slowdown, its rates aren't where 1.15 has them");
		return;
	}

	auto* const page = static_cast<SprintSpeedFactors*>(allocate_near(image));

	if (page == nullptr)
	{
		log::warning("FrameRateUnlocker: not fixing the sprint slowdown, no free page near the game's code");
		return;
	}

	m_sprint_factors.reset(page);
	*m_sprint_factors = compute_sprint_speed_factors(m_frame_rate_limit, 0.0f);
	const std::array<const float*, 3> fields = { &page->stuck_scale, &page->slowdown, &page->recovery };

	for (std::size_t i = 0; i < fields.size(); ++i)
	{
		const std::size_t instruction = matches.front() + k_sprint_mulss_offsets.at(i);
		const std::size_t constant = resolve_rip_relative(image, instruction + k_mulss_displacement,
			instruction + k_mulss_size).value_or(0);

		const std::span<std::byte> displacement = image.subspan(instruction + k_mulss_displacement,
			sizeof(std::int32_t));
		const std::vector<std::byte> original(displacement.begin(), displacement.end());

		const auto new_displacement = static_cast<std::int32_t>(static_cast<std::int64_t>(address_of(fields.at(i))) -
			static_cast<std::int64_t>(address_of(image.data()) + instruction + k_mulss_size));
		std::array<std::byte, sizeof(std::int32_t)> replacement = {};
		std::memcpy(replacement.data(), &new_displacement, sizeof(new_displacement));

		log::debug("FrameRateUnlocker: sprint constant {} at +{:#x} was {}", i, constant,
			read_memory<float>(to_address(constant)).value_or(NAN));

		Result<MemoryPatch> patch = MemoryPatch::apply(displacement, original, replacement);

		if (!patch)
		{
			log::warning("FrameRateUnlocker: not fixing the sprint slowdown, {}", patch.error());
			m_sprint_patches.clear();
			return;
		}

		m_sprint_patches.push_back(std::move(*patch));
	}

	log::info("FrameRateUnlocker: sprint slowdown fixed, check at DarkSoulsIII.exe+{:#x}", matches.front());
}

void FrameRateUnlocker::measure_frame()
{
	const auto now = std::chrono::steady_clock::now();

	if (m_stats.frames == 0)
		m_stats.started = now;
	else
		m_stats.longest = std::max(m_stats.longest, now - m_stats.last_present);

	m_stats.last_present = now;
	++m_stats.frames;
	const std::chrono::duration<float> window = now - m_stats.started;

	if (window < k_stats_interval)
		return;

	const float fps = static_cast<float>(m_stats.frames - 1) / window.count();
	const float longest_ms = std::chrono::duration<float, std::milli>(m_stats.longest).count();
	float speed = NAN;

	if (m_world_chr_man_slot)
	{
		const std::uintptr_t world = read_memory<std::uintptr_t>(to_address(*m_world_chr_man_slot)).value_or(0);
		const std::uintptr_t player = read_memory<std::uintptr_t>(world + 0x40).value_or(0);
		const std::uintptr_t physics = read_memory<std::uintptr_t>(player + 0x28).value_or(0);
		const std::optional<std::array<float, 3>> position = read_memory<std::array<float, 3>>(physics + 0x80);

		if (position && m_last_position)
		{
			const float dx = position->at(0) - m_last_position->at(0);
			const float dz = position->at(2) - m_last_position->at(2);
			speed = std::sqrt(dx * dx + dz * dz) / window.count();
		}

		m_last_position = position;
	}

	log::info("FrameRateUnlocker: {:.1f} fps, longest frame {:.1f} ms, stuck scale {:.1f}, speed {:.2f}", fps,
		longest_ms, m_sprint_factors ? m_sprint_factors->stuck_scale : NAN, speed);
	m_stats = {};
}
} // namespace ds3ea
