#pragma once

#include "Il2CppObject.h"

#pragma pack(push, 4)
struct Il2CppString : Il2CppObject
{
    long length;
    wchar_t chars[32];
};
#pragma pack(pop)