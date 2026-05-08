// gesture.cpp -- headless gesture player. Plays scroll or pinch gestures
// directly via IOCTL. Useful for SSH-driven testing without WM_COMMAND.
//
// Usage:
//   gesture.exe scroll
//   gesture.exe zoom
//   gesture.exe both
//   gesture.exe focus <hwnd>           pick foreground before gesture
//   gesture.exe scroll <className>     find window by class, focus it, then scroll
//   gesture.exe zoom   <className>     find window by class, focus it, then zoom
//   examples:
//     gesture.exe scroll IEFrame
//     gesture.exe zoom   Notepad
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../driver/public.h"

static HANDLE g_h = INVALID_HANDLE_VALUE;

static USHORT toX(int px) {
    int w = GetSystemMetrics(SM_CXSCREEN); if (w <= 0) w = 1;
    long v = (long)px * 32767 / w;
    if (v < 0) v = 0; if (v > 32767) v = 32767;
    return (USHORT)v;
}
static USHORT toY(int px) {
    int h = GetSystemMetrics(SM_CYSCREEN); if (h <= 0) h = 1;
    long v = (long)px * 32767 / h;
    if (v < 0) v = 0; if (v > 32767) v = 32767;
    return (USHORT)v;
}

static bool inject(const HIDTOUCH_INJECT_FRAME& f) {
    DWORD ret = 0;
    HIDTOUCH_INJECT_FRAME copy = f;
    return DeviceIoControl(g_h, IOCTL_HIDTOUCH_INJECT,
                           &copy, sizeof(copy), nullptr, 0, &ret, nullptr) != 0;
}

static void scroll1f(int sx, int sy, int ex, int ey, int steps) {
    HIDTOUCH_INJECT_FRAME f = {0};
    f.ContactCount = 1;
    f.Contacts[0].ContactId = 0;
    f.Contacts[0].InRange = 1; f.Contacts[0].TipSwitch = 1;
    f.Contacts[0].X = toX(sx); f.Contacts[0].Y = toY(sy);
    inject(f); Sleep(20);
    for (int i = 1; i <= steps; ++i) {
        int x = sx + (ex - sx) * i / steps;
        int y = sy + (ey - sy) * i / steps;
        f.Contacts[0].X = toX(x); f.Contacts[0].Y = toY(y);
        inject(f); Sleep(10);
    }
    f.Contacts[0].TipSwitch = 0; f.Contacts[0].InRange = 0;
    inject(f);
}

static void pinch2f(int cx, int cy, int s, int e, int steps) {
    auto build = [&](int sep) {
        HIDTOUCH_INJECT_FRAME f = {0};
        f.ContactCount = 2;
        for (int k = 0; k < 2; ++k) {
            int sign = (k == 0) ? -1 : +1;
            f.Contacts[k].ContactId = (UCHAR)k;
            f.Contacts[k].InRange = 1; f.Contacts[k].TipSwitch = 1;
            f.Contacts[k].X = toX(cx + sign * sep / 2);
            f.Contacts[k].Y = toY(cy);
        }
        return f;
    };
    HIDTOUCH_INJECT_FRAME f = build(s);
    inject(f); Sleep(20);
    for (int i = 1; i <= steps; ++i) {
        int sep = s + (e - s) * i / steps;
        f = build(sep);
        inject(f); Sleep(10);
    }
    f.Contacts[0].TipSwitch = 0; f.Contacts[0].InRange = 0;
    f.Contacts[1].TipSwitch = 0; f.Contacts[1].InRange = 0;
    inject(f);
}

// Find a top-level window by class name (e.g. "IEFrame") and bring to front.
// Returns the window's center point on success.
static bool focusByClass(const char* cls, int* cx, int* cy) {
    HWND h = FindWindowA(cls, nullptr);
    if (!h) {
        printf("focus: class '%s' not found\n", cls);
        return false;
    }
    // Bring to foreground. Win7 has SetForegroundWindow restrictions; the
    // AttachThreadInput dance is the standard workaround.
    DWORD myT = GetCurrentThreadId();
    DWORD fgT = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    AttachThreadInput(myT, fgT, TRUE);
    ShowWindow(h, SW_SHOW);
    SetForegroundWindow(h);
    SetActiveWindow(h);
    BringWindowToTop(h);
    AttachThreadInput(myT, fgT, FALSE);
    Sleep(150);

    RECT r;
    if (!GetWindowRect(h, &r)) return false;
    *cx = (r.left + r.right) / 2;
    *cy = (r.top + r.bottom) / 2;

    char title[256] = {0};
    GetWindowTextA(h, title, sizeof(title));
    printf("focused hwnd=0x%p class='%s' title='%s' rect=(%ld,%ld,%ld,%ld) center=(%d,%d)\n",
           (void*)h, cls, title, r.left, r.top, r.right, r.bottom, *cx, *cy);
    return true;
}

int main(int argc, char** argv) {
    const char* mode = (argc > 1) ? argv[1] : "both";
    const char* cls  = (argc > 2) ? argv[2] : nullptr;

    g_h = CreateFileW(L"\\\\.\\HidTouchInject",
                      GENERIC_READ | GENERIC_WRITE,
                      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                      OPEN_EXISTING, 0, nullptr);
    if (g_h == INVALID_HANDLE_VALUE) {
        printf("CreateFile failed gle=%lu\n", GetLastError());
        return 1;
    }
    int cx = GetSystemMetrics(SM_CXSCREEN) / 2;
    int cy = GetSystemMetrics(SM_CYSCREEN) / 2;
    if (cls) {
        if (!focusByClass(cls, &cx, &cy)) return 2;
    }
    printf("screen %dx%d, gesture mode=%s, target=(%d,%d)\n",
           GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), mode, cx, cy);

    if (strcmp(mode, "scroll") == 0 || strcmp(mode, "both") == 0) {
        printf("scroll: 1-finger up-swipe (%d,%d)->(%d,%d) 30 steps\n",
               cx, cy + 150, cx, cy - 150);
        scroll1f(cx, cy + 150, cx, cy - 150, 30);
    }
    if (strcmp(mode, "zoom") == 0 || strcmp(mode, "both") == 0) {
        Sleep(200);
        printf("zoom: 2-finger pinch-out at (%d,%d) sep 80->320 30 steps\n", cx, cy);
        pinch2f(cx, cy, 80, 320, 30);
    }
    CloseHandle(g_h);
    printf("done\n");
    return 0;
}
