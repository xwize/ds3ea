#include "game_image.hpp"
#include "utils/byte_pattern.hpp"

#include <Windows.h>
#include <Psapi.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace ds3ea;

// The only version our patches are known for; the game's last, 1.15.x
constexpr std::uint16_t k_supported_major = 1;
constexpr std::uint16_t k_supported_minor = 15;

/**
 * @param image A module as mapped in memory.
 * @returns Where each of its executable sections starts (its RVA) and its size.
 */
std::vector<std::pair<std::size_t, std::size_t>> get_code_sections(std::span<const std::byte> image)
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the PE headers are at the start of the image
	const auto* const dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(image.data());
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): and the DOS header says where the NT headers are
	const auto* const headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(image.subspan(dos_header->e_lfanew).data());
	const std::span sections(IMAGE_FIRST_SECTION(headers), headers->FileHeader.NumberOfSections);
	std::vector<std::pair<std::size_t, std::size_t>> code;

	for (const IMAGE_SECTION_HEADER& section : sections)
	{
		if ((section.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
			continue;

		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access): Windows' section header has the size in a union
		code.emplace_back(section.VirtualAddress, section.Misc.VirtualSize);
	}

	return code;
}
} // namespace

namespace ds3ea
{
std::optional<GameVersion> read_game_version()
{
	std::wstring path(MAX_PATH, L' ');
	path.resize(GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size())));
	const DWORD size = GetFileVersionInfoSizeW(path.c_str(), nullptr);
	std::vector<std::byte> info(size);
	VS_FIXEDFILEINFO* fixed = nullptr;
	UINT fixed_size = 0;

	if (size == 0 || GetFileVersionInfoW(path.c_str(), 0, size, info.data()) == FALSE)
		return std::nullopt;

	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): VerQueryValueW returns the block through a void**
	if (VerQueryValueW(info.data(), L"\\", reinterpret_cast<void**>(&fixed), &fixed_size) == FALSE)
		return std::nullopt;

	return GameVersion {
		.major = HIWORD(fixed->dwFileVersionMS),
		.minor = LOWORD(fixed->dwFileVersionMS),
		.patch = HIWORD(fixed->dwFileVersionLS),
		.build = LOWORD(fixed->dwFileVersionLS),
	};
}

bool is_supported_game_version()
{
	const std::optional<GameVersion> version = read_game_version();
	return version && version->major == k_supported_major && version->minor == k_supported_minor;
}

std::span<std::byte> get_game_image()
{
	MODULEINFO info = {};
	GetModuleInformation(GetCurrentProcess(), GetModuleHandleW(nullptr), &info, sizeof(info));
	return { static_cast<std::byte*>(info.lpBaseOfDll), info.SizeOfImage };
}

std::vector<std::size_t> find_in_code(std::span<const std::byte> image, const BytePattern& pattern)
{
	std::vector<std::size_t> matches;

	for (const auto [section_rva, section_size] : get_code_sections(image))
	{
		for (const std::size_t offset : find_byte_pattern(image.subspan(section_rva, section_size), pattern))
			matches.push_back(section_rva + offset);
	}

	return matches;
}

std::optional<std::size_t> resolve_rip_relative(std::span<const std::byte> image, std::size_t displacement_rva,
	std::size_t instruction_end_rva)
{
	std::int32_t displacement = 0;

	if (displacement_rva + sizeof(displacement) > image.size())
		return std::nullopt;

	std::memcpy(&displacement, image.subspan(displacement_rva).data(), sizeof(displacement));
	const auto target = static_cast<std::int64_t>(instruction_end_rva) + displacement;

	if (target < 0 || static_cast<std::uint64_t>(target) >= image.size())
		return std::nullopt;

	return static_cast<std::size_t>(target);
}
} // namespace ds3ea
