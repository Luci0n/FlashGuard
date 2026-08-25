#pragma once

#include <cstdint>
#include <d3d11.h>

// Minimal NVIDIA Optical Flow SDK 5.0 ABI declarations used by FlashGuard.
// The runtime is loaded dynamically from the NVIDIA display driver, so this
// remains dependency-light with no NVIDIA import library or CUDA dependency.
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
