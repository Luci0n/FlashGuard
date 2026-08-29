#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <d3d11.h>
#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>
#include <dxgi1_5.h>
#include <d3dcompiler.h>
#include <winrt/base.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <filesystem>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "motion/Nvof5.h"

namespace
{
    constexpr wchar_t kWindowClass[] = L"OutlastFlashGuardWindow";
    constexpr UINT_PTR kWatchdogTimer = 1;
    constexpr UINT kWatchdogPeriodMs = 33;
    constexpr int64_t kDefaultStaleFrameMs = 1200;
    constexpr ULONGLONG kStartupCaptureTimeoutMs = 5000;
    constexpr int64_t kAutomaticShieldFaultDelayMs = 750;
    constexpr UINT kAnalysisWidth = 128;
    constexpr UINT kAnalysisHeight = 72;
    constexpr UINT kAnalysisReadbackCount = 12;
    constexpr UINT kDebugWidth = 560;
    constexpr UINT kDebugHeight = 640;
    constexpr UINT kHintWidth = 900;
    constexpr UINT kHintHeight = 48;
    constexpr UINT kAutomaticShieldLabelWidth = 520;
    constexpr UINT kAutomaticShieldLabelHeight = 64;
    constexpr wchar_t kSettingsWindowClass[] = L"OutlastFlashGuardSettings";
    constexpr int kControlContrast = 2001;
    constexpr int kControlContrastValue = 2015;
    constexpr int kControlSensitivity = 2002;
    constexpr int kControlLatency = 2003;
    constexpr int kControlDebug = 2004;
    constexpr int kControlSmallSensitivity = 2005;
    constexpr int kControlDisplaySize = 2006;
    constexpr int kControlViewingDistance = 2008;
    constexpr int kControlProfile = 2009;
    constexpr int kControlApply = IDOK;
    constexpr int kControlClose = IDCANCEL;
    constexpr int kControlStatus = 2007;
    constexpr int kControlShieldOnHotkey = 2010;
    constexpr int kControlShieldOffHotkey = 2011;
    constexpr int kControlDebugHotkey = 2012;
    constexpr int kControlOptionsHotkey = 2013;
    constexpr int kControlExitHotkey = 2014;
    constexpr ULONGLONG kShieldToggleCooldownMs = 1000;
#ifndef WDA_EXCLUDEFROMCAPTURE
    constexpr DWORD WDA_EXCLUDEFROMCAPTURE = 0x00000011;
#endif

    struct SafetySettings
    {
        // The ring is time-gated, so the requested look-ahead remains stable when
        // the capture rate changes. Newer analyzed frames classify the oldest one.
        int lookaheadMs = 0;

        float localDeltaThreshold = 0.10f;
        float globalDeltaThreshold = 0.16f;
        float affectedAreaThreshold = 0.18f;
        float strongAffectedArea = 0.30f;
        float globalAreaThreshold = 0.90f;
        float coherenceThreshold = 0.70f;
        float visualFieldAreaThreshold = 0.25f;
        float patternScoreThreshold = 0.24f;
        float cameraMotionSuppression = 0.32f;
        float flashEnergyThreshold = 0.030f;

        // Small intense sources (lamps, muzzle flashes, lightning apertures) can
        // illuminate nearby room surfaces even when they occupy little area.
        float smallFlashAreaThreshold = 0.008f;
        float smallFlashDeltaThreshold = 0.25f;
        float smallFlashCoherenceThreshold = 0.85f;
        float spillDeltaThreshold = 0.035f;
        int spillExpansionCells = 4;
        float localGlobalSupportThreshold = 0.035f;

        // A 0.8 full-screen transition takes about 0.5--0.7 seconds initially.
        float safeRiseRate = 1.35f;
        float safeFallRate = 1.60f;
        float minimumProtectionTime = 0.22f;
        float releaseTime = 0.45f;

        float redThreshold = 0.55f;
        float redDeltaThreshold = 0.18f;
        float redAffectedAreaThreshold = 0.15f;
        float redDesaturation = 0.68f;

        float displayDiagonalInches = 27.0f;
        float viewingDistanceCm = 70.0f;
        float overloadWhiteCeiling = 0.72f;

        // Optional static range compression. Disabled for a clean normal image.
        bool subtleToneMap = true;
        float blackFloor = 0.08f;
        float whiteCeiling = 0.84f;

        bool debugOverlay = false;
    };

    struct ShaderConstants
    {
        float p0[4];
        float p1[4];
        float p2[4];
        float p3[4];
        float p4[4];
        float p5[4];
        float p6[4];
        float p7[4];
        float p8[4];
        float p9[4];
        float p10[4];
        float p11[4];
        float p12[4];
        float p13[4];
        float p14[4];
        float p15[4];
        float p16[4];
    };
    static_assert(sizeof(ShaderConstants) % 16 == 0);

    enum class HazardState
    {
        Safe,
        Suspect,
        Protecting,
        Releasing
    };

    struct AnalysisStats
    {
        float globalLuma = 0.0f;
        float globalDelta = 0.0f;
        float affectedArea = 0.0f;
        float brighteningArea = 0.0f;
        float darkeningArea = 0.0f;
        float directionalCoherence = 0.0f;
        float strongAffectedArea = 0.0f;
        float strongDirectionalCoherence = 0.0f;
        float redAffectedArea = 0.0f;
        float largestRegionArea = 0.0f;
        float visualFieldAffectedArea = 0.0f;
        float flashEnergy = 0.0f;
        float cameraMotionScore = 0.0f;
        float structuralCameraMotionScore = 0.0f;
        float patternScore = 0.0f;
        float dt = 1.0f / 60.0f;
        bool validDelta = false;
    };

    struct RuntimeOptions
    {
        int profilePreset = 1;
        float contrastReduction = 2.0f / 3.0f;
        int fullScreenSensitivity = 1;
        int smallSourceSensitivity = 1;
        int latencyMs = 0;
        int displaySizePreset = 1;
        int viewingDistancePreset = 1;
        bool debugOverlay = false;
    };

    // Full-resolution shader behavior has exact production defaults here. The
    // replay batch may override these values without recompiling or changing UI.
    struct ShaderTuningSettings
    {
        float eventDeltaLow = 0.008f;
        float eventDeltaHigh = 0.035f;
        float holdDeltaLow = 0.028f;
        float holdDeltaHigh = 0.085f;
        float stableSourceLow = 0.012f;
        float stableSourceHigh = 0.055f;
        float intrinsicResidualLow = 0.020f;
        float intrinsicResidualHigh = 0.090f;
        float repeatedMemoryLow = 0.34f;
        float repeatedMemoryHigh = 0.62f;
        float holdGateLow = 0.16f;
        float holdGateHigh = 0.58f;
        float transportConfidenceLow = 0.45f;
        float transportConfidenceHigh = 0.75f;
        float disocclusionResetGate = 0.55f;
        float surfaceRiskTau = 0.55f;
        float eventStateTauScale = 1.0f;
        float releaseStateTauScale = 1.0f;
        float exactHoldThreshold = 0.72f;
        float movingHoldFloorMax = 0.035f;
        float directIntrinsicDisplayLow = 0.010f;
        float directIntrinsicDisplayHigh = 0.040f;
        float eventSeedLow = 0.025f;
        float eventSeedHigh = 0.14f;
    };

    // Replay-batch-only overrides. Production/UI code never supplies these;
    // negative outer values preserve the exact RuntimeOptions-derived defaults.
    struct BenchmarkTuning
    {
        bool enabled = false;
        float localDeltaThreshold = -1.0f;
        float globalDeltaThreshold = -1.0f;
        float affectedAreaThreshold = -1.0f;
        float coherenceThreshold = -1.0f;
        float smallFlashAreaThreshold = -1.0f;
        float localGlobalSupportThreshold = -1.0f;
        float flashEnergyThreshold = -1.0f;
        float safeRiseRate = -1.0f;
        float safeFallRate = -1.0f;
        float minimumProtectionTime = -1.0f;
        float releaseTime = -1.0f;
        float cameraMotionSuppression = -1.0f;
        int architectureMode = -1;
        float riskOnlyNeutralLuma = -1.0f;
        float riskOnlyGain = -1.0f;
        ShaderTuningSettings shader{};
    };

    struct HotkeyBinding
    {
        UINT modifiers = 0;
        UINT virtualKey = 0;
    };

    // Indexed by RegisterHotKey id - 1: shield toggle, unused, exit,
    // diagnostics, settings.
    std::array<HotkeyBinding, 5> g_hotkeys{
        HotkeyBinding{ 0, VK_F8 },
        HotkeyBinding{ 0, 0 },
        HotkeyBinding{ MOD_CONTROL | MOD_SHIFT, VK_F12 },
        HotkeyBinding{ 0, VK_F9 },
        HotkeyBinding{ 0, VK_F10 }
    };
    std::atomic<ULONGLONG> g_lastShieldToggleMs{ 0 };
    // Replay-only diagnostic switch used by Matrix 39. Production leaves this false.
    bool g_replayDisableNvofTemporalHints = false;
    // Live-only A/B switch for latency diagnosis. Replay behavior never uses it.
    bool g_liveDisableNvofForLatencyTest = false;
    // Strong live-only A/B: copy the newest Desktop Duplication image directly
    // to the output swapchain. This intentionally disables flash protection.
    bool g_liveRawPassthroughForLatencyTest = false;

    struct WindowSearch
    {
        std::wstring needle;
        HWND found = nullptr;
    };

    std::wstring Lower(std::wstring s)
    {
        for (auto& c : s) c = static_cast<wchar_t>(towlower(c));
        return s;
    }

    std::wstring HotkeyName(const HotkeyBinding& binding)
    {
        if (binding.virtualKey == 0) return L"Unassigned";
        std::wstring result;
        if (binding.modifiers & MOD_CONTROL) result += L"Ctrl+";
        if (binding.modifiers & MOD_ALT) result += L"Alt+";
        if (binding.modifiers & MOD_SHIFT) result += L"Shift+";
        if (binding.modifiers & MOD_WIN) result += L"Win+";

        wchar_t keyName[64]{};
        const UINT scan = MapVirtualKeyW(binding.virtualKey, MAPVK_VK_TO_VSC);
        LONG keyParam = static_cast<LONG>(scan << 16);
        if (binding.virtualKey == VK_INSERT || binding.virtualKey == VK_DELETE ||
            binding.virtualKey == VK_HOME || binding.virtualKey == VK_END ||
            binding.virtualKey == VK_PRIOR || binding.virtualKey == VK_NEXT ||
            binding.virtualKey == VK_LEFT || binding.virtualKey == VK_RIGHT ||
            binding.virtualKey == VK_UP || binding.virtualKey == VK_DOWN)
            keyParam |= 1 << 24;
        if (GetKeyNameTextW(keyParam, keyName, static_cast<int>(std::size(keyName))) > 0)
            result += keyName;
        else
            result += L"Key " + std::to_wstring(binding.virtualKey);
        return result;
    }

    WORD ToHotkeyControlValue(const HotkeyBinding& binding)
    {
        BYTE flags = 0;
        if (binding.modifiers & MOD_SHIFT) flags |= HOTKEYF_SHIFT;
        if (binding.modifiers & MOD_CONTROL) flags |= HOTKEYF_CONTROL;
        if (binding.modifiers & MOD_ALT) flags |= HOTKEYF_ALT;
        return MAKEWORD(static_cast<BYTE>(binding.virtualKey), flags);
    }

    HotkeyBinding FromHotkeyControlValue(WORD value)
    {
        HotkeyBinding binding{};
        binding.virtualKey = LOBYTE(value);
        const BYTE flags = HIBYTE(value);
        if (flags & HOTKEYF_SHIFT) binding.modifiers |= MOD_SHIFT;
        if (flags & HOTKEYF_CONTROL) binding.modifiers |= MOD_CONTROL;
        if (flags & HOTKEYF_ALT) binding.modifiers |= MOD_ALT;
        return binding;
    }

    std::wstring PreferencesPath()
    {
        wchar_t localAppData[MAX_PATH]{};
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
            nullptr, SHGFP_TYPE_CURRENT, localAppData)))
            return L"FlashGuard.ini";
        std::wstring directory = std::wstring(localAppData) + L"\\OutlastFlashGuard";
        CreateDirectoryW(directory.c_str(), nullptr);
        return directory + L"\\settings.ini";
    }

    int ReadPreference(const std::wstring& path, const wchar_t* key, int fallback)
    {
        return static_cast<int>(GetPrivateProfileIntW(L"FlashGuard", key,
            fallback, path.c_str()));
    }

    void LoadPreferences(RuntimeOptions& options, std::array<HotkeyBinding, 5>& hotkeys)
    {
        const std::wstring path = PreferencesPath();
        const int version = ReadPreference(path, L"Version", 1);
        options.profilePreset = std::clamp(ReadPreference(path, L"Profile", 1), 0, 2);
        options.contrastReduction = version >= 4 ?
            std::clamp(ReadPreference(path, L"ContrastAmount", 667) / 1000.0f, 0.0f, 1.0f) :
            std::clamp(ReadPreference(path, L"Contrast", 2), 0, 3) / 3.0f;
        options.fullScreenSensitivity = std::clamp(ReadPreference(path, L"FullSensitivity", 1), 0, 2);
        options.smallSourceSensitivity = std::clamp(ReadPreference(path, L"SmallSensitivity", 1), 0, 2);
        options.latencyMs = version < 2 ? 0 : ReadPreference(path, L"LookaheadMs", 0);
        // Version 3 makes the instant GPU path with reduced contrast the
        // practical default, while preserving choices saved by newer builds.
        if (version < 3)
        {
            options.profilePreset = 1;
            options.contrastReduction = 2.0f / 3.0f;
            options.fullScreenSensitivity = 1;
            options.smallSourceSensitivity = 1;
            options.latencyMs = 0;
        }
        if (options.latencyMs != 0 && options.latencyMs != 25 && options.latencyMs != 33 &&
            options.latencyMs != 50 && options.latencyMs != 67 && options.latencyMs != 100)
            options.latencyMs = 0;
        options.displaySizePreset = std::clamp(ReadPreference(path, L"DisplaySize", 1), 0, 3);
        options.viewingDistancePreset = std::clamp(ReadPreference(path, L"ViewingDistance", 1), 0, 3);
        options.debugOverlay = ReadPreference(path, L"Debug", 0) != 0;
        for (size_t i = 0; i < hotkeys.size(); ++i)
        {
            const std::wstring vkKey = L"Hotkey" + std::to_wstring(i) + L"Vk";
            const std::wstring modKey = L"Hotkey" + std::to_wstring(i) + L"Modifiers";
            hotkeys[i].virtualKey = static_cast<UINT>(std::clamp(
                ReadPreference(path, vkKey.c_str(), static_cast<int>(hotkeys[i].virtualKey)), 0, 255));
            hotkeys[i].modifiers = static_cast<UINT>(ReadPreference(path, modKey.c_str(),
                static_cast<int>(hotkeys[i].modifiers))) &
                (MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN);
        }
        // The first binding is one toggle for both states. Discard only the
        // legacy separate resume binding so old settings cannot revive it.
        hotkeys[1] = HotkeyBinding{};
    }

    void SavePreferences(const RuntimeOptions& options,
                         const std::array<HotkeyBinding, 5>& hotkeys)
    {
        const std::wstring path = PreferencesPath();
        const auto write = [&path](const wchar_t* key, int value) {
            const std::wstring text = std::to_wstring(value);
            WritePrivateProfileStringW(L"FlashGuard", key, text.c_str(), path.c_str());
        };
        write(L"Version", 4);
        write(L"Profile", options.profilePreset);
        write(L"ContrastAmount", static_cast<int>(std::lround(
            std::clamp(options.contrastReduction, 0.0f, 1.0f) * 1000.0f)));
        write(L"FullSensitivity", options.fullScreenSensitivity);
        write(L"SmallSensitivity", options.smallSourceSensitivity);
        write(L"LookaheadMs", options.latencyMs);
        write(L"DisplaySize", options.displaySizePreset);
        write(L"ViewingDistance", options.viewingDistancePreset);
        write(L"Debug", options.debugOverlay ? 1 : 0);
        for (size_t i = 0; i < hotkeys.size(); ++i)
        {
            const std::wstring vkKey = L"Hotkey" + std::to_wstring(i) + L"Vk";
            const std::wstring modKey = L"Hotkey" + std::to_wstring(i) + L"Modifiers";
            write(vkKey.c_str(), static_cast<int>(hotkeys[i].virtualKey));
            write(modKey.c_str(), static_cast<int>(hotkeys[i].modifiers));
        }
    }

    BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM param)
    {
        auto* search = reinterpret_cast<WindowSearch*>(param);
        if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

        wchar_t title[512]{};
        int len = GetWindowTextW(hwnd, title, static_cast<int>(std::size(title)));
        if (len <= 0) return TRUE;

        auto haystack = Lower(std::wstring(title, title + len));
        if (haystack.find(search->needle) != std::wstring::npos)
        {
            search->found = hwnd;
            return FALSE;
        }
        return TRUE;
    }

    HWND FindWindowBySubstring(const std::wstring& titleSubstring)
    {
        WindowSearch search{ Lower(titleSubstring), nullptr };
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&search));
        return search.found;
    }

    constexpr char kShaderSource[] = R"HLSL(
Texture2D CurrentFrame  : register(t0);
Texture2D DebugOverlay  : register(t1);
Texture2D LocalSafetyMap : register(t2);
Texture2D HotkeyHint : register(t3);
Texture2D CurrentAnalysis : register(t4);
Texture2D PreviousAnalysis : register(t5);
Texture2D PreviousSafety : register(t6);
Texture2D PreviousTemporal : register(t7);
Texture2D PreviousOutput : register(t8);
Texture2D PreviousSource : register(t9);
Texture2D<int2> ForwardOpticalFlow : register(t10);
Texture2D<int2> BackwardOpticalFlow : register(t11);
Texture2D<uint> ForwardOpticalCost : register(t12);
Texture2D<uint> BackwardOpticalCost : register(t13);
Texture2D<int2> GlobalOpticalFlow : register(t14);
Texture2D PreviousProtectionState : register(t15);
SamplerState LinearClamp : register(s0);

cbuffer Safety : register(b0)
{
    float4 P0;
    float4 P1;
    float4 P2;
    float4 P3;
    float4 P4;
    float4 P5;
    float4 P6;
    float4 P7;
    float4 P8;
    float4 P9; // x = idle release tick (no new desktop event)
    float4 P10; // event/hold displayed-delta gates
    float4 P11; // source-stability / intrinsic residual gates
    float4 P12; // repeated-memory / hold-memory gates
    float4 P13; // transport confidence / disocclusion / risk decay
    float4 P14; // temporal-state tau scales / exact hold / moving floor
    float4 P15; // direct-intrinsic display / event-seed gates
    float4 P16; // x = benchmark architecture, y/z = risk params, w = live safe fast path
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOut VSMain(uint id : SV_VertexID)
{
    VSOut o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.uv = uv;
    o.pos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    return o;
}

float3 SrgbToLinear(float3 c)
{
    return lerp(c / 12.92, pow((c + 0.055) / 1.055, 2.4), step(0.04045, c));
}

float3 LinearToSrgb(float3 c)
{
    c = max(c, 0.0);
    return lerp(c * 12.92, 1.055 * pow(c, 1.0 / 2.4) - 0.055, step(0.0031308, c));
}

float Luma(float3 srgb)
{
    return dot(SrgbToLinear(srgb), float3(0.2126, 0.7152, 0.0722));
}

// Compare unfiltered source pixels for motion transport. Luminance dominates,
// while a smaller RGB term prevents an unrelated same-luma surface from being
// accepted as the moving object.
float SourceMatchError(float3 a, float3 b)
{
    const float3 al = SrgbToLinear(a);
    const float3 bl = SrgbToLinear(b);
    const float la = dot(al, float3(0.2126, 0.7152, 0.0722));
    const float lb = dot(bl, float3(0.2126, 0.7152, 0.0722));
    return abs(la - lb) + length(al - bl) * 0.18;
}

float3 RemapGlobalLuminance(float3 c, float rawMean, float permittedMean)
{
    rawMean = saturate(rawMean);
    permittedMean = saturate(permittedMean);
    float3 linearColor = SrgbToLinear(c);
    float l = dot(linearColor, float3(0.2126, 0.7152, 0.0722));
    float mappedL = l;
    float chromaScale = 1.0;

    if (permittedMean < rawMean - 0.0005)
    {
        float gain = permittedMean / max(rawMean, 0.004);
        mappedL = l * gain;
        chromaScale = gain;
    }
    else if (permittedMean > rawMean + 0.0005)
    {
        float whiteScale = (1.0 - permittedMean) / max(1.0 - rawMean, 0.004);
        mappedL = 1.0 - (1.0 - l) * whiteScale;
        chromaScale = whiteScale;
    }
    float3 chroma = linearColor - l.xxx;
    return saturate(LinearToSrgb(mappedL.xxx + chroma * chromaScale));
}

float AnalysisLuma(float3 linearColor);

float4 PSOpticalFlowCopy(VSOut i) : SV_TARGET
{
    return float4(CurrentFrame.SampleLevel(LinearClamp, i.uv, 0.0).rgb, 1.0);
}

float2 LoadOpticalFlow(Texture2D<int2> flowTexture, float2 uv)
{
    const int flowWidth = max((int)P8.z, 1);
    const int flowHeight = max((int)P8.w, 1);
    const float2 p = saturate(uv) * float2(flowWidth, flowHeight) - 0.5;
    // Preserve motion discontinuities. Bilinear interpolation invents vectors at
    // object/background boundaries (for example 0 px and 16 px becoming 8 px),
    // which then fail transport verification and re-enable temporal history.
    // Grid-1 NVOFA is already dense at the OF input resolution; nearest-vector
    // reconstruction is intentionally conservative at full-resolution edges.
    const int2 ip = clamp(int2(floor(p + 0.5)), int2(0, 0),
        int2(flowWidth - 1, flowHeight - 1));
    const float2 flow = (float2)flowTexture.Load(int3(ip, 0)) / 32.0;
    const float2 flowInputToOutputScale = max(P9.yz, float2(1.0, 1.0));
    return flow * flowInputToOutputScale;
}

float2 LoadGlobalOpticalFlow()
{
    // Global flow is a single forward vector generated from the dominant
    // background/camera transport. An unbound SRV reads zero, so this also
    // behaves safely on drivers where global flow is unavailable.
    const float2 flow = (float2)GlobalOpticalFlow.Load(int3(0, 0, 0)) / 32.0;
    const float2 flowInputToOutputScale = max(P9.yz, float2(1.0, 1.0));
    return flow * flowInputToOutputScale;
}

float LoadOpticalCost(Texture2D<uint> costTexture, float2 uv)
{
    const int costWidth = max((int)P8.z, 1);
    const int costHeight = max((int)P8.w, 1);
    const float2 p = saturate(uv) * float2(costWidth, costHeight) - 0.5;
    const int2 ip = clamp(int2(floor(p + 0.5)), int2(0, 0),
        int2(costWidth - 1, costHeight - 1));
    // NVOFA UINT8 cost: lower is better, higher is less trustworthy.
    return saturate((float)costTexture.Load(int3(ip, 0)) / 255.0);
}

struct MainOutput
{
    float4 color : SV_TARGET0;
    float4 historyColor : SV_TARGET1;
    float4 sourceHistoryColor : SV_TARGET2;
    float4 protectionState : SV_TARGET3;
    float4 motionDiagnostics0 : SV_TARGET4;
    float4 motionDiagnostics1 : SV_TARGET5;
    float4 motionDiagnostics2 : SV_TARGET6;
};

MainOutput PSMain(VSOut i)
{
    MainOutput output;
    output.protectionState = float4(0.0, 0.0, 0.0, 0.0);
    output.motionDiagnostics0 = float4(0.0, 0.0, 0.0, 0.0);
    output.motionDiagnostics1 = float4(0.0, 0.0, 0.0, 0.0);
    output.motionDiagnostics2 = float4(0.0, 0.0, 0.0, 0.0);
    float3 cur = CurrentFrame.Sample(LinearClamp, i.uv).rgb;
    // Keep a raw, unfiltered source history specifically for motion matching.
    // Candidate/output histories may intentionally lag during protection, so
    // they are the wrong images to use for optical transport.
    const float3 rawSourceColor = cur;
    const float rawMean = P0.x;
    const float permittedMean = P0.y;
    const float protectionSignal = P0.z;
    const float protectionGate = saturate(abs(protectionSignal));
    const float redGate = saturate(P0.w);
    const float redThreshold = saturate(P1.x);
    const float redDesat = saturate(P1.y);
    const float toneMapEnabled = P1.z;
    const float floorL = saturate(P1.w);
    const float ceilL = max(floorL + 0.05, saturate(P2.x));
    const float overloadGate = P3.y;
    const float overloadCeiling = saturate(P3.z);
    const float dt = max(P5.w, 1.0 / 240.0);

    // Global protection stays as a coarse fail-safe. The per-pixel temporal
    // limiter below is the final authority over displayed frame-to-frame luma.
    if (protectionGate > 0.0)
    {
        const float softPermittedMean = protectionSignal < 0.0 ? permittedMean :
            min(permittedMean, rawMean);
        cur = lerp(cur, RemapGlobalLuminance(cur, rawMean, softPermittedMean), protectionGate);
    }

    // Static range compression is part of the candidate image BEFORE temporal
    // feedback, so history and current luma are measured in the same domain.
    if (toneMapEnabled > 0.5)
    {
        float l = Luma(cur);
        float linearFloor = SrgbToLinear(floorL.xxx).x;
        float linearCeil = SrgbToLinear(ceilL.xxx).x;
        float mappedL = linearFloor + (linearCeil - linearFloor) * l;
        cur = RemapGlobalLuminance(cur, l, mappedL);
    }

    if (overloadGate > 0.5)
    {
        float l = Luma(cur);
        float ceilingL = SrgbToLinear(overloadCeiling.xxx).x;
        const float floorLuma = SrgbToLinear(float3(0.12, 0.12, 0.12)).x;
        float mappedL = clamp(l, floorLuma, ceilingL);
        cur = RemapGlobalLuminance(cur, l, mappedL);
    }

    // Coarse analysis decides whether temporal limiting is allowed at this
    // location. A tiny 3x3 max dilation closes analyzer-cell seams; it does not
    // define the visible shape. Full-resolution output delta does that below.
    uint safetyWidth = 0;
    uint safetyHeight = 0;
    LocalSafetyMap.GetDimensions(safetyWidth, safetyHeight);
    const float2 safetyTexel = 1.0 / max(float2(safetyWidth, safetyHeight), float2(1.0, 1.0));
    float coarseRisk = 0.0;
    float coarseEvent = 0.0;
    float coarseMotion = 0.0;
    float localRedGate = 0.0;
    [unroll]
    for (int sy = -1; sy <= 1; ++sy)
    {
        [unroll]
        for (int sx = -1; sx <= 1; ++sx)
        {
            const float2 uv = i.uv + float2((float)sx, (float)sy) * safetyTexel;
            const float4 safety = LocalSafetyMap.SampleLevel(LinearClamp, uv, 0.0);
            const float spatial = (sx == 0 && sy == 0) ? 1.0 :
                ((sx == 0 || sy == 0) ? 0.88 : 0.72);
            coarseRisk = max(coarseRisk, saturate(safety.b * spatial));
            // In the zero-latency GPU path, G is signed CURRENT state:
            // positive = a hazardous transition is happening now, negative =
            // coherent translation/motion. B remains accumulated flash memory.
            // Delayed/CPU local maps retain their original G=luminance meaning.
            if (P7.x > 0.5)
            {
                coarseEvent = max(coarseEvent, saturate(max(safety.g, 0.0) * spatial));
                coarseMotion = max(coarseMotion, saturate(max(-safety.g, 0.0) * spatial));
            }
            else
            {
                coarseEvent = max(coarseEvent, saturate(safety.b * spatial));
            }
            localRedGate = max(localRedGate, saturate(safety.a * spatial));
        }
    }

    // Red mitigation is also folded into the candidate image before temporal
    // feedback. The history texture therefore records exactly the luma the user
    // saw, excluding only our steady UI overlays.
    float gray = Luma(cur);
    float isolatedRed = saturate((cur.r - max(cur.g, cur.b) - redThreshold) /
                                 max(1.0 - redThreshold, 0.01));
    // A red flash event must remain desaturated through the hazard-memory
    // window. Event-only desaturation lets a 5-10 Hz high half-cycle become
    // saturated red again before the opposing transition, which still forms a
    // WCAG red-flash pair even when luminance is already temporally limited.
    // coarseRisk is persistent flash memory; isolatedRed ensures unrelated
    // non-red content is unaffected by this hold.
    const float redEventGate = max(redGate, localRedGate);
    float temporalRisk = 0.0;
    if (P7.x > 0.5)
        temporalRisk = saturate(PreviousTemporal.SampleLevel(
            LinearClamp, i.uv, 0.0).g);
    const float redMemoryGate = max(
        smoothstep(0.06, 0.42, coarseRisk),
        smoothstep(0.06, 0.42, temporalRisk));
    const float redMitigationGate = max(redEventGate, redMemoryGate);
    float finalRedSafetyAuthority = redMitigationGate;
    // The normal profile desaturation is intentionally moderate, but once a red
    // transition is part of an accumulated flash sequence the residual chroma
    // itself can keep forming WCAG saturated-red pairs. Ramp only the hazardous
    // red component toward full desaturation as flash memory becomes confident.
    const float repeatedRedDesat = smoothstep(0.18, 0.58, redMitigationGate);
    const float effectiveRedDesat =
        saturate(lerp(redDesat, 1.0, repeatedRedDesat));
    cur = lerp(cur, gray.xxx,
        isolatedRed * effectiveRedDesat * redMitigationGate);

    // Preserve the RAW source before temporal feedback. RGB is the unfiltered
    // appearance used for correspondence; alpha carries a short-lived geometry
    // confidence that can be transported with the surface on the next frame.
    const float3 sourceHistoryColor = rawSourceColor;
    float sourceHistoryGeometryConfidence = 0.0;)HLSL" R"HLSL(

    // Displayed history is observation/localization only. Recursive safety state
    // lives in a separate texture and follows a verified surface: protected
    // luminance plus short-lived intrinsic-risk and red-safety authority. Keeping
    // RGB out of that state prevents moving afterimages while preserving hazard
    // memory that the alpha.10 luminance-only experiment discarded.
    float candidateL = Luma(cur);
    float safetyHistoryL = candidateL;
    float protectionStateLuma = candidateL;
    float protectionStateRisk = 0.0;
    float protectionStateRedAuthority = 0.0;
    // Mode 8 reuses protection-state R as an encoded signed prime. Other modes
    // continue to store protected/display luminance there.
    float signedPrimeStateEncoded = 0.5;
    float phaseHoldStateEncoded = 1.0;
    float authorityPreprocessLumaDelta = saturate(abs(candidateL - Luma(rawSourceColor)));
    float authorityArchitectureLumaDelta = 0.0;
    float authorityCurrentEventStrength = 0.0;
    float authoritySurfaceMemoryStrength = 0.0;

    // Ordinary live pixels should not pay the full per-pixel motion/disocclusion
    // search once their transport is already proven. The 128x72 current-frame
    // safety map remains authoritative: any event/risk/red/global signal keeps
    // the complete protection path. This continuation path is disabled in replay
    // so Matrix evidence remains exactly on the fully instrumented shader path.
    if (P16.w > 0.5 && P7.y > 0.5 && P7.z > 0.5 &&
        P2.y < 0.5 && P3.x < 0.5 && P3.w < 0.5 &&
        protectionGate < 0.001 && overloadGate < 0.5 && P6.x < 0.5 &&
        coarseEvent < 0.015 && coarseRisk < 0.015 &&
        redMitigationGate < 0.015 && temporalRisk < 0.015)
    {
        const float4 previousSourceSameFast = PreviousSource.SampleLevel(
            LinearClamp, i.uv, 0.0);
        const float staticResidual = SourceMatchError(
            rawSourceColor, previousSourceSameFast.rgb);
        float fastResidual = staticResidual;
        float2 fastPreviousUv = i.uv;
        float fastTransportConfidence = 0.0;

        // A stationary unchanged pixel is intrinsically safe. For moving pixels,
        // reuse only an already-established geometry track and cheaply revalidate
        // it with current NVOFA forward/backward consistency, cost, and raw-source
        // appearance. A moving flash changes the compensated raw appearance and
        // therefore falls through to FullResolutionMotion on this same frame.
        if (P8.x > 0.75)
        {
            const float2 outputSize = max(
                float2(P2.z, P2.w), float2(1.0, 1.0));
            const float2 forwardPixels =
                LoadOpticalFlow(ForwardOpticalFlow, i.uv);
            const float flowMagnitude = length(forwardPixels);
            const float2 previousUv = i.uv + forwardPixels / outputSize;
            const bool insidePrevious =
                all(previousUv >= float2(0.0, 0.0)) &&
                all(previousUv <= float2(1.0, 1.0));
            if (insidePrevious && flowMagnitude > 0.10)
            {
                const float4 previousWarpedState = PreviousSource.SampleLevel(
                    LinearClamp, previousUv, 0.0);
                const float warpedResidual = SourceMatchError(
                    rawSourceColor, previousWarpedState.rgb);
                const float2 backwardPixels = LoadOpticalFlow(
                    BackwardOpticalFlow, previousUv);
                const float roundTripError =
                    length(forwardPixels + backwardPixels);
                const float allowedRoundTrip =
                    max(1.25, 0.65 + flowMagnitude * 0.22);
                const float fbConfidence = 1.0 - smoothstep(
                    allowedRoundTrip, allowedRoundTrip + 2.0,
                    roundTripError);
                float costConfidence = 1.0;
                if (P9.w > 0.5)
                    costConfidence = 1.0 - smoothstep(0.28, 0.78,
                        LoadOpticalCost(ForwardOpticalCost, i.uv));
                const float continuityConfidence = smoothstep(
                    0.30, 0.70, saturate(previousWarpedState.a));
                fastTransportConfidence =
                    fbConfidence * continuityConfidence *
                    lerp(0.35, 1.0, costConfidence);
                if (fastTransportConfidence > 0.0)
                {
                    fastResidual = warpedResidual;
                    fastPreviousUv = previousUv;
                }
            }
        }

        const float4 previousProtectionFast =
            PreviousProtectionState.SampleLevel(
                LinearClamp, fastPreviousUv, 0.0);
        const bool previousProtectionValidFast =
            previousProtectionFast.a >= 0.5;
        const float previousRiskFast = previousProtectionValidFast ?
            saturate(previousProtectionFast.g) : 0.0;
        const float previousRedFast = previousProtectionValidFast ?
            saturate(previousProtectionFast.b) : 0.0;
        const bool unchangedStatic = staticResidual < 0.004;
        const bool verifiedOrdinaryTransport =
            fastTransportConfidence > 0.62 && fastResidual < 0.010;

        if ((unchangedStatic || verifiedOrdinaryTransport) &&
            previousRiskFast < 0.015 && previousRedFast < 0.015)
        {
            const float fastL = Luma(cur);
            output.color = float4(saturate(cur), 1.0);
            output.historyColor = float4(saturate(cur), saturate(fastL));
            output.sourceHistoryColor = float4(
                saturate(sourceHistoryColor),
                verifiedOrdinaryTransport ?
                    saturate(fastTransportConfidence) : 0.0);
            output.protectionState = float4(
                saturate(fastL), 0.0, 0.0, 1.0);
            return output;
        }
    }

    if (P7.y > 0.5)
    {
)HLSL"
#include "shaders/FullResolutionMotion.inl"
R"HLSL(

        // Keep displayed history at the SAME screen coordinate. Optical flow is
        // used only to decide whether temporal feedback should be bypassed. This
        // removes visible geometry warping/rubber-sheet artifacts from imperfect
        // vectors while preserving the safety filter on genuinely static flashes.
        const float3 previousDisplayed = PreviousOutput.SampleLevel(
            LinearClamp, i.uv, 0.0).rgb;
        const float previousDisplayedL = Luma(previousDisplayed);
        const float displayedDelta = abs(candidateL - previousDisplayedL);

        // Use the exact previous UV selected by the strongest validated transport
        // path in FullResolutionMotion. A disocclusion belongs to a new surface,
        // so it starts from the current candidate and inherits no hazard memory.
        const float rawExplicitDisocclusionGate =
            saturate(max(diagVacatedGate, diagInfillGate));
        const float correctedCurrentPixelDisocclusionGate =
            rawExplicitDisocclusionGate *
                (1.0 - saturate(diagCurrentSurfaceGate));
        // Mode 13 corrects both event and history semantics. Modes 14/15 retain
        // raw disocclusion for history/motion bypass but use the corrected current-
        // pixel definition when deciding whether a current event may exist.
        const bool fullDisocclusionCorrection =
            P16.x > 12.5 && P16.x < 13.5;
        const bool eventDisocclusionCorrection = P16.x > 12.5;
        const float explicitDisocclusionGate = fullDisocclusionCorrection ?
            correctedCurrentPixelDisocclusionGate : rawExplicitDisocclusionGate;
        const float eventDisocclusionGateBase = eventDisocclusionCorrection ?
            correctedCurrentPixelDisocclusionGate : rawExplicitDisocclusionGate;
        const float4 previousProtectionState = PreviousProtectionState.SampleLevel(
            LinearClamp, protectionStatePreviousUv, 0.0);
        const bool previousProtectionValid = previousProtectionState.a >= 0.5;
        float previousProtectedL = previousProtectionValid ?
            saturate(previousProtectionState.r) : candidateL;
        float transportedSurfaceRisk = previousProtectionValid ?
            saturate(previousProtectionState.g) * exp(-dt / max(P13.w, 0.005)) : 0.0;
        float transportedRedAuthority = previousProtectionValid ?
            saturate(previousProtectionState.b) * exp(-dt / 0.20) : 0.0;
        if (explicitDisocclusionGate > P13.z)
        {
            previousProtectedL = candidateL;
            transportedSurfaceRisk = 0.0;
            transportedRedAuthority = 0.0;
        }
        // Architecture 3 validates red memory with the same surface-continuity
        // rule used for risk memory below. Earlier modes remain byte-equivalent.
        if (P16.x <= 2.5)
            finalRedSafetyAuthority = max(
                finalRedSafetyAuthority, transportedRedAuthority);

        // Validate a CURRENT luminance transition against optical transport before
        // treating it as a flash. A translating surface compares against its
        // motion-compensated raw predecessor; a verified disocclusion is already
        // explained by geometry. A moving object that truly changes luminance still
        // leaves a large compensated residual and therefore remains protectable.
        // Keep displayedDelta as a second localization bound so analyzer cells can
        // never create visible protection blocks where the output itself did not move.
        const float validatedEventDelta = P7.z > 0.5 ?
            min(displayedDelta, motionCompensatedSourceDelta) : displayedDelta;

        // A broad motion-gated transition, an active local analyzer cell, global
        // protection, or the overload fallback may authorize filtering. The
        // actual full-resolution luma delta localizes the surface, so analyzer
        // squares cannot become visible blocks/circles.
        const float broadRisk = (P9.x > 0.5) ? 0.0 : (P6.x > 0.5 ? 1.0 : 0.0);
        // CURRENT event confidence and accumulated flash memory are deliberately
        // separate. v6 let stale memory act like a fresh event, which caused
        // ordinary motion to keep blending against old displayed pixels.
        // On an idle release tick there was no new captured desktop event. The
        // last safety map may still contain a positive CURRENT-event bit, so ignore
        // it and use only accumulated memory to let the displayed output converge.
        const float eventSeed = (P9.x > 0.5) ? 0.0 :
            max(max(coarseEvent, broadRisk), max(protectionGate, overloadGate));
        const float holdSeed = max(max(coarseRisk, broadRisk * 0.70),
            max(protectionGate, overloadGate));
        const float eventGate = smoothstep(P15.z, P15.w, eventSeed);
        const float holdGate = smoothstep(P12.z, P12.w, holdSeed);
        // Either analyzer-space coherent translation or verified full-resolution
        // motion classification is sufficient to reject temporal drag. Local flow
        // is especially important for small bright moving objects that occupy too
        // little of a 128x72 analysis patch to register as camera motion.
        const float coarseMotionGate = smoothstep(0.20, 0.68, coarseMotion);
        // CPU readback is advisory only. When fresh NVIDIA flow exists, retain a
        // small CPU whole-frame prior but never let a delayed readback overrule
        // verified current-frame GPU transport.
        const float cpuMotionWeight = hardwareFlowValid ? 0.25 : 1.0;
        const float cpuCameraMotionGate =
            (P9.x > 0.5 || protectionGate > 0.5 || overloadGate > 0.5) ? 0.0 :
            cpuMotionWeight * smoothstep(
                max(0.10, P5.x * 0.85), max(0.18, P5.x * 1.35), P6.y);
        // Sequence-state classification needs an independent whole-frame motion
        // veto. The ordinary CPU prior above is intentionally muted while active
        // protection is running; reusing it made an extreme pan look stationary
        // precisely after the pan itself triggered protection.
        const float unmaskedCpuCameraMotionGate = smoothstep(
            max(0.10, P5.x * 0.85), max(0.18, P5.x * 1.35), P6.y);
        // Normalize the deliberately down-weighted CPU prior before using it as
        // scene-level corroboration. A real fast translation can accumulate
        // flash-like per-pixel reversals, but it also produces coherent coarse or
        // whole-frame motion. A stationary regional flash produces neither.
        const float cpuMotionCorroboration = saturate(
            cpuCameraMotionGate / max(cpuMotionWeight, 0.001));
        const float corroboratedMotionGate = max(
            coarseMotionGate, cpuMotionCorroboration);
)HLSL"
R"HLSL(
        // Modes 16-22 keep mode 14's corrected current-pixel event semantics only
        // for LOCAL motion. During coherent whole-frame/camera motion, photometric
        // overlap is too permissive: blend the event interpretation back to raw
        // disocclusion so fast pans cannot be mistaken for intrinsic flashes.
        const bool cameraAwareEventDisocclusionArchitecture =
            P16.x > 15.5 && P16.x < 24.5;
        // Matrix 25 showed a separate stationary sequence problem: 5 Hz flashes
        // lose protection because the 100 ms surface-risk state decays between
        // opposing half-cycles, while a 4-code reversal is too weak to establish
        // useful opposition authority. Modes 17-22 change neither rule for moving
        // content. They extend already-qualified risk only when scene-level motion
        // is absent, and tiny changes still need an opposing signed reversal
        // before they are allowed to affect the display.
        const bool stationaryWeakRepetitionArchitecture =
            P16.x > 16.5 && P16.x < 24.5;
        // Mode 18 preserves qualified state when CURRENT-surface geometry proves
        // continuity. Mode 19 attempted a textureless fallback, but Matrix 28
        // showed that max(localMotionGate, hardwareMotionGate) is itself polluted
        // by photometric reversals and that signed-prime continuity still used the
        // same raw motion evidence. Modes 20-22 keep mode 18's geometric path and
        // add a fallback only when scene motion is absent AND neither current-
        // surface nor global-flow coherence proves real displacement.
        const bool stationaryQualifiedStateArchitecture =
            P16.x > 17.5 && P16.x < 18.5;
        const bool stationaryMotionOnlyStateArchitecture =
            P16.x > 18.5 && P16.x < 19.5;
        const bool texturelessStationaryPrimeStateArchitecture =
            P16.x > 19.5 && P16.x < 24.5;
        // Mode 23 keeps mode 20 semantics except that an unmasked whole-frame
        // camera translation veto is applied only to the textureless fallback.
        // It does not inherit mode 21/22's broader sequence-state rewrites.
        const bool minimalTexturelessCameraVetoArchitecture =
            P16.x > 22.5 && P16.x < 23.5;
        // Mode 24 isolates mode 21's current-event camera veto. It leaves mode
        // 20's risk/prime lifetimes, stable authority and hold authorization
        // unchanged, so Matrix 33 can attribute any pan change to this branch.
        const bool currentEventOnlyCameraGuardArchitecture =
            P16.x > 23.5 && P16.x < 24.5;
        // Modes 21/22 make the whole-frame translation score authoritative even
        // during active protection. Mode 22 receives a structural/gradient-
        // validated score in P6.y; mode 21 and production retain raw luminance.
        // This guard still affects sequence state only.
        const bool qualifiedStationaryCameraGuardArchitecture =
            P16.x > 20.5 && P16.x < 22.5;
        const float stationaryMotionGuardGate =
            qualifiedStationaryCameraGuardArchitecture ?
                max(corroboratedMotionGate, unmaskedCpuCameraMotionGate) :
                corroboratedMotionGate;
        const float stationaryRepetitionGate =
            stationaryWeakRepetitionArchitecture ?
                (1.0 - smoothstep(0.08, 0.30, stationaryMotionGuardGate)) : 0.0;
        const float localSequenceMotionGate = smoothstep(
            0.10, 0.45, max(localMotionGate, hardwareMotionGate));
        const float coherentSequenceMotionGate = smoothstep(
            0.20, 0.60, max(diagCurrentSurfaceGate, diagGlobalFlowGate));
        const float baseTexturelessStationaryFallbackGate =
            texturelessStationaryPrimeStateArchitecture ?
                stationaryRepetitionGate * (1.0 - coherentSequenceMotionGate) : 0.0;
        const float texturelessStationaryFallbackGate =
            minimalTexturelessCameraVetoArchitecture ?
                baseTexturelessStationaryFallbackGate *
                    (1.0 - unmaskedCpuCameraMotionGate) :
                baseTexturelessStationaryFallbackGate;
        const float stationarySequenceStateGate =
            stationaryMotionOnlyStateArchitecture ?
                stationaryRepetitionGate * (1.0 - localSequenceMotionGate) :
            (texturelessStationaryPrimeStateArchitecture ?
                texturelessStationaryFallbackGate : stationaryRepetitionGate);
)HLSL" R"HLSL(
        const float texturelessStateDisocclusionGateBase =
            qualifiedStationaryCameraGuardArchitecture ?
                lerp(explicitDisocclusionGate,
                    correctedCurrentPixelDisocclusionGate,
                    stationaryRepetitionGate) *
                    (1.0 - texturelessStationaryFallbackGate) :
                min(correctedCurrentPixelDisocclusionGate,
                    explicitDisocclusionGate *
                        (1.0 - texturelessStationaryFallbackGate));
        const float texturelessStateDisocclusionGate =
            minimalTexturelessCameraVetoArchitecture ?
                lerp(texturelessStateDisocclusionGateBase,
                    explicitDisocclusionGate, unmaskedCpuCameraMotionGate) :
                texturelessStateDisocclusionGateBase;
        const bool restoreQualifiedState =
            texturelessStationaryPrimeStateArchitecture ?
                ((qualifiedStationaryCameraGuardArchitecture ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
                 texturelessStateDisocclusionGate <= P13.z) :
            (stationaryMotionOnlyStateArchitecture ?
                (stationarySequenceStateGate > 0.0) :
                (stationaryQualifiedStateArchitecture &&
                 stationarySequenceStateGate > 0.0 &&
                 correctedCurrentPixelDisocclusionGate <= P13.z));
        if (restoreQualifiedState && previousProtectionValid &&
            previousProtectionState.g > 0.06)
        {
            const float restoredRisk = saturate(previousProtectionState.g) *
                exp(-dt / max(P13.w, 0.005));
            transportedSurfaceRisk = max(transportedSurfaceRisk, restoredRisk);
        }
        const float stationaryRiskStateGate =
            qualifiedStationaryCameraGuardArchitecture ?
                stationarySequenceStateGate : stationaryRepetitionGate;
        if (stationaryRiskStateGate > 0.0 && transportedSurfaceRisk > 0.0)
        {
            const float baseRiskTau = max(P13.w, 0.005);
            const float stationaryRiskTau = max(baseRiskTau,
                qualifiedStationaryCameraGuardArchitecture ? 0.40 : 0.22);
            const float decayCompensation = exp(dt *
                (1.0 / baseRiskTau - 1.0 / stationaryRiskTau));
            transportedSurfaceRisk = saturate(transportedSurfaceRisk *
                lerp(1.0, decayCompensation, stationaryRiskStateGate));
        }
        const float currentEventMotionGuardGate =
            currentEventOnlyCameraGuardArchitecture ?
                max(corroboratedMotionGate, unmaskedCpuCameraMotionGate) :
                stationaryMotionGuardGate;
        const float wholeFrameMotionEventVeto =
            cameraAwareEventDisocclusionArchitecture ?
                smoothstep(0.35, 0.75, currentEventMotionGuardGate) : 0.0;
        const float eventDisocclusionGate = lerp(
            eventDisocclusionGateBase, rawExplicitDisocclusionGate,
            wholeFrameMotionEventVeto);
        const float motionGate = max(max(coarseMotionGate, localMotionGate),
            cpuCameraMotionGate);

        // Small ordinary changes should not drag filtered history around. A
        // CURRENT hazard gets a sensitive localization gate; MEMORY-only release
        // requires a much larger outstanding displayed difference.
        const float eventDeltaGate = smoothstep(P10.x, P10.y, validatedEventDelta);
        const float holdDeltaGate = smoothstep(P10.z, P10.w, displayedDelta);

        // During MEMORY-only release, source history is the crucial discriminator:
        // if the raw source barely changed, this is the same protected
        // surface and we must continue a smooth safe catch-up. If the raw source
        // changed substantially but there is no new hazard event, treat it as new
        // content/motion and mostly bypass old history. A high-risk memory keeps a
        // small safety floor in case one detector frame is missed during repetition.
        const float stableSourceGate = 1.0 - smoothstep(P11.x, P11.y, sourceDelta);
        const float veryHighMemory = smoothstep(0.78, 0.96, holdSeed);
        // Memory alone must not drag bright moving content. Keep only a tiny floor
        // for a detector-frame miss during an already very strong flash sequence.
        const float movingHoldFloor = lerp(0.0, P14.w, veryHighMemory);
        const float holdContentGate = max(stableSourceGate, movingHoldFloor);

        // Full-resolution compensated source residual is the intrinsic-change
        // signal. Geometry may be valid while a moving surface flashes; in that
        // case the residual stays large and must override the motion bypass.
        // Stale flash memory is never allowed to invalidate geometry.
        const float intrinsicResidualGate =
            smoothstep(P11.z, P11.w, motionCompensatedSourceDelta);
        const float directIntrinsicEvent =
            intrinsicResidualGate * smoothstep(P15.x, P15.y, displayedDelta);
        const float eventMask =
            max(eventGate * eventDeltaGate, directIntrinsicEvent);
        const float holdMask = holdGate * holdContentGate * holdDeltaGate;
        const float repeatedRisk = max(temporalRisk, transportedSurfaceRisk);
        const float repeatedMemoryGate = smoothstep(P12.x, P12.y, repeatedRisk);
        // A repeated flash spends most of each half-cycle with near-zero
        // frame-to-frame source delta. Keep intrinsic authority alive across that
        // stable interval instead of letting noisy/global flow reopen the bypass.
        // Scrolling does not satisfy this because the raw source at a screen
        // coordinate keeps changing as content moves through it.
        const float repeatedCurrentIntrinsicAuthority =
            repeatedMemoryGate * eventMask * intrinsicResidualGate;
        const float stableMotionConflict =
            smoothstep(0.10, 0.45, stationaryMotionGuardGate);
        const float repeatedStableIntrinsicAuthority =
            repeatedMemoryGate * holdGate * stableSourceGate *
            (1.0 - stableMotionConflict);
        const float repeatedIntrinsicAuthority = max(
            repeatedCurrentIntrinsicAuthority, repeatedStableIntrinsicAuthority);
        const float motionProtectionAuthority = max(
            intrinsicResidualGate, repeatedStableIntrinsicAuthority);

        // Correspondence and appearance are independent. Ordinary verified
        // correspondence bypasses old displayed history only while its compensated
        // residual is small. Vacated/disoccluded pixels normally drop the history
        // of the surface that left, but repeated intrinsic evidence may conservatively
        // override that drop when geometry and flash evidence conflict.
        const float correspondenceGate =
            max(motionGate, hardwareMotionGate);
        const float verifiedFlashOverride =
            hardwareMotionGate * intrinsicResidualGate;
        // Mode 5 tests the architectural invariant that verified disocclusion
        // geometry must not be suppressed by stale/repeated flash memory.
        const float disocclusionAuthority =
            P16.x > 4.5 ? 1.0 : (1.0 - repeatedIntrinsicAuthority);
        const float effectiveMotionGate = max(
            explicitDisocclusionGate * disocclusionAuthority,
            correspondenceGate * (1.0 - motionProtectionAuthority));

        // Red safety gets the same geometry-independent intrinsic authority.
        // Ordinary translating red content has a small compensated residual and
        // therefore remains untouched; a true red appearance change is clamped
        // immediately and stays desaturated through a stable repeated half-cycle.
        const float residualRedAuthority =
            saturate(max(intrinsicResidualGate, repeatedStableIntrinsicAuthority));
        finalRedSafetyAuthority = max(finalRedSafetyAuthority,
            max(residualRedAuthority,
                redMitigationGate * (1.0 - effectiveMotionGate)));
        const float intrinsicRedDesat =
            residualRedAuthority * smoothstep(0.05, 0.35, isolatedRed);
        if (intrinsicRedDesat > 0.001)
        {
            const float residualGray = Luma(cur);
            cur = lerp(cur, residualGray.xxx, intrinsicRedDesat);
            candidateL = Luma(cur);
        }
        authorityPreprocessLumaDelta = saturate(abs(candidateL - Luma(rawSourceColor)));
)HLSL" R"HLSL(


        // Once a pixel has accumulated repeated-flash memory, keep the output
        // truly stationary between opposing transitions. Merely shrinking each
        // excursion leaves tiny reversals that still count in a strict one-second
        // transition test. Geometry remains valid, but current-surface transport
        // cannot veto a hold when its own compensated residual says the surface
        // changed intrinsically.
        const float repeatedHoldAuthorization =
            repeatedMemoryGate * max(eventMask, holdGate * stableSourceGate);
        const float verifiedCurrentSurfaceTransport =
            smoothstep(P13.x, P13.y, diagCurrentSurfaceGate);
        const float verifiedVacatedTransport =
            smoothstep(P13.x, P13.y, diagVacatedGate);
        const float verifiedInfillTransport =
            smoothstep(P13.x, P13.y, diagInfillGate) *
            coarseMotionGate *
            (fullDisocclusionCorrection ?
                (1.0 - verifiedCurrentSurfaceTransport) : 1.0);
        const float currentSurfaceHoldVeto =
            verifiedCurrentSurfaceTransport * (1.0 - motionProtectionAuthority);
        const float disocclusionHoldVeto =
            max(verifiedVacatedTransport, verifiedInfillTransport) *
            disocclusionAuthority;
        const float verifiedLocalTransportGate =
            max(currentSurfaceHoldVeto, disocclusionHoldVeto);
        const float stationaryCurrentHoldAuthorization =
            eventMask *
            (1.0 - smoothstep(0.02, 0.12, stationaryMotionGuardGate)) *
            (1.0 - verifiedLocalTransportGate);
        const float repeatedHoldMask =
            repeatedHoldAuthorization * (1.0 - effectiveMotionGate);
        float temporalMask = max(
            max(eventMask, holdMask) * (1.0 - effectiveMotionGate),
            repeatedHoldMask);

        // A strong intrinsic residual can authorize the current transition even
        // when coarse analyzer-space motion classified the same region as moving.
        if (max(eventSeed, directIntrinsicEvent) >= 0.12 &&
            displayedDelta >= 0.018 && effectiveMotionGate < 0.55)
            temporalMask = max(temporalMask, 1.0 - effectiveMotionGate);
        if (stationaryCurrentHoldAuthorization > 0.72)
            temporalMask = 1.0;

        // Replay binds three full-resolution diagnostic MRTs. Production binds
        // only the display/history targets, so these outputs are discarded there.
        // Keeping every metric at every pixel avoids the old x%3 + stride-4
        // phase locking that badly undersampled small moving objects.
        output.motionDiagnostics0 = float4(
            diagGlobalFlowGate, diagCurrentSurfaceGate,
            diagVacatedGate, diagInfillGate);
        output.motionDiagnostics1 = float4(
            diagPortableGate, hardwareMotionGate,
            effectiveMotionGate, repeatedRisk);
        output.motionDiagnostics2 = float4(
            verifiedFlashOverride, coarseMotionGate,
            cpuCameraMotionGate, temporalMask);
        float surfaceRiskStateSeed = 0.0;

        // Architectures 3+ keep display mitigation image-memory-free. Persistent
        // LOCAL risk may follow only a validated surface; benchmark mode 8 goes
        // further and requires a real signed reversal on that same raw surface
        // before any display-active risk memory can be seeded.
        if (P16.x > 2.5)
        {
            const bool eventOnlyArchitecture =
                P16.x > 5.5 && P16.x < 6.5;
            const bool repetitionGatedArchitecture =
                P16.x > 6.5 && P16.x < 7.5;
            const bool oppositionGatedArchitecture = P16.x > 7.5;
            const bool surfacePropagationArchitecture =
                P16.x > 8.5 && P16.x < 9.5;
            const bool phaseHoldArchitecture =
                P16.x > 9.5 && P16.x < 10.5;
            const bool flowConsensusArchitecture =
                P16.x > 10.5 && P16.x < 11.5;
            const bool interiorIntrinsicArchitecture = P16.x > 11.5;
            const float hardDisocclusion =
                smoothstep(0.30, 0.60, explicitDisocclusionGate);
            const float hardEventDisocclusion =
                smoothstep(0.30, 0.60, eventDisocclusionGate);
            const float stationaryStateDisocclusionGate =
                texturelessStationaryPrimeStateArchitecture ?
                    texturelessStateDisocclusionGate :
                (stationaryMotionOnlyStateArchitecture ?
                    explicitDisocclusionGate *
                        (1.0 - stationarySequenceStateGate) :
                (stationaryQualifiedStateArchitecture ?
                    lerp(explicitDisocclusionGate,
                        correctedCurrentPixelDisocclusionGate,
                        stationaryRepetitionGate) :
                    explicitDisocclusionGate));
            const float hardStateDisocclusion =
                smoothstep(0.30, 0.60, stationaryStateDisocclusionGate);
            const float stateContinuityEvidence =
                texturelessStationaryPrimeStateArchitecture ?
                    max(max(stableSourceGate, verifiedCurrentSurfaceTransport),
                        texturelessStationaryFallbackGate) :
                (stationaryMotionOnlyStateArchitecture ?
                    max(max(stableSourceGate, verifiedCurrentSurfaceTransport),
                        stationarySequenceStateGate) :
                    max(stableSourceGate, verifiedCurrentSurfaceTransport));
            const float surfaceContinuity =
                saturate(stateContinuityEvidence) * (1.0 - hardStateDisocclusion);
            transportedSurfaceRisk *= surfaceContinuity;
            transportedRedAuthority *= surfaceContinuity;
            finalRedSafetyAuthority = max(
                finalRedSafetyAuthority, transportedRedAuthority);

            // Analyzer current-event evidence is accepted on stationary content.
            // A moving surface must instead show an intrinsic compensated residual;
            // ordinary translation therefore cannot become a display mask.
            const float stationaryCurrentEvent =
                eventGate * eventDeltaGate * (1.0 - correspondenceGate);
            const float disocclusionEventVeto = P16.x > 3.5 ?
                smoothstep(0.08, 0.35, eventDisocclusionGate) : 0.0;
            float currentIntrinsicEvent =
                max(directIntrinsicEvent, stationaryCurrentEvent) *
                (1.0 - disocclusionEventVeto);

            // Mode 9 expands CURRENT intrinsic authority across the same moving
            // surface without transporting neighbor RGB or display history. This
            // fills uniform moving-flash interiors where optical flow is locally
            // under-observable, while requiring at least two coherent moving
            // neighbors with nearly identical current appearance. A one-frame
            // propagation therefore cannot remain behind on vacated background.
            if (surfacePropagationArchitecture && P7.z > 0.5 &&
                hardwareFlowAvailable && hardDisocclusion < 0.5)
            {
                const float2 propagationOutputSize =
                    max(float2(P2.z, P2.w), float2(1.0, 1.0));
                const float2 propagationTexel = 1.0 / propagationOutputSize;
                const float2 centerFlow =
                    LoadOpticalFlow(ForwardOpticalFlow, i.uv);
                const float centerFlowMagnitude = length(centerFlow);
                float propagatedEvent = 0.0;
                float propagatedSupportCount = 0.0;
                [unroll]
                for (int pri = 0; pri < 2; ++pri)
                {
                    const float radius = pri == 0 ? 6.0 : 14.0;
                    [unroll]
                    for (int ni = 0; ni < 4; ++ni)
                    {
                        const float2 direction = ni == 0 ? float2(-1.0, 0.0) :
                            (ni == 1 ? float2(1.0, 0.0) :
                            (ni == 2 ? float2(0.0, -1.0) : float2(0.0, 1.0)));
                        const float2 neighborUv =
                            i.uv + direction * propagationTexel * radius;
                        const bool insideNeighbor =
                            all(neighborUv >= float2(0.0, 0.0)) &&
                            all(neighborUv <= float2(1.0, 1.0));
                        if (insideNeighbor)
                        {
                            const float3 neighborCurrent = CurrentFrame.SampleLevel(
                                LinearClamp, neighborUv, 0.0).rgb;
                            const float sameCurrentSurface = 1.0 - smoothstep(
                                0.004, 0.025,
                                SourceMatchError(rawSourceColor, neighborCurrent));
                            const float2 neighborFlow =
                                LoadOpticalFlow(ForwardOpticalFlow, neighborUv);
                            const float neighborFlowMagnitude = length(neighborFlow);
                            if (sameCurrentSurface > 0.05 &&
                                neighborFlowMagnitude > 0.10)
                            {
                                float flowCoherence = 1.0;
                                if (centerFlowMagnitude > 0.10)
                                {
                                    const float directionAgreement =
                                        dot(centerFlow, neighborFlow) /
                                        max(centerFlowMagnitude * neighborFlowMagnitude, 0.001);
                                    const float magnitudeAgreement =
                                        min(centerFlowMagnitude, neighborFlowMagnitude) /
                                        max(centerFlowMagnitude, neighborFlowMagnitude);
                                    flowCoherence =
                                        smoothstep(0.68, 0.92, directionAgreement) *
                                        smoothstep(0.25, 0.70, magnitudeAgreement);
                                }
                                const float2 neighborPreviousUv =
                                    neighborUv + neighborFlow / propagationOutputSize;
                                const bool insideNeighborPrevious =
                                    all(neighborPreviousUv >= float2(0.0, 0.0)) &&
                                    all(neighborPreviousUv <= float2(1.0, 1.0));
                                if (insideNeighborPrevious)
                                {
                                    const float3 neighborPreviousRaw =
                                        PreviousSource.SampleLevel(LinearClamp,
                                            neighborPreviousUv, 0.0).rgb;
                                    const float neighborResidual =
                                        SourceMatchError(
                                            neighborCurrent, neighborPreviousRaw);
                                    const float neighborPreviousDisplayedL = Luma(
                                        PreviousOutput.SampleLevel(
                                            LinearClamp, neighborUv, 0.0).rgb);
                                    const float neighborDisplayedDelta = abs(
                                        Luma(neighborCurrent) -
                                        neighborPreviousDisplayedL);
                                    const float neighborEvent =
                                        smoothstep(P11.z, P11.w, neighborResidual) *
                                        smoothstep(P15.x, P15.y,
                                            neighborDisplayedDelta);
                                    const float support = neighborEvent *
                                        sameCurrentSurface * flowCoherence;
                                    propagatedEvent = max(
                                        propagatedEvent, support);
                                    propagatedSupportCount +=
                                        support > 0.45 ? 1.0 : 0.0;
                                }
                            }
                        }
                    }
                }
                currentIntrinsicEvent = max(currentIntrinsicEvent,
                    propagatedEvent * step(1.5, propagatedSupportCount) *
                    (1.0 - hardDisocclusion));
            }
)HLSL"
R"HLSL(

            // Mode 11 fills the geometry hole rather than propagating display
            // authority. Textureless moving interiors can have usable NVOFA vectors
            // that the normal structure/observability gate rejects. Borrow a vector
            // only from a CURRENT neighbor with nearly identical appearance, verify
            // that vector by forward/backward round trip and NVOFA cost, then apply
            // it to THIS pixel solely to compare current raw source with the previous
            // raw surface. Ordinary translation therefore produces a small residual;
            // an intrinsically flashing moving surface produces a large one. No RGB
            // or event memory is transported by this path.
            if (flowConsensusArchitecture && P7.z > 0.5 &&
                hardwareFlowValid && hardDisocclusion < 0.5 &&
                currentIntrinsicEvent < 0.95 && displayedDelta > P15.x)
            {
                const float2 consensusOutputSize =
                    max(float2(P2.z, P2.w), float2(1.0, 1.0));
                const float2 consensusTexel = 1.0 / consensusOutputSize;
                float consensusIntrinsicEvent = 0.0;
                [unroll]
                for (int fri = 0; fri < 4; ++fri)
                {
                    const float radius = fri == 0 ? 6.0 :
                        (fri == 1 ? 12.0 : (fri == 2 ? 18.0 : 22.0));
                    [unroll]
                    for (int fni = 0; fni < 4; ++fni)
                    {
                        const float2 direction = fni == 0 ? float2(-1.0, 0.0) :
                            (fni == 1 ? float2(1.0, 0.0) :
                            (fni == 2 ? float2(0.0, -1.0) : float2(0.0, 1.0)));
                        const float2 neighborUv =
                            i.uv + direction * consensusTexel * radius;
                        const bool insideNeighbor =
                            all(neighborUv >= float2(0.0, 0.0)) &&
                            all(neighborUv <= float2(1.0, 1.0));
                        if (!insideNeighbor) continue;

                        const float3 neighborCurrent = CurrentFrame.SampleLevel(
                            LinearClamp, neighborUv, 0.0).rgb;
                        const float sameCurrentSurface = 1.0 - smoothstep(
                            0.004, 0.025,
                            SourceMatchError(rawSourceColor, neighborCurrent));
                        if (sameCurrentSurface <= 0.20) continue;

                        const float2 candidateFlow =
                            LoadOpticalFlow(ForwardOpticalFlow, neighborUv);
                        const float candidateMagnitude = length(candidateFlow);
                        if (candidateMagnitude <= 0.10) continue;

                        const float2 previousCandidateUv =
                            i.uv + candidateFlow / consensusOutputSize;
                        const bool insidePreviousCandidate =
                            all(previousCandidateUv >= float2(0.0, 0.0)) &&
                            all(previousCandidateUv <= float2(1.0, 1.0));
                        if (!insidePreviousCandidate) continue;

                        const float2 backwardCandidate = LoadOpticalFlow(
                            BackwardOpticalFlow, previousCandidateUv);
                        const float roundTripError =
                            length(candidateFlow + backwardCandidate);
                        const float allowedRoundTrip =
                            max(1.25, 0.65 + candidateMagnitude * 0.22);
                        const float fbConfidence = 1.0 - smoothstep(
                            allowedRoundTrip, allowedRoundTrip + 2.0,
                            roundTripError);
                        float costConfidence = 1.0;
                        if (P9.w > 0.5)
                            costConfidence = 1.0 - smoothstep(0.28, 0.78,
                                LoadOpticalCost(ForwardOpticalCost, neighborUv));
                        const float geometryEvidence =
                            sameCurrentSurface * fbConfidence *
                            lerp(0.35, 1.0, costConfidence);
                        if (geometryEvidence <= 0.18) continue;

                        const float3 previousCandidateRaw =
                            PreviousSource.SampleLevel(LinearClamp,
                                previousCandidateUv, 0.0).rgb;
                        const float candidateResidual = SourceMatchError(
                            rawSourceColor, previousCandidateRaw);
                        const float candidateEvent =
                            smoothstep(0.18, 0.42, geometryEvidence) *
                            smoothstep(P11.z, P11.w, candidateResidual) *
                            smoothstep(P15.x, P15.y, displayedDelta);
                        consensusIntrinsicEvent = max(
                            consensusIntrinsicEvent, candidateEvent);
                    }
                }
                currentIntrinsicEvent = max(currentIntrinsicEvent,
                    consensusIntrinsicEvent * (1.0 - hardDisocclusion));
            }

            // Mode 12 exploits a geometric invariant that does not require optical
            // flow. With the benchmark's small translation, a pixel well inside a
            // uniform moving surface still covered the same surface at this screen
            // coordinate one frame ago. Ordinary translation therefore has a small
            // same-coordinate raw delta there. A real intrinsic flash changes the
            // whole interior. Require four-sided CURRENT appearance support before
            // trusting sourceDelta, which excludes leading/trailing motion edges and
            // disocclusions. This is current-frame authority only: no display or event
            // memory is added.
            if (interiorIntrinsicArchitecture && P7.z > 0.5 &&
                hardEventDisocclusion < 0.5 && currentIntrinsicEvent < 0.95 &&
                sourceDelta > P11.z && displayedDelta > P15.x)
            {
                const float2 interiorOutputSize =
                    max(float2(P2.z, P2.w), float2(1.0, 1.0));
                const float2 interiorTexel = 1.0 / interiorOutputSize;
                float interiorSupport = 1.0;
                [unroll]
                for (int ini = 0; ini < 4; ++ini)
                {
                    const float2 direction = ini == 0 ? float2(-1.0, 0.0) :
                        (ini == 1 ? float2(1.0, 0.0) :
                        (ini == 2 ? float2(0.0, -1.0) : float2(0.0, 1.0)));
                    const float2 neighborUv =
                        i.uv + direction * interiorTexel * 6.0;
                    const bool insideNeighbor =
                        all(neighborUv >= float2(0.0, 0.0)) &&
                        all(neighborUv <= float2(1.0, 1.0));
                    if (!insideNeighbor)
                    {
                        interiorSupport = 0.0;
                    }
                    else
                    {
                        const float3 neighborCurrent = CurrentFrame.SampleLevel(
                            LinearClamp, neighborUv, 0.0).rgb;
                        const float sameCurrentSurface = 1.0 - smoothstep(
                            0.004, 0.025,
                            SourceMatchError(rawSourceColor, neighborCurrent));
                        interiorSupport = min(interiorSupport, sameCurrentSurface);
                    }
                }
                const float interiorGate =
                    smoothstep(0.70, 0.95, interiorSupport);
                const float interiorIntrinsicEvent =
                    interiorGate * smoothstep(P11.z, P11.w, sourceDelta) *
                    smoothstep(P15.x, P15.y, displayedDelta) *
                    (1.0 - hardEventDisocclusion);
                currentIntrinsicEvent = max(
                    currentIntrinsicEvent, interiorIntrinsicEvent);
            }
)HLSL"
R"HLSL(

            // Mode 8 keeps a short signed PRIME separately from display-active
            // risk. R encodes [-1,+1] around 0.5 only in this benchmark mode.
            // Unlike ordinary risk continuity, prime continuity must tolerate an
            // intrinsic appearance change: a stationary flash would otherwise
            // erase its own first sign before the opposite transition arrives.
            const float previousMatchedRawL =
                (previousProtectionValid && P7.z > 0.5) ?
                Luma(PreviousSource.SampleLevel(
                    LinearClamp, protectionStatePreviousUv, 0.0).rgb) :
                Luma(rawSourceColor);
            const float signedIntrinsicDelta =
                Luma(rawSourceColor) - previousMatchedRawL;
            const float signedMagnitudeGate =
                smoothstep(P15.x, P15.y, abs(signedIntrinsicDelta));
            // Mode 17 may prime a very small stationary transition, but a lone
            // transition is still display-inert. Only a later opposite sign can
            // convert this weak prime into current safety authority.
            const float weakSignedMagnitudeGate =
                stationaryWeakRepetitionArchitecture ?
                    stationaryRepetitionGate *
                    smoothstep(0.0010, 0.0060, abs(signedIntrinsicDelta)) : 0.0;
            const float currentSignedDirection =
                signedIntrinsicDelta >= 0.0 ? 1.0 : -1.0;
            const float baseSignedPrimeTau = max(0.05, 0.18 * P14.x);
            const float stationarySignedPrimeTau =
                qualifiedStationaryCameraGuardArchitecture ?
                    max(baseSignedPrimeTau, 0.40) : baseSignedPrimeTau;
            const float effectiveSignedPrimeTau =
                qualifiedStationaryCameraGuardArchitecture ?
                    lerp(baseSignedPrimeTau, stationarySignedPrimeTau,
                        stationarySequenceStateGate) : baseSignedPrimeTau;
            float transportedSignedPrime = oppositionGatedArchitecture &&
                previousProtectionValid ?
                (saturate(previousProtectionState.r) * 2.0 - 1.0) *
                exp(-dt / effectiveSignedPrimeTau) : 0.0;
            const float localSurfaceMotionEvidence =
                max(localMotionGate, hardwareMotionGate);
            const float stationaryPrimeContinuity =
                1.0 - smoothstep(0.10, 0.45, localSurfaceMotionEvidence);
            const float signedPrimeContinuityEvidence =
                texturelessStationaryPrimeStateArchitecture ?
                    max(max(stationaryPrimeContinuity,
                        verifiedCurrentSurfaceTransport),
                        texturelessStationaryFallbackGate) :
                    max(stationaryPrimeContinuity,
                        verifiedCurrentSurfaceTransport);
            const float signedPrimeContinuity =
                saturate(signedPrimeContinuityEvidence) *
                (1.0 - hardStateDisocclusion);
            transportedSignedPrime *= signedPrimeContinuity;
            const float oppositionStrength = max(
                0.0, -transportedSignedPrime * currentSignedDirection);
            const float opposingTransitionGate = oppositionGatedArchitecture ?
                smoothstep(P12.x, P12.y, oppositionStrength) *
                signedMagnitudeGate : 0.0;
            const float weakOpposingTransitionGate =
                stationaryWeakRepetitionArchitecture ?
                    smoothstep(0.08, 0.28, oppositionStrength) *
                    weakSignedMagnitudeGate : 0.0;
            const float qualifiedIntrinsicEvent = max(
                currentIntrinsicEvent, weakOpposingTransitionGate);
            const float effectiveOpposingTransitionGate = max(
                opposingTransitionGate, weakOpposingTransitionGate);

            const float persistentSeedAuthority = repetitionGatedArchitecture ?
                repeatedMemoryGate : (oppositionGatedArchitecture ?
                    effectiveOpposingTransitionGate : 1.0);
            surfaceRiskStateSeed = eventOnlyArchitecture ? 0.0 :
                qualifiedIntrinsicEvent * persistentSeedAuthority;
            const float surfaceMemoryMitigation = eventOnlyArchitecture ?
                0.0 : smoothstep(0.06, 0.45, transportedSurfaceRisk);

            // Mode 10 adds exactly one nonrecursive frame of event memory. Alpha
            // stores only the PREVIOUS frame's intrinsic-event strength around a
            // 0.5-valid baseline. The hold is allowed only while raw appearance
            // remains stable on the selected surface and is vetoed on verified
            // disocclusion. Unlike surface risk, this state is never fed back
            // into itself, so it cannot grow a long-lived trail.
            const float previousPhaseEvent = phaseHoldArchitecture &&
                previousProtectionValid ?
                saturate((previousProtectionState.a - 0.5) * 2.0) : 0.0;
            const float phaseHoldMitigation = phaseHoldArchitecture ?
                smoothstep(0.06, 0.45, previousPhaseEvent *
                    stableSourceGate * (1.0 - hardDisocclusion)) : 0.0;
            authorityCurrentEventStrength = saturate(qualifiedIntrinsicEvent);
            authoritySurfaceMemoryStrength = saturate(max(
                surfaceMemoryMitigation, phaseHoldMitigation));
            const float currentFrameStrength = saturate(max(max(
                qualifiedIntrinsicEvent, surfaceMemoryMitigation),
                phaseHoldMitigation));
            if (phaseHoldArchitecture)
                phaseHoldStateEncoded =
                    saturate(0.5 + 0.5 * currentIntrinsicEvent);

            if (oppositionGatedArchitecture)
            {
                const float primeWrite = saturate(max(
                    2.0 * currentIntrinsicEvent * signedMagnitudeGate,
                    weakSignedMagnitudeGate));
                const float nextSignedPrime = lerp(
                    transportedSignedPrime, currentSignedDirection, primeWrite);
                signedPrimeStateEncoded =
                    saturate(0.5 + 0.5 * nextSignedPrime);
            }

            const float architectureInputL = candidateL;
            const float resolvedL = lerp(
                candidateL, saturate(P16.y),
                saturate(currentFrameStrength * max(P16.z, 0.0)));
            authorityArchitectureLumaDelta =
                saturate(abs(resolvedL - architectureInputL));
            cur = RemapGlobalLuminance(cur, candidateL, resolvedL);
            candidateL = Luma(cur);
            safetyHistoryL = candidateL;
        }
        // Keep architectures 1/2 intact as failed controls. They remove displayed
        // luminance history but still let screen-space risk/hold memory drive output.
        else if (P16.x > 0.5)
        {
            const float hardDisocclusion =
                smoothstep(0.30, 0.60, explicitDisocclusionGate);
            const float memoryMitigation =
                smoothstep(0.06, 0.45, repeatedRisk);
            float currentFrameStrength = saturate(max(
                max(eventMask, holdMask), memoryMitigation));
            currentFrameStrength *= (1.0 - hardDisocclusion);

            // Risk-only mitigation is intentionally image-memory-free: compress
            // risky current-frame luminance toward a fixed linear-light neutral.
            // This may leave temporary contrast reduction after a flash, but it
            // cannot paste an old object's luminance onto newly revealed pixels.
            float resolvedL = lerp(
                candidateL, 0.18, saturate(currentFrameStrength * 0.92));

            if (P16.x > 1.5)
            {
                // Hybrid mode adds a FIRST-TRANSITION limiter using the previous
                // RAW source, never the previous filtered output. On the next
                // frame PreviousSource already contains the new phase, so this
                // cannot become recursive display history. Motion/disocclusion
                // veto it aggressively.
                const float previousRawL = P7.z > 0.5 ?
                    Luma(PreviousSource.SampleLevel(LinearClamp, protectionStatePreviousUv, 0.0).rgb) :
                    candidateL;
                const float sameSurfaceGate =
                    (1.0 - hardDisocclusion) * max(
                        1.0 - smoothstep(0.08, 0.35, corroboratedMotionGate), verifiedCurrentSurfaceTransport);
                const float firstEventGate =
                    smoothstep(0.10, 0.24, validatedEventDelta) *
                    smoothstep(0.35, 0.80, eventMask) * sameSurfaceGate;
                const float firstFrameStep = 1.30 * dt;
                const float firstLimitedL = clamp(
                    resolvedL, previousRawL - firstFrameStep,
                    previousRawL + firstFrameStep);
                resolvedL = lerp(
                    resolvedL, firstLimitedL, saturate(firstEventGate));
            }

            cur = RemapGlobalLuminance(cur, candidateL, resolvedL);
            candidateL = Luma(cur);
            safetyHistoryL = candidateL;
        }
        else if (temporalMask > 0.001)
        {
            const float severe = smoothstep(0.45, 0.90,
                max(max(eventSeed, holdSeed), directIntrinsicEvent));
            // Intrinsic full-resolution evidence can mark a transition current
            // even when the coarse analyzer called the same pixels motion.
            const bool currentEvent = eventMask > 0.05;
            const float tau = currentEvent ?
                lerp(0.150, 0.235, severe) * P14.x :
                lerp(0.030, 0.050, severe) * P14.y;
            const float stateAlpha = 1.0 - exp(-dt / max(tau, 0.005));

            // Recursive RGB is deliberately gone. Evolve only the transported
            // protected luminance, then impose that scalar constraint on the
            // CURRENT candidate color. Chroma therefore comes from the current
            // surface rather than a stale screen-coordinate display sample.
            float protectedL = lerp(
                previousProtectedL, candidateL, stateAlpha);
            const float exactHoldGate = max(
                repeatedHoldAuthorization, stationaryCurrentHoldAuthorization);
            if (exactHoldGate > P14.z)
                protectedL = previousProtectedL;

            // Attack and release remain bounded and monotonic in luminance-state
            // space. This keeps the existing flash-rate limit while guaranteeing
            // stale RGB cannot persist after the underlying surface changes.
            const float profileStep = min(P4.z, P4.w);
            const float standardsStep = 1.30 * dt;
            float maxStep = currentEvent ? min(profileStep, standardsStep) :
                max(standardsStep, 2.60 * dt);
            maxStep *= currentEvent ? lerp(1.0, 0.72, severe) : 1.0;
            if (exactHoldGate > P14.z)
                maxStep = 0.0;
            protectedL = clamp(protectedL,
                previousProtectedL - maxStep,
                previousProtectedL + maxStep);

            const float resolvedL = lerp(
                candidateL, protectedL, saturate(temporalMask));
            cur = RemapGlobalLuminance(cur, candidateL, resolvedL);
            candidateL = Luma(cur);
            safetyHistoryL = candidateL;
        }

        // Replay-only attribution reuses the third diagnostic MRT for risk-only
        // architectures. Production binds only targets 0-3, so this cannot alter
        // the displayed image or persistent production state.
        if (P16.x > 2.5)
            output.motionDiagnostics2 = float4(authorityPreprocessLumaDelta,
                authorityArchitectureLumaDelta, authorityCurrentEventStrength,
                authoritySurfaceMemoryStrength);

        // Refresh surface-local safety memory from intrinsic evidence only. The
        // analyzer's broader temporalRisk remains independent, while this state
        // follows moving geometry and therefore survives a genuine moving flash
        // without dragging stale RGB behind the object.
        if (P16.x > 5.5 && P16.x < 6.5)
            protectionStateRisk = 0.0;
        else if (P16.x > 3.5)
            protectionStateRisk = saturate(max(
                transportedSurfaceRisk, surfaceRiskStateSeed));
        else
            protectionStateRisk = saturate(max(transportedSurfaceRisk,
                max(directIntrinsicEvent, eventMask * intrinsicResidualGate)));
        protectionStateRedAuthority = saturate(finalRedSafetyAuthority);
    }
)HLSL" R"HLSL(

    // Temporal RGB feedback can reintroduce hazardous red from PreviousOutput
    // after the pre-temporal source clamp. Neutralize the final display/history
    // state when red hazard authority survives motion correspondence. This gate
    // must use chromatic dominance rather than absolute red amplitude: temporal
    // filtering can make a dim pixel remain fully saturated red while its sRGB R
    // value falls below the detector's source-amplitude redThreshold.
    const float finalOutputGray = Luma(cur);
    const float3 finalOutputLinear = SrgbToLinear(cur);
    const float finalOutputLinearSum =
        finalOutputLinear.r + finalOutputLinear.g + finalOutputLinear.b;
    const float finalOutputRedRatio = finalOutputLinearSum > 0.0001 ?
        finalOutputLinear.r / finalOutputLinearSum : 0.0;
    const float finalOutputRedDesat =
        smoothstep(0.04, 0.22, finalRedSafetyAuthority) *
        smoothstep(0.68, 0.80, finalOutputRedRatio);
    const float3 finalOutputGraySrgb = LinearToSrgb(finalOutputGray.xxx);
    cur = lerp(cur, finalOutputGraySrgb, finalOutputRedDesat);
    protectionStateLuma = P16.x > 7.5 ?
        signedPrimeStateEncoded : Luma(cur);

    // Save filtered content color BEFORE debug/hotkey/shield-label overlays so
    // UI pixels never contaminate temporal state. RGB remains the displayed
    // pre-overlay image for replay/localization; alpha is the compact protection
    // state: protected linear-light luminance only.
    safetyHistoryL = Luma(cur);
    const float4 historyColor = float4(cur, safetyHistoryL);

    // The debug panel is a steady, non-flashing texture composited after safety
    // processing, so its own status indicator cannot pulse with detector state.
    if (P2.y > 0.5)
    {
        float2 debugUv = i.uv * float2(P2.z / 560.0, P2.w / 640.0);
        if (debugUv.x <= 1.0 && debugUv.y <= 1.0)
        {
            float4 d = DebugOverlay.SampleLevel(LinearClamp, debugUv, 0.0);
            cur = lerp(cur, d.rgb, d.a);
        }
    }

    if (P3.x > 0.5)
    {
        const float hintWidthUv = 900.0 / max(P2.z, 1.0);
        const float hintHeightUv = 48.0 / max(P2.w, 1.0);
        if (i.uv.x <= hintWidthUv && i.uv.y >= 1.0 - hintHeightUv)
        {
            float2 hintUv = float2(
                i.uv.x / max(hintWidthUv, 0.0001),
                (i.uv.y - (1.0 - hintHeightUv)) / max(hintHeightUv, 0.0001));
            float4 h = HotkeyHint.SampleLevel(LinearClamp, hintUv, 0.0);
            cur = lerp(cur, h.rgb, h.a);
        }
    }

    if (P3.w > 0.5)
    {
        const float labelWidthUv = 520.0 / max(P2.z, 1.0);
        const float labelHeightUv = 64.0 / max(P2.w, 1.0);
        const float2 labelMin = float2(
            0.5 - labelWidthUv * 0.5, 0.5 - labelHeightUv * 0.5);
        if (i.uv.x >= labelMin.x && i.uv.x <= labelMin.x + labelWidthUv &&
            i.uv.y >= labelMin.y && i.uv.y <= labelMin.y + labelHeightUv)
        {
            const float2 labelUv = (i.uv - labelMin) /
                float2(labelWidthUv, labelHeightUv);
            float4 label = PreviousSafety.SampleLevel(LinearClamp, labelUv, 0.0);
            cur = lerp(cur, label.rgb, label.a);
        }
    }

    output.color = float4(saturate(cur), 1.0);
    output.historyColor = float4(
        saturate(historyColor.rgb), saturate(historyColor.a));
    output.sourceHistoryColor = float4(
        saturate(sourceHistoryColor), saturate(sourceHistoryGeometryConfidence));
    output.protectionState = float4(
        saturate(protectionStateLuma), saturate(protectionStateRisk),
        saturate(protectionStateRedAuthority), saturate(phaseHoldStateEncoded));
    return output;
}
)HLSL"
R"HLSL(
float4 PSAnalyze(VSOut i) : SV_TARGET
{
    // Approximate each analysis cell's average with nine samples spread across
    // the cell. A single center sample can miss narrow strobes or alias patterns.
    const float2 cell = float2(1.0 / 128.0, 1.0 / 72.0);
    const float2 d = cell * 0.30;
    float3 c = 0.0;
    c += SrgbToLinear(CurrentFrame.SampleLevel(LinearClamp, i.uv + float2(-d.x, -d.y), 0.0).rgb);
    c += SrgbToLinear(CurrentFrame.SampleLevel(LinearClamp, i.uv + float2( 0.0, -d.y), 0.0).rgb);
    c += SrgbToLinear(CurrentFrame.SampleLevel(LinearClamp, i.uv + float2( d.x, -d.y), 0.0).rgb);
    c += SrgbToLinear(CurrentFrame.SampleLevel(LinearClamp, i.uv + float2(-d.x,  0.0), 0.0).rgb);
    c += SrgbToLinear(CurrentFrame.SampleLevel(LinearClamp, i.uv, 0.0).rgb);
    c += SrgbToLinear(CurrentFrame.SampleLevel(LinearClamp, i.uv + float2( d.x,  0.0), 0.0).rgb);
    c += SrgbToLinear(CurrentFrame.SampleLevel(LinearClamp, i.uv + float2(-d.x,  d.y), 0.0).rgb);
    c += SrgbToLinear(CurrentFrame.SampleLevel(LinearClamp, i.uv + float2( 0.0,  d.y), 0.0).rgb);
    c += SrgbToLinear(CurrentFrame.SampleLevel(LinearClamp, i.uv + float2( d.x,  d.y), 0.0).rgb);
    return float4(c / 9.0, 1.0);
}

float AnalysisLuma(float3 linearColor)
{
    return dot(linearColor, float3(0.2126, 0.7152, 0.0722));
}

float MoveTowardsShader(float value, float target, float amount)
{
    return value < target ? min(value + amount, target) : max(value - amount, target);
}

struct InstantSafetyOutput
{
    float4 safety : SV_TARGET0;
    float4 temporal : SV_TARGET1;
};

InstantSafetyOutput PSInstantSafety(VSOut i)
{
    InstantSafetyOutput output;
    const float2 cell = float2(1.0 / 128.0, 1.0 / 72.0);
    const float3 cur = CurrentAnalysis.SampleLevel(LinearClamp, i.uv, 0.0).rgb;
    const float3 prev = PreviousAnalysis.SampleLevel(LinearClamp, i.uv, 0.0).rgb;
    const float4 temporalHistory = PreviousTemporal.SampleLevel(LinearClamp, i.uv, 0.0);
    const float curL = AnalysisLuma(cur);
    const float prevL = AnalysisLuma(prev);
    if (P5.z < 0.5)
    {
        output.safety = float4(curL, curL, 0.0, 0.0);
        output.temporal = float4(curL, 0.0, 0.0, 0.066);
        return output;
    }

    const float threshold = P4.x;
    const float dt = max(P5.w, 1.0 / 240.0);
    const float delta = curL - prevL;
    const float absDelta = abs(delta);
    const float direction = delta >= 0.0 ? 1.0 : -1.0;
    float affected = 0.0;
    float matching = 0.0;
    float patternEvidence = 0.0;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 uv = i.uv + float2(x, y) * cell;
            const float nc = AnalysisLuma(CurrentAnalysis.SampleLevel(LinearClamp, uv, 0.0).rgb);
            const float np = AnalysisLuma(PreviousAnalysis.SampleLevel(LinearClamp, uv, 0.0).rgb);
            const float nd = nc - np;
            if (abs(nd) >= threshold)
            {
                affected += 1.0;
                if ((nd >= 0.0 ? 1.0 : -1.0) == direction) matching += 1.0;
            }
            if (abs(nc - curL) >= 0.16)
                patternEvidence += 1.0;
        }
    }
    const float coherence = affected > 0.0 ? matching / affected : 0.0;

    // Fractional-cell translation search preserves the v1 scrolling fix.
    float bestError = absDelta;
    float2 bestOffset = float2(0.0, 0.0);
    [unroll]
    for (int ri = 0; ri < 7; ++ri)
    {
        const float radius = exp2((float)ri - 3.0);
        [unroll]
        for (int my = -1; my <= 1; ++my)
        {
            [unroll]
            for (int mx = -1; mx <= 1; ++mx)
            {
                if (mx != 0 || my != 0)
                {
                    const float2 offset = float2(mx, my) * cell * radius;
                    const float error = abs(curL - AnalysisLuma(
                        PreviousAnalysis.SampleLevel(LinearClamp, i.uv + offset, 0.0).rgb));
                    if (error < bestError)
                    {
                        bestError = error;
                        bestOffset = offset;
                    }
                }
            }
        }
    }

    // A single same-valued neighbor is not enough evidence for motion: a white
    // flash can otherwise "match" unrelated white background a few cells away.
    // Verify the winning sub-cell offset over a 3x3 PATCH, and require its
    // spatial structure to align better after the shift. This is deliberately
    // exposure-tolerant: scrolling text keeps the same local structure while a
    // luminance flash at fixed geometry generally aligns best at zero shift.
    float samePatchError = 0.0;
    float shiftedPatchError = 0.0;
    float sumCur = 0.0, sumSame = 0.0, sumShift = 0.0;
    float sumCur2 = 0.0, sumSame2 = 0.0, sumShift2 = 0.0;
    float sumCurSame = 0.0, sumCurShift = 0.0;
    [unroll]
    for (int py = -1; py <= 1; ++py)
    {
        [unroll]
        for (int px = -1; px <= 1; ++px)
        {
            const float2 patchOffset = float2(px, py) * cell;
            const float patchCur = AnalysisLuma(CurrentAnalysis.SampleLevel(
                LinearClamp, i.uv + patchOffset, 0.0).rgb);
            const float patchPrevSame = AnalysisLuma(PreviousAnalysis.SampleLevel(
                LinearClamp, i.uv + patchOffset, 0.0).rgb);
            const float patchPrevShift = AnalysisLuma(PreviousAnalysis.SampleLevel(
                LinearClamp, i.uv + patchOffset + bestOffset, 0.0).rgb);
            samePatchError += abs(patchCur - patchPrevSame);
            shiftedPatchError += abs(patchCur - patchPrevShift);
            sumCur += patchCur;
            sumSame += patchPrevSame;
            sumShift += patchPrevShift;
            sumCur2 += patchCur * patchCur;
            sumSame2 += patchPrevSame * patchPrevSame;
            sumShift2 += patchPrevShift * patchPrevShift;
            sumCurSame += patchCur * patchPrevSame;
            sumCurShift += patchCur * patchPrevShift;
        }
    }
    const float motionExplained = samePatchError > 0.002 ?
        saturate(1.0 - shiftedPatchError / samePatchError) : 0.0;
    const float invN = 1.0 / 9.0;
    const float varCur = max(0.0, sumCur2 - sumCur * sumCur * invN);
    const float varSame = max(0.0, sumSame2 - sumSame * sumSame * invN);
    const float varShift = max(0.0, sumShift2 - sumShift * sumShift * invN);
    const float covSame = sumCurSame - sumCur * sumSame * invN;
    const float covShift = sumCurShift - sumCur * sumShift * invN;
    const float corrSame = covSame * rsqrt(max(varCur * varSame, 1e-6));
    const float corrShift = covShift * rsqrt(max(varCur * varShift, 1e-6));
    const bool structuredPatch = varCur >= 0.002 && varShift >= 0.002;
    const bool structuralShift = corrShift >= 0.78 &&
        (corrShift >= corrSame + 0.015 || motionExplained >= 0.65);
    const bool translatedMotion = structuredPatch && structuralShift &&
        motionExplained >= max(P5.x, 0.34);

)HLSL" R"HLSL(

    const float curRedRatio = cur.r / max(cur.r + cur.g + cur.b, 0.0001);
    const float prevRedRatio = prev.r / max(prev.r + prev.g + prev.b, 0.0001);
    const bool redTransition = max(curRedRatio, prevRedRatio) >= 0.80 &&
        length(cur - prev) >= P5.y;

    float globalAffected = 0.0;
    float globalPositive = 0.0;
    float globalNegative = 0.0;
    [unroll]
    for (int gy = 0; gy < 8; ++gy)
    {
        [unroll]
        for (int gx = 0; gx < 8; ++gx)
        {
            const float2 guv = (float2(gx, gy) + 0.5) / 8.0;
            const float gd = AnalysisLuma(CurrentAnalysis.SampleLevel(LinearClamp, guv, 0.0).rgb) -
                AnalysisLuma(PreviousAnalysis.SampleLevel(LinearClamp, guv, 0.0).rgb);
            if (abs(gd) >= threshold)
            {
                globalAffected += 1.0;
                globalPositive += gd > 0.0 ? 1.0 : 0.0;
                globalNegative += gd < 0.0 ? 1.0 : 0.0;
            }
        }
    }
    const bool globalFlash = globalAffected >= 42.0 &&
        max(globalPositive, globalNegative) / max(globalAffected, 1.0) >= 0.75;

    // Track the start of a monotonic transition for ~66 ms. This catches a
    // hazardous change spread over several high-refresh frames instead of only
    // comparing adjacent frames.
    float anchorL = temporalHistory.r;
    // Flash memory bridges real repetitive reversals, but current-event/source
    // separation below prevents that memory from ghosting unrelated motion.
    // Decay and sustained-event accumulation are both integrated in seconds.
    // This prevents a 120/240 Hz stream from adding the same event energy two or
    // four times as often as 60 Hz while preserving the existing decay lifetime.
    const float riskDecayTau = 0.55;
    const float riskDecay = exp(-dt / riskDecayTau);
    float riskEnergy = saturate(temporalHistory.g * riskDecay);
    const float previousDirection = temporalHistory.b;
    float transitionAge = temporalHistory.a + dt;
    const bool meaningfulMotion = absDelta >= 0.002;
    const bool directionReversal = meaningfulMotion && previousDirection * direction < -0.5;

    if (!meaningfulMotion)
    {
        if (transitionAge > 0.066)
        {
            anchorL = curL;
            transitionAge = 0.066;
        }
    }
    else if (previousDirection == 0.0 || directionReversal || transitionAge > 0.066)
    {
        anchorL = prevL;
        transitionAge = dt;
    }

    const float cumulativeDelta = abs(curL - anchorL);
    const float qualifyingDelta = max(absDelta, cumulativeDelta);
    const float transitionStrength = smoothstep(
        max(0.055, threshold * 0.65), max(0.16, threshold * 1.55), qualifyingDelta);
    const bool withinTransitionWindow = transitionAge <= 0.066 + dt * 0.5;
    const bool enoughSpatialSupport = affected >= 2.0 ||
        coherence >= 0.55 || qualifyingDelta >= max(0.18, threshold * 1.4);
    const bool motionSuppressed = translatedMotion && !globalFlash && !redTransition &&
        !(directionReversal && temporalHistory.g >= 0.08);
    const bool rapidTransition = withinTransitionWindow &&
        qualifyingDelta >= max(0.075, threshold * 0.80) && enoughSpatialSupport;
    const bool transitionEvent = !motionSuppressed &&
        (globalFlash || rapidTransition || (redTransition && !translatedMotion));

    if (transitionEvent)
    {
        // The old boost was tuned as a 60 Hz per-frame increment. Treat it as a
        // continuous rate and integrate it analytically through the same decay,
        // so an equal wall-clock event produces the same memory at every FPS.
        const float eventRate =
            (0.24 + 0.44 * transitionStrength) * 60.0;
        riskEnergy += eventRate * riskDecayTau * (1.0 - riskDecay);
        // A direction reversal is a discrete physical event, not a rate.
        if (directionReversal && temporalHistory.g >= 0.10)
            riskEnergy += 0.36;
        riskEnergy = saturate(riskEnergy);
    }

    // Repetitive flashes become progressively harder to pass. The risk energy
    // also keeps a genuine transition in recovery long enough for displayed
    // luminance to converge smoothly instead of snapping back when detection
    // momentarily drops out.
    float riskStrength = max(transitionEvent ? transitionStrength : 0.0,
        smoothstep(0.10, 0.52, riskEnergy));

    if (motionSuppressed)
    {
        // Scrolling/panning wins immediately over stale temporal state.
        riskEnergy = 0.0;
        riskStrength = 0.0;
        anchorL = curL;
        transitionAge = 0.066;
    }

    // Pattern evidence cannot create a spatial pattern in the output; it only
    // increases temporal filtering where the underlying pixels are changing.
    const bool patternRisk = patternEvidence >= 4.0 && affected >= 3.0 && !translatedMotion;
    if (patternRisk)
    {
        riskEnergy = max(riskEnergy, 0.55);
        riskStrength = max(riskStrength, 0.80);
    }

    // G carries signed CURRENT state to the full-resolution temporal stage:
    // positive = hazard now; negative = coherent translation now. Keeping this
    // separate from B (accumulated risk memory) prevents stale protection from
    // ghosting ordinary motion after a flash has ended.
    float currentEventStrength = transitionEvent ? max(transitionStrength, 0.18) : 0.0;
    if (patternRisk) currentEventStrength = max(currentEventStrength, 0.80);
    if (globalFlash) currentEventStrength = max(currentEventStrength, 1.0);
    const float signedCurrentState = motionSuppressed ? -1.0 : currentEventStrength;
)HLSL" R"HLSL(    output.safety = float4(curL, signedCurrentState, saturate(riskStrength),
        redTransition && !translatedMotion ? 1.0 : 0.0);
    output.temporal = float4(anchorL, saturate(riskEnergy),
        meaningfulMotion ? direction : previousDirection, transitionAge);
    return output;
}
)HLSL";

    bool HasCommandLineFlag(const wchar_t* flag)
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv) return false;
        bool found = false;
        for (int i = 1; i < argc; ++i)
            if (_wcsicmp(argv[i], flag) == 0) found = true;
        LocalFree(argv);
        return found;
    }

    bool ValidateShaderSource()
    {
        struct ShaderTarget { const char* entry; const char* profile; };
        constexpr ShaderTarget targets[] = {
            { "VSMain", "vs_5_0" }, { "PSMain", "ps_5_0" },
            { "PSAnalyze", "ps_5_0" }, { "PSInstantSafety", "ps_5_0" },
            { "PSOpticalFlowCopy", "ps_5_0" }
        };
        for (const auto& target : targets)
        {
            winrt::com_ptr<ID3DBlob> blob, errors;
            const HRESULT hr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1,
                "FlashGuard", nullptr, nullptr, target.entry, target.profile,
                D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, blob.put(), errors.put());
            if (FAILED(hr))
            {
                if (errors) OutputDebugStringA(
                    static_cast<const char*>(errors->GetBufferPointer()));
                if (errors)
                {
                    HANDLE file = CreateFileW(L"FlashGuard-shader-errors.txt", GENERIC_WRITE,
                        FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (file != INVALID_HANDLE_VALUE)
                    {
                        DWORD written = 0;
                        WriteFile(file, errors->GetBufferPointer(),
                            static_cast<DWORD>(errors->GetBufferSize()), &written, nullptr);
                        CloseHandle(file);
                    }
                }
                return false;
            }
        }
        return true;
    }

    bool ValidateRiskIntegrator()
    {
        // Two identical 1/30 s event windows separated by 4/30 s. The second
        // begins with one discrete reversal impulse. All boundaries land exactly
        // on frame boundaries at 30, 60, 120, and 240 Hz, so dt is the only
        // variable and the final energy should be invariant.
        const auto simulate = [](int fps) {
            constexpr double tau = 0.55;
            constexpr double rate = 0.24 * 60.0;
            double risk = 0.0;
            const double dt = 1.0 / static_cast<double>(fps);
            const int frameCount = fps;
            const int firstWindowFrames = fps / 30;
            const int secondWindowStart = fps * 5 / 30;
            const int secondWindowEnd = fps * 6 / 30;

            for (int frame = 0; frame < frameCount; ++frame)
            {
                const double previousRisk = risk;
                const double decay = std::exp(-dt / tau);
                risk = previousRisk * decay;
                const bool event =
                    frame < firstWindowFrames ||
                    (frame >= secondWindowStart && frame < secondWindowEnd);
                if (event)
                {
                    risk += rate * tau * (1.0 - decay);
                    const bool reversal = frame == secondWindowStart;
                    if (reversal && previousRisk >= 0.10)
                        risk += 0.36;
                    risk = std::clamp(risk, 0.0, 1.0);
                }
            }
            return risk;
        };

        const double baseline = simulate(60);
        constexpr std::array<int, 4> frameRates{ 30, 60, 120, 240 };
        for (const int fps : frameRates)
        {
            const double result = simulate(fps);
            if (!std::isfinite(result) || std::fabs(result - baseline) > 1e-6)
            {
                std::fprintf(stderr,
                    "risk integrator invariance failed at %d Hz: %.9f vs %.9f\n",
                    fps, result, baseline);
                return false;
            }
        }
        return true;
    }

    class FlashGuardApp
    {
    public:
        ~FlashGuardApp() { Stop(); }

        void Initialize(HWND output, HMONITOR monitor,
                        HWND startupStatus = nullptr, HWND startupProgress = nullptr)
        {
            const auto setStartupProgress = [&](int percent, const wchar_t* text)
            {
                if (startupStatus && text)
                {
                    std::wstring label;
                    if (g_liveRawPassthroughForLatencyTest)
                        label = L"RAW PASSTHROUGH - NO PROTECTION\n";
                    label += text;
                    SetWindowTextW(startupStatus, label.c_str());
                    RedrawWindow(startupStatus, nullptr, nullptr,
                        RDW_INVALIDATE | RDW_UPDATENOW);
                }
                if (startupProgress)
                {
                    SendMessageW(startupProgress, PBM_SETPOS,
                        static_cast<WPARAM>(std::clamp(percent, 0, 100)), 0);
                    UpdateWindow(startupProgress);
                }
            };

            m_output = output;
            m_monitor = monitor;
            m_debugEnabled.store(m_safety.debugOverlay, std::memory_order_release);

            setStartupProgress(5, L"Selecting display and GPU...");
            FindOutputAndCreateDevice();

            setStartupProgress(15, L"Creating Direct3D presentation path...");
            winrtlessEnableMultithreadProtection();
            CreateSwapChain();

            setStartupProgress(24, L"Compiling protection shaders...");
            CreatePipeline();

            setStartupProgress(48, L"Allocating analysis resources...");
            CreateAnalysisResources();

            setStartupProgress(58, L"Preparing output history...");
            RecreateOutputResources();

            setStartupProgress(68, L"Preparing safety and diagnostics...");
            CreateShieldTexture();
            CreateDebugResources();

            setStartupProgress(76, L"Preparing overlay resources...");
            CreateHintResources();
            ClearAllToBlack();

            setStartupProgress(84, L"Starting Desktop Duplication...");
            CreateDuplication();

            m_lastFrameMs.store(NowMs(), std::memory_order_release);
            m_captureThread = std::thread([this] { CaptureLoop(); });
            setStartupProgress(90, L"Warming up desktop capture...");
        }

        void InitializeReplay(HWND output, HMONITOR monitor)
        {
            m_output = output;
            m_monitor = monitor;
            m_replayMode = true;
            m_replayClockSeconds = 0.0;
            m_debugEnabled.store(false, std::memory_order_release);
            FindOutputAndCreateDevice();

            winrtlessEnableMultithreadProtection();
            CreateSwapChain();
            CreatePipeline();
            CreateAnalysisResources();
            RecreateOutputResources();
            CreateShieldTexture();
            CreateDebugResources();
            CreateHintResources();
            m_hintUntilMs = 0;
            ClearAllToBlack();

            D3D11_TEXTURE2D_DESC staging{};
            m_outputHistoryTextures[0]->GetDesc(&staging);
            staging.Usage = D3D11_USAGE_STAGING;
            staging.BindFlags = 0;
            staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            staging.MiscFlags = 0;
            ThrowIfFailed(m_device->CreateTexture2D(
                &staging, nullptr, m_replayReadback.put()));

            // Replay-only full-resolution diagnostic planes. Spatial coverage is
            // more important than replay render-target bandwidth: each of the 12
            // decision metrics now exists at every output pixel.
            D3D11_TEXTURE2D_DESC diagnostic{};
            m_outputHistoryTextures[0]->GetDesc(&diagnostic);
            diagnostic.Usage = D3D11_USAGE_DEFAULT;
            diagnostic.BindFlags = D3D11_BIND_RENDER_TARGET;
            diagnostic.CPUAccessFlags = 0;
            diagnostic.MiscFlags = 0;
            for (size_t group = 0; group < m_motionDiagnosticTextures.size(); ++group)
            {
                ThrowIfFailed(m_device->CreateTexture2D(
                    &diagnostic, nullptr, m_motionDiagnosticTextures[group].put()));
                ThrowIfFailed(m_device->CreateRenderTargetView(
                    m_motionDiagnosticTextures[group].get(), nullptr,
                    m_motionDiagnosticRTVs[group].put()));
            }
            diagnostic.Usage = D3D11_USAGE_STAGING;
            diagnostic.BindFlags = 0;
            diagnostic.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            for (size_t group = 0; group < m_motionDiagnosticReadbacks.size(); ++group)
            {
                ThrowIfFailed(m_device->CreateTexture2D(
                    &diagnostic, nullptr, m_motionDiagnosticReadbacks[group].put()));
            }
        }

        bool RunSyntheticReplay(const std::wstring& reportPath,
                                const std::wstring& visualDir = L"",
                                int replayFps = 60,
                                float motionScale = 1.0f,
                                bool replayScreening = false)
        {
            if (!m_replayMode || !m_device || !m_context || !m_replayReadback ||
                !m_motionDiagnosticTextures[0] || !m_motionDiagnosticReadbacks[0] ||
                m_outputWidth == 0 || m_outputHeight == 0)
                return false;

            replayFps = std::clamp(replayFps, 30, 240);
            motionScale = std::clamp(motionScale, 0.25f, 4.0f);
            const float dt = 1.0f / static_cast<float>(replayFps);
            const double motionFrameScale =
                60.0 / static_cast<double>(replayFps) *
                static_cast<double>(motionScale);
            // Screening is deliberately cheap: lower-duration cases and a sparser
            // general metric grid. Finalists still run the canonical full replay.
            const int warmupFrames = replayScreening ?
                std::max(4, replayFps / 8) : std::max(8, replayFps / 3);
            const int settleFrames = replayScreening ?
                std::max(6, replayFps / 4) : std::max(12, replayFps / 2);
            const UINT width = m_outputWidth;
            const UINT height = m_outputHeight;
            const UINT sampleStride = replayScreening ? 8u : 4u;
            std::vector<uint32_t> pixels(static_cast<size_t>(width) * height);
            std::vector<uint32_t> previousDiagnosticSource(pixels.size());
            bool previousDiagnosticSourceValid = false;
            const int diagnosticSampleInterval = replayScreening ?
                std::max(1, replayFps / 6) : std::max(1, replayFps / 30);
            uint64_t diagnosticRequestCount = 0;
            const bool writeVisuals = !visualDir.empty();
            if (writeVisuals)
            {
                std::filesystem::create_directories(visualDir);
                const auto readmePath =
                    std::filesystem::path(visualDir) / L"README.txt";
                FILE* visualReadme = nullptr;
                if (_wfopen_s(&visualReadme, readmePath.c_str(), L"wb") == 0 &&
                    visualReadme)
                {
                    std::fputs("Each BMP is SOURCE | FILTERED | 6x RGB DIFFERENCE.\r\n"
                        "Sampling cadence follows the selected synthetic replay FPS.\r\n",
                        visualReadme);
                    std::fclose(visualReadme);
                }
            }

            D3D11_TEXTURE2D_DESC sourceDesc{};
            sourceDesc.Width = width;
            sourceDesc.Height = height;
            sourceDesc.MipLevels = 1;
            sourceDesc.ArraySize = 1;
            sourceDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            sourceDesc.SampleDesc.Count = 1;
            sourceDesc.Usage = D3D11_USAGE_DEFAULT;
            winrt::com_ptr<ID3D11Texture2D> source;
            ThrowIfFailed(m_device->CreateTexture2D(
                &sourceDesc, nullptr, source.put()));

            const auto grayPixel = [](uint8_t value) {
                const uint32_t v = value;
                return v | (v << 8) | (v << 16) | 0xFF000000u;
            };
            const auto rgbPixel = [](uint8_t r, uint8_t g, uint8_t b) {
                return static_cast<uint32_t>(b) |
                    (static_cast<uint32_t>(g) << 8) |
                    (static_cast<uint32_t>(r) << 16) | 0xFF000000u;
            };
            const auto srgbToLinear = [](double value) {
                value = std::clamp(value, 0.0, 1.0);
                return value <= 0.04045 ? value / 12.92 :
                    std::pow((value + 0.055) / 1.055, 2.4);
            };
            const auto fillGray = [&](uint8_t value) {
                std::fill(pixels.begin(), pixels.end(), grayPixel(value));
            };
            const auto pingPongCoordinate = [](int origin, double travel,
                                               int maxPosition) {
                maxPosition = std::max(maxPosition, origin);
                const double span = static_cast<double>(maxPosition - origin);
                if (span <= 0.0) return origin;
                const double period = span * 2.0;
                double phase = std::fmod(std::max(travel, 0.0), period);
                if (phase > span) phase = period - phase;
                return origin + static_cast<int>(std::lround(phase));
            };
            const auto fillPanPattern = [&](int offset) {
                for (UINT y = 0; y < height; ++y)
                {
                    for (UINT x = 0; x < width; ++x)
                    {
                        const UINT sx = (x + static_cast<UINT>(offset)) % width;
                        uint8_t value = (((sx / 16u) ^ (y / 16u)) & 1u) ? 210u : 42u;
                        if (((sx / 7u) + (y / 11u)) % 9u == 0u) value = 126u;
                        pixels[static_cast<size_t>(y) * width + x] = grayPixel(value);
                    }
                }
            };

            // Render a real small-font desktop-text surface once, then translate
            // that raster either smoothly at fractional-pixel offsets or through
            // integer snapping. This separates optical-flow behavior from the
            // artificial move/stall cadence caused by lround() in older cases.
            std::vector<uint32_t> textPage(pixels.size(), grayPixel(244));
            {
                BITMAPINFO bmi{};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = static_cast<LONG>(width);
                bmi.bmiHeader.biHeight = -static_cast<LONG>(height);
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;
                void* bits = nullptr;
                HDC dc = CreateCompatibleDC(nullptr);
                HBITMAP bitmap = dc ? CreateDIBSection(
                    dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
                if (dc && bitmap && bits)
                {
                    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
                    RECT page{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
                    HBRUSH background = CreateSolidBrush(RGB(244, 244, 244));
                    FillRect(dc, &page, background);
                    DeleteObject(background);
                    HFONT font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
                    HGDIOBJ oldFont = SelectObject(dc, font);
                    SetBkMode(dc, TRANSPARENT);
                    SetTextColor(dc, RGB(22, 22, 22));
                    RECT textRect{ 18, 8, static_cast<LONG>(width) - 18,
                        static_cast<LONG>(height) - 8 };
                    DrawTextW(dc,
                        L"FlashGuard scrolling text regression\r\n"
                        L"small glyph edges should remain crisp while moving\r\n"
                        L"0123456789  ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n"
                        L"motion motion motion  contrast contrast contrast\r\n"
                        L"The quick brown fox jumps over the lazy dog.\r\n"
                        L"scroll stop recovery must not wait for cursor motion.",
                        -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
                    memcpy(textPage.data(), bits, textPage.size() * sizeof(uint32_t));
                    for (uint32_t& pixel : textPage) pixel |= 0xFF000000u;
                    SelectObject(dc, oldFont);
                    SelectObject(dc, oldBitmap);
                    DeleteObject(font);
                    DeleteObject(bitmap);
                    DeleteDC(dc);
                }
                else
                {
                    if (bitmap) DeleteObject(bitmap);
                    if (dc) DeleteDC(dc);
                }
            }
            const auto lerpPackedPixel = [](uint32_t a, uint32_t b, double t) {
                uint32_t result = 0xFF000000u;
                for (int shift : { 0, 8, 16 })
                {
                    const double av = static_cast<double>((a >> shift) & 0xFFu);
                    const double bv = static_cast<double>((b >> shift) & 0xFFu);
                    const uint32_t value = static_cast<uint32_t>(std::lround(
                        av + (bv - av) * std::clamp(t, 0.0, 1.0)));
                    result |= std::min(value, 255u) << shift;
                }
                return result;
            };
            const auto fillTextScroll = [&](double offsetPixels, bool smooth) {
                double wrapped = std::fmod(offsetPixels, static_cast<double>(height));
                if (wrapped < 0.0) wrapped += static_cast<double>(height);
                const int base = smooth ? static_cast<int>(std::floor(wrapped)) :
                    static_cast<int>(std::lround(wrapped)) % static_cast<int>(height);
                const double fraction = smooth ? wrapped - std::floor(wrapped) : 0.0;
                for (UINT y = 0; y < height; ++y)
                {
                    const UINT sy0 = static_cast<UINT>((base + static_cast<int>(y)) %
                        static_cast<int>(height));
                    const UINT sy1 = (sy0 + 1u) % height;
                    for (UINT x = 0; x < width; ++x)
                    {
                        const uint32_t a = textPage[static_cast<size_t>(sy0) * width + x];
                        const uint32_t b = textPage[static_cast<size_t>(sy1) * width + x];
                        pixels[static_cast<size_t>(y) * width + x] =
                            smooth ? lerpPackedPixel(a, b, fraction) : a;
                    }
                }
            };
            const auto halfToFloat = [](uint16_t h) {
                const uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16;
                const uint32_t exponent = (h >> 10) & 0x1Fu;
                uint32_t mantissa = h & 0x03FFu;
                uint32_t bits = 0;
                if (exponent == 0)
                {
                    if (mantissa == 0)
                    {
                        bits = sign;
                    }
                    else
                    {
                        int e = -14;
                        while ((mantissa & 0x0400u) == 0)
                        {
                            mantissa <<= 1;
                            --e;
                        }
                        mantissa &= 0x03FFu;
                        bits = sign |
                            (static_cast<uint32_t>(e + 127) << 23) |
                            (mantissa << 13);
                    }
                }
                else if (exponent == 31)
                {
                    bits = sign | 0x7F800000u | (mantissa << 13);
                }
                else
                {
                    bits = sign | ((exponent + 112u) << 23) | (mantissa << 13);
                }
                float value = 0.0f;
                memcpy(&value, &bits, sizeof(value));
                return value;
            };
            const auto writeVisualBmp = [&](const wchar_t* caseName, int frameIndex,
                                            const D3D11_MAPPED_SUBRESOURCE& mapped) {
                if (!writeVisuals || !caseName || frameIndex < 0) return;
                const auto caseDir = std::filesystem::path(visualDir) / caseName;
                std::filesystem::create_directories(caseDir);

                const UINT triptychWidth = width * 3u;
                std::vector<uint32_t> image(
                    static_cast<size_t>(triptychWidth) * height, 0xFF000000u);
                const auto toByte = [](float value) {
                    return static_cast<uint8_t>(std::lround(
                        std::clamp(value, 0.0f, 1.0f) * 255.0f));
                };
                for (UINT y = 0; y < height; ++y)
                {
                    const auto* row = reinterpret_cast<const uint16_t*>(
                        static_cast<const uint8_t*>(mapped.pData) +
                        static_cast<size_t>(y) * mapped.RowPitch);
                    for (UINT x = 0; x < width; ++x)
                    {
                        const uint32_t sourcePixel =
                            pixels[static_cast<size_t>(y) * width + x];
                        const uint8_t outR = toByte(halfToFloat(row[x * 4 + 0]));
                        const uint8_t outG = toByte(halfToFloat(row[x * 4 + 1]));
                        const uint8_t outB = toByte(halfToFloat(row[x * 4 + 2]));
                        const uint32_t filteredPixel = static_cast<uint32_t>(outB) |
                            (static_cast<uint32_t>(outG) << 8) |
                            (static_cast<uint32_t>(outR) << 16) | 0xFF000000u;
                        const int sourceB = static_cast<int>(sourcePixel & 0xFFu);
                        const int sourceG = static_cast<int>((sourcePixel >> 8) & 0xFFu);
                        const int sourceR = static_cast<int>((sourcePixel >> 16) & 0xFFu);
                        const uint8_t diffB = static_cast<uint8_t>(std::min(
                            255, std::abs(static_cast<int>(outB) - sourceB) * 6));
                        const uint8_t diffG = static_cast<uint8_t>(std::min(
                            255, std::abs(static_cast<int>(outG) - sourceG) * 6));
                        const uint8_t diffR = static_cast<uint8_t>(std::min(
                            255, std::abs(static_cast<int>(outR) - sourceR) * 6));
                        const uint32_t diffPixel = static_cast<uint32_t>(diffB) |
                            (static_cast<uint32_t>(diffG) << 8) |
                            (static_cast<uint32_t>(diffR) << 16) | 0xFF000000u;
                        const size_t dst = static_cast<size_t>(y) * triptychWidth + x;
                        image[dst] = sourcePixel;
                        image[dst + width] = filteredPixel;
                        image[dst + width * 2u] = diffPixel;
                    }
                }

                wchar_t fileName[64]{};
                swprintf_s(fileName, L"frame-%03d.bmp", frameIndex);
                const auto filePath = caseDir / fileName;
                BITMAPFILEHEADER fileHeader{};
                BITMAPINFOHEADER infoHeader{};
                fileHeader.bfType = 0x4D42;
                fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
                fileHeader.bfSize = fileHeader.bfOffBits +
                    static_cast<DWORD>(image.size() * sizeof(uint32_t));
                infoHeader.biSize = sizeof(BITMAPINFOHEADER);
                infoHeader.biWidth = static_cast<LONG>(triptychWidth);
                infoHeader.biHeight = -static_cast<LONG>(height);
                infoHeader.biPlanes = 1;
                infoHeader.biBitCount = 32;
                infoHeader.biCompression = BI_RGB;
                FILE* file = nullptr;
                if (_wfopen_s(&file, filePath.c_str(), L"wb") != 0 || !file) return;
                std::fwrite(&fileHeader, sizeof(fileHeader), 1, file);
                std::fwrite(&infoHeader, sizeof(infoHeader), 1, file);
                std::fwrite(image.data(), sizeof(uint32_t), image.size(), file);
                std::fclose(file);
            };

            struct MotionDiagnosticAggregate
            {
                std::array<double, 12> sum{};
                std::array<double, 12> maximum{};
                std::array<uint64_t, 12> active{};
                std::array<uint64_t, 12> count{};
                std::array<double, 12> insideSum{};
                std::array<double, 12> insideMaximum{};
                std::array<uint64_t, 12> insideActive{};
                std::array<uint64_t, 12> insideCount{};
                std::array<double, 12> outsideSum{};
                std::array<double, 12> outsideMaximum{};
                std::array<uint64_t, 12> outsideActive{};
                std::array<uint64_t, 12> outsideCount{};
                std::array<double, 12> changedSum{};
                std::array<double, 12> changedMaximum{};
                std::array<uint64_t, 12> changedActive{};
                std::array<uint64_t, 12> changedCount{};
            };

            struct AuthorityDiagnosticAggregate
            {
                std::array<double, 4> sampleSum{};
                std::array<uint64_t, 4> sampleActive{};
                std::array<double, 4> sum{};
                std::array<double, 4> maximum{};
                std::array<double, 4> errorWeightedSum{};
                std::array<uint64_t, 4> active{};
                std::array<double, 8> geometrySampleSum{};
                std::array<uint64_t, 8> geometrySampleActive{};
                std::array<double, 8> geometrySum{};
                std::array<double, 8> geometryMaximum{};
                std::array<double, 8> geometryErrorWeightedSum{};
                std::array<uint64_t, 8> geometryActive{};
                uint64_t trailPixels = 0;
                uint64_t errorPixels = 0;
                uint64_t unexplainedErrorPixels = 0;
                double errorSum = 0.0;
            };

            const auto collectMotionDiagnostics =
                [&](MotionDiagnosticAggregate& aggregate, const RECT* activeRect)
            {
                if (!previousDiagnosticSourceValid) return;
                const size_t groupCount = m_benchmarkArchitectureMode > 2 ?
                    2u : m_motionDiagnosticTextures.size();
                for (size_t group = 0; group < groupCount; ++group)
                {
                    m_context->CopyResource(
                        m_motionDiagnosticReadbacks[group].get(),
                        m_motionDiagnosticTextures[group].get());
                    D3D11_MAPPED_SUBRESOURCE mapped{};
                    ThrowIfFailed(m_context->Map(
                        m_motionDiagnosticReadbacks[group].get(), 0,
                        D3D11_MAP_READ, 0, &mapped));
                    for (UINT y = 0; y < height; ++y)
                    {
                        const auto* row = reinterpret_cast<const uint16_t*>(
                            static_cast<const uint8_t*>(mapped.pData) +
                            static_cast<size_t>(y) * mapped.RowPitch);
                        for (UINT x = 0; x < width; ++x)
                        {
                            const size_t pixelIndex = static_cast<size_t>(y) * width + x;
                            const uint32_t currentPacked = pixels[pixelIndex];
                            const uint32_t previousPacked = previousDiagnosticSource[pixelIndex];
                            const int sourceRgbDelta =
                                std::abs(static_cast<int>(currentPacked & 0xFFu) -
                                    static_cast<int>(previousPacked & 0xFFu)) +
                                std::abs(static_cast<int>((currentPacked >> 8) & 0xFFu) -
                                    static_cast<int>((previousPacked >> 8) & 0xFFu)) +
                                std::abs(static_cast<int>((currentPacked >> 16) & 0xFFu) -
                                    static_cast<int>((previousPacked >> 16) & 0xFFu));
                            const bool changed = sourceRgbDelta >= 12;
                            const bool inside = activeRect &&
                                static_cast<LONG>(x) >= activeRect->left &&
                                static_cast<LONG>(x) < activeRect->right &&
                                static_cast<LONG>(y) >= activeRect->top &&
                                static_cast<LONG>(y) < activeRect->bottom;
                            for (size_t channel = 0; channel < 4; ++channel)
                            {
                                const size_t metric = group * 4 + channel;
                                const double value = std::clamp(
                                    static_cast<double>(halfToFloat(
                                        row[static_cast<size_t>(x) * 4 + channel])),
                                    0.0, 1.0);
                                aggregate.sum[metric] += value;
                                aggregate.maximum[metric] =
                                    std::max(aggregate.maximum[metric], value);
                                aggregate.active[metric] += value > 0.5 ? 1u : 0u;
                                ++aggregate.count[metric];
                                if (inside)
                                {
                                    aggregate.insideSum[metric] += value;
                                    aggregate.insideMaximum[metric] =
                                        std::max(aggregate.insideMaximum[metric], value);
                                    aggregate.insideActive[metric] += value > 0.5 ? 1u : 0u;
                                    ++aggregate.insideCount[metric];
                                }
                                else
                                {
                                    aggregate.outsideSum[metric] += value;
                                    aggregate.outsideMaximum[metric] =
                                        std::max(aggregate.outsideMaximum[metric], value);
                                    aggregate.outsideActive[metric] += value > 0.5 ? 1u : 0u;
                                    ++aggregate.outsideCount[metric];
                                }
                                if (changed)
                                {
                                    aggregate.changedSum[metric] += value;
                                    aggregate.changedMaximum[metric] =
                                        std::max(aggregate.changedMaximum[metric], value);
                                    aggregate.changedActive[metric] += value > 0.5 ? 1u : 0u;
                                    ++aggregate.changedCount[metric];
                                }
                            }
                        }
                    }
                    m_context->Unmap(m_motionDiagnosticReadbacks[group].get(), 0);
                }
            };

            const auto collectAuthorityDiagnostics =
                [&](AuthorityDiagnosticAggregate& aggregate, const RECT* trailRect,
                    const RECT* activeRect)
            {
                if (!trailRect || m_benchmarkArchitectureMode <= 2 ||
                    !m_motionDiagnosticTextures[2] || !m_motionDiagnosticReadbacks[2])
                    return;
                m_context->CopyResource(m_motionDiagnosticReadbacks[0].get(),
                    m_motionDiagnosticTextures[0].get());
                m_context->CopyResource(m_motionDiagnosticReadbacks[1].get(),
                    m_motionDiagnosticTextures[1].get());
                m_context->CopyResource(m_motionDiagnosticReadbacks[2].get(),
                    m_motionDiagnosticTextures[2].get());
                D3D11_MAPPED_SUBRESOURCE outputMapped{};
                D3D11_MAPPED_SUBRESOURCE geometry0Mapped{};
                D3D11_MAPPED_SUBRESOURCE geometry1Mapped{};
                D3D11_MAPPED_SUBRESOURCE authorityMapped{};
                ThrowIfFailed(m_context->Map(m_replayReadback.get(), 0,
                    D3D11_MAP_READ, 0, &outputMapped));
                ThrowIfFailed(m_context->Map(m_motionDiagnosticReadbacks[0].get(), 0,
                    D3D11_MAP_READ, 0, &geometry0Mapped));
                ThrowIfFailed(m_context->Map(m_motionDiagnosticReadbacks[1].get(), 0,
                    D3D11_MAP_READ, 0, &geometry1Mapped));
                ThrowIfFailed(m_context->Map(m_motionDiagnosticReadbacks[2].get(), 0,
                    D3D11_MAP_READ, 0, &authorityMapped));

                const LONG left = std::max<LONG>(0, trailRect->left);
                const LONG top = std::max<LONG>(0, trailRect->top);
                const LONG right = std::min<LONG>(static_cast<LONG>(width), trailRect->right);
                const LONG bottom = std::min<LONG>(static_cast<LONG>(height), trailRect->bottom);
                for (LONG y = top; y < bottom; ++y)
                {
                    const auto* outputRow = reinterpret_cast<const uint16_t*>(
                        static_cast<const uint8_t*>(outputMapped.pData) +
                        static_cast<size_t>(y) * outputMapped.RowPitch);
                    const auto* authorityRow = reinterpret_cast<const uint16_t*>(
                        static_cast<const uint8_t*>(authorityMapped.pData) +
                        static_cast<size_t>(y) * authorityMapped.RowPitch);
                    const auto* geometry0Row = reinterpret_cast<const uint16_t*>(
                        static_cast<const uint8_t*>(geometry0Mapped.pData) +
                        static_cast<size_t>(y) * geometry0Mapped.RowPitch);
                    const auto* geometry1Row = reinterpret_cast<const uint16_t*>(
                        static_cast<const uint8_t*>(geometry1Mapped.pData) +
                        static_cast<size_t>(y) * geometry1Mapped.RowPitch);
                    for (LONG x = left; x < right; ++x)
                    {
                        const bool stillOccupied = activeRect &&
                            x >= activeRect->left && x < activeRect->right &&
                            y >= activeRect->top && y < activeRect->bottom;
                        if (stillOccupied) continue;
                        ++aggregate.trailPixels;
                        const size_t base = static_cast<size_t>(x) * 4;
                        const float r = std::clamp(halfToFloat(outputRow[base + 0]), 0.0f, 1.0f);
                        const float g = std::clamp(halfToFloat(outputRow[base + 1]), 0.0f, 1.0f);
                        const float b = std::clamp(halfToFloat(outputRow[base + 2]), 0.0f, 1.0f);
                        const double outputLuma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
                        const uint32_t packed = pixels[
                            static_cast<size_t>(y) * width + static_cast<UINT>(x)];
                        const float sb = static_cast<float>(packed & 0xFFu) / 255.0f;
                        const float sg = static_cast<float>((packed >> 8) & 0xFFu) / 255.0f;
                        const float sr = static_cast<float>((packed >> 16) & 0xFFu) / 255.0f;
                        const double sourceLuma = 0.2126 * sr + 0.7152 * sg + 0.0722 * sb;
                        const double error = std::fabs(outputLuma - sourceLuma);
                        std::array<double, 4> authorityValues{};
                        for (size_t channel = 0; channel < 4; ++channel)
                        {
                            const double value = std::clamp(static_cast<double>(halfToFloat(
                                authorityRow[base + channel])), 0.0, 1.0);
                            authorityValues[channel] = value;
                            aggregate.sampleSum[channel] += value;
                            const double threshold = channel < 2 ? 0.01 : 0.05;
                            if (value > threshold) ++aggregate.sampleActive[channel];
                        }
                        std::array<double, 8> geometryValues{};
                        const uint16_t* geometryRows[2] = { geometry0Row, geometry1Row };
                        for (size_t group = 0; group < 2; ++group)
                        {
                            for (size_t channel = 0; channel < 4; ++channel)
                            {
                                const size_t metric = group * 4 + channel;
                                const double value = std::clamp(static_cast<double>(
                                    halfToFloat(geometryRows[group][base + channel])),
                                    0.0, 1.0);
                                geometryValues[metric] = value;
                                aggregate.geometrySampleSum[metric] += value;
                                if (value > 0.5) ++aggregate.geometrySampleActive[metric];
                            }
                        }
                        if (error <= 0.05) continue;
                        ++aggregate.errorPixels;
                        aggregate.errorSum += error;
                        bool explained = false;
                        for (size_t channel = 0; channel < 4; ++channel)
                        {
                            const double value = authorityValues[channel];
                            aggregate.sum[channel] += value;
                            aggregate.maximum[channel] = std::max(aggregate.maximum[channel], value);
                            aggregate.errorWeightedSum[channel] += value * error;
                            const double threshold = channel < 2 ? 0.01 : 0.05;
                            if (value > threshold)
                            {
                                ++aggregate.active[channel];
                                explained = true;
                            }
                        }
                        for (size_t metric = 0; metric < 8; ++metric)
                        {
                            const double value = geometryValues[metric];
                            aggregate.geometrySum[metric] += value;
                            aggregate.geometryMaximum[metric] = std::max(
                                aggregate.geometryMaximum[metric], value);
                            aggregate.geometryErrorWeightedSum[metric] += value * error;
                            if (value > 0.5) ++aggregate.geometryActive[metric];
                        }
                        if (!explained) ++aggregate.unexplainedErrorPixels;
                    }
                }
                m_context->Unmap(m_motionDiagnosticReadbacks[2].get(), 0);
                m_context->Unmap(m_motionDiagnosticReadbacks[1].get(), 0);
                m_context->Unmap(m_motionDiagnosticReadbacks[0].get(), 0);
                m_context->Unmap(m_replayReadback.get(), 0);
            };

            struct FrameSample
            {
                double sourceMean = 0.0;
                double outputMean = 0.0;
                double sourceRed = 0.0;
                double sourceGreen = 0.0;
                double sourceBlue = 0.0;
                double outputRed = 0.0;
                double outputGreen = 0.0;
                double outputBlue = 0.0;
                double sourceLinearRed = 0.0;
                double sourceLinearGreen = 0.0;
                double sourceLinearBlue = 0.0;
                double outputLinearRed = 0.0;
                double outputLinearGreen = 0.0;
                double outputLinearBlue = 0.0;
                double outputDisplayLinearRed = 0.0;
                double outputDisplayLinearGreen = 0.0;
                double outputDisplayLinearBlue = 0.0;
                double protectedLumaState = 0.0;
                double sourceWcagLuma = 0.0;
                double outputWcagLuma = 0.0;
                double outputDisplayWcagLuma = 0.0;
                double mae = 0.0;
                double outsideMae = 0.0;
                double insideMae = 0.0;
                double edgeMae = 0.0;
                // Full-pixel measurements over only the pixels vacated by the
                // moving object. These are intentionally not diluted by the rest
                // of the frame, unlike outsideMae.
                double vacatedMean = 0.0;
                double vacatedP95 = 0.0;
                double vacatedP99 = 0.0;
                double vacatedPeak = 0.0;
                double vacatedAreaAbove02 = 0.0;
                double vacatedAreaAbove05 = 0.0;
                uint64_t vacatedSamples = 0;
            };

            uint64_t flowFrames = 0;
            const auto renderAndSample = [&](const RECT* activeRect,
                                             const wchar_t* visualCase = nullptr,
                                             int visualFrame = -1,
                                             MotionDiagnosticAggregate* motionDiagnostics = nullptr,
                                             std::vector<uint32_t>* sourceDisplayCodes = nullptr,
                                             std::vector<uint32_t>* outputDisplayCodes = nullptr,
                                             const RECT* previousRect = nullptr,
                                             AuthorityDiagnosticAggregate* authorityDiagnostics = nullptr) {
                m_context->UpdateSubresource(source.get(), 0, nullptr,
                    pixels.data(), width * 4u, 0);
                QueueCapturedFrame(source.get(), dt);
                if (m_nvofFlowValid) ++flowFrames;

                m_context->CopyResource(m_replayReadback.get(),
                    m_outputHistoryTextures[m_outputHistoryIndex].get());
                D3D11_MAPPED_SUBRESOURCE mapped{};
                ThrowIfFailed(m_context->Map(
                    m_replayReadback.get(), 0, D3D11_MAP_READ, 0, &mapped));

                FrameSample sample{};
                uint64_t count = 0;
                uint64_t outsideCount = 0;
                uint64_t insideCount = 0;
                uint64_t edgeCount = 0;
                std::vector<double> vacatedErrors;
                if (previousRect) vacatedErrors.reserve(4096);
                const auto quantizeUnorm8Code = [](float value) {
                    return static_cast<uint32_t>(std::lround(
                        std::clamp(value, 0.0f, 1.0f) * 255.0f));
                };
                const size_t sampledPixelCount =
                    static_cast<size_t>((width + sampleStride - 1u) / sampleStride) *
                    ((height + sampleStride - 1u) / sampleStride);
                if (sourceDisplayCodes) {
                    sourceDisplayCodes->clear();
                    sourceDisplayCodes->reserve(sampledPixelCount);
                }
                if (outputDisplayCodes) {
                    outputDisplayCodes->clear();
                    outputDisplayCodes->reserve(sampledPixelCount);
                }
                const UINT stride = sampleStride;
                for (UINT y = 0; y < height; y += stride)
                {
                    const auto* row = reinterpret_cast<const uint16_t*>(
                        static_cast<const uint8_t*>(mapped.pData) +
                        static_cast<size_t>(y) * mapped.RowPitch);
                    for (UINT x = 0; x < width; x += stride)
                    {
                        const float r = std::clamp(halfToFloat(row[x * 4 + 0]), 0.0f, 1.0f);
                        const float g = std::clamp(halfToFloat(row[x * 4 + 1]), 0.0f, 1.0f);
                        const float b = std::clamp(halfToFloat(row[x * 4 + 2]), 0.0f, 1.0f);
                        const double protectedLumaState = std::clamp(
                            static_cast<double>(halfToFloat(row[x * 4 + 3])),
                            0.0, 1.0);
                        const double outputLuma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
                        const double outputLinearR = srgbToLinear(r);
                        const double outputLinearG = srgbToLinear(g);
                        const double outputLinearB = srgbToLinear(b);
                        const double outputWcagLuma = 0.2126 * outputLinearR +
                            0.7152 * outputLinearG + 0.0722 * outputLinearB;
                        // SC 2.3.2/G19 is evaluated on the representable display
                        // state, not sub-LSB R16 history noise. Keep each sampled
                        // pixel's code so spatially distinct changes cannot create
                        // a false flash by oscillating only in the frame average.
                        const uint32_t displayR8 = quantizeUnorm8Code(r);
                        const uint32_t displayG8 = quantizeUnorm8Code(g);
                        const uint32_t displayB8 = quantizeUnorm8Code(b);
                        const double displayR = static_cast<double>(displayR8) / 255.0;
                        const double displayG = static_cast<double>(displayG8) / 255.0;
                        const double displayB = static_cast<double>(displayB8) / 255.0;
                        const double outputDisplayLinearR = srgbToLinear(displayR);
                        const double outputDisplayLinearG = srgbToLinear(displayG);
                        const double outputDisplayLinearB = srgbToLinear(displayB);
                        const double outputDisplayWcagLuma =
                            0.2126 * outputDisplayLinearR +
                            0.7152 * outputDisplayLinearG +
                            0.0722 * outputDisplayLinearB;

                        const uint32_t packed = pixels[static_cast<size_t>(y) * width + x];
                        const float sb = static_cast<float>(packed & 0xFFu) / 255.0f;
                        const float sg = static_cast<float>((packed >> 8) & 0xFFu) / 255.0f;
                        const float sr = static_cast<float>((packed >> 16) & 0xFFu) / 255.0f;
                        const double sourceLuma = 0.2126 * sr + 0.7152 * sg + 0.0722 * sb;
                        const double sourceLinearR = srgbToLinear(sr);
                        const double sourceLinearG = srgbToLinear(sg);
                        const double sourceLinearB = srgbToLinear(sb);
                        const double sourceWcagLuma = 0.2126 * sourceLinearR +
                            0.7152 * sourceLinearG + 0.0722 * sourceLinearB;
                        const double error = std::fabs(outputLuma - sourceLuma);

                        sample.sourceMean += sourceLuma;
                        sample.outputMean += outputLuma;
                        sample.sourceRed += sr;
                        sample.sourceGreen += sg;
                        sample.sourceBlue += sb;
                        sample.outputRed += r;
                        sample.outputGreen += g;
                        sample.outputBlue += b;
                        sample.sourceLinearRed += sourceLinearR;
                        sample.sourceLinearGreen += sourceLinearG;
                        sample.sourceLinearBlue += sourceLinearB;
                        sample.outputLinearRed += outputLinearR;
                        sample.outputLinearGreen += outputLinearG;
                        sample.outputLinearBlue += outputLinearB;
                        sample.outputDisplayLinearRed += outputDisplayLinearR;
                        sample.outputDisplayLinearGreen += outputDisplayLinearG;
                        sample.outputDisplayLinearBlue += outputDisplayLinearB;
                        sample.protectedLumaState += protectedLumaState;
                        sample.sourceWcagLuma += sourceWcagLuma;
                        sample.outputWcagLuma += outputWcagLuma;
                        sample.outputDisplayWcagLuma += outputDisplayWcagLuma;
                        sample.mae += error;
                        if (sourceDisplayCodes)
                            sourceDisplayCodes->push_back(packed & 0x00FFFFFFu);
                        if (outputDisplayCodes)
                            outputDisplayCodes->push_back(
                                displayB8 | (displayG8 << 8) | (displayR8 << 16));
                        ++count;

                        const bool inside = activeRect &&
                            static_cast<LONG>(x) >= activeRect->left &&
                            static_cast<LONG>(x) < activeRect->right &&
                            static_cast<LONG>(y) >= activeRect->top &&
                            static_cast<LONG>(y) < activeRect->bottom;
                        if (!inside)
                        {
                            sample.outsideMae += error;
                            ++outsideCount;
                        }
                        else
                        {
                            sample.insideMae += error;
                            ++insideCount;
                            constexpr LONG edgeBand = 8;
                            const bool edge =
                                static_cast<LONG>(x) < activeRect->left + edgeBand ||
                                static_cast<LONG>(x) >= activeRect->right - edgeBand ||
                                static_cast<LONG>(y) < activeRect->top + edgeBand ||
                                static_cast<LONG>(y) >= activeRect->bottom - edgeBand;
                            if (edge)
                            {
                                sample.edgeMae += error;
                                ++edgeCount;
                            }
                        }
                    }
                }

                // Revisit only the just-vacated source footprint at full pixel
                // resolution. A 1-3 px bright trail can be obvious to a person yet
                // nearly disappear from a 4 px grid and whole-background MAE.
                if (previousRect)
                {
                    const LONG left = std::max<LONG>(0, previousRect->left);
                    const LONG top = std::max<LONG>(0, previousRect->top);
                    const LONG right = std::min<LONG>(static_cast<LONG>(width),
                        previousRect->right);
                    const LONG bottom = std::min<LONG>(static_cast<LONG>(height),
                        previousRect->bottom);
                    for (LONG y = top; y < bottom; ++y)
                    {
                        const auto* row = reinterpret_cast<const uint16_t*>(
                            static_cast<const uint8_t*>(mapped.pData) +
                            static_cast<size_t>(y) * mapped.RowPitch);
                        for (LONG x = left; x < right; ++x)
                        {
                            const bool stillOccupied = activeRect &&
                                x >= activeRect->left && x < activeRect->right &&
                                y >= activeRect->top && y < activeRect->bottom;
                            if (stillOccupied) continue;
                            const float r = std::clamp(halfToFloat(
                                row[static_cast<size_t>(x) * 4 + 0]), 0.0f, 1.0f);
                            const float g = std::clamp(halfToFloat(
                                row[static_cast<size_t>(x) * 4 + 1]), 0.0f, 1.0f);
                            const float b = std::clamp(halfToFloat(
                                row[static_cast<size_t>(x) * 4 + 2]), 0.0f, 1.0f);
                            const double outputLuma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
                            const uint32_t packed = pixels[
                                static_cast<size_t>(y) * width + static_cast<UINT>(x)];
                            const float sb = static_cast<float>(packed & 0xFFu) / 255.0f;
                            const float sg = static_cast<float>((packed >> 8) & 0xFFu) / 255.0f;
                            const float sr = static_cast<float>((packed >> 16) & 0xFFu) / 255.0f;
                            const double sourceLuma = 0.2126 * sr + 0.7152 * sg + 0.0722 * sb;
                            vacatedErrors.push_back(std::fabs(outputLuma - sourceLuma));
                        }
                    }
                }
                if (!vacatedErrors.empty())
                {
                    std::sort(vacatedErrors.begin(), vacatedErrors.end());
                    double sum = 0.0;
                    uint64_t above02 = 0;
                    uint64_t above05 = 0;
                    for (double error : vacatedErrors)
                    {
                        sum += error;
                        above02 += error > 0.02 ? 1u : 0u;
                        above05 += error > 0.05 ? 1u : 0u;
                    }
                    const auto percentile = [&](double q) {
                        const size_t index = std::min(vacatedErrors.size() - 1,
                            static_cast<size_t>(std::ceil(
                                q * static_cast<double>(vacatedErrors.size()))) - 1);
                        return vacatedErrors[index];
                    };
                    sample.vacatedSamples = static_cast<uint64_t>(vacatedErrors.size());
                    sample.vacatedMean = sum / static_cast<double>(vacatedErrors.size());
                    sample.vacatedP95 = percentile(0.95);
                    sample.vacatedP99 = percentile(0.99);
                    sample.vacatedPeak = vacatedErrors.back();
                    sample.vacatedAreaAbove02 = static_cast<double>(above02) /
                        static_cast<double>(vacatedErrors.size());
                    sample.vacatedAreaAbove05 = static_cast<double>(above05) /
                        static_cast<double>(vacatedErrors.size());
                }
                writeVisualBmp(visualCase, visualFrame, mapped);
                m_context->Unmap(m_replayReadback.get(), 0);
                if (authorityDiagnostics)
                    collectAuthorityDiagnostics(*authorityDiagnostics, previousRect, activeRect);
                if (motionDiagnostics)
                {
                    const uint64_t request = diagnosticRequestCount++;
                    if (request % static_cast<uint64_t>(diagnosticSampleInterval) == 0)
                        collectMotionDiagnostics(*motionDiagnostics, activeRect);
                }
                previousDiagnosticSource = pixels;
                previousDiagnosticSourceValid = true;
                if (count)
                {
                    sample.sourceMean /= static_cast<double>(count);
                    sample.outputMean /= static_cast<double>(count);
                    sample.sourceRed /= static_cast<double>(count);
                    sample.sourceGreen /= static_cast<double>(count);
                    sample.sourceBlue /= static_cast<double>(count);
                    sample.outputRed /= static_cast<double>(count);
                    sample.outputGreen /= static_cast<double>(count);
                    sample.outputBlue /= static_cast<double>(count);
                    sample.sourceLinearRed /= static_cast<double>(count);
                    sample.sourceLinearGreen /= static_cast<double>(count);
                    sample.sourceLinearBlue /= static_cast<double>(count);
                    sample.outputLinearRed /= static_cast<double>(count);
                    sample.outputLinearGreen /= static_cast<double>(count);
                    sample.outputLinearBlue /= static_cast<double>(count);
                    sample.outputDisplayLinearRed /= static_cast<double>(count);
                    sample.outputDisplayLinearGreen /= static_cast<double>(count);
                    sample.outputDisplayLinearBlue /= static_cast<double>(count);
                    sample.protectedLumaState /= static_cast<double>(count);
                    sample.sourceWcagLuma /= static_cast<double>(count);
                    sample.outputWcagLuma /= static_cast<double>(count);
                    sample.outputDisplayWcagLuma /= static_cast<double>(count);
                    sample.mae /= static_cast<double>(count);
                }
                if (outsideCount)
                    sample.outsideMae /= static_cast<double>(outsideCount);
                if (insideCount)
                    sample.insideMae /= static_cast<double>(insideCount);
                if (edgeCount)
                    sample.edgeMae /= static_cast<double>(edgeCount);
                return sample;
            };

            const auto resetCase = [&]() {
                ResetDelayedPipeline();
                m_timelineSeconds = 0.0f;
                m_replayClockSeconds = 0.0;
                m_lastHazardTime = -100.0f;
                m_lastGlobalHazardTime = -100.0f;
                m_nextSequence = 0;
                ClearAllToBlack();
                previousDiagnosticSourceValid = false;
            };

            resetCase();
            fillGray(96);
            for (int i = 0; i < warmupFrames; ++i) renderAndSample(nullptr);
            double staticMae = 0.0;
            const int staticFrames = replayScreening ?
                std::max(8, replayFps / 4) : std::max(20, replayFps * 2 / 3);
            for (int i = 0; i < staticFrames; ++i)
                staticMae += renderAndSample(nullptr).mae;
            staticMae /= static_cast<double>(staticFrames);
            const uint64_t staticFlowFrames = flowFrames;

            resetCase();
            fillGray(20);
            FrameSample previous = renderAndSample(nullptr);
            for (int i = 1; i < warmupFrames; ++i)
                previous = renderAndSample(nullptr);
            double rawVariation = 0.0;
            double outputVariation = 0.0;
            const int flashFrames = replayFps * (replayScreening ? 1 : 2);
            for (int i = 0; i < flashFrames; ++i)
            {
                const double phase = std::fmod(
                    (static_cast<double>(i) + 0.5) * 15.0 /
                    static_cast<double>(replayFps), 1.0);
                fillGray(phase < 0.5 ? 235 : 20);
                const FrameSample current = renderAndSample(nullptr,
                    (writeVisuals && i % std::max(1, replayFps / 6) == 0) ?
                        L"flash_15hz" : nullptr,
                    i);
                rawVariation += std::fabs(current.sourceMean - previous.sourceMean);
                outputVariation += std::fabs(current.outputMean - previous.outputMean);
                previous = current;
            }
            const double flashReduction = rawVariation > 1e-9 ?
                1.0 - outputVariation / rawVariation : 0.0;
            const uint64_t flashFlowFrames = flowFrames - staticFlowFrames;

            // WCAG 2.2-oriented deterministic flash sweep. General-flash
            // transitions use linear-sRGB relative luminance. Saturated-red
            // transitions use the WCAG 2.2 CIE 1976 u-prime/v-prime definition.
            // Flash counts are the maximum completed opposing transition pairs
            // found in any one-second window, measured between temporal extrema.
            // Physical display size and viewing distance calibrate the solid
            // angle of each synthetic flashing region. This remains a regression
            // corpus, not an external certification or arbitrary-video analyzer.
            struct FlashSweepResult
            {
                std::string caseName;
                double frequencyHz = 0.0;
                double rawVariation = 0.0;
                double outputVariation = 0.0;
                double reduction = 0.0;
                double peakOutputDelta = 0.0;
                double rawGeneralFlashesPerSecond = 0.0;
                double outputGeneralFlashesPerSecond = 0.0;
                double rawRedFlashesPerSecond = 0.0;
                double outputRedFlashesPerSecond = 0.0;
                // Legacy R16 epsilon counters remain diagnostics only.
                double rawStrictTransitionsPerSecond = 0.0;
                double outputStrictTransitionsPerSecond = 0.0;
                double rawStrictFlashesPerSecond = 0.0;
                double outputStrictFlashesPerSecond = 0.0;
                // G19-style transition counting uses the final 8-bit display state.
                double rawDisplayTransitionsPerSecond = 0.0;
                double outputDisplayTransitionsPerSecond = 0.0;
                double rawDisplayFlashesPerSecond = 0.0;
                double outputDisplayFlashesPerSecond = 0.0;
                double regionSolidAngleSr = 0.0;
                bool areaBelowThreshold = false;
                bool sc231StimulusValid = false;
                bool sc232StimulusValid = false;
                bool wcagSc231Pass = false;
                bool wcagSc232Pass = false;
            };
            std::vector<FlashSweepResult> flashSweep;
            MotionDiagnosticAggregate quarterFlashDiagnostics{};
            MotionDiagnosticAggregate movingDiagnostics{};
            MotionDiagnosticAggregate obliqueDiagnostics{};
            MotionDiagnosticAggregate smallMovingDiagnostics{};
            MotionDiagnosticAggregate smoothScrollDiagnostics{};
            MotionDiagnosticAggregate snappedScrollDiagnostics{};
            const std::vector<double> sweepFrequencies = replayScreening ?
                std::vector<double>{ 10.0 } :
                std::vector<double>{
                    5.0, 7.5, 10.0, 12.0, 15.0, 20.0, 25.0, 30.0 };
            struct SweepCase
            {
                const char* name;
                int kind; // 0=full luminance, 1=full saturated red, 2=quarter luminance
            };
            const std::vector<SweepCase> sweepCases = replayScreening ?
                std::vector<SweepCase>{{ "luminance_full", 0 }} :
                std::vector<SweepCase>{
                    { "luminance_full", 0 },
                    { "red_full", 1 },
                    { "luminance_quarter", 2 }
                };
            const int sweepFps = replayFps;
            const int sweepFrames = sweepFps * (replayScreening ? 1 : 2);
            const double sweepSeconds =
                static_cast<double>(sweepFrames) / static_cast<double>(sweepFps);

            struct WcagChromaticity
            {
                double redRatio = 0.0;
                double u = 0.0;
                double v = 0.0;
            };
            const auto wcagChromaticity = [](const FrameSample& sample, bool output) {
                const double r = output ?
                    sample.outputDisplayLinearRed : sample.sourceLinearRed;
                const double g = output ?
                    sample.outputDisplayLinearGreen : sample.sourceLinearGreen;
                const double b = output ?
                    sample.outputDisplayLinearBlue : sample.sourceLinearBlue;
                WcagChromaticity state{};
                const double sum = r + g + b;
                state.redRatio = sum > 1e-12 ? r / sum : 0.0;
                const double X = 0.4124564 * r + 0.3575761 * g + 0.1804375 * b;
                const double Y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b;
                const double Z = 0.0193339 * r + 0.1191920 * g + 0.9503041 * b;
                const double denominator = X + 15.0 * Y + 3.0 * Z;
                if (denominator > 1e-12)
                {
                    state.u = 4.0 * X / denominator;
                    state.v = 9.0 * Y / denominator;
                }
                return state;
            };
            const auto turningPoints = [](size_t count, const auto& valueAt) {
                std::vector<size_t> points;
                if (count == 0) return points;
                points.push_back(0);
                int trend = 0;
                for (size_t i = 1; i < count; ++i)
                {
                    const double delta = valueAt(i) - valueAt(i - 1);
                    const int direction = delta > 1e-7 ? 1 : (delta < -1e-7 ? -1 : 0);
                    if (direction == 0) continue;
                    if (trend == 0)
                    {
                        trend = direction;
                    }
                    else if (direction != trend)
                    {
                        if (points.back() != i - 1) points.push_back(i - 1);
                        trend = direction;
                    }
                }
                if (points.back() != count - 1) points.push_back(count - 1);
                return points;
            };
            const auto turningPointsExact = [](size_t count, const auto& valueAt) {
                std::vector<size_t> points;
                if (count == 0) return points;
                points.push_back(0);
                int trend = 0;
                for (size_t i = 1; i < count; ++i)
                {
                    const double delta = valueAt(i) - valueAt(i - 1);
                    const int direction = delta > 0.0 ? 1 : (delta < 0.0 ? -1 : 0);
                    if (direction == 0) continue;
                    if (trend == 0)
                    {
                        trend = direction;
                    }
                    else if (direction != trend)
                    {
                        if (points.back() != i - 1) points.push_back(i - 1);
                        trend = direction;
                    }
                }
                if (points.back() != count - 1) points.push_back(count - 1);
                return points;
            };
            struct WcagTransition
            {
                int frame = 0;
                int direction = 0;
            };
            const auto maxOpposingPairsInOneSecond = [&](const std::vector<WcagTransition>& transitions) {
                int maximum = 0;
                for (int startFrame = 0; startFrame <= sweepFrames; ++startFrame)
                {
                    int completed = 0;
                    int pendingDirection = 0;
                    for (const WcagTransition& transition : transitions)
                    {
                        if (transition.frame < startFrame) continue;
                        if (transition.frame >= startFrame + sweepFps) break;
                        if (pendingDirection == 0)
                        {
                            pendingDirection = transition.direction;
                        }
                        else if (transition.direction == -pendingDirection)
                        {
                            ++completed;
                            pendingDirection = 0;
                        }
                        else
                        {
                            pendingDirection = transition.direction;
                        }
                    }
                    maximum = std::max(maximum, completed);
                }
                return maximum;
            };
            const auto maxTransitionsInOneSecond = [&](const std::vector<WcagTransition>& transitions) {
                int maximum = 0;
                for (int startFrame = 0; startFrame <= sweepFrames; ++startFrame)
                {
                    int count = 0;
                    for (const WcagTransition& transition : transitions)
                    {
                        if (transition.frame < startFrame) continue;
                        if (transition.frame >= startFrame + sweepFps) break;
                        ++count;
                    }
                    maximum = std::max(maximum, count);
                }
                return maximum;
            };
            const auto maxGeneralFlashesInOneSecond = [&](const std::vector<FrameSample>& samples,
                                                           bool output) {
                const auto valueAt = [&](size_t index) {
                    return output ? samples[index].outputDisplayWcagLuma :
                        samples[index].sourceWcagLuma;
                };
                const std::vector<size_t> points = turningPoints(samples.size(), valueAt);
                std::vector<WcagTransition> transitions;
                for (size_t i = 1; i < points.size(); ++i)
                {
                    const double previousValue = valueAt(points[i - 1]);
                    const double currentValue = valueAt(points[i]);
                    const double delta = currentValue - previousValue;
                    if (std::fabs(delta) >= 0.10 &&
                        std::min(previousValue, currentValue) < 0.80)
                    {
                        transitions.push_back({ static_cast<int>(points[i]),
                            delta > 0.0 ? 1 : -1 });
                    }
                }
                return maxOpposingPairsInOneSecond(transitions);
            };
            const auto maxRedFlashesInOneSecond = [&](const std::vector<FrameSample>& samples,
                                                       bool output) {
                const auto ratioAt = [&](size_t index) {
                    return wcagChromaticity(samples[index], output).redRatio;
                };
                const std::vector<size_t> points = turningPoints(samples.size(), ratioAt);
                std::vector<WcagTransition> transitions;
                for (size_t i = 1; i < points.size(); ++i)
                {
                    const WcagChromaticity previous =
                        wcagChromaticity(samples[points[i - 1]], output);
                    const WcagChromaticity current =
                        wcagChromaticity(samples[points[i]], output);
                    const double du = current.u - previous.u;
                    const double dv = current.v - previous.v;
                    const double distance = std::sqrt(du * du + dv * dv);
                    if ((previous.redRatio >= 0.80 || current.redRatio >= 0.80) &&
                        distance > 0.20)
                    {
                        const double ratioDelta = current.redRatio - previous.redRatio;
                        transitions.push_back({ static_cast<int>(points[i]),
                            ratioDelta >= 0.0 ? 1 : -1 });
                    }
                }
                return maxOpposingPairsInOneSecond(transitions);
            };
            const auto maxStrictTransitionsInOneSecond = [&](const std::vector<FrameSample>& samples,
                                                              bool output) {
                const auto valueAt = [&](size_t index) {
                    return output ? samples[index].outputWcagLuma :
                        samples[index].sourceWcagLuma;
                };
                const std::vector<size_t> points = turningPoints(samples.size(), valueAt);
                std::vector<WcagTransition> transitions;
                for (size_t i = 1; i < points.size(); ++i)
                {
                    const double delta = valueAt(points[i]) - valueAt(points[i - 1]);
                    if (std::fabs(delta) > 1e-7)
                    {
                        transitions.push_back({ static_cast<int>(points[i]),
                            delta > 0.0 ? 1 : -1 });
                    }
                }
                return maxTransitionsInOneSecond(transitions);
            };
            std::array<double, 256> displayLinearLut{};
            for (size_t code = 0; code < displayLinearLut.size(); ++code)
                displayLinearLut[code] = srgbToLinear(
                    static_cast<double>(code) / 255.0);
            const auto displayLumaFromPacked = [&](uint32_t packed) {
                return 0.2126 * displayLinearLut[(packed >> 16) & 0xFFu] +
                    0.7152 * displayLinearLut[(packed >> 8) & 0xFFu] +
                    0.0722 * displayLinearLut[packed & 0xFFu];
            };
            const auto maxPixelDisplayTransitionsInOneSecond =
                [&](const std::vector<std::vector<uint32_t>>& timeline) {
                if (timeline.size() < 2 || timeline.front().empty()) return 0;
                const size_t pixelCount = timeline.front().size();
                for (const auto& frame : timeline)
                    if (frame.size() != pixelCount) return 0;

                int maximum = 0;
                std::vector<int> transitionFrames;
                transitionFrames.reserve(timeline.size());
                for (size_t pixel = 0; pixel < pixelCount; ++pixel)
                {
                    transitionFrames.clear();
                    int previousDirection = 0;
                    double previousValue = displayLumaFromPacked(timeline[0][pixel]);
                    for (size_t frame = 1; frame < timeline.size(); ++frame)
                    {
                        const double value = displayLumaFromPacked(timeline[frame][pixel]);
                        const double delta = value - previousValue;
                        const int direction = delta > 0.0 ? 1 : (delta < 0.0 ? -1 : 0);
                        if (direction != 0 && direction != previousDirection)
                        {
                            // One transition per monotonic light/dark segment.
                            // Multi-frame ramps are therefore not counted once per frame.
                            transitionFrames.push_back(static_cast<int>(frame));
                            previousDirection = direction;
                        }
                        previousValue = value;
                    }

                    size_t left = 0;
                    for (size_t right = 0; right < transitionFrames.size(); ++right)
                    {
                        while (left < right &&
                               transitionFrames[right] - transitionFrames[left] >= sweepFps)
                            ++left;
                        maximum = std::max(maximum,
                            static_cast<int>(right - left + 1));
                    }
                }
                return maximum;
            };
            const auto sweepRegionSolidAngle = [&](const SweepCase& sweepCase) {
                const double aspect = static_cast<double>(width) /
                    std::max(1.0, static_cast<double>(height));
                const double diagonalCm = m_safety.displayDiagonalInches * 2.54;
                const double screenHeightCm =
                    diagonalCm / std::sqrt(aspect * aspect + 1.0);
                const double screenWidthCm = screenHeightCm * aspect;
                const double widthFraction = sweepCase.kind == 2 ? 0.5 : 1.0;
                const double heightFraction = sweepCase.kind == 2 ? 0.5 : 1.0;
                const double halfWidth = screenWidthCm * widthFraction * 0.5;
                const double halfHeight = screenHeightCm * heightFraction * 0.5;
                const double distance = std::max(1.0,
                    static_cast<double>(m_safety.viewingDistanceCm));
                return 4.0 * std::atan((halfWidth * halfHeight) /
                    (distance * std::sqrt(distance * distance +
                        halfWidth * halfWidth + halfHeight * halfHeight)));
            };

            bool flashSweepPass = true;
            std::vector<FrameSample> red15HzTrace;
            for (const SweepCase& sweepCase : sweepCases)
            {
                for (double frequencyHz : sweepFrequencies)
                {
                    if (frequencyHz > static_cast<double>(sweepFps) * 0.5)
                        continue; // not representable at this synthetic frame rate
                    resetCase();
                    if (sweepCase.kind == 1)
                        std::fill(pixels.begin(), pixels.end(), rgbPixel(8, 8, 8));
                    else
                        fillGray(20);
                    std::vector<uint32_t> previousSourceDisplayCodes;
                    std::vector<uint32_t> previousOutputDisplayCodes;
                    FrameSample previousSweep = renderAndSample(nullptr, nullptr, -1, nullptr,
                        &previousSourceDisplayCodes, &previousOutputDisplayCodes);
                    for (int i = 1; i < warmupFrames; ++i)
                        previousSweep = renderAndSample(nullptr, nullptr, -1, nullptr,
                            &previousSourceDisplayCodes, &previousOutputDisplayCodes);

                    FlashSweepResult result{};
                    result.caseName = sweepCase.name;
                    result.frequencyHz = frequencyHz;
                    double rawRedVariation = 0.0;
                    double outputRedVariation = 0.0;
                    std::vector<FrameSample> sweepTimeline;
                    sweepTimeline.reserve(static_cast<size_t>(sweepFrames) + 1u);
                    sweepTimeline.push_back(previousSweep);
                    std::vector<std::vector<uint32_t>> sourceDisplayTimeline;
                    std::vector<std::vector<uint32_t>> outputDisplayTimeline;
                    sourceDisplayTimeline.reserve(static_cast<size_t>(sweepFrames) + 1u);
                    outputDisplayTimeline.reserve(static_cast<size_t>(sweepFrames) + 1u);
                    sourceDisplayTimeline.push_back(previousSourceDisplayCodes);
                    outputDisplayTimeline.push_back(previousOutputDisplayCodes);

                    for (int i = 0; i < sweepFrames; ++i)
                    {
                        const double phase = std::fmod(
                            (static_cast<double>(i) + 0.5) * frequencyHz /
                            static_cast<double>(sweepFps), 1.0);
                        const bool high = phase < 0.5;
                        if (sweepCase.kind == 0)
                        {
                            fillGray(high ? 235 : 20);
                        }
                        else if (sweepCase.kind == 1)
                        {
                            const uint32_t value = high ?
                                rgbPixel(235, 0, 0) : rgbPixel(8, 8, 8);
                            std::fill(pixels.begin(), pixels.end(), value);
                        }
                        else
                        {
                            fillGray(20);
                            if (high)
                            {
                                const int x0 = static_cast<int>(width) / 4;
                                const int y0 = static_cast<int>(height) / 4;
                                const int x1 = x0 + static_cast<int>(width) / 2;
                                const int y1 = y0 + static_cast<int>(height) / 2;
                                for (int y = y0; y < y1; ++y)
                                    for (int x = x0; x < x1; ++x)
                                        pixels[static_cast<size_t>(y) * width + x] =
                                            grayPixel(235);
                            }
                        }

                        const LONG quarterX = static_cast<LONG>(width / 4u);
                        const LONG quarterY = static_cast<LONG>(height / 4u);
                        const RECT quarterRect{
                            quarterX, quarterY,
                            quarterX + static_cast<LONG>(width / 2u),
                            quarterY + static_cast<LONG>(height / 2u)
                        };
                        MotionDiagnosticAggregate* sweepDiagnostics =
                            sweepCase.kind == 2 && std::fabs(frequencyHz - 15.0) < 0.001 ?
                            &quarterFlashDiagnostics : nullptr;
                        std::vector<uint32_t> currentSourceDisplayCodes;
                        std::vector<uint32_t> currentOutputDisplayCodes;
                        const FrameSample currentSweep = renderAndSample(
                            sweepDiagnostics ? &quarterRect : nullptr,
                            nullptr, -1, sweepDiagnostics,
                            &currentSourceDisplayCodes, &currentOutputDisplayCodes);
                        sourceDisplayTimeline.push_back(std::move(currentSourceDisplayCodes));
                        outputDisplayTimeline.push_back(std::move(currentOutputDisplayCodes));
                        const double rawDelta =
                            std::fabs(currentSweep.sourceMean - previousSweep.sourceMean);
                        const double outputDelta =
                            std::fabs(currentSweep.outputMean - previousSweep.outputMean);
                        result.rawVariation += rawDelta;
                        result.outputVariation += outputDelta;
                        result.peakOutputDelta = std::max(
                            result.peakOutputDelta, outputDelta);
                        rawRedVariation += std::fabs(
                            currentSweep.sourceRed - previousSweep.sourceRed);
                        outputRedVariation += std::fabs(
                            currentSweep.outputRed - previousSweep.outputRed);
                        sweepTimeline.push_back(currentSweep);
                        previousSweep = currentSweep;
                    }

                    result.reduction = result.rawVariation > 1e-9 ?
                        1.0 - result.outputVariation / result.rawVariation : 0.0;
                    result.rawGeneralFlashesPerSecond = static_cast<double>(
                        maxGeneralFlashesInOneSecond(sweepTimeline, false));
                    result.outputGeneralFlashesPerSecond = static_cast<double>(
                        maxGeneralFlashesInOneSecond(sweepTimeline, true));
                    result.rawRedFlashesPerSecond = static_cast<double>(
                        maxRedFlashesInOneSecond(sweepTimeline, false));
                    result.outputRedFlashesPerSecond = static_cast<double>(
                        maxRedFlashesInOneSecond(sweepTimeline, true));
                    result.rawStrictTransitionsPerSecond = static_cast<double>(
                        maxStrictTransitionsInOneSecond(sweepTimeline, false));
                    result.outputStrictTransitionsPerSecond = static_cast<double>(
                        maxStrictTransitionsInOneSecond(sweepTimeline, true));
                    result.rawStrictFlashesPerSecond =
                        result.rawStrictTransitionsPerSecond * 0.5;
                    result.outputStrictFlashesPerSecond =
                        result.outputStrictTransitionsPerSecond * 0.5;
                    result.rawDisplayTransitionsPerSecond = static_cast<double>(
                        maxPixelDisplayTransitionsInOneSecond(sourceDisplayTimeline));
                    result.outputDisplayTransitionsPerSecond = static_cast<double>(
                        maxPixelDisplayTransitionsInOneSecond(outputDisplayTimeline));
                    result.rawDisplayFlashesPerSecond =
                        result.rawDisplayTransitionsPerSecond * 0.5;
                    result.outputDisplayFlashesPerSecond =
                        result.outputDisplayTransitionsPerSecond * 0.5;

                    result.regionSolidAngleSr = sweepRegionSolidAngle(sweepCase);
                    result.areaBelowThreshold = result.regionSolidAngleSr <= 0.006;
                    result.sc231StimulusValid =
                        result.rawGeneralFlashesPerSecond > 3.0 ||
                        result.rawRedFlashesPerSecond > 3.0;
                    result.sc232StimulusValid = result.sc231StimulusValid;
                    const bool outputBelowSc231Threshold =
                        result.outputGeneralFlashesPerSecond <= 3.0 &&
                        result.outputRedFlashesPerSecond <= 3.0;
                    result.wcagSc231Pass =
                        outputBelowSc231Threshold || result.areaBelowThreshold;
                    // SC 2.3.2 removes the area exemption but retains the
                    // defined general-flash and saturated-red thresholds.
                    result.wcagSc232Pass = outputBelowSc231Threshold;
                    flashSweepPass = flashSweepPass &&
                        result.sc231StimulusValid && result.sc232StimulusValid &&
                        result.wcagSc231Pass && result.wcagSc232Pass;
                    if (sweepCase.kind == 1 && std::fabs(frequencyHz - 15.0) < 0.001)
                        red15HzTrace = sweepTimeline;
                    flashSweep.push_back(result);
                }
            }

            if (!red15HzTrace.empty())
            {
                const auto redTracePath =
                    std::filesystem::path(reportPath).parent_path() /
                    L"red-flash-trace-15hz.json";
                FILE* redTraceReport = nullptr;
                if (_wfopen_s(&redTraceReport, redTracePath.c_str(), L"wb") == 0 &&
                    redTraceReport)
                {
                    std::fprintf(redTraceReport,
                        "{\n"
                        "  \"schema\": \"RED_FLASH_TRACE/2\",\n"
                        "  \"fps\": %d,\n"
                        "  \"note\": \"Replay-only trace of the 15 Hz full-screen saturated-red case. RGB values are final 8-bit-display-equivalent linear-sRGB means; u/v are CIE 1976 u-prime/v-prime. protected_luma_state is the transported linear-light protection state stored in output-history alpha.\",\n"
                        "  \"frames\": [\n",
                        sweepFps);
                    for (size_t frame = 0; frame < red15HzTrace.size(); ++frame)
                    {
                        const FrameSample& sample = red15HzTrace[frame];
                        const WcagChromaticity sourceState =
                            wcagChromaticity(sample, false);
                        const WcagChromaticity outputState =
                            wcagChromaticity(sample, true);
                        std::fprintf(redTraceReport,
                            "    {\"frame\":%zu,"
                            "\"source_linear_rgb\":[%.8f,%.8f,%.8f],"
                            "\"output_display_linear_rgb\":[%.8f,%.8f,%.8f],"
                            "\"source_red_ratio\":%.8f,\"source_u\":%.8f,\"source_v\":%.8f,"
                            "\"output_red_ratio\":%.8f,\"output_u\":%.8f,\"output_v\":%.8f,"
                            "\"protected_luma_state\":%.8f}%s\n",
                            frame,
                            sample.sourceLinearRed, sample.sourceLinearGreen,
                            sample.sourceLinearBlue,
                            sample.outputDisplayLinearRed,
                            sample.outputDisplayLinearGreen,
                            sample.outputDisplayLinearBlue,
                            sourceState.redRatio, sourceState.u, sourceState.v,
                            outputState.redRatio, outputState.u, outputState.v,
                            sample.protectedLumaState,
                            (frame + 1 < red15HzTrace.size()) ? "," : "");
                    }
                    std::fputs("  ]\n}\n", redTraceReport);
                    std::fclose(redTraceReport);
                }
            }

            const auto flashSweepPath =
                std::filesystem::path(reportPath).parent_path() / L"flash-sweep.json";
            FILE* flashSweepReport = nullptr;
            if (_wfopen_s(&flashSweepReport, flashSweepPath.c_str(), L"wb") != 0 ||
                !flashSweepReport)
                return false;
            std::fprintf(flashSweepReport,
                "{\n"
                "  \"schema\": \"FLASHGUARD_FLASH_SWEEP/6\",\n"
                "  \"wcag_profile\": \"WCAG_FLASH/5\",\n"
                "  \"status\": \"%s\",\n"
                "  \"fps\": %d,\n"
                "  \"duration_seconds_per_case\": %.2f,\n"
                "  \"display_diagonal_inches\": %.2f,\n"
                "  \"viewing_distance_cm\": %.2f,\n"
                "  \"sc_2_3_2_measurement_surface\": \"B8G8R8A8_UNORM-equivalent replay output\",\n"
                "  \"g19_display_transition_diagnostic_normative\": false,\n"
                "  \"internal_r16_epsilon_diagnostic_normative\": false,\n"
                "  \"note\": \"WCAG 2.2 deterministic regression: SC 2.3.1 and SC 2.3.2 use the general/red flash definitions on 8-bit-equivalent output; SC 2.3.2 differs by having no area exemption. G19-style one-code display transitions and internal R16 epsilon reversals are diagnostic only. Not an external certification or arbitrary-video analyzer.\",\n"
                "  \"cases\": [\n",
                flashSweepPass ? "SUCCESS" : "FAILED", sweepFps, sweepSeconds,
                m_safety.displayDiagonalInches, m_safety.viewingDistanceCm);
            for (size_t i = 0; i < flashSweep.size(); ++i)
            {
                const FlashSweepResult& result = flashSweep[i];
                std::fprintf(flashSweepReport,
                    "    {\"case\":\"%s\",\"frequency_hz\":%.2f,"
                    "\"raw_variation\":%.8f,\"output_variation\":%.8f,"
                    "\"reduction\":%.8f,\"peak_output_delta\":%.8f,"
                    "\"raw_general_flashes_per_second\":%.3f,"
                    "\"output_general_flashes_per_second\":%.3f,"
                    "\"raw_red_flashes_per_second\":%.3f,"
                    "\"output_red_flashes_per_second\":%.3f,"
                    "\"raw_g19_display_transitions_per_second\":%.3f,"
                    "\"output_g19_display_transitions_per_second\":%.3f,"
                    "\"raw_g19_display_flashes_per_second\":%.3f,"
                    "\"output_g19_display_flashes_per_second\":%.3f,"
                    "\"raw_internal_r16_epsilon_transitions_per_second\":%.3f,"
                    "\"output_internal_r16_epsilon_transitions_per_second\":%.3f,"
                    "\"raw_internal_r16_epsilon_flashes_per_second\":%.3f,"
                    "\"output_internal_r16_epsilon_flashes_per_second\":%.3f,"
                    "\"region_solid_angle_sr\":%.8f,"
                    "\"area_below_0_006_sr\":%s,"
                    "\"sc_2_3_1_stimulus_valid\":%s,"
                    "\"sc_2_3_2_stimulus_valid\":%s,"
                    "\"wcag_sc_2_3_1_pass\":%s,"
                    "\"wcag_sc_2_3_2_pass\":%s}%s\n",
                    result.caseName.c_str(), result.frequencyHz,
                    result.rawVariation, result.outputVariation, result.reduction,
                    result.peakOutputDelta,
                    result.rawGeneralFlashesPerSecond,
                    result.outputGeneralFlashesPerSecond,
                    result.rawRedFlashesPerSecond,
                    result.outputRedFlashesPerSecond,
                    result.rawDisplayTransitionsPerSecond,
                    result.outputDisplayTransitionsPerSecond,
                    result.rawDisplayFlashesPerSecond,
                    result.outputDisplayFlashesPerSecond,
                    result.rawStrictTransitionsPerSecond,
                    result.outputStrictTransitionsPerSecond,
                    result.rawStrictFlashesPerSecond,
                    result.outputStrictFlashesPerSecond,
                    result.regionSolidAngleSr,
                    result.areaBelowThreshold ? "true" : "false",
                    result.sc231StimulusValid ? "true" : "false",
                    result.sc232StimulusValid ? "true" : "false",
                    result.wcagSc231Pass ? "true" : "false",
                    result.wcagSc232Pass ? "true" : "false",
                    (i + 1 < flashSweep.size()) ? "," : "");
            }
            std::fputs("  ]\n}\n", flashSweepReport);
            std::fclose(flashSweepReport);

            // Perceptual calibration sweep for the lower-contrast static flashes
            // reported manually. It is deliberately separate from WCAG pass/fail:
            // these cases measure attenuation continuity below normative thresholds.
            double perceptualMinReduction = 0.0;
            double perceptualMaxOutputDelta = 0.0;
            int perceptualCaseCount = 0;
            {
                // Uniform perceptual flash cases do not need a full CPU traversal of
                // the mapped frame. Sample the center pixel after the real render/NVOFA
                // path so broad low-contrast sweeps remain cheap.
                const auto renderCenterLuma = [&]() {
                    m_context->UpdateSubresource(source.get(), 0, nullptr,
                        pixels.data(), width * 4u, 0);
                    QueueCapturedFrame(source.get(), dt);
                    if (m_nvofFlowValid) ++flowFrames;
                    m_context->CopyResource(m_replayReadback.get(),
                        m_outputHistoryTextures[m_outputHistoryIndex].get());
                    D3D11_MAPPED_SUBRESOURCE mapped{};
                    ThrowIfFailed(m_context->Map(
                        m_replayReadback.get(), 0, D3D11_MAP_READ, 0, &mapped));
                    const UINT x = width / 2u;
                    const UINT y = height / 2u;
                    const auto* row = reinterpret_cast<const uint16_t*>(
                        static_cast<const uint8_t*>(mapped.pData) +
                        static_cast<size_t>(y) * mapped.RowPitch);
                    const float r = std::clamp(halfToFloat(row[x * 4 + 0]), 0.0f, 1.0f);
                    const float g = std::clamp(halfToFloat(row[x * 4 + 1]), 0.0f, 1.0f);
                    const float b = std::clamp(halfToFloat(row[x * 4 + 2]), 0.0f, 1.0f);
                    const double outputLuma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
                    const uint32_t packed = pixels[static_cast<size_t>(y) * width + x];
                    const float sb = static_cast<float>(packed & 0xFFu) / 255.0f;
                    const float sg = static_cast<float>((packed >> 8) & 0xFFu) / 255.0f;
                    const float sr = static_cast<float>((packed >> 16) & 0xFFu) / 255.0f;
                    const double sourceLuma = 0.2126 * sr + 0.7152 * sg + 0.0722 * sb;
                    m_context->Unmap(m_replayReadback.get(), 0);
                    return std::pair<double, double>{ sourceLuma, outputLuma };
                };
                const auto perceptualPath =
                    std::filesystem::path(reportPath).parent_path() /
                    L"perceptual-sweep.json";
                FILE* perceptualReport = nullptr;
                if (_wfopen_s(&perceptualReport, perceptualPath.c_str(), L"wb") != 0 ||
                    !perceptualReport)
                    return false;
                std::fputs(
                    "{\n  \"schema\": \"FLASHGUARD_PERCEPTUAL_SWEEP/1\",\n"
                    "  \"status\": \"SUCCESS\",\n"
                    "  \"calibration_only\": true,\n"
                    "  \"background_code\": 96,\n"
                    "  \"cases\": [\n", perceptualReport);
                const std::vector<int> deltas = replayScreening ?
                    std::vector<int>{ 8, 16 } :
                    std::vector<int>{ 4, 8, 12, 16, 24, 32 };
                const std::vector<double> frequencies = replayScreening ?
                    std::vector<double>{ 10.0 } :
                    std::vector<double>{ 5.0, 10.0, 15.0 };
                const std::vector<double> phaseFrames = replayScreening ?
                    std::vector<double>{ 0.5 } :
                    std::vector<double>{ 0.0, 0.5 };
                bool firstPerceptual = true;
                perceptualMinReduction = 1.0;
                for (int deltaCode : deltas)
                {
                    const int lowCode = 96 - deltaCode / 2;
                    const int highCode = 96 + (deltaCode + 1) / 2;
                    for (double frequencyHz : frequencies)
                    {
                        if (frequencyHz > static_cast<double>(replayFps) * 0.5)
                            continue;
                        for (double phaseFrame : phaseFrames)
                        {
                            resetCase();
                            fillGray(static_cast<uint8_t>(lowCode));
                            std::pair<double, double> previousCenter{};
                            for (int i = 0; i < std::max(4, replayFps / 10); ++i)
                                previousCenter = renderCenterLuma();
                            double sourceVariation = 0.0;
                            double filteredVariation = 0.0;
                            double peakOutputDelta = 0.0;
                            const int caseFrames = replayScreening ?
                                std::max(12, replayFps * 2 / 5) : std::max(30, replayFps);
                            for (int i = 0; i < caseFrames; ++i)
                            {
                                const double phase = std::fmod(
                                    (static_cast<double>(i) + 0.5 + phaseFrame) *
                                    frequencyHz / static_cast<double>(replayFps), 1.0);
                                fillGray(static_cast<uint8_t>(
                                    phase < 0.5 ? highCode : lowCode));
                                const auto currentCenter = renderCenterLuma();
                                sourceVariation += std::fabs(
                                    currentCenter.first - previousCenter.first);
                                const double outputDelta = std::fabs(
                                    currentCenter.second - previousCenter.second);
                                filteredVariation += outputDelta;
                                peakOutputDelta = std::max(peakOutputDelta, outputDelta);
                                previousCenter = currentCenter;
                            }
                            const double reduction = sourceVariation > 1e-9 ?
                                1.0 - filteredVariation / sourceVariation : 0.0;
                            perceptualMinReduction = std::min(
                                perceptualMinReduction, reduction);
                            perceptualMaxOutputDelta = std::max(
                                perceptualMaxOutputDelta, peakOutputDelta);
                            ++perceptualCaseCount;
                            std::fprintf(perceptualReport,
                                "%s    {\"delta_code\":%d,\"low_code\":%d,"
                                "\"high_code\":%d,\"frequency_hz\":%.2f,"
                                "\"phase_frames\":%.2f,\"reduction\":%.8f,"
                                "\"peak_output_delta\":%.8f}",
                                firstPerceptual ? "" : ",\n",
                                deltaCode, lowCode, highCode, frequencyHz,
                                phaseFrame, reduction, peakOutputDelta);
                            firstPerceptual = false;
                        }
                    }
                }
                std::fputs("\n  ]\n}\n", perceptualReport);
                std::fclose(perceptualReport);

                if (replayScreening)
                {
                    struct ScreeningProbeResult
                    {
                        double sourceVariation = 0.0;
                        double outputVariation = 0.0;
                        double residualRatio = 1.0;
                    };
                    const auto runScreeningProbe =
                        [&](int highCode, double targetAreaFraction)
                    {
                        const bool fullScreen = targetAreaFraction <= 0.0;
                        const int side = fullScreen ? 0 : std::max(4,
                            static_cast<int>(std::lround(std::sqrt(
                                static_cast<double>(width) *
                                static_cast<double>(height) *
                                targetAreaFraction))));
                        resetCase();
                        fillGray(96);
                        std::pair<double, double> previousCenter{};
                        for (int i = 0; i < 3; ++i)
                            previousCenter = renderCenterLuma();

                        ScreeningProbeResult result{};
                        constexpr int probeFrames = 12;
                        for (int i = 0; i < probeFrames; ++i)
                        {
                            fillGray(96);
                            const bool high = (i % 4) < 2;
                            if (high)
                            {
                                if (fullScreen)
                                {
                                    fillGray(static_cast<uint8_t>(highCode));
                                }
                                else
                                {
                                    const int x0 = std::max(0,
                                        static_cast<int>(width) / 2 - side / 2);
                                    const int y0 = std::max(0,
                                        static_cast<int>(height) / 2 - side / 2);
                                    const int x1 = std::min(static_cast<int>(width),
                                        x0 + side);
                                    const int y1 = std::min(static_cast<int>(height),
                                        y0 + side);
                                    const uint32_t value =
                                        grayPixel(static_cast<uint8_t>(highCode));
                                    for (int y = y0; y < y1; ++y)
                                        for (int x = x0; x < x1; ++x)
                                            pixels[static_cast<size_t>(y) * width + x] =
                                                value;
                                }
                            }
                            const auto currentCenter = renderCenterLuma();
                            result.sourceVariation += std::fabs(
                                currentCenter.first - previousCenter.first);
                            result.outputVariation += std::fabs(
                                currentCenter.second - previousCenter.second);
                            previousCenter = currentCenter;
                        }
                        if (result.sourceVariation > 1e-9)
                            result.residualRatio =
                                result.outputVariation / result.sourceVariation;
                        return result;
                    };

                    // Isolate detector families so a "full sensitivity" probe
                    // cannot also win through the small-source path (and vice versa).
                    // The real runtime settings are restored before any normal replay
                    // case runs, so these diagnostics cannot affect behavior metrics.
                    const SafetySettings savedProbeSafety = m_safety;

                    // Full-screen probes: use a fixed local binarization threshold
                    // and disable compact/calibrated-region paths. The only changing
                    // full-screen gates are then the configured full sensitivity.
                    m_safety.localDeltaThreshold = 0.05f;
                    m_safety.smallFlashAreaThreshold = 2.0f;
                    m_safety.affectedAreaThreshold = 2.0f;
                    m_safety.coherenceThreshold = 2.0f;
                    m_safety.visualFieldAreaThreshold = 2.0f;
                    m_safety.patternScoreThreshold = 2.0f;
                    const auto fullHighOnly =
                        runScreeningProbe(139, 0.0);
                    const auto fullMediumHigh =
                        runScreeningProbe(148, 0.0);

                    // Small-source probes: restore the configured small thresholds,
                    // then make broad/coherent global paths impossible for this
                    // diagnostic. The compact-source family remains intact.
                    m_safety = savedProbeSafety;
                    m_safety.affectedAreaThreshold = 2.0f;
                    m_safety.globalDeltaThreshold = 2.0f;
                    m_safety.globalAreaThreshold = 2.0f;
                    m_safety.visualFieldAreaThreshold = 2.0f;
                    m_safety.patternScoreThreshold = 2.0f;
                    const auto smallHighOnly =
                        runScreeningProbe(168, 0.006);
                    const auto smallMediumHigh =
                        runScreeningProbe(168, 0.011);
                    m_safety = savedProbeSafety;

                    const auto probePath =
                        std::filesystem::path(reportPath).parent_path() /
                        L"screening-probes.json";
                    FILE* probeReport = nullptr;
                    if (_wfopen_s(&probeReport, probePath.c_str(), L"wb") != 0 ||
                        !probeReport)
                        return false;
                    std::fprintf(probeReport,
                        "{\n"
                        "  \"schema\": \"FLASHGUARD_SCREENING_PROBES/3\",\n"
                        "  \"status\": \"SUCCESS\",\n"
                        "  \"detector_family_isolation\": true,\n"
                        "  \"global_delta_only_full_probe\": true,\n"
                        "  \"background_code\": 96,\n"
                        "  \"fps\": %d,\n"
                        "  \"full_high_only_high_code\": 139,\n"
                        "  \"full_medium_high_high_code\": 148,\n"
                        "  \"small_probe_high_code\": 168,\n"
                        "  \"small_high_only_target_area_fraction\": 0.006,\n"
                        "  \"small_medium_high_target_area_fraction\": 0.011,\n"
                        "  \"full_high_only_residual_ratio\": %.8f,\n"
                        "  \"full_medium_high_residual_ratio\": %.8f,\n"
                        "  \"small_high_only_residual_ratio\": %.8f,\n"
                        "  \"small_medium_high_residual_ratio\": %.8f\n"
                        "}\n",
                        replayFps,
                        fullHighOnly.residualRatio,
                        fullMediumHigh.residualRatio,
                        smallHighOnly.residualRatio,
                        smallMediumHigh.residualRatio);
                    std::fclose(probeReport);
                }
            }

            const uint64_t movingFlowStart = flowFrames;

            resetCase();
            fillGray(24);
            for (int i = 0; i < (replayScreening ? 6 : 20); ++i)
                renderAndSample(nullptr);
            double movingGhostMae = 0.0;
            double movingInsideMae = 0.0;
            double movingEdgeMae = 0.0;
            double movingVacatedMean = 0.0;
            double movingVacatedP95Max = 0.0;
            double movingVacatedP99Max = 0.0;
            double movingVacatedPeak = 0.0;
            double movingVacatedArea02Max = 0.0;
            double movingVacatedArea05Max = 0.0;
            double movingTrailMeanErrorAuc = 0.0;
            std::vector<double> movingTrailP99Frames;
            std::vector<double> movingTrailArea05Frames;
            int movingVacatedFrames = 0;
            AuthorityDiagnosticAggregate movingTrailAuthority{};
            AuthorityDiagnosticAggregate movingRecoveryAuthority{};
            constexpr int squareSize = 64;
            const int movingFrames = replayScreening ?
                std::max(18, replayFps / 2) : std::max(45, replayFps * 3 / 2);
            const int settledX0 = 20;
            const int settledY0 = static_cast<int>(height) / 2 - squareSize / 2;
            for (int i = 0; i < settleFrames; ++i)
            {
                fillGray(24);
                const int x1 = std::min(settledX0 + squareSize, static_cast<int>(width));
                const int y1 = std::min(settledY0 + squareSize, static_cast<int>(height));
                for (int y = std::max(settledY0, 0); y < y1; ++y)
                    for (int x = std::max(settledX0, 0); x < x1; ++x)
                        pixels[static_cast<size_t>(y) * width + x] = grayPixel(235);
                renderAndSample(nullptr);
            }
            RECT previousMovingRect{
                settledX0, settledY0,
                std::min(settledX0 + squareSize, static_cast<int>(width)),
                std::min(settledY0 + squareSize, static_cast<int>(height))
            };
            const int movingTrailHistoryFrames = std::max(3, replayFps / 2);
            std::deque<RECT> movingTrailHistory;
            const auto trailBounds = [](const std::deque<RECT>& history) {
                RECT bounds = history.front();
                for (const RECT& rect : history)
                {
                    bounds.left = std::min(bounds.left, rect.left);
                    bounds.top = std::min(bounds.top, rect.top);
                    bounds.right = std::max(bounds.right, rect.right);
                    bounds.bottom = std::max(bounds.bottom, rect.bottom);
                }
                return bounds;
            };
            for (int i = 0; i < movingFrames; ++i)
            {
                fillGray(24);
                const int x0 = pingPongCoordinate(
                    settledX0,
                    static_cast<double>(i + 1) * 4.0 * motionFrameScale,
                    static_cast<int>(width) - squareSize);
                const int y0 = static_cast<int>(height) / 2 - squareSize / 2;
                const int x1 = std::min(x0 + squareSize, static_cast<int>(width));
                const int y1 = std::min(y0 + squareSize, static_cast<int>(height));
                for (int y = std::max(y0, 0); y < y1; ++y)
                    for (int x = std::max(x0, 0); x < x1; ++x)
                        pixels[static_cast<size_t>(y) * width + x] = grayPixel(235);
                const RECT square{ x0, y0, x1, y1 };
                movingTrailHistory.push_back(previousMovingRect);
                while (static_cast<int>(movingTrailHistory.size()) > movingTrailHistoryFrames)
                    movingTrailHistory.pop_front();
                const RECT movingTrailRect = trailBounds(movingTrailHistory);
                const FrameSample sample = renderAndSample(&square,
                    (writeVisuals && i % 10 == 0) ? L"bright_motion" : nullptr,
                    i, (replayScreening && m_benchmarkArchitectureMode < 4) ?
                        nullptr : &movingDiagnostics,
                    nullptr, nullptr, &movingTrailRect,
                    &movingTrailAuthority);
                movingGhostMae += sample.outsideMae;
                movingInsideMae += sample.insideMae;
                movingEdgeMae += sample.edgeMae;
                if (sample.vacatedSamples > 0)
                {
                    movingVacatedMean += sample.vacatedMean;
                    movingVacatedP95Max = std::max(
                        movingVacatedP95Max, sample.vacatedP95);
                    movingVacatedP99Max = std::max(
                        movingVacatedP99Max, sample.vacatedP99);
                    movingVacatedPeak = std::max(
                        movingVacatedPeak, sample.vacatedPeak);
                    movingVacatedArea02Max = std::max(
                        movingVacatedArea02Max, sample.vacatedAreaAbove02);
                    movingVacatedArea05Max = std::max(
                        movingVacatedArea05Max, sample.vacatedAreaAbove05);
                    movingTrailMeanErrorAuc += sample.vacatedMean * static_cast<double>(dt);
                    movingTrailP99Frames.push_back(sample.vacatedP99);
                    movingTrailArea05Frames.push_back(sample.vacatedAreaAbove05);
                    ++movingVacatedFrames;
                }
                previousMovingRect = square;
            }
            movingGhostMae /= static_cast<double>(movingFrames);
            movingInsideMae /= static_cast<double>(movingFrames);
            movingEdgeMae /= static_cast<double>(movingFrames);
            if (movingVacatedFrames > 0)
                movingVacatedMean /= static_cast<double>(movingVacatedFrames);
            const auto seriesMean = [](const std::vector<double>& values) {
                if (values.empty()) return 0.0;
                double sum = 0.0;
                for (double value : values) sum += value;
                return sum / static_cast<double>(values.size());
            };
            const auto seriesPercentile = [](std::vector<double> values, double q) {
                if (values.empty()) return 0.0;
                std::sort(values.begin(), values.end());
                const size_t index = std::min(values.size() - 1,
                    static_cast<size_t>(std::ceil(
                        q * static_cast<double>(values.size()))) - 1);
                return values[index];
            };
            const double movingTrailP99FrameMean = seriesMean(movingTrailP99Frames);
            const double movingTrailP99FrameP50 =
                seriesPercentile(movingTrailP99Frames, 0.50);
            const double movingTrailP99FrameP90 =
                seriesPercentile(movingTrailP99Frames, 0.90);
            const double movingTrailP99FrameP95 =
                seriesPercentile(movingTrailP99Frames, 0.95);
            const double movingTrailArea05FrameMean = seriesMean(movingTrailArea05Frames);
            const double movingTrailArea05FrameP95 =
                seriesPercentile(movingTrailArea05Frames, 0.95);

            // Recovery is explicitly censored instead of pretending the end of a
            // short observation window is the clear time. Measure the whole recent
            // footprint corridor and integrate residual severity even if it never clears.
            movingTrailHistory.push_back(previousMovingRect);
            while (static_cast<int>(movingTrailHistory.size()) > movingTrailHistoryFrames)
                movingTrailHistory.pop_front();
            const RECT recoveryTrailRect = trailBounds(movingTrailHistory);
            const int trailRecoveryFrames = replayScreening ?
                std::max(24, replayFps) : std::max(60, replayFps * 2);
            int trailClear01 = -1;
            int trailClear02 = -1;
            int trailClear05 = -1;
            int trailRecoveryFramesMeasured = 0;
            double trailRecoveryP99Auc = 0.0;
            double trailRecoveryArea05Auc = 0.0;
            double trailRecoveryP99Final = 0.0;
            for (int i = 0; i < trailRecoveryFrames; ++i)
            {
                fillGray(24);
                const FrameSample recovery = renderAndSample(
                    nullptr, nullptr, -1, nullptr, nullptr, nullptr,
                    &recoveryTrailRect, &movingRecoveryAuthority);
                ++trailRecoveryFramesMeasured;
                trailRecoveryP99Final = recovery.vacatedP99;
                trailRecoveryP99Auc += recovery.vacatedP99 * static_cast<double>(dt);
                trailRecoveryArea05Auc +=
                    recovery.vacatedAreaAbove05 * static_cast<double>(dt);
                if (trailClear01 < 0 && recovery.vacatedP99 <= 0.01) trailClear01 = i;
                if (trailClear02 < 0 && recovery.vacatedP99 <= 0.02) trailClear02 = i;
                if (trailClear05 < 0 && recovery.vacatedP99 <= 0.05) trailClear05 = i;
                if (trailClear01 >= 0 && trailClear02 >= 0 && trailClear05 >= 0) break;
            }
            const auto trailFramesToMs = [&](int frame) {
                return frame >= 0 ? 1000.0 * static_cast<double>(frame + 1) /
                    static_cast<double>(replayFps) : -1.0;
            };
            const double trailClear01Ms = trailFramesToMs(trailClear01);
            const double trailClear02Ms = trailFramesToMs(trailClear02);
            const double trailClear05Ms = trailFramesToMs(trailClear05);
            const double trailRecoveryMeasuredMs = 1000.0 *
                static_cast<double>(trailRecoveryFramesMeasured) /
                static_cast<double>(replayFps);
            const double trailRecoveryWindowMaxMs = 1000.0 *
                static_cast<double>(trailRecoveryFrames) /
                static_cast<double>(replayFps);
            const double trailClear01LowerBoundMs =
                trailClear01 >= 0 ? trailClear01Ms : trailRecoveryMeasuredMs;
            const double trailClear02LowerBoundMs =
                trailClear02 >= 0 ? trailClear02Ms : trailRecoveryMeasuredMs;
            const double trailClear05LowerBoundMs =
                trailClear05 >= 0 ? trailClear05Ms : trailRecoveryMeasuredMs;
            const uint64_t movingFlowFrames = flowFrames - movingFlowStart;

            // Bright oblique motion: real game objects rarely move on an exact
            // cardinal or 45-degree path. Keep the object present long enough for
            // its initial appearance protection to release, then measure motion.
            const uint64_t obliqueFlowStart = flowFrames;
            resetCase();
            constexpr int obliqueSize = 32;
            const int obliqueFrames = replayScreening ?
                1 : std::max(40, replayFps * 4 / 3);
            const int obliqueStartX = 80;
            const int obliqueStartY = 80;
            for (int i = 0; i < settleFrames; ++i)
            {
                fillGray(24);
                for (int y = obliqueStartY; y < obliqueStartY + obliqueSize; ++y)
                    for (int x = obliqueStartX; x < obliqueStartX + obliqueSize; ++x)
                        pixels[static_cast<size_t>(y) * width + x] = grayPixel(235);
                renderAndSample(nullptr);
            }
            double obliqueGhostMae = 0.0;
            double obliqueInsideMae = 0.0;
            double obliqueEdgeMae = 0.0;
            for (int i = 0; i < obliqueFrames; ++i)
            {
                fillGray(24);
                const int x0 = pingPongCoordinate(
                    obliqueStartX,
                    static_cast<double>(i + 1) * 3.0 * motionFrameScale,
                    static_cast<int>(width) - obliqueSize);
                const int y0 = pingPongCoordinate(
                    obliqueStartY,
                    static_cast<double>(i + 1) * 1.0 * motionFrameScale,
                    static_cast<int>(height) - obliqueSize);
                const int x1 = x0 + obliqueSize;
                const int y1 = y0 + obliqueSize;
                for (int y = y0; y < y1; ++y)
                    for (int x = x0; x < x1; ++x)
                        pixels[static_cast<size_t>(y) * width + x] = grayPixel(235);
                const RECT square{ x0, y0, x1, y1 };
                const FrameSample sample = renderAndSample(&square,
                    (writeVisuals && i % 10 == 0) ? L"bright_oblique" : nullptr,
                    i, replayScreening ? nullptr : &obliqueDiagnostics);
                obliqueGhostMae += sample.outsideMae;
                obliqueInsideMae += sample.insideMae;
                obliqueEdgeMae += sample.edgeMae;
            }
            obliqueGhostMae /= static_cast<double>(obliqueFrames);
            obliqueInsideMae /= static_cast<double>(obliqueFrames);
            obliqueEdgeMae /= static_cast<double>(obliqueFrames);
            const uint64_t obliqueFlowFrames = flowFrames - obliqueFlowStart;

            // Quake-like local-motion regression: a small, medium-contrast object
            // is intentionally below the coarse NVOFA scheduling thresholds. It
            // exercises the anchor-only portable motion classifier.
            const uint64_t smallMovingFlowStart = flowFrames;
            resetCase();
            fillGray(72);
            for (int i = 0; i < warmupFrames; ++i) renderAndSample(nullptr);
            double smallMovingGhostMae = 0.0;
            double smallMovingVacatedP99Max = 0.0;
            double smallMovingVacatedPeak = 0.0;
            constexpr int smallSquareSize = 24;
            const int smallMovingFrames = replayScreening ?
                std::max(18, replayFps / 2) : std::max(45, replayFps * 3 / 2);
            RECT previousSmallRect{};
            bool havePreviousSmallRect = false;
            for (int i = 0; i < smallMovingFrames; ++i)
            {
                fillGray(72);
                const int x0 = pingPongCoordinate(
                    40,
                    static_cast<double>(i) * 2.0 * motionFrameScale,
                    static_cast<int>(width) - smallSquareSize);
                const int y0 = static_cast<int>(height) / 3 - smallSquareSize / 2;
                const int x1 = std::min(x0 + smallSquareSize, static_cast<int>(width));
                const int y1 = std::min(y0 + smallSquareSize, static_cast<int>(height));
                for (int y = std::max(y0, 0); y < y1; ++y)
                    for (int x = std::max(x0, 0); x < x1; ++x)
                        pixels[static_cast<size_t>(y) * width + x] = grayPixel(150);
                const RECT square{ x0, y0, x1, y1 };
                const FrameSample sample = renderAndSample(&square,
                    (writeVisuals && i % 10 == 0) ? L"small_motion" : nullptr,
                    i, replayScreening ? nullptr : &smallMovingDiagnostics,
                    nullptr, nullptr, havePreviousSmallRect ? &previousSmallRect : nullptr);
                smallMovingGhostMae += sample.outsideMae;
                smallMovingVacatedP99Max = std::max(
                    smallMovingVacatedP99Max, sample.vacatedP99);
                smallMovingVacatedPeak = std::max(
                    smallMovingVacatedPeak, sample.vacatedPeak);
                previousSmallRect = square;
                havePreviousSmallRect = true;
            }
            smallMovingGhostMae /= static_cast<double>(smallMovingFrames);
            const uint64_t smallMovingFlowFrames =
                flowFrames - smallMovingFlowStart;
            const uint64_t panFlowStart = flowFrames;

            resetCase();
            fillPanPattern(0);
            const int panWarmupFrames = std::max(6, replayFps / 5);
            for (int i = 0; i < panWarmupFrames; ++i) renderAndSample(nullptr);
            double panMae = 0.0;
            double panCameraMotion = 0.0;
            double panAffectedArea = 0.0;
            double panCoherence = 0.0;
            double panFlashEnergy = 0.0;
            float panCameraMotionMax = 0.0f;
            const int panFrames = replayScreening ?
                std::max(12, replayFps * 2 / 5) : std::max(45, replayFps * 3 / 2);
            for (int i = 0; i < panFrames; ++i)
            {
                fillPanPattern(static_cast<int>(std::lround(
                    static_cast<double>(i + 1) * 3.0 * motionFrameScale)));
                panMae += renderAndSample(nullptr,
                    (writeVisuals && i % 10 == 0) ? L"pan" : nullptr,
                    i).mae;
                panCameraMotion += m_latestStats.cameraMotionScore;
                panAffectedArea += m_latestStats.affectedArea;
                panCoherence += m_latestStats.directionalCoherence;
                panFlashEnergy += m_latestStats.flashEnergy;
                panCameraMotionMax = std::max(
                    panCameraMotionMax, m_latestStats.cameraMotionScore);
            }
            panMae /= static_cast<double>(panFrames);
            panCameraMotion /= static_cast<double>(panFrames);
            panAffectedArea /= static_cast<double>(panFrames);
            panCoherence /= static_cast<double>(panFrames);
            panFlashEnergy /= static_cast<double>(panFrames);
            const uint64_t panFlowFrames = flowFrames - panFlowStart;

            const uint64_t fastPanFlowStart = flowFrames;
            resetCase();
            fillPanPattern(0);
            for (int i = 0; i < (replayScreening ? 1 : panWarmupFrames); ++i)
                renderAndSample(nullptr);
            const int auxiliaryPanFrames = replayScreening ? 1 : panFrames;
            double fastPanMae = 0.0;
            for (int i = 0; i < auxiliaryPanFrames; ++i)
            {
                fillPanPattern(static_cast<int>(std::lround(
                    static_cast<double>(i + 1) * 8.0 * motionFrameScale)));
                fastPanMae += renderAndSample(nullptr).mae;
            }
            fastPanMae /= static_cast<double>(auxiliaryPanFrames);
            const uint64_t fastPanFlowFrames = flowFrames - fastPanFlowStart;

            const uint64_t extremePanFlowStart = flowFrames;
            resetCase();
            fillPanPattern(0);
            for (int i = 0; i < (replayScreening ? 1 : panWarmupFrames); ++i)
                renderAndSample(nullptr);
            double extremePanMae = 0.0;
            for (int i = 0; i < auxiliaryPanFrames; ++i)
            {
                fillPanPattern(static_cast<int>(std::lround(
                    static_cast<double>(i + 1) * 16.0 * motionFrameScale)));
                extremePanMae += renderAndSample(nullptr).mae;
            }
            extremePanMae /= static_cast<double>(auxiliaryPanFrames);
            const uint64_t extremePanFlowFrames = flowFrames - extremePanFlowStart;

            // REPLAY/5 desktop-motion corpus. Smooth and snapped scroll use the
            // same requested physical velocity; only rasterization differs.
            const double requestedScrollVelocityPxPerSecond = 45.0 * motionScale;
            const int scrollFrames = replayScreening ?
                1 : std::max(30, replayFps);
            std::vector<double> smoothScrollOffsets;
            std::vector<double> snappedScrollOffsets;
            smoothScrollOffsets.reserve(scrollFrames);
            snappedScrollOffsets.reserve(scrollFrames);

            resetCase();
            fillTextScroll(0.0, true);
            for (int i = 0; i < warmupFrames; ++i) renderAndSample(nullptr);
            double smoothScrollMae = 0.0;
            double smoothFinalOffset = 0.0;
            for (int i = 0; i < scrollFrames; ++i)
            {
                const double offset = static_cast<double>(i + 1) *
                    requestedScrollVelocityPxPerSecond / static_cast<double>(replayFps);
                smoothFinalOffset = offset;
                smoothScrollOffsets.push_back(offset);
                fillTextScroll(offset, true);
                smoothScrollMae += renderAndSample(nullptr,
                    (writeVisuals && i % std::max(1, replayFps / 10) == 0) ?
                        L"smooth_subpixel_scroll" : nullptr,
                    i, &smoothScrollDiagnostics).mae;
            }
            smoothScrollMae /= static_cast<double>(scrollFrames);

            const int recoveryFrames = replayScreening ?
                3 : std::max(15, replayFps / 2);
            int recoveryStableFrames = 0;
            int recoveryFrame = -1;
            double scrollStopPeakMae = 0.0;
            for (int i = 0; i < recoveryFrames; ++i)
            {
                fillTextScroll(smoothFinalOffset, true);
                const FrameSample sample = renderAndSample(nullptr);
                scrollStopPeakMae = std::max(scrollStopPeakMae, sample.mae);
                if (sample.mae < 0.003)
                {
                    if (++recoveryStableFrames >= 3 && recoveryFrame < 0)
                        recoveryFrame = i - 2;
                }
                else
                {
                    recoveryStableFrames = 0;
                }
            }
            const double scrollStopRecoveryMs = recoveryFrame >= 0 ?
                1000.0 * static_cast<double>(recoveryFrame + 1) / replayFps :
                1000.0 * static_cast<double>(recoveryFrames) / replayFps;

            resetCase();
            fillTextScroll(0.0, false);
            for (int i = 0; i < warmupFrames; ++i) renderAndSample(nullptr);
            double snappedScrollMae = 0.0;
            for (int i = 0; i < scrollFrames; ++i)
            {
                const double requestedOffset = static_cast<double>(i + 1) *
                    requestedScrollVelocityPxPerSecond / static_cast<double>(replayFps);
                const double realizedOffset = std::lround(requestedOffset);
                snappedScrollOffsets.push_back(realizedOffset);
                fillTextScroll(realizedOffset, false);
                snappedScrollMae += renderAndSample(nullptr,
                    (writeVisuals && i % std::max(1, replayFps / 10) == 0) ?
                        L"integer_snapped_scroll" : nullptr,
                    i, &snappedScrollDiagnostics).mae;
            }
            snappedScrollMae /= static_cast<double>(scrollFrames);

            // Saturated-red translation exercises the chromatic path without an
            // intrinsic flash. Record-only until the REPLAY/5 baseline is reviewed.
            resetCase();
            fillGray(24);
            for (int i = 0; i < warmupFrames; ++i) renderAndSample(nullptr);
            constexpr int redSquareSize = 48;
            const int redMotionFrames = replayScreening ?
                1 : std::max(30, replayFps);
            double redMotionGhostMae = 0.0;
            double redMotionInsideMae = 0.0;
            for (int i = 0; i < redMotionFrames; ++i)
            {
                fillGray(24);
                const int x0 = pingPongCoordinate(32,
                    static_cast<double>(i + 1) * 2.0 * motionFrameScale,
                    static_cast<int>(width) - redSquareSize);
                const int y0 = static_cast<int>(height) / 2 - redSquareSize / 2;
                const int x1 = x0 + redSquareSize;
                const int y1 = y0 + redSquareSize;
                for (int y = y0; y < y1; ++y)
                    for (int x = x0; x < x1; ++x)
                        pixels[static_cast<size_t>(y) * width + x] = rgbPixel(235, 0, 0);
                const RECT square{ x0, y0, x1, y1 };
                const FrameSample sample = renderAndSample(&square);
                redMotionGhostMae += sample.outsideMae;
                redMotionInsideMae += sample.insideMae;
            }
            redMotionGhostMae /= static_cast<double>(redMotionFrames);
            redMotionInsideMae /= static_cast<double>(redMotionFrames);

            // A translating object that genuinely flashes must still be attenuated;
            // correspondence is not allowed to become a blanket motion exemption.
            resetCase();
            fillGray(24);
            FrameSample previousMovingFlash = renderAndSample(nullptr);
            for (int i = 1; i < warmupFrames; ++i)
                previousMovingFlash = renderAndSample(nullptr);
            const int movingFlashFrames = replayScreening ?
                std::max(15, replayFps / 2) : std::max(60, replayFps * 2);
            double movingFlashRawVariation = 0.0;
            double movingFlashOutputVariation = 0.0;
            AuthorityDiagnosticAggregate movingFlashTransitionAuthority{};
            AuthorityDiagnosticAggregate movingFlashStableAuthority{};
            bool haveMovingFlashPhase = false;
            bool previousMovingFlashHigh = false;
            for (int i = 0; i < movingFlashFrames; ++i)
            {
                fillGray(24);
                const int x0 = pingPongCoordinate(32,
                    static_cast<double>(i + 1) * 2.0 * motionFrameScale,
                    static_cast<int>(width) - redSquareSize);
                const int y0 = static_cast<int>(height) / 2 - redSquareSize / 2;
                const bool high = std::fmod((static_cast<double>(i) + 0.5) *
                    10.0 / static_cast<double>(replayFps), 1.0) < 0.5;
                const uint32_t value = grayPixel(high ? 235 : 45);
                for (int y = y0; y < y0 + redSquareSize; ++y)
                    for (int x = x0; x < x0 + redSquareSize; ++x)
                        pixels[static_cast<size_t>(y) * width + x] = value;
                const RECT movingFlashRect{
                    x0, y0, x0 + redSquareSize, y0 + redSquareSize };
                const bool phaseTransition =
                    !haveMovingFlashPhase || high != previousMovingFlashHigh;
                AuthorityDiagnosticAggregate& phaseAuthority = phaseTransition ?
                    movingFlashTransitionAuthority : movingFlashStableAuthority;
                const FrameSample currentMovingFlash = renderAndSample(
                    nullptr, nullptr, -1, nullptr, nullptr, nullptr,
                    &movingFlashRect, &phaseAuthority);
                movingFlashRawVariation += std::fabs(
                    currentMovingFlash.sourceMean - previousMovingFlash.sourceMean);
                movingFlashOutputVariation += std::fabs(
                    currentMovingFlash.outputMean - previousMovingFlash.outputMean);
                previousMovingFlash = currentMovingFlash;
                previousMovingFlashHigh = high;
                haveMovingFlashPhase = true;
            }
            const double movingFlashReduction = movingFlashRawVariation > 1e-9 ?
                1.0 - movingFlashOutputVariation / movingFlashRawVariation : 0.0;

            const auto realizationPath =
                std::filesystem::path(reportPath).parent_path() / L"motion-realization.json";
            FILE* realizationReport = nullptr;
            if (_wfopen_s(&realizationReport, realizationPath.c_str(), L"wb") != 0 ||
                !realizationReport)
                return false;
            std::fprintf(realizationReport,
                "{\n  \"schema\": \"MOTION_REALIZATION/1\",\n"
                "  \"fps\": %d,\n  \"motion_scale\": %.3f,\n"
                "  \"requested_scroll_velocity_px_per_second\": %.6f,\n"
                "  \"smooth_offsets_px\": [",
                replayFps, motionScale, requestedScrollVelocityPxPerSecond);
            for (size_t i = 0; i < smoothScrollOffsets.size(); ++i)
                std::fprintf(realizationReport, "%.6f%s", smoothScrollOffsets[i],
                    i + 1 < smoothScrollOffsets.size() ? "," : "");
            std::fputs("],\n  \"snapped_offsets_px\": [", realizationReport);
            for (size_t i = 0; i < snappedScrollOffsets.size(); ++i)
                std::fprintf(realizationReport, "%.0f%s", snappedScrollOffsets[i],
                    i + 1 < snappedScrollOffsets.size() ? "," : "");
            std::fputs("]\n}\n", realizationReport);
            std::fclose(realizationReport);

            const bool screeningMetricsFinite =
                std::isfinite(staticMae) && std::isfinite(flashReduction) &&
                std::isfinite(movingGhostMae) &&
                std::isfinite(smallMovingGhostMae) &&
                std::isfinite(panMae) && std::isfinite(movingFlashReduction) &&
                std::isfinite(movingVacatedMean) &&
                std::isfinite(smallMovingVacatedP99Max);
            const bool fullMetricsFinite = std::isfinite(staticMae) &&
                std::isfinite(flashReduction) && std::isfinite(movingGhostMae) &&
                std::isfinite(movingInsideMae) && std::isfinite(movingEdgeMae) &&
                std::isfinite(obliqueGhostMae) && std::isfinite(obliqueInsideMae) &&
                std::isfinite(obliqueEdgeMae) &&
                std::isfinite(smallMovingGhostMae) && std::isfinite(panMae) &&
                std::isfinite(fastPanMae) && std::isfinite(extremePanMae) &&
                std::isfinite(smoothScrollMae) && std::isfinite(snappedScrollMae) &&
                std::isfinite(scrollStopRecoveryMs) &&
                std::isfinite(redMotionGhostMae) && std::isfinite(redMotionInsideMae) &&
                std::isfinite(movingFlashReduction);
            const bool commonPass = staticMae < 0.005 &&
                rawVariation > 0.10 && flashReduction > 0.90 &&
                movingGhostMae < 0.005 && smallMovingGhostMae < 0.003 &&
                panMae < 0.010 && flowFrames > 0;
            const bool pass = replayScreening ?
                (screeningMetricsFinite && commonPass) :
                (fullMetricsFinite && commonPass &&
                    fastPanMae < 0.020 && extremePanMae < 0.030);

            constexpr const char* motionDiagnosticNames[12] = {
                "global_flow", "current_surface", "vacated_surface",
                "disocclusion_infill", "portable_matcher", "hardware_combined",
                "effective_motion", "repeated_risk", "verified_flash_override",
                "coarse_gpu_motion", "cpu_motion_prior", "temporal_mask"
            };
            const auto diagnosticPath =
                std::filesystem::path(reportPath).parent_path() /
                L"motion-diagnostics.json";
            FILE* diagnosticReport = nullptr;
            if (_wfopen_s(&diagnosticReport, diagnosticPath.c_str(), L"wb") != 0 ||
                !diagnosticReport)
                return false;
            std::fprintf(diagnosticReport,
                "{\n"
                "  \"schema\": \"MOTION_DIAGNOSTICS/2\",\n"
                "  \"fps\": %d,\n"
                "  \"motion_scale\": %.3f,\n"
                "  \"activation_threshold\": 0.5,\n"
                "  \"sampling_stride\": 1,\n"
                "  \"temporal_sampling_hz\": %d,\n"
                "  \"layout\": \"three_full_resolution_mrts\",\n"
                "  \"cases\": {\n",
                replayFps, motionScale, std::min(replayFps, 30));
            const auto writeMotionDiagnosticCase =
                [&](const char* caseName,
                    const MotionDiagnosticAggregate& aggregate, bool trailingComma)
            {
                const auto mean = [](double sum, uint64_t count) {
                    return count ? sum / static_cast<double>(count) : 0.0;
                };
                const auto fraction = [](uint64_t active, uint64_t count) {
                    return count ? static_cast<double>(active) /
                        static_cast<double>(count) : 0.0;
                };
                std::fprintf(diagnosticReport, "    \"%s\": {\n", caseName);
                for (size_t metric = 0; metric < 12; ++metric)
                {
                    std::fprintf(diagnosticReport,
                        "      \"%s\":{\"mean\":%.8f,\"max\":%.8f,"
                        "\"active_fraction\":%.8f,\"inside_mean\":%.8f,"
                        "\"inside_max\":%.8f,\"inside_active_fraction\":%.8f,"
                        "\"outside_mean\":%.8f,\"outside_max\":%.8f,"
                        "\"outside_active_fraction\":%.8f,"
                        "\"changed_mean\":%.8f,\"changed_max\":%.8f,"
                        "\"changed_active_fraction\":%.8f}%s\n",
                        motionDiagnosticNames[metric],
                        mean(aggregate.sum[metric], aggregate.count[metric]),
                        aggregate.maximum[metric],
                        fraction(aggregate.active[metric], aggregate.count[metric]),
                        mean(aggregate.insideSum[metric], aggregate.insideCount[metric]),
                        aggregate.insideMaximum[metric],
                        fraction(aggregate.insideActive[metric],
                            aggregate.insideCount[metric]),
                        mean(aggregate.outsideSum[metric], aggregate.outsideCount[metric]),
                        aggregate.outsideMaximum[metric],
                        fraction(aggregate.outsideActive[metric],
                            aggregate.outsideCount[metric]),
                        mean(aggregate.changedSum[metric], aggregate.changedCount[metric]),
                        aggregate.changedMaximum[metric],
                        fraction(aggregate.changedActive[metric],
                            aggregate.changedCount[metric]),
                        metric + 1 < 12 ? "," : "");
                }
                std::fprintf(diagnosticReport, "    }%s\n",
                    trailingComma ? "," : "");
            };
            writeMotionDiagnosticCase(
                "quarter_flash_15hz", quarterFlashDiagnostics, true);
            writeMotionDiagnosticCase(
                "moving_square", movingDiagnostics, true);
            writeMotionDiagnosticCase(
                "bright_oblique", obliqueDiagnostics, true);
            writeMotionDiagnosticCase(
                "small_moving_square", smallMovingDiagnostics, true);
            writeMotionDiagnosticCase(
                "smooth_subpixel_scroll", smoothScrollDiagnostics, true);
            writeMotionDiagnosticCase(
                "integer_snapped_scroll", snappedScrollDiagnostics, false);
            std::fputs("  }\n}\n", diagnosticReport);
            std::fclose(diagnosticReport);

            const auto authorityPath =
                std::filesystem::path(reportPath).parent_path() /
                L"authority-diagnostics.json";
            FILE* authorityReport = nullptr;
            if (_wfopen_s(&authorityReport, authorityPath.c_str(), L"wb") != 0 ||
                !authorityReport)
                return false;
            const auto writeAuthorityCase =
                [&](const char* name, const AuthorityDiagnosticAggregate& aggregate,
                    bool trailingComma)
            {
                constexpr const char* channelNames[4] = {
                    "preprocess_luma_delta", "architecture_luma_delta",
                    "current_event_strength", "surface_memory_strength"
                };
                const double errorFraction = aggregate.trailPixels ?
                    static_cast<double>(aggregate.errorPixels) /
                        static_cast<double>(aggregate.trailPixels) : 0.0;
                const double meanError = aggregate.errorPixels ?
                    aggregate.errorSum / static_cast<double>(aggregate.errorPixels) : 0.0;
                const double unexplainedFraction = aggregate.errorPixels ?
                    static_cast<double>(aggregate.unexplainedErrorPixels) /
                        static_cast<double>(aggregate.errorPixels) : 0.0;
                std::fprintf(authorityReport,
                    "    \"%s\":{\n"
                    "      \"trail_pixel_count\":%llu,\n"
                    "      \"error_pixel_count\":%llu,\n"
                    "      \"error_pixel_fraction\":%.8f,\n"
                    "      \"mean_error_on_error_pixels\":%.8f,\n"
                    "      \"unexplained_error_fraction\":%.8f,\n"
                    "      \"channels\":{\n",
                    name,
                    static_cast<unsigned long long>(aggregate.trailPixels),
                    static_cast<unsigned long long>(aggregate.errorPixels),
                    errorFraction, meanError, unexplainedFraction);
                for (size_t channel = 0; channel < 4; ++channel)
                {
                    const double mean = aggregate.errorPixels ?
                        aggregate.sum[channel] /
                            static_cast<double>(aggregate.errorPixels) : 0.0;
                    const double activeFraction = aggregate.errorPixels ?
                        static_cast<double>(aggregate.active[channel]) /
                            static_cast<double>(aggregate.errorPixels) : 0.0;
                    const double weightedMean = aggregate.errorSum > 0.0 ?
                        aggregate.errorWeightedSum[channel] / aggregate.errorSum : 0.0;
                    const double sampleMean = aggregate.trailPixels ?
                        aggregate.sampleSum[channel] /
                            static_cast<double>(aggregate.trailPixels) : 0.0;
                    const double sampleActiveFraction = aggregate.trailPixels ?
                        static_cast<double>(aggregate.sampleActive[channel]) /
                            static_cast<double>(aggregate.trailPixels) : 0.0;
                    std::fprintf(authorityReport,
                        "        \"%s\":{\"mean_on_sample_pixels\":%.8f,"
                        "\"active_fraction_on_sample_pixels\":%.8f,"
                        "\"mean_on_error_pixels\":%.8f,\"max\":%.8f,"
                        "\"active_fraction_on_error_pixels\":%.8f,"
                        "\"error_weighted_mean\":%.8f}%s\n",
                        channelNames[channel], sampleMean, sampleActiveFraction, mean,
                        aggregate.maximum[channel], activeFraction, weightedMean,
                        channel + 1 < 4 ? "," : "");
                }
                std::fputs("      },\n      \"geometry_on_error_pixels\":{\n",
                    authorityReport);
                constexpr const char* geometryNames[8] = {
                    "global_flow", "current_surface", "vacated", "infill",
                    "portable", "hardware", "effective_motion", "repeated_risk"
                };
                for (size_t metric = 0; metric < 8; ++metric)
                {
                    const double mean = aggregate.errorPixels ?
                        aggregate.geometrySum[metric] /
                            static_cast<double>(aggregate.errorPixels) : 0.0;
                    const double activeFraction = aggregate.errorPixels ?
                        static_cast<double>(aggregate.geometryActive[metric]) /
                            static_cast<double>(aggregate.errorPixels) : 0.0;
                    const double weightedMean = aggregate.errorSum > 0.0 ?
                        aggregate.geometryErrorWeightedSum[metric] / aggregate.errorSum : 0.0;
                    const double sampleMean = aggregate.trailPixels ?
                        aggregate.geometrySampleSum[metric] /
                            static_cast<double>(aggregate.trailPixels) : 0.0;
                    const double sampleActiveFraction = aggregate.trailPixels ?
                        static_cast<double>(aggregate.geometrySampleActive[metric]) /
                            static_cast<double>(aggregate.trailPixels) : 0.0;
                    std::fprintf(authorityReport,
                        "        \"%s\":{\"mean_on_sample_pixels\":%.8f,"
                        "\"active_fraction_on_sample_pixels\":%.8f,"
                        "\"mean_on_error_pixels\":%.8f,\"max\":%.8f,"
                        "\"active_fraction_on_error_pixels\":%.8f,"
                        "\"error_weighted_mean\":%.8f}%s\n",
                        geometryNames[metric], sampleMean, sampleActiveFraction, mean,
                        aggregate.geometryMaximum[metric], activeFraction, weightedMean,
                        metric + 1 < 8 ? "," : "");
                }
                std::fprintf(authorityReport, "      }\n    }%s\n",
                    trailingComma ? "," : "");
            };
            std::fputs(
                "{\n  \"schema\":\"FLASHGUARD_AUTHORITY_DIAGNOSTICS/4\",\n"
                "  \"scope\":\"risk-architecture sample pixels plus >0.05 changed/error pixels; moving flash split into transition and stable phase frames\",\n"
                "  \"delta_active_threshold\":0.01,\n"
                "  \"strength_active_threshold\":0.05,\n"
                "  \"cases\":{\n", authorityReport);
            writeAuthorityCase("moving_square", movingTrailAuthority, true);
            writeAuthorityCase("moving_flash_transition",
                movingFlashTransitionAuthority, true);
            writeAuthorityCase("moving_flash_stable",
                movingFlashStableAuthority, true);
            writeAuthorityCase("moving_square_recovery", movingRecoveryAuthority, false);
            std::fputs("  }\n}\n", authorityReport);
            std::fclose(authorityReport);

            const auto trailMetricsPath =
                std::filesystem::path(reportPath).parent_path() / L"trail-metrics.json";
            FILE* trailMetricsReport = nullptr;
            if (_wfopen_s(&trailMetricsReport, trailMetricsPath.c_str(), L"wb") != 0 ||
                !trailMetricsReport)
                return false;
            std::fprintf(trailMetricsReport,
                "{\n"
                "  \"schema\": \"FLASHGUARD_TRAIL_METRICS/2\",\n"
                "  \"status\": \"SUCCESS\",\n"
                "  \"test_scope\": \"%s\",\n"
                "  \"width\": %u,\n  \"height\": %u,\n  \"fps\": %d,\n"
                "  \"motion_scale\": %.3f,\n  \"metric_sample_stride\": %u,\n"
                "  \"moving_square_trail_history_ms\": %.3f,\n"
                "  \"moving_square_vacated_mean_mae\": %.8f,\n"
                "  \"moving_square_vacated_p95_max\": %.8f,\n"
                "  \"moving_square_vacated_p99_max\": %.8f,\n"
                "  \"moving_square_vacated_peak\": %.8f,\n"
                "  \"moving_square_vacated_area_above_0_02_max\": %.8f,\n"
                "  \"moving_square_vacated_area_above_0_05_max\": %.8f,\n"
                "  \"moving_square_trail_p99_frame_mean\": %.8f,\n"
                "  \"moving_square_trail_p99_frame_p50\": %.8f,\n"
                "  \"moving_square_trail_p99_frame_p90\": %.8f,\n"
                "  \"moving_square_trail_p99_frame_p95\": %.8f,\n"
                "  \"moving_square_trail_area_above_0_05_frame_mean\": %.8f,\n"
                "  \"moving_square_trail_area_above_0_05_frame_p95\": %.8f,\n"
                "  \"moving_square_trail_mean_error_auc\": %.8f,\n"
                "  \"moving_square_recovery_frames_measured\": %d,\n"
                "  \"moving_square_recovery_window_max_ms\": %.3f,\n"
                "  \"moving_square_recovery_p99_auc\": %.8f,\n"
                "  \"moving_square_recovery_area_above_0_05_auc\": %.8f,\n"
                "  \"moving_square_recovery_p99_final\": %.8f,\n"
                "  \"moving_square_clear_to_0_01_observed\": %s,\n"
                "  \"moving_square_clear_to_0_02_observed\": %s,\n"
                "  \"moving_square_clear_to_0_05_observed\": %s,\n"
                "  \"moving_square_clear_to_0_01_ms\": %.3f,\n"
                "  \"moving_square_clear_to_0_02_ms\": %.3f,\n"
                "  \"moving_square_clear_to_0_05_ms\": %.3f,\n"
                "  \"moving_square_clear_to_0_01_lower_bound_ms\": %.3f,\n"
                "  \"moving_square_clear_to_0_02_lower_bound_ms\": %.3f,\n"
                "  \"moving_square_clear_to_0_05_lower_bound_ms\": %.3f,\n"
                "  \"small_moving_square_vacated_p99_max\": %.8f,\n"
                "  \"small_moving_square_vacated_peak\": %.8f\n"
                "}\n",
                replayScreening ? "screening" : "full",
                width, height, replayFps, motionScale, static_cast<unsigned>(sampleStride),
                1000.0 * static_cast<double>(movingTrailHistoryFrames) /
                    static_cast<double>(replayFps),
                movingVacatedMean, movingVacatedP95Max, movingVacatedP99Max,
                movingVacatedPeak, movingVacatedArea02Max, movingVacatedArea05Max,
                movingTrailP99FrameMean, movingTrailP99FrameP50, movingTrailP99FrameP90,
                movingTrailP99FrameP95, movingTrailArea05FrameMean,
                movingTrailArea05FrameP95, movingTrailMeanErrorAuc,
                trailRecoveryFramesMeasured, trailRecoveryWindowMaxMs,
                trailRecoveryP99Auc, trailRecoveryArea05Auc, trailRecoveryP99Final,
                trailClear01 >= 0 ? "true" : "false",
                trailClear02 >= 0 ? "true" : "false",
                trailClear05 >= 0 ? "true" : "false",
                trailClear01Ms, trailClear02Ms, trailClear05Ms,
                trailClear01LowerBoundMs, trailClear02LowerBoundMs,
                trailClear05LowerBoundMs,
                smallMovingVacatedP99Max, smallMovingVacatedPeak);
            std::fclose(trailMetricsReport);

            FILE* report = nullptr;
            if (_wfopen_s(&report, reportPath.c_str(), L"wb") != 0 || !report)
                return false;
            std::fprintf(report,
                "{\n"
                "  \"schema\": \"FLASHGUARD_REPLAY/6\",\n"
                "  \"status\": \"%s\",\n"
                "  \"width\": %u,\n"
                "  \"height\": %u,\n"
                "  \"fps\": %d,\n"
                "  \"motion_scale\": %.3f,\n"
                "  \"static_mae\": %.8f,\n"
                "  \"flash_raw_variation\": %.8f,\n"
                "  \"flash_output_variation\": %.8f,\n"
                "  \"flash_reduction\": %.8f,\n"
                "  \"moving_square_ghost_mae\": %.8f,\n"
                "  \"moving_square_inside_mae\": %.8f,\n"
                "  \"moving_square_edge_mae\": %.8f,\n"
                "  \"bright_oblique_ghost_mae\": %.8f,\n"
                "  \"bright_oblique_inside_mae\": %.8f,\n"
                "  \"bright_oblique_edge_mae\": %.8f,\n"
                "  \"small_moving_square_ghost_mae\": %.8f,\n"
                "  \"pan_mae\": %.8f,\n"
                "  \"fast_pan_mae\": %.8f,\n"
                "  \"extreme_pan_mae\": %.8f,\n"
                "  \"smooth_subpixel_scroll_mae\": %.8f,\n"
                "  \"integer_snapped_scroll_mae\": %.8f,\n"
                "  \"scroll_stop_peak_mae\": %.8f,\n"
                "  \"scroll_stop_recovery_ms\": %.3f,\n"
                "  \"saturated_red_motion_ghost_mae\": %.8f,\n"
                "  \"saturated_red_motion_inside_mae\": %.8f,\n"
                "  \"moving_flash_reduction\": %.8f,\n"
                "  \"requested_scroll_velocity_px_per_second\": %.6f,\n"
                "  \"pan_camera_motion_mean\": %.8f,\n"
                "  \"pan_camera_motion_max\": %.8f,\n"
                "  \"pan_affected_area_mean\": %.8f,\n"
                "  \"pan_coherence_mean\": %.8f,\n"
                "  \"pan_flash_energy_mean\": %.8f,\n"
                "  \"nvof_grid\": %u,\n"
                "  \"static_flow_frames\": %llu,\n"
                "  \"flash_flow_frames\": %llu,\n"
                "  \"moving_flow_frames\": %llu,\n"
                "  \"bright_oblique_flow_frames\": %llu,\n"
                "  \"small_moving_flow_frames\": %llu,\n"
                "  \"pan_flow_frames\": %llu,\n"
                "  \"fast_pan_flow_frames\": %llu,\n"
                "  \"extreme_pan_flow_frames\": %llu,\n"
                "  \"nvof_flow_frames\": %llu\n"
                "}\n",
                pass ? "SUCCESS" : "FAILED", width, height,
                replayFps, motionScale,
                staticMae, rawVariation, outputVariation, flashReduction,
                movingGhostMae, movingInsideMae, movingEdgeMae,
                obliqueGhostMae, obliqueInsideMae, obliqueEdgeMae,
                smallMovingGhostMae,
                panMae, fastPanMae, extremePanMae,
                smoothScrollMae, snappedScrollMae,
                scrollStopPeakMae, scrollStopRecoveryMs,
                redMotionGhostMae, redMotionInsideMae, movingFlashReduction,
                requestedScrollVelocityPxPerSecond,
                panCameraMotion, panCameraMotionMax,
                panAffectedArea, panCoherence, panFlashEnergy,
                static_cast<unsigned>(m_nvofGridSize),
                static_cast<unsigned long long>(staticFlowFrames),
                static_cast<unsigned long long>(flashFlowFrames),
                static_cast<unsigned long long>(movingFlowFrames),
                static_cast<unsigned long long>(obliqueFlowFrames),
                static_cast<unsigned long long>(smallMovingFlowFrames),
                static_cast<unsigned long long>(panFlowFrames),
                static_cast<unsigned long long>(fastPanFlowFrames),
                static_cast<unsigned long long>(extremePanFlowFrames),
                static_cast<unsigned long long>(flowFrames));
            std::fclose(report);
            return pass;
        }

        void Stop()
        {
            if (m_cleanupDone.exchange(true)) return;
            m_stopped.store(true, std::memory_order_release);
            m_liveFrameCv.notify_all();

            if (m_captureThread.joinable())
                m_captureThread.join();
            if (m_processingThread.joinable())
                m_processingThread.join();

            if (m_frameLatencyWaitableObject)
            {
                CloseHandle(m_frameLatencyWaitableObject);
                m_frameLatencyWaitableObject = nullptr;
            }
            std::scoped_lock lock(m_mutex);
            DestroyOpticalFlow();
            m_duplication = nullptr;
            m_dxgiOutput = nullptr;
        }

        void SetEmergencyShield(bool enabled)
        {
            m_manualShield.store(enabled, std::memory_order_release);
            if (enabled)
            {
                DiscardPendingLiveFrames();
                RenderShieldStep();
            }
            else
            {
                DiscardPendingLiveFrames();
                {
                    std::scoped_lock lock(m_mutex);
                    ResetDelayedPipeline();
                }
                // Resume is also an explicit recovery operation. Some drivers
                // leave Desktop Duplication in a permanently failing state after
                // a display-mode change unless the duplication object is rebuilt.
                m_captureRestartRequested.store(true, std::memory_order_release);
            }
        }

        void ToggleEmergencyShield()
        {
            SetEmergencyShield(!m_manualShield.load(std::memory_order_acquire));
        }

        void ToggleDebugOverlay()
        {
            const bool enabled = !m_debugEnabled.load(std::memory_order_acquire);
            m_debugEnabled.store(enabled, std::memory_order_release);
        }

        void RefreshHotkeyHint()
        {
            std::scoped_lock lock(m_mutex);
            UpdateHintTexture();
            m_hintUntilMs = NowMs() + 10000;
        }

        RuntimeOptions GetRuntimeOptions()
        {
            std::scoped_lock lock(m_mutex);
            return RuntimeOptions{
                m_profilePreset,
                m_contrastReduction,
                m_fullScreenSensitivity,
                m_smallSourceSensitivity,
                m_safety.lookaheadMs,
                m_displaySizePreset,
                m_viewingDistancePreset,
                m_debugEnabled.load(std::memory_order_acquire)
            };
        }

        void ApplyRuntimeOptions(const RuntimeOptions& options)
        {
            std::scoped_lock lock(m_mutex);
            m_profilePreset = std::clamp(options.profilePreset, 0, 2);
            if (m_profilePreset == 0)
            {
                m_safety.safeRiseRate = 2.20f;
                m_safety.safeFallRate = 2.50f;
                m_safety.minimumProtectionTime = 0.16f;
                m_safety.releaseTime = 0.30f;
                m_safety.redDesaturation = 0.55f;
            }
            else if (m_profilePreset == 1)
            {
                m_safety.safeRiseRate = 1.35f;
                m_safety.safeFallRate = 1.60f;
                m_safety.minimumProtectionTime = 0.22f;
                m_safety.releaseTime = 0.45f;
                m_safety.redDesaturation = 0.68f;
            }
            else
            {
                m_safety.safeRiseRate = 0.78f;
                m_safety.safeFallRate = 0.92f;
                m_safety.minimumProtectionTime = 0.35f;
                m_safety.releaseTime = 0.65f;
                m_safety.redDesaturation = 0.82f;
            }

            m_contrastReduction = std::clamp(options.contrastReduction, 0.0f, 1.0f);
            m_safety.subtleToneMap = m_contrastReduction > 0.0005f;
            m_safety.blackFloor = 0.12f * m_contrastReduction;
            m_safety.whiteCeiling = 1.0f - 0.24f * m_contrastReduction;

            m_fullScreenSensitivity = std::clamp(options.fullScreenSensitivity, 0, 2);
            if (m_fullScreenSensitivity == 0)
            {
                m_safety.globalDeltaThreshold = 0.20f;
                m_safety.affectedAreaThreshold = 0.22f;
                m_safety.coherenceThreshold = 0.78f;
                m_safety.globalAreaThreshold = 0.92f;
            }
            else if (m_fullScreenSensitivity == 1)
            {
                m_safety.globalDeltaThreshold = 0.16f;
                m_safety.affectedAreaThreshold = 0.18f;
                m_safety.coherenceThreshold = 0.70f;
                m_safety.globalAreaThreshold = 0.90f;
            }
            else
            {
                m_safety.globalDeltaThreshold = 0.12f;
                m_safety.affectedAreaThreshold = 0.12f;
                m_safety.coherenceThreshold = 0.65f;
                m_safety.globalAreaThreshold = 0.88f;
            }

            m_smallSourceSensitivity = std::clamp(options.smallSourceSensitivity, 0, 2);
            if (m_smallSourceSensitivity == 0)
            {
                m_safety.localDeltaThreshold = 0.12f;
                m_safety.smallFlashAreaThreshold = 0.015f;
                m_safety.localGlobalSupportThreshold = 0.050f;
                m_safety.flashEnergyThreshold = 0.045f;
            }
            else if (m_smallSourceSensitivity == 1)
            {
                m_safety.localDeltaThreshold = 0.10f;
                m_safety.smallFlashAreaThreshold = 0.008f;
                m_safety.localGlobalSupportThreshold = 0.035f;
                m_safety.flashEnergyThreshold = 0.030f;
            }
            else
            {
                m_safety.localDeltaThreshold = 0.08f;
                m_safety.smallFlashAreaThreshold = 0.004f;
                m_safety.localGlobalSupportThreshold = 0.020f;
                m_safety.flashEnergyThreshold = 0.018f;
            }

            m_displaySizePreset = std::clamp(options.displaySizePreset, 0, 3);
            constexpr float displaySizes[] = { 24.0f, 27.0f, 32.0f, 42.0f };
            m_safety.displayDiagonalInches = displaySizes[m_displaySizePreset];
            m_viewingDistancePreset = std::clamp(options.viewingDistancePreset, 0, 3);
            constexpr float distances[] = { 50.0f, 70.0f, 100.0f, 140.0f };
            m_safety.viewingDistanceCm = distances[m_viewingDistancePreset];

            // Production protection is GPU-instant only. CPU/readback analysis is
            // advisory and must never queue the image before the current-frame GPU
            // detector has a chance to protect it.
            const int latencyMs = 0;
            if (m_safety.lookaheadMs != latencyMs)
            {
                m_safety.lookaheadMs = latencyMs;
                m_rawFrames.clear();
                m_inputWidth = m_inputHeight = 0;
                m_inputFormat = DXGI_FORMAT_UNKNOWN;
                ResetDelayedPipeline();
            }
            // Settings without UI presets are reset explicitly so benchmark
            // overrides cannot leak into the next persistent-batch case.
            m_safety.cameraMotionSuppression = 0.32f;
            m_shaderTuning = ShaderTuningSettings{};
            m_benchmarkArchitectureMode = 0;
            m_benchmarkRiskOnlyNeutralLuma = 0.18f;
            m_benchmarkRiskOnlyGain = 0.92f;
            m_debugEnabled.store(options.debugOverlay, std::memory_order_release);
        }

        void ApplyBenchmarkTuning(const BenchmarkTuning& tuning)
        {
            if (!tuning.enabled) return;
            std::scoped_lock lock(m_mutex);
            const auto apply = [](float& target, float value, float low, float high) {
                if (std::isfinite(value) && value >= 0.0f)
                    target = std::clamp(value, low, high);
            };
            apply(m_safety.localDeltaThreshold,
                tuning.localDeltaThreshold, 0.02f, 0.30f);
            apply(m_safety.globalDeltaThreshold,
                tuning.globalDeltaThreshold, 0.04f, 0.40f);
            apply(m_safety.affectedAreaThreshold,
                tuning.affectedAreaThreshold, 0.02f, 0.60f);
            apply(m_safety.coherenceThreshold,
                tuning.coherenceThreshold, 0.30f, 0.98f);
            apply(m_safety.smallFlashAreaThreshold,
                tuning.smallFlashAreaThreshold, 0.001f, 0.05f);
            apply(m_safety.localGlobalSupportThreshold,
                tuning.localGlobalSupportThreshold, 0.005f, 0.15f);
            apply(m_safety.flashEnergyThreshold,
                tuning.flashEnergyThreshold, 0.005f, 0.12f);
            apply(m_safety.safeRiseRate,
                tuning.safeRiseRate, 0.25f, 4.0f);
            apply(m_safety.safeFallRate,
                tuning.safeFallRate, 0.25f, 4.0f);
            apply(m_safety.minimumProtectionTime,
                tuning.minimumProtectionTime, 0.05f, 0.80f);
            apply(m_safety.releaseTime,
                tuning.releaseTime, 0.10f, 1.50f);
            apply(m_safety.cameraMotionSuppression,
                tuning.cameraMotionSuppression, 0.05f, 0.90f);
            if (tuning.architectureMode >= 0)
                m_benchmarkArchitectureMode =
                    std::clamp(tuning.architectureMode, 0, 24);
            apply(m_benchmarkRiskOnlyNeutralLuma,
                tuning.riskOnlyNeutralLuma, 0.03f, 0.50f);
            apply(m_benchmarkRiskOnlyGain,
                tuning.riskOnlyGain, 0.20f, 2.50f);

            // Shader candidates are generated by FlashBench with ordered pairs.
            // Clamp every scalar defensively, then reject malformed pairs back to
            // the exact production defaults instead of letting one case poison the
            // persistent batch.
            ShaderTuningSettings shader = tuning.shader;
            const auto clamp = [](float value, float low, float high) {
                return std::isfinite(value) ? std::clamp(value, low, high) : low;
            };
            shader.eventDeltaLow = clamp(shader.eventDeltaLow, 0.0f, 0.20f);
            shader.eventDeltaHigh = clamp(shader.eventDeltaHigh, 0.001f, 0.30f);
            shader.holdDeltaLow = clamp(shader.holdDeltaLow, 0.0f, 0.25f);
            shader.holdDeltaHigh = clamp(shader.holdDeltaHigh, 0.001f, 0.35f);
            shader.stableSourceLow = clamp(shader.stableSourceLow, 0.0f, 0.20f);
            shader.stableSourceHigh = clamp(shader.stableSourceHigh, 0.001f, 0.30f);
            shader.intrinsicResidualLow = clamp(shader.intrinsicResidualLow, 0.0f, 0.25f);
            shader.intrinsicResidualHigh = clamp(shader.intrinsicResidualHigh, 0.001f, 0.35f);
            shader.repeatedMemoryLow = clamp(shader.repeatedMemoryLow, 0.0f, 0.95f);
            shader.repeatedMemoryHigh = clamp(shader.repeatedMemoryHigh, 0.01f, 1.0f);
            shader.holdGateLow = clamp(shader.holdGateLow, 0.0f, 0.95f);
            shader.holdGateHigh = clamp(shader.holdGateHigh, 0.01f, 1.0f);
            shader.transportConfidenceLow = clamp(shader.transportConfidenceLow, 0.0f, 0.95f);
            shader.transportConfidenceHigh = clamp(shader.transportConfidenceHigh, 0.01f, 1.0f);
            shader.disocclusionResetGate = clamp(shader.disocclusionResetGate, 0.05f, 0.95f);
            shader.surfaceRiskTau = clamp(shader.surfaceRiskTau, 0.005f, 2.0f);
            shader.eventStateTauScale = clamp(shader.eventStateTauScale, 0.20f, 3.0f);
            shader.releaseStateTauScale = clamp(shader.releaseStateTauScale, 0.20f, 3.0f);
            shader.exactHoldThreshold = clamp(shader.exactHoldThreshold, 0.10f, 0.99f);
            shader.movingHoldFloorMax = clamp(shader.movingHoldFloorMax, 0.0f, 0.20f);
            shader.directIntrinsicDisplayLow = clamp(shader.directIntrinsicDisplayLow, 0.0f, 0.20f);
            shader.directIntrinsicDisplayHigh = clamp(shader.directIntrinsicDisplayHigh, 0.001f, 0.30f);
            shader.eventSeedLow = clamp(shader.eventSeedLow, 0.0f, 0.50f);
            shader.eventSeedHigh = clamp(shader.eventSeedHigh, 0.001f, 0.80f);
            const ShaderTuningSettings defaults{};
            const auto ordered = [](float low, float high) { return low + 0.0005f < high; };
            if (!ordered(shader.eventDeltaLow, shader.eventDeltaHigh)) { shader.eventDeltaLow = defaults.eventDeltaLow; shader.eventDeltaHigh = defaults.eventDeltaHigh; }
            if (!ordered(shader.holdDeltaLow, shader.holdDeltaHigh)) { shader.holdDeltaLow = defaults.holdDeltaLow; shader.holdDeltaHigh = defaults.holdDeltaHigh; }
            if (!ordered(shader.stableSourceLow, shader.stableSourceHigh)) { shader.stableSourceLow = defaults.stableSourceLow; shader.stableSourceHigh = defaults.stableSourceHigh; }
            if (!ordered(shader.intrinsicResidualLow, shader.intrinsicResidualHigh)) { shader.intrinsicResidualLow = defaults.intrinsicResidualLow; shader.intrinsicResidualHigh = defaults.intrinsicResidualHigh; }
            if (!ordered(shader.repeatedMemoryLow, shader.repeatedMemoryHigh)) { shader.repeatedMemoryLow = defaults.repeatedMemoryLow; shader.repeatedMemoryHigh = defaults.repeatedMemoryHigh; }
            if (!ordered(shader.holdGateLow, shader.holdGateHigh)) { shader.holdGateLow = defaults.holdGateLow; shader.holdGateHigh = defaults.holdGateHigh; }
            if (!ordered(shader.transportConfidenceLow, shader.transportConfidenceHigh)) { shader.transportConfidenceLow = defaults.transportConfidenceLow; shader.transportConfidenceHigh = defaults.transportConfidenceHigh; }
            if (!ordered(shader.directIntrinsicDisplayLow, shader.directIntrinsicDisplayHigh)) { shader.directIntrinsicDisplayLow = defaults.directIntrinsicDisplayLow; shader.directIntrinsicDisplayHigh = defaults.directIntrinsicDisplayHigh; }
            if (!ordered(shader.eventSeedLow, shader.eventSeedHigh)) { shader.eventSeedLow = defaults.eventSeedLow; shader.eventSeedHigh = defaults.eventSeedHigh; }
            m_shaderTuning = shader;
        }

        bool ReadyToShow() const
        {
            return m_validCaptureFrames.load(std::memory_order_acquire) >=
                static_cast<uint32_t>(std::max(3, (m_safety.lookaheadMs + 15) / 16 + 2));
        }

        void Watchdog()
        {
            if (m_stopped.load()) return;
            const int64_t now = NowMs();
            const auto age = now - m_lastFrameMs.load(std::memory_order_acquire);
            const int64_t faultSince = m_captureFaultSinceMs.load(std::memory_order_acquire);
            const bool sustainedFault = m_captureFault.load(std::memory_order_acquire) &&
                faultSince > 0 && now - faultSince >= kAutomaticShieldFaultDelayMs;
            if ((sustainedFault || age > kDefaultStaleFrameMs) &&
                !m_automaticShieldActive.exchange(true, std::memory_order_acq_rel))
            {
                DiscardPendingLiveFrames();
                m_captureRecoveryFrames.store(8, std::memory_order_release);
            }
            if (m_manualShield.load(std::memory_order_acquire) ||
                m_automaticShieldActive.load(std::memory_order_acquire))
            {
                RenderShieldStep();
            }
        }

        void ResizeOutput()
        {
            DiscardPendingLiveFrames();
            std::scoped_lock lock(m_mutex);
            if (!m_swapChain) return;
            m_backBuffer = nullptr;
            m_backBufferRTV = nullptr;
            UINT resizeFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
            if (m_allowTearing)
                resizeFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
            HRESULT hr = m_swapChain->ResizeBuffers(
                0, 0, 0, DXGI_FORMAT_UNKNOWN, resizeFlags);
            if (FAILED(hr)) return;
            RecreateOutputResources();
            ClearAllToBlack();
        }

    private:
        struct RawFrameSlot
        {
            winrt::com_ptr<ID3D11Texture2D> texture;
            winrt::com_ptr<ID3D11ShaderResourceView> srv;
            uint64_t sequence = 0;
            AnalysisStats stats{};
            std::vector<float> cellLuma;
            std::vector<float> redChangeMask;
            float captureDt = 1.0f / 60.0f;
            bool statsReady = false;
        };

        struct AnalysisReadbackSlot
        {
            winrt::com_ptr<ID3D11Texture2D> staging;
            winrt::com_ptr<ID3D11Query> completion;
            uint64_t sequence = 0;
            float dt = 1.0f / 60.0f;
            bool pending = false;
        };

        struct LiveCaptureSlot
        {
            winrt::com_ptr<ID3D11Texture2D> texture;
            uint64_t sequence = 0;
            uint64_t generation = 0;
            std::chrono::steady_clock::time_point capturedAt{};
            bool ready = false;
        };

        struct FlashEvent
        {
            float time = 0.0f;
            int direction = 0;
        };

        struct HazardPrediction
        {
            bool hazard = false;
            bool global = false;
            bool local = false;
            bool red = false;
            bool pattern = false;
            bool futureReversal = false;
            int futureFrames = 0;
        };

        static int64_t NowMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        static void ThrowIfFailed(HRESULT hr)
        {
            if (FAILED(hr)) throw hr;
        }

        void FindOutputAndCreateDevice()
        {
            winrt::com_ptr<IDXGIFactory1> factory;
            ThrowIfFailed(CreateDXGIFactory1(__uuidof(IDXGIFactory1), factory.put_void()));

            winrt::com_ptr<IDXGIAdapter1> selectedAdapter;
            winrt::com_ptr<IDXGIOutput> selectedOutput;

            for (UINT ai = 0; ; ++ai)
            {
                winrt::com_ptr<IDXGIAdapter1> adapter;
                if (factory->EnumAdapters1(ai, adapter.put()) == DXGI_ERROR_NOT_FOUND) break;

                for (UINT oi = 0; ; ++oi)
                {
                    winrt::com_ptr<IDXGIOutput> output;
                    if (adapter->EnumOutputs(oi, output.put()) == DXGI_ERROR_NOT_FOUND) break;
                    DXGI_OUTPUT_DESC desc{};
                    if (SUCCEEDED(output->GetDesc(&desc)) && desc.Monitor == m_monitor)
                    {
                        selectedAdapter = adapter;
                        selectedOutput = output;
                        break;
                    }
                }
                if (selectedOutput) break;
            }

            if (!selectedAdapter || !selectedOutput)
                throw HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

            DXGI_OUTPUT_DESC outputDesc{};
            ThrowIfFailed(selectedOutput->GetDesc(&outputDesc));
            if (outputDesc.Rotation != DXGI_MODE_ROTATION_IDENTITY &&
                outputDesc.Rotation != DXGI_MODE_ROTATION_UNSPECIFIED)
            {
                MessageBoxW(nullptr,
                    L"This FlashGuard build does not yet support a rotated monitor.",
                    L"FlashGuard", MB_ICONERROR | MB_OK);
                throw E_NOTIMPL;
            }

            UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
            flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
            D3D_FEATURE_LEVEL levels[] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1,
                D3D_FEATURE_LEVEL_10_0
            };
            D3D_FEATURE_LEVEL created{};
            ThrowIfFailed(D3D11CreateDevice(
                selectedAdapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
                levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
                m_device.put(), &created, m_context.put()));

            m_dxgiOutput = selectedOutput.as<IDXGIOutput1>();
        }

        void winrtlessEnableMultithreadProtection()
        {
            winrt::com_ptr<ID3D11Multithread> mt;
            if (SUCCEEDED(m_context->QueryInterface(__uuidof(ID3D11Multithread), mt.put_void())))
                mt->SetMultithreadProtected(TRUE);
        }

        void CreateDuplication()
        {
            m_duplication = nullptr;
            HRESULT hr = m_dxgiOutput->DuplicateOutput(m_device.get(), m_duplication.put());
            if (FAILED(hr)) ThrowIfFailed(hr);
        }

        void SignalCaptureFault()
        {
            DiscardPendingLiveFrames();
            m_captureFault.store(true, std::memory_order_release);
            int64_t expected = 0;
            m_captureFaultSinceMs.compare_exchange_strong(expected, NowMs(),
                std::memory_order_acq_rel);
        }


        void EnsureLiveCaptureSlotsLocked(ID3D11Texture2D* source)
        {
            if (!source) return;

            D3D11_TEXTURE2D_DESC sourceDesc{};
            source->GetDesc(&sourceDesc);
            bool matches = true;
            for (const auto& slot : m_liveCaptureSlots)
            {
                if (!slot.texture)
                {
                    matches = false;
                    break;
                }
                D3D11_TEXTURE2D_DESC slotDesc{};
                slot.texture->GetDesc(&slotDesc);
                if (slotDesc.Width != sourceDesc.Width ||
                    slotDesc.Height != sourceDesc.Height ||
                    slotDesc.Format != sourceDesc.Format ||
                    slotDesc.SampleDesc.Count != sourceDesc.SampleDesc.Count ||
                    slotDesc.SampleDesc.Quality != sourceDesc.SampleDesc.Quality)
                {
                    matches = false;
                    break;
                }
            }
            if (matches) return;

            m_liveFrameGeneration.fetch_add(1, std::memory_order_acq_rel);
            D3D11_TEXTURE2D_DESC copyDesc = sourceDesc;
            copyDesc.MipLevels = 1;
            copyDesc.ArraySize = 1;
            copyDesc.SampleDesc.Count = 1;
            copyDesc.SampleDesc.Quality = 0;
            copyDesc.Usage = D3D11_USAGE_DEFAULT;
            copyDesc.BindFlags = 0;
            copyDesc.CPUAccessFlags = 0;
            copyDesc.MiscFlags = 0;
            for (auto& slot : m_liveCaptureSlots)
            {
                slot.texture = nullptr;
                slot.sequence = 0;
                slot.generation = 0;
                slot.ready = false;
                ThrowIfFailed(m_device->CreateTexture2D(
                    &copyDesc, nullptr, slot.texture.put()));
            }
            m_liveWriteCursor = 0;
        }

        void DiscardPendingLiveFrames()
        {
            std::scoped_lock lock(m_liveFrameMutex);
            m_liveFrameGeneration.fetch_add(1, std::memory_order_acq_rel);
            for (auto& slot : m_liveCaptureSlots)
                slot.ready = false;
        }

        void QueueLatestLiveFrame(ID3D11Texture2D* source)
        {
            std::unique_lock lock(m_liveFrameMutex);
            EnsureLiveCaptureSlotsLocked(source);
            const uint64_t generation =
                m_liveFrameGeneration.load(std::memory_order_acquire);

            // Keep at most one not-yet-processed frame. If processing falls
            // behind, newer desktop content replaces stale intermediate frames
            // instead of turning GPU cost into visible input latency.
            for (auto& slot : m_liveCaptureSlots)
            {
                if (slot.ready)
                {
                    slot.ready = false;
                    m_droppedPresents.fetch_add(1, std::memory_order_relaxed);
                }
            }

            size_t writeIndex = m_liveCaptureSlots.size();
            for (size_t attempt = 0; attempt < m_liveCaptureSlots.size(); ++attempt)
            {
                const size_t candidate =
                    (m_liveWriteCursor + attempt) % m_liveCaptureSlots.size();
                if (candidate != m_liveProcessingSlot)
                {
                    writeIndex = candidate;
                    break;
                }
            }
            if (writeIndex >= m_liveCaptureSlots.size()) return;

            LiveCaptureSlot& slot = m_liveCaptureSlots[writeIndex];
            m_context->CopyResource(slot.texture.get(), source);
            slot.sequence = ++m_liveCaptureSequence;
            slot.generation = generation;
            slot.capturedAt = std::chrono::steady_clock::now();
            slot.ready = true;
            m_liveWriteCursor = (writeIndex + 1) % m_liveCaptureSlots.size();

            lock.unlock();
            m_liveFrameCv.notify_one();
        }

        void ProcessingLoop()
        {
            uint64_t lastRenderedGeneration = 0;
            while (!m_stopped.load(std::memory_order_acquire))
            {
                winrt::com_ptr<ID3D11Texture2D> texture;
                std::chrono::steady_clock::time_point capturedAt{};
                uint64_t frameGeneration = 0;
                size_t processingSlot = m_liveCaptureSlots.size();
                {
                    std::unique_lock lock(m_liveFrameMutex);
                    m_liveFrameCv.wait_for(lock, std::chrono::milliseconds(16), [&] {
                        if (m_stopped.load(std::memory_order_acquire)) return true;
                        for (const auto& slot : m_liveCaptureSlots)
                            if (slot.ready) return true;
                        return false;
                    });
                    if (m_stopped.load(std::memory_order_acquire)) break;

                    uint64_t newestSequence = 0;
                    for (size_t i = 0; i < m_liveCaptureSlots.size(); ++i)
                    {
                        if (m_liveCaptureSlots[i].ready &&
                            m_liveCaptureSlots[i].sequence >= newestSequence)
                        {
                            newestSequence = m_liveCaptureSlots[i].sequence;
                            processingSlot = i;
                        }
                    }
                    if (processingSlot < m_liveCaptureSlots.size())
                    {
                        LiveCaptureSlot& slot = m_liveCaptureSlots[processingSlot];
                        texture = slot.texture;
                        capturedAt = slot.capturedAt;
                        frameGeneration = slot.generation;
                        slot.ready = false;
                        m_liveProcessingSlot = processingSlot;
                    }
                }

                if (texture)
                {
                    float dt = 1.0f / 60.0f;
                    if (frameGeneration == lastRenderedGeneration)
                    {
                        dt = std::chrono::duration<float>(
                            capturedAt - m_lastFrameTime).count();
                        if (!std::isfinite(dt) || dt <= 0.0f || dt > 0.5f)
                            dt = 1.0f / 60.0f;
                    }

                    const auto processingStart = std::chrono::steady_clock::now();
                    bool rendered = false;
                    try
                    {
                        std::scoped_lock lock(m_mutex);
                        if (!m_stopped.load() &&
                            frameGeneration == m_liveFrameGeneration.load(
                                std::memory_order_acquire) &&
                            !m_manualShield.load(std::memory_order_acquire) &&
                            !m_automaticShieldActive.load(std::memory_order_acquire))
                        {
                            if (g_liveRawPassthroughForLatencyTest)
                                PresentRawCapturedFrame(texture.get());
                            else
                                QueueCapturedFrame(texture.get(), dt);
                            rendered = true;
                        }
                    }
                    catch (...)
                    {
                        SignalCaptureFault();
                    }

                    if (rendered)
                    {
                        lastRenderedGeneration = frameGeneration;
                        m_lastFrameTime = capturedAt;
                        m_captureProcessMs.store(
                            std::chrono::duration<float, std::milli>(
                                std::chrono::steady_clock::now() -
                                processingStart).count(),
                            std::memory_order_release);
                        m_validCaptureFrames.fetch_add(
                            1, std::memory_order_release);
                    }

                    {
                        std::scoped_lock lock(m_liveFrameMutex);
                        if (m_liveProcessingSlot == processingSlot)
                            m_liveProcessingSlot = m_liveCaptureSlots.size();
                    }
                    continue;
                }

                const int64_t nowMs = NowMs();
                const int64_t lastRealCaptureMs =
                    m_lastRealCaptureMs.load(std::memory_order_acquire);
                const bool trulyIdle = lastRealCaptureMs > 0 &&
                    nowMs - lastRealCaptureMs >= 40;
                if (!g_liveRawPassthroughForLatencyTest && trulyIdle &&
                    nowMs < m_idleReleaseUntilMs.load(std::memory_order_acquire) &&
                    !m_manualShield.load(std::memory_order_acquire) &&
                    !m_automaticShieldActive.load(std::memory_order_acquire))
                {
                    const auto now = std::chrono::steady_clock::now();
                    float dt = std::chrono::duration<float>(
                        now - m_lastFrameTime).count();
                    if (!std::isfinite(dt) || dt <= 0.0f || dt > 0.10f)
                        dt = 1.0f / 60.0f;
                    m_lastFrameTime = now;
                    try { RenderIdleReleaseStep(dt); }
                    catch (...) { SignalCaptureFault(); }
                }
            }
        }

        void CaptureLoop()
        {
            while (!m_stopped.load(std::memory_order_acquire))
            {
                if (m_captureRestartRequested.exchange(false, std::memory_order_acq_rel))
                {
                    DiscardPendingLiveFrames();
                    m_duplication = nullptr;
                    SignalCaptureFault();
                }
                if (!m_duplication)
                {
                    try
                    {
                        CreateDuplication();
                        {
                            std::scoped_lock lock(m_mutex);
                            ResetDelayedPipeline();
                        }
                    }
                    catch (...)
                    {
                        SignalCaptureFault();
                        Sleep(250);
                        continue;
                    }
                }

                // Latency experiment 1: Desktop Duplication already blocks for the
                // next desktop update. Do not additionally pace acquisition behind
                // FlashGuard's own swap-chain readiness; use the existing F9 timing
                // diagnostics to determine whether this reduces displayed frame age.
                m_presentReadyWaitMs.store(0.0f, std::memory_order_release);

                DXGI_OUTDUPL_FRAME_INFO info{};
                winrt::com_ptr<IDXGIResource> resource;
                HRESULT hr = m_duplication->AcquireNextFrame(25, &info, resource.put());

                if (hr == DXGI_ERROR_WAIT_TIMEOUT)
                {
                    // No desktop change is a healthy capture heartbeat. However,
                    // temporal protection is feedback-based: if we stop rendering
                    // here, a partially filtered image remains frozen until some
                    // unrelated desktop event (often cursor motion) arrives.
                    m_lastFrameMs.store(NowMs(), std::memory_order_release);
                    m_captureFault.store(false, std::memory_order_release);
                    m_captureFaultSinceMs.store(0, std::memory_order_release);

                    const int64_t nowMs = NowMs();
                    const int64_t lastRealCaptureMs =
                        m_lastRealCaptureMs.load(std::memory_order_acquire);
                    const bool trulyIdle = lastRealCaptureMs > 0 &&
                        nowMs - lastRealCaptureMs >= 40;
                    if (!g_liveRawPassthroughForLatencyTest && trulyIdle &&
                        nowMs < m_idleReleaseUntilMs.load(std::memory_order_acquire) &&
                        !m_manualShield.load(std::memory_order_acquire) &&
                        !m_automaticShieldActive.load(std::memory_order_acquire))
                    {
                        auto now = std::chrono::steady_clock::now();
                        float dt = std::chrono::duration<float>(now - m_lastFrameTime).count();
                        if (!std::isfinite(dt) || dt <= 0.0f || dt > 0.10f)
                            dt = 1.0f / 60.0f;
                        m_lastFrameTime = now;
                        RenderIdleReleaseStep(dt);
                    }
                    continue;
                }

                if (hr == DXGI_ERROR_ACCESS_LOST)
                {
                    m_duplication = nullptr;
                    SignalCaptureFault();
                    continue;
                }

                if (FAILED(hr))
                {
                    // Do not keep retrying a poisoned duplication interface.
                    // Recreate it on the next loop, just like ACCESS_LOST.
                    m_duplication = nullptr;
                    SignalCaptureFault();
                    Sleep(50);
                    continue;
                }

                bool mustRelease = true;
                try
                {
                    m_captureAccumulatedFrames.store(
                        info.AccumulatedFrames, std::memory_order_release);
                    if (info.LastPresentTime.QuadPart > 0)
                    {
                        static const double qpcToMs = [] {
                            LARGE_INTEGER frequency{};
                            return QueryPerformanceFrequency(&frequency) &&
                                frequency.QuadPart > 0 ?
                                1000.0 / static_cast<double>(frequency.QuadPart) : 0.0;
                        }();
                        LARGE_INTEGER qpcNow{};
                        QueryPerformanceCounter(&qpcNow);
                        const double ageMs = std::max(0.0,
                            static_cast<double>(qpcNow.QuadPart -
                                info.LastPresentTime.QuadPart) * qpcToMs);
                        m_captureImageAgeMs.store(static_cast<float>(ageMs),
                            std::memory_order_release);
                    }
                    const int64_t realCaptureNowMs = NowMs();
                    m_lastFrameMs.store(realCaptureNowMs, std::memory_order_release);
                    m_lastRealCaptureMs.store(realCaptureNowMs, std::memory_order_release);
                    m_captureFault.store(false, std::memory_order_release);
                    m_captureFaultSinceMs.store(0, std::memory_order_release);

                    const bool captureRecovering =
                        m_automaticShieldActive.load(std::memory_order_acquire);
                    if (captureRecovering)
                    {
                        const int previous = m_captureRecoveryFrames.fetch_sub(
                            1, std::memory_order_acq_rel);
                        if (previous <= 1)
                        {
                            m_captureRecoveryFrames.store(0, std::memory_order_release);
                            m_automaticShieldActive.store(false, std::memory_order_release);
                        }
                    }

                    if (!m_manualShield.load(std::memory_order_acquire) && !captureRecovering)
                    {
                        auto texture = resource.as<ID3D11Texture2D>();
                        auto now = std::chrono::steady_clock::now();
                        float dt = std::chrono::duration<float>(now - m_lastFrameTime).count();
                        if (!std::isfinite(dt) || dt <= 0.0f || dt > 0.5f)
                            dt = 1.0f / 60.0f;
                        m_lastFrameTime = now;

                        const auto processingStart = std::chrono::steady_clock::now();
                        {
                            std::scoped_lock lock(m_mutex);
                            if (!m_stopped.load())
                            {
                                if (g_liveRawPassthroughForLatencyTest)
                                    PresentRawCapturedFrame(texture.get());
                                else
                                    QueueCapturedFrame(texture.get(), dt);
                            }
                        }
                        m_captureProcessMs.store(std::chrono::duration<float, std::milli>(
                            std::chrono::steady_clock::now() - processingStart).count(),
                            std::memory_order_release);
                        m_validCaptureFrames.fetch_add(1, std::memory_order_release);
                    }
                }
                catch (...)
                {
                    SignalCaptureFault();
                }

                if (mustRelease && m_duplication)
                {
                    HRESULT releaseHr = m_duplication->ReleaseFrame();
                    if (FAILED(releaseHr))
                    {
                        m_duplication = nullptr;
                        SignalCaptureFault();
                    }
                }
            }
        }

        void PresentRawCapturedFrame(ID3D11Texture2D* source)
        {
            if (!source || !m_backBuffer || !m_swapChain) return;

            D3D11_TEXTURE2D_DESC sourceDesc{};
            D3D11_TEXTURE2D_DESC backBufferDesc{};
            source->GetDesc(&sourceDesc);
            m_backBuffer->GetDesc(&backBufferDesc);
            if (sourceDesc.Width != backBufferDesc.Width ||
                sourceDesc.Height != backBufferDesc.Height ||
                sourceDesc.Format != backBufferDesc.Format ||
                sourceDesc.SampleDesc.Count != backBufferDesc.SampleDesc.Count ||
                sourceDesc.SampleDesc.Quality != backBufferDesc.SampleDesc.Quality)
                throw E_INVALIDARG;

            // Latency experiment: no analysis ring, instant-safety pass, NVOFA,
            // temporal/history processing, protection shader, or diagnostic overlay.
            m_context->OMSetRenderTargets(0, nullptr, nullptr);
            m_context->CopyResource(m_backBuffer.get(), source);

            const auto presentStart = std::chrono::steady_clock::now();
            const UINT presentFlags =
                m_allowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
            const HRESULT presentHr = m_swapChain->Present(0, presentFlags);
            m_presentCallMs.store(std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - presentStart).count(),
                std::memory_order_release);
            ThrowIfFailed(presentHr);
        }

        void CreateSwapChain()
        {
            winrt::com_ptr<IDXGIDevice> dxgiDevice;
            ThrowIfFailed(m_device->QueryInterface(__uuidof(IDXGIDevice), dxgiDevice.put_void()));
            winrt::com_ptr<IDXGIAdapter> adapter;
            ThrowIfFailed(dxgiDevice->GetAdapter(adapter.put()));
            winrt::com_ptr<IDXGIFactory2> factory;
            ThrowIfFailed(adapter->GetParent(__uuidof(IDXGIFactory2), factory.put_void()));

            // Query tearing support once for the FlashGuard presentation swapchain.
            // When available, sync-interval-0 presents do not have to wait for the
            // next compositor vblank, reducing the capture-to-display leg.
            m_allowTearing = false;
            winrt::com_ptr<IDXGIFactory5> factory5;
            if (SUCCEEDED(factory->QueryInterface(
                    __uuidof(IDXGIFactory5), factory5.put_void())))
            {
                BOOL allowTearing = FALSE;
                if (SUCCEEDED(factory5->CheckFeatureSupport(
                        DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                        &allowTearing, sizeof(allowTearing))))
                    m_allowTearing = allowTearing == TRUE;
            }

            RECT rc{};
            GetClientRect(m_output, &rc);
            DXGI_SWAP_CHAIN_DESC1 desc{};
            desc.Width = static_cast<UINT>(rc.right - rc.left);
            desc.Height = static_cast<UINT>(rc.bottom - rc.top);
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.BufferCount = 2;
            desc.Scaling = DXGI_SCALING_STRETCH;
            desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
            if (m_allowTearing)
                desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

            ThrowIfFailed(factory->CreateSwapChainForHwnd(
                m_device.get(), m_output, &desc, nullptr, nullptr, m_swapChain.put()));
            factory->MakeWindowAssociation(m_output, DXGI_MWA_NO_ALT_ENTER);

            auto swapChain2 = m_swapChain.as<IDXGISwapChain2>();
            ThrowIfFailed(swapChain2->SetMaximumFrameLatency(1));
            m_frameLatencyWaitableObject = swapChain2->GetFrameLatencyWaitableObject();
            if (!m_frameLatencyWaitableObject)
                throw E_FAIL;
        }

        void CreatePipeline()
        {
            winrt::com_ptr<ID3DBlob> vsBlob, psBlob, errors;
            HRESULT hr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1, "FlashGuard", nullptr, nullptr,
                                    "VSMain", "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                                    vsBlob.put(), errors.put());
            if (FAILED(hr))
            {
                if (errors) OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
                ThrowIfFailed(hr);
            }
            errors = nullptr;
            hr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1, "FlashGuard", nullptr, nullptr,
                            "PSMain", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                            psBlob.put(), errors.put());
            if (FAILED(hr))
            {
                if (errors) OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
                ThrowIfFailed(hr);
            }

            ThrowIfFailed(m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_vs.put()));
            ThrowIfFailed(m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_ps.put()));

            errors = nullptr;
            winrt::com_ptr<ID3DBlob> analyzeBlob;
            hr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1, "FlashGuard", nullptr, nullptr,
                            "PSAnalyze", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                            analyzeBlob.put(), errors.put());
            if (FAILED(hr))
            {
                if (errors) OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
                ThrowIfFailed(hr);
            }
            ThrowIfFailed(m_device->CreatePixelShader(analyzeBlob->GetBufferPointer(), analyzeBlob->GetBufferSize(), nullptr, m_psAnalyze.put()));

            errors = nullptr;
            winrt::com_ptr<ID3DBlob> instantBlob;
            hr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1, "FlashGuard", nullptr, nullptr,
                            "PSInstantSafety", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                            instantBlob.put(), errors.put());
            if (FAILED(hr))
            {
                if (errors) OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
                ThrowIfFailed(hr);
            }
            ThrowIfFailed(m_device->CreatePixelShader(instantBlob->GetBufferPointer(),
                instantBlob->GetBufferSize(), nullptr, m_psInstantSafety.put()));

            errors = nullptr;
            winrt::com_ptr<ID3DBlob> opticalFlowCopyBlob;
            hr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1, "FlashGuard", nullptr, nullptr,
                            "PSOpticalFlowCopy", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                            opticalFlowCopyBlob.put(), errors.put());
            if (FAILED(hr))
            {
                if (errors) OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
                ThrowIfFailed(hr);
            }
            ThrowIfFailed(m_device->CreatePixelShader(opticalFlowCopyBlob->GetBufferPointer(),
                opticalFlowCopyBlob->GetBufferSize(), nullptr, m_psOpticalFlowCopy.put()));

            D3D11_SAMPLER_DESC sd{};
            sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            sd.MinLOD = 0;
            sd.MaxLOD = D3D11_FLOAT32_MAX;
            ThrowIfFailed(m_device->CreateSamplerState(&sd, m_sampler.put()));

            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth = sizeof(ShaderConstants);
            bd.Usage = D3D11_USAGE_DYNAMIC;
            bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            ThrowIfFailed(m_device->CreateBuffer(&bd, nullptr, m_constants.put()));
        }

        void CreateAnalysisResources()
        {
            D3D11_TEXTURE2D_DESC analysisTd{};
            analysisTd.Width = kAnalysisWidth;
            analysisTd.Height = kAnalysisHeight;
            analysisTd.MipLevels = 1;
            analysisTd.ArraySize = 1;
            analysisTd.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            analysisTd.SampleDesc.Count = 1;
            analysisTd.Usage = D3D11_USAGE_DEFAULT;
            analysisTd.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            ThrowIfFailed(m_device->CreateTexture2D(&analysisTd, nullptr, m_analysisTex.put()));
            ThrowIfFailed(m_device->CreateRenderTargetView(m_analysisTex.get(), nullptr, m_analysisRTV.put()));
            ThrowIfFailed(m_device->CreateShaderResourceView(m_analysisTex.get(), nullptr,
                m_analysisSRV.put()));

            ThrowIfFailed(m_device->CreateTexture2D(&analysisTd, nullptr,
                m_instantPreviousAnalysis.put()));
            ThrowIfFailed(m_device->CreateShaderResourceView(m_instantPreviousAnalysis.get(),
                nullptr, m_instantPreviousAnalysisSRV.put()));

            D3D11_TEXTURE2D_DESC stateTd{};
            stateTd.Width = kAnalysisWidth;
            stateTd.Height = kAnalysisHeight;
            stateTd.MipLevels = 1;
            stateTd.ArraySize = 1;
            stateTd.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            stateTd.SampleDesc.Count = 1;
            stateTd.Usage = D3D11_USAGE_DEFAULT;
            stateTd.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            for (size_t i = 0; i < m_instantSafetyTextures.size(); ++i)
            {
                ThrowIfFailed(m_device->CreateTexture2D(&stateTd, nullptr,
                    m_instantSafetyTextures[i].put()));
                ThrowIfFailed(m_device->CreateRenderTargetView(m_instantSafetyTextures[i].get(),
                    nullptr, m_instantSafetyRTVs[i].put()));
                ThrowIfFailed(m_device->CreateShaderResourceView(m_instantSafetyTextures[i].get(),
                    nullptr, m_instantSafetySRVs[i].put()));

                ThrowIfFailed(m_device->CreateTexture2D(&stateTd, nullptr,
                    m_instantTemporalTextures[i].put()));
                ThrowIfFailed(m_device->CreateRenderTargetView(m_instantTemporalTextures[i].get(),
                    nullptr, m_instantTemporalRTVs[i].put()));
                ThrowIfFailed(m_device->CreateShaderResourceView(m_instantTemporalTextures[i].get(),
                    nullptr, m_instantTemporalSRVs[i].put()));
            }

            D3D11_QUERY_DESC qd{};
            qd.Query = D3D11_QUERY_EVENT;
            for (auto& slot : m_analysisReadbacks)
            {
                D3D11_TEXTURE2D_DESC staging = stateTd;
                staging.Usage = D3D11_USAGE_STAGING;
                staging.BindFlags = 0;
                staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                ThrowIfFailed(m_device->CreateTexture2D(&staging, nullptr, slot.staging.put()));
                ThrowIfFailed(m_device->CreateQuery(&qd, slot.completion.put()));
            }

            D3D11_TEXTURE2D_DESC local{};
            local.Width = kAnalysisWidth;
            local.Height = kAnalysisHeight;
            local.MipLevels = 1;
            local.ArraySize = 1;
            local.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            local.SampleDesc.Count = 1;
            local.Usage = D3D11_USAGE_DEFAULT;
            local.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            ThrowIfFailed(m_device->CreateTexture2D(&local, nullptr, m_localSafetyTexture.put()));
            ThrowIfFailed(m_device->CreateShaderResourceView(
                m_localSafetyTexture.get(), nullptr, m_localSafetySRV.put()));

            const size_t count = static_cast<size_t>(kAnalysisWidth) * kAnalysisHeight;
            m_prevAnalysis.assign(count, 0.0f);
            m_currentAnalysis.assign(count, 0.0f);
            m_prevRed.assign(count, 0.0f);
            m_currentRed.assign(count, 0.0f);
            m_prevU.assign(count, 0.0f);
            m_prevV.assign(count, 0.0f);
            m_currentU.assign(count, 0.0f);
            m_currentV.assign(count, 0.0f);
            m_prevSaturatedRed.assign(count, 0);
            m_currentSaturatedRed.assign(count, 0);
            m_redChangeScratch.assign(count, 0.0f);
            m_activationScratch.assign(count, 0.0f);
            m_changeMaskScratch.assign(count, 0);
            m_componentVisited.assign(count, 0);
            m_componentQueue.reserve(count);
            m_integralScratch.assign((kAnalysisWidth + 1) * (kAnalysisHeight + 1), 0);
            m_localSafetyData.assign(count * 4, 0.0f);
            m_localDisplayedLuma.assign(count, 0.0f);
            m_previousOutputLuma.assign(count, 0.0f);
            m_localLumaGate.assign(count, 0.0f);
            m_localRedGate.assign(count, 0.0f);
        }

        static float SmoothStepCPU(float edge0, float edge1, float x)
        {
            if (edge1 <= edge0) return x >= edge1 ? 1.0f : 0.0f;
            float t = (x - edge0) / (edge1 - edge0);
            t = std::max(0.0f, std::min(1.0f, t));
            return t * t * (3.0f - 2.0f * t);
        }

        void DrawAnalysis(ID3D11ShaderResourceView* source, uint64_t sequence, float dt)
        {
            ResolveAnalysisResults(0, false);

            AnalysisReadbackSlot* readback = nullptr;
            for (auto& slot : m_analysisReadbacks)
            {
                if (!slot.pending)
                {
                    readback = &slot;
                    break;
                }
            }
            if (!readback)
            {
                // Never force the capture/present path to wait for the GPU. Under
                // extreme load this frame will have no analysis metadata and will
                // use the clean raw fallback when it reaches the display point.
                return;
            }

            D3D11_VIEWPORT vp{};
            vp.Width = static_cast<float>(kAnalysisWidth);
            vp.Height = static_cast<float>(kAnalysisHeight);
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            m_context->RSSetViewports(1, &vp);

            ID3D11RenderTargetView* rtv = m_analysisRTV.get();
            m_context->OMSetRenderTargets(1, &rtv, nullptr);
            m_context->IASetInputLayout(nullptr);
            m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_context->VSSetShader(m_vs.get(), nullptr, 0);
            m_context->PSSetShader(m_psAnalyze.get(), nullptr, 0);
            m_context->PSSetShaderResources(0, 1, &source);
            ID3D11SamplerState* sampler = m_sampler.get();
            m_context->PSSetSamplers(0, 1, &sampler);
            m_context->Draw(3, 0);

            ID3D11ShaderResourceView* nullSRV = nullptr;
            m_context->PSSetShaderResources(0, 1, &nullSRV);
            ID3D11RenderTargetView* nullRTV = nullptr;
            m_context->OMSetRenderTargets(1, &nullRTV, nullptr);

            m_context->CopySubresourceRegion(readback->staging.get(), 0, 0, 0, 0,
                m_analysisTex.get(), 0, nullptr);
            m_context->End(readback->completion.get());
            readback->sequence = sequence;
            readback->dt = dt;
            readback->pending = true;
        }

        void DrawInstantSafetyMap(float dt)
        {
            m_instantFrameDt = std::clamp(dt, 1.0f / 240.0f, 0.05f);
            UpdateConstants(0.5f, 0.5f, 0.0f, 0.0f, false, false, false);
            const size_t writeIndex = 1 - m_instantSafetyIndex;

            D3D11_VIEWPORT vp{};
            vp.Width = static_cast<float>(kAnalysisWidth);
            vp.Height = static_cast<float>(kAnalysisHeight);
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            m_context->RSSetViewports(1, &vp);

            ID3D11RenderTargetView* rtvs[] = {
                m_instantSafetyRTVs[writeIndex].get(),
                m_instantTemporalRTVs[writeIndex].get()
            };
            m_context->OMSetRenderTargets(2, rtvs, nullptr);
            m_context->IASetInputLayout(nullptr);
            m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_context->VSSetShader(m_vs.get(), nullptr, 0);
            m_context->PSSetShader(m_psInstantSafety.get(), nullptr, 0);
            ID3D11ShaderResourceView* srvs[] = {
                m_analysisSRV.get(), m_instantPreviousAnalysisSRV.get(),
                m_instantSafetySRVs[m_instantSafetyIndex].get(),
                m_instantTemporalSRVs[m_instantSafetyIndex].get()
            };
            m_context->PSSetShaderResources(4, 4, srvs);
            ID3D11SamplerState* sampler = m_sampler.get();
            m_context->PSSetSamplers(0, 1, &sampler);
            ID3D11Buffer* cb = m_constants.get();
            m_context->PSSetConstantBuffers(0, 1, &cb);
            m_context->Draw(3, 0);

            ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
            m_context->PSSetShaderResources(4, 4, nullSRVs);
            ID3D11RenderTargetView* nullRTVs[2] = { nullptr, nullptr };
            m_context->OMSetRenderTargets(2, nullRTVs, nullptr);
            m_instantSafetyIndex = writeIndex;
        }

        void ResolveAnalysisResults(uint64_t requiredSequence, bool wait)
        {
            for (;;)
            {
                AnalysisReadbackSlot* oldest = nullptr;
                for (auto& slot : m_analysisReadbacks)
                {
                    if (slot.pending && (!oldest || slot.sequence < oldest->sequence))
                        oldest = &slot;
                }
                if (!oldest) return;

                HRESULT hr = m_context->GetData(oldest->completion.get(), nullptr, 0,
                    wait && oldest->sequence <= requiredSequence ? 0 : D3D11_ASYNC_GETDATA_DONOTFLUSH);
                if (hr == S_FALSE)
                {
                    if (!wait || oldest->sequence > requiredSequence) return;
                    Sleep(0);
                    continue;
                }
                ThrowIfFailed(hr);
                ConsumeAnalysis(*oldest);
                oldest->pending = false;
            }
        }

        void ConsumeAnalysis(AnalysisReadbackSlot& readback)
        {
            AnalysisStats stats{};
            stats.dt = readback.dt;

            D3D11_MAPPED_SUBRESOURCE mapped{};
            ThrowIfFailed(m_context->Map(readback.staging.get(), 0, D3D11_MAP_READ, 0, &mapped));

            uint32_t affected = 0;
            uint32_t positive = 0;
            uint32_t negative = 0;
            uint32_t strongAffected = 0;
            uint32_t strongPositive = 0;
            uint32_t strongNegative = 0;
            uint32_t redAffected = 0;
            float sumLuma = 0.0f;
            float sumAbsDelta = 0.0f;
            const uint32_t count = kAnalysisWidth * kAnalysisHeight;
            std::fill(m_redChangeScratch.begin(), m_redChangeScratch.end(), 0.0f);

            for (UINT y = 0; y < kAnalysisHeight; ++y)
            {
                const auto* row = static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch;
                for (UINT x = 0; x < kAnalysisWidth; ++x)
                {
                    const float* px = reinterpret_cast<const float*>(row) + x * 4;
                    const float r = std::max(0.0f, px[0]);
                    const float g = std::max(0.0f, px[1]);
                    const float b = std::max(0.0f, px[2]);
                    const float l = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                    const float rgbSum = r + g + b;
                    const float redRatio = rgbSum > 0.0001f ? r / rgbSum : 0.0f;
                    const float X = 0.4124564f * r + 0.3575761f * g + 0.1804375f * b;
                    const float Y = l;
                    const float Z = 0.0193339f * r + 0.1191920f * g + 0.9503041f * b;
                    const float uvDenom = X + 15.0f * Y + 3.0f * Z;
                    const float u = uvDenom > 0.0001f ? 4.0f * X / uvDenom : 0.0f;
                    const float v = uvDenom > 0.0001f ? 9.0f * Y / uvDenom : 0.0f;
                    const size_t index = static_cast<size_t>(y) * kAnalysisWidth + x;
                    m_currentAnalysis[index] = l;
                    m_currentRed[index] = redRatio;
                    m_currentU[index] = u;
                    m_currentV[index] = v;
                    m_currentSaturatedRed[index] = redRatio >= 0.80f ? 1 : 0;
                    sumLuma += l;

                    if (m_havePrevAnalysis)
                    {
                        const float delta = l - m_prevAnalysis[index];
                        sumAbsDelta += std::fabs(delta);
                        if (std::fabs(delta) >= m_safety.localDeltaThreshold)
                        {
                            ++affected;
                            if (delta > 0.0f) ++positive; else ++negative;
                        }
                        if (std::fabs(delta) >= m_safety.smallFlashDeltaThreshold)
                        {
                            ++strongAffected;
                            if (delta > 0.0f) ++strongPositive; else ++strongNegative;
                        }
                        const float du = u - m_prevU[index];
                        const float dv = v - m_prevV[index];
                        const float chromaDistance = std::sqrt(du * du + dv * dv);
                        if ((m_currentSaturatedRed[index] || m_prevSaturatedRed[index]) &&
                            chromaDistance >= 0.20f)
                        {
                            ++redAffected;
                            m_redChangeScratch[index] = 1.0f;
                        }
                    }
                }
            }
            m_context->Unmap(readback.staging.get(), 0);

            stats.globalLuma = sumLuma / static_cast<float>(count);
            if (m_havePrevAnalysis)
            {
                stats.validDelta = true;
                stats.globalDelta = stats.globalLuma - m_previousGlobalLuma;
                stats.affectedArea = static_cast<float>(affected) / count;
                stats.brighteningArea = static_cast<float>(positive) / count;
                stats.darkeningArea = static_cast<float>(negative) / count;
                stats.directionalCoherence = affected ?
                    static_cast<float>(std::max(positive, negative)) / affected : 0.0f;
                stats.strongAffectedArea = static_cast<float>(strongAffected) / count;
                stats.strongDirectionalCoherence = strongAffected ?
                    static_cast<float>(std::max(strongPositive, strongNegative)) /
                    strongAffected : 0.0f;
                stats.redAffectedArea = static_cast<float>(redAffected) / count;
                stats.flashEnergy = sumAbsDelta / static_cast<float>(count);

                const int dominantDirection = positive >= negative ? 1 : -1;
                std::fill(m_changeMaskScratch.begin(), m_changeMaskScratch.end(), uint8_t{});
                for (size_t i = 0; i < count; ++i)
                {
                    const float delta = m_currentAnalysis[i] - m_prevAnalysis[i];
                    if (std::fabs(delta) >= m_safety.localDeltaThreshold &&
                        (delta >= 0.0f ? 1 : -1) == dominantDirection)
                        m_changeMaskScratch[i] = 1;
                }

                // Largest coherent connected component: moving hands usually
                // fragment into edge bands, while flashes form broad regions.
                std::fill(m_componentVisited.begin(), m_componentVisited.end(), uint8_t{});
                size_t largestComponent = 0;
                for (size_t seed = 0; seed < count; ++seed)
                {
                    if (!m_changeMaskScratch[seed] || m_componentVisited[seed]) continue;
                    m_componentQueue.clear();
                    m_componentQueue.push_back(seed);
                    m_componentVisited[seed] = 1;
                    size_t head = 0;
                    while (head < m_componentQueue.size())
                    {
                        const size_t current = m_componentQueue[head++];
                        const int cx = static_cast<int>(current % kAnalysisWidth);
                        const int cy = static_cast<int>(current / kAnalysisWidth);
                        for (int oy = -1; oy <= 1; ++oy)
                        for (int ox = -1; ox <= 1; ++ox)
                        {
                            if (ox == 0 && oy == 0) continue;
                            const int nx = cx + ox, ny = cy + oy;
                            if (nx < 0 || ny < 0 || nx >= static_cast<int>(kAnalysisWidth) ||
                                ny >= static_cast<int>(kAnalysisHeight)) continue;
                            const size_t ni = static_cast<size_t>(ny) * kAnalysisWidth + nx;
                            if (m_changeMaskScratch[ni] && !m_componentVisited[ni])
                            {
                                m_componentVisited[ni] = 1;
                                m_componentQueue.push_back(ni);
                            }
                        }
                    }
                    largestComponent = std::max(largestComponent, m_componentQueue.size());
                }
                stats.largestRegionArea = static_cast<float>(largestComponent) / count;

                // Approximate a calibrated 10-degree visual-field window and find
                // the densest concurrently changing region within it.
                const float aspect = m_inputHeight ?
                    static_cast<float>(m_inputWidth) / m_inputHeight : 16.0f / 9.0f;
                const float diagonalCm = m_safety.displayDiagonalInches * 2.54f;
                const float screenWidthCm = diagonalCm * aspect / std::sqrt(aspect * aspect + 1.0f);
                const float screenHeightCm = diagonalCm / std::sqrt(aspect * aspect + 1.0f);
                const float fieldCm = 2.0f * m_safety.viewingDistanceCm *
                    std::tan(5.0f * 3.14159265f / 180.0f);
                const int windowW = std::clamp(static_cast<int>(std::lround(
                    kAnalysisWidth * fieldCm / std::max(1.0f, screenWidthCm))), 2,
                    static_cast<int>(kAnalysisWidth));
                const int windowH = std::clamp(static_cast<int>(std::lround(
                    kAnalysisHeight * fieldCm / std::max(1.0f, screenHeightCm))), 2,
                    static_cast<int>(kAnalysisHeight));
                const int stride = static_cast<int>(kAnalysisWidth) + 1;
                std::fill(m_integralScratch.begin(), m_integralScratch.end(), 0);
                for (int y = 0; y < static_cast<int>(kAnalysisHeight); ++y)
                for (int x = 0; x < static_cast<int>(kAnalysisWidth); ++x)
                {
                    const int dst = (y + 1) * stride + (x + 1);
                    m_integralScratch[dst] = m_changeMaskScratch[static_cast<size_t>(y) * kAnalysisWidth + x] +
                        m_integralScratch[dst - 1] + m_integralScratch[dst - stride] -
                        m_integralScratch[dst - stride - 1];
                }
                uint32_t maxWindowAffected = 0;
                for (int y = 0; y + windowH <= static_cast<int>(kAnalysisHeight); ++y)
                for (int x = 0; x + windowW <= static_cast<int>(kAnalysisWidth); ++x)
                {
                    const int x2 = x + windowW, y2 = y + windowH;
                    const uint32_t area = m_integralScratch[y2 * stride + x2] -
                        m_integralScratch[y * stride + x2] -
                        m_integralScratch[y2 * stride + x] + m_integralScratch[y * stride + x];
                    maxWindowAffected = std::max(maxWindowAffected, area);
                }
                stats.visualFieldAffectedArea = static_cast<float>(maxWindowAffected) /
                    static_cast<float>(windowW * windowH);

#include "analysis/CameraMotionAnalysis.inl"

                uint32_t patternCells = 0, patternTests = 0;
                for (int y = 1; y < static_cast<int>(kAnalysisHeight) - 1; ++y)
                for (int x = 1; x < static_cast<int>(kAnalysisWidth) - 1; ++x)
                {
                    const size_t i = static_cast<size_t>(y) * kAnalysisWidth + x;
                    const float c = m_currentAnalysis[i];
                    const float dl = c - m_currentAnalysis[i - 1];
                    const float dr = m_currentAnalysis[i + 1] - c;
                    const float du = c - m_currentAnalysis[i - kAnalysisWidth];
                    const float dd = m_currentAnalysis[i + kAnalysisWidth] - c;
                    if ((std::fabs(dl) > 0.18f && std::fabs(dr) > 0.18f && dl * dr < 0.0f) ||
                        (std::fabs(du) > 0.18f && std::fabs(dd) > 0.18f && du * dd < 0.0f))
                        ++patternCells;
                    ++patternTests;
                }
                stats.patternScore = patternTests ?
                    static_cast<float>(patternCells) / patternTests : 0.0f;
            }

            for (auto& frame : m_rawFrames)
            {
                if (frame.sequence == readback.sequence)
                {
                    frame.stats = stats;
                    frame.cellLuma = m_currentAnalysis;
                    frame.redChangeMask = m_redChangeScratch;
                    frame.statsReady = true;
                    break;
                }
            }

            m_prevAnalysis.swap(m_currentAnalysis);
            m_prevRed.swap(m_currentRed);
            m_prevU.swap(m_currentU);
            m_prevV.swap(m_currentV);
            m_prevSaturatedRed.swap(m_currentSaturatedRed);
            m_previousGlobalLuma = stats.globalLuma;
            m_havePrevAnalysis = true;
            if (m_safety.lookaheadMs == 0)
            {
                m_latestStats = stats;
                m_predictionFrames = 0;
            }
        }

        void EnsureRawRing(ID3D11Texture2D* source)
        {
            D3D11_TEXTURE2D_DESC src{};
            source->GetDesc(&src);
            if (!m_rawFrames.empty() && src.Width == m_inputWidth &&
                src.Height == m_inputHeight && src.Format == m_inputFormat)
                return;

            m_rawFrames.clear();
            const size_t capacity = 24;
            m_rawFrames.resize(capacity);

            D3D11_TEXTURE2D_DESC td = src;
            td.MipLevels = 1;
            td.ArraySize = 1;
            td.SampleDesc.Count = 1;
            td.SampleDesc.Quality = 0;
            td.Usage = D3D11_USAGE_DEFAULT;
            td.CPUAccessFlags = 0;
            td.MiscFlags = 0;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            for (auto& frame : m_rawFrames)
            {
                ThrowIfFailed(m_device->CreateTexture2D(&td, nullptr, frame.texture.put()));
                ThrowIfFailed(m_device->CreateShaderResourceView(frame.texture.get(), nullptr, frame.srv.put()));
                frame.cellLuma.assign(static_cast<size_t>(kAnalysisWidth) * kAnalysisHeight, 0.0f);
                frame.redChangeMask.assign(static_cast<size_t>(kAnalysisWidth) * kAnalysisHeight, 0.0f);
            }
            m_inputWidth = src.Width;
            m_inputHeight = src.Height;
            m_inputFormat = src.Format;
            ResetDelayedPipeline();
        }

        void ResetDelayedPipeline()
        {
            DiscardPendingLiveFrames();
            m_ringRead = 0;
            m_ringWrite = 0;
            m_bufferedFrameCount = 0;
            m_bufferedDuration = 0.0f;
            for (auto& frame : m_rawFrames) frame.statsReady = false;
            for (auto& readback : m_analysisReadbacks) readback.pending = false;
            m_havePrevAnalysis = false;
            m_instantHistoryValid = false;
            m_instantSafetyIndex = 0;
            // Protection state uses the output-history ping-pong index.
            m_outputHistoryIndex = 0;
            m_sourceHistoryIndex = 0;
            m_outputHistoryValid = false;
            m_sourceHistoryValid = false;
            m_idleReleaseUntilMs.store(0, std::memory_order_release);
            m_lastRealCaptureMs.store(0, std::memory_order_release);
            // Keep NVOFA's pair sequence aligned with the display/source histories.
            // After a reset, the next captured frame becomes a fresh optical-flow anchor.
            m_nvofPreviousValid = false;
            m_nvofFlowValid = false;
            m_nvofLastExecuteSuccessful = false;
            m_haveDisplayedLuma = false;
            m_haveHardRiseLuma = false;
            m_hardGlobalActive = false;
            m_hazardState = HazardState::Safe;
            m_protectionStrength = 0.0f;
            m_redProtectionStrength = 0.0f;
            m_globalProtectionActive = false;
            m_lastGlobalHazardTime = -100.0f;
            m_lastEventWasLocal = false;
            m_haveLocalHistory = false;
            std::fill(m_localLumaGate.begin(), m_localLumaGate.end(), 0.0f);
            std::fill(m_localRedGate.begin(), m_localRedGate.end(), 0.0f);
            m_recentFlashes.clear();
            m_flashCountLastSecond = 0;
            m_alternatingFlashCount = 0;
            m_pendingTransitionDirection = 0;
            m_pendingTransitionTime = -100.0f;
            m_predictionFrames = 0;
            m_patternProtectionActive = false;
            m_latestPrediction = {};
            m_latestStats = {};
        }

        static float MoveTowards(float value, float target, float maxDelta)
        {
            if (value < target) return std::min(value + maxDelta, target);
            return std::max(value - maxDelta, target);
        }

        HazardPrediction BuildHazardPrediction(const RawFrameSlot& delayed)
        {
            HazardPrediction prediction{};
            const AnalysisStats& stats = delayed.stats;
            const bool coherent = stats.validDelta &&
                stats.affectedArea >= m_safety.affectedAreaThreshold &&
                stats.directionalCoherence >= m_safety.coherenceThreshold;
            const bool globalExposure = stats.validDelta &&
                std::fabs(stats.globalDelta) >= m_safety.globalDeltaThreshold &&
                stats.affectedArea >= m_safety.globalAreaThreshold;
            const bool smallIntense = stats.validDelta &&
                stats.strongAffectedArea >= m_safety.smallFlashAreaThreshold &&
                stats.strongDirectionalCoherence >= m_safety.smallFlashCoherenceThreshold;
            const bool calibratedRegion = stats.validDelta &&
                stats.visualFieldAffectedArea >= m_safety.visualFieldAreaThreshold &&
                stats.flashEnergy >= m_safety.flashEnergyThreshold;
            const bool firstBrightTransient = stats.validDelta &&
                stats.brighteningArea > stats.darkeningArea &&
                stats.strongAffectedArea >= m_safety.smallFlashAreaThreshold &&
                stats.strongDirectionalCoherence >= 0.60f &&
                (stats.largestRegionArea >= m_safety.smallFlashAreaThreshold * 1.25f ||
                 std::fabs(stats.globalDelta) >= m_safety.localGlobalSupportThreshold);
            prediction.red = stats.validDelta &&
                stats.redAffectedArea >= m_safety.redAffectedAreaThreshold;
            prediction.pattern = stats.patternScore >= m_safety.patternScoreThreshold &&
                stats.affectedArea >= 0.04f &&
                stats.cameraMotionScore < m_safety.cameraMotionSuppression;
            prediction.global = globalExposure ||
                (coherent && stats.affectedArea >= m_safety.globalAreaThreshold);

            const int direction = stats.brighteningArea >= stats.darkeningArea ? 1 : -1;
            for (size_t offset = 1; offset < m_bufferedFrameCount; ++offset)
            {
                const RawFrameSlot& future = m_rawFrames[(m_ringRead + offset) % m_rawFrames.size()];
                if (!future.statsReady) continue;
                ++prediction.futureFrames;
                const AnalysisStats& f = future.stats;
                if (!f.validDelta) continue;
                const int futureDirection = f.brighteningArea >= f.darkeningArea ? 1 : -1;
                const bool meaningfulFuture =
                    f.affectedArea >= m_safety.affectedAreaThreshold * 0.55f ||
                    f.strongAffectedArea >= m_safety.smallFlashAreaThreshold ||
                    std::fabs(f.globalDelta) >= m_safety.globalDeltaThreshold * 0.55f;
                if (meaningfulFuture && futureDirection != direction)
                {
                    prediction.futureReversal = true;
                    break;
                }
            }

            const bool directionMatchesGlobal =
                (stats.globalDelta >= 0.0f ? 1 : -1) == direction;
            bool localCandidate = (coherent || smallIntense || calibratedRegion ||
                firstBrightTransient) &&
                (std::fabs(stats.globalDelta) >= m_safety.localGlobalSupportThreshold ||
                 stats.largestRegionArea >= m_safety.smallFlashAreaThreshold * 1.5f ||
                 firstBrightTransient || prediction.futureReversal) && directionMatchesGlobal;

            // A translation-explained change is normally camera/character motion.
            // A future reversal, large exposure change, red flash, or pattern risk
            // overrides that suppression.
            if (stats.cameraMotionScore >= m_safety.cameraMotionSuppression &&
                !prediction.futureReversal && !prediction.global &&
                !prediction.red && !prediction.pattern)
                localCandidate = false;

            prediction.local = localCandidate;
            prediction.hazard = prediction.global || prediction.local ||
                prediction.red || prediction.pattern;
            return prediction;
        }

        void UpdateHardRiseLimiter(const AnalysisStats& stats, bool trigger)
        {
            const float dt = std::max(1.0f / 240.0f, std::min(stats.dt, 0.1f));
            if (!m_haveHardRiseLuma)
            {
                m_hardRiseLuma = stats.globalLuma;
                m_haveHardRiseLuma = true;
            }
            if (trigger) m_hardGlobalActive = true;
            if (!m_hardGlobalActive)
            {
                m_hardRiseLuma = stats.globalLuma;
                return;
            }
            else
            {
                const float rate = stats.globalLuma > m_hardRiseLuma ?
                    m_safety.safeRiseRate : m_safety.safeFallRate;
                m_hardRiseLuma = MoveTowards(m_hardRiseLuma, stats.globalLuma,
                    rate * dt);
            }
            if (std::fabs(m_hardRiseLuma - stats.globalLuma) <= 0.006f)
            {
                m_hardRiseLuma = stats.globalLuma;
                m_hardGlobalActive = false;
            }
        }

        void UpdateHazardState(const AnalysisStats& stats, const HazardPrediction& prediction)
        {
            const float dt = std::max(1.0f / 240.0f, std::min(stats.dt, 0.1f));
            m_timelineSeconds += dt;
            m_latestStats = stats;

            const bool globalHazard = prediction.global;
            const bool localFlash = prediction.local || prediction.pattern;
            const bool redTransition = prediction.red;
            const bool hazard = prediction.hazard;
            m_lastEventWasLocal = (localFlash || redTransition) && !globalHazard;
            m_patternProtectionActive = prediction.pattern;
            m_predictionFrames = prediction.futureFrames;
            m_latestPrediction = prediction;
            const bool suspect = stats.validDelta && !hazard &&
                ((stats.affectedArea >= m_safety.affectedAreaThreshold * 0.65f &&
                  stats.directionalCoherence >= m_safety.coherenceThreshold * 0.85f) ||
                 std::fabs(stats.globalDelta) >= m_safety.globalDeltaThreshold * 0.65f);

            while (!m_recentFlashes.empty() &&
                   m_timelineSeconds - m_recentFlashes.front().time > 1.0f)
                m_recentFlashes.pop_front();

            if (!m_haveDisplayedLuma)
            {
                m_displayedGlobalLuma = stats.globalLuma;
                m_haveDisplayedLuma = true;
            }
            const bool hardGlobalTrigger = stats.validDelta &&
                stats.affectedArea >= m_safety.globalAreaThreshold &&
                stats.directionalCoherence >= m_safety.coherenceThreshold;
            UpdateHardRiseLimiter(stats, hardGlobalTrigger);

            if (hazard)
            {
                if (globalHazard)
                {
                    m_globalProtectionActive = true;
                    m_lastGlobalHazardTime = m_timelineSeconds;
                }
                int direction = stats.globalDelta >= 0.0f ? 1 : -1;
                if (std::fabs(stats.globalDelta) < 0.01f)
                    direction = stats.brighteningArea >= stats.darkeningArea ? 1 : -1;
                if (m_pendingTransitionDirection == 0 ||
                    m_timelineSeconds - m_pendingTransitionTime > 1.0f)
                {
                    m_pendingTransitionDirection = direction;
                    m_pendingTransitionTime = m_timelineSeconds;
                }
                else if (direction != m_pendingTransitionDirection)
                {
                    // One flash is a completed pair of opposing transitions.
                    m_recentFlashes.push_back({ m_timelineSeconds, direction });
                    m_pendingTransitionDirection = direction;
                    m_pendingTransitionTime = m_timelineSeconds;
                }
                m_lastHazardTime = m_timelineSeconds;

                const float areaStrength = SmoothStepCPU(
                    m_safety.affectedAreaThreshold, m_safety.strongAffectedArea,
                    stats.affectedArea);
                const float globalStrength = SmoothStepCPU(
                    m_safety.globalDeltaThreshold, 0.45f, std::fabs(stats.globalDelta));
                const float redStrength = SmoothStepCPU(
                    m_safety.redAffectedAreaThreshold, 0.40f, stats.redAffectedArea);
                m_protectionStrength = std::max(m_protectionStrength,
                    std::max(0.35f, std::max(areaStrength, std::max(globalStrength, redStrength))));
                if (redTransition)
                    m_redProtectionStrength = std::max(m_redProtectionStrength, std::max(0.40f, redStrength));
                m_hazardState = HazardState::Protecting;
            }

            // Local protection has its own per-cell luminance history. Keep the
            // unused global limiter aligned with raw output so a later full-screen
            // event starts from the immediately preceding scene brightness.
            if (!m_globalProtectionActive)
                m_displayedGlobalLuma = stats.globalLuma;

            m_flashCountLastSecond = static_cast<int>(m_recentFlashes.size());
            m_alternatingFlashCount = 0;
            for (size_t i = 1; i < m_recentFlashes.size(); ++i)
                if (m_recentFlashes[i - 1].direction != m_recentFlashes[i].direction)
                    ++m_alternatingFlashCount;

            if (m_flashCountLastSecond >= 2)
            {
                const float repeatBoost = std::min(1.0f,
                    0.45f + 0.12f * static_cast<float>(m_flashCountLastSecond - 2) +
                    0.18f * static_cast<float>(m_alternatingFlashCount));
                m_protectionStrength = std::max(m_protectionStrength, repeatBoost);
            }

            if (m_hazardState == HazardState::Safe || m_hazardState == HazardState::Suspect)
            {
                m_hazardState = suspect ? HazardState::Suspect : HazardState::Safe;
                m_displayedGlobalLuma = stats.globalLuma;
                m_protectionStrength = 0.0f;
                m_redProtectionStrength = 0.0f;
                m_globalProtectionActive = false;
                return;
            }

            float rate = stats.globalLuma >= m_displayedGlobalLuma ?
                m_safety.safeRiseRate : m_safety.safeFallRate;
            if (m_flashCountLastSecond >= 2)
            {
                const float repeatScale = std::max(0.35f,
                    1.0f - 0.14f * static_cast<float>(m_flashCountLastSecond - 1) -
                    0.12f * static_cast<float>(m_alternatingFlashCount));
                rate *= repeatScale;
            }

            if (m_hazardState == HazardState::Protecting)
            {
                m_displayedGlobalLuma = MoveTowards(
                    m_displayedGlobalLuma, stats.globalLuma, rate * dt);
                if (!hazard && m_timelineSeconds - m_lastHazardTime >= m_safety.minimumProtectionTime)
                {
                    m_hazardState = HazardState::Releasing;
                    m_releaseStartTime = m_timelineSeconds;
                    m_releaseStartStrength = m_protectionStrength;
                }
            }
            else if (m_hazardState == HazardState::Releasing)
            {
                const float t = std::min(1.0f,
                    (m_timelineSeconds - m_releaseStartTime) / std::max(0.05f, m_safety.releaseTime));
                const float releaseRate = std::max(
                    rate, 1.0f / std::max(0.05f, m_safety.releaseTime));
                m_displayedGlobalLuma = MoveTowards(
                    m_displayedGlobalLuma, stats.globalLuma, releaseRate * dt);
                m_protectionStrength = m_releaseStartStrength * (1.0f - t);
                m_redProtectionStrength *= std::max(0.0f, 1.0f - dt / std::max(0.05f, m_safety.releaseTime));
                if (t >= 1.0f)
                {
                    m_hazardState = HazardState::Safe;
                    m_displayedGlobalLuma = stats.globalLuma;
                    m_protectionStrength = 0.0f;
                    m_redProtectionStrength = 0.0f;
                    m_globalProtectionActive = false;
                }
            }

            // Local flashing must not keep an old global limiter alive. Let the
            // global permitted mean converge first, then retire it independently
            // without a whole-screen step.
            if (m_globalProtectionActive && !globalHazard &&
                m_timelineSeconds - m_lastGlobalHazardTime >= m_safety.minimumProtectionTime &&
                std::fabs(m_displayedGlobalLuma - stats.globalLuma) <= 0.012f)
            {
                m_globalProtectionActive = false;
                m_displayedGlobalLuma = stats.globalLuma;
            }
        }

        void UpdateLocalSafetyMap(const AnalysisStats& stats,
                                  const std::vector<float>& cellLuma,
                                  const std::vector<float>& redChangeMask)
        {
            const size_t count = static_cast<size_t>(kAnalysisWidth) * kAnalysisHeight;
            if (cellLuma.size() != count || redChangeMask.size() != count ||
                m_localSafetyData.size() != count * 4)
                return;

            if (!m_haveLocalHistory)
            {
                m_previousOutputLuma = cellLuma;
                m_localDisplayedLuma = cellLuma;
                std::fill(m_localLumaGate.begin(), m_localLumaGate.end(), 0.0f);
                std::fill(m_localRedGate.begin(), m_localRedGate.end(), 0.0f);
                m_haveLocalHistory = true;
            }

            const float dt = std::max(1.0f / 240.0f, std::min(stats.dt, 0.1f));
            const bool filtering = m_hazardState == HazardState::Protecting ||
                                   m_hazardState == HazardState::Releasing;
            const int dominantDirection = stats.brighteningArea >= stats.darkeningArea ? 1 : -1;
            float rateScale = 1.0f;
            if (m_flashCountLastSecond >= 2)
            {
                rateScale = std::max(0.35f,
                    1.0f - 0.14f * static_cast<float>(m_flashCountLastSecond - 1) -
                    0.12f * static_cast<float>(m_alternatingFlashCount));
            }
            float releaseGate = 1.0f;
            if (m_hazardState == HazardState::Releasing)
            {
                releaseGate = std::max(0.0f, 1.0f -
                    (m_timelineSeconds - m_releaseStartTime) /
                    std::max(0.05f, m_safety.releaseTime));
            }
            const bool motionBypass = !m_globalProtectionActive &&
                stats.cameraMotionScore >= m_safety.cameraMotionSuppression &&
                !m_latestPrediction.futureReversal && !m_latestPrediction.red;

            std::fill(m_activationScratch.begin(), m_activationScratch.end(), 0.0f);
            auto& activationMask = m_activationScratch;
            if (m_lastEventWasLocal)
            {
                for (size_t i = 0; i < count; ++i)
                {
                    const float delta = cellLuma[i] - m_previousOutputLuma[i];
                    const int direction = delta >= 0.0f ? 1 : -1;
                    if (direction == dominantDirection &&
                        std::fabs(delta) >= m_safety.localDeltaThreshold)
                        activationMask[i] = 1.0f;
                }

                const bool smallIntense =
                    stats.strongAffectedArea >= m_safety.smallFlashAreaThreshold &&
                    stats.strongDirectionalCoherence >= m_safety.smallFlashCoherenceThreshold;
                if (smallIntense)
                {
                    const int radius = std::max(0, m_safety.spillExpansionCells);
                    for (int y = 0; y < static_cast<int>(kAnalysisHeight); ++y)
                    {
                        for (int x = 0; x < static_cast<int>(kAnalysisWidth); ++x)
                        {
                            const size_t seed = static_cast<size_t>(y) * kAnalysisWidth + x;
                            const float seedDelta = cellLuma[seed] - m_previousOutputLuma[seed];
                            if (std::fabs(seedDelta) < m_safety.smallFlashDeltaThreshold ||
                                (seedDelta >= 0.0f ? 1 : -1) != dominantDirection)
                                continue;

                            for (int oy = -radius; oy <= radius; ++oy)
                            {
                                const int ny = y + oy;
                                if (ny < 0 || ny >= static_cast<int>(kAnalysisHeight)) continue;
                                for (int ox = -radius; ox <= radius; ++ox)
                                {
                                    const int nx = x + ox;
                                    if (nx < 0 || nx >= static_cast<int>(kAnalysisWidth)) continue;
                                    const size_t neighbor = static_cast<size_t>(ny) * kAnalysisWidth + nx;
                                    const int distance = std::max(std::abs(ox), std::abs(oy));
                                    const float weight = 1.0f - 0.80f *
                                        static_cast<float>(distance) / static_cast<float>(radius + 1);
                                    activationMask[neighbor] = std::max(
                                        activationMask[neighbor], weight);
                                }
                            }
                        }
                    }
                }
            }

            for (size_t i = 0; i < count; ++i)
            {
                const float raw = cellLuma[i];
                const bool redAffected = redChangeMask[i] > 0.5f;

                if (m_lastEventWasLocal && (activationMask[i] > 0.0f || redAffected))
                {
                    m_localLumaGate[i] = std::max(
                        m_localLumaGate[i], redAffected ? 1.0f : activationMask[i]);
                    if (redAffected) m_localRedGate[i] = 1.0f;
                }

                // A gate belongs to the luminance transition, not to a screen
                // coordinate. If unrelated geometry replaces that cell—or a pan
                // explains the frame—drop the old gate immediately so it cannot
                // leave a dim/bright trail behind the moving object.
                const bool replacedContent = activationMask[i] <= 0.0f && !redAffected &&
                    std::fabs(raw - m_previousOutputLuma[i]) >=
                        m_safety.localDeltaThreshold * 0.75f;
                if (motionBypass || replacedContent)
                {
                    m_localLumaGate[i] = 0.0f;
                    m_localRedGate[i] = 0.0f;
                    m_localDisplayedLuma[i] = raw;
                }

                if (!filtering)
                {
                    m_localDisplayedLuma[i] = raw;
                    m_localLumaGate[i] = 0.0f;
                    m_localRedGate[i] = 0.0f;
                }
                else
                {
                    if (m_globalProtectionActive)
                        m_localLumaGate[i] = 0.0f;
                    else if (m_localLumaGate[i] > 0.0f)
                    {
                        const float rate = raw >= m_localDisplayedLuma[i] ?
                            m_safety.safeRiseRate : m_safety.safeFallRate;
                        m_localDisplayedLuma[i] = MoveTowards(
                            m_localDisplayedLuma[i], raw, rate * rateScale * dt);
                    }
                    else
                    {
                        m_localDisplayedLuma[i] = raw;
                    }

                    if (m_hazardState == HazardState::Releasing)
                    {
                        m_localLumaGate[i] = std::min(m_localLumaGate[i], releaseGate);
                        m_localRedGate[i] = std::min(m_localRedGate[i], releaseGate);
                    }
                }

                const size_t base = i * 4;
                m_localSafetyData[base + 0] = raw;
                m_localSafetyData[base + 1] = m_localDisplayedLuma[i];
                m_localSafetyData[base + 2] = m_localLumaGate[i];
                m_localSafetyData[base + 3] = m_localRedGate[i];
                m_previousOutputLuma[i] = m_localDisplayedLuma[i];
            }

            m_context->UpdateSubresource(m_localSafetyTexture.get(), 0, nullptr,
                m_localSafetyData.data(), kAnalysisWidth * 4 * static_cast<UINT>(sizeof(float)), 0);
        }

#include "motion/NvofBackend.inl"

        void RecreateOutputResources()
        {
            ThrowIfFailed(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), m_backBuffer.put_void()));
            ThrowIfFailed(m_device->CreateRenderTargetView(m_backBuffer.get(), nullptr, m_backBufferRTV.put()));

            D3D11_TEXTURE2D_DESC bb{};
            m_backBuffer->GetDesc(&bb);
            m_outputWidth = bb.Width;
            m_outputHeight = bb.Height;

            D3D11_TEXTURE2D_DESC history{};
            history.Width = bb.Width;
            history.Height = bb.Height;
            history.MipLevels = 1;
            history.ArraySize = 1;
            history.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            history.SampleDesc.Count = 1;
            history.Usage = D3D11_USAGE_DEFAULT;
            history.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            for (size_t i = 0; i < m_outputHistoryTextures.size(); ++i)
            {
                // Recreate cleanly after a swap-chain resize. These textures are
                // independent of the swap chain but must match its new dimensions.
                m_outputHistorySRVs[i] = nullptr;
                m_outputHistoryRTVs[i] = nullptr;
                m_outputHistoryTextures[i] = nullptr;
                m_sourceHistorySRVs[i] = nullptr;
                m_sourceHistoryRTVs[i] = nullptr;
                m_sourceHistoryTextures[i] = nullptr;
                m_protectionStateSRVs[i] = nullptr;
                m_protectionStateRTVs[i] = nullptr;
                m_protectionStateTextures[i] = nullptr;
                ThrowIfFailed(m_device->CreateTexture2D(&history, nullptr,
                    m_outputHistoryTextures[i].put()));
                ThrowIfFailed(m_device->CreateRenderTargetView(
                    m_outputHistoryTextures[i].get(), nullptr, m_outputHistoryRTVs[i].put()));
                ThrowIfFailed(m_device->CreateShaderResourceView(
                    m_outputHistoryTextures[i].get(), nullptr, m_outputHistorySRVs[i].put()));
                ThrowIfFailed(m_device->CreateTexture2D(&history, nullptr,
                    m_sourceHistoryTextures[i].put()));
                ThrowIfFailed(m_device->CreateRenderTargetView(
                    m_sourceHistoryTextures[i].get(), nullptr, m_sourceHistoryRTVs[i].put()));
                ThrowIfFailed(m_device->CreateShaderResourceView(
                    m_sourceHistoryTextures[i].get(), nullptr, m_sourceHistorySRVs[i].put()));
                ThrowIfFailed(m_device->CreateTexture2D(&history, nullptr,
                    m_protectionStateTextures[i].put()));
                ThrowIfFailed(m_device->CreateRenderTargetView(
                    m_protectionStateTextures[i].get(), nullptr, m_protectionStateRTVs[i].put()));
                ThrowIfFailed(m_device->CreateShaderResourceView(
                    m_protectionStateTextures[i].get(), nullptr, m_protectionStateSRVs[i].put()));
                const float clearHistory[4] = { 0, 0, 0, 0 };
                m_context->ClearRenderTargetView(m_outputHistoryRTVs[i].get(), clearHistory);
                m_context->ClearRenderTargetView(m_sourceHistoryRTVs[i].get(), clearHistory);
                m_context->ClearRenderTargetView(m_protectionStateRTVs[i].get(), clearHistory);
            }
            m_outputHistoryIndex = 0;
            m_outputHistoryValid = false;
            m_sourceHistoryIndex = 0;
            m_sourceHistoryValid = false;
        }

        void CreateShieldTexture()
        {
            const uint8_t pixel[4] = { 20, 20, 20, 255 };
            D3D11_TEXTURE2D_DESC td{};
            td.Width = 1;
            td.Height = 1;
            td.MipLevels = 1;
            td.ArraySize = 1;
            td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_IMMUTABLE;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA data{};
            data.pSysMem = pixel;
            data.SysMemPitch = 4;
            ThrowIfFailed(m_device->CreateTexture2D(&td, &data, m_shieldTex.put()));
            ThrowIfFailed(m_device->CreateShaderResourceView(m_shieldTex.get(), nullptr, m_shieldSRV.put()));
        }

        void CreateDebugResources()
        {
            D3D11_TEXTURE2D_DESC td{};
            td.Width = kDebugWidth;
            td.Height = kDebugHeight;
            td.MipLevels = 1;
            td.ArraySize = 1;
            td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            ThrowIfFailed(m_device->CreateTexture2D(&td, nullptr, m_debugTexture.put()));
            ThrowIfFailed(m_device->CreateShaderResourceView(m_debugTexture.get(), nullptr, m_debugSRV.put()));
            UpdateDebugTexture(true);
        }

        void CreateHintResources()
        {
            D3D11_TEXTURE2D_DESC td{};
            td.Width = kHintWidth;
            td.Height = kHintHeight;
            td.MipLevels = 1;
            td.ArraySize = 1;
            td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            ThrowIfFailed(m_device->CreateTexture2D(&td, nullptr, m_hintTexture.put()));
            ThrowIfFailed(m_device->CreateShaderResourceView(
                m_hintTexture.get(), nullptr, m_hintSRV.put()));
            td.Width = kAutomaticShieldLabelWidth;
            td.Height = kAutomaticShieldLabelHeight;
            ThrowIfFailed(m_device->CreateTexture2D(
                &td, nullptr, m_automaticShieldLabelTexture.put()));
            ThrowIfFailed(m_device->CreateShaderResourceView(
                m_automaticShieldLabelTexture.get(), nullptr,
                m_automaticShieldLabelSRV.put()));
            UpdateHintTexture();
            UpdateAutomaticShieldLabelTexture();
            m_hintUntilMs = NowMs() + 10000;
        }

        void UpdateAutomaticShieldLabelTexture()
        {
            if (!m_automaticShieldLabelTexture) return;
            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = static_cast<LONG>(kAutomaticShieldLabelWidth);
            bmi.bmiHeader.biHeight = -static_cast<LONG>(kAutomaticShieldLabelHeight);
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            void* bits = nullptr;
            HDC dc = CreateCompatibleDC(nullptr);
            HBITMAP bitmap = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS,
                &bits, nullptr, 0);
            if (!dc || !bitmap || !bits)
            {
                if (bitmap) DeleteObject(bitmap);
                if (dc) DeleteDC(dc);
                return;
            }
            HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
            RECT rect{ 0, 0, static_cast<LONG>(kAutomaticShieldLabelWidth),
                static_cast<LONG>(kAutomaticShieldLabelHeight) };
            HBRUSH background = CreateSolidBrush(RGB(12, 17, 23));
            FillRect(dc, &rect, background);
            DeleteObject(background);
            HFONT font = CreateFontW(-24, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HGDIOBJ oldFont = SelectObject(dc, font);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(225, 234, 245));
            DrawTextW(dc, L"Automatic shield activated", -1, &rect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            auto* pixels = static_cast<uint32_t*>(bits);
            for (size_t i = 0;
                i < static_cast<size_t>(kAutomaticShieldLabelWidth) * kAutomaticShieldLabelHeight;
                ++i)
                pixels[i] = (pixels[i] & 0x00FFFFFFu) | 0xE6000000u;
            m_context->UpdateSubresource(m_automaticShieldLabelTexture.get(), 0,
                nullptr, bits, kAutomaticShieldLabelWidth * 4, 0);
            SelectObject(dc, oldFont);
            SelectObject(dc, oldBitmap);
            DeleteObject(font);
            DeleteObject(bitmap);
            DeleteDC(dc);
        }

        void UpdateHintTexture()
        {
            if (!m_hintTexture) return;
            const std::wstring text = L"FlashGuard   " + HotkeyName(g_hotkeys[0]) +
                L": Toggle shield   " +
                HotkeyName(g_hotkeys[3]) + L": Diagnostics   " +
                HotkeyName(g_hotkeys[4]) + L": Options   " +
                HotkeyName(g_hotkeys[2]) + L": Exit";

            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = static_cast<LONG>(kHintWidth);
            bmi.bmiHeader.biHeight = -static_cast<LONG>(kHintHeight);
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            void* bits = nullptr;
            HDC dc = CreateCompatibleDC(nullptr);
            HBITMAP bitmap = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
            if (!dc || !bitmap || !bits)
            {
                if (bitmap) DeleteObject(bitmap);
                if (dc) DeleteDC(dc);
                return;
            }
            HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
            RECT rect{ 0, 0, static_cast<LONG>(kHintWidth), static_cast<LONG>(kHintHeight) };
            HBRUSH background = CreateSolidBrush(RGB(6, 10, 8));
            FillRect(dc, &rect, background);
            DeleteObject(background);
            HFONT font = CreateFontW(-15, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HGDIOBJ oldFont = SelectObject(dc, font);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(205, 225, 210));
            rect.left = 12;
            rect.top = 13;
            DrawTextW(dc, text.c_str(), -1, &rect,
                DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            auto* pixels = static_cast<uint32_t*>(bits);
            for (size_t i = 0; i < static_cast<size_t>(kHintWidth) * kHintHeight; ++i)
                pixels[i] = (pixels[i] & 0x00FFFFFFu) | 0xC8000000u;
            m_context->UpdateSubresource(
                m_hintTexture.get(), 0, nullptr, bits, kHintWidth * 4, 0);
            SelectObject(dc, oldFont);
            SelectObject(dc, oldBitmap);
            DeleteObject(font);
            DeleteObject(bitmap);
            DeleteDC(dc);
        }

        const wchar_t* HazardStateName() const
        {
            switch (m_hazardState)
            {
            case HazardState::Safe: return L"SAFE";
            case HazardState::Suspect: return L"SUSPECT";
            case HazardState::Protecting: return L"PROTECTING";
            case HazardState::Releasing: return L"RELEASING";
            default: return L"UNKNOWN";
            }
        }

        void UpdateDebugTexture(bool force = false)
        {
            if (!m_debugTexture) return;
            const int64_t now = NowMs();
            if (!force && now - m_lastDebugTextureMs < 100) return;
            m_lastDebugTextureMs = now;

            wchar_t text[2048]{};
            const int latencyMs = m_safety.lookaheadMs;
            swprintf_s(text, sizeof(text) / sizeof(text[0]),
                L"FLASHGUARD DETECTOR: ACTIVE\n"
                L"Global luma:       %.3f\n"
                L"Global delta:     %+0.3f\n"
                L"Affected area:     %.1f%%\n"
                L"Bright / dark:     %.1f%% / %.1f%%\n"
                L"Coherence:         %.1f%%\n"
                L"Largest region:    %.1f%%\n"
                L"10-deg field:      %.1f%%\n"
                L"Flash energy:      %.3f\n"
                L"Motion explained:  %.1f%%\n"
                L"Motion backend:    %s (grid %u)\n"
                L"Pattern score:     %.1f%%\n"
                L"Red affected:      %.1f%%\n"
                L"Flashes / 1 sec:   %d (alternating %d)\n"
                L"State:             %s\n"
                L"Trigger:           %s\n"
                L"Strength:          %.2f\n"
                L"Future stats:      %d frames\n"
                L"Buffered frames:   %zu\n"
                L"Latency target:    %d ms\n"
                L"Desktop frame age: %.2f ms\n"
                L"Desktop accumulated: %u\n"
                L"Capture processing: %.2f ms\n"
                L"Present queue wait: %.2f ms\n"
                L"Present call:       %.2f ms\n"
                L"Analysis misses:   %llu\n"
                L"Dropped presents:  %llu",
                m_latestStats.globalLuma, m_latestStats.globalDelta,
                m_latestStats.affectedArea * 100.0f,
                m_latestStats.brighteningArea * 100.0f,
                m_latestStats.darkeningArea * 100.0f,
                m_latestStats.directionalCoherence * 100.0f,
                m_latestStats.largestRegionArea * 100.0f,
                m_latestStats.visualFieldAffectedArea * 100.0f,
                m_latestStats.flashEnergy,
                m_latestStats.cameraMotionScore * 100.0f,
                (g_liveDisableNvofForLatencyTest && !m_replayMode) ?
                    L"NVOFA BYPASSED (LATENCY TEST)" :
                    (m_nvofHandle ?
                        (m_nvofFlowValid ?
                            (m_nvofCostEnabled ? L"NVOFA ACTIVE+COST 0.5x FAST" : L"NVOFA ACTIVE 0.5x FAST") :
                            (m_nvofCostEnabled ? L"NVOFA ANCHOR+COST 0.5x FAST" : L"NVOFA ANCHOR 0.5x FAST")) :
                        L"FALLBACK"),
                static_cast<unsigned>(m_nvofGridSize),
                m_latestStats.patternScore * 100.0f,
                m_latestStats.redAffectedArea * 100.0f,
                m_flashCountLastSecond, m_alternatingFlashCount,
                HazardStateName(),
                m_latestPrediction.pattern ? L"PATTERN" :
                m_latestPrediction.red ? L"SATURATED RED" :
                m_latestPrediction.global ? L"GLOBAL" :
                m_latestPrediction.local ? L"LOCAL" : L"NONE",
                m_protectionStrength,
                m_predictionFrames, m_bufferedFrameCount, latencyMs,
                m_captureImageAgeMs.load(std::memory_order_acquire),
                m_captureAccumulatedFrames.load(std::memory_order_acquire),
                m_captureProcessMs.load(std::memory_order_acquire),
                m_presentReadyWaitMs.load(std::memory_order_acquire),
                m_presentCallMs.load(std::memory_order_acquire),
                static_cast<unsigned long long>(m_analysisDeadlineMisses),
                static_cast<unsigned long long>(
                    m_droppedPresents.load(std::memory_order_acquire)));

            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = static_cast<LONG>(kDebugWidth);
            bmi.bmiHeader.biHeight = -static_cast<LONG>(kDebugHeight);
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            void* bits = nullptr;
            HDC dc = CreateCompatibleDC(nullptr);
            HBITMAP bitmap = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
            if (!dc || !bitmap || !bits)
            {
                if (bitmap) DeleteObject(bitmap);
                if (dc) DeleteDC(dc);
                return;
            }
            HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
            RECT rect{ 0, 0, static_cast<LONG>(kDebugWidth), static_cast<LONG>(kDebugHeight) };
            HBRUSH background = CreateSolidBrush(RGB(7, 12, 9));
            FillRect(dc, &rect, background);
            DeleteObject(background);
            HFONT font = CreateFontW(-17, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
            HGDIOBJ oldFont = SelectObject(dc, font);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(110, 235, 145));
            rect.left = 12;
            rect.top = 8;
            DrawTextW(dc, text, -1, &rect, DT_LEFT | DT_TOP | DT_NOPREFIX);

            auto* pixels = static_cast<uint32_t*>(bits);
            for (size_t i = 0; i < static_cast<size_t>(kDebugWidth) * kDebugHeight; ++i)
                pixels[i] = (pixels[i] & 0x00FFFFFFu) | 0xD2000000u;
            m_context->UpdateSubresource(m_debugTexture.get(), 0, nullptr, bits, kDebugWidth * 4, 0);

            SelectObject(dc, oldFont);
            SelectObject(dc, oldBitmap);
            DeleteObject(font);
            DeleteObject(bitmap);
            DeleteDC(dc);
        }

        void UpdateConstants(float rawMean, float permittedMean, float protectionGate,
                             float redGate, bool overloadFallback, bool broadLocalTransition,
                             bool idleReleaseTick)
        {
            ShaderConstants c{};
            c.p0[0] = rawMean;
            c.p0[1] = permittedMean;
            c.p0[2] = protectionGate;
            c.p0[3] = redGate;
            c.p1[0] = m_safety.redThreshold;
            c.p1[1] = m_safety.redDesaturation;
            c.p1[2] = m_safety.subtleToneMap ? 1.0f : 0.0f;
            c.p1[3] = m_safety.blackFloor;
            c.p2[0] = m_safety.whiteCeiling;
            c.p2[1] = m_debugEnabled.load(std::memory_order_acquire) ? 1.0f : 0.0f;
            c.p2[2] = static_cast<float>(m_outputWidth);
            c.p2[3] = static_cast<float>(m_outputHeight);
            const int64_t behaviorNowMs = m_replayMode ?
                static_cast<int64_t>(std::llround(m_replayClockSeconds * 1000.0)) : NowMs();
            c.p3[0] = behaviorNowMs < m_hintUntilMs ? 1.0f : 0.0f;
            c.p3[1] = overloadFallback ? 1.0f : 0.0f;
            c.p3[2] = m_safety.overloadWhiteCeiling;
            c.p3[3] = m_automaticShieldActive.load(std::memory_order_acquire) ? 1.0f : 0.0f;
            c.p4[0] = m_safety.localDeltaThreshold;
            c.p4[1] = m_safety.coherenceThreshold;
            c.p4[2] = m_safety.safeRiseRate * m_instantFrameDt;
            c.p4[3] = m_safety.safeFallRate * m_instantFrameDt;
            c.p5[0] = m_safety.cameraMotionSuppression;
            c.p5[1] = m_safety.redDeltaThreshold;
            c.p5[2] = m_instantHistoryValid ? 1.0f : 0.0f;
            c.p5[3] = m_instantFrameDt;
            const float shaderCameraMotionScore =
                m_benchmarkArchitectureMode == 22 ?
                    m_latestStats.structuralCameraMotionScore :
                    m_latestStats.cameraMotionScore;
            c.p6[1] = m_latestStats.validDelta ? shaderCameraMotionScore : 0.0f;
            c.p6[2] = 1.0f / static_cast<float>(kAnalysisWidth);
            c.p6[3] = 1.0f / static_cast<float>(kAnalysisHeight);
            c.p6[0] = broadLocalTransition ? 1.0f : 0.0f;
            c.p7[0] = m_safety.lookaheadMs == 0 ? 1.0f : 0.0f;
            c.p7[1] = m_outputHistoryValid ? 1.0f : 0.0f;
            c.p7[2] = m_sourceHistoryValid ? 1.0f : 0.0f;
            c.p7[3] = 0.066f;
            c.p8[0] = m_nvofFlowValid ? 1.0f : (m_nvofHandle ? 0.5f : 0.0f);
            c.p8[1] = static_cast<float>(m_nvofGridSize);
            c.p8[2] = static_cast<float>(m_nvofFlowWidth);
            c.p8[3] = static_cast<float>(m_nvofFlowHeight);
            c.p9[0] = idleReleaseTick ? 1.0f : 0.0f;
            c.p9[1] = m_nvofInputWidth > 0 ?
                static_cast<float>(m_outputWidth) / static_cast<float>(m_nvofInputWidth) : 1.0f;
            c.p9[2] = m_nvofInputHeight > 0 ?
                static_cast<float>(m_outputHeight) / static_cast<float>(m_nvofInputHeight) : 1.0f;
            c.p9[3] = (m_nvofFlowValid && m_nvofCostEnabled) ? 1.0f : 0.0f;
            c.p10[0] = m_shaderTuning.eventDeltaLow;
            c.p10[1] = m_shaderTuning.eventDeltaHigh;
            c.p10[2] = m_shaderTuning.holdDeltaLow;
            c.p10[3] = m_shaderTuning.holdDeltaHigh;
            c.p11[0] = m_shaderTuning.stableSourceLow;
            c.p11[1] = m_shaderTuning.stableSourceHigh;
            c.p11[2] = m_shaderTuning.intrinsicResidualLow;
            c.p11[3] = m_shaderTuning.intrinsicResidualHigh;
            c.p12[0] = m_shaderTuning.repeatedMemoryLow;
            c.p12[1] = m_shaderTuning.repeatedMemoryHigh;
            c.p12[2] = m_shaderTuning.holdGateLow;
            c.p12[3] = m_shaderTuning.holdGateHigh;
            c.p13[0] = m_shaderTuning.transportConfidenceLow;
            c.p13[1] = m_shaderTuning.transportConfidenceHigh;
            c.p13[2] = m_shaderTuning.disocclusionResetGate;
            c.p13[3] = m_shaderTuning.surfaceRiskTau;
            c.p14[0] = m_shaderTuning.eventStateTauScale;
            c.p14[1] = m_shaderTuning.releaseStateTauScale;
            c.p14[2] = m_shaderTuning.exactHoldThreshold;
            c.p14[3] = m_shaderTuning.movingHoldFloorMax;
            c.p15[0] = m_shaderTuning.directIntrinsicDisplayLow;
            c.p15[1] = m_shaderTuning.directIntrinsicDisplayHigh;
            c.p15[2] = m_shaderTuning.eventSeedLow;
            c.p15[3] = m_shaderTuning.eventSeedHigh;
            c.p16[0] = static_cast<float>(m_benchmarkArchitectureMode);
            c.p16[1] = m_benchmarkRiskOnlyNeutralLuma;
            c.p16[2] = m_benchmarkRiskOnlyGain;
            c.p16[3] = m_replayMode ? 0.0f : 1.0f;

            D3D11_MAPPED_SUBRESOURCE mapped{};
            ThrowIfFailed(m_context->Map(m_constants.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
            memcpy(mapped.pData, &c, sizeof(c));
            m_context->Unmap(m_constants.get(), 0);
        }

        void RenderSource(ID3D11ShaderResourceView* source, float rawMean,
                          float permittedMean, float protectionGate, float redGate,
                          bool overloadFallback = false,
                          bool broadLocalTransition = false,
                          bool useOpticalFlow = true,
                          bool idleReleaseTick = false)
        {
            const bool bypassLiveNvof =
                g_liveDisableNvofForLatencyTest && !m_replayMode;
            if (useOpticalFlow && !bypassLiveNvof)
            {
                // NVOFA runs on its dedicated hardware engine and is current-frame
                // evidence. Execute it for every new captured source frame instead
                // of asking delayed CPU readback whether motion is worth measuring.
                // Idle-release/shield renders pass useOpticalFlow=false and do not
                // spend an optical-flow solve.
                UpdateOpticalFlow(source, true);
            }
            else
                m_nvofFlowValid = false;
            if (m_debugEnabled.load(std::memory_order_acquire)) UpdateDebugTexture();
            UpdateConstants(rawMean, permittedMean, protectionGate, redGate,
                overloadFallback, broadLocalTransition, idleReleaseTick);

            D3D11_VIEWPORT vp{};
            vp.Width = static_cast<float>(m_outputWidth);
            vp.Height = static_cast<float>(m_outputHeight);
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            m_context->RSSetViewports(1, &vp);

            const size_t historyWriteIndex = 1 - m_outputHistoryIndex;
            const size_t sourceHistoryWriteIndex = 1 - m_sourceHistoryIndex;
            // The three full-resolution motion diagnostic targets are expensive
            // bandwidth and are not part of protection state. Keep them enabled
            // for synthetic replay/Matrix evidence and while F9 diagnostics are
            // actively requested, but do not write them during ordinary live use.
            const bool writeMotionDiagnostics =
                m_replayMode || m_debugEnabled.load(std::memory_order_acquire);
            ID3D11RenderTargetView* rtvs[] = {
                m_backBufferRTV.get(),
                m_outputHistoryRTVs[historyWriteIndex].get(),
                m_sourceHistoryRTVs[sourceHistoryWriteIndex].get(),
                m_protectionStateRTVs[historyWriteIndex].get(),
                m_motionDiagnosticRTVs[0].get(),
                m_motionDiagnosticRTVs[1].get(),
                m_motionDiagnosticRTVs[2].get()
            };
            const UINT rtvCount =
                writeMotionDiagnostics && m_motionDiagnosticRTVs[0] ? 7u : 4u;
            m_context->OMSetRenderTargets(rtvCount, rtvs, nullptr);
            m_context->IASetInputLayout(nullptr);
            m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_context->VSSetShader(m_vs.get(), nullptr, 0);
            m_context->PSSetShader(m_ps.get(), nullptr, 0);

            ID3D11ShaderResourceView* srvs[] = {
                source, m_debugSRV.get(),
                m_safety.lookaheadMs == 0 ?
                    m_instantSafetySRVs[m_instantSafetyIndex].get() : m_localSafetySRV.get(),
                m_hintSRV.get(),
                m_safety.lookaheadMs == 0 ? m_analysisSRV.get() : nullptr,
                m_safety.lookaheadMs == 0 ? m_instantPreviousAnalysisSRV.get() : nullptr,
                m_automaticShieldLabelSRV.get()
            };
            m_context->PSSetShaderResources(0, 7, srvs);
            ID3D11ShaderResourceView* temporalSRV = m_safety.lookaheadMs == 0 ?
                m_instantTemporalSRVs[m_instantSafetyIndex].get() : nullptr;
            m_context->PSSetShaderResources(7, 1, &temporalSRV);
            ID3D11ShaderResourceView* outputHistorySRV = m_outputHistoryValid ?
                m_outputHistorySRVs[m_outputHistoryIndex].get() : nullptr;
            m_context->PSSetShaderResources(8, 1, &outputHistorySRV);
            ID3D11ShaderResourceView* sourceHistorySRV = m_sourceHistoryValid ?
                m_sourceHistorySRVs[m_sourceHistoryIndex].get() : nullptr;
            m_context->PSSetShaderResources(9, 1, &sourceHistorySRV);
            ID3D11ShaderResourceView* flowSRVs[2] = {
                m_nvofFlowValid ? m_nvofForwardSRV.get() : nullptr,
                m_nvofFlowValid ? m_nvofBackwardSRV.get() : nullptr
            };
            m_context->PSSetShaderResources(10, 2, flowSRVs);
            ID3D11ShaderResourceView* costSRVs[2] = {
                (m_nvofFlowValid && m_nvofCostEnabled) ? m_nvofForwardCostSRV.get() : nullptr,
                (m_nvofFlowValid && m_nvofCostEnabled) ? m_nvofBackwardCostSRV.get() : nullptr
            };
            m_context->PSSetShaderResources(12, 2, costSRVs);
            ID3D11ShaderResourceView* globalFlowSRV =
                (m_nvofFlowValid && m_nvofGlobalFlowEnabled) ?
                m_nvofGlobalFlowSRV.get() : nullptr;
            m_context->PSSetShaderResources(14, 1, &globalFlowSRV);
            ID3D11ShaderResourceView* protectionStateSRV = m_outputHistoryValid ?
                m_protectionStateSRVs[m_outputHistoryIndex].get() : nullptr;
            m_context->PSSetShaderResources(15, 1, &protectionStateSRV);
            ID3D11SamplerState* sampler = m_sampler.get();
            m_context->PSSetSamplers(0, 1, &sampler);
            ID3D11Buffer* cb = m_constants.get();
            m_context->PSSetConstantBuffers(0, 1, &cb);
            m_context->Draw(3, 0);

            ID3D11ShaderResourceView* nullSRV[7] = {
                nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
            };
            m_context->PSSetShaderResources(0, 7, nullSRV);
            ID3D11ShaderResourceView* nullTemporalSRV = nullptr;
            m_context->PSSetShaderResources(7, 1, &nullTemporalSRV);
            ID3D11ShaderResourceView* nullOutputHistorySRV = nullptr;
            m_context->PSSetShaderResources(8, 1, &nullOutputHistorySRV);
            ID3D11ShaderResourceView* nullSourceHistorySRV = nullptr;
            m_context->PSSetShaderResources(9, 1, &nullSourceHistorySRV);
            ID3D11ShaderResourceView* nullFlowSRVs[2] = { nullptr, nullptr };
            m_context->PSSetShaderResources(10, 2, nullFlowSRVs);
            ID3D11ShaderResourceView* nullCostSRVs[2] = { nullptr, nullptr };
            m_context->PSSetShaderResources(12, 2, nullCostSRVs);
            ID3D11ShaderResourceView* nullGlobalFlowSRV = nullptr;
            m_context->PSSetShaderResources(14, 1, &nullGlobalFlowSRV);
            ID3D11ShaderResourceView* nullProtectionStateSRV = nullptr;
            m_context->PSSetShaderResources(15, 1, &nullProtectionStateSRV);
            ID3D11RenderTargetView* nullRTVs[7] = {
                nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
            };
            m_context->OMSetRenderTargets(rtvCount, nullRTVs, nullptr);
            m_outputHistoryIndex = historyWriteIndex;
            m_outputHistoryValid = true;
            m_sourceHistoryIndex = sourceHistoryWriteIndex;
            m_sourceHistoryValid = true;
            if (!m_replayMode)
            {
                // Desktop Duplication already paces capture to desktop updates. Queue
                // this frame immediately but do not discard it: DO_NOT_WAIT produced
                // visible motion judder whenever the compositor was briefly busy.
                const auto presentStart = std::chrono::steady_clock::now();
                const UINT presentFlags =
                    m_allowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
                const HRESULT presentHr = m_swapChain->Present(0, presentFlags);
                m_presentCallMs.store(std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - presentStart).count(),
                    std::memory_order_release);
                ThrowIfFailed(presentHr);
            }
            else
            {
                m_presentCallMs.store(0.0f, std::memory_order_release);
            }
        }

        void QueueCapturedFrame(ID3D11Texture2D* source, float dt)
        {
            m_instantFrameDt = std::clamp(dt, 1.0f / 240.0f, 0.05f);
            if (m_replayMode)
                m_replayClockSeconds += static_cast<double>(m_instantFrameDt);

            // Keep feedback rendering alive briefly if the desktop becomes static.
            // This is long enough for the fast release path to converge even after
            // a near full-range protected transition.
            const int64_t behaviorNowMs = m_replayMode ?
                static_cast<int64_t>(std::llround(m_replayClockSeconds * 1000.0)) : NowMs();
            m_idleReleaseUntilMs.store(behaviorNowMs + 500, std::memory_order_release);
            EnsureRawRing(source);
            if (m_rawFrames.empty()) return;

            if (m_bufferedFrameCount == m_rawFrames.size())
            {
                m_bufferedDuration = std::max(0.0f,
                    m_bufferedDuration - m_rawFrames[m_ringRead].captureDt);
                m_ringRead = (m_ringRead + 1) % m_rawFrames.size();
                --m_bufferedFrameCount;
            }

            RawFrameSlot& incoming = m_rawFrames[m_ringWrite];
            incoming.sequence = ++m_nextSequence;
            incoming.statsReady = false;
            incoming.stats = {};
            incoming.stats.dt = dt;
            incoming.captureDt = dt;
            m_context->CopyResource(incoming.texture.get(), source);
            DrawAnalysis(incoming.srv.get(), incoming.sequence, dt);
            m_ringWrite = (m_ringWrite + 1) % m_rawFrames.size();
            ++m_bufferedFrameCount;
            m_bufferedDuration += dt;

            if (m_replayMode)
                ResolveAnalysisResults(incoming.sequence, true);
            else
                ResolveAnalysisResults(0, false);
            if (m_safety.lookaheadMs == 0)
            {
                DrawInstantSafetyMap(dt);
                const bool hardGlobalTrigger = m_latestStats.validDelta &&
                    m_latestStats.affectedArea >= m_safety.globalAreaThreshold &&
                    m_latestStats.directionalCoherence >= m_safety.coherenceThreshold;
                if (m_havePrevAnalysis)
                    UpdateHardRiseLimiter(m_latestStats, hardGlobalTrigger);
                const bool hardRiseLimited = m_haveHardRiseLuma && m_hardGlobalActive &&
                    std::fabs(m_hardRiseLuma - m_latestStats.globalLuma) > 0.001f;
                const bool broadTransition = m_latestStats.validDelta &&
                    m_latestStats.affectedArea >= 0.18f &&
                    m_latestStats.strongAffectedArea >= 0.08f &&
                    m_latestStats.directionalCoherence >= m_safety.coherenceThreshold * 0.85f &&
                    !hardGlobalTrigger &&
                    m_latestStats.cameraMotionScore < m_safety.cameraMotionSuppression;
                RenderSource(incoming.srv.get(),
                    m_haveHardRiseLuma ? m_latestStats.globalLuma : 0.5f,
                    m_haveHardRiseLuma ? m_hardRiseLuma : 0.5f,
                    hardRiseLimited ? -1.0f : 0.0f, 0.0f, false, broadTransition);
                m_context->CopyResource(m_instantPreviousAnalysis.get(), m_analysisTex.get());
                m_instantHistoryValid = true;
                m_ringRead = m_ringWrite;
                m_bufferedFrameCount = 0;
                m_bufferedDuration = 0.0f;
                return;
            }
            if (m_bufferedFrameCount > 1 &&
                m_bufferedDuration >= static_cast<float>(m_safety.lookaheadMs) / 1000.0f)
            {
                RawFrameSlot& delayed = m_rawFrames[m_ringRead];
                // The look-ahead normally makes this query ready. Do not block if
                // the GPU is overloaded; blocking here is perceived as game lag.
                ResolveAnalysisResults(delayed.sequence, false);
                if (delayed.statsReady)
                {
                    const HazardPrediction prediction = BuildHazardPrediction(delayed);
                    UpdateHazardState(delayed.stats, prediction);
                    UpdateLocalSafetyMap(
                        delayed.stats, delayed.cellLuma, delayed.redChangeMask);
                    const bool filtering = m_hazardState == HazardState::Protecting ||
                                           m_hazardState == HazardState::Releasing;
                    const float permittedLuma = m_displayedGlobalLuma < delayed.stats.globalLuma ?
                        std::min(m_displayedGlobalLuma, m_hardRiseLuma) : m_hardRiseLuma;
                    const bool hardRiseLimited = m_hardGlobalActive &&
                        std::fabs(permittedLuma - delayed.stats.globalLuma) > 0.001f;
                    const bool broadTransition = delayed.stats.validDelta &&
                        delayed.stats.affectedArea >= 0.14f &&
                        delayed.stats.strongAffectedArea >= 0.05f &&
                        delayed.stats.cameraMotionScore < m_safety.cameraMotionSuppression;
                    RenderSource(delayed.srv.get(), delayed.stats.globalLuma,
                        permittedLuma,
                        hardRiseLimited ? -1.0f :
                            (filtering && m_globalProtectionActive ? 1.0f : 0.0f),
                        filtering && m_globalProtectionActive ? m_redProtectionStrength : 0.0f,
                        false, broadTransition);
                }
                else
                {
                    ++m_analysisDeadlineMisses;
                    std::fill(m_localSafetyData.begin(), m_localSafetyData.end(), 0.0f);
                    m_context->UpdateSubresource(m_localSafetyTexture.get(), 0, nullptr,
                        m_localSafetyData.data(), kAnalysisWidth * 4 * static_cast<UINT>(sizeof(float)), 0);
                    RenderSource(delayed.srv.get(), 0.5f, 0.5f, 0.0f, 0.0f, true);
                }
                m_bufferedDuration = std::max(0.0f, m_bufferedDuration - delayed.captureDt);
                m_ringRead = (m_ringRead + 1) % m_rawFrames.size();
                --m_bufferedFrameCount;
            }
        }

        void RenderIdleReleaseStep(float dt)
        {
            std::scoped_lock lock(m_mutex);
            if (!m_context || !m_swapChain || m_stopped.load() ||
                !m_outputHistoryValid || !m_sourceHistoryValid)
                return;

            // Re-render the last RAW source through the feedback filter. P9.x tells
            // PSMain that there was no new desktop event, so stale CURRENT-event
            // bits cannot keep the strong attack filter engaged. Accumulated memory
            // may still perform the fast release until output catches the source.
            m_instantFrameDt = std::clamp(dt, 1.0f / 240.0f, 0.05f);
            ID3D11ShaderResourceView* lastRawSource =
                m_sourceHistorySRVs[m_sourceHistoryIndex].get();
            if (!lastRawSource) return;

            RenderSource(lastRawSource, 0.5f, 0.5f, 0.0f, 0.0f,
                false, false, false, true);
        }

        void ClearAllToBlack()
        {
            const float black[4] = { 0, 0, 0, 1 };
            if (m_backBufferRTV) m_context->ClearRenderTargetView(m_backBufferRTV.get(), black);
            const float historyBlack[4] = { 0, 0, 0, 0 };
            for (auto& rtv : m_outputHistoryRTVs)
                if (rtv) m_context->ClearRenderTargetView(rtv.get(), historyBlack);
            for (auto& rtv : m_sourceHistoryRTVs)
                if (rtv) m_context->ClearRenderTargetView(rtv.get(), historyBlack);
            // Protection-state alpha is its validity. Clear both ping-pong buffers
            // so risk/signed-prime state cannot survive a replay/capture reset.
            for (auto& rtv : m_protectionStateRTVs)
                if (rtv) m_context->ClearRenderTargetView(rtv.get(), historyBlack);
            for (auto& rtv : m_motionDiagnosticRTVs)
                if (rtv) m_context->ClearRenderTargetView(rtv.get(), historyBlack);
            m_outputHistoryValid = false;
            m_sourceHistoryValid = false;
            if (m_swapChain && !m_replayMode) m_swapChain->Present(1, 0);
        }

        void RenderShieldStep()
        {
            std::scoped_lock lock(m_mutex);
            if (!m_context || !m_swapChain || !m_shieldTex || m_stopped.load()) return;
            try
            {
                constexpr float shieldLuma = 20.0f / 255.0f;
                if (!m_haveDisplayedLuma)
                {
                    m_displayedGlobalLuma = shieldLuma;
                    m_haveDisplayedLuma = true;
                }
                m_displayedGlobalLuma = MoveTowards(
                    m_displayedGlobalLuma, shieldLuma, m_safety.safeFallRate / 60.0f);
                m_hazardState = HazardState::Protecting;
                m_protectionStrength = 1.0f;
                std::fill(m_localSafetyData.begin(), m_localSafetyData.end(), 0.0f);
                m_context->UpdateSubresource(m_localSafetyTexture.get(), 0, nullptr,
                    m_localSafetyData.data(), kAnalysisWidth * 4 * static_cast<UINT>(sizeof(float)), 0);
                RenderSource(m_shieldSRV.get(), shieldLuma, m_displayedGlobalLuma,
                    1.0f, 0.0f, false, false, false);
            }
            catch (...) {}
        }

        HWND m_output{};
        HMONITOR m_monitor{};
        SafetySettings m_safety{};
        ShaderTuningSettings m_shaderTuning{};
        int m_benchmarkArchitectureMode = 0;
        float m_benchmarkRiskOnlyNeutralLuma = 0.18f;
        float m_benchmarkRiskOnlyGain = 0.92f;
        float m_contrastReduction = 2.0f / 3.0f;
        int m_profilePreset = 1;
        int m_fullScreenSensitivity = 1;
        int m_smallSourceSensitivity = 1;
        int m_displaySizePreset = 1;
        int m_viewingDistancePreset = 1;
        std::mutex m_mutex;
        std::thread m_captureThread;
        std::thread m_processingThread;
        std::mutex m_liveFrameMutex;
        std::condition_variable m_liveFrameCv;
        std::array<LiveCaptureSlot, 3> m_liveCaptureSlots;
        size_t m_liveWriteCursor = 0;
        size_t m_liveProcessingSlot = 3;
        uint64_t m_liveCaptureSequence = 0;
        std::atomic<uint64_t> m_liveFrameGeneration{ 1 };
        std::atomic<bool> m_stopped{ false };
        std::atomic<bool> m_cleanupDone{ false };
        std::atomic<bool> m_manualShield{ false };
        std::atomic<bool> m_captureFault{ false };
        std::atomic<bool> m_automaticShieldActive{ false };
        std::atomic<int64_t> m_captureFaultSinceMs{ 0 };
        std::atomic<int> m_captureRecoveryFrames{ 0 };
        std::atomic<bool> m_captureRestartRequested{ false };
        std::atomic<bool> m_debugEnabled{ false };
        std::atomic<int64_t> m_lastFrameMs{ 0 };
        std::atomic<int64_t> m_lastRealCaptureMs{ 0 };
        std::atomic<int64_t> m_idleReleaseUntilMs{ 0 };
        std::atomic<uint32_t> m_validCaptureFrames{ 0 };
        std::atomic<float> m_captureImageAgeMs{ 0.0f };
        std::atomic<uint32_t> m_captureAccumulatedFrames{ 0 };
        std::atomic<float> m_captureProcessMs{ 0.0f };
        std::atomic<float> m_presentReadyWaitMs{ 0.0f };
        std::atomic<float> m_presentCallMs{ 0.0f };
        std::chrono::steady_clock::time_point m_lastFrameTime{ std::chrono::steady_clock::now() };

        winrt::com_ptr<ID3D11Device> m_device;
        winrt::com_ptr<ID3D11DeviceContext> m_context;
        winrt::com_ptr<IDXGIOutput1> m_dxgiOutput;
        winrt::com_ptr<IDXGIOutputDuplication> m_duplication;
        winrt::com_ptr<IDXGISwapChain1> m_swapChain;
        HANDLE m_frameLatencyWaitableObject = nullptr;
        bool m_allowTearing = false;
        winrt::com_ptr<ID3D11Texture2D> m_backBuffer;
        winrt::com_ptr<ID3D11RenderTargetView> m_backBufferRTV;
        std::array<winrt::com_ptr<ID3D11Texture2D>, 2> m_outputHistoryTextures;
        std::array<winrt::com_ptr<ID3D11RenderTargetView>, 2> m_outputHistoryRTVs;
        std::array<winrt::com_ptr<ID3D11ShaderResourceView>, 2> m_outputHistorySRVs;
        std::array<winrt::com_ptr<ID3D11Texture2D>, 2> m_protectionStateTextures;
        std::array<winrt::com_ptr<ID3D11RenderTargetView>, 2> m_protectionStateRTVs;
        std::array<winrt::com_ptr<ID3D11ShaderResourceView>, 2> m_protectionStateSRVs;
        size_t m_outputHistoryIndex = 0;
        bool m_outputHistoryValid = false;
        bool m_replayMode = false;
        double m_replayClockSeconds = 0.0;
        winrt::com_ptr<ID3D11Texture2D> m_replayReadback;
        std::array<winrt::com_ptr<ID3D11Texture2D>, 3> m_motionDiagnosticTextures;
        std::array<winrt::com_ptr<ID3D11RenderTargetView>, 3> m_motionDiagnosticRTVs;
        std::array<winrt::com_ptr<ID3D11Texture2D>, 3> m_motionDiagnosticReadbacks;
        std::array<winrt::com_ptr<ID3D11Texture2D>, 2> m_sourceHistoryTextures;
        std::array<winrt::com_ptr<ID3D11RenderTargetView>, 2> m_sourceHistoryRTVs;
        std::array<winrt::com_ptr<ID3D11ShaderResourceView>, 2> m_sourceHistorySRVs;
        size_t m_sourceHistoryIndex = 0;
        bool m_sourceHistoryValid = false;

        winrt::com_ptr<ID3D11VertexShader> m_vs;
        winrt::com_ptr<ID3D11PixelShader> m_ps;
        winrt::com_ptr<ID3D11PixelShader> m_psAnalyze;
        winrt::com_ptr<ID3D11PixelShader> m_psInstantSafety;
        winrt::com_ptr<ID3D11PixelShader> m_psOpticalFlowCopy;
        winrt::com_ptr<ID3D11SamplerState> m_sampler;

        HMODULE m_nvofModule = nullptr;
        nvof5::NV_OF_D3D11_API_FUNCTION_LIST m_nvofApi{};
        nvof5::NvOFHandle m_nvofHandle = nullptr;
        // NVIDIA recommends a small input/reference pool instead of immediately
        // reusing a two-buffer ping-pong; four surfaces reduce cross-engine
        // resource hazards while keeping memory modest.
        std::array<winrt::com_ptr<ID3D11Texture2D>, 4> m_nvofInputTextures;
        std::array<winrt::com_ptr<ID3D11RenderTargetView>, 4> m_nvofInputRTVs;
        std::array<nvof5::NvOFGPUBufferHandle, 4> m_nvofInputHandles{};
        winrt::com_ptr<ID3D11Texture2D> m_nvofForwardTexture;
        winrt::com_ptr<ID3D11Texture2D> m_nvofBackwardTexture;
        winrt::com_ptr<ID3D11ShaderResourceView> m_nvofForwardSRV;
        winrt::com_ptr<ID3D11ShaderResourceView> m_nvofBackwardSRV;
        winrt::com_ptr<ID3D11Texture2D> m_nvofForwardCostTexture;
        winrt::com_ptr<ID3D11Texture2D> m_nvofBackwardCostTexture;
        winrt::com_ptr<ID3D11Texture2D> m_nvofGlobalFlowTexture;
        winrt::com_ptr<ID3D11ShaderResourceView> m_nvofForwardCostSRV;
        winrt::com_ptr<ID3D11ShaderResourceView> m_nvofBackwardCostSRV;
        winrt::com_ptr<ID3D11ShaderResourceView> m_nvofGlobalFlowSRV;
        nvof5::NvOFGPUBufferHandle m_nvofForwardHandle = nullptr;
        nvof5::NvOFGPUBufferHandle m_nvofBackwardHandle = nullptr;
        nvof5::NvOFGPUBufferHandle m_nvofForwardCostHandle = nullptr;
        nvof5::NvOFGPUBufferHandle m_nvofBackwardCostHandle = nullptr;
        nvof5::NvOFGPUBufferHandle m_nvofGlobalFlowHandle = nullptr;
        bool m_nvofCostEnabled = false;
        bool m_nvofCostDisabledByRuntime = false;
        bool m_nvofGlobalFlowEnabled = false;
        bool m_nvofGlobalFlowDisabledByRuntime = false;
        UINT m_nvofWidth = 0;
        UINT m_nvofHeight = 0;
        UINT m_nvofInputWidth = 0;
        UINT m_nvofInputHeight = 0;
        UINT m_nvofFlowWidth = 0;
        UINT m_nvofFlowHeight = 0;
        UINT m_nvofGridSize = 0;
        size_t m_nvofPreviousIndex = 0;
        bool m_nvofPreviousValid = false;
        bool m_nvofFlowValid = false;
        bool m_nvofLastExecuteSuccessful = false;
        bool m_nvofUnavailable = false;
        winrt::com_ptr<ID3D11Buffer> m_constants;

        winrt::com_ptr<ID3D11Texture2D> m_analysisTex;
        winrt::com_ptr<ID3D11RenderTargetView> m_analysisRTV;
        winrt::com_ptr<ID3D11ShaderResourceView> m_analysisSRV;
        winrt::com_ptr<ID3D11Texture2D> m_instantPreviousAnalysis;
        winrt::com_ptr<ID3D11ShaderResourceView> m_instantPreviousAnalysisSRV;
        std::array<winrt::com_ptr<ID3D11Texture2D>, 2> m_instantSafetyTextures;
        std::array<winrt::com_ptr<ID3D11RenderTargetView>, 2> m_instantSafetyRTVs;
        std::array<winrt::com_ptr<ID3D11ShaderResourceView>, 2> m_instantSafetySRVs;
        std::array<winrt::com_ptr<ID3D11Texture2D>, 2> m_instantTemporalTextures;
        std::array<winrt::com_ptr<ID3D11RenderTargetView>, 2> m_instantTemporalRTVs;
        std::array<winrt::com_ptr<ID3D11ShaderResourceView>, 2> m_instantTemporalSRVs;
        size_t m_instantSafetyIndex = 0;
        bool m_instantHistoryValid = false;
        float m_instantFrameDt = 1.0f / 60.0f;
        std::array<AnalysisReadbackSlot, kAnalysisReadbackCount> m_analysisReadbacks;
        winrt::com_ptr<ID3D11Texture2D> m_localSafetyTexture;
        winrt::com_ptr<ID3D11ShaderResourceView> m_localSafetySRV;
        std::vector<float> m_prevAnalysis;
        std::vector<float> m_currentAnalysis;
        std::vector<float> m_prevRed;
        std::vector<float> m_currentRed;
        std::vector<float> m_prevU;
        std::vector<float> m_prevV;
        std::vector<float> m_currentU;
        std::vector<float> m_currentV;
        std::vector<uint8_t> m_prevSaturatedRed;
        std::vector<uint8_t> m_currentSaturatedRed;
        std::vector<float> m_redChangeScratch;
        std::vector<float> m_activationScratch;
        std::vector<uint8_t> m_changeMaskScratch;
        std::vector<uint8_t> m_componentVisited;
        std::vector<size_t> m_componentQueue;
        std::vector<uint32_t> m_integralScratch;
        std::vector<float> m_localSafetyData;
        std::vector<float> m_localDisplayedLuma;
        std::vector<float> m_previousOutputLuma;
        std::vector<float> m_localLumaGate;
        std::vector<float> m_localRedGate;
        bool m_havePrevAnalysis = false;
        bool m_haveLocalHistory = false;
        float m_previousGlobalLuma = 0.0f;

        std::vector<RawFrameSlot> m_rawFrames;
        size_t m_ringRead = 0;
        size_t m_ringWrite = 0;
        size_t m_bufferedFrameCount = 0;
        float m_bufferedDuration = 0.0f;
        uint64_t m_nextSequence = 0;
        winrt::com_ptr<ID3D11Texture2D> m_shieldTex;
        winrt::com_ptr<ID3D11ShaderResourceView> m_shieldSRV;
        UINT m_inputWidth{};
        UINT m_inputHeight{};
        DXGI_FORMAT m_inputFormat{ DXGI_FORMAT_UNKNOWN };

        HazardState m_hazardState = HazardState::Safe;
        AnalysisStats m_latestStats{};
        std::deque<FlashEvent> m_recentFlashes;
        float m_timelineSeconds = 0.0f;
        float m_lastHazardTime = -100.0f;
        float m_lastGlobalHazardTime = -100.0f;
        float m_releaseStartTime = 0.0f;
        float m_releaseStartStrength = 0.0f;
        float m_displayedGlobalLuma = 0.0f;
        float m_hardRiseLuma = 0.0f;
        bool m_haveHardRiseLuma = false;
        bool m_hardGlobalActive = false;
        float m_protectionStrength = 0.0f;
        float m_redProtectionStrength = 0.0f;
        bool m_globalProtectionActive = false;
        bool m_lastEventWasLocal = false;
        bool m_haveDisplayedLuma = false;
        int m_flashCountLastSecond = 0;
        int m_alternatingFlashCount = 0;
        int m_pendingTransitionDirection = 0;
        float m_pendingTransitionTime = -100.0f;
        uint64_t m_analysisDeadlineMisses = 0;
        std::atomic<uint64_t> m_droppedPresents{ 0 };
        int m_predictionFrames = 0;
        bool m_patternProtectionActive = false;
        HazardPrediction m_latestPrediction{};

        winrt::com_ptr<ID3D11Texture2D> m_debugTexture;
        winrt::com_ptr<ID3D11ShaderResourceView> m_debugSRV;
        int64_t m_lastDebugTextureMs = 0;
        winrt::com_ptr<ID3D11Texture2D> m_hintTexture;
        winrt::com_ptr<ID3D11ShaderResourceView> m_hintSRV;
        winrt::com_ptr<ID3D11Texture2D> m_automaticShieldLabelTexture;
        winrt::com_ptr<ID3D11ShaderResourceView> m_automaticShieldLabelSRV;
        int64_t m_hintUntilMs = 0;
        UINT m_outputWidth{};
        UINT m_outputHeight{};
    };

    FlashGuardApp* g_app = nullptr;
    HWND g_settingsWindow = nullptr;
    HWND g_gameWindow = nullptr;
    HWND g_overlayWindow = nullptr;
    bool g_hotkeysSuspended = false;
    bool g_settingsPreview = false;
    HFONT g_settingsFont = nullptr;
    HFONT g_settingsTitleFont = nullptr;
    HFONT g_settingsSectionFont = nullptr;
    HBRUSH g_settingsBrush = nullptr;
    HBITMAP g_settingsPanelBitmap = nullptr;
    HWND g_settingsTooltip = nullptr;
    UINT g_settingsDpi = 96;

    int SettingsScale(int value)
    {
        return MulDiv(value, static_cast<int>(g_settingsDpi), 96);
    }

    HBITMAP CreateFrostedPanelBitmap(HWND hwnd)
    {
        RECT client{};
        GetClientRect(hwnd, &client);
        const int width = client.right;
        const int height = client.bottom;
        if (width <= 0 || height <= 0) return nullptr;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* rawPixels = nullptr;
        HDC dc = GetDC(hwnd);
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS,
            &rawPixels, nullptr, 0);
        ReleaseDC(hwnd, dc);
        if (!bitmap || !rawPixels) return bitmap;

        auto* pixels = static_cast<std::uint8_t*>(rawPixels);
        for (int y = 0; y < height; ++y)
        {
            const float fy = static_cast<float>(y) / std::max(1, height - 1);
            for (int x = 0; x < width; ++x)
            {
                const float fx = static_cast<float>(x) / std::max(1, width - 1);
                const auto glow = [](float xDelta, float yDelta, float spread) {
                    return std::exp(-(xDelta * xDelta + yDelta * yDelta) / spread);
                };
                const float blue = glow(fx - 0.88f, fy - 0.12f, 0.16f);
                const float violet = glow(fx - 0.08f, fy - 0.78f, 0.22f);
                const float amber = glow(fx - 0.65f, fy - 0.55f, 0.10f);
                const float shade = 1.0f - 0.15f * fy;
                const int red = static_cast<int>((20.0f + 16.0f * violet +
                    9.0f * amber + 3.0f * blue) * shade);
                const int green = static_cast<int>((23.0f + 8.0f * violet +
                    7.0f * amber + 9.0f * blue) * shade);
                const int blueChannel = static_cast<int>((31.0f + 21.0f * blue +
                    13.0f * violet + 2.0f * amber) * shade);
                const size_t index = (static_cast<size_t>(y) * width + x) * 4;
                pixels[index + 0] = static_cast<std::uint8_t>(std::clamp(blueChannel, 0, 255));
                pixels[index + 1] = static_cast<std::uint8_t>(std::clamp(green, 0, 255));
                pixels[index + 2] = static_cast<std::uint8_t>(std::clamp(red, 0, 255));
                pixels[index + 3] = 255;
            }
        }

        HDC panelDc = CreateCompatibleDC(nullptr);
        HGDIOBJ oldBitmap = SelectObject(panelDc, bitmap);
        HBRUSH cardBrush = CreateSolidBrush(RGB(26, 29, 40));
        HPEN cardPen = CreatePen(PS_SOLID, 1, RGB(49, 56, 75));
        HGDIOBJ oldBrush = SelectObject(panelDc, cardBrush);
        HGDIOBJ oldPen = SelectObject(panelDc, cardPen);
        const auto card = [panelDc](int left, int top, int right, int bottom) {
            RoundRect(panelDc, SettingsScale(left), SettingsScale(top),
                SettingsScale(right), SettingsScale(bottom),
                SettingsScale(12), SettingsScale(12));
        };
        card(14, 68, 546, 386);
        card(14, 396, 546, 600);
        card(14, 610, 546, 710);
        HPEN separatorPen = CreatePen(PS_SOLID, 1, RGB(39, 45, 61));
        SelectObject(panelDc, separatorPen);
        const int mainSeparators[]{ 115, 153, 191, 229, 267, 305, 343 };
        for (int y : mainSeparators)
        {
            MoveToEx(panelDc, SettingsScale(28), SettingsScale(y), nullptr);
            LineTo(panelDc, SettingsScale(532), SettingsScale(y));
        }
        const int hotkeySeparators[]{ 466, 498, 530, 562 };
        for (int y : hotkeySeparators)
        {
            MoveToEx(panelDc, SettingsScale(28), SettingsScale(y), nullptr);
            LineTo(panelDc, SettingsScale(532), SettingsScale(y));
        }
        SelectObject(panelDc, oldPen);
        SelectObject(panelDc, oldBrush);
        SelectObject(panelDc, oldBitmap);
        DeleteObject(separatorPen);
        DeleteObject(cardPen);
        DeleteObject(cardBrush);
        DeleteDC(panelDc);
        return bitmap;
    }

    void EnableSettingsBackdrop(HWND hwnd)
    {
        const BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
        const int rounded = 2;
        DwmSetWindowAttribute(hwnd, 33, &rounded, sizeof(rounded));
        const int backdrop = 1;
        DwmSetWindowAttribute(hwnd, 38, &backdrop, sizeof(backdrop));
    }

    void UnregisterAllConfiguredHotkeys()
    {
        if (!IsWindow(g_overlayWindow)) return;
        for (int id = 1; id <= 5; ++id) UnregisterHotKey(g_overlayWindow, id);
    }

    void RegisterCurrentConfiguredHotkeys()
    {
        if (!IsWindow(g_overlayWindow)) return;
        for (size_t i = 0; i < g_hotkeys.size(); ++i)
        {
            if (g_hotkeys[i].virtualKey == 0) continue;
            RegisterHotKey(g_overlayWindow, static_cast<int>(i + 1),
                g_hotkeys[i].modifiers | MOD_NOREPEAT, g_hotkeys[i].virtualKey);
        }
    }

    bool InstallHotkeysTransactional(const std::array<HotkeyBinding, 5>& requested)
    {
        if (!IsWindow(g_overlayWindow)) return false;
        for (size_t i = 0; i < requested.size(); ++i)
        {
            for (size_t j = i + 1; j < requested.size(); ++j)
            {
                if (requested[i].virtualKey != 0 &&
                    requested[i].virtualKey == requested[j].virtualKey &&
                    requested[i].modifiers == requested[j].modifiers)
                    return false;
            }
        }

        const auto previous = g_hotkeys;
        UnregisterAllConfiguredHotkeys();
        bool installed = true;
        std::array<int, 5> installedIds{};
        int installedCount = 0;
        for (size_t i = 0; i < requested.size(); ++i)
        {
            if (requested[i].virtualKey == 0) continue;
            if (!RegisterHotKey(g_overlayWindow, static_cast<int>(i + 1),
                requested[i].modifiers | MOD_NOREPEAT, requested[i].virtualKey))
            {
                installed = false;
                break;
            }
            installedIds[installedCount++] = static_cast<int>(i + 1);
        }
        if (installed)
        {
            g_hotkeys = requested;
            if (g_hotkeysSuspended) UnregisterAllConfiguredHotkeys();
            return true;
        }

        for (int i = 0; i < installedCount; ++i)
            UnregisterHotKey(g_overlayWindow, installedIds[i]);
        if (!g_hotkeysSuspended)
        {
            for (size_t i = 0; i < previous.size(); ++i)
            {
                if (previous[i].virtualKey == 0) continue;
                RegisterHotKey(g_overlayWindow, static_cast<int>(i + 1),
                    previous[i].modifiers | MOD_NOREPEAT, previous[i].virtualKey);
            }
        }
        return false;
    }

    void SetControlFont(HWND control)
    {
        SendMessageW(control, WM_SETFONT,
            reinterpret_cast<WPARAM>(g_settingsFont ? g_settingsFont :
                GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    }

    void AddSettingsTooltip(HWND control, const wchar_t* text)
    {
        if (!IsWindow(g_settingsTooltip) || !IsWindow(control)) return;
        TOOLINFOW info{};
        info.cbSize = TTTOOLINFO_V1_SIZE;
        info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        info.hwnd = GetParent(control);
        info.uId = reinterpret_cast<UINT_PTR>(control);
        info.lpszText = const_cast<wchar_t*>(text);
        SendMessageW(g_settingsTooltip, TTM_ADDTOOLW, 0,
            reinterpret_cast<LPARAM>(&info));
    }

    LRESULT CALLBACK ComboSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                       LPARAM lParam, UINT_PTR, DWORD_PTR)
    {
        const LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
        if (msg == WM_PAINT || msg == WM_NCPAINT || msg == WM_SETFOCUS || msg == WM_KILLFOCUS)
        {
            HDC dc = GetWindowDC(hwnd);
            if (dc)
            {
                RECT rect{};
                GetWindowRect(hwnd, &rect);
                OffsetRect(&rect, -rect.left, -rect.top);
                HBRUSH border = CreateSolidBrush(GetFocus() == hwnd ?
                    RGB(79, 122, 184) : RGB(63, 70, 88));
                FrameRect(dc, &rect, border);
                InflateRect(&rect, -1, -1);
                FrameRect(dc, &rect, border);
                DeleteObject(border);
                ReleaseDC(hwnd, dc);
            }
        }
        if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, ComboSubclassProc, 2);
        return result;
    }

    HWND AddSettingsControl(HWND parent, DWORD exStyle, const wchar_t* cls,
                            const wchar_t* text, DWORD style,
                            int x, int y, int width, int height, int id)
    {
        HWND control = CreateWindowExW(exStyle, cls, text, style,
            SettingsScale(x), SettingsScale(y), SettingsScale(width), SettingsScale(height), parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
        if (control) SetControlFont(control);
        if (control) SetWindowTheme(control, L"DarkMode_Explorer", nullptr);
        if (control && lstrcmpW(cls, L"COMBOBOX") == 0)
            SetWindowSubclass(control, ComboSubclassProc, 2, 0);
        return control;
    }

    LRESULT CALLBACK HotkeySubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                        LPARAM lParam, UINT_PTR, DWORD_PTR)
    {
        if (msg == WM_PAINT)
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            RECT rect{};
            GetClientRect(hwnd, &rect);
            HBRUSH background = CreateSolidBrush(RGB(29, 33, 42));
            FillRect(dc, &rect, background);
            DeleteObject(background);
            HBRUSH border = CreateSolidBrush(GetFocus() == hwnd ?
                RGB(84, 151, 255) : RGB(79, 86, 101));
            FrameRect(dc, &rect, border);
            DeleteObject(border);
            const WORD value = static_cast<WORD>(SendMessageW(hwnd, HKM_GETHOTKEY, 0, 0));
            const std::wstring label = HotkeyName(FromHotkeyControlValue(value));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(235, 238, 246));
            HGDIOBJ oldFont = SelectObject(dc, g_settingsFont);
            rect.left += 8;
            DrawTextW(dc, label.c_str(), -1, &rect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(dc, oldFont);
            EndPaint(hwnd, &paint);
            return 0;
        }
        const LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
        if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS || msg == WM_KEYDOWN ||
            msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP)
            InvalidateRect(hwnd, nullptr, TRUE);
        if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, HotkeySubclassProc, 1);
        return result;
    }

    LRESULT CALLBACK ContrastSliderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                LPARAM lParam, UINT_PTR, DWORD_PTR)
    {
        if (msg == TBM_GETPOS)
            return static_cast<LRESULT>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == TBM_SETPOS)
        {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                std::clamp(static_cast<int>(lParam), 0, 1000));
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (msg == TBM_SETRANGE || msg == TBM_SETLINESIZE || msg == TBM_SETPAGESIZE)
            return 0;
        if (msg == WM_ERASEBKGND) return 1;
        if (msg == WM_PAINT)
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            RECT client{};
            GetClientRect(hwnd, &client);
            HBRUSH background = CreateSolidBrush(RGB(26, 29, 40));
            FillRect(dc, &client, background);
            DeleteObject(background);

            const int centerY = (client.top + client.bottom) / 2;
            const int margin = SettingsScale(9);
            RECT rail{ margin, centerY - SettingsScale(3),
                client.right - margin, centerY + SettingsScale(3) };
            HBRUSH railBrush = CreateSolidBrush(RGB(66, 73, 91));
            FillRect(dc, &rail, railBrush);
            DeleteObject(railBrush);

            const int position = std::clamp(static_cast<int>(SendMessageW(
                hwnd, TBM_GETPOS, 0, 0)), 0, 1000);
            const int thumbX = rail.left + MulDiv(
                rail.right - rail.left, position, 1000);
            RECT activeRail = rail;
            activeRail.right = thumbX;
            HBRUSH activeBrush = CreateSolidBrush(RGB(83, 145, 235));
            FillRect(dc, &activeRail, activeBrush);
            DeleteObject(activeBrush);

            const int radius = SettingsScale(7);
            HBRUSH thumbBrush = CreateSolidBrush(RGB(219, 228, 244));
            HPEN thumbPen = CreatePen(PS_SOLID, 1, RGB(105, 164, 247));
            HGDIOBJ oldBrush = SelectObject(dc, thumbBrush);
            HGDIOBJ oldPen = SelectObject(dc, thumbPen);
            Ellipse(dc, thumbX - radius, centerY - radius,
                thumbX + radius + 1, centerY + radius + 1);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(thumbPen);
            DeleteObject(thumbBrush);
            if (GetFocus() == hwnd)
            {
                RECT focus = client;
                InflateRect(&focus, -1, -1);
                DrawFocusRect(dc, &focus);
            }
            EndPaint(hwnd, &paint);
            return 0;
        }
        if (msg == WM_LBUTTONDOWN ||
            (msg == WM_MOUSEMOVE && (wParam & MK_LBUTTON) != 0))
        {
            if (msg == WM_LBUTTONDOWN)
            {
                SetFocus(hwnd);
                SetCapture(hwnd);
            }
            RECT client{};
            GetClientRect(hwnd, &client);
            const int margin = SettingsScale(9);
            const int width = std::max(1, static_cast<int>(client.right) - margin * 2);
            const int mouseX = static_cast<int>(static_cast<short>(LOWORD(lParam)));
            const int x = std::clamp(mouseX - margin, 0, width);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, MulDiv(x, 1000, width));
            InvalidateRect(hwnd, nullptr, FALSE);
            SendMessageW(GetParent(hwnd), WM_HSCROLL, TB_THUMBTRACK,
                reinterpret_cast<LPARAM>(hwnd));
            return 0;
        }
        if (msg == WM_LBUTTONUP)
        {
            if (GetCapture() == hwnd) ReleaseCapture();
            SendMessageW(GetParent(hwnd), WM_HSCROLL, TB_ENDTRACK,
                reinterpret_cast<LPARAM>(hwnd));
            return 0;
        }
        if (msg == WM_KEYDOWN &&
            (wParam == VK_LEFT || wParam == VK_DOWN ||
             wParam == VK_RIGHT || wParam == VK_UP ||
             wParam == VK_PRIOR || wParam == VK_NEXT ||
             wParam == VK_HOME || wParam == VK_END))
        {
            int position = static_cast<int>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (wParam == VK_LEFT || wParam == VK_DOWN) position -= 1;
            if (wParam == VK_RIGHT || wParam == VK_UP) position += 1;
            if (wParam == VK_PRIOR) position += 50;
            if (wParam == VK_NEXT) position -= 50;
            if (wParam == VK_HOME) position = 0;
            if (wParam == VK_END) position = 1000;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, std::clamp(position, 0, 1000));
            InvalidateRect(hwnd, nullptr, FALSE);
            SendMessageW(GetParent(hwnd), WM_HSCROLL, TB_THUMBTRACK,
                reinterpret_cast<LPARAM>(hwnd));
            return 0;
        }
        const LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
        if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS)
            InvalidateRect(hwnd, nullptr, FALSE);
        if (msg == WM_NCDESTROY)
            RemoveWindowSubclass(hwnd, ContrastSliderSubclassProc, 3);
        return result;
    }

    void PopulateSettingsWindow(HWND hwnd)
    {
        const RuntimeOptions options = g_app ? g_app->GetRuntimeOptions() : RuntimeOptions{};

        HWND profileLabel = AddSettingsControl(hwnd, 0, L"STATIC", L"Protection profile", WS_CHILD | WS_VISIBLE | SS_NOTIFY,
            28, 88, 200, 20, 0);
        HWND profile = AddSettingsControl(hwnd, 0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            245, 82, 285, 150, kControlProfile);
        const wchar_t* profileItems[] = {
            L"Performance - instant / original", L"Balanced - instant / low contrast",
            L"Maximum - instant / strongest"
        };
        for (const auto* item : profileItems)
            SendMessageW(profile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        SendMessageW(profile, CB_SETCURSEL, options.profilePreset, 0);
        AddSettingsTooltip(profile,
            L"All profiles use current-frame GPU protection. Performance changes the least; Balanced adds reduced contrast; Maximum uses the strongest protection curves. Click Apply to save.");
        AddSettingsTooltip(profileLabel,
            L"All profiles use current-frame GPU protection. Performance changes the least; Balanced adds reduced contrast; Maximum uses the strongest protection curves. Click Apply to save.");

        wchar_t contrastText[64]{};
        swprintf_s(contrastText, L"Contrast reduction: %.3f", options.contrastReduction);
        HWND contrastLabel = AddSettingsControl(hwnd, 0, L"STATIC", contrastText, WS_CHILD | WS_VISIBLE | SS_NOTIFY,
            28, 126, 205, 20, kControlContrastValue);
        HWND contrast = AddSettingsControl(hwnd, 0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | SS_NOTIFY,
            245, 116, 285, 32, kControlContrast);
        SetWindowSubclass(contrast, ContrastSliderSubclassProc, 3, 0);
        SendMessageW(contrast, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1000));
        SendMessageW(contrast, TBM_SETLINESIZE, 0, 1);
        SendMessageW(contrast, TBM_SETPAGESIZE, 0, 50);
        SendMessageW(contrast, TBM_SETPOS, TRUE, static_cast<LPARAM>(
            std::lround(options.contrastReduction * 1000.0f)));
        AddSettingsTooltip(contrast,
            L"Always-on contrast reduction. It lifts very dark tones and lowers bright highlights without blending frames or adding temporal delay.");
        AddSettingsTooltip(contrastLabel,
            L"Always-on contrast reduction. It lifts very dark tones and lowers bright highlights without blending frames or adding temporal delay.");

        HWND sensitivityLabel = AddSettingsControl(hwnd, 0, L"STATIC", L"Full-screen sensitivity", WS_CHILD | WS_VISIBLE | SS_NOTIFY,
            28, 164, 200, 20, 0);
        HWND sensitivity = AddSettingsControl(hwnd, 0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            245, 158, 285, 160, kControlSensitivity);
        const wchar_t* sensitivityItems[] = {
            L"Low false positives", L"Balanced", L"High small-flash sensitivity"
        };
        for (const auto* item : sensitivityItems)
            SendMessageW(sensitivity, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        SendMessageW(sensitivity, CB_SETCURSEL, options.fullScreenSensitivity, 0);
        AddSettingsTooltip(sensitivity,
            L"Controls detection of broad, coherent screen changes. Higher sensitivity catches weaker or smaller changes but can react more often to cuts and camera movement.");
        AddSettingsTooltip(sensitivityLabel,
            L"Controls detection of broad, coherent screen changes. Higher sensitivity catches weaker or smaller changes but can react more often to cuts and camera movement.");

        HWND smallSensitivityLabel = AddSettingsControl(hwnd, 0, L"STATIC", L"Small-source sensitivity", WS_CHILD | WS_VISIBLE | SS_NOTIFY,
            28, 202, 200, 20, 0);
        HWND smallSensitivity = AddSettingsControl(hwnd, 0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            245, 196, 285, 160, kControlSmallSensitivity);
        for (const auto* item : sensitivityItems)
            SendMessageW(smallSensitivity, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        SendMessageW(smallSensitivity, CB_SETCURSEL, options.smallSourceSensitivity, 0);
        AddSettingsTooltip(smallSensitivity,
            L"Controls protection for small intense flashing objects. Higher sensitivity protects smaller sources and expands their softened area, with a greater chance of affecting moving highlights.");
        AddSettingsTooltip(smallSensitivityLabel,
            L"Controls protection for small intense flashing objects. Higher sensitivity protects smaller sources and expands their softened area, with a greater chance of affecting moving highlights.");

        HWND latencyLabel = AddSettingsControl(hwnd, 0, L"STATIC", L"Protection timing", WS_CHILD | WS_VISIBLE | SS_NOTIFY,
            28, 240, 200, 20, 0);
        HWND latency = AddSettingsControl(hwnd, 0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            245, 234, 285, 170, kControlLatency);
        SendMessageW(latency, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(L"Instant GPU (required)"));
        SendMessageW(latency, CB_SETCURSEL, 0, 0);
        EnableWindow(latency, FALSE);
        AddSettingsTooltip(latency,
            L"Protection always uses the current GPU frame. CPU analysis remains advisory and never intentionally delays the displayed image.");
        AddSettingsTooltip(latencyLabel,
            L"Protection always uses the current GPU frame. CPU analysis remains advisory and never intentionally delays the displayed image.");

        HWND displayLabel = AddSettingsControl(hwnd, 0, L"STATIC", L"Display diagonal", WS_CHILD | WS_VISIBLE | SS_NOTIFY,
            28, 278, 200, 20, 0);
        HWND display = AddSettingsControl(hwnd, 0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
            245, 272, 285, 150, kControlDisplaySize);
        const wchar_t* displayItems[] = { L"24 inches", L"27 inches", L"32 inches", L"42 inches" };
        for (const auto* item : displayItems)
            SendMessageW(display, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        SendMessageW(display, CB_SETCURSEL, options.displaySizePreset, 0);
        AddSettingsTooltip(display,
            L"Approximate physical monitor diagonal. Used with viewing distance to estimate how much of your visual field a flashing region occupies.");
        AddSettingsTooltip(displayLabel,
            L"Approximate physical monitor diagonal. Used with viewing distance to estimate how much of your visual field a flashing region occupies.");

        HWND distanceLabel = AddSettingsControl(hwnd, 0, L"STATIC", L"Viewing distance", WS_CHILD | WS_VISIBLE | SS_NOTIFY,
            28, 316, 200, 20, 0);
        HWND distance = AddSettingsControl(hwnd, 0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
            245, 310, 285, 150, kControlViewingDistance);
        const wchar_t* distanceItems[] = { L"50 cm", L"70 cm", L"100 cm", L"140 cm" };
        for (const auto* item : distanceItems)
            SendMessageW(distance, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        SendMessageW(distance, CB_SETCURSEL, options.viewingDistancePreset, 0);
        AddSettingsTooltip(distance,
            L"Approximate distance from your eyes to the display. A closer screen makes a small flashing object occupy more of the visual field.");
        AddSettingsTooltip(distanceLabel,
            L"Approximate distance from your eyes to the display. A closer screen makes a small flashing object occupy more of the visual field.");

        HWND debug = AddSettingsControl(hwnd, 0, L"BUTTON", L"Show diagnostics overlay",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            28, 350, 300, 24, kControlDebug);
        SendMessageW(debug, BM_SETCHECK, options.debugOverlay ? BST_CHECKED : BST_UNCHECKED, 0);
        AddSettingsTooltip(debug,
            L"Shows live detector measurements and protection state in a steady diagnostic panel. Intended for testing and tuning rather than normal use.");

        AddSettingsControl(hwnd, 0, L"STATIC", L"Custom hotkeys", WS_CHILD | WS_VISIBLE,
            28, 408, 190, 20, 0);
        const wchar_t* hotkeyLabels[] = {
            L"Toggle neutral shield", L"Diagnostics overlay",
            L"Open options", L"Exit FlashGuard"
        };
        const int hotkeyIds[] = {
            kControlShieldOnHotkey, kControlDebugHotkey,
            kControlOptionsHotkey, kControlExitHotkey
        };
        const size_t hotkeyIndexes[] = { 0, 3, 4, 2 };
        const wchar_t* hotkeyHelp[] = {
            L"Toggles the persistent neutral shield on or off. A one-second cooldown prevents accidental double activation.",
            L"Shows or hides the live diagnostics overlay.",
            L"Opens this options window and releases any cursor confinement.",
            L"Closes FlashGuard. Clear the field if you do not want a global exit shortcut."
        };
        for (int i = 0; i < 4; ++i)
        {
            const int rowY = 438 + i * 32;
            HWND hotkeyLabel = AddSettingsControl(hwnd, 0, L"STATIC", hotkeyLabels[i], WS_CHILD | WS_VISIBLE | SS_NOTIFY,
                42, rowY + 3, 272, 20, 0);
            HWND hotkey = AddSettingsControl(hwnd, 0, HOTKEY_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                330, rowY, 200, 24, hotkeyIds[i]);
            SetWindowSubclass(hotkey, HotkeySubclassProc, 1, 0);
            SendMessageW(hotkey, HKM_SETHOTKEY,
                ToHotkeyControlValue(g_hotkeys[hotkeyIndexes[i]]), 0);
            AddSettingsTooltip(hotkey, hotkeyHelp[i]);
            AddSettingsTooltip(hotkeyLabel, hotkeyHelp[i]);
        }

        AddSettingsControl(hwnd, 0, L"STATIC",
            L"Keyboard: Tab moves between controls; arrows change a selection; Enter applies; Escape closes.",
            WS_CHILD | WS_VISIBLE,
            28, 622, 500, 40, 0);
        AddSettingsControl(hwnd, 0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            28, 674, 300, 22, kControlStatus);
        AddSettingsControl(hwnd, 0, L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE |
            WS_TABSTOP | BS_OWNERDRAW, 346, 670, 84, 30, kControlApply);
        AddSettingsControl(hwnd, 0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE |
            WS_TABSTOP | BS_OWNERDRAW, 446, 670, 84, 30, kControlClose);
        SetFocus(profile);
    }

    void ApplyProfilePresetToControls(HWND hwnd)
    {
        const int profile = std::clamp(static_cast<int>(SendDlgItemMessageW(
            hwnd, kControlProfile, CB_GETCURSEL, 0, 0)), 0, 2);
        if (profile == 0)
        {
            SendDlgItemMessageW(hwnd, kControlContrast, TBM_SETPOS, TRUE, 0);
            SendDlgItemMessageW(hwnd, kControlSensitivity, CB_SETCURSEL, 0, 0);
            SendDlgItemMessageW(hwnd, kControlSmallSensitivity, CB_SETCURSEL, 0, 0);
            SendDlgItemMessageW(hwnd, kControlLatency, CB_SETCURSEL, 0, 0);
        }
        else if (profile == 1)
        {
            SendDlgItemMessageW(hwnd, kControlContrast, TBM_SETPOS, TRUE, 667);
            SendDlgItemMessageW(hwnd, kControlSensitivity, CB_SETCURSEL, 1, 0);
            SendDlgItemMessageW(hwnd, kControlSmallSensitivity, CB_SETCURSEL, 1, 0);
            SendDlgItemMessageW(hwnd, kControlLatency, CB_SETCURSEL, 0, 0);
        }
        else
        {
            SendDlgItemMessageW(hwnd, kControlContrast, TBM_SETPOS, TRUE, 667);
            SendDlgItemMessageW(hwnd, kControlSensitivity, CB_SETCURSEL, 2, 0);
            SendDlgItemMessageW(hwnd, kControlSmallSensitivity, CB_SETCURSEL, 2, 0);
            SendDlgItemMessageW(hwnd, kControlLatency, CB_SETCURSEL, 0, 0);
        }
        SetDlgItemTextW(hwnd, kControlStatus,
            L"Preset loaded. Choose Apply to save it.");
        const float contrast = static_cast<float>(SendDlgItemMessageW(
            hwnd, kControlContrast, TBM_GETPOS, 0, 0)) / 1000.0f;
        wchar_t contrastText[64]{};
        swprintf_s(contrastText, L"Contrast reduction: %.3f", contrast);
        SetDlgItemTextW(hwnd, kControlContrastValue, contrastText);
    }

    void ApplySettingsWindow(HWND hwnd)
    {
        if (!g_app) return;
        RuntimeOptions options{};
        options.profilePreset = static_cast<int>(SendDlgItemMessageW(
            hwnd, kControlProfile, CB_GETCURSEL, 0, 0));
        options.contrastReduction = static_cast<float>(SendDlgItemMessageW(
            hwnd, kControlContrast, TBM_GETPOS, 0, 0)) / 1000.0f;
        options.fullScreenSensitivity = static_cast<int>(SendDlgItemMessageW(
            hwnd, kControlSensitivity, CB_GETCURSEL, 0, 0));
        options.smallSourceSensitivity = static_cast<int>(SendDlgItemMessageW(
            hwnd, kControlSmallSensitivity, CB_GETCURSEL, 0, 0));
        options.latencyMs = 0;
        options.displaySizePreset = static_cast<int>(SendDlgItemMessageW(
            hwnd, kControlDisplaySize, CB_GETCURSEL, 0, 0));
        options.viewingDistancePreset = static_cast<int>(SendDlgItemMessageW(
            hwnd, kControlViewingDistance, CB_GETCURSEL, 0, 0));
        options.debugOverlay = SendDlgItemMessageW(
            hwnd, kControlDebug, BM_GETCHECK, 0, 0) == BST_CHECKED;
        g_app->ApplyRuntimeOptions(options);

        auto requested = g_hotkeys;
        const auto readHotkey = [hwnd](int controlId) {
            return FromHotkeyControlValue(static_cast<WORD>(
                SendDlgItemMessageW(hwnd, controlId, HKM_GETHOTKEY, 0, 0)));
        };
        requested[0] = readHotkey(kControlShieldOnHotkey);
        requested[1] = HotkeyBinding{};
        requested[3] = readHotkey(kControlDebugHotkey);
        requested[4] = readHotkey(kControlOptionsHotkey);
        requested[2] = readHotkey(kControlExitHotkey);

        if (InstallHotkeysTransactional(requested))
        {
            SavePreferences(options, g_hotkeys);
            g_app->RefreshHotkeyHint();
            SetDlgItemTextW(hwnd, kControlStatus, L"Applied and saved.");
        }
        else
        {
            SavePreferences(options, g_hotkeys);
            SetDlgItemTextW(hwnd, kControlStatus,
                L"Settings saved; hotkey conflict restored.");
        }
    }

    LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_CREATE:
        {
            g_settingsDpi = GetDpiForWindow(hwnd);
            g_settingsBrush = CreateSolidBrush(RGB(27, 29, 36));
            g_settingsFont = CreateFontW(SettingsScale(-16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_settingsTitleFont = CreateFontW(SettingsScale(-27), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_settingsSectionFont = CreateFontW(SettingsScale(-15), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            EnableSettingsBackdrop(hwnd);
            g_settingsPanelBitmap = CreateFrostedPanelBitmap(hwnd);
            SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
            g_settingsTooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW,
                nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (g_settingsTooltip)
            {
                SendMessageW(g_settingsTooltip, TTM_ACTIVATE, TRUE, 0);
                SetWindowTheme(g_settingsTooltip, L"DarkMode_Explorer", nullptr);
                SendMessageW(g_settingsTooltip, TTM_SETMAXTIPWIDTH, 0, SettingsScale(380));
                SendMessageW(g_settingsTooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 450);
                SendMessageW(g_settingsTooltip, TTM_SETDELAYTIME, TTDT_RESHOW, 100);
                SendMessageW(g_settingsTooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 16000);
                SendMessageW(g_settingsTooltip, TTM_SETTIPBKCOLOR, RGB(30, 34, 46), 0);
                SendMessageW(g_settingsTooltip, TTM_SETTIPTEXTCOLOR, RGB(222, 227, 238), 0);
                RECT tooltipMargin{ SettingsScale(8), SettingsScale(5),
                    SettingsScale(8), SettingsScale(5) };
                SendMessageW(g_settingsTooltip, TTM_SETMARGIN, 0,
                    reinterpret_cast<LPARAM>(&tooltipMargin));
                SetWindowPos(g_settingsTooltip, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            HWND title = AddSettingsControl(hwnd, 0, L"STATIC", L"FlashGuard",
                WS_CHILD | WS_VISIBLE, 24, 14, 300, 36, 0);
            if (title) SendMessageW(title, WM_SETFONT,
                reinterpret_cast<WPARAM>(g_settingsTitleFont), TRUE);
            HWND subtitle = AddSettingsControl(hwnd, 0, L"STATIC",
                L"GPU protection and display preferences",
                WS_CHILD | WS_VISIBLE, 26, 50, 430, 22, 0);
            if (subtitle) SendMessageW(subtitle, WM_SETFONT,
                reinterpret_cast<WPARAM>(g_settingsSectionFont), TRUE);
            PopulateSettingsWindow(hwnd);
            return 0;
        }
        case WM_ERASEBKGND:
        {
            RECT client{};
            GetClientRect(hwnd, &client);
            HDC dc = reinterpret_cast<HDC>(wParam);
            if (g_settingsPanelBitmap)
            {
                HDC memory = CreateCompatibleDC(dc);
                HGDIOBJ old = SelectObject(memory, g_settingsPanelBitmap);
                BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
                SelectObject(memory, old);
                DeleteDC(memory);
            }
            else
            {
                FillRect(dc, &client, g_settingsBrush);
            }
            return 1;
        }
        case WM_CTLCOLORSTATIC:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, RGB(207, 213, 226));
            if (GetDlgCtrlID(reinterpret_cast<HWND>(lParam)) == kControlContrastValue)
            {
                SetBkMode(dc, OPAQUE);
                SetBkColor(dc, RGB(26, 29, 40));
                return reinterpret_cast<LRESULT>(g_settingsBrush);
            }
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetStockObject(HOLLOW_BRUSH));
        }
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, RGB(220, 225, 236));
            SetBkColor(dc, RGB(27, 29, 36));
            return reinterpret_cast<LRESULT>(g_settingsBrush);
        }
        case WM_DRAWITEM:
        {
            const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (!item || (item->CtlID != kControlApply && item->CtlID != kControlClose))
                return DefWindowProcW(hwnd, msg, wParam, lParam);
            const bool apply = item->CtlID == kControlApply;
            const bool pressed = (item->itemState & ODS_SELECTED) != 0;
            const COLORREF color = apply ?
                (pressed ? RGB(45, 104, 195) : RGB(61, 126, 224)) :
                (pressed ? RGB(47, 51, 61) : RGB(60, 65, 78));
            HBRUSH brush = CreateSolidBrush(color);
            HPEN pen = CreatePen(PS_SOLID, 1, apply ? RGB(110, 166, 255) : RGB(92, 99, 116));
            HGDIOBJ oldBrush = SelectObject(item->hDC, brush);
            HGDIOBJ oldPen = SelectObject(item->hDC, pen);
            RoundRect(item->hDC, item->rcItem.left, item->rcItem.top,
                item->rcItem.right, item->rcItem.bottom, 8, 8);
            SelectObject(item->hDC, oldPen);
            SelectObject(item->hDC, oldBrush);
            DeleteObject(pen);
            DeleteObject(brush);
            SetBkMode(item->hDC, TRANSPARENT);
            SetTextColor(item->hDC, RGB(248, 249, 252));
            HGDIOBJ oldFont = SelectObject(item->hDC, g_settingsFont);
            const wchar_t* label = apply ? L"Apply" : L"Close";
            RECT textRect = item->rcItem;
            DrawTextW(item->hDC, label, -1, &textRect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(item->hDC, oldFont);
            if (item->itemState & ODS_FOCUS)
            {
                RECT focus = item->rcItem;
                InflateRect(&focus, -3, -3);
                DrawFocusRect(item->hDC, &focus);
            }
            return TRUE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == kControlProfile && HIWORD(wParam) == CBN_SELCHANGE)
            {
                ApplyProfilePresetToControls(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == kControlApply && HIWORD(wParam) == BN_CLICKED)
            {
                ApplySettingsWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == kControlClose && HIWORD(wParam) == BN_CLICKED)
            {
                DestroyWindow(hwnd);
                return 0;
            }
            return 0;
        case WM_HSCROLL:
            if (reinterpret_cast<HWND>(lParam) == GetDlgItem(hwnd, kControlContrast))
            {
                const float value = static_cast<float>(SendDlgItemMessageW(
                    hwnd, kControlContrast, TBM_GETPOS, 0, 0)) / 1000.0f;
                wchar_t contrastText[64]{};
                swprintf_s(contrastText, L"Contrast reduction: %.3f", value);
                SetDlgItemTextW(hwnd, kControlContrastValue, contrastText);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_settingsWindow = nullptr;
            if (g_settingsFont) DeleteObject(g_settingsFont);
            if (g_settingsTitleFont) DeleteObject(g_settingsTitleFont);
            if (g_settingsSectionFont) DeleteObject(g_settingsSectionFont);
            if (g_settingsBrush) DeleteObject(g_settingsBrush);
            if (g_settingsPanelBitmap) DeleteObject(g_settingsPanelBitmap);
            g_settingsFont = nullptr;
            g_settingsTitleFont = nullptr;
            g_settingsSectionFont = nullptr;
            g_settingsBrush = nullptr;
            g_settingsPanelBitmap = nullptr;
            g_settingsTooltip = nullptr;
            if (g_hotkeysSuspended)
            {
                g_hotkeysSuspended = false;
                RegisterCurrentConfiguredHotkeys();
            }
            if (IsWindow(g_gameWindow)) SetForegroundWindow(g_gameWindow);
            if (g_settingsPreview) PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }

    void ShowSettingsWindow(HWND overlay)
    {
        if (IsWindow(g_settingsWindow))
        {
            ShowWindow(g_settingsWindow, SW_RESTORE);
            SetForegroundWindow(g_settingsWindow);
            return;
        }

        HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = SettingsWndProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kSettingsWindowClass;
        if (!GetClassInfoExW(instance, kSettingsWindowClass, &wc))
        {
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = SettingsWndProc;
            wc.hInstance = instance;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            wc.lpszClassName = kSettingsWindowClass;
            if (!RegisterClassExW(&wc)) return;
        }

        g_settingsDpi = overlay ? GetDpiForWindow(overlay) : GetDpiForSystem();
        RECT windowRect{ 0, 0, SettingsScale(560), SettingsScale(720) };
        const DWORD settingsExStyle = g_settingsPreview ?
            (WS_EX_APPWINDOW | WS_EX_CONTROLPARENT) :
            (WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT);
        AdjustWindowRectExForDpi(&windowRect, WS_CAPTION | WS_SYSMENU,
            FALSE, settingsExStyle, g_settingsDpi);
        const int width = windowRect.right - windowRect.left;
        const int height = windowRect.bottom - windowRect.top;
        HMONITOR monitor = MonitorFromWindow(overlay, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{ sizeof(mi) };
        GetMonitorInfoW(monitor, &mi);
        const int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - width) / 2;
        const int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - height) / 2;

        ClipCursor(nullptr);
        g_settingsWindow = CreateWindowExW(
            settingsExStyle,
            kSettingsWindowClass, L"FlashGuard Options",
            WS_CAPTION | WS_SYSMENU,
            x, y, width, height, nullptr, nullptr, instance, nullptr);
        if (!g_settingsWindow) return;
        UnregisterAllConfiguredHotkeys();
        g_hotkeysSuspended = true;
        if (!g_settingsPreview)
            SetWindowDisplayAffinity(g_settingsWindow, WDA_EXCLUDEFROMCAPTURE);
        ShowWindow(g_settingsWindow, SW_SHOWNORMAL);
        SetForegroundWindow(g_settingsWindow);
        SetActiveWindow(g_settingsWindow);
    }

    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            if (g_app && wParam != SIZE_MINIMIZED)
            {
                try { g_app->ResizeOutput(); } catch (...) {}
            }
            return 0;
        case WM_TIMER:
            if (wParam == kWatchdogTimer && g_app) g_app->Watchdog();
            return 0;
        case WM_HOTKEY:
            if (!g_app) return 0;
            if (wParam == 1)
            {
                // Some keyboard layers deliver a registration with extra or
                // missing modifiers. Require the physical modifier state to
                // match the configured toggle and reject rapid duplicates.
                UINT activeModifiers = 0;
                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) activeModifiers |= MOD_SHIFT;
                if (GetAsyncKeyState(VK_CONTROL) & 0x8000) activeModifiers |= MOD_CONTROL;
                if (GetAsyncKeyState(VK_MENU) & 0x8000) activeModifiers |= MOD_ALT;
                if ((GetAsyncKeyState(VK_LWIN) & 0x8000) ||
                    (GetAsyncKeyState(VK_RWIN) & 0x8000)) activeModifiers |= MOD_WIN;
                const ULONGLONG now = GetTickCount64();
                ULONGLONG previous = g_lastShieldToggleMs.load(std::memory_order_acquire);
                if (activeModifiers == g_hotkeys[0].modifiers &&
                    now - previous >= kShieldToggleCooldownMs &&
                    g_lastShieldToggleMs.compare_exchange_strong(
                        previous, now, std::memory_order_acq_rel))
                    g_app->ToggleEmergencyShield();
            }
            if (wParam == 3) DestroyWindow(hwnd);
            if (wParam == 4) g_app->ToggleDebugOverlay();
            if (wParam == 5) ShowSettingsWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (IsWindow(g_settingsWindow)) DestroyWindow(g_settingsWindow);
            UnregisterHotKey(hwnd, 1);
            UnregisterHotKey(hwnd, 2);
            UnregisterHotKey(hwnd, 3);
            UnregisterHotKey(hwnd, 4);
            UnregisterHotKey(hwnd, 5);
            KillTimer(hwnd, kWatchdogTimer);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }

    std::wstring ParseArgumentValue(const wchar_t* flag)
    {
        std::wstring result;
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv) return result;
        for (int i = 1; i + 1 < argc; ++i)
        {
            if (_wcsicmp(argv[i], flag) == 0)
            {
                result = argv[i + 1];
                break;
            }
        }
        LocalFree(argv);
        return result;
    }

    std::wstring ParseTitleArgument()
    {
        std::wstring result;
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv) return result;
        for (int i = 1; i < argc; ++i)
        {
            if (std::wstring(argv[i]) == L"--title" && i + 1 < argc)
                result = argv[++i];
        }
        LocalFree(argv);
        return result;
    }

    HWND CreateOutputWindow(HINSTANCE instance, HMONITOR mon)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wc.lpszClassName = kWindowClass;
        RegisterClassExW(&wc);

        MONITORINFO mi{ sizeof(mi) };
        if (!GetMonitorInfoW(mon, &mi)) return nullptr;

        HWND hwnd = CreateWindowExW(
            // WS_EX_TRANSPARENT alone mainly affects paint ordering. Combining it
            // with WS_EX_LAYERED makes this top-level overlay genuinely mouse
            // pass-through to windows in other processes.
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED,
            kWindowClass,
            L"FlashGuard Protection Overlay",
            WS_POPUP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            nullptr, nullptr, instance, nullptr);
        if (!hwnd) return nullptr;

        // Keep the layered overlay visually opaque while retaining click-through
        // semantics from WS_EX_LAYERED | WS_EX_TRANSPARENT.
        if (!SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA))
        {
            DestroyWindow(hwnd);
            return nullptr;
        }

        // Critical for display capture: the guard window must not become part of
        // its own Desktop Duplication input. Abort if Windows cannot exclude it.
        if (!SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE))
        {
            DestroyWindow(hwnd);
            return nullptr;
        }

        return hwnd;
    }

    HWND CreateReplayWindow(HINSTANCE instance, HMONITOR mon,
                            int replayWidth = 640, int replayHeight = 360)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wc.lpszClassName = kWindowClass;
        RegisterClassExW(&wc);

        MONITORINFO mi{ sizeof(mi) };
        if (!GetMonitorInfoW(mon, &mi)) return nullptr;
        replayWidth = std::clamp(replayWidth, 160, 1920);
        replayHeight = std::clamp(replayHeight, 90, 1080);
        return CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClass,
            L"FlashGuard Synthetic Replay", WS_POPUP,
            mi.rcMonitor.left, mi.rcMonitor.top, replayWidth, replayHeight,
            nullptr, nullptr, instance, nullptr);
    }

    HWND CreateStartupStatusWindow(HINSTANCE instance, HMONITOR mon,
                                   HWND& statusText, HWND& progress)
    {
        statusText = nullptr;
        progress = nullptr;

        MONITORINFO mi{ sizeof(mi) };
        if (!GetMonitorInfoW(mon, &mi)) return nullptr;

        constexpr int width = 440;
        constexpr int height = 94;
        const int x = mi.rcWork.left +
            (mi.rcWork.right - mi.rcWork.left - width) / 2;
        const int y = mi.rcWork.top +
            (mi.rcWork.bottom - mi.rcWork.top - height) / 2;
        HWND hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            L"STATIC", L"",
            WS_POPUP | WS_BORDER | WS_CLIPCHILDREN,
            x, y, width, height,
            nullptr, nullptr, instance, nullptr);
        if (!hwnd) return nullptr;

        // The complete startup UI is one top-level capture-excluded window.
        // Child controls share that visual ownership, avoiding a second
        // WDA_EXCLUDEFROMCAPTURE failure point for the progress bar.
        if (!SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE))
        {
            DestroyWindow(hwnd);
            return nullptr;
        }

        statusText = CreateWindowExW(
            0, L"STATIC",
            L"FlashGuard is starting...\nPreparing GPU capture.",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            12, 8, width - 24, 50,
            hwnd, nullptr, instance, nullptr);
        progress = CreateWindowExW(
            0, PROGRESS_CLASSW, nullptr,
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            18, 65, width - 36, 16,
            hwnd, nullptr, instance, nullptr);
        if (!statusText || !progress)
        {
            DestroyWindow(hwnd);
            statusText = nullptr;
            progress = nullptr;
            return nullptr;
        }

        SendMessageW(statusText, WM_SETFONT,
            reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(progress, PBM_SETRANGE32, 0, 100);
        SendMessageW(progress, PBM_SETPOS, 2, 0);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        UpdateWindow(hwnd);
        UpdateWindow(statusText);
        UpdateWindow(progress);
        return hwnd;
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    HWND startupWindow = nullptr;
    HWND startupStatus = nullptr;
    HWND startupProgress = nullptr;
    try
    {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_WIN95_CLASSES };
        InitCommonControlsEx(&controls);
        g_liveDisableNvofForLatencyTest =
            HasCommandLineFlag(L"--latency-disable-nvof");
        g_liveRawPassthroughForLatencyTest =
            HasCommandLineFlag(L"--latency-raw-passthrough");
        if (HasCommandLineFlag(L"--validate-shaders"))
            return ValidateShaderSource() ? 0 : 2;
        if (HasCommandLineFlag(L"--validate-risk-integrator"))
            return ValidateRiskIntegrator() ? 0 : 9;

        const std::wstring replayReport = ParseArgumentValue(L"--synthetic-replay");
        const std::wstring replayVisualDir =
            ParseArgumentValue(L"--synthetic-replay-visual");
        const std::wstring replayFpsArg = ParseArgumentValue(L"--replay-fps");
        const std::wstring replayMotionScaleArg =
            ParseArgumentValue(L"--replay-motion-scale");
        const std::wstring replayProfileArg =
            ParseArgumentValue(L"--replay-profile");
        const std::wstring replayFullSensitivityArg =
            ParseArgumentValue(L"--replay-full-sensitivity");
        const std::wstring replaySmallSensitivityArg =
            ParseArgumentValue(L"--replay-small-sensitivity");
        const std::wstring replayBatchPlan =
            ParseArgumentValue(L"--synthetic-replay-batch");
        const std::wstring replayBatchOutput =
            ParseArgumentValue(L"--synthetic-replay-batch-output");
        const std::wstring replayWidthArg = ParseArgumentValue(L"--replay-width");
        const std::wstring replayHeightArg = ParseArgumentValue(L"--replay-height");
        const bool replayScreening = HasCommandLineFlag(L"--replay-screening");
        g_replayDisableNvofTemporalHints =
            HasCommandLineFlag(L"--replay-disable-nvof-temporal-hints");
        const int replayWidth = replayWidthArg.empty() ? (replayScreening ? 320 : 640) :
            std::clamp(_wtoi(replayWidthArg.c_str()), 160, 1920);
        const int replayHeight = replayHeightArg.empty() ? (replayScreening ? 180 : 360) :
            std::clamp(_wtoi(replayHeightArg.c_str()), 90, 1080);
        if (!replayBatchPlan.empty() && !replayBatchOutput.empty())
        {
            POINT origin{ 0, 0 };
            HMONITOR monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
            HWND replayWindow = CreateReplayWindow(
                instance, monitor, replayWidth, replayHeight);
            if (!replayWindow) return 6;
            try
            {
                FILE* plan = nullptr;
                if (_wfopen_s(&plan, replayBatchPlan.c_str(), L"rt, ccs=UTF-8") != 0 || !plan)
                {
                    DestroyWindow(replayWindow);
                    return 10;
                }
                struct BatchResult
                {
                    std::string name;
                    bool passed = false;
                    long long elapsedMs = 0;
                };
                // Six columns remains backward compatible. Matrix v4 appends
                // twelve outer detector/temporal settings; matrix v5 appends 24
                // full-resolution shader transport/hold settings after those.
                // Matrix v7 adds architecture mode; v8 adds risk neutral/gain.
                std::vector<std::array<std::wstring, 45>> specs;
                wchar_t line[4096]{};
                while (std::fgetws(line, static_cast<int>(std::size(line)), plan))
                {
                    std::wstring text(line);
                    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n'))
                        text.pop_back();
                    if (!text.empty() && text.front() == 0xFEFF) text.erase(text.begin());
                    if (text.empty() || text.front() == L'#') continue;
                    std::vector<std::wstring> fields;
                    size_t start = 0;
                    for (;;)
                    {
                        const size_t tab = text.find(L'\t', start);
                        fields.push_back(text.substr(start,
                            tab == std::wstring::npos ? std::wstring::npos : tab - start));
                        if (tab == std::wstring::npos) break;
                        start = tab + 1;
                    }
                    if (fields.size() != 6 && fields.size() != 18 &&
                        fields.size() != 42 && fields.size() != 43 &&
                        fields.size() != 45)
                    {
                        std::fclose(plan);
                        DestroyWindow(replayWindow);
                        return 10;
                    }
                    std::array<std::wstring, 45> spec{};
                    for (size_t i = 0; i < fields.size(); ++i) spec[i] = fields[i];
                    const bool safeName = !spec[0].empty() && spec[0].size() <= 80 &&
                        std::all_of(spec[0].begin(), spec[0].end(), [](wchar_t c) {
                            return (c >= L'a' && c <= L'z') ||
                                (c >= L'A' && c <= L'Z') ||
                                (c >= L'0' && c <= L'9') || c == L'_' || c == L'-';
                        });
                    if (!safeName)
                    {
                        std::fclose(plan);
                        DestroyWindow(replayWindow);
                        return 10;
                    }
                    specs.push_back(std::move(spec));
                }
                std::fclose(plan);
                if (specs.empty())
                {
                    DestroyWindow(replayWindow);
                    return 10;
                }

                std::filesystem::create_directories(replayBatchOutput);
                FlashGuardApp app;
                RuntimeOptions replayOptions{};
                replayOptions.contrastReduction = 0.0f;
                replayOptions.latencyMs = 0;
                replayOptions.debugOverlay = false;
                app.ApplyRuntimeOptions(replayOptions);
                app.InitializeReplay(replayWindow, monitor);
                std::vector<BatchResult> results;
                bool infrastructureOk = true;
                for (const auto& spec : specs)
                {
                    replayOptions.profilePreset = std::clamp(_wtoi(spec[1].c_str()), 0, 2);
                    replayOptions.fullScreenSensitivity = std::clamp(_wtoi(spec[2].c_str()), 0, 2);
                    replayOptions.smallSourceSensitivity = std::clamp(_wtoi(spec[3].c_str()), 0, 2);
                    const int fps = std::clamp(_wtoi(spec[4].c_str()), 30, 240);
                    const float scale = std::clamp(
                        static_cast<float>(_wtof(spec[5].c_str())), 0.25f, 4.0f);
                    BenchmarkTuning tuning{};
                    const auto tune = [&](size_t index, float& target) {
                        if (!spec[index].empty())
                        {
                            target = static_cast<float>(_wtof(spec[index].c_str()));
                            tuning.enabled = true;
                        }
                    };
                    tune(6, tuning.localDeltaThreshold);
                    tune(7, tuning.globalDeltaThreshold);
                    tune(8, tuning.affectedAreaThreshold);
                    tune(9, tuning.coherenceThreshold);
                    tune(10, tuning.smallFlashAreaThreshold);
                    tune(11, tuning.localGlobalSupportThreshold);
                    tune(12, tuning.flashEnergyThreshold);
                    tune(13, tuning.safeRiseRate);
                    tune(14, tuning.safeFallRate);
                    tune(15, tuning.minimumProtectionTime);
                    tune(16, tuning.releaseTime);
                    tune(17, tuning.cameraMotionSuppression);
                    tune(18, tuning.shader.eventDeltaLow);
                    tune(19, tuning.shader.eventDeltaHigh);
                    tune(20, tuning.shader.holdDeltaLow);
                    tune(21, tuning.shader.holdDeltaHigh);
                    tune(22, tuning.shader.stableSourceLow);
                    tune(23, tuning.shader.stableSourceHigh);
                    tune(24, tuning.shader.intrinsicResidualLow);
                    tune(25, tuning.shader.intrinsicResidualHigh);
                    tune(26, tuning.shader.repeatedMemoryLow);
                    tune(27, tuning.shader.repeatedMemoryHigh);
                    tune(28, tuning.shader.holdGateLow);
                    tune(29, tuning.shader.holdGateHigh);
                    tune(30, tuning.shader.transportConfidenceLow);
                    tune(31, tuning.shader.transportConfidenceHigh);
                    tune(32, tuning.shader.disocclusionResetGate);
                    tune(33, tuning.shader.surfaceRiskTau);
                    tune(34, tuning.shader.eventStateTauScale);
                    tune(35, tuning.shader.releaseStateTauScale);
                    tune(36, tuning.shader.exactHoldThreshold);
                    tune(37, tuning.shader.movingHoldFloorMax);
                    tune(38, tuning.shader.directIntrinsicDisplayLow);
                    tune(39, tuning.shader.directIntrinsicDisplayHigh);
                    tune(40, tuning.shader.eventSeedLow);
                    tune(41, tuning.shader.eventSeedHigh);
                    if (!spec[42].empty())
                    {
                        const int requestedArchitecture =
                            _wtoi(spec[42].c_str());
                        tuning.architectureMode =
                            std::clamp(requestedArchitecture, 0, 24);
                        if (tuning.architectureMode != requestedArchitecture)
                        {
                            DestroyWindow(replayWindow);
                            return 10;
                        }
                        tuning.enabled = true;
                    }
                    tune(43, tuning.riskOnlyNeutralLuma);
                    tune(44, tuning.riskOnlyGain);
                    app.ApplyRuntimeOptions(replayOptions);
                    app.ApplyBenchmarkTuning(tuning);
                    const auto caseDir = std::filesystem::path(replayBatchOutput) / spec[0];
                    std::filesystem::create_directories(caseDir);
                    const auto caseReport = caseDir / L"synthetic-replay.json";
                    const auto started = std::chrono::steady_clock::now();
                    const bool passed = app.RunSyntheticReplay(
                        caseReport.wstring(), L"", fps, scale, replayScreening);
                    const long long elapsedMs = std::chrono::duration_cast<
                        std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
                    infrastructureOk = infrastructureOk && std::filesystem::exists(caseReport);
                    std::string name;
                    name.reserve(spec[0].size());
                    for (wchar_t c : spec[0]) name.push_back(static_cast<char>(c));
                    results.push_back({ name, passed, elapsedMs });
                }

                const auto batchReportPath =
                    std::filesystem::path(replayBatchOutput) / L"batch.json";
                FILE* batchReport = nullptr;
                if (_wfopen_s(&batchReport, batchReportPath.c_str(), L"wb") != 0 ||
                    !batchReport)
                    infrastructureOk = false;
                else
                {
                    std::fprintf(batchReport,
                        "{\n  \"schema\": \"FLASHGUARD_REPLAY_BATCH/1\",\n"
                        "  \"status\": \"%s\",\n  \"screening\": %s,\n"
                        "  \"width\": %d,\n  \"height\": %d,\n  \"cases\": [\n",
                        infrastructureOk ? "SUCCESS" : "FAILED",
                        replayScreening ? "true" : "false", replayWidth, replayHeight);
                    for (size_t i = 0; i < results.size(); ++i)
                        std::fprintf(batchReport,
                            "    {\"name\":\"%s\",\"behavior_pass\":%s,"
                            "\"elapsed_ms\":%lld}%s\n",
                            results[i].name.c_str(), results[i].passed ? "true" : "false",
                            results[i].elapsedMs, i + 1 < results.size() ? "," : "");
                    std::fputs("  ]\n}\n", batchReport);
                    std::fclose(batchReport);
                }
                app.Stop();
                DestroyWindow(replayWindow);
                return infrastructureOk ? 0 : 10;
            }
            catch (...)
            {
                DestroyWindow(replayWindow);
                std::fprintf(stderr, "synthetic replay batch exception\n");
                return 10;
            }
        }
        if (!replayReport.empty())
        {
            const int replayFps = replayFpsArg.empty() ? 60 :
                std::clamp(_wtoi(replayFpsArg.c_str()), 30, 240);
            const float replayMotionScale = replayMotionScaleArg.empty() ? 1.0f :
                std::clamp(static_cast<float>(_wtof(replayMotionScaleArg.c_str())),
                    0.25f, 4.0f);
            POINT origin{ 0, 0 };
            HMONITOR monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
            HWND replayWindow = CreateReplayWindow(instance, monitor);
            if (!replayWindow) return 6;
            try
            {
                FlashGuardApp app;
                RuntimeOptions replayOptions{};
                replayOptions.profilePreset = replayProfileArg.empty() ? 1 :
                    std::clamp(_wtoi(replayProfileArg.c_str()), 0, 2);
                replayOptions.fullScreenSensitivity =
                    replayFullSensitivityArg.empty() ? 1 :
                    std::clamp(_wtoi(replayFullSensitivityArg.c_str()), 0, 2);
                replayOptions.smallSourceSensitivity =
                    replaySmallSensitivityArg.empty() ? 1 :
                    std::clamp(_wtoi(replaySmallSensitivityArg.c_str()), 0, 2);
                replayOptions.contrastReduction = 0.0f;
                replayOptions.latencyMs = 0;
                replayOptions.debugOverlay = false;
                app.ApplyRuntimeOptions(replayOptions);
                app.InitializeReplay(replayWindow, monitor);
                const bool passed = app.RunSyntheticReplay(
                    replayReport, replayVisualDir, replayFps, replayMotionScale);
                app.Stop();
                DestroyWindow(replayWindow);
                return passed ? 0 : 7;
            }
            catch (HRESULT hr)
            {
                std::fprintf(stderr,
                    "synthetic replay HRESULT 0x%08X\n",
                    static_cast<unsigned>(hr));
                DestroyWindow(replayWindow);
                return 8;
            }
            catch (const std::exception& error)
            {
                std::fprintf(stderr, "synthetic replay exception: %s\n", error.what());
                DestroyWindow(replayWindow);
                return 8;
            }
            catch (...)
            {
                std::fprintf(stderr, "synthetic replay exception: unknown\n");
                DestroyWindow(replayWindow);
                return 8;
            }
        }

        HANDLE mutexHandle = CreateMutexW(nullptr, FALSE,
            L"Local\\OutlastFlashGuard.SingleInstance");
        if (!mutexHandle) winrt::throw_last_error();
        const DWORD mutexStatus = GetLastError();
        winrt::handle instanceMutex{ mutexHandle };
        if (mutexStatus == ERROR_ALREADY_EXISTS)
        {
            MessageBoxW(nullptr, L"FlashGuard is already running.", L"FlashGuard",
                MB_ICONINFORMATION | MB_OK);
            return 0;
        }
        if (HasCommandLineFlag(L"--settings-preview"))
        {
            g_settingsPreview = true;
            ShowSettingsWindow(nullptr);
            MSG previewMessage{};
            while (GetMessageW(&previewMessage, nullptr, 0, 0) > 0)
            {
                if (IsWindow(g_settingsWindow) &&
                    IsDialogMessageW(g_settingsWindow, &previewMessage))
                    continue;
                TranslateMessage(&previewMessage);
                DispatchMessageW(&previewMessage);
            }
            return 0;
        }
        RuntimeOptions savedOptions{};
        LoadPreferences(savedOptions, g_hotkeys);
        const std::wstring title = ParseTitleArgument();
        const bool windowBoundMode = !title.empty();
        HWND target = windowBoundMode ? FindWindowBySubstring(title) : GetForegroundWindow();

        if (windowBoundMode && !target)
        {
            std::wstring msg = L"Could not find a visible, non-minimized window containing:\n\n" + title +
                L"\n\nOpen that application in Borderless/Windowed mode, then run FlashGuard again.";
            MessageBoxW(nullptr, msg.c_str(), L"FlashGuard", MB_ICONWARNING | MB_OK);
            return 3;
        }
        g_gameWindow = IsWindow(target) ? target : nullptr;

        HMONITOR monitor = nullptr;
        if (windowBoundMode)
        {
            monitor = MonitorFromWindow(target, MONITOR_DEFAULTTONEAREST);
        }
        else
        {
            // General-purpose mode protects the complete monitor under the
            // pointer and is independent of any particular app or game.
            POINT cursor{};
            GetCursorPos(&cursor);
            monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
        }

        // Give immediate feedback before synchronous D3D/shader/capture setup.
        // The same capture-excluded parent owns both status text and progress.
        startupWindow = CreateStartupStatusWindow(
            instance, monitor, startupStatus, startupProgress);

        HWND output = CreateOutputWindow(instance, monitor);
        if (!output)
        {
            if (startupWindow) DestroyWindow(startupWindow);
            startupWindow = startupStatus = startupProgress = nullptr;
            MessageBoxW(nullptr,
                L"FlashGuard could not create a capture-excluded overlay.\n\n"
                L"For safety, display-capture mode will not run unless Windows accepts WDA_EXCLUDEFROMCAPTURE, because otherwise the overlay could capture itself.",
                L"FlashGuard", MB_ICONERROR | MB_OK);
            return 4;
        }
        g_overlayWindow = output;

        FlashGuardApp app;
        g_app = &app;
        app.ApplyRuntimeOptions(savedOptions);
        app.Initialize(output, monitor, startupStatus, startupProgress);

        // Desktop Duplication pre-rolls while the overlay is still hidden. It
        // duplicates the selected monitor rather than capturing one app window.
        const ULONGLONG prerollStart = GetTickCount64();
        while (!app.ReadyToShow() && GetTickCount64() - prerollStart < kStartupCaptureTimeoutMs)
        {
            if (windowBoundMode && !IsWindow(target)) break;
            Sleep(5);
        }

        if (!app.ReadyToShow())
        {
            if (startupWindow) DestroyWindow(startupWindow);
            startupWindow = startupStatus = startupProgress = nullptr;
            app.Stop();
            g_app = nullptr;
            DestroyWindow(output);
            MessageBoxW(nullptr,
                L"FlashGuard did not receive enough desktop frames during startup.\n\n"
                L"The fullscreen overlay was NOT enabled. Make sure the selected monitor is active and not locked.",
                L"FlashGuard - capture not ready", MB_ICONWARNING | MB_OK);
            return 5;
        }

        if (startupProgress)
        {
            SendMessageW(startupProgress, PBM_SETPOS, 100, 0);
            UpdateWindow(startupProgress);
        }
        if (startupStatus)
        {
            SetWindowTextW(startupStatus,
                g_liveRawPassthroughForLatencyTest ?
                    L"RAW PASSTHROUGH - NO PROTECTION\nReady." :
                    L"FlashGuard is ready.");
            UpdateWindow(startupStatus);
        }
        if (startupWindow) DestroyWindow(startupWindow);
        startupWindow = startupStatus = startupProgress = nullptr;
        ShowWindow(output, SW_SHOWNOACTIVATE);
        SetWindowPos(output, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);

        RegisterCurrentConfiguredHotkeys();
        SetTimer(output, kWatchdogTimer, kWatchdogPeriodMs, nullptr);

        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0)
        {
            if (IsWindow(g_settingsWindow))
            {
                if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
                {
                    DestroyWindow(g_settingsWindow);
                    continue;
                }
                if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN)
                {
                    if (GetDlgCtrlID(msg.hwnd) == kControlClose)
                        DestroyWindow(g_settingsWindow);
                    else
                        ApplySettingsWindow(g_settingsWindow);
                    continue;
                }
                if (IsDialogMessageW(g_settingsWindow, &msg))
                    continue;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        app.Stop();
        g_app = nullptr;
        return static_cast<int>(msg.wParam);
    }
    catch (HRESULT hr)
    {
        if (startupWindow) DestroyWindow(startupWindow);
        startupWindow = startupStatus = startupProgress = nullptr;
        wchar_t buf[256]{};
        swprintf_s(buf, L"FlashGuard failed with HRESULT 0x%08X.\n\nThe filter was not started. Do not assume the screen is protected.", static_cast<unsigned>(hr));
        MessageBoxW(nullptr, buf, L"FlashGuard", MB_ICONERROR | MB_OK);
        return static_cast<int>(hr);
    }
    catch (...)
    {
        if (startupWindow) DestroyWindow(startupWindow);
        startupWindow = startupStatus = startupProgress = nullptr;
        MessageBoxW(nullptr,
            L"FlashGuard failed unexpectedly. The display filter was not started. Do not assume the screen is protected.",
            L"FlashGuard", MB_ICONERROR | MB_OK);
        return 1;
    }
}
