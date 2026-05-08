// app.cpp — TouchDirve 全局热键控制器
// 后台运行 + 全局热键 → 给鼠标光标下的窗口发滚轮 / Ctrl+滚轮缩放。
//
// 设计原则：
//   - 不识别窗口、不切前台、不抢焦点
//   - 鼠标光标在哪窗口 → 滚哪窗口（Windows 默认行为）
//   - 用 keybd_event/mouse_event（老 API，跨进程兼容性最好）
//
// 热键 (Win+Shift 组合避开 IME / QQ 等冲突)：
//   Win+Shift+J  向下滚动 (6 notch)
//   Win+Shift+K  向上滚动 (6 notch)
//   Win+Shift+I  放大 (Ctrl+滚轮上 4 次)
//   Win+Shift+O  缩小 (Ctrl+滚轮下 4 次)
//   Win+Shift+0  重置缩放 (Ctrl+0)
//   Win+Shift+Q  退出

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdint.h>

#include "../driver/public.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")

// ---------------------------------------------------------------------
// Driver handle (kept for backward compat with the touch driver — the
// hotkey path doesn't actually need it, but we open it so users can verify
// the driver is reachable from the status text).
// ---------------------------------------------------------------------

static HANDLE g_Driver = INVALID_HANDLE_VALUE;
static FILE*  g_Log    = nullptr;

static void LogF(const char* fmt, ...)
{
    if (!g_Log) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(g_Log, "[%02d:%02d:%02d.%03d] ",
            t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list ap; va_start(ap, fmt);
    vfprintf(g_Log, fmt, ap);
    va_end(ap);
    fputc('\n', g_Log);
    fflush(g_Log);
}

static bool OpenDriver()
{
    g_Driver = CreateFileW(L"\\\\.\\HidTouchInject",
                          GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                          OPEN_EXISTING, 0, nullptr);
    return g_Driver != INVALID_HANDLE_VALUE;
}

// ---------------------------------------------------------------------
// Hotkey IDs
// ---------------------------------------------------------------------
#define HK_SCROLL_DOWN  1
#define HK_SCROLL_UP    2
#define HK_ZOOM_IN      3
#define HK_ZOOM_OUT     4
#define HK_ZOOM_RESET   5
#define HK_QUIT         6

// ---------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------
#define IDC_STATUS  100
#define IDC_LOG     101

static HWND g_hStatus = nullptr;
static HWND g_hLog    = nullptr;

static void AppendLog(const wchar_t* s)
{
    if (!g_hLog) return;
    int len = GetWindowTextLengthW(g_hLog);
    SendMessageW(g_hLog, EM_SETSEL, len, len);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)s);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
}

static void SetStatus(const wchar_t* s)
{
    if (g_hStatus) SetWindowTextW(g_hStatus, s);
}

// ---------------------------------------------------------------------
// Action: scroll wheel at current cursor position. The mouse_event API
// delivers wheel events to whatever window is under the cursor — we don't
// need to identify or focus the target.
// ---------------------------------------------------------------------
static void DoScrollDown()
{
    for (int i = 0; i < 6; ++i) {
        mouse_event(MOUSEEVENTF_WHEEL, 0, 0, (DWORD)(-WHEEL_DELTA), 0);
        Sleep(40);
    }
    LogF("scroll-down 6x at cursor");
}

static void DoScrollUp()
{
    for (int i = 0; i < 6; ++i) {
        mouse_event(MOUSEEVENTF_WHEEL, 0, 0, WHEEL_DELTA, 0);
        Sleep(40);
    }
    LogF("scroll-up 6x at cursor");
}

static void DoZoomIn()
{
    keybd_event(VK_CONTROL, 0x1D, 0, 0);
    Sleep(50);
    for (int i = 0; i < 4; ++i) {
        mouse_event(MOUSEEVENTF_WHEEL, 0, 0, WHEEL_DELTA, 0);
        Sleep(120);
    }
    keybd_event(VK_CONTROL, 0x1D, KEYEVENTF_KEYUP, 0);
    LogF("zoom-in (Ctrl + 4x wheel-up)");
}

static void DoZoomOut()
{
    keybd_event(VK_CONTROL, 0x1D, 0, 0);
    Sleep(50);
    for (int i = 0; i < 4; ++i) {
        mouse_event(MOUSEEVENTF_WHEEL, 0, 0, (DWORD)(-WHEEL_DELTA), 0);
        Sleep(120);
    }
    keybd_event(VK_CONTROL, 0x1D, KEYEVENTF_KEYUP, 0);
    LogF("zoom-out (Ctrl + 4x wheel-down)");
}

static void DoZoomReset()
{
    keybd_event(VK_CONTROL, 0x1D, 0, 0);
    Sleep(50);
    keybd_event('0', 0x0B, 0, 0);
    Sleep(40);
    keybd_event('0', 0x0B, KEYEVENTF_KEYUP, 0);
    Sleep(20);
    keybd_event(VK_CONTROL, 0x1D, KEYEVENTF_KEYUP, 0);
    LogF("zoom-reset (Ctrl+0)");
}

