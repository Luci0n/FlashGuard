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
#include <d3dcompiler.h>
#include <winrt/base.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <deque>
#include <filesystem>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>


// Minimal NVIDIA Optical Flow SDK 5.0 ABI declarations used by FlashGuard.
// The runtime is loaded dynamically from the NVIDIA display driver, so this
// remains a single-file build with no NVIDIA import library or CUDA dependency.
namespace nvof5
{
#ifdef _WIN32
#define FG_NVOFAPI __stdcall
#else
#define FG_NVOFAPI
#endif
    constexpr uint32_t kApiVersion = (5u << 4);

    struct NvOFHandle_st;
    struct NvOFGPUBufferHandle_st;
    struct NvOFPrivDataHandle_st;
    using NvOFHandle = NvOFHandle_st*;
    using NvOFGPUBufferHandle = NvOFGPUBufferHandle_st*;
    using NvOFPrivDataHandle = NvOFPrivDataHandle_st*;

    enum NV_OF_STATUS
    {
        NV_OF_SUCCESS,
        NV_OF_ERR_OF_NOT_AVAILABLE,
        NV_OF_ERR_UNSUPPORTED_DEVICE,
        NV_OF_ERR_DEVICE_DOES_NOT_EXIST,
        NV_OF_ERR_INVALID_PTR,
        NV_OF_ERR_INVALID_PARAM,
        NV_OF_ERR_INVALID_CALL,
        NV_OF_ERR_INVALID_VERSION,
        NV_OF_ERR_OUT_OF_MEMORY,
        NV_OF_ERR_NOT_INITIALIZED,
        NV_OF_ERR_UNSUPPORTED_FEATURE,
        NV_OF_ERR_GENERIC
    };

    enum NV_OF_BOOL { NV_OF_FALSE = 0, NV_OF_TRUE = 1 };
    enum NV_OF_CAPS
    {
        NV_OF_CAPS_SUPPORTED_OUTPUT_GRID_SIZES,
        NV_OF_CAPS_SUPPORTED_HINT_GRID_SIZES,
        NV_OF_CAPS_SUPPORT_HINT_WITH_OF_MODE,
        NV_OF_CAPS_SUPPORT_HINT_WITH_ST_MODE,
        NV_OF_CAPS_WIDTH_MIN,
        NV_OF_CAPS_HEIGHT_MIN,
        NV_OF_CAPS_WIDTH_MAX,
        NV_OF_CAPS_HEIGHT_MAX,
        NV_OF_CAPS_SUPPORT_ROI,
        NV_OF_CAPS_SUPPORT_ROI_MAX_NUM,
        NV_OF_CAPS_SUPPORT_STEREO,
        NV_OF_CAPS_SUPPORT_MAX
    };
    enum NV_OF_PERF_LEVEL
    {
        NV_OF_PERF_LEVEL_UNDEFINED,
        NV_OF_PERF_LEVEL_SLOW = 5,
        NV_OF_PERF_LEVEL_MEDIUM = 10,
        NV_OF_PERF_LEVEL_FAST = 20,
        NV_OF_PERF_LEVEL_MAX
    };
    enum NV_OF_OUTPUT_VECTOR_GRID_SIZE
    {
        NV_OF_OUTPUT_VECTOR_GRID_SIZE_UNDEFINED,
        NV_OF_OUTPUT_VECTOR_GRID_SIZE_1 = 1,
        NV_OF_OUTPUT_VECTOR_GRID_SIZE_2 = 2,
        NV_OF_OUTPUT_VECTOR_GRID_SIZE_4 = 4,
        NV_OF_OUTPUT_VECTOR_GRID_SIZE_MAX
    };
    enum NV_OF_HINT_VECTOR_GRID_SIZE
    {
        NV_OF_HINT_VECTOR_GRID_SIZE_UNDEFINED,
        NV_OF_HINT_VECTOR_GRID_SIZE_1 = 1,
        NV_OF_HINT_VECTOR_GRID_SIZE_2 = 2,
        NV_OF_HINT_VECTOR_GRID_SIZE_4 = 4,
        NV_OF_HINT_VECTOR_GRID_SIZE_8 = 8,
        NV_OF_HINT_VECTOR_GRID_SIZE_MAX
    };
    enum NV_OF_MODE
    {
        NV_OF_MODE_UNDEFINED,
        NV_OF_MODE_OPTICALFLOW,
        NV_OF_MODE_STEREODISPARITY,
        NV_OF_MODE_MAX
    };
    enum NV_OF_BUFFER_USAGE
    {
        NV_OF_BUFFER_USAGE_UNDEFINED,
        NV_OF_BUFFER_USAGE_INPUT,
        NV_OF_BUFFER_USAGE_OUTPUT,
        NV_OF_BUFFER_USAGE_HINT,
        NV_OF_BUFFER_USAGE_COST,
        NV_OF_BUFFER_USAGE_GLOBAL_FLOW,
        NV_OF_BUFFER_USAGE_MAX
    };
    enum NV_OF_BUFFER_FORMAT
    {
        NV_OF_BUFFER_FORMAT_UNDEFINED,
        NV_OF_BUFFER_FORMAT_GRAYSCALE8,
        NV_OF_BUFFER_FORMAT_NV12,
        NV_OF_BUFFER_FORMAT_ABGR8,
        NV_OF_BUFFER_FORMAT_SHORT,
        NV_OF_BUFFER_FORMAT_SHORT2,
        NV_OF_BUFFER_FORMAT_UINT,
        NV_OF_BUFFER_FORMAT_UINT8,
        NV_OF_BUFFER_FORMAT_MAX
    };
    enum NV_OF_STEREO_DISPARITY_RANGE
    {
        NV_OF_STEREO_DISPARITY_RANGE_UNDEFINED,
        NV_OF_STEREO_DISPARITY_RANGE_128 = 128,
        NV_OF_STEREO_DISPARITY_RANGE_256 = 256,
        NV_OF_STEREO_DISPARITY_RANGE_MAX
    };
    enum NV_OF_PRED_DIRECTION
    {
        NV_OF_PRED_DIRECTION_FORWARD = 0,
        NV_OF_PRED_DIRECTION_BOTH = 2,
        NV_OF_PRED_DIRECTION_MAX
    };

    struct NV_OF_FLOW_VECTOR { int16_t flowx; int16_t flowy; };
    struct NV_OF_ROI_RECT
    {
        uint32_t start_x, start_y, width, height;
    };
    struct NV_OF_INIT_PARAMS
    {
        uint32_t width;
        uint32_t height;
        NV_OF_OUTPUT_VECTOR_GRID_SIZE outGridSize;
        NV_OF_HINT_VECTOR_GRID_SIZE hintGridSize;
        NV_OF_MODE mode;
        NV_OF_PERF_LEVEL perfLevel;
        NV_OF_BOOL enableExternalHints;
        NV_OF_BOOL enableOutputCost;
        NvOFPrivDataHandle hPrivData;
        NV_OF_STEREO_DISPARITY_RANGE disparityRange;
        NV_OF_BOOL enableRoi;
        NV_OF_PRED_DIRECTION predDirection;
        NV_OF_BOOL enableGlobalFlow;
        NV_OF_BUFFER_FORMAT inputBufferFormat;
    };
    struct NV_OF_EXECUTE_INPUT_PARAMS
    {
        NvOFGPUBufferHandle inputFrame;
        NvOFGPUBufferHandle referenceFrame;
        NvOFGPUBufferHandle externalHints;
        NV_OF_BOOL disableTemporalHints;
        uint32_t padding;
        NvOFPrivDataHandle hPrivData;
        uint32_t padding2;
        uint32_t numRois;
        NV_OF_ROI_RECT* roiData;
    };
    struct NV_OF_EXECUTE_OUTPUT_PARAMS
    {
        NvOFGPUBufferHandle outputBuffer;
        NvOFGPUBufferHandle outputCostBuffer;
        NvOFPrivDataHandle hPrivData;
        NvOFGPUBufferHandle bwdOutputBuffer;
        NvOFGPUBufferHandle bwdOutputCostBuffer;
        NvOFGPUBufferHandle globalFlowBuffer;
    };

    using PFNNVCREATEOPTICALFLOWD3D11 = NV_OF_STATUS(FG_NVOFAPI*)(
        ID3D11Device* const, ID3D11DeviceContext* const, NvOFHandle*);
    using PFNNVOFINIT = NV_OF_STATUS(FG_NVOFAPI*)(NvOFHandle, const NV_OF_INIT_PARAMS*);
    using PFNNVOFGETSURFACEFORMATCOUNTD3D11 = NV_OF_STATUS(FG_NVOFAPI*)(
        NvOFHandle, const NV_OF_BUFFER_USAGE, const NV_OF_MODE, uint32_t* const);
    using PFNNVOFGETSURFACEFORMATD3D11 = NV_OF_STATUS(FG_NVOFAPI*)(
        NvOFHandle, const NV_OF_BUFFER_USAGE, const NV_OF_MODE, DXGI_FORMAT* const);
    using PFNNVOFREGISTERRESOURCED3D11 = NV_OF_STATUS(FG_NVOFAPI*)(
        NvOFHandle, ID3D11Resource*, NvOFGPUBufferHandle* const);
    using PFNNVOFUNREGISTERRESOURCED3D11 = NV_OF_STATUS(FG_NVOFAPI*)(NvOFGPUBufferHandle);
    using PFNNVOFEXECUTE = NV_OF_STATUS(FG_NVOFAPI*)(
        NvOFHandle, const NV_OF_EXECUTE_INPUT_PARAMS*, NV_OF_EXECUTE_OUTPUT_PARAMS*);
    using PFNNVOFDESTROY = NV_OF_STATUS(FG_NVOFAPI*)(NvOFHandle);
    using PFNNVOFGETLASTERROR = NV_OF_STATUS(FG_NVOFAPI*)(NvOFHandle, char[], uint32_t*);
    using PFNNVOFGETCAPS = NV_OF_STATUS(FG_NVOFAPI*)(NvOFHandle, NV_OF_CAPS, uint32_t*, uint32_t*);

    struct NV_OF_D3D11_API_FUNCTION_LIST
    {
        PFNNVCREATEOPTICALFLOWD3D11 nvCreateOpticalFlowD3D11;
        PFNNVOFINIT nvOFInit;
        PFNNVOFGETSURFACEFORMATCOUNTD3D11 nvOFGetSurfaceFormatCountD3D11;
        PFNNVOFGETSURFACEFORMATD3D11 nvOFGetSurfaceFormatD3D11;
        PFNNVOFREGISTERRESOURCED3D11 nvOFRegisterResourceD3D11;
        PFNNVOFUNREGISTERRESOURCED3D11 nvOFUnregisterResourceD3D11;
        PFNNVOFEXECUTE nvOFExecute;
        PFNNVOFDESTROY nvOFDestroy;
        PFNNVOFGETLASTERROR nvOFGetLastError;
        PFNNVOFGETCAPS nvOFGetCaps;
    };
    using PFNNVOFAPICREATEINSTANCED3D11 = NV_OF_STATUS(FG_NVOFAPI*)(
        uint32_t, NV_OF_D3D11_API_FUNCTION_LIST*);
#undef FG_NVOFAPI
}

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
    const int2 p0 = clamp(int2(floor(p)), int2(0, 0), int2(flowWidth - 1, flowHeight - 1));
    const int2 p1 = min(p0 + int2(1, 1), int2(flowWidth - 1, flowHeight - 1));
    const float2 f = frac(p);
    const float2 a = (float2)flowTexture.Load(int3(p0.x, p0.y, 0)) / 32.0;
    const float2 b = (float2)flowTexture.Load(int3(p1.x, p0.y, 0)) / 32.0;
    const float2 c = (float2)flowTexture.Load(int3(p0.x, p1.y, 0)) / 32.0;
    const float2 d = (float2)flowTexture.Load(int3(p1.x, p1.y, 0)) / 32.0;
    const float2 flowInputToOutputScale = max(P9.yz, float2(1.0, 1.0));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y) * flowInputToOutputScale;
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
};

