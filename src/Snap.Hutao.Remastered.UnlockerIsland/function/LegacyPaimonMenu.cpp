#include "LegacyPaimonMenu.h"

#include "../dllmain.h"
#include "../framework.h"
#include "../il2cpp/MoleMole/PaimonEntryConfig.h"
#include "../Logger.h"
#include "../utils/MemoryUtils.h"
#include "HooksShared.h"

#include <cstddef>
#include <cstdint>

// ============================================================
// IFunction
// ============================================================

void LegacyPaimonMenu::Initialize()
{
    if (g_pEnv->Offsets.SetupPaimonConfig)
    {
        LPVOID setupPaimonConfigAddr = GetFunctionAddress(g_pEnv->Offsets.SetupPaimonConfig);
        if (setupPaimonConfigAddr)
        {
            MH_CreateHook(setupPaimonConfigAddr, &LegacyPaimonMenu::HookSetupPaimonConfig,
                &originalSetupPaimonConfig);
        }
    }
}

void* LegacyPaimonMenu::GetHookFunction()
{
    return (void*)&LegacyPaimonMenu::HookSetupPaimonConfig;
}

bool LegacyPaimonMenu::IsEnabled()
{
    return g_pEnv->LegacyPaimonMenu != FALSE;
}

void LegacyPaimonMenu::HookSetupPaimonConfig(void* pThis)
{
    if (g_pEnv->LegacyPaimonMenu)
    {
        ApplyPatches();
    }

    if (originalSetupPaimonConfig != nullptr)
    {
        ((SetupPaimonConfigFn)originalSetupPaimonConfig)(pThis);
    }
}

// ============================================================
// UIEntryConfig 补丁规则
// ============================================================

const char* LegacyPaimonMenu::EntryTypeName(MoleMole::PaimonEntryType type)
{
    switch (type)
    {
    case MoleMole::PaimonEntryType::BeyondMall:
        return "BeyondMall";
    case MoleMole::PaimonEntryType::BeyondRecommended:
        return "BeyondRecommended";
    default:
        return "unknown";
    }
}

bool LegacyPaimonMenu::ShouldShowInBeyondOnly(MoleMole::PaimonEntryType type)
{
    return type == MoleMole::PaimonEntryType::BeyondMall ||
        type == MoleMole::PaimonEntryType::BeyondRecommended;
}

// ============================================================
// 补丁
// ============================================================

void LegacyPaimonMenu::PatchEntryConfigs(void* listPointer)
{
    auto* list = (Il2CppList<MoleMole::PaimonEntryConfig>*)listPointer;
    const int count = list->Count();

    for (int i = 0; i < count; ++i)
    {
        MoleMole::PaimonEntryConfig entry = list->Get(i);
        if (!ShouldShowInBeyondOnly(entry.entryType))
        {
            continue;
        }

        const bool teyvatFixed = entry.teyvatList == MoleMole::PaimonTeyvatListType::Hidden;
        const bool beyondFixed = entry.beyondList == MoleMole::PaimonBeyondListType::Sub;

        if (teyvatFixed && beyondFixed)
        {
            continue;
        }

        Logf("LegacyPaimonMenu: entryType=%d (%s), "
            "teyvatList %d -> %d, beyondList %d -> %d",
            (int)entry.entryType, EntryTypeName(entry.entryType),
            (int)entry.teyvatList, (int)MoleMole::PaimonTeyvatListType::UGCFold,
            (int)entry.beyondList, (int)MoleMole::PaimonBeyondListType::Main);

        entry.teyvatList = MoleMole::PaimonTeyvatListType::UGCFold;
        entry.beyondList = MoleMole::PaimonBeyondListType::Main;
        list->Set(i, entry);
    }
}

void* LegacyPaimonMenu::ResolveUIEntryConfig()
{
    void* staticBlock = *(void**)((uint8_t*)g_hModule + g_pEnv->Offsets.UIEntryConfigStaticBlock);
    return *(void**)((uint8_t*)staticBlock + g_pEnv->Offsets.UIEntryConfigStaticField);
}

void LegacyPaimonMenu::PatchEntryConfigsIfReady()
{
    void* config = ResolveUIEntryConfig();
    void* listPointer = *(void**)((uint8_t*)config + g_pEnv->Offsets.UIEntryConfigEntryConfigs);
    PatchEntryConfigs(listPointer);
}

void LegacyPaimonMenu::ApplyPatches()
{
    __try
    {
        PatchEntryConfigsIfReady();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Log("LegacyPaimonMenu: patch failed");
    }
}
