#pragma once

#include <cstddef>

#include "../Il2CppString.h"
#include "PaimonBeyondListType.h"
#include "PaimonEntryCategory.h"
#include "PaimonEntryType.h"
#include "PaimonTeyvatListType.h"

namespace MoleMole
{
#pragma pack(push, 8)
	struct PaimonEntryConfig
	{
		PaimonEntryType entryType;
		Il2CppString* titleTextMapID;
		void* icon; // UnityEngine.Sprite
		PaimonEntryCategory category;
		PaimonEntryCategory showCategory;
		PaimonTeyvatListType teyvatList;
		PaimonBeyondListType beyondList;
	};
#pragma pack(pop)
} /* MoleMole */
