#pragma once

#include "utils/result.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace ds3ea
{
/**
 * Bytes overwritten in the game's memory, usually its code, put back when the patch is destroyed. apply() checks the
 * memory still holds the bytes the patch was written against, so a patch made for one game build never lands on
 * different code.
 */
class MemoryPatch
{
public:
	/** Puts the original bytes back. A failure is logged, since there's nothing else to do about it. */
	~MemoryPatch();

	/** Movable so a Result can hold one; not copyable, since only one patch may put the bytes back. */
	MemoryPatch(const MemoryPatch&) = delete;
	MemoryPatch& operator=(const MemoryPatch&) = delete;
	MemoryPatch(MemoryPatch&& other) noexcept;
	MemoryPatch& operator=(MemoryPatch&& other) noexcept;

	/**
	 * Overwrites target with replacement if it holds original, lifting the page protection while it writes.
	 * @param target The memory to patch.
	 * @param original What target must hold; the same size as target.
	 * @param replacement What to write; the same size as target.
	 * @returns The patch, a not_found Error if target holds something else, or an internal Error if the page
	 *     protection can't be changed.
	 */
	[[nodiscard]] static Result<MemoryPatch> apply(std::span<std::byte> target, std::span<const std::byte> original,
		std::span<const std::byte> replacement);

private:
	/**
	 * @param target The patched memory.
	 * @param original What it held before.
	 */
	MemoryPatch(std::span<std::byte> target, std::span<const std::byte> original);

	// Empty once moved from
	std::span<std::byte> m_target;
	std::vector<std::byte> m_original;
};
} // namespace ds3ea
