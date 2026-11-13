#include "Hooks.h"

#include "../framework.h"
#include "../function/HooksShared.h"
#include "../function/FovOverride.h"
#include "../function/DisablePlayerPerspective.h"
#include "../function/DisablePlayerDiveMosaic.h"
#include "../function/DisableFog.h"
#include "../function/EnableSetFps.h"
#include "../function/RemoveTeamProgress.h"
#include "../function/HideQuestBanner.h"
#include "../function/DisableCameraMove.h"
#include "../function/DisableDamageText.h"
#include "../function/TouchMode.h"
#include "../function/ResinItem.h"
#include "../function/DisplayPaimon.h"
#include "../function/HidePlayerInfo.h"
#include "../function/HideGrass.h"
#include "../function/GamepadHotSwitchFunc.h"
#include "../function/InLevelClockPageSpeedUp.h"
#include "../function/CombineHotkey.h"
#include "../function/WeakMapCheck.h"
#include "../function/FastSkipTalk.h"
#include "../function/CatherineRedirect.h"
#include "../function/LegacyPaimonMenu.h"

#include "../BeyondResist.h"
#include "../utils/Task.h"
#include "../utils/Scanner.h"
#include "../Logger.h"
#include "../hook/HookWndProc.h"

#include <vector>
#include <cstring>
#include <cstdint>
#include <Windows.h>

// Offsets are defined in Constants.cpp (g_ChinaOffsets / g_OverseaOffsets)

// ===================================================================
// Shared globals (defined here, declared extern in HooksShared.h)
// ===================================================================

// Core utilities
LPVOID findString = nullptr;
LPVOID findGameObject = nullptr;
LPVOID setActive = nullptr;
LPVOID getActive = nullptr;
LPVOID getName = nullptr;

// Input switching
LPVOID switchInputDeviceToTouchScreen = nullptr;
LPVOID switchInputDeviceToKeyboardMouse = nullptr;
LPVOID switchInputDeviceToJoypad = nullptr;

// Craft / Combine
LPVOID craftEntryPartner = nullptr;
LPVOID checkCanOpenMap = nullptr;

// Team
LPVOID checkCanEnter = nullptr;
LPVOID openTeamPageAccordingly = nullptr;

// Paimon display
LPVOID avatarPaimonAppear = nullptr;

// InLevelClockPage
LPVOID inLevelClockPageCloseButtonClicked = nullptr;

// State flags
bool gameUpdateInit = false;
bool touchScreenInit = false;
bool gamepadHotSwitchInitialized = false;
bool isResistedLastFrame = false;

// Craft menu request flag
bool requestOpenCraft = false;

// ===================================================================
// Original trampolines (set by MH_CreateHook in function Initialize())
// ===================================================================
LPVOID originalGetFrameCount = nullptr;
LPVOID originalSetFov = nullptr;
LPVOID originalPlayerPerspective = nullptr;
LPVOID originalPlayerDiveMosaic = nullptr;
LPVOID originalSetupQuestBanner = nullptr;
LPVOID originalEventCameraMove = nullptr;
LPVOID originalShowOneDamageTextEx = nullptr;
LPVOID originalCraftEntry = nullptr;
LPVOID originalCheckCanOpenMap = nullptr;
LPVOID originalOpenTeam = nullptr;
LPVOID originalSetUID = nullptr;
LPVOID originalSetActive = nullptr;
LPVOID originalSetupResinList = nullptr;
LPVOID originalInLevelClockPageOkButtonClicked = nullptr;
LPVOID originalGameUpdate = nullptr;
LPVOID originalSetWaterMaskUID = nullptr;
LPVOID originalSetupPlayerProfilePage = nullptr;
LPVOID originalAvatarPaimonDisappear = nullptr;

// Non-hooked call targets
LPVOID setFrameCount = nullptr;
LPVOID fnDisplayFog = nullptr;

// Catherine redirect（定义在这里，声明在 function/HooksShared.h）
LPVOID catherineOpenDialog = nullptr;
LPVOID explorationPageEntry = nullptr;
LPVOID originalCatherineEntry = nullptr;
LPVOID originalCatherineDialogCtor = nullptr;

// Legacy Paimon menu（定义在这里，声明在 function/HooksShared.h）
LPVOID originalSetupPaimonConfig = nullptr;

// ===================================================================
// Function registry & dispatch
// ===================================================================
static std::vector<IFunction*> g_functions;

