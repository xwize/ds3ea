#include "utils/memory_patch.hpp"
#include "utils/boundary.hpp"
#include "utils/error.hpp"
#include "utils/result.hpp"

#include <Windows.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>
#include <system_error>
#include <utility>

namespace
{
using namespace ds3ea;

/**
 * Copies bytes over target, making its pages writable for the copy and flushing the instruction cache after, since
 * target is usually code.
 * @param target Where to write.
 * @param bytes What to write; the same size as target.
 * @returns Nothing, or an internal Error if the page protection can't be changed.
 */
Result<void> write_protected(std::span<std::byte> target, std::span<const std::byte> bytes)
{
	assert(target.size() == bytes.size() && "Bug: a memory patch's sizes differ");

	DWORD old_protection = 0;

	if (VirtualProtect(target.data(), target.size(), PAGE_EXECUTE_READWRITE, &old_protection) == FALSE)
	{
		const std::error_code error(static_cast<int>(GetLastError()), std::system_category());
		return make_error(ErrorCategory::internal, "cannot make {} bytes at {} writable: {}", target.size(),
			static_cast<const void*>(target.data()), error.message());
	}

	std::ranges::copy(bytes, target.begin());
	VirtualProtect(target.data(), target.size(), old_protection, &old_protection);
	FlushInstructionCache(GetCurrentProcess(), target.data(), target.size());
	return {};
}
} // namespace

namespace ds3ea
{
MemoryPatch::MemoryPatch(std::span<std::byte> target, std::span<const std::byte> original)
	: m_target(target)
	, m_original(original.begin(), original.end())
{
}

MemoryPatch::~MemoryPatch()
{
	if (m_target.empty())
		return;

	call_and_catch("memory patch restore", [this] { get_or_throw(write_protected(m_target, m_original)); });
}

MemoryPatch::MemoryPatch(MemoryPatch&& other) noexcept
	: m_target(std::exchange(other.m_target, {}))
	, m_original(std::move(other.m_original))
{
}

MemoryPatch& MemoryPatch::operator=(MemoryPatch&& other) noexcept
{
	if (this != &other)
	{
		MemoryPatch old(std::move(*this));
		m_target = std::exchange(other.m_target, {});
		m_original = std::move(other.m_original);
	}

	return *this;
}

Result<MemoryPatch> MemoryPatch::apply(std::span<std::byte> target, std::span<const std::byte> original,
	std::span<const std::byte> replacement)
{
	assert(original.size() == target.size() && replacement.size() == target.size() &&
		"Bug: a memory patch's sizes differ");

	if (!std::ranges::equal(target, original))
		return make_error(ErrorCategory::not_found, "the {} bytes at {} aren't the ones the patch expects",
			target.size(), static_cast<const void*>(target.data()));

	return write_protected(target, replacement).map([target, original] { return MemoryPatch(target, original); });
}
} // namespace ds3ea
