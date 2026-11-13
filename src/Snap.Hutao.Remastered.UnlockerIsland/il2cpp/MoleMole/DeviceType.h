#pragma once

#include <cstdint>

namespace MoleMole
{
	// dump.cs: HBPCBEIMOCC (TypeDefIndex: 44060, Assembly-CSharp)
	// 当前输入设备类型；framework.h 里原来直接叫 DeviceType。
	enum class DeviceType : std::int32_t
	{
		TouchScreen = 0,
		KeyboardWithTouchScreen = 1,
		KeyboardWithMouse = 2,
		Joypad = 3,
	};

	static_assert(sizeof(DeviceType) == 4, "DeviceType 必须与 IL2CPP 枚举同为 4 字节");
} /* MoleMole */