typedef int (*SetFovFn)(void*, float);
typedef void (*UpdateFn)(void*);
typedef void (*SetUidFn)(void*, uint32_t);
typedef void (*SetWaterMaskUIDFn)(void*, Il2CppString*, bool);

static void DispatchUpdate()
{
	bool isResisted = CheckResistInBeyd();

	isResistedLastFrame = isResisted;

	// Dispatch OnUpdate to all registered functions
	for (auto* func : g_functions)
	{
		if (func->IsEnabled())
		{
			func->OnUpdate();
		}
	}
}

DWORD ScanPlayerDiveMosaic()
{
	void* caller = Scanner::Scan(PlayerDiveMosaicPattern);
	void* displayEffect = Scanner::Scan(DisplayEffectPattern);

	if (!caller || !displayEffect)
		return 0;

	int window = 0x800;
	void* lastCall = nullptr;

	for (int i = 0; i < window - 4; ++i)
	{
		uint8_t* currentByte = (uint8_t*)caller + i;

		if (IsCallOpcode(currentByte))
		{
			void* target = Scanner::ResolveRelative(currentByte, 1, 5);
			if (target == displayEffect)
			{
				lastCall = currentByte;
			}
		}
	}

	if (lastCall)
	{
		return (DWORD)GetVirtualAddress((INT64)lastCall);
	}

	return 0;
}

