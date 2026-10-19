#include "utils/error.hpp"
#include "utils/memory_patch.hpp"
#include "utils/result.hpp"

#include <gtest/gtest.h>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>

namespace
{
constexpr std::array<std::byte, 4> k_original = { std::byte { 0xE8 }, std::byte { 0x01 }, std::byte { 0x02 },
	std::byte { 0x03 } };
constexpr std::array<std::byte, 4> k_replacement = { std::byte { 0x90 }, std::byte { 0x90 }, std::byte { 0x90 },
	std::byte { 0x90 } };

/** A read-only, executable page holding k_original, like the game's code. */
class MemoryPatchTest : public testing::Test
{
protected:
	void SetUp() override
	{
		m_page = static_cast<std::byte*>(VirtualAlloc(nullptr, k_original.size(), MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE));
		ASSERT_NE(m_page, nullptr);
		m_target = std::span(m_page, k_original.size());
		std::ranges::copy(k_original, m_target.begin());
		DWORD old_protection = 0;
		ASSERT_NE(VirtualProtect(m_page, k_original.size(), PAGE_EXECUTE_READ, &old_protection), FALSE);
	}

	void TearDown() override
	{
		VirtualFree(m_page, 0, MEM_RELEASE);
	}

	[[nodiscard]] bool holds(std::span<const std::byte> bytes) const
	{
		return std::ranges::equal(m_target, bytes);
	}

	std::span<std::byte> m_target;

private:
	std::byte* m_page = nullptr;
};

TEST_F(MemoryPatchTest, WritesTheReplacementIntoReadOnlyCode)
{
	const ds3ea::Result<ds3ea::MemoryPatch> patch = ds3ea::MemoryPatch::apply(m_target, k_original, k_replacement);

	ASSERT_TRUE(patch.has_value());
	EXPECT_TRUE(holds(k_replacement));
}

TEST_F(MemoryPatchTest, KeepsThePageProtection)
{
	const ds3ea::Result<ds3ea::MemoryPatch> patch = ds3ea::MemoryPatch::apply(m_target, k_original, k_replacement);
	MEMORY_BASIC_INFORMATION info = {};
	VirtualQuery(m_target.data(), &info, sizeof(info));

	EXPECT_EQ(info.Protect, static_cast<DWORD>(PAGE_EXECUTE_READ));
}

TEST_F(MemoryPatchTest, PutsTheOriginalBackWhenDestroyed)
{
	{
		const ds3ea::Result<ds3ea::MemoryPatch> patch = ds3ea::MemoryPatch::apply(m_target, k_original,
			k_replacement);
		ASSERT_TRUE(patch.has_value());
	}

	EXPECT_TRUE(holds(k_original));
}

TEST_F(MemoryPatchTest, RefusesMemoryThatDoesNotHoldTheOriginal)
{
	const ds3ea::Result<ds3ea::MemoryPatch> patch = ds3ea::MemoryPatch::apply(m_target, k_replacement, k_original);

	ASSERT_FALSE(patch.has_value());
	EXPECT_EQ(patch.error().category, ds3ea::ErrorCategory::not_found);
	EXPECT_TRUE(holds(k_original));
}

TEST_F(MemoryPatchTest, OnlyTheLastOwnerPutsTheOriginalBack)
{
	std::optional<ds3ea::MemoryPatch> owner;
	{
		ds3ea::Result<ds3ea::MemoryPatch> patch = ds3ea::MemoryPatch::apply(m_target, k_original, k_replacement);
		ASSERT_TRUE(patch.has_value());
		owner.emplace(std::move(*patch));
	}

	EXPECT_TRUE(holds(k_replacement));
	owner.reset();
	EXPECT_TRUE(holds(k_original));
}
} // namespace