// ---------------------------------------------------------------------
// Window proc
// ---------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hi = ((LPCREATESTRUCT)l)->hInstance;
        g_hStatus = CreateWindowW(L"STATIC", L"",
                      WS_CHILD | WS_VISIBLE | SS_LEFT,
                      10, 10, 480, 20, hwnd, (HMENU)IDC_STATUS, hi, nullptr);
        g_hLog = CreateWindowW(L"EDIT",
                      L"TouchDirve 全局热键控制器\r\n"
                      L"=========================\r\n"
                      L"Win+Shift+J  向下滚动\r\n"
                      L"Win+Shift+K  向上滚动\r\n"
                      L"Win+Shift+I  放大\r\n"
                      L"Win+Shift+O  缩小\r\n"
                      L"Win+Shift+0  重置缩放\r\n"
                      L"Win+Shift+Q  退出\r\n"
                      L"\r\n"
                      L"使用方法：把鼠标光标移到要滚动的窗口上，然后按热键。\r\n"
                      L"任何能用鼠标轮的程序都支持（Chrome、IE、记事本、画图、QQ 音乐、PotPlayer 等等）。\r\n"
                      L"\r\n",
                      WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                      10, 35, 480, 280, hwnd, (HMENU)IDC_LOG, hi, nullptr);
        return 0;
    }
    case WM_HOTKEY: {
        switch (w) {
        case HK_SCROLL_DOWN: SetStatus(L"向下滚动..."); DoScrollDown(); SetStatus(L"就绪"); break;
        case HK_SCROLL_UP:   SetStatus(L"向上滚动..."); DoScrollUp();   SetStatus(L"就绪"); break;
        case HK_ZOOM_IN:     SetStatus(L"放大...");   DoZoomIn();     SetStatus(L"就绪"); break;
        case HK_ZOOM_OUT:    SetStatus(L"缩小...");   DoZoomOut();    SetStatus(L"就绪"); break;
        case HK_ZOOM_RESET:  SetStatus(L"重置缩放..."); DoZoomReset(); SetStatus(L"就绪"); break;
        case HK_QUIT:        DestroyWindow(hwnd); break;
        }
        return 0;
    }
    case WM_CLOSE:   DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0);  return 0;
    }
    return DefWindowProcW(hwnd, msg, w, l);
}

// ---------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------
int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    g_Log = fopen("C:\\hidtouch\\app.log", "w");
    LogF("app start");

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"TouchDirveHotkey";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName,
                                L"TouchDirve 全局热键 (Win+Shift+J/K/I/O/0/Q)",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 520, 380,
                                nullptr, nullptr, hInst, nullptr);
    if (!hwnd) { LogF("CreateWindow failed gle=%lu", GetLastError()); return 1; }

    bool drvOk = OpenDriver();
    LogF("OpenDriver=%d gle=%lu", drvOk, GetLastError());

    SetStatus(L"启动中...");
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Register global hotkeys one-by-one. Use Win+Shift to avoid common
    // conflicts with IMEs (Sogou/QQ拼音 grab Ctrl+Alt) and other apps.
    // Per-key logging so partial failure is visible. We keep going even if
    // some hotkeys fail to register — others still work.
    struct HK { int id; UINT vk; };
    HK keys[] = {
        { HK_SCROLL_DOWN, 'J' },
        { HK_SCROLL_UP,   'K' },
        { HK_ZOOM_IN,     'I' },
        { HK_ZOOM_OUT,    'O' },
        { HK_ZOOM_RESET,  '0' },
        { HK_QUIT,        'Q' },
    };
    UINT mods = MOD_WIN | MOD_SHIFT;
    int okCount = 0;
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        BOOL r = RegisterHotKey(hwnd, keys[i].id, mods, keys[i].vk);
        DWORD gle = GetLastError();
        LogF("RegisterHotKey id=%d vk='%c' result=%d gle=%lu",
             keys[i].id, keys[i].vk, r, gle);
        if (r) okCount++;
    }
    LogF("hotkey registration done: %d/6 ok", okCount);

    wchar_t status[256];
    wsprintfW(status, L"就绪 — %d/6 热键已注册%s",
              okCount,
              drvOk ? L" (驱动已连接)" : L" (驱动未连接,但热键照样 work)");
    SetStatus(status);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterHotKey(hwnd, HK_SCROLL_DOWN);
    UnregisterHotKey(hwnd, HK_SCROLL_UP);
    UnregisterHotKey(hwnd, HK_ZOOM_IN);
    UnregisterHotKey(hwnd, HK_ZOOM_OUT);
    UnregisterHotKey(hwnd, HK_ZOOM_RESET);
    UnregisterHotKey(hwnd, HK_QUIT);

    if (g_Driver != INVALID_HANDLE_VALUE) CloseHandle(g_Driver);
    if (g_Log) fclose(g_Log);
    return 0;
}
