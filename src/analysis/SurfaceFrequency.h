#pragma once

// A self-contained GPU path shared by live capture and replay. State belongs to
// validated predecessor cells, while presentation samples only the newest image.
class SurfaceFrequency
{
    static constexpr char Shader[] =
#include "../shaders/SurfaceFrequency.inl"
    ;
    struct Plane
    {
        winrt::com_ptr<ID3D11Texture2D> texture;
        winrt::com_ptr<ID3D11RenderTargetView> rtv;
        winrt::com_ptr<ID3D11ShaderResourceView> srv;
    };
    struct Timing
    {
        winrt::com_ptr<ID3D11Query> disjoint;
        std::array<winrt::com_ptr<ID3D11Query>, 4> stamp;
        bool pending = false;
    };
    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> context;
    winrt::com_ptr<ID3D11VertexShader> vs;
    std::array<winrt::com_ptr<ID3D11PixelShader>, 3> ps;
    winrt::com_ptr<ID3D11ComputeShader> activityShader;
    winrt::com_ptr<ID3D11Texture2D> activity;
    winrt::com_ptr<ID3D11ShaderResourceView> activitySrv;
    winrt::com_ptr<ID3D11UnorderedAccessView> activityUav;
    winrt::com_ptr<ID3D11SamplerState> sampler;
    winrt::com_ptr<ID3D11Buffer> constants;
    winrt::com_ptr<ID3D11Texture2D> raw;
    winrt::com_ptr<ID3D11ShaderResourceView> rawSrv;
    std::array<Plane, 2> features;
    std::array<std::array<Plane, 5>, 2> state;
    std::array<Timing, 12> timing;
    size_t timingIndex = 0;
    std::array<std::vector<double>, 4> gpuMs;
    UINT width = 0, height = 0, gridWidth = 0, gridHeight = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    size_t index = 0;
    bool valid = false;
    float idleElapsed = 0;
    uint64_t observations = 0;

