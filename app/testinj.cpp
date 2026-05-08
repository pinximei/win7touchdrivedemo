// testinj.cpp — minimal console test that opens our driver and pumps a few
// fake touch frames. Verifies the IOCTL path between user mode and the driver,
// independently of the WM_TOUCH render path.
#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include "../driver/public.h"

#pragma comment(lib, "setupapi.lib")

int main()
{
    const wchar_t* path = L"\\\\.\\HidTouchInject";
    printf("opening %ls\n", path);
    HANDLE h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                          OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        printf(" CreateFile FAILED %lu\n", GetLastError());
    }

    if (h == INVALID_HANDLE_VALUE) {
        printf("no device opened\n");
        return 1;
    }
    printf("OPENED handle=%p\n", h);

    HIDTOUCH_INJECT_FRAME f = { 0 };
    f.ContactCount = 1;
    f.Contacts[0].ContactId = 0;
    f.Contacts[0].TipSwitch = 1;
    f.Contacts[0].InRange   = 1;
    f.Contacts[0].X = 16384;
    f.Contacts[0].Y = 16384;

    DWORD ret = 0;
    BOOL ok = DeviceIoControl(h, IOCTL_HIDTOUCH_INJECT, &f, sizeof(f),
                              nullptr, 0, &ret, nullptr);
    printf("IOCTL_HIDTOUCH_INJECT ok=%d gle=%lu ret=%lu\n", ok, GetLastError(), ret);

    Sleep(100);

    f.Contacts[0].TipSwitch = 0;
    f.Contacts[0].InRange   = 0;
    ok = DeviceIoControl(h, IOCTL_HIDTOUCH_INJECT, &f, sizeof(f),
                         nullptr, 0, &ret, nullptr);
    printf("IOCTL up ok=%d gle=%lu\n", ok, GetLastError());

    CloseHandle(h);
    return 0;
}
