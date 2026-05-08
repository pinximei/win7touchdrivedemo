// autotest.cpp -- run touchsink as child, force it foreground, then inject
// gestures. Reads frames from selfinject_result.txt afterwards.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "../driver/public.h"

#pragma comment(lib, "user32.lib")

static HANDLE g_drv = INVALID_HANDLE_VALUE;

static USHORT toX(int px, int sw) {
    long v = (long)px * 32767 / (sw > 0 ? sw : 1);
    if (v < 0) v = 0; if (v > 32767) v = 32767;
    return (USHORT)v;
}
static USHORT toY(int px, int sh) {
    long v = (long)px * 32767 / (sh > 0 ? sh : 1);
    if (v < 0) v = 0; if (v > 32767) v = 32767;
    return (USHORT)v;
}

static bool inject(const HIDTOUCH_INJECT_FRAME& f) {
    DWORD ret = 0;
    HIDTOUCH_INJECT_FRAME copy = f;
    return DeviceIoControl(g_drv, IOCTL_HIDTOUCH_INJECT,
                           &copy, sizeof(copy), nullptr, 0, &ret, nullptr) != 0;
}

int main(int argc, char** argv) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int cx = sw / 2, cy = sh / 2;
    printf("[autotest] screen=%dx%d, target_center=(%d,%d)\n", sw, sh, cx, cy);

    // Find foreground window
    HWND fg = GetForegroundWindow();
    char cls[256] = {0}, title[256] = {0};
    if (fg) {
        GetClassNameA(fg, cls, sizeof(cls));
        GetWindowTextA(fg, title, sizeof(title));
    }
    printf("[autotest] foreground hwnd=0x%p class='%s' title='%s'\n", fg, cls, title);

    // Open driver
    g_drv = CreateFileW(L"\\\\.\\HidTouchInject",
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                        OPEN_EXISTING, 0, nullptr);
    if (g_drv == INVALID_HANDLE_VALUE) {
        printf("[autotest] CreateFile failed gle=%lu\n", GetLastError());
        return 1;
    }

    // Inject 1-finger up-swipe centered at screen center, 30 steps
    HIDTOUCH_INJECT_FRAME f = {0};
    f.ContactCount = 1;
    f.Contacts[0].ContactId = 0;
    f.Contacts[0].InRange = 1; f.Contacts[0].TipSwitch = 1;
    f.Contacts[0].X = toX(cx, sw);
    f.Contacts[0].Y = toY(cy + 150, sh);
    inject(f); Sleep(20);

    int sx = cx, sy = cy + 150, ex = cx, ey = cy - 150;
    for (int i = 1; i <= 30; ++i) {
        int x = sx + (ex - sx) * i / 30;
        int y = sy + (ey - sy) * i / 30;
        f.Contacts[0].X = toX(x, sw);
        f.Contacts[0].Y = toY(y, sh);
        inject(f); Sleep(10);
    }
    f.Contacts[0].TipSwitch = 0; f.Contacts[0].InRange = 0;
    inject(f);
    printf("[autotest] injected 32 frames at (%d,%d)\n", cx, cy);

    CloseHandle(g_drv);
    return 0;
}
