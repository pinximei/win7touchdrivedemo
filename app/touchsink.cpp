// touchsink.cpp -- a window that registers itself as a touch consumer and
// renders every WM_TOUCH it receives. Used to prove the WM_TOUCH dispatch
// chain works end-to-end with our virtual driver.
//
// Run on the VM desktop (NOT over SSH -- WM_TOUCH dispatch is bound to the
// console session). Then in another cmd run gesture.exe; touch points should
// appear in this window.
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// WM_TOUCH and TOUCHINPUT are Win7+. SDK headers expose them; if compiling
// with an older SDK these would need manual definitions, but VS2022 is fine.

static const int kMaxTrails = 256;
struct Trail { POINT pt; DWORD t; UINT id; bool down; };
static Trail g_trails[kMaxTrails];
static int g_trailHead = 0;
static int g_trailCount = 0;
static UINT g_touchEvts = 0;
static UINT g_touchFrames = 0;
static UINT g_lastTouches = 0;

static void pushTrail(POINT p, UINT id, bool down) {
    g_trails[g_trailHead] = { p, GetTickCount(), id, down };
    g_trailHead = (g_trailHead + 1) % kMaxTrails;
    if (g_trailCount < kMaxTrails) ++g_trailCount;
}

static UINT g_anyInputCount = 0;
static UINT g_lastMsgSeen = 0;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    // log any non-trivial mouse / touch / input messages
    if (msg == WM_TOUCH || msg == WM_GESTURE || msg == WM_GESTURENOTIFY ||
        msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN ||
        msg == WM_MOUSEHOVER || msg == 0x0240 /* WM_TABLET_FIRST */ ||
        msg == 0x0246 || msg == 0x0247 || msg == WM_INPUT) {
        g_anyInputCount++;
        g_lastMsgSeen = msg;
        printf("[input] msg=0x%04X w=0x%p l=0x%p\n", msg, (void*)w, (void*)l);
    }

    switch (msg) {
    case WM_CREATE: {
        // Try with TWF_FINETOUCH | TWF_WANTPALM - some OS configurations only
        // dispatch WM_TOUCH when these flags are set.
        BOOL ok = RegisterTouchWindow(hwnd, TWF_FINETOUCH | TWF_WANTPALM);
        printf("[create] hwnd=0x%p RegisterTouchWindow(FINETOUCH|WANTPALM)=%d gle=%lu\n",
               (void*)hwnd, ok, GetLastError());
        ULONG flags = 0;
        BOOL is = IsTouchWindow(hwnd, &flags);
        printf("[create] IsTouchWindow=%d flags=0x%X\n", is, flags);

        // Subscribe to raw input from digitizer too
        RAWINPUTDEVICE rid = {0};
        rid.usUsagePage = 0x0D;  // Digitizers
        rid.usUsage = 0x04;      // Touch Screen
        rid.dwFlags = RIDEV_INPUTSINK;
        rid.hwndTarget = hwnd;
        BOOL rok = RegisterRawInputDevices(&rid, 1, sizeof(rid));
        printf("[create] RegisterRawInputDevices(touch screen)=%d gle=%lu\n", rok, GetLastError());

        SetTimer(hwnd, 1, 250, nullptr);
        return 0;
    }
    case WM_TIMER:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_INPUT: {
        UINT sz = 0;
        GetRawInputData((HRAWINPUT)l, RID_INPUT, nullptr, &sz, sizeof(RAWINPUTHEADER));
        BYTE buf[2048];
        if (sz <= sizeof(buf)) {
            GetRawInputData((HRAWINPUT)l, RID_INPUT, buf, &sz, sizeof(RAWINPUTHEADER));
            RAWINPUT* ri = (RAWINPUT*)buf;
            printf("[wm_input] type=%lu hDevice=0x%p hidDataSize=%lu count=%lu\n",
                   ri->header.dwType, (void*)ri->header.hDevice,
                   ri->data.hid.dwSizeHid, ri->data.hid.dwCount);
        }
        return DefWindowProcA(hwnd, msg, w, l);
    }
    case WM_TOUCH: {
        UINT n = LOWORD(w);
        TOUCHINPUT ti[10] = {0};
        if (n > 10) n = 10;
        if (GetTouchInputInfo((HTOUCHINPUT)l, n, ti, sizeof(TOUCHINPUT))) {
            g_touchFrames++;
            g_lastTouches = n;
            for (UINT i = 0; i < n; ++i) {
                POINT p = { ti[i].x / 100, ti[i].y / 100 };
                ScreenToClient(hwnd, &p);
                bool down = (ti[i].dwFlags & TOUCHEVENTF_UP) == 0;
                pushTrail(p, ti[i].dwID, down);
                g_touchEvts++;
            }
            CloseTouchInputHandle((HTOUCHINPUT)l);
            InvalidateRect(hwnd, nullptr, FALSE);
        } else {
            printf("[wm_touch] GetTouchInputInfo failed gle=%lu\n", GetLastError());
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r; GetClientRect(hwnd, &r);
        FillRect(dc, &r, (HBRUSH)(COLOR_WINDOW + 1));

        // Status text
        char buf[256];
        wsprintfA(buf, "frames=%u evts=%u lastN=%u  -- inject via gesture.exe; touch the window",
                  g_touchFrames, g_touchEvts, g_lastTouches);
        SetBkMode(dc, TRANSPARENT);
        TextOutA(dc, 8, 8, buf, lstrlenA(buf));

        DWORD now = GetTickCount();
        for (int i = 0; i < g_trailCount; ++i) {
            const Trail& t = g_trails[i];
            DWORD age = now - t.t;
            if (age > 5000) continue;
            int alpha = 255 - (int)(age * 255 / 5000);
            if (alpha < 16) alpha = 16;
            HBRUSH b = CreateSolidBrush(t.down
                ? RGB((t.id*73)%200+55, (t.id*131)%200+55, 200)
                : RGB(200, 80, 80));
            int rad = t.down ? 18 : 8;
            RECT cr = { t.pt.x - rad, t.pt.y - rad, t.pt.x + rad, t.pt.y + rad };
            FillRect(dc, &cr, b);
            DeleteObject(b);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:   DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd, msg, w, l);
}

// Self-inject: in a worker thread, bring this window to foreground and pump
// touch frames at its center. Verifies the entire chain from kernel driver
// up through WM_TOUCH dispatch in one process.
#include "../driver/public.h"

static HWND g_hwnd = nullptr;

static DWORD WINAPI SelfInjectThread(LPVOID) {
    HANDLE drv = CreateFileW(L"\\\\.\\HidTouchInject",
                             GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                             OPEN_EXISTING, 0, nullptr);
    if (drv == INVALID_HANDLE_VALUE) {
        printf("[self-inject] CreateFile failed gle=%lu\n", GetLastError());
        return 1;
    }
    Sleep(2000);  // let main window come up

    RECT r;
    GetWindowRect(g_hwnd, &r);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int cx = (r.left + r.right) / 2;
    int cy = (r.top + r.bottom) / 2;
    printf("[self-inject] window center screen=(%d,%d) of %dx%d\n", cx, cy, sw, sh);

    HIDTOUCH_INJECT_FRAME f = {0};
    f.ContactCount = 1;
    f.Contacts[0].ContactId = 0;
    f.Contacts[0].InRange = 1; f.Contacts[0].TipSwitch = 1;

    auto toX = [&](int x){ long v = (long)x * 32767 / sw; if (v<0)v=0; if (v>32767)v=32767; return (USHORT)v; };
    auto toY = [&](int y){ long v = (long)y * 32767 / sh; if (v<0)v=0; if (v>32767)v=32767; return (USHORT)v; };

    for (int i = 0; i < 30; ++i) {
        f.Contacts[0].X = toX(cx);
        f.Contacts[0].Y = toY(cy + i * 5);   // drift down a bit
        DWORD ret = 0;
        DeviceIoControl(drv, IOCTL_HIDTOUCH_INJECT, &f, sizeof(f), nullptr, 0, &ret, nullptr);
        Sleep(33);
    }
    f.Contacts[0].TipSwitch = 0; f.Contacts[0].InRange = 0;
    DWORD ret = 0;
    DeviceIoControl(drv, IOCTL_HIDTOUCH_INJECT, &f, sizeof(f), nullptr, 0, &ret, nullptr);
    printf("[self-inject] done 30 frames\n");
    CloseHandle(drv);

    // wait 3s for WM_TOUCH dispatch to drain, then write result + quit so an
    // automated runner (schtasks / SSH) can read the outcome from a file.
    Sleep(3000);
    FILE* fp = fopen("C:\\hidtouch\\selfinject_result.txt", "w");
    if (fp) {
        fprintf(fp, "frames=%u evts=%u lastN=%u anyInput=%u lastMsg=0x%04X\n",
                g_touchFrames, g_touchEvts, g_lastTouches, g_anyInputCount, g_lastMsgSeen);
        fclose(fp);
    }
    if (g_hwnd) PostMessageA(g_hwnd, WM_CLOSE, 0, 0);
    return 0;
}

int APIENTRY WinMain(HINSTANCE hi, HINSTANCE, LPSTR cmdline, int) {
    AllocConsole();
    freopen("CONOUT$", "w", stdout);

    bool selfInject = (cmdline && strstr(cmdline, "selfinject") != nullptr);

    int sm94 = GetSystemMetrics(94);
    printf("[start] SM_DIGITIZER=0x%X (NID_READY=%d MULTI_INPUT=%d INTEGRATED_TOUCH=%d)\n",
           sm94, (sm94 & 0x80) != 0, (sm94 & 0x40) != 0, (sm94 & 0x01) != 0);

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "TouchSink";
    RegisterClassA(&wc);

    HWND h = CreateWindowExA(WS_EX_TOPMOST, "TouchSink",
                             "TouchSink - WM_TOUCH receiver",
                             WS_OVERLAPPEDWINDOW,
                             100, 100, 1400, 800, nullptr, nullptr, hi, nullptr);
    if (!h) { printf("CreateWindow failed gle=%lu\n", GetLastError()); return 1; }
    g_hwnd = h;
    ShowWindow(h, SW_MAXIMIZE);
    UpdateWindow(h);
    SetForegroundWindow(h);

    if (selfInject) {
        printf("[main] self-inject mode: launching worker thread in 2s\n");
        CreateThread(nullptr, 0, SelfInjectThread, nullptr, 0, nullptr);
    }

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}