MainOutput PSMain(VSOut i)
{
    MainOutput output;
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
    const float redMemoryGate = smoothstep(0.10, 0.50, coarseRisk);
    const float redMitigationGate = max(redEventGate, redMemoryGate);
    cur = lerp(cur, gray.xxx, isolatedRed * redDesat * redMitigationGate);

    // Preserve the RAW source before temporal feedback. This history is used for
    // local motion transport and release discrimination. Keeping it raw means a
    // protected/tonemapped previous output cannot masquerade as object motion.
    const float3 sourceHistoryColor = rawSourceColor;)HLSL" R"HLSL(

    // TRUE OUTPUT-SPACE TEMPORAL FILTER.
    // The old versions generated a new luminance target from the source but did
    // not constrain the actual displayed pixel against the previous displayed
    // pixel. That lets repetitive flashes leak through even while "protected".
    // PreviousOutput is a full-resolution ping-pong luminance history written by
    // this same shader on the preceding present.
    float candidateL = Luma(cur);
    if (P7.y > 0.5)
    {
        // First compare raw source at the same screen coordinate. Bright moving
        // objects create a huge same-coordinate delta even though their appearance
        // is unchanged; using that delta directly is what produced v7's trails.
        float sourceDelta = 0.0;
        float localMotionGate = 0.0;
        // P8.x encodes NVOFA state: 0=fallback/unavailable, 0.5=anchor-only, 1=fresh flow.
        // A skipped execute still keeps the immediate previous frame as the next anchor,
        // but must NOT trigger the expensive portable matcher.
        const bool hardwareFlowAvailable = P8.x > 0.25;
        const bool hardwareFlowValid = P8.x > 0.75 && P7.z > 0.5;
        if (P7.z > 0.5)
        {
            const float3 previousSourceSame = PreviousSource.SampleLevel(
                LinearClamp, i.uv, 0.0).rgb;
            sourceDelta = SourceMatchError(rawSourceColor, previousSourceSame);

            // NVIDIA Optical Flow SDK path.
            //
            // There are TWO motion cases that matter for ghosting:
            //   1) current-surface transport: where a moving object is NOW;
            //   2) vacated/disoccluded pixels: where that object USED TO BE.
            //
            // v9 only handled (1). A bright object therefore left its filtered
            // luminance behind on trailing edges even with valid grid-2 flow.
            const float localHazardNeed = max(max(coarseEvent, coarseRisk),
                max(max(protectionGate, overloadGate), P6.x > 0.5 ? 1.0 : 0.0));
            const bool coarseMotionAlreadyExplains = coarseMotion >= 0.58;
            if (hardwareFlowValid && sourceDelta > 0.004 &&
                localHazardNeed > 0.018 && !coarseMotionAlreadyExplains)
            {
                const float2 outputSize = max(float2(P2.z, P2.w), float2(1.0, 1.0));
                const float2 outputTexel = 1.0 / outputSize;

                // --- A. Current surface -> its previous position -----------------
                const float2 forwardPixels = LoadOpticalFlow(ForwardOpticalFlow, i.uv);
                const float2 previousUv = i.uv + forwardPixels / outputSize;
                const float flowMagnitude = length(forwardPixels);
                const bool insidePrevious = all(previousUv >= float2(0.0, 0.0)) &&
                    all(previousUv <= float2(1.0, 1.0));

                if (insidePrevious && flowMagnitude > 0.10)
                {
                    const float2 backwardPixels = LoadOpticalFlow(
                        BackwardOpticalFlow, previousUv);
                    const float roundTripError = length(forwardPixels + backwardPixels);

                    // Verify transport with a small CROSS patch, not one pixel.
                    // This makes a good flow match decisive while rejecting a flash
                    // that merely happens to produce a plausible vector.
                    float samePatchError = 0.0;
                    float warpedPatchError = 0.0;
                    [unroll]
                    for (int py = -1; py <= 1; ++py)
                    {
                        [unroll]
                        for (int px = -1; px <= 1; ++px)
                        {
                            if (abs(px) + abs(py) <= 1)
                            {
                                const float2 patchOffset =
                                    float2((float)px, (float)py) * outputTexel * 2.0;
                                const float2 currentPatchUv = i.uv + patchOffset;
                                const float3 currentPatch = CurrentFrame.SampleLevel(
                                    LinearClamp, currentPatchUv, 0.0).rgb;
                                const float3 previousSamePatch = PreviousSource.SampleLevel(
                                    LinearClamp, currentPatchUv, 0.0).rgb;

                                const float2 patchForward = LoadOpticalFlow(
                                    ForwardOpticalFlow, currentPatchUv);
                                const float2 patchPreviousUv =
                                    currentPatchUv + patchForward / outputSize;
                                const float3 previousWarpedPatch = PreviousSource.SampleLevel(
                                    LinearClamp, patchPreviousUv, 0.0).rgb;

                                samePatchError += SourceMatchError(
                                    currentPatch, previousSamePatch);
                                warpedPatchError += SourceMatchError(
                                    currentPatch, previousWarpedPatch);
                            }
                        }
                    }
                    samePatchError /= 5.0;
                    warpedPatchError /= 5.0;

                    const float patchImprovement = samePatchError > 0.003 ?
                        saturate(1.0 - warpedPatchError / samePatchError) : 0.0;
                    const float absoluteMatch =
                        1.0 - smoothstep(0.030, 0.125, warpedPatchError);
                    const float allowedRoundTrip =
                        max(1.25, 0.65 + flowMagnitude * 0.22);
                    const float fbConfidence =
                        1.0 - smoothstep(allowedRoundTrip,
                                       allowedRoundTrip + 2.0,
                                       roundTripError);

                    float costConfidence = 1.0;
                    if (P9.w > 0.5)
                    {
                        const float flowCost = LoadOpticalCost(
                            ForwardOpticalCost, i.uv);
                        costConfidence = 1.0 - smoothstep(0.28, 0.78, flowCost);
                    }
                    const float transportEvidence =
                        fbConfidence * absoluteMatch *
                        smoothstep(0.10, 0.34, patchImprovement) *
                        lerp(0.35, 1.0, costConfidence);

                    // Once the warped raw patch genuinely explains the new image,
                    // make motion nearly binary. Partial gates are precisely what
                    // left 30-70% of old history visible in v9.
                    const float transportGate =
                        transportEvidence > 0.34 ? 1.0 :
                        smoothstep(0.18, 0.34, transportEvidence);

                    if (transportGate > localMotionGate)
                    {
                        // Flow is classification evidence only. Never spatially warp
                        // displayed history: one bad vector should not bend geometry.
                        localMotionGate = transportGate;
                    }
                }

                // --- B. Previous surface moved AWAY from this screen pixel -------
                // At a trailing edge there is no valid current->previous
                // correspondence for the newly exposed background. Use backward
                // flow from the PREVIOUS pixel instead: if that old pixel is found
                // at its new current destination, this location was vacated by
                // motion and old filtered history must be dropped immediately.
                const float2 backwardFromHere =
                    LoadOpticalFlow(BackwardOpticalFlow, i.uv);
                const float vacatedMagnitude = length(backwardFromHere);
                const float2 movedToUv = i.uv + backwardFromHere / outputSize;
                const bool insideMovedTo = all(movedToUv >= float2(0.0, 0.0)) &&
                    all(movedToUv <= float2(1.0, 1.0));

                if (insideMovedTo && vacatedMagnitude > 0.10)
                {
                    const float2 forwardAtDestination =
                        LoadOpticalFlow(ForwardOpticalFlow, movedToUv);
                    const float vacatedRoundTrip =
                        length(backwardFromHere + forwardAtDestination);

                    const float3 previousHere = PreviousSource.SampleLevel(
                        LinearClamp, i.uv, 0.0).rgb;
                    const float3 currentAtDestination = CurrentFrame.SampleLevel(
                        LinearClamp, movedToUv, 0.0).rgb;

                    const float movedObjectError =
                        SourceMatchError(previousHere, currentAtDestination);
                    const float stayedHereError =
                        SourceMatchError(previousHere, rawSourceColor);
                    const float movedImprovement = stayedHereError > 0.003 ?
                        saturate(1.0 - movedObjectError / stayedHereError) : 0.0;

                    const float vacatedAllowedRoundTrip =
                        max(1.25, 0.65 + vacatedMagnitude * 0.22);
                    const float vacatedFb =
                        1.0 - smoothstep(vacatedAllowedRoundTrip,
                                       vacatedAllowedRoundTrip + 2.0,
                                       vacatedRoundTrip);
                    const float vacatedMatch =
                        1.0 - smoothstep(0.030, 0.125, movedObjectError);
                    float vacatedCostConfidence = 1.0;
                    if (P9.w > 0.5)
                    {
                        const float flowCost = LoadOpticalCost(
                            BackwardOpticalCost, i.uv);
                        vacatedCostConfidence = 1.0 - smoothstep(0.28, 0.78, flowCost);
                    }
                    const float vacatedEvidence =
                        vacatedFb * vacatedMatch *
                        smoothstep(0.12, 0.38, movedImprovement) *
                        lerp(0.35, 1.0, vacatedCostConfidence);

                    const float vacatedGate =
                        vacatedEvidence > 0.34 ? 1.0 :
                        smoothstep(0.18, 0.34, vacatedEvidence);

                    // Do NOT transport history for a vacated pixel: the previous
                    // history here belongs to the object that left. We only need the
                    // motion bypass so the freshly revealed background appears now.
                    localMotionGate = max(localMotionGate, vacatedGate);
                }
            }

            // Fresh NVOFA gets first chance, but bright/flat surfaces can leave
            // weak edge/disocclusion evidence. Let the raw-image matcher verify
            // low-confidence pixels instead of accepting a partial motion gate.
            if ((!hardwareFlowValid || localMotionGate < 0.80) &&
                P6.y < max(0.10, P5.x * 0.75) &&
                sourceDelta > 0.010 && coarseMotion < 0.30 &&
                max(coarseEvent, coarseRisk) > 0.010)
            {
                const float2 outputTexel = 1.0 / max(
                    float2(P2.z, P2.w), float2(1.0, 1.0));

                // First determine the most plausible CARDINAL transport direction
                // from inexpensive center-pixel matches. Cardinal preference avoids
                // the ambiguity of a flat bright object matching many diagonal)HLSL" R"HLSL(
                // interior pixels.
                float bestCardinalError = sourceDelta;
                float2 bestCardinalDir = float2(0.0, 0.0);
                [unroll]
                for (int ri = 0; ri < 5; ++ri)
                {
                    const float radius = exp2((float)ri); // 1,2,4,8,16 pixels
                    [unroll]
                    for (int my = -1; my <= 1; ++my)
                    {
                        [unroll]
                        for (int mx = -1; mx <= 1; ++mx)
                        {
                            if (abs(mx) + abs(my) == 1)
                            {
                                const float2 dir = float2((float)mx, (float)my);
                                const float2 offset = dir * outputTexel * radius;
                                const float3 previousMoved = PreviousSource.SampleLevel(
                                    LinearClamp, i.uv + offset, 0.0).rgb;
                                const float error = SourceMatchError(
                                    rawSourceColor, previousMoved);
                                if (error < bestCardinalError)
                                {
                                    bestCardinalError = error;
                                    bestCardinalDir = dir;
                                }
                            }
                        }
                    }
                }

                // Current/source patch error at the unshifted coordinate. The
                // verification patch is center + four cardinal neighbors, 2 px
                // apart, so a moving edge/shape has spatial structure to align.
                float samePatchError = 0.0;
                const float verifyRadius = 2.0;
                [unroll]
                for (int py = -1; py <= 1; ++py)
                {
                    [unroll]
                    for (int px = -1; px <= 1; ++px)
                    {
                        if (abs(px) + abs(py) <= 1)
                        {
                            const float2 patchOffset = float2((float)px, (float)py) *
                                outputTexel * verifyRadius;
                            const float3 currentPatch = CurrentFrame.SampleLevel(
                                LinearClamp, i.uv + patchOffset, 0.0).rgb;
                            const float3 previousSamePatch = PreviousSource.SampleLevel(
                                LinearClamp, i.uv + patchOffset, 0.0).rgb;
                            samePatchError += SourceMatchError(
                                currentPatch, previousSamePatch);
                        }
                    }
                }

                // Once a direction is known, choose its radius by PATCH error, not
                // center color. This is what lets a translating bright rectangle
                // resolve to its true previous edge instead of an arbitrary bright
                // interior sample.
                if (dot(bestCardinalDir, bestCardinalDir) > 0.0 &&
                    bestCardinalError < sourceDelta * 0.80 && samePatchError > 0.012)
                {
                    float bestPatchError = samePatchError;
                    float2 bestPatchOffset = float2(0.0, 0.0);
                    [unroll]
                    for (int ri = 0; ri < 5; ++ri)
                    {
                        const float radius = exp2((float)ri);
                        const float2 offset = bestCardinalDir * outputTexel * radius;
                        float shiftedPatchError = 0.0;
                        [unroll]
                        for (int py = -1; py <= 1; ++py)
                        {
                            [unroll]
                            for (int px = -1; px <= 1; ++px)
                            {
                                if (abs(px) + abs(py) <= 1)
                                {
                                    const float2 patchOffset = float2((float)px, (float)py) *
                                        outputTexel * verifyRadius;
                                    const float3 currentPatch = CurrentFrame.SampleLevel(
                                        LinearClamp, i.uv + patchOffset, 0.0).rgb;
                                    const float3 previousShiftPatch = PreviousSource.SampleLevel(
                                        LinearClamp, i.uv + patchOffset + offset, 0.0).rgb;
                                    shiftedPatchError += SourceMatchError(
                                        currentPatch, previousShiftPatch);
                                }
                            }
                        }
                        if (shiftedPatchError < bestPatchError)
                        {
                            bestPatchError = shiftedPatchError;
                            bestPatchOffset = offset;
                        }
                    }

                    const float patchImprovement = saturate(
                        1.0 - bestPatchError / samePatchError);
                    const float absoluteMatch = 1.0 - smoothstep(0.040, 0.145,
                        bestPatchError / 5.0);
                    const float portableGate =
                        smoothstep(0.32, 0.68, patchImprovement) * absoluteMatch;
                    localMotionGate = max(localMotionGate,
                        portableGate > 0.60 ? 1.0 : portableGate);
                }

                // Diagonal fallback only when cardinal transport was inconclusive.
                // It is intentionally conditional so ordinary gameplay does not
                // pay for two full local searches on every changing pixel.
                if (localMotionGate < 0.45 && samePatchError > 0.012)
                {
                    float bestDiagonalError = sourceDelta;
                    float2 bestDiagonalDir = float2(0.0, 0.0);
                    [unroll]
                    for (int ri = 1; ri < 5; ++ri) // 2,4,8,16 pixels
                    {
                        const float radius = exp2((float)ri);
                        [unroll]
                        for (int my = -1; my <= 1; my += 2)
                        {
                            [unroll]
                            for (int mx = -1; mx <= 1; mx += 2)
                            {
                                const float2 dir = float2((float)mx, (float)my);
                                const float2 offset = dir * outputTexel * radius;
                                const float3 previousMoved = PreviousSource.SampleLevel(
                                    LinearClamp, i.uv + offset, 0.0).rgb;
                                const float error = SourceMatchError(
                                    rawSourceColor, previousMoved);
                                if (error < bestDiagonalError)
                                {
                                    bestDiagonalError = error;
                                    bestDiagonalDir = dir;
                                }
                            }
                        }
                    }

                    if (dot(bestDiagonalDir, bestDiagonalDir) > 0.0 &&
                        bestDiagonalError < sourceDelta * 0.80)
                    {
                        float bestPatchError = samePatchError;
                        float2 bestPatchOffset = float2(0.0, 0.0);
                        [unroll]
                        for (int ri = 1; ri < 5; ++ri)
                        {
                            const float radius = exp2((float)ri);
                            const float2 offset = bestDiagonalDir * outputTexel * radius;
                            float shiftedPatchError = 0.0;
                            [unroll]
                            for (int py = -1; py <= 1; ++py)
                            {
                                [unroll]
                                for (int px = -1; px <= 1; ++px)
                                {
                                    if (abs(px) + abs(py) <= 1)
                                    {
                                        const float2 patchOffset = float2((float)px, (float)py) *
                                            outputTexel * verifyRadius;
                                        const float3 currentPatch = CurrentFrame.SampleLevel(
                                            LinearClamp, i.uv + patchOffset, 0.0).rgb;
                                        const float3 previousShiftPatch = PreviousSource.SampleLevel(
                                            LinearClamp, i.uv + patchOffset + offset, 0.0).rgb;
                                        shiftedPatchError += SourceMatchError(
                                            currentPatch, previousShiftPatch);
                                    }
                                }
                            }
                            if (shiftedPatchError < bestPatchError)
                            {
                                bestPatchError = shiftedPatchError;
                                bestPatchOffset = offset;
                            }
                        }

                        const float patchImprovement = saturate(
                            1.0 - bestPatchError / samePatchError);
                        const float absoluteMatch = 1.0 - smoothstep(0.040, 0.145,
                            bestPatchError / 5.0);
                        const float diagonalGate = smoothstep(0.32, 0.68,
                            patchImprovement) * absoluteMatch;
                        if (diagonalGate > localMotionGate)
                        {
                            localMotionGate = diagonalGate > 0.60 ? 1.0 : diagonalGate;
                        }
                    }
                }

                // Arbitrary shallow/steep slopes are common for projectiles,
                // particles and model edges. Cardinal + 45-degree searches can
                // miss a 3:1 or 3:2 translation even when the raw source clearly
                // proves motion. Refine any non-decisive cheaper match: a partial
                // cardinal gate must not prevent a better exact oblique candidate.
                if (localMotionGate < 0.90 && samePatchError > 0.012)
                {
                    float bestObliqueError = sourceDelta;
                    float2 bestObliqueOffset = float2(0.0, 0.0);
                    [unroll]
                    for (int major = 2; major <= 3; ++major)
                    {
                        [unroll]
                        for (int minor = 1; minor < major; ++minor)
                        {
                            [unroll]
                            for (int swapAxes = 0; swapAxes < 2; ++swapAxes)
                            {
                                [unroll]
                                for (int sy = -1; sy <= 1; sy += 2)
                                {
                                    [unroll]
                                    for (int sx = -1; sx <= 1; sx += 2)
                                    {
                                        const float2 offsetPixels = swapAxes == 0 ?
                                            float2((float)(sx * major), (float)(sy * minor)) :
                                            float2((float)(sx * minor), (float)(sy * major));
                                        const float2 offset = offsetPixels * outputTexel;
                                        const float3 previousMoved = PreviousSource.SampleLevel(
                                            LinearClamp, i.uv + offset, 0.0).rgb;
                                        const float error = SourceMatchError(
                                            rawSourceColor, previousMoved);
                                        if (error < bestObliqueError)
                                        {
                                            bestObliqueError = error;
                                            bestObliqueOffset = offset;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (dot(bestObliqueOffset, bestObliqueOffset) > 0.0 &&
                        bestObliqueError < sourceDelta * 0.82)
                    {
                        float shiftedPatchError = 0.0;
                        [unroll]
                        for (int py = -1; py <= 1; ++py)
                        {
                            [unroll]
                            for (int px = -1; px <= 1; ++px)
                            {
                                if (abs(px) + abs(py) <= 1)
                                {
                                    const float2 patchOffset = float2((float)px, (float)py) *
                                        outputTexel * verifyRadius;
                                    const float3 currentPatch = CurrentFrame.SampleLevel(
                                        LinearClamp, i.uv + patchOffset, 0.0).rgb;
                                    const float3 previousShiftPatch = PreviousSource.SampleLevel(
                                        LinearClamp, i.uv + patchOffset + bestObliqueOffset, 0.0).rgb;
                                    shiftedPatchError += SourceMatchError(
                                        currentPatch, previousShiftPatch);
                                }
                            }
                        }

                        const float patchImprovement = saturate(
                            1.0 - shiftedPatchError / samePatchError);
                        const float absoluteMatch = 1.0 - smoothstep(
                            0.040, 0.145, shiftedPatchError / 5.0);
                        const float obliqueGate = smoothstep(
                            0.28, 0.62, patchImprovement) * absoluteMatch;
                        localMotionGate = max(localMotionGate,
                            obliqueGate > 0.56 ? 1.0 : obliqueGate);
                    }
                }

                // Dense patch-space refinement resolves the ambiguity of flat
                // bright objects: many offsets can match the center color, so the)HLSL" R"HLSL(
                // earlier directional searches may pick the wrong transport. Test
                // every small local translation by PATCH error and make a strong
                // structural match decisive. Keep this off global/broad protection
                // so a true flash never pays the dense search or gets bypassed.
                if (localMotionGate < 0.98 && samePatchError > 0.012 &&
                    abs(protectionGate) < 0.5 && overloadGate < 0.5 && P6.x < 0.5)
                {
                    float bestDensePatchError = samePatchError;
                    [loop]
                    for (int denseY = -4; denseY <= 4; ++denseY)
                    {
                        [loop]
                        for (int denseX = -4; denseX <= 4; ++denseX)
                        {
                            if (denseX == 0 && denseY == 0) continue;
                            const float2 denseOffset = float2(
                                (float)denseX, (float)denseY) * outputTexel;
                            float densePatchError = 0.0;
                            [unroll]
                            for (int py = -1; py <= 1; ++py)
                            {
                                [unroll]
                                for (int px = -1; px <= 1; ++px)
                                {
                                    if (abs(px) + abs(py) <= 1)
                                    {
                                        const float2 patchOffset =
                                            float2((float)px, (float)py) *
                                            outputTexel * verifyRadius;
                                        const float3 currentPatch =
                                            CurrentFrame.SampleLevel(
                                                LinearClamp, i.uv + patchOffset, 0.0).rgb;
                                        const float3 previousPatch =
                                            PreviousSource.SampleLevel(
                                                LinearClamp,
                                                i.uv + patchOffset + denseOffset,
                                                0.0).rgb;
                                        densePatchError += SourceMatchError(
                                            currentPatch, previousPatch);
                                    }
                                }
                            }
                            bestDensePatchError = min(
                                bestDensePatchError, densePatchError);
                        }
                    }

                    const float denseImprovement = saturate(
                        1.0 - bestDensePatchError / samePatchError);
                    const float denseMeanError = bestDensePatchError / 5.0;
                    if (denseImprovement > 0.24 && denseMeanError < 0.085)
                    {
                        localMotionGate = 1.0;
                    }
                    else
                    {
                        const float denseAbsoluteMatch = 1.0 - smoothstep(
                            0.035, 0.120, denseMeanError);
                        const float denseGate = smoothstep(
                            0.24, 0.60, denseImprovement) * denseAbsoluteMatch;
                        localMotionGate = max(localMotionGate, denseGate);
                    }
                }
            }
        })HLSL" R"HLSL(

        // Keep displayed history at the SAME screen coordinate. Optical flow is
        // used only to decide whether temporal feedback should be bypassed. This
        // removes visible geometry warping/rubber-sheet artifacts from imperfect
        // vectors while preserving the safety filter on genuinely static flashes.
        const float3 previousDisplayed = PreviousOutput.SampleLevel(
            LinearClamp, i.uv, 0.0).rgb;
        const float previousDisplayedL = Luma(previousDisplayed);
        const float displayedDelta = abs(candidateL - previousDisplayedL);

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
        const float eventGate = smoothstep(0.025, 0.14, eventSeed);
        const float holdGate = smoothstep(0.16, 0.58, holdSeed);
        // Either analyzer-space coherent translation or verified full-resolution
        // motion classification is sufficient to reject temporal drag. Local flow
        // is especially important for small bright moving objects that occupy too
        // little of a 128x72 analysis patch to register as camera motion.
        const float coarseMotionGate = smoothstep(0.20, 0.68, coarseMotion);
        // The CPU analyzer already has a robust whole-frame translation score.
        // RenderSource uses it to avoid unnecessary NVOFA solves, so PSMain must
        // honor the same decision or camera pans can still blend stale history.
        const float cpuCameraMotionGate =
            (P9.x > 0.5 || protectionGate > 0.5 || overloadGate > 0.5) ? 0.0 :
            smoothstep(max(0.10, P5.x * 0.85), max(0.18, P5.x * 1.35), P6.y);
        const float motionGate = max(max(coarseMotionGate, localMotionGate),
            cpuCameraMotionGate);

        // Small ordinary changes should not drag filtered history around. A
        // CURRENT hazard gets a sensitive localization gate; MEMORY-only release
        // requires a much larger outstanding displayed difference.
        const float eventDeltaGate = smoothstep(0.008, 0.035, displayedDelta);
        const float holdDeltaGate = smoothstep(0.028, 0.085, displayedDelta);

        // During MEMORY-only release, source history is the crucial discriminator:
        // if the raw source barely changed, this is the same protected
        // surface and we must continue a smooth safe catch-up. If the raw source
        // changed substantially but there is no new hazard event, treat it as new
        // content/motion and mostly bypass old history. A high-risk memory keeps a
        // small safety floor in case one detector frame is missed during repetition.
        const float stableSourceGate = 1.0 - smoothstep(0.012, 0.055, sourceDelta);
        const float veryHighMemory = smoothstep(0.78, 0.96, holdSeed);
        // Memory alone must not drag bright moving content. Keep only a tiny floor
        // for a detector-frame miss during an already very strong flash sequence.
        const float movingHoldFloor = lerp(0.0, 0.035, veryHighMemory);
        const float holdContentGate = max(stableSourceGate, movingHoldFloor);

        const float eventMask = eventGate * eventDeltaGate;
        const float holdMask = holdGate * holdContentGate * holdDeltaGate;
        float temporalMask = max(eventMask, holdMask) * (1.0 - motionGate);

        // Only a CURRENT strong detector event can force full temporal authority.
        // Stale memory can remain strong on a static protected surface, but it can
        // never turn newly moving content into a 100% history blend.
        if (eventSeed >= 0.12 && displayedDelta >= 0.018)
            temporalMask = max(temporalMask, 1.0 - motionGate);

        if (temporalMask > 0.001)
        {
            const float severe = smoothstep(0.45, 0.90, max(eventSeed, holdSeed));
            // While a transition is actively happening, retain the strong
            // research-inspired temporal low-pass. During release, converge much
            // faster so a finished flash does not turn later motion into trails.
            const bool currentEvent = eventGate > 0.05;
            const float tau = currentEvent ?
                lerp(0.150, 0.235, severe) : lerp(0.030, 0.050, severe);
            const float alpha = 1.0 - exp(-dt / tau);

            // Blend the actual previous FILTERED image toward the candidate in
            // linear light. This is true temporal filtering rather than painting
            // a newly invented gray target onto a few detector cells. It also
            // attenuates chromatic alternation whose luminance happens to match.
            const float3 previousLinear = SrgbToLinear(previousDisplayed);
            const float3 candidateLinear = SrgbToLinear(cur);
            float3 temporallyFiltered = LinearToSrgb(
                lerp(previousLinear, candidateLinear, alpha));
            float limitedL = Luma(temporallyFiltered);

            // During a CURRENT hazardous transition, keep the hard symmetric
            // slew bound. During release, allow faster convergence; release is
            // weakly masked and motion-bypassed above, which removes persistent
            // trails without snapping a still-protected flash to full amplitude.
            const float profileStep = min(P4.z, P4.w);
            const float standardsStep = 1.30 * dt;
            float maxStep = currentEvent ? min(profileStep, standardsStep) :
                max(standardsStep, 2.60 * dt);
            maxStep *= currentEvent ? lerp(1.0, 0.72, severe) : 1.0;
            const float slewLimitedL = clamp(limitedL,
                previousDisplayedL - maxStep,
                previousDisplayedL + maxStep);
            temporallyFiltered = RemapGlobalLuminance(
                temporallyFiltered, limitedL, slewLimitedL);

            cur = lerp(cur, temporallyFiltered, saturate(temporalMask));
            candidateL = Luma(cur);
        }
    }

    // Save filtered content color BEFORE debug/hotkey/shield-label overlays so
    // UI pixels never contaminate the temporal feedback state.
    const float4 historyColor = float4(cur, 1.0);

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
    output.historyColor = float4(saturate(historyColor.rgb), 1.0);
    output.sourceHistoryColor = float4(saturate(sourceHistoryColor), 1.0);
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
    // separation below prevents that memory from ghosting unrelated motion. A
    // moderate decay preserves low-frequency repetition better than v7's first
    // aggressive draft while still clearing a lone event much faster than v6.
    float riskEnergy = max(0.0, temporalHistory.g - dt * 1.20);
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
        float eventBoost = 0.24 + 0.44 * transitionStrength;
        if (directionReversal && temporalHistory.g >= 0.10)
            eventBoost += 0.36;
        riskEnergy = saturate(riskEnergy + eventBoost);
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
    output.safety = float4(curL, signedCurrentState, saturate(riskStrength),
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

    class FlashGuardApp
    {
    public:
        ~FlashGuardApp() { Stop(); }

        void Initialize(HWND output, HMONITOR monitor)
        {
            m_output = output;
            m_monitor = monitor;
            m_debugEnabled.store(m_safety.debugOverlay, std::memory_order_release);
            FindOutputAndCreateDevice();

            winrtlessEnableMultithreadProtection();
            CreateSwapChain();
            CreatePipeline();
            CreateAnalysisResources();
            RecreateOutputResources();
            CreateShieldTexture();
            CreateDebugResources();
            CreateHintResources();
            ClearAllToBlack();
            CreateDuplication();

            m_lastFrameMs.store(NowMs(), std::memory_order_release);
            m_captureThread = std::thread([this] { CaptureLoop(); });
        }

        void InitializeReplay(HWND output, HMONITOR monitor)
        {
            m_output = output;
            m_monitor = monitor;
            m_replayMode = true;
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
        }

        bool RunSyntheticReplay(const std::wstring& reportPath,
                                const std::wstring& visualDir = L"")
        {
            if (!m_replayMode || !m_device || !m_context || !m_replayReadback ||
                m_outputWidth == 0 || m_outputHeight == 0)
                return false;

            constexpr float dt = 1.0f / 60.0f;
            const UINT width = m_outputWidth;
            const UINT height = m_outputHeight;
            std::vector<uint32_t> pixels(static_cast<size_t>(width) * height);
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
                        "Frames are sampled every 10 synthetic frames at 60 FPS.\r\n", visualReadme);
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
                double sourceWcagLuma = 0.0;
                double outputWcagLuma = 0.0;
                double mae = 0.0;
                double outsideMae = 0.0;
                double insideMae = 0.0;
                double edgeMae = 0.0;
            };

            uint64_t flowFrames = 0;
            const auto renderAndSample = [&](const RECT* activeRect,
                                             const wchar_t* visualCase = nullptr,
                                             int visualFrame = -1) {
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
                constexpr UINT stride = 4;
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
                        const double outputLuma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
                        const double outputLinearR = srgbToLinear(r);
                        const double outputLinearG = srgbToLinear(g);
                        const double outputLinearB = srgbToLinear(b);
                        const double outputWcagLuma = 0.2126 * outputLinearR +
                            0.7152 * outputLinearG + 0.0722 * outputLinearB;

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
                        sample.sourceWcagLuma += sourceWcagLuma;
                        sample.outputWcagLuma += outputWcagLuma;
                        sample.mae += error;
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
                writeVisualBmp(visualCase, visualFrame, mapped);
                m_context->Unmap(m_replayReadback.get(), 0);
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
                    sample.sourceWcagLuma /= static_cast<double>(count);
                    sample.outputWcagLuma /= static_cast<double>(count);
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
                m_lastHazardTime = -100.0f;
                m_lastGlobalHazardTime = -100.0f;
                m_nextSequence = 0;
                ClearAllToBlack();
            };

            resetCase();
            fillGray(96);
            for (int i = 0; i < 20; ++i) renderAndSample(nullptr);
            double staticMae = 0.0;
            for (int i = 0; i < 40; ++i)
                staticMae += renderAndSample(nullptr).mae;
            staticMae /= 40.0;
            const uint64_t staticFlowFrames = flowFrames;

            resetCase();
            fillGray(20);
            FrameSample previous = renderAndSample(nullptr);
            for (int i = 0; i < 19; ++i) previous = renderAndSample(nullptr);
            double rawVariation = 0.0;
            double outputVariation = 0.0;
            for (int i = 0; i < 120; ++i)
            {
                fillGray(((i / 2) & 1) ? 235 : 20);
                const FrameSample current = renderAndSample(nullptr,
                    (writeVisuals && i % 10 == 0) ? L"flash_15hz" : nullptr,
                    i);
                rawVariation += std::fabs(current.sourceMean - previous.sourceMean);
                outputVariation += std::fabs(current.outputMean - previous.outputMean);
                previous = current;
            }
            const double flashReduction = rawVariation > 1e-9 ?
                1.0 - outputVariation / rawVariation : 0.0;
            const uint64_t flashFlowFrames = flowFrames - staticFlowFrames;

            // Standards-oriented flash sweep. W3C defines a flash as two
            // opposing transitions. For the general-flash rate below, a
            // transition must change relative luminance by at least 0.10 and
            // have a darker state below 0.80. The saturated-red counter follows
            // the WCAG working definition using R/(R+G+B) >= 0.8 and a
            // (R-G-B)*320 transition magnitude above 20. The quarter-screen case
            // is deliberately conservative but is not a formal visual-angle
            // compliance measurement.
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
            };
            std::vector<FlashSweepResult> flashSweep;
            constexpr std::array<double, 8> sweepFrequencies{
                5.0, 7.5, 10.0, 12.0, 15.0, 20.0, 25.0, 30.0
            };
            struct SweepCase
            {
                const char* name;
                int kind; // 0=full luminance, 1=full saturated red, 2=quarter luminance
            };
            constexpr std::array<SweepCase, 3> sweepCases{{
                { "luminance_full", 0 },
                { "red_full", 1 },
                { "luminance_quarter", 2 }
            }};
            const auto countGeneralTransition = [](
                double previousValue, double currentValue, int& rises, int& falls) {
                const double delta = currentValue - previousValue;
                if (std::fabs(delta) >= 0.10 &&
                    std::min(previousValue, currentValue) < 0.80)
                {
                    if (delta > 0.0) ++rises;
                    else ++falls;
                }
            };
            const auto countRedTransition = [](
                const FrameSample& previousSample, const FrameSample& currentSample,
                bool output, int& rises, int& falls) {
                struct Chromaticity
                {
                    double redRatio = 0.0;
                    double u = 0.0;
                    double v = 0.0;
                };
                const auto chromaticity = [](double r, double g, double b) {
                    Chromaticity state{};
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
                const double pr = output ? previousSample.outputLinearRed : previousSample.sourceLinearRed;
                const double pg = output ? previousSample.outputLinearGreen : previousSample.sourceLinearGreen;
                const double pb = output ? previousSample.outputLinearBlue : previousSample.sourceLinearBlue;
                const double cr = output ? currentSample.outputLinearRed : currentSample.sourceLinearRed;
                const double cg = output ? currentSample.outputLinearGreen : currentSample.sourceLinearGreen;
                const double cb = output ? currentSample.outputLinearBlue : currentSample.sourceLinearBlue;
                const Chromaticity previous = chromaticity(pr, pg, pb);
                const Chromaticity current = chromaticity(cr, cg, cb);
                const bool saturated = previous.redRatio >= 0.80 || current.redRatio >= 0.80;
                const double du = current.u - previous.u;
                const double dv = current.v - previous.v;
                if (saturated && std::sqrt(du * du + dv * dv) > 0.20)
                {
                    if (current.redRatio > previous.redRatio) ++rises;
                    else ++falls;
                }
            };
            constexpr int sweepFrames = 120; // two seconds at 60 FPS
            constexpr double sweepSeconds = static_cast<double>(sweepFrames) / 60.0;
            bool flashSweepPass = true;
            for (const SweepCase& sweepCase : sweepCases)
            {
                for (double frequencyHz : sweepFrequencies)
                {
                    resetCase();
                    if (sweepCase.kind == 1)
                        std::fill(pixels.begin(), pixels.end(), rgbPixel(8, 8, 8));
                    else
                        fillGray(20);
                    FrameSample previousSweep = renderAndSample(nullptr);
                    for (int i = 0; i < 19; ++i)
                        previousSweep = renderAndSample(nullptr);

                    FlashSweepResult result{};
                    result.caseName = sweepCase.name;
                    result.frequencyHz = frequencyHz;
                    double rawRedVariation = 0.0;
                    double outputRedVariation = 0.0;
                    int rawGeneralRises = 0;
                    int rawGeneralFalls = 0;
                    int outputGeneralRises = 0;
                    int outputGeneralFalls = 0;
                    int rawRedRises = 0;
                    int rawRedFalls = 0;
                    int outputRedRises = 0;
                    int outputRedFalls = 0;

                    for (int i = 0; i < sweepFrames; ++i)
                    {
                        const double phase = std::fmod(
                            (static_cast<double>(i) + 0.5) * frequencyHz / 60.0, 1.0);
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

                        const FrameSample currentSweep = renderAndSample(nullptr);
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
                        countGeneralTransition(previousSweep.sourceMean,
                            currentSweep.sourceMean, rawGeneralRises, rawGeneralFalls);
                        countGeneralTransition(previousSweep.outputMean,
                            currentSweep.outputMean, outputGeneralRises, outputGeneralFalls);
                        countRedTransition(previousSweep, currentSweep, false,
                            rawRedRises, rawRedFalls);
                        countRedTransition(previousSweep, currentSweep, true,
                            outputRedRises, outputRedFalls);
                        previousSweep = currentSweep;
                    }

                    result.reduction = result.rawVariation > 1e-9 ?
                        1.0 - result.outputVariation / result.rawVariation : 0.0;
                    result.rawGeneralFlashesPerSecond =
                        static_cast<double>(std::min(rawGeneralRises, rawGeneralFalls)) /
                        sweepSeconds;
                    result.outputGeneralFlashesPerSecond =
                        static_cast<double>(std::min(
                            outputGeneralRises, outputGeneralFalls)) / sweepSeconds;
                    result.rawRedFlashesPerSecond =
                        static_cast<double>(std::min(rawRedRises, rawRedFalls)) /
                        sweepSeconds;
                    result.outputRedFlashesPerSecond =
                        static_cast<double>(std::min(outputRedRises, outputRedFalls)) /
                        sweepSeconds;

                    const bool stimulusValid =
                        result.rawGeneralFlashesPerSecond > 3.0 ||
                        (sweepCase.kind == 1 && result.rawRedFlashesPerSecond > 3.0);
                    const bool outputBelowThree =
                        result.outputGeneralFlashesPerSecond <= 3.0 &&
                        (sweepCase.kind != 1 ||
                         result.outputRedFlashesPerSecond <= 3.0);
                    flashSweepPass = flashSweepPass && stimulusValid && outputBelowThree;
                    flashSweep.push_back(result);
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
                "  \"schema\": \"FLASHGUARD_FLASH_SWEEP/1\",\n"
                "  \"status\": \"%s\",\n"
                "  \"fps\": 60,\n"
                "  \"duration_seconds_per_case\": %.2f,\n"
                "  \"note\": \"Screen-mean W3C-style regression; quarter-screen area is not a formal visual-angle certification.\",\n"
                "  \"cases\": [\n",
                flashSweepPass ? "SUCCESS" : "FAILED", sweepSeconds);
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
                    "\"output_red_flashes_per_second\":%.3f}%s\n",
                    result.caseName.c_str(), result.frequencyHz,
                    result.rawVariation, result.outputVariation, result.reduction,
                    result.peakOutputDelta,
                    result.rawGeneralFlashesPerSecond,
                    result.outputGeneralFlashesPerSecond,
                    result.rawRedFlashesPerSecond,
                    result.outputRedFlashesPerSecond,
                    (i + 1 < flashSweep.size()) ? "," : "");
            }
            std::fputs("  ]\n}\n", flashSweepReport);
            std::fclose(flashSweepReport);

            const uint64_t movingFlowStart = flowFrames;

            resetCase();
            fillGray(24);
            for (int i = 0; i < 20; ++i) renderAndSample(nullptr);
            double movingGhostMae = 0.0;
            double movingInsideMae = 0.0;
            double movingEdgeMae = 0.0;
            constexpr int squareSize = 64;
            constexpr int movingFrames = 90;
            const int settledX0 = 20;
            const int settledY0 = static_cast<int>(height) / 2 - squareSize / 2;
            for (int i = 0; i < 30; ++i)
            {
                fillGray(24);
                const int x1 = std::min(settledX0 + squareSize, static_cast<int>(width));
                const int y1 = std::min(settledY0 + squareSize, static_cast<int>(height));
                for (int y = std::max(settledY0, 0); y < y1; ++y)
                    for (int x = std::max(settledX0, 0); x < x1; ++x)
                        pixels[static_cast<size_t>(y) * width + x] = grayPixel(235);
                renderAndSample(nullptr);
            }
            for (int i = 0; i < movingFrames; ++i)
            {
                fillGray(24);
                const int x0 = settledX0 + (i + 1) * 4;
                const int y0 = static_cast<int>(height) / 2 - squareSize / 2;
                const int x1 = std::min(x0 + squareSize, static_cast<int>(width));
                const int y1 = std::min(y0 + squareSize, static_cast<int>(height));
                for (int y = std::max(y0, 0); y < y1; ++y)
                    for (int x = std::max(x0, 0); x < x1; ++x)
                        pixels[static_cast<size_t>(y) * width + x] = grayPixel(235);
                const RECT square{ x0, y0, x1, y1 };
                const FrameSample sample = renderAndSample(&square,
                    (writeVisuals && i % 10 == 0) ? L"bright_motion" : nullptr,
                    i);
                movingGhostMae += sample.outsideMae;
                movingInsideMae += sample.insideMae;
                movingEdgeMae += sample.edgeMae;
            }
            movingGhostMae /= static_cast<double>(movingFrames);
            movingInsideMae /= static_cast<double>(movingFrames);
            movingEdgeMae /= static_cast<double>(movingFrames);
            const uint64_t movingFlowFrames = flowFrames - movingFlowStart;

            // Bright oblique motion: real game objects rarely move on an exact
            // cardinal or 45-degree path. Keep the object present long enough for
            // its initial appearance protection to release, then measure motion.
            const uint64_t obliqueFlowStart = flowFrames;
            resetCase();
            constexpr int obliqueSize = 32;
            constexpr int obliqueFrames = 80;
            const int obliqueStartX = 80;
            const int obliqueStartY = 80;
            for (int i = 0; i < 30; ++i)
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
                const int x0 = obliqueStartX + (i + 1) * 3;
                const int y0 = obliqueStartY + (i + 1);
                const int x1 = x0 + obliqueSize;
                const int y1 = y0 + obliqueSize;
                for (int y = y0; y < y1; ++y)
                    for (int x = x0; x < x1; ++x)
                        pixels[static_cast<size_t>(y) * width + x] = grayPixel(235);
                const RECT square{ x0, y0, x1, y1 };
                const FrameSample sample = renderAndSample(&square,
                    (writeVisuals && i % 10 == 0) ? L"bright_oblique" : nullptr,
                    i);
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
            for (int i = 0; i < 20; ++i) renderAndSample(nullptr);
            double smallMovingGhostMae = 0.0;
            constexpr int smallSquareSize = 24;
            constexpr int smallMovingFrames = 90;
            for (int i = 0; i < smallMovingFrames; ++i)
            {
                fillGray(72);
                const int x0 = 40 + i * 2;
                const int y0 = static_cast<int>(height) / 3 - smallSquareSize / 2;
                const int x1 = std::min(x0 + smallSquareSize, static_cast<int>(width));
                const int y1 = std::min(y0 + smallSquareSize, static_cast<int>(height));
                for (int y = std::max(y0, 0); y < y1; ++y)
                    for (int x = std::max(x0, 0); x < x1; ++x)
                        pixels[static_cast<size_t>(y) * width + x] = grayPixel(150);
                const RECT square{ x0, y0, x1, y1 };
                smallMovingGhostMae += renderAndSample(&square,
                    (writeVisuals && i % 10 == 0) ? L"small_motion" : nullptr,
                    i).outsideMae;
            }
            smallMovingGhostMae /= static_cast<double>(smallMovingFrames);
            const uint64_t smallMovingFlowFrames =
                flowFrames - smallMovingFlowStart;
            const uint64_t panFlowStart = flowFrames;

            resetCase();
            fillPanPattern(0);
            for (int i = 0; i < 12; ++i) renderAndSample(nullptr);
            double panMae = 0.0;
            double panCameraMotion = 0.0;
            double panAffectedArea = 0.0;
            double panCoherence = 0.0;
            double panFlashEnergy = 0.0;
            float panCameraMotionMax = 0.0f;
            constexpr int panFrames = 90;
            for (int i = 0; i < panFrames; ++i)
            {
                fillPanPattern((i + 1) * 3);
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
            for (int i = 0; i < 12; ++i) renderAndSample(nullptr);
            double fastPanMae = 0.0;
            for (int i = 0; i < panFrames; ++i)
            {
                fillPanPattern((i + 1) * 8);
                fastPanMae += renderAndSample(nullptr).mae;
            }
            fastPanMae /= static_cast<double>(panFrames);
            const uint64_t fastPanFlowFrames = flowFrames - fastPanFlowStart;

            const uint64_t extremePanFlowStart = flowFrames;
            resetCase();
            fillPanPattern(0);
            for (int i = 0; i < 12; ++i) renderAndSample(nullptr);
            double extremePanMae = 0.0;
            for (int i = 0; i < panFrames; ++i)
            {
                fillPanPattern((i + 1) * 16);
                extremePanMae += renderAndSample(nullptr).mae;
            }
            extremePanMae /= static_cast<double>(panFrames);
            const uint64_t extremePanFlowFrames = flowFrames - extremePanFlowStart;

            const bool metricsFinite = std::isfinite(staticMae) &&
                std::isfinite(flashReduction) && std::isfinite(movingGhostMae) &&
                std::isfinite(movingInsideMae) && std::isfinite(movingEdgeMae) &&
                std::isfinite(obliqueGhostMae) && std::isfinite(obliqueInsideMae) &&
                std::isfinite(obliqueEdgeMae) &&
                std::isfinite(smallMovingGhostMae) && std::isfinite(panMae) &&
                std::isfinite(fastPanMae) && std::isfinite(extremePanMae);
            const bool pass = metricsFinite && staticMae < 0.005 &&
                rawVariation > 0.10 && flashReduction > 0.90 &&
                flashSweepPass &&
                movingGhostMae < 0.005 && smallMovingGhostMae < 0.003 &&
                panMae < 0.010 && fastPanMae < 0.020 &&
                extremePanMae < 0.030 && flowFrames > 0;

            FILE* report = nullptr;
            if (_wfopen_s(&report, reportPath.c_str(), L"wb") != 0 || !report)
                return false;
            std::fprintf(report,
                "{\n"
                "  \"schema\": \"FLASHGUARD_REPLAY/1\",\n"
                "  \"status\": \"%s\",\n"
                "  \"width\": %u,\n"
                "  \"height\": %u,\n"
                "  \"fps\": 60,\n"
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
                staticMae, rawVariation, outputVariation, flashReduction,
                movingGhostMae, movingInsideMae, movingEdgeMae,
                obliqueGhostMae, obliqueInsideMae, obliqueEdgeMae,
                smallMovingGhostMae,
                panMae, fastPanMae, extremePanMae,
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

            if (m_captureThread.joinable())
                m_captureThread.join();

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
                RenderShieldStep();
            }
            else
            {
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

            const int latencyMs = options.latencyMs == 0 ? 0 :
                std::clamp(options.latencyMs, 25, 100);
            if (m_safety.lookaheadMs != latencyMs)
            {
                m_safety.lookaheadMs = latencyMs;
                m_rawFrames.clear();
                m_inputWidth = m_inputHeight = 0;
                m_inputFormat = DXGI_FORMAT_UNKNOWN;
                ResetDelayedPipeline();
            }
            m_debugEnabled.store(options.debugOverlay, std::memory_order_release);
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
                m_captureRecoveryFrames.store(8, std::memory_order_release);
            if (m_manualShield.load(std::memory_order_acquire) ||
                m_automaticShieldActive.load(std::memory_order_acquire))
            {
                RenderShieldStep();
            }
        }

        void ResizeOutput()
        {
            std::scoped_lock lock(m_mutex);
            if (!m_swapChain) return;
            m_backBuffer = nullptr;
            m_backBufferRTV = nullptr;
            HRESULT hr = m_swapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN,
                DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
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
            m_captureFault.store(true, std::memory_order_release);
            int64_t expected = 0;
            m_captureFaultSinceMs.compare_exchange_strong(expected, NowMs(),
                std::memory_order_acq_rel);
        }

        void CaptureLoop()
        {
            while (!m_stopped.load(std::memory_order_acquire))
            {
                if (m_captureRestartRequested.exchange(false, std::memory_order_acq_rel))
                {
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

                // Keep the present queue empty BEFORE acquiring the next active
                // desktop frame. Waiting after capture makes the captured image
                // older while DXGI/DWM catches up, which is especially visible in
                // sharp, high-FPS windowed games. Skip the wait after true idle so
                // a newly changing desktop can wake capture immediately.
                const int64_t previousCaptureMs =
                    m_lastRealCaptureMs.load(std::memory_order_acquire);
                const auto presentReadyWaitStart = std::chrono::steady_clock::now();
                if (m_frameLatencyWaitableObject && previousCaptureMs > 0 &&
                    NowMs() - previousCaptureMs < 50)
                {
                    WaitForSingleObjectEx(m_frameLatencyWaitableObject, 20, FALSE);
                }
                m_presentReadyWaitMs.store(std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - presentReadyWaitStart).count(),
                    std::memory_order_release);

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
                    if (trulyIdle &&
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
                        if (!std::isfinite(dt) || dt <= 0.0f || dt > 0.5f) dt = 1.0f / 60.0f;
                        m_lastFrameTime = now;

                        const auto processingStart = std::chrono::steady_clock::now();
                        {
                            std::scoped_lock lock(m_mutex);
                            if (!m_stopped.load())
                                QueueCapturedFrame(texture.get(), dt);
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

        void CreateSwapChain()
        {
            winrt::com_ptr<IDXGIDevice> dxgiDevice;
            ThrowIfFailed(m_device->QueryInterface(__uuidof(IDXGIDevice), dxgiDevice.put_void()));
            winrt::com_ptr<IDXGIAdapter> adapter;
            ThrowIfFailed(dxgiDevice->GetAdapter(adapter.put()));
            winrt::com_ptr<IDXGIFactory2> factory;
            ThrowIfFailed(adapter->GetParent(__uuidof(IDXGIFactory2), factory.put_void()));

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

                // Translation matching suppresses coherent pans without suppressing
                // large exposure changes or confirmed future reversals. First find
                // the best whole-cell shift, then refine it to sub-cell precision.
                // This is important for slow scrolling: at 1080p one analysis cell
                // is about 15 screen pixels, so a few pixels of motion is fractional.
                constexpr int motionMarginX = 6;
                constexpr int motionMarginY = 10;
                const int analysisW = static_cast<int>(kAnalysisWidth);
                const int analysisH = static_cast<int>(kAnalysisHeight);

                const auto samplePrevious = [&](float x, float y) {
                    const int x0 = static_cast<int>(std::floor(x));
                    const int y0 = static_cast<int>(std::floor(y));
                    const int x1 = std::min(x0 + 1, analysisW - 1);
                    const int y1 = std::min(y0 + 1, analysisH - 1);
                    const float tx = x - static_cast<float>(x0);
                    const float ty = y - static_cast<float>(y0);
                    const auto at = [&](int sx, int sy) {
                        return m_prevAnalysis[static_cast<size_t>(sy) * kAnalysisWidth + sx];
                    };
                    const float top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * tx;
                    const float bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * tx;
                    return top + (bottom - top) * ty;
                };

                const auto integerShiftError = [&](int shiftX, int shiftY) {
                    float error = 0.0f;
                    uint32_t samples = 0;
                    for (int y = motionMarginY; y < analysisH - motionMarginY; ++y)
                    for (int x = motionMarginX; x < analysisW - motionMarginX; ++x)
                    {
                        const size_t cur = static_cast<size_t>(y) * kAnalysisWidth + x;
                        const size_t prev = static_cast<size_t>(y + shiftY) * kAnalysisWidth +
                            static_cast<size_t>(x + shiftX);
                        error += std::fabs(m_currentAnalysis[cur] - m_prevAnalysis[prev]);
                        ++samples;
                    }
                    return samples ? error / static_cast<float>(samples) : 0.0f;
                };

                const auto fractionalShiftError = [&](float shiftX, float shiftY) {
                    float error = 0.0f;
                    uint32_t samples = 0;
                    for (int y = motionMarginY; y < analysisH - motionMarginY; ++y)
                    for (int x = motionMarginX; x < analysisW - motionMarginX; ++x)
                    {
                        const size_t cur = static_cast<size_t>(y) * kAnalysisWidth + x;
                        const float previous = samplePrevious(
                            static_cast<float>(x) + shiftX,
                            static_cast<float>(y) + shiftY);
                        error += std::fabs(m_currentAnalysis[cur] - previous);
                        ++samples;
                    }
                    return samples ? error / static_cast<float>(samples) : 0.0f;
                };

                const float sameError = integerShiftError(0, 0);
                float bestShiftError = sameError;
                float bestShiftX = 0.0f;
                float bestShiftY = 0.0f;

                // Preserve the original search range for large motion. Keep this
                // stage on direct texel loads; only the small refinement is bilinear.
                for (int sy = -8; sy <= 8; ++sy)
                for (int sx = -4; sx <= 4; ++sx)
                {
                    if (sx == 0 && sy == 0) continue;
                    const float error = integerShiftError(sx, sy);
                    if (error < bestShiftError)
                    {
                        bestShiftError = error;
                        bestShiftX = static_cast<float>(sx);
                        bestShiftY = static_cast<float>(sy);
                    }
                }

                // Hierarchical fractional refinement adds only 24 candidates while
                // resolving motion down to 1/8 of an analysis cell.
                constexpr float refineSteps[] = { 0.5f, 0.25f, 0.125f };
                for (float step : refineSteps)
                {
                    float refinedError = bestShiftError;
                    float refinedX = bestShiftX;
                    float refinedY = bestShiftY;
                    for (int oy = -1; oy <= 1; ++oy)
                    for (int ox = -1; ox <= 1; ++ox)
                    {
                        if (ox == 0 && oy == 0) continue;
                        const float candidateX = bestShiftX + static_cast<float>(ox) * step;
                        const float candidateY = bestShiftY + static_cast<float>(oy) * step;
                        if (std::fabs(candidateX) > 4.875f || std::fabs(candidateY) > 8.875f)
                            continue;
                        const float error = fractionalShiftError(candidateX, candidateY);
                        if (error < refinedError)
                        {
                            refinedError = error;
                            refinedX = candidateX;
                            refinedY = candidateY;
                        }
                    }
                    bestShiftError = refinedError;
                    bestShiftX = refinedX;
                    bestShiftY = refinedY;
                }

                if (sameError > 0.001f)
                    stats.cameraMotionScore = std::clamp(
                        1.0f - bestShiftError / sameError, 0.0f, 1.0f);

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
            m_ringRead = 0;
            m_ringWrite = 0;
            m_bufferedFrameCount = 0;
            m_bufferedDuration = 0.0f;
            for (auto& frame : m_rawFrames) frame.statsReady = false;
            for (auto& readback : m_analysisReadbacks) readback.pending = false;
            m_havePrevAnalysis = false;
            m_instantHistoryValid = false;
            m_instantSafetyIndex = 0;
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

        void DestroyOpticalFlow()
        {
            m_nvofFlowValid = false;
            m_nvofPreviousValid = false;
            m_nvofLastExecuteSuccessful = false;
            if (m_nvofApi.nvOFUnregisterResourceD3D11)
            {
                for (auto& handle : m_nvofInputHandles)
                {
                    if (handle) m_nvofApi.nvOFUnregisterResourceD3D11(handle);
                    handle = nullptr;
                }
                if (m_nvofForwardHandle)
                    m_nvofApi.nvOFUnregisterResourceD3D11(m_nvofForwardHandle);
                if (m_nvofBackwardHandle)
                    m_nvofApi.nvOFUnregisterResourceD3D11(m_nvofBackwardHandle);
                if (m_nvofForwardCostHandle)
                    m_nvofApi.nvOFUnregisterResourceD3D11(m_nvofForwardCostHandle);
                if (m_nvofBackwardCostHandle)
                    m_nvofApi.nvOFUnregisterResourceD3D11(m_nvofBackwardCostHandle);
            }
            m_nvofForwardHandle = nullptr;
            m_nvofBackwardHandle = nullptr;
            m_nvofForwardCostHandle = nullptr;
            m_nvofBackwardCostHandle = nullptr;
            m_nvofForwardSRV = nullptr;
            m_nvofBackwardSRV = nullptr;
            m_nvofForwardCostSRV = nullptr;
            m_nvofBackwardCostSRV = nullptr;
            m_nvofForwardTexture = nullptr;
            m_nvofBackwardTexture = nullptr;
            m_nvofForwardCostTexture = nullptr;
            m_nvofBackwardCostTexture = nullptr;
            m_nvofCostEnabled = false;
            for (size_t i = 0; i < m_nvofInputTextures.size(); ++i)
            {
                m_nvofInputRTVs[i] = nullptr;
                m_nvofInputTextures[i] = nullptr;
            }
            if (m_nvofHandle && m_nvofApi.nvOFDestroy)
                m_nvofApi.nvOFDestroy(m_nvofHandle);
            m_nvofHandle = nullptr;
            m_nvofApi = {};
            if (m_nvofModule)
                FreeLibrary(m_nvofModule);
            m_nvofModule = nullptr;
            m_nvofWidth = m_nvofHeight = 0;
            m_nvofInputWidth = m_nvofInputHeight = 0;
            m_nvofFlowWidth = m_nvofFlowHeight = 0;
            m_nvofGridSize = 0;
        }

        bool NvofHasFormat(nvof5::NV_OF_BUFFER_USAGE usage, DXGI_FORMAT wanted)
        {
            uint32_t count = 0;
            if (!m_nvofApi.nvOFGetSurfaceFormatCountD3D11 ||
                m_nvofApi.nvOFGetSurfaceFormatCountD3D11(
                    m_nvofHandle, usage, nvof5::NV_OF_MODE_OPTICALFLOW, &count) != nvof5::NV_OF_SUCCESS ||
                count == 0)
                return false;
            std::vector<DXGI_FORMAT> formats(count);
            if (m_nvofApi.nvOFGetSurfaceFormatD3D11(
                    m_nvofHandle, usage, nvof5::NV_OF_MODE_OPTICALFLOW, formats.data()) != nvof5::NV_OF_SUCCESS)
                return false;
            return std::find(formats.begin(), formats.end(), wanted) != formats.end();
        }

        bool EnsureOpticalFlow(UINT width, UINT height)
        {
            if (m_nvofUnavailable) return false;
            if (m_nvofHandle && m_nvofWidth == width && m_nvofHeight == height)
                return true;
            DestroyOpticalFlow();
            if (width < 32 || height < 16) return false;

            // NVOFA sits on the render-critical dependency chain because PSMain
            // consumes its vectors immediately. Run the OF engine at half source
            // resolution: this cuts its pixel workload to ~25% while grid 1 still
            // gives roughly 2x2 full-resolution motion granularity.
            const UINT ofWidth = std::max<UINT>(32u, (width + 1u) / 2u);
            const UINT ofHeight = std::max<UINT>(16u, (height + 1u) / 2u);

            m_nvofModule = LoadLibraryW(L"nvofapi64.dll");
            if (!m_nvofModule)
            {
                m_nvofUnavailable = true;
                return false;
            }
            auto createInstance = reinterpret_cast<nvof5::PFNNVOFAPICREATEINSTANCED3D11>(
                GetProcAddress(m_nvofModule, "NvOFAPICreateInstanceD3D11"));
            if (!createInstance || createInstance(nvof5::kApiVersion, &m_nvofApi) != nvof5::NV_OF_SUCCESS)
            {
                DestroyOpticalFlow();
                m_nvofUnavailable = true;
                return false;
            }
            if (!m_nvofApi.nvCreateOpticalFlowD3D11 ||
                m_nvofApi.nvCreateOpticalFlowD3D11(
                    m_device.get(), m_context.get(), &m_nvofHandle) != nvof5::NV_OF_SUCCESS || !m_nvofHandle)
            {
                DestroyOpticalFlow();
                m_nvofUnavailable = true;
                return false;
            }

            if (!NvofHasFormat(nvof5::NV_OF_BUFFER_USAGE_INPUT, DXGI_FORMAT_B8G8R8A8_UNORM) ||
                !NvofHasFormat(nvof5::NV_OF_BUFFER_USAGE_OUTPUT, DXGI_FORMAT_R16G16_SINT))
            {
                DestroyOpticalFlow();
                m_nvofUnavailable = true;
                return false;
            }

            uint32_t gridCount = 0;
            if (m_nvofApi.nvOFGetCaps(m_nvofHandle,
                    nvof5::NV_OF_CAPS_SUPPORTED_OUTPUT_GRID_SIZES, nullptr, &gridCount) != nvof5::NV_OF_SUCCESS ||
                gridCount == 0)
            {
                DestroyOpticalFlow();
                m_nvofUnavailable = true;
                return false;
            }
            std::vector<uint32_t> grids(gridCount);
            if (m_nvofApi.nvOFGetCaps(m_nvofHandle,
                    nvof5::NV_OF_CAPS_SUPPORTED_OUTPUT_GRID_SIZES, grids.data(), &gridCount) != nvof5::NV_OF_SUCCESS)
            {
                DestroyOpticalFlow();
                m_nvofUnavailable = true;
                return false;
            }
            const auto supportsGrid = [&grids](uint32_t g) {
                return std::find(grids.begin(), grids.end(), g) != grids.end();
            };
            // Prefer the finest hardware-supported field for motion boundaries.
            // Ampere-class GPUs commonly expose grid 1; fall back to 2 then 4.
            m_nvofGridSize = supportsGrid(1) ? 1u : (supportsGrid(2) ? 2u : (supportsGrid(4) ? 4u : 0u));
            if (!m_nvofGridSize)
            {
                DestroyOpticalFlow();
                m_nvofUnavailable = true;
                return false;
            }

            // Use NVOFA's native 8-bit cost/confidence when exposed by the
            // D3D11 driver. NVIDIA recommends this over legacy 32-bit cost.
            m_nvofCostEnabled = !m_nvofCostDisabledByRuntime && NvofHasFormat(
                nvof5::NV_OF_BUFFER_USAGE_COST, DXGI_FORMAT_R8_UINT);

            nvof5::NV_OF_INIT_PARAMS init{};
            init.width = ofWidth;
            init.height = ofHeight;
            init.outGridSize = static_cast<nvof5::NV_OF_OUTPUT_VECTOR_GRID_SIZE>(m_nvofGridSize);
            init.hintGridSize = nvof5::NV_OF_HINT_VECTOR_GRID_SIZE_UNDEFINED;
            init.mode = nvof5::NV_OF_MODE_OPTICALFLOW;
            init.perfLevel = nvof5::NV_OF_PERF_LEVEL_FAST;
            init.enableExternalHints = nvof5::NV_OF_FALSE;
            init.enableOutputCost = m_nvofCostEnabled ?
                nvof5::NV_OF_TRUE : nvof5::NV_OF_FALSE;
            init.disparityRange = nvof5::NV_OF_STEREO_DISPARITY_RANGE_UNDEFINED;
            init.enableRoi = nvof5::NV_OF_FALSE;
            init.predDirection = nvof5::NV_OF_PRED_DIRECTION_BOTH;
            init.enableGlobalFlow = nvof5::NV_OF_FALSE;
            init.inputBufferFormat = nvof5::NV_OF_BUFFER_FORMAT_ABGR8;
            if (!m_nvofApi.nvOFInit ||
                m_nvofApi.nvOFInit(m_nvofHandle, &init) != nvof5::NV_OF_SUCCESS)
            {
                DestroyOpticalFlow();
                m_nvofUnavailable = true;
                return false;
            }

            D3D11_TEXTURE2D_DESC inputDesc{};
            inputDesc.Width = ofWidth;
            inputDesc.Height = ofHeight;
            inputDesc.MipLevels = 1;
            inputDesc.ArraySize = 1;
            inputDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            inputDesc.SampleDesc.Count = 1;
            inputDesc.Usage = D3D11_USAGE_DEFAULT;
            inputDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            for (size_t i = 0; i < m_nvofInputTextures.size(); ++i)
            {
                if (FAILED(m_device->CreateTexture2D(&inputDesc, nullptr, m_nvofInputTextures[i].put())) ||
                    FAILED(m_device->CreateRenderTargetView(
                        m_nvofInputTextures[i].get(), nullptr, m_nvofInputRTVs[i].put())) ||
                    m_nvofApi.nvOFRegisterResourceD3D11(
                        m_nvofHandle, m_nvofInputTextures[i].get(), &m_nvofInputHandles[i]) != nvof5::NV_OF_SUCCESS)
                {
                    DestroyOpticalFlow();
                    return false;
                }
            }

            m_nvofInputWidth = ofWidth;
            m_nvofInputHeight = ofHeight;
            m_nvofFlowWidth = (ofWidth + m_nvofGridSize - 1) / m_nvofGridSize;
            m_nvofFlowHeight = (ofHeight + m_nvofGridSize - 1) / m_nvofGridSize;
            D3D11_TEXTURE2D_DESC flowDesc{};
            flowDesc.Width = m_nvofFlowWidth;
            flowDesc.Height = m_nvofFlowHeight;
            flowDesc.MipLevels = 1;
            flowDesc.ArraySize = 1;
            flowDesc.Format = DXGI_FORMAT_R16G16_SINT;
            flowDesc.SampleDesc.Count = 1;
            flowDesc.Usage = D3D11_USAGE_DEFAULT;
            flowDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
            if (FAILED(m_device->CreateTexture2D(&flowDesc, nullptr, m_nvofForwardTexture.put())) ||
                FAILED(m_device->CreateTexture2D(&flowDesc, nullptr, m_nvofBackwardTexture.put())) ||
                FAILED(m_device->CreateShaderResourceView(
                    m_nvofForwardTexture.get(), nullptr, m_nvofForwardSRV.put())) ||
                FAILED(m_device->CreateShaderResourceView(
                    m_nvofBackwardTexture.get(), nullptr, m_nvofBackwardSRV.put())) ||
                m_nvofApi.nvOFRegisterResourceD3D11(
                    m_nvofHandle, m_nvofForwardTexture.get(), &m_nvofForwardHandle) != nvof5::NV_OF_SUCCESS ||
                m_nvofApi.nvOFRegisterResourceD3D11(
                    m_nvofHandle, m_nvofBackwardTexture.get(), &m_nvofBackwardHandle) != nvof5::NV_OF_SUCCESS)
            {
                DestroyOpticalFlow();
                m_nvofUnavailable = true;
                return false;
            }

            if (m_nvofCostEnabled)
            {
                D3D11_TEXTURE2D_DESC costDesc = flowDesc;
                costDesc.Format = DXGI_FORMAT_R8_UINT;
                if (FAILED(m_device->CreateTexture2D(
                        &costDesc, nullptr, m_nvofForwardCostTexture.put())) ||
                    FAILED(m_device->CreateTexture2D(
                        &costDesc, nullptr, m_nvofBackwardCostTexture.put())) ||
                    FAILED(m_device->CreateShaderResourceView(
                        m_nvofForwardCostTexture.get(), nullptr, m_nvofForwardCostSRV.put())) ||
                    FAILED(m_device->CreateShaderResourceView(
                        m_nvofBackwardCostTexture.get(), nullptr, m_nvofBackwardCostSRV.put())) ||
                    m_nvofApi.nvOFRegisterResourceD3D11(
                        m_nvofHandle, m_nvofForwardCostTexture.get(),
                        &m_nvofForwardCostHandle) != nvof5::NV_OF_SUCCESS ||
                    m_nvofApi.nvOFRegisterResourceD3D11(
                        m_nvofHandle, m_nvofBackwardCostTexture.get(),
                        &m_nvofBackwardCostHandle) != nvof5::NV_OF_SUCCESS)
                {
                    // Cost is optional. If a driver advertises R8_UINT but rejects
                    // the resource, retry the session without cost instead of
                    // throwing away NVOFA motion classification entirely.
                    m_nvofCostDisabledByRuntime = true;
                    DestroyOpticalFlow();
                    return EnsureOpticalFlow(width, height);
                }
            }

            m_nvofWidth = width;
            m_nvofHeight = height;
            m_nvofPreviousIndex = 0;
            m_nvofPreviousValid = false;
            m_nvofFlowValid = false;
            return true;
        }

        void UpdateOpticalFlow(ID3D11ShaderResourceView* source, bool executeFlow)
        {
            const bool previousExecuteSuccessful = m_nvofLastExecuteSuccessful;
            m_nvofLastExecuteSuccessful = false;
            m_nvofFlowValid = false;
            if (!source || !m_psOpticalFlowCopy) return;

            winrt::com_ptr<ID3D11Resource> resource;
            source->GetResource(resource.put());
            winrt::com_ptr<ID3D11Texture2D> sourceTexture;
            if (!resource || FAILED(resource->QueryInterface(__uuidof(ID3D11Texture2D), sourceTexture.put_void())))
                return;
            D3D11_TEXTURE2D_DESC sourceDesc{};
            sourceTexture->GetDesc(&sourceDesc);
            if (!EnsureOpticalFlow(sourceDesc.Width, sourceDesc.Height)) return;

            const size_t writeIndex = m_nvofPreviousValid ?
                (m_nvofPreviousIndex + 1) % m_nvofInputTextures.size() : 0;
            D3D11_VIEWPORT vp{};
            vp.Width = static_cast<float>(m_nvofInputWidth);
            vp.Height = static_cast<float>(m_nvofInputHeight);
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            m_context->RSSetViewports(1, &vp);
            ID3D11RenderTargetView* inputRtv = m_nvofInputRTVs[writeIndex].get();
            m_context->OMSetRenderTargets(1, &inputRtv, nullptr);
            m_context->IASetInputLayout(nullptr);
            m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_context->VSSetShader(m_vs.get(), nullptr, 0);
            m_context->PSSetShader(m_psOpticalFlowCopy.get(), nullptr, 0);
            m_context->PSSetShaderResources(0, 1, &source);
            ID3D11SamplerState* sampler = m_sampler.get();
            m_context->PSSetSamplers(0, 1, &sampler);
            m_context->Draw(3, 0);
            ID3D11ShaderResourceView* nullSrv = nullptr;
            m_context->PSSetShaderResources(0, 1, &nullSrv);
            ID3D11RenderTargetView* nullRtv = nullptr;
            m_context->OMSetRenderTargets(1, &nullRtv, nullptr);

            if (executeFlow && m_nvofPreviousValid)
            {
                nvof5::NV_OF_EXECUTE_INPUT_PARAMS in{};
                in.inputFrame = m_nvofInputHandles[writeIndex];
                in.referenceFrame = m_nvofInputHandles[m_nvofPreviousIndex];
                // Temporal hints come from the previous NvOFExecute, not from the
                // most recent anchor copy. Our classifier intentionally skips flow
                // solves on ordinary frames, so a solve after any skipped/failed
                // execute must start without a stale hint. Consecutive successful
                // solves may keep temporal hints for quality and speed.
                in.disableTemporalHints = previousExecuteSuccessful ?
                    nvof5::NV_OF_FALSE : nvof5::NV_OF_TRUE;
                nvof5::NV_OF_EXECUTE_OUTPUT_PARAMS out{};
                out.outputBuffer = m_nvofForwardHandle;
                out.bwdOutputBuffer = m_nvofBackwardHandle;
                out.outputCostBuffer = m_nvofCostEnabled ?
                    m_nvofForwardCostHandle : nullptr;
                out.bwdOutputCostBuffer = m_nvofCostEnabled ?
                    m_nvofBackwardCostHandle : nullptr;
                if (m_nvofApi.nvOFExecute &&
                    m_nvofApi.nvOFExecute(m_nvofHandle, &in, &out) == nvof5::NV_OF_SUCCESS)
                {
                    m_nvofFlowValid = true;
                    m_nvofLastExecuteSuccessful = true;
                }
            }
            m_nvofPreviousIndex = writeIndex;
            m_nvofPreviousValid = true;
        }

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
                const float clearHistory[4] = { 0, 0, 0, 0 };
                m_context->ClearRenderTargetView(m_outputHistoryRTVs[i].get(), clearHistory);
                m_context->ClearRenderTargetView(m_sourceHistoryRTVs[i].get(), clearHistory);
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
                m_nvofHandle ?
                    (m_nvofFlowValid ?
                        (m_nvofCostEnabled ? L"NVOFA ACTIVE+COST 0.5x FAST" : L"NVOFA ACTIVE 0.5x FAST") :
                        (m_nvofCostEnabled ? L"NVOFA ANCHOR+COST 0.5x FAST" : L"NVOFA ANCHOR 0.5x FAST")) :
                    L"FALLBACK",
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
                static_cast<unsigned long long>(m_droppedPresents));

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
            c.p3[0] = NowMs() < m_hintUntilMs ? 1.0f : 0.0f;
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
            c.p6[1] = m_latestStats.validDelta ? m_latestStats.cameraMotionScore : 0.0f;
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
            if (useOpticalFlow)
            {
                // Always refresh the half-resolution NVOFA anchor, but avoid paying
                // for a full flow solve on ordinary frames. Sharp high-FPS games
                // (notably Quake 3) otherwise run the optical-flow engine on nearly
                // every desktop update even when temporal protection is inactive.
                const bool detectorChange = m_latestStats.validDelta &&
                    (m_latestStats.affectedArea >= 0.010f ||
                     m_latestStats.strongAffectedArea >= 0.003f ||
                     m_latestStats.flashEnergy >= 0.003f);
                const bool filterActive = std::fabs(protectionGate) > 0.001f ||
                    redGate > 0.001f || overloadFallback || broadLocalTransition;
                const bool coarseCameraMotion = m_latestStats.validDelta &&
                    m_latestStats.cameraMotionScore >=
                        m_safety.cameraMotionSuppression * 0.85f;
                const bool executeFlow = filterActive ||
                    (detectorChange && !coarseCameraMotion);
                UpdateOpticalFlow(source, executeFlow);
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
            ID3D11RenderTargetView* rtvs[] = {
                m_backBufferRTV.get(),
                m_outputHistoryRTVs[historyWriteIndex].get(),
                m_sourceHistoryRTVs[sourceHistoryWriteIndex].get()
            };
            m_context->OMSetRenderTargets(3, rtvs, nullptr);
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
            ID3D11RenderTargetView* nullRTVs[3] = { nullptr, nullptr, nullptr };
            m_context->OMSetRenderTargets(3, nullRTVs, nullptr);
            m_outputHistoryIndex = historyWriteIndex;
            m_outputHistoryValid = true;
            m_sourceHistoryIndex = sourceHistoryWriteIndex;
            m_sourceHistoryValid = true;
            // Desktop Duplication already paces capture to desktop updates. Queue
            // this frame immediately but do not discard it: DO_NOT_WAIT produced
            // visible motion judder whenever the compositor was briefly busy.
            const auto presentStart = std::chrono::steady_clock::now();
            const HRESULT presentHr = m_swapChain->Present(0, 0);
            m_presentCallMs.store(std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - presentStart).count(),
                std::memory_order_release);
            ThrowIfFailed(presentHr);
        }

        void QueueCapturedFrame(ID3D11Texture2D* source, float dt)
        {
            m_instantFrameDt = std::clamp(dt, 1.0f / 240.0f, 0.05f);
            // Keep feedback rendering alive briefly if the desktop becomes static.
            // This is long enough for the fast release path to converge even after
            // a near full-range protected transition.
            m_idleReleaseUntilMs.store(NowMs() + 500, std::memory_order_release);
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
            m_outputHistoryValid = false;
            m_sourceHistoryValid = false;
            if (m_swapChain) m_swapChain->Present(1, 0);
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
        float m_contrastReduction = 2.0f / 3.0f;
        int m_profilePreset = 1;
        int m_fullScreenSensitivity = 1;
        int m_smallSourceSensitivity = 1;
        int m_displaySizePreset = 1;
        int m_viewingDistancePreset = 1;
        std::mutex m_mutex;
        std::thread m_captureThread;
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
        winrt::com_ptr<ID3D11Texture2D> m_backBuffer;
        winrt::com_ptr<ID3D11RenderTargetView> m_backBufferRTV;
        std::array<winrt::com_ptr<ID3D11Texture2D>, 2> m_outputHistoryTextures;
        std::array<winrt::com_ptr<ID3D11RenderTargetView>, 2> m_outputHistoryRTVs;
        std::array<winrt::com_ptr<ID3D11ShaderResourceView>, 2> m_outputHistorySRVs;
        size_t m_outputHistoryIndex = 0;
        bool m_outputHistoryValid = false;
        bool m_replayMode = false;
        winrt::com_ptr<ID3D11Texture2D> m_replayReadback;
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
        winrt::com_ptr<ID3D11ShaderResourceView> m_nvofForwardCostSRV;
        winrt::com_ptr<ID3D11ShaderResourceView> m_nvofBackwardCostSRV;
        nvof5::NvOFGPUBufferHandle m_nvofForwardHandle = nullptr;
        nvof5::NvOFGPUBufferHandle m_nvofBackwardHandle = nullptr;
        nvof5::NvOFGPUBufferHandle m_nvofForwardCostHandle = nullptr;
        nvof5::NvOFGPUBufferHandle m_nvofBackwardCostHandle = nullptr;
        bool m_nvofCostEnabled = false;
        bool m_nvofCostDisabledByRuntime = false;
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
        uint64_t m_droppedPresents = 0;
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
            L"Maximum - 50 ms predictive"
        };
        for (const auto* item : profileItems)
            SendMessageW(profile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        SendMessageW(profile, CB_SETCURSEL, options.profilePreset, 0);
        AddSettingsTooltip(profile,
            L"Loads a coordinated starting point. Performance changes the least; Balanced uses instant GPU protection with reduced contrast; Maximum adds 50 ms prediction for stronger protection. Click Apply to save.");
        AddSettingsTooltip(profileLabel,
            L"Loads a coordinated starting point. Performance changes the least; Balanced uses instant GPU protection with reduced contrast; Maximum adds 50 ms prediction for stronger protection. Click Apply to save.");

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

        HWND latencyLabel = AddSettingsControl(hwnd, 0, L"STATIC", L"Maximum look-ahead", WS_CHILD | WS_VISIBLE | SS_NOTIFY,
            28, 240, 200, 20, 0);
        HWND latency = AddSettingsControl(hwnd, 0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            245, 234, 285, 170, kControlLatency);
        const wchar_t* latencyItems[] = {
            L"Instant GPU (recommended)",
            L"25 ms - lowest automatic latency", L"33 ms", L"50 ms - balanced",
            L"67 ms", L"100 ms - strongest prediction"
        };
        for (const auto* item : latencyItems)
            SendMessageW(latency, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        constexpr int latencyValues[] = { 0, 25, 33, 50, 67, 100 };
        int latencySelection = 3;
        for (int i = 0; i < 6; ++i) if (options.latencyMs == latencyValues[i]) latencySelection = i;
        SendMessageW(latency, CB_SETCURSEL, latencySelection, 0);
        AddSettingsTooltip(latency,
            L"Instant GPU compares the current and previous analysis frames with no intentional queue. Look-ahead modes delay the image by the selected time so future reversals can confirm ambiguous flashes.");
        AddSettingsTooltip(latencyLabel,
            L"Instant GPU compares the current and previous analysis frames with no intentional queue. Look-ahead modes delay the image by the selected time so future reversals can confirm ambiguous flashes.");

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
            SendDlgItemMessageW(hwnd, kControlLatency, CB_SETCURSEL, 3, 0);
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
        constexpr int latencyValues[] = { 0, 25, 33, 50, 67, 100 };
        const int latencyIndex = std::clamp(static_cast<int>(SendDlgItemMessageW(
            hwnd, kControlLatency, CB_GETCURSEL, 0, 0)), 0, 5);
        options.latencyMs = latencyValues[latencyIndex];
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

    HWND CreateReplayWindow(HINSTANCE instance, HMONITOR mon)
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
        return CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClass,
            L"FlashGuard Synthetic Replay", WS_POPUP,
            mi.rcMonitor.left, mi.rcMonitor.top, 640, 360,
            nullptr, nullptr, instance, nullptr);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    try
    {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_WIN95_CLASSES };
        InitCommonControlsEx(&controls);
        if (HasCommandLineFlag(L"--validate-shaders"))
            return ValidateShaderSource() ? 0 : 2;

        const std::wstring replayReport = ParseArgumentValue(L"--synthetic-replay");
        const std::wstring replayVisualDir =
            ParseArgumentValue(L"--synthetic-replay-visual");
        if (!replayReport.empty())
        {
            POINT origin{ 0, 0 };
            HMONITOR monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
            HWND replayWindow = CreateReplayWindow(instance, monitor);
            if (!replayWindow) return 6;
            try
            {
                FlashGuardApp app;
                RuntimeOptions replayOptions{};
                replayOptions.profilePreset = 1;
                replayOptions.contrastReduction = 0.0f;
                replayOptions.latencyMs = 0;
                replayOptions.debugOverlay = false;
                app.ApplyRuntimeOptions(replayOptions);
                app.InitializeReplay(replayWindow, monitor);
                const bool passed = app.RunSyntheticReplay(replayReport, replayVisualDir);
                app.Stop();
                DestroyWindow(replayWindow);
                return passed ? 0 : 7;
            }
            catch (...)
            {
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
        HWND output = CreateOutputWindow(instance, monitor);
        if (!output)
        {
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
        app.Initialize(output, monitor);

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
            app.Stop();
            g_app = nullptr;
            DestroyWindow(output);
            MessageBoxW(nullptr,
                L"FlashGuard did not receive enough desktop frames during startup.\n\n"
                L"The fullscreen overlay was NOT enabled. Make sure the selected monitor is active and not locked.",
                L"FlashGuard - capture not ready", MB_ICONWARNING | MB_OK);
            return 5;
        }

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
        wchar_t buf[256]{};
        swprintf_s(buf, L"FlashGuard failed with HRESULT 0x%08X.\n\nThe filter was not started. Do not assume the screen is protected.", static_cast<unsigned>(hr));
        MessageBoxW(nullptr, buf, L"FlashGuard", MB_ICONERROR | MB_OK);
        return static_cast<int>(hr);
    }
    catch (...)
    {
        MessageBoxW(nullptr,
            L"FlashGuard failed unexpectedly. The display filter was not started. Do not assume the screen is protected.",
            L"FlashGuard", MB_ICONERROR | MB_OK);
        return 1;
    }
}
