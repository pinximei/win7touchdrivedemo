// injtest.cpp — headless test: inject 1-finger swipe at screen center and
// observe mouse cursor position before/during/after to determine if
// touch->mouse emulation is wired up by win32k for our virtual device.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

#include "../driver/public.h"

#pragma comment(lib, "user32.lib")

static HANDLE g_Driver = INVALID_HANDLE_VALUE;

static bool OpenDriver()
{
    g_Driver = CreateFileW(L"\\\\.\\HidTouchInject",
                          GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                          OPEN_EXISTING, 0, nullptr);
    return g_Driver != INVALID_HANDLE_VALUE;
}

static bool InjectFrame(const HIDTOUCH_INJECT_FRAME& f)
{
    if (g_Driver == INVALID_HANDLE_VALUE) return false;
    DWORD ret = 0;
    HIDTOUCH_INJECT_FRAME copy = f;
    return DeviceIoControl(g_Driver, IOCTL_HIDTOUCH_INJECT,
                           &copy, sizeof(copy), nullptr, 0, &ret, nullptr) != 0;
}

static USHORT ToHidX(int px) {
    int w = GetSystemMetrics(SM_CXSCREEN); if (w <= 0) w = 1;
    long v = (long)px * 32767 / w;
    if (v < 0) v = 0; if (v > 32767) v = 32767;
    return (USHORT)v;
}
static USHORT ToHidY(int px) {
    int h = GetSystemMetrics(SM_CYSCREEN); if (h <= 0) h = 1;
    long v = (long)px * 32767 / h;
    if (v < 0) v = 0; if (v > 32767) v = 32767;
    return (USHORT)v;
}

static void PrintCursor(const char* tag) {
    POINT p; GetCursorPos(&p);
    printf("[%s] cursor=(%d,%d)\n", tag, p.x, p.y);
}

static void PrintWindowAt(int x, int y, const char* tag) {
    POINT pt = { x, y };
    HWND h = WindowFromPoint(pt);
    char cls[128] = {0}; char title[128] = {0};
    if (h) { GetClassNameA(h, cls, 128); GetWindowTextA(h, title, 128); }
    printf("[%s] WindowFromPoint(%d,%d)=%p class='%s' title='%s'\n",
           tag, x, y, (void*)h, cls, title);
}

int main()
{
    int cx = GetSystemMetrics(SM_CXSCREEN);
    int cy = GetSystemMetrics(SM_CYSCREEN);
    int mx = cx / 2;
    int my = cy / 2;
    printf("Screen=%dx%d  center=(%d,%d)\n", cx, cy, mx, my);

    if (!OpenDriver()) {
        printf("OpenDriver failed gle=%lu\n", GetLastError());
        return 1;
    }
    printf("Driver opened.\n");

    PrintCursor("BEFORE");
    PrintWindowAt(mx, my, "BEFORE");

    // Move physical cursor far away so we can see if touch emulation moves it
    SetCursorPos(0, 0);
    Sleep(100);
    PrintCursor("AFTER_SetCursorPos(0,0)");

    // 1-finger tap at center
    HIDTOUCH_INJECT_FRAME f = {0};
    f.ContactCount = 1;
    f.Contacts[0].ContactId = 0;
    f.Contacts[0].InRange = 1;
    f.Contacts[0].TipSwitch = 1;
    f.Contacts[0].X = ToHidX(mx);
    f.Contacts[0].Y = ToHidY(my);

    printf("\n--- Injecting tip-down at (%d,%d) ---\n", mx, my);
    InjectFrame(f);
    Sleep(50);
    PrintCursor("AFTER_DOWN");

    printf("\n--- Injecting 5 move frames toward (%d,%d-200) ---\n", mx, my);
    for (int i = 1; i <= 5; ++i) {
        int y = my - i * 40;
        f.Contacts[0].X = ToHidX(mx);
        f.Contacts[0].Y = ToHidY(y);
        f.Contacts[0].TipSwitch = 1;
        f.Contacts[0].InRange = 1;
        InjectFrame(f);
        Sleep(50);
        char tag[32]; sprintf(tag, "MOVE_%d", i);
        PrintCursor(tag);
    }

    printf("\n--- Injecting tip-up ---\n");
    f.Contacts[0].TipSwitch = 0;
    f.Contacts[0].InRange = 0;
    InjectFrame(f);
    Sleep(50);
    PrintCursor("AFTER_UP");

    PrintWindowAt(mx, my, "AFTER_UP");

    CloseHandle(g_Driver);
    return 0;
}
