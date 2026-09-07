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
                if (m_nvofGlobalFlowHandle)
                    m_nvofApi.nvOFUnregisterResourceD3D11(m_nvofGlobalFlowHandle);
            }
            m_nvofForwardHandle = nullptr;
            m_nvofBackwardHandle = nullptr;
            m_nvofForwardCostHandle = nullptr;
            m_nvofBackwardCostHandle = nullptr;
            m_nvofGlobalFlowHandle = nullptr;
            m_nvofForwardSRV = nullptr;
            m_nvofBackwardSRV = nullptr;
            m_nvofForwardCostSRV = nullptr;
            m_nvofBackwardCostSRV = nullptr;
            m_nvofGlobalFlowSRV = nullptr;
            m_nvofForwardTexture = nullptr;
            m_nvofBackwardTexture = nullptr;
            m_nvofForwardCostTexture = nullptr;
            m_nvofBackwardCostTexture = nullptr;
            m_nvofGlobalFlowTexture = nullptr;
            m_nvofCostEnabled = false;
            m_nvofGlobalFlowEnabled = false;
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
            // The lite profile keeps enough spatial detail for object boundaries
            // while reducing the amount of vector data consumed by PSMain. At
            // half-resolution input, grid 2 is roughly 4x4 full-resolution motion.
            if (g_liveNvofLiteForLatencyTest && !m_replayMode)
                m_nvofGridSize = supportsGrid(2) ? 2u :
                    (supportsGrid(4) ? 4u : (supportsGrid(1) ? 1u : 0u));
            else
                m_nvofGridSize = supportsGrid(1) ? 1u :
                    (supportsGrid(2) ? 2u : (supportsGrid(4) ? 4u : 0u));
            if (!m_nvofGridSize)
            {
                DestroyOpticalFlow();
                m_nvofUnavailable = true;
                return false;
            }

            // Use NVOFA's native 8-bit cost/confidence when exposed by the
            // D3D11 driver. NVIDIA recommends this over legacy 32-bit cost.
            const bool liteProfile =
                g_liveNvofLiteForLatencyTest && !m_replayMode;
            m_nvofCostEnabled = !liteProfile &&
                !m_nvofCostDisabledByRuntime && NvofHasFormat(
                nvof5::NV_OF_BUFFER_USAGE_COST, DXGI_FORMAT_R8_UINT);
            m_nvofGlobalFlowEnabled = !liteProfile &&
                !m_nvofGlobalFlowDisabledByRuntime &&
                NvofHasFormat(nvof5::NV_OF_BUFFER_USAGE_GLOBAL_FLOW,
                    DXGI_FORMAT_R16G16_SINT);

            nvof5::NV_OF_INIT_PARAMS init{};
            init.width = ofWidth;
            init.height = ofHeight;
            init.outGridSize = static_cast<nvof5::NV_OF_OUTPUT_VECTOR_GRID_SIZE>(m_nvofGridSize);
            init.hintGridSize = nvof5::NV_OF_HINT_VECTOR_GRID_SIZE_UNDEFINED;
            init.mode = nvof5::NV_OF_MODE_OPTICALFLOW;
            init.perfLevel = liteProfile ?
                nvof5::NV_OF_PERF_LEVEL_FAST :
                nvof5::NV_OF_PERF_LEVEL_MEDIUM;
            init.enableExternalHints = nvof5::NV_OF_FALSE;
            init.enableOutputCost = m_nvofCostEnabled ?
                nvof5::NV_OF_TRUE : nvof5::NV_OF_FALSE;
            init.disparityRange = nvof5::NV_OF_STEREO_DISPARITY_RANGE_UNDEFINED;
            init.enableRoi = nvof5::NV_OF_FALSE;
            init.predDirection = nvof5::NV_OF_PRED_DIRECTION_BOTH;
            init.enableGlobalFlow = m_nvofGlobalFlowEnabled ?
                nvof5::NV_OF_TRUE : nvof5::NV_OF_FALSE;
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

            if (m_nvofGlobalFlowEnabled)
            {
                D3D11_TEXTURE2D_DESC globalDesc = flowDesc;
                // NVIDIA global flow is one vector for the complete frame.
                globalDesc.Width = 1;
                globalDesc.Height = 1;
                if (FAILED(m_device->CreateTexture2D(
                        &globalDesc, nullptr, m_nvofGlobalFlowTexture.put())) ||
                    FAILED(m_device->CreateShaderResourceView(
                        m_nvofGlobalFlowTexture.get(), nullptr,
                        m_nvofGlobalFlowSRV.put())) ||
                    m_nvofApi.nvOFRegisterResourceD3D11(
                        m_nvofHandle, m_nvofGlobalFlowTexture.get(),
                        &m_nvofGlobalFlowHandle) != nvof5::NV_OF_SUCCESS)
                {
                    // Global flow is an optional quality path. Retry without it if
                    // a driver advertises the format but rejects the one-vector
                    // resource instead of disabling NVOFA completely.
                    m_nvofGlobalFlowDisabledByRuntime = true;
                    DestroyOpticalFlow();
                    return EnsureOpticalFlow(width, height);
                }
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
                in.disableTemporalHints =
                    (g_replayDisableNvofTemporalHints || !previousExecuteSuccessful) ?
                        nvof5::NV_OF_TRUE : nvof5::NV_OF_FALSE;
                nvof5::NV_OF_EXECUTE_OUTPUT_PARAMS out{};
                out.outputBuffer = m_nvofForwardHandle;
                out.bwdOutputBuffer = m_nvofBackwardHandle;
                out.outputCostBuffer = m_nvofCostEnabled ?
                    m_nvofForwardCostHandle : nullptr;
                out.bwdOutputCostBuffer = m_nvofCostEnabled ?
                    m_nvofBackwardCostHandle : nullptr;
                out.globalFlowBuffer = m_nvofGlobalFlowEnabled ?
                    m_nvofGlobalFlowHandle : nullptr;
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
