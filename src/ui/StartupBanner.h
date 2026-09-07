// Dedicated UI thread: shader compilation must never block banner repainting.
struct StartupBannerState
{
    HWND status = nullptr, progress = nullptr;
    HFONT font = nullptr, titleFont = nullptr;
    float displayed = 0;
    ULONGLONG started = GetTickCount64();
    UINT dpi = 96;
};
std::thread g_startupThread;

LRESULT CALLBACK StartupBannerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<StartupBannerState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE)
    {
        state = static_cast<StartupBannerState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state) return DefWindowProcW(hwnd, msg, wParam, lParam);
    const auto scale = [state](int n) { return MulDiv(n, state->dpi, 96); };
    switch (msg)
    {
    case WM_CREATE:
        state->dpi = GetDpiForWindow(hwnd);
        state->font = CreateFontW(scale(-14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"JetBrains Mono");
        state->titleFont = CreateFontW(scale(-20), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"JetBrains Mono");
        SetTimer(hwnd, 1, 33, nullptr);
        return 0;
    case WM_TIMER:
    {
        const float target = static_cast<float>(SendMessageW(state->progress, PBM_GETPOS, 0, 0));
        state->displayed += (target - state->displayed) * .16f;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT paint{}; HDC dc = BeginPaint(hwnd, &paint);
        RECT rc{}; GetClientRect(hwnd, &rc);
        HDC memory = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        HGDIOBJ oldBitmap = SelectObject(memory, bitmap);
        const auto fill = [&](RECT area, COLORREF color) {
            HBRUSH brush = CreateSolidBrush(color); FillRect(memory, &area, brush); DeleteObject(brush);
        };
        fill(rc, RGB(20, 22, 27));
        fill({0, 0, scale(3), rc.bottom}, RGB(91, 153, 232));
        SetBkMode(memory, TRANSPARENT);
        SetTextColor(memory, RGB(237, 240, 247));
        HGDIOBJ oldFont = SelectObject(memory, state->titleFont);
        RECT title{scale(20), scale(12), rc.right - scale(20), scale(40)};
        DrawTextW(memory, L"FlashGuard", -1, &title, DT_LEFT | DT_SINGLELINE);
        SelectObject(memory, state->font);
        wchar_t elapsed[80]{};
        swprintf_s(elapsed, L"LOADING  \u00b7  %llus", (GetTickCount64() - state->started) / 1000);
        SetTextColor(memory, RGB(150, 163, 183));
        DrawTextW(memory, elapsed, -1, &title, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        wchar_t text[256]{}; GetWindowTextW(state->status, text, 256);
        RECT status{scale(20), scale(48), rc.right - scale(20), scale(88)};
        SetTextColor(memory, RGB(211, 218, 230));
        DrawTextW(memory, text, -1, &status, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        RECT track{scale(20), rc.bottom - scale(14), rc.right - scale(20), rc.bottom - scale(10)};
        fill(track, RGB(44, 50, 61));
        track.right = track.left + static_cast<int>((track.right - track.left) * state->displayed / 100);
        fill(track, RGB(91, 153, 232));
        SelectObject(memory, oldFont);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, memory, 0, 0, SRCCOPY);
        SelectObject(memory, oldBitmap); DeleteObject(bitmap); DeleteDC(memory); EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1); DeleteObject(state->font); DeleteObject(state->titleFont);
        PostQuitMessage(0); return 0;
    default: return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void CloseStartupWindow(HWND hwnd)
{
    if (IsWindow(hwnd)) SendMessageW(hwnd, WM_CLOSE, 0, 0);
    if (g_startupThread.joinable()) g_startupThread.join();
}

HWND CreateStartupStatusWindow(HINSTANCE instance, HMONITOR monitor,
    HWND& statusText, HWND& progress, bool preview = false)
{
    statusText = progress = nullptr;
    std::promise<HWND> ready;
    auto result = ready.get_future();
    g_startupThread = std::thread([&, instance, monitor, preview, ready = std::move(ready)]() mutable {
        StartupBannerState state;
        HWND hwnd = nullptr;
        try
        {
            WNDCLASSEXW wc{sizeof(wc)};
            wc.lpfnWndProc = StartupBannerProc; wc.hInstance = instance;
            wc.lpszClassName = L"FlashGuardStartupBanner";
            if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                winrt::throw_last_error();
            MONITORINFO mi{sizeof(mi)};
            if (!GetMonitorInfoW(monitor, &mi)) winrt::throw_last_error();
            const UINT dpi = GetDpiForSystem();
            hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
                wc.lpszClassName, L"FlashGuard Loading", WS_POPUP,
                mi.rcWork.left + 24, mi.rcWork.top + 24, MulDiv(560, dpi, 96), MulDiv(112, dpi, 96),
                nullptr, nullptr, instance, &state);
            if (!hwnd) winrt::throw_last_error();
            const int opacity = std::clamp(ReadPreference(PreferencesPath(), L"MenuOpacity", 92), 60, 100);
            if (!SetLayeredWindowAttributes(hwnd, 0, static_cast<BYTE>(MulDiv(opacity, 255, 100)), LWA_ALPHA))
                winrt::throw_last_error();
            if (!preview && !SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE))
                winrt::throw_last_error();
            // Hidden message endpoints preserve the existing progress-reporting API.
            state.status = CreateWindowExW(0, L"STATIC", L"Preparing your display filter...", WS_CHILD,
                0, 0, 0, 0, hwnd, nullptr, instance, nullptr);
            state.progress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD,
                0, 0, 0, 0, hwnd, nullptr, instance, nullptr);
            if (!state.status || !state.progress) winrt::throw_last_error();
            SendMessageW(state.progress, PBM_SETRANGE32, 0, 100);
            statusText = state.status; progress = state.progress;
            ShowWindow(hwnd, SW_SHOWNOACTIVATE); UpdateWindow(hwnd);
            ready.set_value(hwnd);
        }
        catch (...)
        {
            if (hwnd) DestroyWindow(hwnd);
            ready.set_exception(std::current_exception());
            return;
        }
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
    });
    try { return result.get(); }
    catch (...) { if (g_startupThread.joinable()) g_startupThread.join(); throw; }
}
