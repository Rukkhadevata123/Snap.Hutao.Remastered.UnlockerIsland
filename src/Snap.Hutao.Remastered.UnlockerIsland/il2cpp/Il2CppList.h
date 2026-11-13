#pragma once

#include "Il2CppArray.h"
#include "Il2CppObject.h"

#pragma pack(push, 8)
template<typename T> class Il2CppList
{
    Il2CppObject obj;
    Il2CppArray<T>* array;
    int size;
    int version;

public:
    inline T Get(int index) {
        return array->Get(index);
    }

    inline void Set(int index, T value) {
        array->Set(index, value);
    }

    inline void RemoveAt(int index) {
        if (index < 0 || index >= size) {
            return;
        }

        array->RemoveAt(index);
        --size;
        ++version;
    }

    inline void Remove(T value) {
        array->Remove(value);

        size--;
        version++;
    }

    inline int Count() {
        return size;
    }
};
#pragma pack(pop)
