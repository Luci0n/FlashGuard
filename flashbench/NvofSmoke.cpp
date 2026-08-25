#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

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

    struct NV_OF_ROI_RECT { uint32_t start_x, start_y, width, height; };
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
    struct SmokeReport
    {
        std::wstring path;
        const char* status = "FAILED";
        const char* stage = "startup";
        int code = 1;
        uint32_t grid = 0;
        uint64_t nonzero = 0;
        uint64_t total = 0;
        double meanAbsFlow = 0.0;

        void Write() const
        {
            FILE* f = nullptr;
            if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) return;
            std::fprintf(f,
                "{\n"
                "  \"schema\": \"NVOF_SMOKE/1\",\n"
                "  \"status\": \"%s\",\n"
                "  \"stage\": \"%s\",\n"
                "  \"code\": %d,\n"
                "  \"grid\": %u,\n"
                "  \"nonzero_vectors\": %llu,\n"
                "  \"total_vectors\": %llu,\n"
                "  \"mean_abs_flow_pixels\": %.6f\n"
                "}\n",
                status, stage, code, grid,
                static_cast<unsigned long long>(nonzero),
                static_cast<unsigned long long>(total), meanAbsFlow);
            std::fclose(f);
        }
    };

    struct NvofSession
    {
        HMODULE module = nullptr;
        nvof5::NV_OF_D3D11_API_FUNCTION_LIST api{};
        nvof5::NvOFHandle handle = nullptr;
        std::array<nvof5::NvOFGPUBufferHandle, 4> buffers{};

        ~NvofSession()
        {
            if (api.nvOFUnregisterResourceD3D11)
                for (auto& b : buffers)
                    if (b) api.nvOFUnregisterResourceD3D11(b);
            if (handle && api.nvOFDestroy) api.nvOFDestroy(handle);
            if (module) FreeLibrary(module);
        }
    };

    uint32_t PatternPixel(int x, int y)
    {
        const uint32_t h = static_cast<uint32_t>(x * 73856093u) ^
                           static_cast<uint32_t>(y * 19349663u) ^
                           static_cast<uint32_t>((x / 8 + y / 8) * 83492791u);
        const uint8_t b = static_cast<uint8_t>(32u + (h & 0xBFu));
        const uint8_t g = static_cast<uint8_t>(32u + ((h >> 8) & 0xBFu));
        const uint8_t r = static_cast<uint8_t>(32u + ((h >> 16) & 0xBFu));
        return static_cast<uint32_t>(b) |
               (static_cast<uint32_t>(g) << 8) |
               (static_cast<uint32_t>(r) << 16) | 0xFF000000u;
    }

    bool HasFormat(const NvofSession& s, nvof5::NV_OF_BUFFER_USAGE usage, DXGI_FORMAT wanted)
    {
        uint32_t count = 0;
        if (!s.api.nvOFGetSurfaceFormatCountD3D11 ||
            s.api.nvOFGetSurfaceFormatCountD3D11(
                s.handle, usage, nvof5::NV_OF_MODE_OPTICALFLOW, &count) != nvof5::NV_OF_SUCCESS ||
            count == 0)
            return false;
        std::vector<DXGI_FORMAT> formats(count);
        if (s.api.nvOFGetSurfaceFormatD3D11(
                s.handle, usage, nvof5::NV_OF_MODE_OPTICALFLOW, formats.data()) != nvof5::NV_OF_SUCCESS)
            return false;
        return std::find(formats.begin(), formats.end(), wanted) != formats.end();
    }
}