    static winrt::com_ptr<ID3DBlob> Compile(const char* entry, const char* target)
    {
        winrt::com_ptr<ID3DBlob> blob, errors;
        const HRESULT hr = fg_shader_cache::Compile(Shader, sizeof(Shader) - 1,
            "SurfaceFrequency", nullptr, nullptr, entry, target,
            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, blob.put(), errors.put());
        if (FAILED(hr) && errors)
            std::fprintf(stderr, "%s\n", static_cast<const char*>(errors->GetBufferPointer()));
        winrt::check_hresult(hr);
        return blob;
    }
    void MakePlane(Plane& plane)
    {
        plane = {};
        D3D11_TEXTURE2D_DESC d{};
        d.Width = gridWidth; d.Height = gridHeight;
        d.MipLevels = d.ArraySize = d.SampleDesc.Count = 1;
        d.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        winrt::check_hresult(device->CreateTexture2D(&d, nullptr, plane.texture.put()));
        winrt::check_hresult(device->CreateRenderTargetView(plane.texture.get(), nullptr, plane.rtv.put()));
        winrt::check_hresult(device->CreateShaderResourceView(plane.texture.get(), nullptr, plane.srv.put()));
    }
    void EnsureSize(ID3D11Texture2D* source)
    {
        D3D11_TEXTURE2D_DESC d{};
        source->GetDesc(&d);
        if (raw && width == d.Width && height == d.Height && format == d.Format) return;
        width = d.Width; height = d.Height; format = d.Format;
        gridWidth = (width + 1) / 2; gridHeight = (height + 1) / 2;
        raw = nullptr; rawSrv = nullptr;
        d.MipLevels = d.ArraySize = d.SampleDesc.Count = 1;
        d.SampleDesc.Quality = 0;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        d.CPUAccessFlags = d.MiscFlags = 0;
        winrt::check_hresult(device->CreateTexture2D(&d, nullptr, raw.put()));
        winrt::check_hresult(device->CreateShaderResourceView(raw.get(), nullptr, rawSrv.put()));
        for (auto& plane : features) MakePlane(plane);
        for (auto& frame : state) for (auto& plane : frame) MakePlane(plane);
        Reset();
    }
    void Unbind()
    {
        ID3D11ShaderResourceView* empty[9]{};
        context->PSSetShaderResources(0, 9, empty);
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }
    void Viewport(UINT w, UINT h)
    {
        D3D11_VIEWPORT v{};
        v.Width = static_cast<float>(w); v.Height = static_cast<float>(h); v.MaxDepth = 1;
        context->RSSetViewports(1, &v);
    }
    void PollTiming()
    {
        for (auto& t : timing)
        {
            if (!t.pending) continue;
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d{};
            if (context->GetData(t.disjoint.get(), &d, sizeof(d), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
                continue;
            std::array<UINT64, 4> ticks{};
            bool ready = true;
            for (size_t k = 0; k < ticks.size(); ++k)
                ready &= context->GetData(t.stamp[k].get(), &ticks[k], sizeof(UINT64),
                    D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
            if (!ready) continue;
            if (!d.Disjoint && d.Frequency)
                for (size_t k = 0; k < gpuMs.size(); ++k)
                {
                    if (gpuMs[k].size() == 4096) gpuMs[k].erase(gpuMs[k].begin());
                    const UINT64 elapsed = k == 3 ? ticks[3] - ticks[0] : ticks[k + 1] - ticks[k];
                    gpuMs[k].push_back(1000.0 * static_cast<double>(elapsed) /
                        static_cast<double>(d.Frequency));
                }
            t.pending = false;
        }
    }
public:
    static bool SelfTest(const std::filesystem::path& path, int onlyScene = -1);
    static bool Benchmark(const std::filesystem::path& path);
    static bool LiveProbe(const std::filesystem::path& directory, float contrast, bool replayState = false);
    double LastGpuMs() const { return gpuMs[3].empty() ? 0.0 : gpuMs[3].back(); }
    static bool Validate()
    {
        Compile("VS", "vs_5_0");
        for (const char* entry : { "FeaturePS", "StatePS", "CompositePS" }) Compile(entry, "ps_5_0");
        Compile("ActivityCS", "cs_5_0");
        return true;
    }
    void Initialize(ID3D11Device* d, ID3D11DeviceContext* c)
    {
        if (device.get() == d && vs) return;
        *this = SurfaceFrequency{};
        device.copy_from(d); context.copy_from(c);
        auto vertex = Compile("VS", "vs_5_0");
        winrt::check_hresult(device->CreateVertexShader(vertex->GetBufferPointer(), vertex->GetBufferSize(), nullptr, vs.put()));
        const char* entries[] = { "FeaturePS", "StatePS", "CompositePS" };
        for (size_t k = 0; k < ps.size(); ++k)
        {
            auto blob = Compile(entries[k], "ps_5_0");
            winrt::check_hresult(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, ps[k].put()));
        }
        auto compute = Compile("ActivityCS", "cs_5_0");
        winrt::check_hresult(device->CreateComputeShader(compute->GetBufferPointer(), compute->GetBufferSize(), nullptr, activityShader.put()));
        D3D11_TEXTURE2D_DESC ad{};
        ad.Width = ad.Height = ad.MipLevels = ad.ArraySize = ad.SampleDesc.Count = 1;
        ad.Format = DXGI_FORMAT_R32_UINT;
        ad.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        winrt::check_hresult(device->CreateTexture2D(&ad, nullptr, activity.put()));
        winrt::check_hresult(device->CreateShaderResourceView(activity.get(), nullptr, activitySrv.put()));
        winrt::check_hresult(device->CreateUnorderedAccessView(activity.get(), nullptr, activityUav.put()));
        D3D11_SAMPLER_DESC s{};
        s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        s.MaxLOD = D3D11_FLOAT32_MAX;
        winrt::check_hresult(device->CreateSamplerState(&s, sampler.put()));
        D3D11_BUFFER_DESC b{};
        b.ByteWidth = 64; b.Usage = D3D11_USAGE_DYNAMIC;
        b.BindFlags = D3D11_BIND_CONSTANT_BUFFER; b.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        winrt::check_hresult(device->CreateBuffer(&b, nullptr, constants.put()));
        for (auto& t : timing)
        {
            D3D11_QUERY_DESC q{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
            winrt::check_hresult(device->CreateQuery(&q, t.disjoint.put()));
            q.Query = D3D11_QUERY_TIMESTAMP;
            for (auto& stamp : t.stamp) winrt::check_hresult(device->CreateQuery(&q, stamp.put()));
        }
    }
    void Reset()
    {
        valid = false; index = 0; idleElapsed = 0;
        if (!context) return;
        const float zero[4]{};
        for (auto& p : features) if (p.rtv) context->ClearRenderTargetView(p.rtv.get(), zero);
        for (auto& frame : state) for (auto& p : frame)
            if (p.rtv) context->ClearRenderTargetView(p.rtv.get(), zero);
    }
    bool HasSource() const { return valid && raw; }
    void Render(ID3D11Texture2D* source, ID3D11RenderTargetView* output,
        float dt, bool staticMap, float floor, float ceiling,
        ID3D11ShaderResourceView* debug = nullptr)
    {
        const bool idle = source == nullptr;
        if (idle && !HasSource()) return;
        if (!idle) EnsureSize(source);
        Unbind();
        PollTiming();
        if (dt > .25f || !std::isfinite(dt) || dt <= 0) Reset();
        dt = std::isfinite(dt) ? std::clamp(dt, .001f, .25f) : 1.f / 60;
        if (idle) idleElapsed += dt;
        else
        {
            // Capture dt includes time already advanced by idle-release draws.
            dt = std::max(0.f, dt - idleElapsed);
            idleElapsed = 0;
        }
        if (!idle) { context->CopyResource(raw.get(), source); ++observations; }
        const size_t next = 1 - index;
        const auto linear = [](float x) { return x <= .04045f ? x / 12.92f : std::pow((x + .055f) / 1.055f, 2.4f); };
        const float values[16] = {
            static_cast<float>(gridWidth), static_cast<float>(gridHeight), dt, valid ? 1.f : 0.f,
            static_cast<float>(width), static_cast<float>(height), staticMap ? 1.f : 0.f, idle ? 1.f : 0.f,
            linear(floor), linear(ceiling), 5.f, 30.f,
            debug ? 1.f : 0.f, 560.f, 640.f, 0.f
        };
        D3D11_MAPPED_SUBRESOURCE mapped{};
        winrt::check_hresult(context->Map(constants.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        memcpy(mapped.pData, values, sizeof(values)); context->Unmap(constants.get(), 0);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs.get(), nullptr, 0);
        auto* cb = constants.get(); context->PSSetConstantBuffers(0, 1, &cb);
        auto* samp = sampler.get(); context->PSSetSamplers(0, 1, &samp);
        Timing* measure = timing[timingIndex].pending ? nullptr : &timing[timingIndex];
        timingIndex = (timingIndex + 1) % timing.size();
        if (measure) { context->Begin(measure->disjoint.get()); context->End(measure->stamp[0].get()); }
        Viewport(gridWidth, gridHeight);
        auto* featureTarget = features[next].rtv.get();
        context->OMSetRenderTargets(1, &featureTarget, nullptr);
        auto* input = rawSrv.get(); context->PSSetShaderResources(0, 1, &input);
        context->PSSetShader(ps[0].get(), nullptr, 0); context->Draw(3, 0); Unbind();
        // Compare untransported observations on the GPU. A frozen desktop must
        // not invent edges from transported history; invisible moving phases
        // can still carry state. No CPU readback or capture queue is added.
        const UINT zero[4]{};
        context->ClearUnorderedAccessViewUint(activityUav.get(), zero);
        if (!idle)
        {
            auto* uav = activityUav.get();
            context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
            ID3D11ShaderResourceView* observationInputs[] = { features[next].srv.get(), features[index].srv.get() };
            context->CSSetShaderResources(1, 2, observationInputs);
            context->CSSetConstantBuffers(0, 1, &cb);
            context->CSSetShader(activityShader.get(), nullptr, 0);
            context->Dispatch((gridWidth + 7) / 8, (gridHeight + 7) / 8, 1);
            ID3D11UnorderedAccessView* noUav = nullptr;
            context->CSSetUnorderedAccessViews(0, 1, &noUav, nullptr);
            ID3D11ShaderResourceView* noInputs[2]{};
            context->CSSetShaderResources(1, 2, noInputs);
            context->CSSetShader(nullptr, nullptr, 0);
        }
        if (measure) context->End(measure->stamp[1].get());
        ID3D11RenderTargetView* targets[] = { state[next][0].rtv.get(), state[next][1].rtv.get(), state[next][2].rtv.get(), state[next][3].rtv.get(), state[next][4].rtv.get() };
        context->OMSetRenderTargets(5, targets, nullptr);
        ID3D11ShaderResourceView* inputs[] = { rawSrv.get(), features[next].srv.get(), state[index][3].srv.get(),
            state[index][0].srv.get(), state[index][1].srv.get(), state[index][2].srv.get(), debug };
        context->PSSetShaderResources(0, 7, inputs);
        auto* active = activitySrv.get(); context->PSSetShaderResources(7, 1, &active);
        auto* floorInput = state[index][4].srv.get(); context->PSSetShaderResources(8, 1, &floorInput);
        context->PSSetShader(ps[1].get(), nullptr, 0); context->Draw(3, 0); Unbind();
        if (measure) context->End(measure->stamp[2].get());
        Viewport(width, height);
        context->OMSetRenderTargets(1, &output, nullptr);
        floorInput = state[next][4].srv.get(); context->PSSetShaderResources(8, 1, &floorInput);
        inputs[3] = state[next][0].srv.get(); inputs[4] = state[next][1].srv.get(); inputs[5] = state[next][2].srv.get();
        context->PSSetShaderResources(0, 7, inputs);
        context->PSSetShader(ps[2].get(), nullptr, 0); context->Draw(3, 0); Unbind();
        if (measure)
        {
            context->End(measure->stamp[3].get()); context->End(measure->disjoint.get()); measure->pending = true;
        }
        index = next; valid = true;
    }
    bool WriteMetrics(const std::filesystem::path& path)
    {
        PollTiming();
        FILE* file = nullptr;
        if (_wfopen_s(&file, path.c_str(), L"wb") || !file) return false;
        std::fprintf(file, "{\n  \"pipeline\":\"surface-frequency-v1\",\n  \"frequency_hz\":[5,30],\n"
            "  \"nvof\":false,\n  \"displayed_history\":false,\n  \"observations\":%llu,\n"
            "  \"grid\":[%u,%u],\n  \"gpu_timing_scope\":\"last up to 4096 completed draws; excludes copy and presentation\",\n"
            "  \"gpu_ms\":{\n", static_cast<unsigned long long>(observations), gridWidth, gridHeight);
        const char* names[] = { "features", "tracking", "composite", "total" };
        for (size_t k = 0; k < gpuMs.size(); ++k)
        {
            auto sorted = gpuMs[k]; std::sort(sorted.begin(), sorted.end());
            const auto quantile = [&](double q) { return sorted.empty() ? 0.0 : sorted[static_cast<size_t>(q * (sorted.size() - 1))]; };
            std::fprintf(file, "    \"%s\":{\"samples\":%zu,\"p50\":%.6f,\"p95\":%.6f,\"p99\":%.6f}%s\n",
                names[k], sorted.size(), quantile(.5), quantile(.95), quantile(.99), k + 1 == gpuMs.size() ? "" : ",");
        }
        std::fputs("  }\n}\n", file); std::fclose(file); return true;
    }
};

// Explicit diagnostic only: capture locally without displaying an overlay.
// Captures stay in the requested directory and must not be packaged/published.
inline bool SurfaceFrequency::LiveProbe(const std::filesystem::path& directory, float contrast, bool replayState)
{
    std::filesystem::create_directories(directory);
    winrt::com_ptr<ID3D11Device> d;
    winrt::com_ptr<ID3D11DeviceContext> c;
    D3D_FEATURE_LEVEL level{};
    winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, nullptr, 0, D3D11_SDK_VERSION, d.put(), &level, c.put()));
    winrt::com_ptr<IDXGIAdapter> adapter;
    winrt::check_hresult(d.as<IDXGIDevice>()->GetAdapter(adapter.put()));
    winrt::com_ptr<IDXGIOutput> display;
    winrt::check_hresult(adapter->EnumOutputs(0, display.put()));
    winrt::com_ptr<IDXGIOutputDuplication> duplication;
    if (!replayState) winrt::check_hresult(display.as<IDXGIOutput1>()->DuplicateOutput(d.get(), duplication.put()));
    SurfaceFrequency pipeline;
    pipeline.Initialize(d.get(), c.get());
    winrt::com_ptr<ID3D11Texture2D> rendered;
    winrt::com_ptr<ID3D11RenderTargetView> target;
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    LONGLONG previousCapture = 0;
    ULONGLONG start = GetTickCount64(), lastRender = start;
    unsigned frames = 0;
    while (!replayState && GetTickCount64() - start < 5000)
    {
        DXGI_OUTDUPL_FRAME_INFO info{};
        winrt::com_ptr<IDXGIResource> resource;
        HRESULT hr = duplication->AcquireNextFrame(20, &info, resource.put());
        const ULONGLONG now = GetTickCount64();
        float dt = std::max(.001f, (now - lastRender) / 1000.f);
        lastRender = now;
        if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        {
            if (target) pipeline.Render(nullptr, target.get(), dt, contrast > 0, .12f * contrast, 1 - .24f * contrast);
            continue;
        }
        winrt::check_hresult(hr);
        try
        {
            auto input = resource.as<ID3D11Texture2D>();
            if (!target)
            {
                D3D11_TEXTURE2D_DESC td{};
                input->GetDesc(&td);
                td.BindFlags = D3D11_BIND_RENDER_TARGET;
                td.MiscFlags = td.CPUAccessFlags = 0;
                winrt::check_hresult(d->CreateTexture2D(&td, nullptr, rendered.put()));
                winrt::check_hresult(d->CreateRenderTargetView(rendered.get(), nullptr, target.put()));
            }
            const bool observation = !pipeline.HasSource() || info.LastPresentTime.QuadPart > previousCapture;
            if (observation)
            {
                if (previousCapture > 0) dt = float(double(info.LastPresentTime.QuadPart - previousCapture) / frequency.QuadPart);
                previousCapture = info.LastPresentTime.QuadPart;
                ++frames;
            }
            pipeline.Render(observation ? input.get() : nullptr, target.get(), dt,
                contrast > 0, .12f * contrast, 1 - .24f * contrast);
        }
        catch (...) { duplication->ReleaseFrame(); throw; }
        winrt::check_hresult(duplication->ReleaseFrame());
    }
    if (replayState)
    {
        FILE* f = nullptr;
        if (_wfopen_s(&f, (directory / L"state-format.txt").c_str(), L"rb") || !f) return false;
        char format[32]{};
        const size_t formatSize = fread(format, 1, sizeof(format) - 1, f); fclose(f);
        if (std::string(format, formatSize) != "rgb-phase-floor-v2\n") return false;
        if (_wfopen_s(&f, (directory / L"source.bmp").c_str(), L"rb") || !f) return false;
        BITMAPFILEHEADER bf{}; BITMAPINFOHEADER bi{};
        bool ok = fread(&bf, sizeof(bf), 1, f) == 1 && fread(&bi, sizeof(bi), 1, f) == 1 &&
            bf.bfType == 0x4d42 && bi.biBitCount == 32 && bi.biHeight < 0 && bi.biWidth > 0;
        if (!ok) { fclose(f); return false; }
        std::vector<uint32_t> pixels(size_t(bi.biWidth) * -bi.biHeight);
        fseek(f, bf.bfOffBits, SEEK_SET);
        ok = fread(pixels.data(), 4, pixels.size(), f) == pixels.size(); fclose(f);
        if (!ok) return false;
        D3D11_TEXTURE2D_DESC td{};
        td.Width = bi.biWidth; td.Height = -bi.biHeight;
        td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        winrt::com_ptr<ID3D11Texture2D> input;
        D3D11_SUBRESOURCE_DATA initial{ pixels.data(), td.Width * 4, 0 };
        winrt::check_hresult(d->CreateTexture2D(&td, &initial, input.put()));
        td.BindFlags = D3D11_BIND_RENDER_TARGET;
        winrt::check_hresult(d->CreateTexture2D(&td, nullptr, rendered.put()));
        winrt::check_hresult(d->CreateRenderTargetView(rendered.get(), nullptr, target.put()));
        pipeline.Render(input.get(), target.get(), 1.f / 60, contrast > 0, .12f * contrast, 1 - .24f * contrast);
        for (int plane = 0; plane < 5; ++plane)
        {
            if (_wfopen_s(&f, (directory / (L"state-" + std::to_wstring(plane) + L".bin")).c_str(), L"rb") || !f) return false;
            UINT dimensions[2]{};
            ok = fread(dimensions, sizeof(UINT), 2, f) == 2 &&
                dimensions[0] == pipeline.gridWidth && dimensions[1] == pipeline.gridHeight;
            if (!ok) { fclose(f); return false; }
            std::vector<float> data(size_t(dimensions[0]) * dimensions[1] * 4);
            ok = fread(data.data(), sizeof(float), data.size(), f) == data.size(); fclose(f);
            if (!ok) return false;
            c->UpdateSubresource(pipeline.state[pipeline.index][plane].texture.get(), 0, nullptr,
                data.data(), dimensions[0] * 16, 0);
        }
        auto* rtv = target.get(); c->OMSetRenderTargets(1, &rtv, nullptr);
        ID3D11ShaderResourceView* inputs[] = {pipeline.rawSrv.get(), pipeline.features[pipeline.index].srv.get(), nullptr,
            pipeline.state[pipeline.index][0].srv.get(), pipeline.state[pipeline.index][1].srv.get(),
            pipeline.state[pipeline.index][2].srv.get(), nullptr};
        c->PSSetShaderResources(0, 7, inputs);
        auto* floorInput = pipeline.state[pipeline.index][4].srv.get(); c->PSSetShaderResources(8, 1, &floorInput);
        c->PSSetShader(pipeline.ps[2].get(), nullptr, 0); c->Draw(3, 0); pipeline.Unbind();
    }
    if (!pipeline.HasSource()) return false;
    const auto save = [&](ID3D11Texture2D* texture, const wchar_t* filename)
    {
        D3D11_TEXTURE2D_DESC td{};
        texture->GetDesc(&td);
        if (td.Format != DXGI_FORMAT_B8G8R8A8_UNORM) throw E_NOTIMPL;
        td.Usage = D3D11_USAGE_STAGING; td.BindFlags = td.MiscFlags = 0;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        winrt::com_ptr<ID3D11Texture2D> staging;
        winrt::check_hresult(d->CreateTexture2D(&td, nullptr, staging.put()));
        c->CopyResource(staging.get(), texture);
        D3D11_MAPPED_SUBRESOURCE map{};
        winrt::check_hresult(c->Map(staging.get(), 0, D3D11_MAP_READ, 0, &map));
        BITMAPFILEHEADER bf{}; BITMAPINFOHEADER bi{};
        bf.bfType = 0x4d42; bf.bfOffBits = sizeof(bf) + sizeof(bi);
        bf.bfSize = bf.bfOffBits + td.Width * td.Height * 4;
        bi.biSize = sizeof(bi); bi.biWidth = td.Width; bi.biHeight = -LONG(td.Height);
        bi.biPlanes = 1; bi.biBitCount = 32; bi.biCompression = BI_RGB;
        FILE* f = nullptr;
        if (_wfopen_s(&f, (directory / filename).c_str(), L"wb") || !f) { c->Unmap(staging.get(), 0); throw E_FAIL; }
        fwrite(&bf, sizeof(bf), 1, f); fwrite(&bi, sizeof(bi), 1, f);
        for (UINT y = 0; y < td.Height; ++y)
            fwrite(static_cast<const uint8_t*>(map.pData) + y * map.RowPitch, td.Width * 4, 1, f);
        fclose(f); c->Unmap(staging.get(), 0);
    };
    if (replayState) { save(rendered.get(), L"replayed.bmp"); return true; }
    save(pipeline.raw.get(), L"source.bmp");
    save(rendered.get(), L"filtered.bmp");
    {
        FILE* f = nullptr;
        if (_wfopen_s(&f, (directory / L"state-format.txt").c_str(), L"wb") || !f) return false;
        std::fputs("rgb-phase-floor-v2\n", f); fclose(f);
    }
    for (int plane = 0; plane < 5; ++plane)
    {
        D3D11_TEXTURE2D_DESC td{};
        pipeline.state[pipeline.index][plane].texture->GetDesc(&td);
        td.Usage = D3D11_USAGE_STAGING; td.BindFlags = td.MiscFlags = 0;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        winrt::com_ptr<ID3D11Texture2D> staging;
        winrt::check_hresult(d->CreateTexture2D(&td, nullptr, staging.put()));
        c->CopyResource(staging.get(), pipeline.state[pipeline.index][plane].texture.get());
        D3D11_MAPPED_SUBRESOURCE map{};
        winrt::check_hresult(c->Map(staging.get(), 0, D3D11_MAP_READ, 0, &map));
        FILE* f = nullptr;
        const auto filename = directory / (L"state-" + std::to_wstring(plane) + L".bin");
        if (_wfopen_s(&f, filename.c_str(), L"wb") || !f) { c->Unmap(staging.get(), 0); throw E_FAIL; }
        fwrite(&td.Width, sizeof(UINT), 1, f); fwrite(&td.Height, sizeof(UINT), 1, f);
        for (UINT y = 0; y < td.Height; ++y)
            fwrite(static_cast<const uint8_t*>(map.pData) + y * map.RowPitch, td.Width * 16, 1, f);
        fclose(f); c->Unmap(staging.get(), 0);
    }
    pipeline.WriteMetrics(directory / L"metrics.json");
    // Exact same captured image and tone settings, with no temporal detector
    // history: separates static tone mapping from newly introduced speckles.
    D3D11_TEXTURE2D_DESC snapshotDesc{};
    pipeline.raw->GetDesc(&snapshotDesc);
    snapshotDesc.BindFlags = 0;
    winrt::com_ptr<ID3D11Texture2D> snapshot;
    winrt::check_hresult(d->CreateTexture2D(&snapshotDesc, nullptr, snapshot.put()));
    c->CopyResource(snapshot.get(), pipeline.raw.get());
    pipeline.Reset();
    pipeline.Render(snapshot.get(), target.get(), 1.f / 60, contrast > 0,
        .12f * contrast, 1 - .24f * contrast);
    save(rendered.get(), L"tone-only.bmp");
    std::fprintf(stderr, "Captured %u source observations; contrast %.3f\n", frames, contrast);
    return true;
}

inline bool SurfaceFrequency::SelfTest(const std::filesystem::path& path, int onlyScene)
{
    winrt::com_ptr<ID3D11Device> d;
    winrt::com_ptr<ID3D11DeviceContext> c;
    D3D_FEATURE_LEVEL level{};
    winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE,
        nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, d.put(), &level, c.put()));
    SurfaceFrequency pipeline;
    pipeline.Initialize(d.get(), c.get());
    constexpr UINT w = 320, h = 180;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w; td.Height = h; td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    winrt::com_ptr<ID3D11Texture2D> input;
    winrt::check_hresult(d->CreateTexture2D(&td, nullptr, input.put()));
    td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    winrt::com_ptr<ID3D11Texture2D> output, readback;
    winrt::com_ptr<ID3D11RenderTargetView> outputRtv;
    winrt::check_hresult(d->CreateTexture2D(&td, nullptr, output.put()));
    winrt::check_hresult(d->CreateRenderTargetView(output.get(), nullptr, outputRtv.put()));
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    winrt::check_hresult(d->CreateTexture2D(&td, nullptr, readback.put()));
    D3D11_TEXTURE2D_DESC pd = td;
    pd.Width = pd.Height = 1;
    winrt::com_ptr<ID3D11Texture2D> probe;
    winrt::check_hresult(d->CreateTexture2D(&pd, nullptr, probe.put()));
    std::vector<uint32_t> pixels(w * h);
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    FILE* report = nullptr;
    if (_wfopen_s(&report, path.c_str(), L"wb") || !report) return false;
    std::fputs("{\n  \"schema\":\"SURFACE_FREQUENCY_TEST/2\",\n  \"width\":320,\"height\":180,\n"
        "  \"moving_trajectory\":\"continuous triangle, 240 pixels/second\",\n  \"cases\":[\n", report);
    bool first = true, passed = true;
    const auto linear = [](double x) { return x <= .04045 ? x / 12.92 : std::pow((x + .055) / 1.055, 2.4); };
    for (const int fps : { 60, 120, 144 })
    for (int kind = 0; kind < 8; ++kind)
    for (double hz : { 2.0, 4.0, 5.0, 7.5, 10.0, 15.0, 20.0, 30.0, 40.0 })
    {
        if (onlyScene != -1) continue;
        const bool outOfBand = hz < 5 || hz > 30;
        if ((kind == 5) != outOfBand || hz > fps * .5) continue;
        if (kind == 3 && hz != 5.0) continue;
        pipeline.Reset();
        double sourceVariation = 0, outputVariation = 0;
        double previousSource = 0, previousOutput = 0, trailPeak = 0;
        int visibleConfirmed = 0, visibleObserved = 0;
        const bool moving = (kind >= 2 && kind <= 4) || kind == 7;
        const bool flashes = kind != 3;
        const bool weak = kind == 0 || kind == 4 || kind == 5 || kind >= 6;
        const uint8_t low = weak ? 126 : 16;
        const uint8_t high = weak ? 130 : 240;
        int oldX = 0;
        for (int frame = 0; frame < fps * 2; ++frame)
        {
            const bool highPhase = !flashes || std::fmod((frame + .37) * hz / fps, 1.0) < .5;
            const uint8_t value = highPhase ? high : low;
            const uint32_t color = 0xFF000000u | (static_cast<uint32_t>(value) * 0x010101u);
            const uint32_t background = 0xFF101010u;
            std::fill(pixels.begin(), pixels.end(), moving ? background : color);
            const int travel = (frame * 240 / fps) % 480;
            int x = moving ? 20 + (travel <= 240 ? travel : 480 - travel) : 0;
            if (moving)
                for (int yy = 66; yy < 114; ++yy)
                    for (int xx = x; xx < x + 48; ++xx) pixels[yy * w + xx] = color;
            c->UpdateSubresource(input.get(), 0, nullptr, pixels.data(), w * 4, 0);
            pipeline.Render(input.get(), outputRtv.get(), 1.f / fps, false, 0, 1);
            c->CopyResource(readback.get(), output.get());
            D3D11_MAPPED_SUBRESOURCE map{};
            winrt::check_hresult(c->Map(readback.get(), 0, D3D11_MAP_READ, 0, &map));
            auto* row = reinterpret_cast<const float*>(static_cast<const uint8_t*>(map.pData) + 90 * map.RowPitch);
            const double out = linear(row[(moving ? x + 24 : 160) * 4]);
            const double src = linear(value / 255.0);
            if (frame > fps / 2)
            {
                sourceVariation += std::fabs(src - previousSource);
                outputVariation += std::fabs(out - previousOutput);
            }
            if (moving && frame > 0)
                for (int xx = oldX; xx < oldX + 48; ++xx)
                    if (xx < x || xx >= x + 48)
                        trailPeak = std::max(trailPeak, std::fabs(static_cast<double>(row[xx * 4]) - 16.0 / 255));
            c->Unmap(readback.get(), 0);
            if (highPhase && frame > fps / 2)
            {
                const UINT cx = static_cast<UINT>(moving ? x + 24 : 160) * pipeline.gridWidth / w;
                const UINT cy = 90 * pipeline.gridHeight / h;
                D3D11_BOX box{ cx, cy, 0, cx + 1, cy + 1, 1 };
                c->CopySubresourceRegion(probe.get(), 0, 0, 0, 0,
                    pipeline.state[pipeline.index][0].texture.get(), 0, &box);
                D3D11_MAPPED_SUBRESOURCE stateMap{};
                winrt::check_hresult(c->Map(probe.get(), 0, D3D11_MAP_READ, 0, &stateMap));
                const auto* gpuPhase = static_cast<const float*>(stateMap.pData);
                const double detected = gpuPhase[2] > 0 ? 1.0 / gpuPhase[2] : 0;
                if (gpuPhase[3] >= 1 && (outOfBand || std::fabs(detected - hz) < hz * .15)) ++visibleConfirmed;
                ++visibleObserved;
                c->Unmap(probe.get(), 0);
            }
            previousSource = src; previousOutput = out; oldX = x;
            if (kind >= 6)
                pipeline.Render(nullptr, outputRtv.get(), .5f / fps, false, 0, 1);
        }
        // Read the actual GPU detector's center state after the final frame.
        D3D11_TEXTURE2D_DESC sd{};
        pipeline.state[pipeline.index][0].texture->GetDesc(&sd);
        sd.Usage = D3D11_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        winrt::com_ptr<ID3D11Texture2D> phaseReadback;
        winrt::check_hresult(d->CreateTexture2D(&sd, nullptr, phaseReadback.put()));
        c->CopyResource(phaseReadback.get(), pipeline.state[pipeline.index][0].texture.get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        winrt::check_hresult(c->Map(phaseReadback.get(), 0, D3D11_MAP_READ, 0, &mapped));
        const auto* phaseRow = reinterpret_cast<const float*>(static_cast<const uint8_t*>(mapped.pData) +
            (90 * pipeline.gridHeight / h) * mapped.RowPitch);
        const int centerX = (moving ? oldX + 24 : 160) * pipeline.gridWidth / w;
        const float period = phaseRow[centerX * 4 + 2];
        const float evidence = phaseRow[centerX * 4 + 3];
        c->Unmap(phaseReadback.get(), 0);
        const double reduction = sourceVariation > .000001 ? 1 - outputVariation / sourceVariation : 0;
        const bool ok = outOfBand ? visibleConfirmed == 0 && std::fabs(reduction) < .02 :
            (flashes ? reduction >= .70 && visibleConfirmed > 0 : trailPeak <= 1.0 / 255);
        passed &= ok;
        std::fprintf(report, "%s    {\"kind\":%d,\"hz\":%.2f,\"fps\":%d,\"reduction\":%.8f,"
            "\"vacated_peak_code\":%.4f,\"estimated_hz\":%.4f,\"evidence\":%.1f,"
            "\"visible_confirmed_frames\":%d,\"visible_observed_frames\":%d,\"pass\":%s}",
            first ? "" : ",\n", kind, hz, fps, reduction, trailPeak * 255,
            period > 0 ? 1.0 / period : 0, evidence, visibleConfirmed, visibleObserved, ok ? "true" : "false");
        first = false;
    }
    std::fputs("\n  ],\n  \"regressions\":[\n", report);
    // Inspect every RGB channel, including texture and boundaries. The original
    // grayscale center probe cannot expose chromatic leaks or a noisy mask.
    bool firstRegression = true;
    for (int scene = 0; scene < 17; ++scene)
    for (const int fps : {60, 120, 144})
    for (double hz : {5.0, 10.0, 20.0, 30.0})
    {
        if (onlyScene >= 0 && scene != onlyScene) continue;
        if (onlyScene == -2 && scene < 12) continue;
        if ((scene < 3 || (scene >= 8 && scene <= 11)) && hz != 10) continue;
        pipeline.Reset();
        std::vector<double> lastSource(w * h * 3), lastOutput(w * h * 3);
        double sourceVariation = 0, outputVariation = 0, maxError = 0;
        double settledError = 0, spatialPeak = 0, backgroundError = 0;
        double onsetSource = 0, onsetOutput = 0;
        for (int frame = 0; frame < fps * 2; ++frame)
        {
            bool high = std::fmod((frame + .37) * hz / fps, 1.0) < .5;
            const int travel = (frame * 240 / fps) % 480;
            const int left = 20 + (travel <= 240 ? travel : 480 - travel);
            for (UINT y = 0; y < h; ++y)
            for (UINT x = 0; x < w; ++x)
            {
                int r, g, b;
                if (scene < 3 || scene == 11)
                {
                    int shift = scene == 2 ? std::min(frame, fps / 2) * 2 : 0;
                    int tile = ((x + shift) / 7 + y / 9) % 5;
                    r = 80 + tile * 30; g = 200 - tile * 25; b = 100 + tile * 20;
                    // A visually static image with one-code dither, spatially
                    // correlated like quantization in a compressed video.
                    if (scene == 1) { int noise = high ? 1 : -1; r += noise; g += noise; b += noise; }
                    if (scene == 11)
                    {
                        uint32_t seed = (x / 3) * 73856093u ^ (y / 3) * 19349663u ^ frame * 83492791u;
                        seed ^= seed >> 16; seed *= 0x7feb352du; seed ^= seed >> 15;
                        int noise = static_cast<int>(seed % 5) - 2;
                        r += noise; g += noise; b += noise;
                    }
                }
                else
                {
                    // White/red; white with a changing colored low phase;
                    // textured white/red; moving white/red; equal-luma chroma.
                    int palette = scene == 4 ? (frame / (fps / 3)) % 3 : 0;
                    r = high ? 255 : (palette == 0 ? 255 : 0);
                    g = high ? 255 : (palette == 1 ? 255 : 0);
                    b = high ? 255 : (palette == 2 ? 255 : 0);
                    if (scene == 5 && ((x / 3 + y / 3) % 2)) { r = r * 3 / 4; g = g * 3 / 4; b = b * 3 / 4; }
                    if (scene == 6 || (scene >= 8 && scene <= 10))
                    {
                        if (scene >= 8) r = g = b = 240;
                        if (x < static_cast<UINT>(left) || x >= static_cast<UINT>(left + 48) || y < 66 || y >= 114)
                        {
                            r = scene == 8 ? 255 : (scene >= 8 ? 0 : 16);
                            g = scene == 9 ? 255 : (scene >= 8 ? 0 : 16);
                            b = scene == 10 ? 255 : (scene >= 8 ? 0 : 16);
                        }
                    }
                    if (scene == 7) { r = high ? 255 : 0; g = high ? 0 : 148; b = 0; }
                    if (scene >= 12)
                    {
                        // H=0 S=78% V=95% versus S=0 V=5%, plus hue,
                        // saturation and changing foreground hue at the same V.
                        r = high ? 242 : 13; g = b = high ? 53 : 13;
                        if (scene == 13) { r = high ? 242 : 53; g = high ? 53 : 242; b = 53; }
                        if (scene == 14) { r = 242; g = b = high ? 53 : 82; }
                        if (scene == 15 && high)
                        {
                            int hue = (frame / std::max(1, fps / 3)) % 3;
                            r = hue == 0 ? 242 : 53; g = hue == 1 ? 242 : 53; b = hue == 2 ? 242 : 53;
                        }
                        if (scene == 16) { r = 20; g = high ? 20 : 40; b = high ? 40 : 20; }
                    }
                }
                pixels[y * w + x] = 0xff000000u | r | (g << 8) | (b << 16);
                // Keep a separate animated patch active after the main image
                // stops. A global "any change" flag cannot establish that the
                // rest of a desktop is static (clocks and cursors keep changing).
                if (scene == 2 && y < 16 && x > w - 32)
                    pixels[y * w + x] = (frame % 2) ? 0xffeeeeeeu : 0xff101010u;
            }
            c->UpdateSubresource(input.get(), 0, nullptr, pixels.data(), w * 4, 0);
            pipeline.Render(input.get(), outputRtv.get(), 1.f / fps, false, 0, 1);
            c->CopyResource(readback.get(), output.get());
            D3D11_MAPPED_SUBRESOURCE map{};
            winrt::check_hresult(c->Map(readback.get(), 0, D3D11_MAP_READ, 0, &map));
            for (UINT y = 0; y < h; ++y)
            {
                const auto* row = reinterpret_cast<const float*>(static_cast<const uint8_t*>(map.pData) + y * map.RowPitch);
                for (UINT x = 0; x < w; ++x)
                for (UINT ch = 0; ch < 3; ++ch)
                {
                    if (scene == 2 && y < 48) continue;
                    const double srcCode = ((pixels[y * w + x] >> (ch * 8)) & 255) / 255.0;
                    const double src = linear(srcCode), out = linear(row[x * 4 + ch]);
                    const double error = std::fabs(row[x * 4 + ch] - srcCode);
                    const bool inside = x >= static_cast<UINT>(left) && x < static_cast<UINT>(left + 48) && y >= 66 && y < 114;
                    if ((scene == 6 || (scene >= 8 && scene <= 10)) && !inside)
                    {
                        backgroundError = std::max(backgroundError, error);
                        continue;
                    }
                    // For motion, compare every pixel in object coordinates;
                    // crossing an edge at a fixed screen pixel is not flashing.
                    const size_t offset = (scene == 6 || (scene >= 8 && scene <= 10)) ? ((y - 66) * 48 + x - left) * 3 + ch : (y * w + x) * 3 + ch;
                    if (scene >= 12 && frame >= static_cast<int>(std::ceil(fps / hz)) && frame <= fps / 2)
                    {
                        onsetSource += std::fabs(src - lastSource[offset]);
                        onsetOutput += std::fabs(out - lastOutput[offset]);
                    }
                    if (frame > fps / 2)
                    {
                        sourceVariation += std::fabs(src - lastSource[offset]);
                        outputVariation += std::fabs(out - lastOutput[offset]);
                        maxError = std::max(maxError, error);
                        if (frame > fps) settledError = std::max(settledError, error);
                    }
                    if (scene == 5 && frame > fps && high) spatialPeak = std::max(spatialPeak, out);
                    lastSource[offset] = src; lastOutput[offset] = out;
                }
            }
            c->Unmap(readback.get(), 0);
        }
        const double reduction = sourceVariation > .000001 ? 1 - outputVariation / sourceVariation : 0;
        const double onsetReduction = onsetSource > .000001 ? 1 - onsetOutput / onsetSource : 1;
        const bool ok = scene >= 12 ? reduction >= .70 && onsetReduction >= .70 :
            scene == 11 ? maxError <= 4.05 / 255 :
            scene >= 8 ? backgroundError < 1.0 / 255 :
            scene < 2 ? maxError < 1.0 / 255 : scene == 2 ? settledError < 1.0 / 255 :
            reduction >= .70 && backgroundError < 1.0 / 255;
        passed &= ok;
        std::fprintf(report, "%s    {\"scene\":%d,\"fps\":%d,\"hz\":%.1f,\"rgb_reduction\":%.8f,"
            "\"onset_reduction\":%.8f,\"max_error_code\":%.5f,\"settled_error_code\":%.5f,\"texture_peak_linear\":%.5f,\"background_error_code\":%.5f,\"pass\":%s}",
            firstRegression ? "" : ",\n", scene, fps, hz, reduction, onsetReduction, maxError * 255, settledError * 255, spatialPeak,
            backgroundError * 255, ok ? "true" : "false");
        firstRegression = false;
    }
    std::fprintf(report, "\n  ],\n  \"pass\":%s\n}\n", passed ? "true" : "false");
    std::fclose(report);
    pipeline.WriteMetrics(path.parent_path() / L"surface-frequency-metrics.json");
    return passed;
}

inline bool SurfaceFrequency::Benchmark(const std::filesystem::path& path)
{
    winrt::com_ptr<ID3D11Device> d;
    winrt::com_ptr<ID3D11DeviceContext> c;
    D3D_FEATURE_LEVEL level{};
    winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE,
        nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, d.put(), &level, c.put()));
    SurfaceFrequency pipeline;
    pipeline.Initialize(d.get(), c.get());
    constexpr UINT w = 1920, h = 1080;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w; td.Height = h; td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    winrt::com_ptr<ID3D11Texture2D> input, output, finish;
    winrt::com_ptr<ID3D11RenderTargetView> target;
    winrt::check_hresult(d->CreateTexture2D(&td, nullptr, input.put()));
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    winrt::check_hresult(d->CreateTexture2D(&td, nullptr, output.put()));
    winrt::check_hresult(d->CreateRenderTargetView(output.get(), nullptr, target.put()));
    td.Width = td.Height = 1;
    td.BindFlags = 0; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    winrt::check_hresult(d->CreateTexture2D(&td, nullptr, finish.put()));
    std::vector<uint32_t> pixels(w * h);
    for (int frame = 0; frame < 360; ++frame)
    {
        for (UINT y = 0; y < h; ++y)
            for (UINT x = 0; x < w; ++x)
            {
                const UINT offset = frame < 180 ? 0u : static_cast<UINT>(frame * 3);
                const UINT value = 24 + (((x + offset) / 23 + y / 19) % 5) * 12;
                pixels[y * w + x] = 0xFF000000u | value * 0x010101u;
            }
        const UINT left = static_cast<UINT>((frame * 3) % 1760);
        const uint32_t value = (frame / 4) % 2 ? 0xFF202020u : 0xFFF0F0F0u;
        for (UINT y = 440; y < 600; ++y)
            for (UINT x = left; x < left + 160; ++x) pixels[y * w + x] = value;
        c->UpdateSubresource(input.get(), 0, nullptr, pixels.data(), w * 4, 0);
        pipeline.Render(input.get(), target.get(), 1.f / 120, false, 0, 1);
        if (frame == 29 || frame == 359)
        {
            D3D11_BOX box{ 0,0,0,1,1,1 };
            c->CopySubresourceRegion(finish.get(), 0, 0, 0, 0, output.get(), 0, &box);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            winrt::check_hresult(c->Map(finish.get(), 0, D3D11_MAP_READ, 0, &mapped));
            c->Unmap(finish.get(), 0);
            pipeline.PollTiming();
            if (frame == 29) for (auto& samples : pipeline.gpuMs) samples.clear();
        }
    }
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    return pipeline.WriteMetrics(path);
}
