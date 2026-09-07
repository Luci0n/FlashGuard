#pragma once
// DXBC is portable shader bytecode. Cache by source, entry point and compiler flags;
// never by the executable timestamp, so UI-only builds can reuse it.
namespace fg_shader_cache {
inline std::atomic<unsigned> hits{0}, misses{0};
inline uint64_t Hash(const void* bytes, size_t size, uint64_t value = 14695981039346656037ull) {
    const auto* p = static_cast<const unsigned char*>(bytes);
    while (size--) { value ^= *p++; value *= 1099511628211ull; }
    return value;
}
inline std::filesystem::path LocalDirectory() {
    wchar_t path[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path))) return {};
    return std::filesystem::path(path) / L"OutlastFlashGuard" / L"shader-cache";
}
struct Header { uint64_t magic, key, size, checksum; };
inline HRESULT Compile(const void* source, SIZE_T size, const char* name,
    const D3D_SHADER_MACRO* defines, ID3DInclude* includes, const char* entry,
    const char* target, UINT flags, UINT effects, ID3DBlob** code, ID3DBlob** errors) {
    if (defines || includes) return D3DCompile(source,size,name,defines,includes,entry,target,flags,effects,code,errors);
    uint64_t key = Hash(source, size);
    key = Hash(entry, strlen(entry) + 1, key); key = Hash(target, strlen(target) + 1, key);
    const UINT settings[]{flags, effects, D3D_COMPILER_VERSION}; key = Hash(settings, sizeof(settings), key);
    wchar_t leaf[32]{}; swprintf_s(leaf, L"%016llx.fgshader", key);
    wchar_t module[MAX_PATH]{}; GetModuleFileNameW(nullptr, module, MAX_PATH);
    const auto local = LocalDirectory();
    const auto bundled = std::filesystem::path(module).parent_path() / L"shaders";
    for (const auto& directory : {bundled, local}) {
        if (directory.empty()) continue;
        FILE* f = nullptr;
        if (_wfopen_s(&f, (directory / leaf).c_str(), L"rb") || !f) continue;
        Header h{}; winrt::com_ptr<ID3DBlob> blob;
        bool ok = fread(&h, sizeof(h), 1, f) == 1 && h.magic == 0x314348534746ull && h.key == key &&
            h.size >= 4 && h.size <= 64 * 1024 * 1024 && SUCCEEDED(D3DCreateBlob(static_cast<SIZE_T>(h.size), blob.put()));
        if (ok) ok = fread(blob->GetBufferPointer(), 1, static_cast<size_t>(h.size), f) == h.size &&
            fgetc(f) == EOF && !memcmp(blob->GetBufferPointer(), "DXBC", 4) &&
            Hash(blob->GetBufferPointer(), blob->GetBufferSize()) == h.checksum;
        fclose(f);
        if (ok) { if (errors) *errors = nullptr; *code = blob.detach(); ++hits; return S_OK; }
    }
    ++misses;
    HRESULT hr = D3DCompile(source,size,name,defines,includes,entry,target,flags,effects,code,errors);
    if (FAILED(hr) || local.empty()) return hr;
    std::error_code ec; std::filesystem::create_directories(local, ec);
    if (ec) return hr; // Caching is optional; a read-only profile must still run.
    auto tmp = local / (std::wstring(leaf) + L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetCurrentThreadId()) + L".tmp");
    FILE* f = nullptr;
    if (_wfopen_s(&f, tmp.c_str(), L"wb") || !f) return hr;
    Header h{0x314348534746ull, key, (*code)->GetBufferSize(), Hash((*code)->GetBufferPointer(), (*code)->GetBufferSize())};
    bool ok = fwrite(&h, sizeof(h), 1, f) == 1 && fwrite((*code)->GetBufferPointer(), 1, static_cast<size_t>(h.size), f) == h.size;
    ok = fclose(f) == 0 && ok;
    if (ok) MoveFileExW(tmp.c_str(), (local / leaf).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    std::filesystem::remove(tmp, ec);
    return hr;
}
}