// ===================================================================
// Resin 列表的字段偏移
//
// SetupResinList 里对树脂列表的访问总是编译成
//     48 8B <modrm=10xxx011> <disp32>    ; mov reg,[rbx+disp32]
//     48 85 C9                            ; test rcx,rcx
//     0F 84 <rel32>                       ; je
// 取第一条这样的指令的 disp32。7.2 国服 = 0x228、国际服 = 0x230。
// 找不到返回 0（调用方据此禁用树脂过滤，而不是拿垃圾偏移去解引用）。
// ===================================================================
static DWORD FindResinListFieldOffset(LPVOID function)
{
    if (function == nullptr)
    {
        return 0;
    }

    const auto* code = (const uint8_t*)function;

    __try
    {
        for (int i = 0; i + 11 <= 0x60; ++i)
        {
            // mov r64, [rbx+disp32]：REX.W + 8B + modrm(mod=10, rm=011)
            // 紧跟着是 test rcx,rcx 和 je（0F 84 rel32 或 74 rel8 都认）
            if (code[i] == 0x48 && code[i + 1] == 0x8B && (code[i + 2] & 0xC7) == 0x83 &&
                code[i + 7] == 0x48 && code[i + 8] == 0x85 && code[i + 9] == 0xC9 &&
                (code[i + 10] == 0x0F || code[i + 10] == 0x74))
            {
                DWORD displacement = 0;
                memcpy(&displacement, code + i + 3, sizeof(displacement));
                return displacement;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }

    return 0;
}

// ===================================================================
// 对话冷却门（FastSkipTalk 要 NOP 的那条指令）
//
// 函数里是
//     0F 2E <modrm 10xxx???> <disp32>   ; ucomiss xmm0,[reg+disp32]  当前时间 vs 冷却时间戳
//     0F 86 <rel32>                     ; jbe  -> 冷却没到就直接返回
// 把那条 jbe 换成 6 个 0x90，等效于"冷却永远已过"。
//
// 这条 jbe 在 7.0 国服 = 0x09123FBE、7.1 国服 = 0x0D43F38E、7.2 国服 = 0x10C5E7A7、
// 7.2 国际服 = 0x10C761D0 —— 相对函数入口的偏移从 0x37E 漂到 0x3A7 / 0x3B0，
// 所以不能再写死 +0x37E，改成按指令形状在函数体里找。
// ===================================================================
static void* FindDialogueCooldownJump(void* function)
{
    if (function == nullptr)
    {
        return nullptr;
    }

    const auto* code = (const uint8_t*)function;

    __try
    {
        for (int i = 0; i + 9 <= 0x600; ++i)
        {
            if (code[i] != 0x0F || code[i + 1] != 0x2E)
            {
                continue;                          // ucomiss
            }
            if ((code[i + 2] & 0xC0) != 0x80)
            {
                continue;                          // 只认 [reg+disp32]
            }
            if (code[i + 7] != 0x0F || code[i + 8] != 0x86)
            {
                continue;                          // jbe rel32
            }
            return const_cast<uint8_t*>(code + i + 7);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }

    return nullptr;
}

// ===================================================================
// Pattern-scan offset resolver
// Overrides whatever offsets the test or fallback tables provided
// with values obtained via pattern scanning. Functions that have no
// pattern remain at their fallback value (which may be 0 / nullptr).
// ===================================================================

static void ResolveOffsetsFromPatterns(HookFunctionOffsets& offsets)
{
	ZeroMemory(&offsets, sizeof(offsets));
    // ---- Direct-address patterns (scan result IS the function) ----

    auto ScanDirect = [&](const std::string& pattern, DWORD& out)
    {
        if (pattern.empty()) return;
        if (auto* addr = Scanner::Scan(pattern))
            out = (DWORD)GetVirtualAddress((INT64)addr);
    };

    ScanDirect(SetFovPattern,                        offsets.SetFov);
    ScanDirect(SwitchInputDeviceToTouchScreenPattern, offsets.TouchInput);
    ScanDirect(SwitchInputDeviceToJoypadPattern,      offsets.JoypadInput);
    ScanDirect(SwitchInputDeviceToKeyboardPattern,    offsets.KeyboardMouseInput);
    ScanDirect(SetupQuestBannerPattern,                offsets.QuestBanner);
    ScanDirect(FindGameObjectPattern,                  offsets.FindObject);
    ScanDirect(SetUIDPattern,                          offsets.SetUid);
    ScanDirect(SetWaterMaskUIDPattern,                 offsets.SetWaterMaskUID);
    ScanDirect(SetupPlayerProfilePagePattern,          offsets.SetupPlayerProfilePage);
    ScanDirect(EventCameraMovePattern,                 offsets.CameraMove);
    ScanDirect(ShowOneDamageTextExPattern,             offsets.DamageText);
    ScanDirect(FindStringPattern,                      offsets.FindString);
    ScanDirect(CraftEntryPartnerPattern,               offsets.CombineEntryPartner);
    ScanDirect(CraftEntryPattern,                      offsets.CombineEntry);
    ScanDirect(CheckCanEnterPattern,                   offsets.CheckEnter);
    ScanDirect(OpenTeamPageAccordinglyPattern,         offsets.OpenTeamAdvanced);
    ScanDirect(OpenTeamPattern,                        offsets.OpenTeam);
    ScanDirect(GetNamePattern,                         offsets.GetName);
    ScanDirect(GameUpdatePattern,                      offsets.GameUpdate);
    ScanDirect(InLevelClockPageOkButtonClickedPattern,  offsets.InLevelClockPageOkButtonClicked);
    ScanDirect(InLevelClockPageCloseButtonClickedPattern, offsets.InLevelClockPageCloseButtonClicked);
	ScanDirect(GetComponentPattern,                     offsets.GetComponent);
	ScanDirect(AvatarPaimonAppearPattern,               offsets.AvatarPaimonAppear);
	ScanDirect(AvatarPaimonDisappearPattern,            offsets.AvatarPaimonDisappear);
	ScanDirect(PlayerPerspectivePattern,                offsets.PlayerPerspective);
	ScanDirect(SetTextPattern,                          offsets.SetText);
    offsets.PlayerDiveMosaic = ScanPlayerDiveMosaic();

    // ---- REL (relative-call) patterns ----
    // Scan finds a CALL (E8) instruction; ResolveRelative gives the target.

    auto ScanRel = [&](const std::string& pattern, DWORD& out)
    {
        if (pattern.empty()) return;
        if (auto* addr = Scanner::Scan(pattern))
            if (auto* target = Scanner::ResolveRelative(addr))
                out = (DWORD)GetVirtualAddress((INT64)target);
    };

    ScanRel(GetFrameCountPattern,      offsets.GetFps);
    ScanRel(SetFrameCountPattern,      offsets.SetFps);
    ScanRel(SetActivePattern,          offsets.ObjectActive);
    ScanRel(IsActivePattern,           offsets.IsObjectActive);
    ScanRel(DisplayFogPattern,         offsets.SetFog);
    //ScanRel(PlayerPerspectivePattern,  offsets.PlayerPerspective);
    ScanRel(SetupResinListPattern,     offsets.SetupResinList);
    ScanRel(CheckCanOpenMapPattern, offsets.CheckCanOpenMap);

    // ---- Derived data offsets (read from code at a fixed offset) ----

    // ClosePage = *(int32_t*)(ClosePageCallerPattern_result + 0x2A)
    if (!ClosePageCallerPattern.empty())
    {
        if (auto* addr = Scanner::Scan(ClosePageCallerPattern))
            offsets.ClosePage = Scanner::ReadFieldOffset(addr, 0x2A);
    }

    if (!FastSkipTalkPattern.empty())
    {
        if (auto* funcAddr = Scanner::Scan(FastSkipTalkPattern))
        {
            if (auto* cooldownJump = FindDialogueCooldownJump(funcAddr))
            {
                offsets.FastSkipTalk = (DWORD)GetVirtualAddress((INT64)cooldownJump);
            }
            else
            {
                Log("FastSkipTalk: cooldown jump not found in function - feature disabled");
            }
        }
    }

    if (offsets.SetupResinList != 0)
    {
        auto* funcAddr = GetFunctionAddress(offsets.SetupResinList);
        if (funcAddr)
        {
            offsets.ResinList = FindResinListFieldOffset(funcAddr);
            if (offsets.ResinList == 0)
            {
                Log("ResinList: field offset not found in SetupResinList - resin filtering disabled");
            }
        }
    }
    // If SetupResinList couldn't be resolved, ResinList stays as fallback.
}

// ===================================================================
// 凯瑟琳重定向用到的偏移
//
// 一条特征码同时适配国服/国际服（同版本）；换版本要重新生成，
// 方法见 docs/catherine-redirect-version-guide.md。
// 这四个字段是后加的，外部 Provider 通常不填；所以无论 ProvideOffsets 与否，
// 这里都补扫一遍（只补 0 值的，已有就跳过）。
// ===================================================================
static void ResolveCatherineOffsetsFromPatterns(HookFunctionOffsets& offsets)
{
	auto ScanIfMissing = [&](const std::string& pattern, DWORD& out)
	{
		if (out != 0 || pattern.empty())
		{
			return;
		}

		if (auto* addr = Scanner::Scan(pattern))
		{
			out = (DWORD)GetVirtualAddress((INT64)addr);
		}
	};

	ScanIfMissing(CatherineEntryPattern,      offsets.CatherineEntry);
	ScanIfMissing(CatherineOpenDialogPattern, offsets.CatherineOpenDialog);

	ScanIfMissing(CatherineDialogCtorPattern,         offsets.CatherineDialogCtor);
	ScanIfMissing(CatherineDialogCtorPatternOversea,  offsets.CatherineDialogCtor);
	ScanIfMissing(ExplorationEntryPattern,            offsets.ExplorationEntry);
	ScanIfMissing(ExplorationEntryPatternOversea,     offsets.ExplorationEntry);
}

// ===================================================================
// 旧版派蒙菜单用到的偏移
//
// SetupPaimonConfig 重建列表之前，会从游戏自己的静态缓存里取
// UIEntryConfig。7.2 的机器码形状：
//
//     mov rax, [rip+disp32]      ; 静态块
//     mov rax, [rax+disp32]      ; UIEntryConfig 实例
//     test rax, rax
//     je ...
//     mov r13, [rax+disp8]       ; entryConfigs
//
// 这三个偏移每个版本都不一样，所以扫描到函数后直接从机器码里解析；
// 换版本时只需要更新 SetupPaimonConfigPattern 这一条特征码。
// ===================================================================
static constexpr int kUIEntryConfigScanLimit = 0x600;
static constexpr int kUIEntryConfigAccessSize = 27;

// 访问序列里各条指令的偏移。
static constexpr int kStaticBlockMov = 0;
static constexpr int kStaticBlockDisp = 3;
static constexpr int kStaticBlockMovSize = 7;
static constexpr int kStaticFieldMov = 7;
static constexpr int kStaticFieldDisp = 10;
static constexpr int kTestRax = 14;
static constexpr int kJe = 17;
static constexpr int kEntryConfigsMov = 23;
static constexpr int kEntryConfigsDisp = 26;

// 模块镜像不会超过 1GB；用来过滤明显不成立的 RIP 目标。
static constexpr INT64 kMaxModuleRva = 0x40000000;

static bool MatchesUIEntryConfigAccess(const uint8_t* code)
{
	// mov rax, [rip+disp32]
	if (code[kStaticBlockMov] != 0x48 ||
		code[kStaticBlockMov + 1] != 0x8B ||
		code[kStaticBlockMov + 2] != 0x05)
	{
		return false;
	}

	// mov rax, [rax+disp32]
	if (code[kStaticFieldMov] != 0x48 ||
		code[kStaticFieldMov + 1] != 0x8B ||
		code[kStaticFieldMov + 2] != 0x80)
	{
		return false;
	}

	// test rax, rax
	if (code[kTestRax] != 0x48 ||
		code[kTestRax + 1] != 0x85 ||
		code[kTestRax + 2] != 0xC0)
	{
		return false;
	}

	// je rel32
	if (code[kJe] != 0x0F || code[kJe + 1] != 0x84)
	{
		return false;
	}

	// mov r8-r15, [rax+disp8]（7.2 是 mov r13, [rax+18h]）
	if (code[kEntryConfigsMov] != 0x4C ||
		code[kEntryConfigsMov + 1] != 0x8B ||
		(code[kEntryConfigsMov + 2] & 0xC7) != 0x40)
	{
		return false;
	}

	return true;
}

static bool TryResolveUIEntryConfigAccess(const void* function,
	DWORD& staticBlock,
	DWORD& staticField,
	DWORD& entryConfigs)
{
	if (function == nullptr || g_hModule == nullptr)
	{
		return false;
	}

	const auto* code = (const uint8_t*)function;

	__try
	{
		for (int offset = 0; offset + kUIEntryConfigAccessSize <= kUIEntryConfigScanLimit; ++offset)
		{
			const uint8_t* access = code + offset;
			if (!MatchesUIEntryConfigAccess(access))
			{
				continue;
			}

			void* block = Scanner::ResolveRelative(
				(void*)(access + kStaticBlockMov), kStaticBlockDisp, kStaticBlockMovSize);
			if (block == nullptr)
			{
				continue;
			}

			const auto rva = GetVirtualAddress((INT64)block);
			if (rva <= 0 || rva > kMaxModuleRva)
			{
				continue;
			}

			DWORD field = 0;
			memcpy(&field, access + kStaticFieldDisp, sizeof(field));
			if (field == 0)
			{
				continue;
			}

			staticBlock = (DWORD)rva;
			staticField = field;
			entryConfigs = access[kEntryConfigsDisp];
			return true;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return false;
}

static void ResolveLegacyPaimonMenuOffsetsFromPatterns(HookFunctionOffsets& offsets)
{
	if (offsets.SetupPaimonConfig == 0)
	{
		if (auto* addr = Scanner::Scan(SetupPaimonConfigPattern))
		{
			offsets.SetupPaimonConfig = (DWORD)GetVirtualAddress((INT64)addr);
		}
	}

	if (offsets.SetupPaimonConfig == 0)
	{
		return;
	}

	if (offsets.UIEntryConfigStaticBlock != 0 &&
		offsets.UIEntryConfigStaticField != 0 &&
		offsets.UIEntryConfigEntryConfigs != 0)
	{
		return;
	}

	auto* function = (const void*)GetFunctionAddress(offsets.SetupPaimonConfig);
	DWORD staticBlock = 0;
	DWORD staticField = 0;
	DWORD entryConfigs = 0;

	if (!TryResolveUIEntryConfigAccess(function, staticBlock, staticField, entryConfigs))
	{
		Log("LegacyPaimonMenu: UIEntryConfig access not found in SetupPaimonConfig - feature disabled");
		offsets.SetupPaimonConfig = 0;
		return;
	}

	offsets.UIEntryConfigStaticBlock = staticBlock;
	offsets.UIEntryConfigStaticField = staticField;
	offsets.UIEntryConfigEntryConfigs = entryConfigs;
}


// ===================================================================
// Master SetFov hook — also serves as the main per-frame dispatch
// ===================================================================
static int MasterHookSetFov(void* a1, float changeFovValue)
{
	if (!gameUpdateInit)
	{
		gameUpdateInit = true;
	}

	DispatchUpdate();

	// FOV override (must be done here because it modifies the parameter)
	if (changeFovValue > 30.0f && g_pEnv->EnableSetFov && !CheckResistInBeyd())
	{
		changeFovValue = g_pEnv->FieldOfView;
	}

	if (originalSetFov)
	{
		SetFovFn original = (SetFovFn)originalSetFov;
		return original(a1, changeFovValue);
	}
	return 0;
}

// ===================================================================
// GameUpdate hook — Task ticking + resist monitoring
// ===================================================================
static void HookGameUpdate(void* pThis)
{
	bool isResisted = CheckResistInBeyd();

	isResistedLastFrame = isResisted;

	if (!GetUnityMainWindow())
	{
		InitializeWndProcHooks();
	}

	Task::Tick();

	if (originalGameUpdate)
	{
		UpdateFn original = (UpdateFn)originalGameUpdate;
		original(pThis);
	}
}

// ===================================================================
// SetUID hook — capture UID from game
// ===================================================================
static void HookSetUID(void* pThis, uint32_t uid)
{
	g_pEnv->Uid = uid;

	if (originalSetUID)
	{
		SetUidFn original = (SetUidFn)originalSetUID;
		original(pThis, uid);
	}
}

// ===================================================================
// SetWaterMaskUID hook — string-driven resist detection
// ===================================================================
static void HookSetWaterMaskUID(void* pThis, Il2CppString* text, bool flag)
{
	// Run even when text is null: exiting 千星奇域 may clear the watermark
	// UID (null), which must also lift the resist state.
	CacheResistState(text);

	if (originalSetWaterMaskUID)
	{
		SetWaterMaskUIDFn original = (SetWaterMaskUIDFn)originalSetWaterMaskUID;
		original(pThis, text, flag);
	}

	// Hide the watermark UID object after it has been (re)set.
	HidePlayerInfo::HideUidWatermark();
}

// ===================================================================
// Public API
// ===================================================================
void RequestOpenCraft()
{
	requestOpenCraft = true;
}

// ===================================================================
// SetupHooks — create all IFunction instances, initialize, set up hooks
// ===================================================================
void SetupHooks()
{
	// Choose which offsets to use based on ProvideOffsets flag
	HookFunctionOffsets* offsets = &g_pEnv->Offsets;
	if (!g_pEnv->ProvideOffsets)
	{
		ResolveOffsetsFromPatterns(*offsets);
	}

	ResolveCatherineOffsetsFromPatterns(*offsets);
	ResolveLegacyPaimonMenuOffsetsFromPatterns(*offsets);

	g_pEnv->Offsets = *offsets;

	// Create and register all IFunction instances
	// Order does not matter; each reads from g_pEnv->Offsets in Initialize()
	g_functions.push_back(new FovOverride());
	g_functions.push_back(new DisablePlayerPerspective());
	g_functions.push_back(new DisablePlayerDiveMosaic());
	g_functions.push_back(new DisableFog());
	g_functions.push_back(new EnableSetFps());
	g_functions.push_back(new RemoveTeamProgress());
	g_functions.push_back(new HideQuestBanner());
	g_functions.push_back(new DisableCameraMove());
	g_functions.push_back(new DisableDamageText());
	g_functions.push_back(new TouchMode());
	g_functions.push_back(new ResinItem());
	g_functions.push_back(new DisplayPaimon());
	g_functions.push_back(new HidePlayerInfo());
	g_functions.push_back(new HideGrass());
	g_functions.push_back(new GamepadHotSwitchFunc());
	g_functions.push_back(new InLevelClockPageSpeedUp());
	g_functions.push_back(new CombineHotkey());
	g_functions.push_back(new WeakMapCheck());
	g_functions.push_back(new FastSkipTalk());
	g_functions.push_back(new CatherineRedirect());
	g_functions.push_back(new LegacyPaimonMenu());

	// Initialize all functions (resolves offsets, creates MinHook hooks)
	for (auto* func : g_functions)
	{
		func->Initialize();
	}

	// Set up the master SetFov dispatch hook
	if (offsets->SetFov)
	{
		LPVOID changeFovAddr = GetFunctionAddress(offsets->SetFov);
		if (changeFovAddr)
		{
			MH_CreateHook(changeFovAddr, MasterHookSetFov, &originalSetFov);
		}
	}

	// Set up the SetUID hook
	if (offsets->SetUid)
	{
		LPVOID setUIDAddr = GetFunctionAddress(offsets->SetUid);
		if (setUIDAddr)
		{
			MH_CreateHook(setUIDAddr, HookSetUID, &originalSetUID);
		}
	}

	// Set up the GameUpdate hook
	if (offsets->GameUpdate)
	{
		LPVOID gameUpdateAddr = GetFunctionAddress(offsets->GameUpdate);
		if (gameUpdateAddr)
		{
			MH_CreateHook(gameUpdateAddr, HookGameUpdate, &originalGameUpdate);
		}
	}

	// Set up the SetWaterMaskUID hook (string-driven resist detection)
	if (offsets->SetWaterMaskUID)
	{
		LPVOID setWaterMaskUIDAddr = GetFunctionAddress(offsets->SetWaterMaskUID);
		if (setWaterMaskUIDAddr)
		{
			MH_CreateHook(setWaterMaskUIDAddr, HookSetWaterMaskUID, &originalSetWaterMaskUID);
		}
	}
}
