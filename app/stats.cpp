// stats.cpp — read driver stats counters via control device IOCTL.
#include <windows.h>
#include <stdio.h>
#include "../driver/public.h"

int main()
{
    HANDLE h = CreateFileW(L"\\\\.\\HidTouchInject",
                          GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                          OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateFile failed %lu\n", GetLastError());
        return 1;
    }
    HIDTOUCH_STATS s = {0};
    DWORD got = 0;
    BOOL ok = DeviceIoControl(h, IOCTL_HIDTOUCH_GET_STATS,
                              nullptr, 0, &s, sizeof(s), &got, nullptr);
    if (!ok) {
        printf("IOCTL_HIDTOUCH_GET_STATS failed %lu\n", GetLastError());
        CloseHandle(h);
        return 2;
    }
    printf("InjectCalls       = %u\n", s.InjectCalls);
    printf("ReadRequests      = %u  (HID READ_REPORTs received from hidclass)\n", s.ReadRequests);
    printf("ReadCompletions   = %u  (reads completed via inject)\n", s.ReadCompletions);
    printf("GetHidDescCalls   = %u\n", s.GetHidDescCalls);
    printf("GetReportDescCalls= %u\n", s.GetReportDescCalls);
    printf("GetAttribCalls    = %u\n", s.GetAttribCalls);
    printf("GetStringCalls    = %u\n", s.GetStringCalls);
    printf("GetFeatureCalls   = %u\n", s.GetFeatureCalls);
    printf("SetFeatureCalls   = %u  (mtconfig sets device mode)\n", s.SetFeatureCalls);
    printf("LastDeviceMode    = %u  (2 = multi-touch active)\n", s.LastDeviceMode);
    printf("WriteReportCalls  = %u\n", s.WriteReportCalls);
    printf("ReadForwardOk     = %u  (READs successfully forwarded to manual queue)\n", s.ReadForwardOk);
    printf("ReadForwardFail   = %u  (READs that failed forward; status in LastReadFwdStatus)\n", s.ReadForwardFail);
    printf("LastReadFwdStatus = 0x%08X\n", s.LastReadFwdStatus);
    printf("InjectQueueEmpty  = %u  (inject IOCTLs that found queue empty)\n", s.InjectQueueEmpty);
    printf("InjectQueueGot    = %u  (inject IOCTLs that pulled a read out)\n", s.InjectQueueGot);
    printf("ReadsCanceled     = %u  (READs cancelled while parked on manual queue)\n", s.ReadsCanceled);
    printf("ReportSize        = %u  (sizeof HID input report = bytes pushed per inject)\n", s.ReportSize);
    printf("(driver returned %lu bytes of stats struct; user-mode expected %d)\n",
           got, (int)sizeof(HIDTOUCH_STATS));
    CloseHandle(h);
    return 0;
}
