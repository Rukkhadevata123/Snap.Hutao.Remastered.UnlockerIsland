#pragma once

#include <cstdint>

namespace MoleMole
{
	enum class PaimonEntryCategory : std::int32_t
	{
		Unknown = 0,
		Teyvat = 1,
		Beyond = 2,
	};

	constexpr PaimonEntryCategory operator|(PaimonEntryCategory lhs, PaimonEntryCategory rhs) noexcept
	{
		return (PaimonEntryCategory)((std::int32_t)lhs | (std::int32_t)rhs);
	}

	constexpr PaimonEntryCategory operator&(PaimonEntryCategory lhs, PaimonEntryCategory rhs) noexcept
	{
		return (PaimonEntryCategory)((std::int32_t)lhs & (std::int32_t)rhs);
	}

	constexpr PaimonEntryCategory operator^(PaimonEntryCategory lhs, PaimonEntryCategory rhs) noexcept
	{
		return (PaimonEntryCategory)((std::int32_t)lhs ^ (std::int32_t)rhs);
	}

	constexpr PaimonEntryCategory operator~(PaimonEntryCategory value) noexcept
	{
		return (PaimonEntryCategory)(~(std::int32_t)value);
	}

	constexpr PaimonEntryCategory& operator|=(PaimonEntryCategory& lhs, PaimonEntryCategory rhs) noexcept
	{
		return lhs = lhs | rhs;
	}

	constexpr PaimonEntryCategory& operator&=(PaimonEntryCategory& lhs, PaimonEntryCategory rhs) noexcept
	{
		return lhs = lhs & rhs;
	}

	static_assert(sizeof(PaimonEntryCategory) == 4, "PaimonEntryCategory 必须与 IL2CPP 枚举同为 4 字节");
} /* MoleMole */
