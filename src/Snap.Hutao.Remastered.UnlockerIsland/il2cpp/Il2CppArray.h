#pragma once

#include <cstdint>

#include "Il2CppObject.h"

// IL2CPP 数组头（x64）：
//   0x00  Il2CppObject obj
//   0x10  Il2CppArrayBounds* bounds
//   0x18  il2cpp_array_size_t max_length
//   0x20  元素起始地址
typedef std::uintptr_t il2cpp_array_size_t;
typedef std::int32_t il2cpp_array_lower_bound_t;

struct Il2CppArrayBounds
{
    il2cpp_array_size_t length;
    il2cpp_array_lower_bound_t lower_bound;
};

// 元素区只声明成 [32] 占位，目的是让 obj/bounds/max_length 的偏移正确；
// 真实长度一律看 max_length（Count()），不要按 32 去循环。
// 显式 pack(8) 固定 x64 指针对齐（这些偏移本来就是 8 的倍数，属于防御性写法）。
#pragma pack(push, 8)
template<typename T> class Il2CppArray
{
    Il2CppObject obj;
    Il2CppArrayBounds* bounds;
    il2cpp_array_size_t max_length;
    T vector[32];

public:
    inline T Get(int index) {
        return vector[index];
    }

    inline void Set(int index, T value) {
        vector[index] = value;
    }

    inline void RemoveAt(int index) {
        if (index < 0 || (il2cpp_array_size_t)index >= max_length) {
            return;
        }

        const il2cpp_array_size_t pos = (il2cpp_array_size_t)index;
        for (il2cpp_array_size_t i = pos + 1; i < max_length; ++i) {
            vector[i - 1] = vector[i];
        }

        --max_length;
        vector[max_length] = T();
    }

    inline void Remove(T value) {
        for (il2cpp_array_size_t i = 0; i < max_length; ++i) {
            if (vector[i] == value) {
                RemoveAt((int)i);
                break;
            }
        }
    }

    inline il2cpp_array_size_t Count() {
        return max_length;
    }
};
#pragma pack(pop)
