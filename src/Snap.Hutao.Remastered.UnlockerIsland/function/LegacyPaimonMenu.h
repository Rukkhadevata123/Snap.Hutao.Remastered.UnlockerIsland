#pragma once

#include "IFunction.h"
#include "FunctionType.h"
#include "../il2cpp/MoleMole/PaimonEntryType.h"

// ============================================================
// 使用旧版派蒙菜单（Legacy Paimon menu）
//
// 7.2 的 UIEntryConfig 把千星奇域的“商城 / 推荐”入口标成了
// Teyvat|Beyond、teyvatList=Main、beyondList=Main，导致提瓦特菜单里
// 也会多出一份，千星奇域里还占主栏。本功能在 SetupPaimonConfig 重建
// 列表之前，把这两条改成 showCategory=Beyond、teyvatList=Hidden、
// beyondList=Sub。
//
// SetupPaimonConfig 由 Constants.cpp 的特征码定位；UIEntryConfig 实例
// 从该函数自己的静态缓存读取，静态块 / 字段偏移由 Hooks.cpp 从机器码
// 里解析，不再依赖固定 RVA。
// ============================================================
class LegacyPaimonMenu : public IFunction
{
public:
    void Initialize() override;
    void OnUpdate() override {}
    void* GetHookFunction() override;
    bool IsEnabled() override;
    void SetEnabled(bool enabled) override { (void)enabled; }
    FunctionType GetFunctionType() override { return FunctionType::LEGACY_PAIMON_MENU; }

    static void HookSetupPaimonConfig(void* pThis);

private:
    static const char* EntryTypeName(MoleMole::PaimonEntryType type);
    static bool ShouldShowInBeyondOnly(MoleMole::PaimonEntryType type);

    static void PatchEntryConfigs(void* listPointer);
    static void* ResolveUIEntryConfig();
    static void PatchEntryConfigsIfReady();
    static void ApplyPatches();
};