int wmain(int argc, wchar_t** argv)
{
    SmokeReport report;
    report.path = argc > 1 ? argv[1] : L"nvof-smoke.json";
    const auto fail = [&report](const char* stage, int code) {
        report.stage = stage;
        report.code = code;
        report.Write();
        std::fprintf(stderr, "NVOF smoke failed at %s (%d)\n", stage, code);
        return code;
    };

    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return fail("CreateDXGIFactory1", 10);

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; ; ++i)
    {
        ComPtr<IDXGIAdapter1> candidate;
        if (factory->EnumAdapters1(i, &candidate) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(candidate->GetDesc1(&desc)) && desc.VendorId == 0x10DE &&
            (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
        {
            adapter = candidate;
            break;
        }
    }
    if (!adapter) return fail("FindNvidiaAdapter", 11);

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL created{};
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    if (FAILED(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, static_cast<UINT>(std::size(levels)),
        D3D11_SDK_VERSION, &device, &created, &context)))
        return fail("D3D11CreateDevice", 12);

    NvofSession session;
    session.module = LoadLibraryW(L"nvofapi64.dll");
    if (!session.module) return fail("LoadLibrary-nvofapi64", 20);
    auto createInstance = reinterpret_cast<nvof5::PFNNVOFAPICREATEINSTANCED3D11>(
        GetProcAddress(session.module, "NvOFAPICreateInstanceD3D11"));
    if (!createInstance) return fail("GetProcAddress-NvOFAPICreateInstanceD3D11", 21);
    if (createInstance(nvof5::kApiVersion, &session.api) != nvof5::NV_OF_SUCCESS)
        return fail("NvOFAPICreateInstanceD3D11", 22);
    if (!session.api.nvCreateOpticalFlowD3D11 ||
        session.api.nvCreateOpticalFlowD3D11(device.Get(), context.Get(), &session.handle) != nvof5::NV_OF_SUCCESS ||
        !session.handle)
        return fail("nvCreateOpticalFlowD3D11", 23);

    if (!HasFormat(session, nvof5::NV_OF_BUFFER_USAGE_INPUT, DXGI_FORMAT_B8G8R8A8_UNORM))
        return fail("InputFormat-B8G8R8A8", 24);
    if (!HasFormat(session, nvof5::NV_OF_BUFFER_USAGE_OUTPUT, DXGI_FORMAT_R16G16_SINT))
        return fail("OutputFormat-R16G16_SINT", 25);

    uint32_t gridCount = 0;
    if (!session.api.nvOFGetCaps ||
        session.api.nvOFGetCaps(session.handle, nvof5::NV_OF_CAPS_SUPPORTED_OUTPUT_GRID_SIZES,
            nullptr, &gridCount) != nvof5::NV_OF_SUCCESS || gridCount == 0)
        return fail("nvOFGetCaps-count", 26);
    std::vector<uint32_t> grids(gridCount);
    if (session.api.nvOFGetCaps(session.handle, nvof5::NV_OF_CAPS_SUPPORTED_OUTPUT_GRID_SIZES,
        grids.data(), &gridCount) != nvof5::NV_OF_SUCCESS)
        return fail("nvOFGetCaps-data", 27);
    const auto supports = [&grids](uint32_t g) {
        return std::find(grids.begin(), grids.end(), g) != grids.end();
    };
    const uint32_t grid = supports(1) ? 1u : (supports(2) ? 2u : (supports(4) ? 4u : 0u));
    if (!grid) return fail("NoSupportedGrid", 28);
    report.grid = grid;

    constexpr UINT width = 320;
    constexpr UINT height = 180;
    nvof5::NV_OF_INIT_PARAMS init{};
    init.width = width;
    init.height = height;
    init.outGridSize = static_cast<nvof5::NV_OF_OUTPUT_VECTOR_GRID_SIZE>(grid);
    init.hintGridSize = nvof5::NV_OF_HINT_VECTOR_GRID_SIZE_UNDEFINED;
    init.mode = nvof5::NV_OF_MODE_OPTICALFLOW;
    init.perfLevel = nvof5::NV_OF_PERF_LEVEL_FAST;
    init.enableExternalHints = nvof5::NV_OF_FALSE;
    init.enableOutputCost = nvof5::NV_OF_FALSE;
    init.disparityRange = nvof5::NV_OF_STEREO_DISPARITY_RANGE_UNDEFINED;
    init.enableRoi = nvof5::NV_OF_FALSE;
    init.predDirection = nvof5::NV_OF_PRED_DIRECTION_BOTH;
    init.enableGlobalFlow = nvof5::NV_OF_FALSE;
    init.inputBufferFormat = nvof5::NV_OF_BUFFER_FORMAT_ABGR8;
    if (!session.api.nvOFInit || session.api.nvOFInit(session.handle, &init) != nvof5::NV_OF_SUCCESS)
        return fail("nvOFInit", 30);

    std::vector<uint32_t> previous(static_cast<size_t>(width) * height);
    std::vector<uint32_t> current(previous.size());
    constexpr int shift = 8;
    for (UINT y = 0; y < height; ++y)
    for (UINT x = 0; x < width; ++x)
    {
        previous[static_cast<size_t>(y) * width + x] = PatternPixel(static_cast<int>(x), static_cast<int>(y));
        const int sourceX = (static_cast<int>(x) - shift + static_cast<int>(width)) % static_cast<int>(width);
        current[static_cast<size_t>(y) * width + x] = PatternPixel(sourceX, static_cast<int>(y));
    }

    D3D11_TEXTURE2D_DESC inputDesc{};
    inputDesc.Width = width;
    inputDesc.Height = height;
    inputDesc.MipLevels = 1;
    inputDesc.ArraySize = 1;
    inputDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    inputDesc.SampleDesc.Count = 1;
    inputDesc.Usage = D3D11_USAGE_DEFAULT;
    inputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA previousData{ previous.data(), width * 4u, 0 };
    D3D11_SUBRESOURCE_DATA currentData{ current.data(), width * 4u, 0 };
    ComPtr<ID3D11Texture2D> previousTex, currentTex;
    if (FAILED(device->CreateTexture2D(&inputDesc, &previousData, &previousTex)) ||
        FAILED(device->CreateTexture2D(&inputDesc, &currentData, &currentTex)))
        return fail("CreateInputTextures", 31);

    const UINT flowWidth = (width + grid - 1) / grid;
    const UINT flowHeight = (height + grid - 1) / grid;
    D3D11_TEXTURE2D_DESC flowDesc{};
    flowDesc.Width = flowWidth;
    flowDesc.Height = flowHeight;
    flowDesc.MipLevels = 1;
    flowDesc.ArraySize = 1;
    flowDesc.Format = DXGI_FORMAT_R16G16_SINT;
    flowDesc.SampleDesc.Count = 1;
    flowDesc.Usage = D3D11_USAGE_DEFAULT;
    flowDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    ComPtr<ID3D11Texture2D> forwardTex, backwardTex;
    ComPtr<ID3D11UnorderedAccessView> forwardUav, backwardUav;
    if (FAILED(device->CreateTexture2D(&flowDesc, nullptr, &forwardTex)) ||
        FAILED(device->CreateTexture2D(&flowDesc, nullptr, &backwardTex)) ||
        FAILED(device->CreateUnorderedAccessView(forwardTex.Get(), nullptr, &forwardUav)) ||
        FAILED(device->CreateUnorderedAccessView(backwardTex.Get(), nullptr, &backwardUav)))
        return fail("CreateFlowTextures", 32);
    const UINT clearValue[4] = { 0, 0, 0, 0 };
    context->ClearUnorderedAccessViewUint(forwardUav.Get(), clearValue);
    context->ClearUnorderedAccessViewUint(backwardUav.Get(), clearValue);

    if (!session.api.nvOFRegisterResourceD3D11 ||
        session.api.nvOFRegisterResourceD3D11(session.handle, currentTex.Get(), &session.buffers[0]) != nvof5::NV_OF_SUCCESS ||
        session.api.nvOFRegisterResourceD3D11(session.handle, previousTex.Get(), &session.buffers[1]) != nvof5::NV_OF_SUCCESS ||
        session.api.nvOFRegisterResourceD3D11(session.handle, forwardTex.Get(), &session.buffers[2]) != nvof5::NV_OF_SUCCESS ||
        session.api.nvOFRegisterResourceD3D11(session.handle, backwardTex.Get(), &session.buffers[3]) != nvof5::NV_OF_SUCCESS)
        return fail("nvOFRegisterResourceD3D11", 33);

    nvof5::NV_OF_EXECUTE_INPUT_PARAMS in{};
    in.inputFrame = session.buffers[0];
    in.referenceFrame = session.buffers[1];
    in.disableTemporalHints = nvof5::NV_OF_TRUE;
    nvof5::NV_OF_EXECUTE_OUTPUT_PARAMS out{};
    out.outputBuffer = session.buffers[2];
    out.bwdOutputBuffer = session.buffers[3];
    if (!session.api.nvOFExecute || session.api.nvOFExecute(session.handle, &in, &out) != nvof5::NV_OF_SUCCESS)
        return fail("nvOFExecute", 40);

    D3D11_TEXTURE2D_DESC stagingDesc = flowDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&stagingDesc, nullptr, &staging)))
        return fail("CreateStaging", 41);
    context->CopyResource(staging.Get(), forwardTex.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
        return fail("MapFlow", 42);

    uint64_t nonzero = 0;
    uint64_t total = static_cast<uint64_t>(flowWidth) * flowHeight;
    double sumAbs = 0.0;
    for (UINT y = 0; y < flowHeight; ++y)
    {
        const auto* row = reinterpret_cast<const int16_t*>(
            static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch);
        for (UINT x = 0; x < flowWidth; ++x)
        {
            const double fx = static_cast<double>(row[x * 2 + 0]) / 32.0;
            const double fy = static_cast<double>(row[x * 2 + 1]) / 32.0;
            const double magnitude = std::sqrt(fx * fx + fy * fy);
            sumAbs += magnitude;
            if (magnitude >= 0.25) ++nonzero;
        }
    }
    context->Unmap(staging.Get(), 0);

    report.nonzero = nonzero;
    report.total = total;
    report.meanAbsFlow = total ? sumAbs / static_cast<double>(total) : 0.0;
    if (total == 0 || nonzero < std::max<uint64_t>(8u, total / 100u))
        return fail("FlowFieldStayedZero", 43);

    report.status = "SUCCESS";
    report.stage = "complete";
    report.code = 0;
    report.Write();
    std::printf("NVOF smoke success: grid=%u nonzero=%llu/%llu mean_abs=%.3f px\n",
        grid, static_cast<unsigned long long>(nonzero),
        static_cast<unsigned long long>(total), report.meanAbsFlow);
    return 0;
}
