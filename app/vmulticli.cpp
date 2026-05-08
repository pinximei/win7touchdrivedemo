// vmulticli.cpp -- minimal vmulti client. Finds the vmulti HID device by
// VID/PID, then sends a 2-finger swipe gesture via HidD_SetOutputReport
// using REPORTID_CONTROL wrapper.
//
// Usage:
//   vmulticli.exe scroll
//   vmulticli.exe zoom
//   vmulticli.exe both
//
// Built against vmulti's vmulticommon.h (reuses struct definitions).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vmulti/vmulticommon.h"

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "user32.lib")

// vmulti VID / PID (from vmulticommon.h: VMULTI_VID=0x00FF, VMULTI_PID=0xBACC)

// Find vmulti's Vendor Defined collection (UsagePage 0xFF00, Usage 0x01).
// vmulti exposes multiple TLCs as separate HID children -- we need the
// vendor one because that's where REPORTID_CONTROL output reports are
// accepted. The Touch Screen TLC won't accept SetOutputReport.
static HANDLE FindVmultiDevice() {
    GUID hidguid;
    HidD_GetHidGuid(&hidguid);

    HDEVINFO hdi = SetupDiGetClassDevs(&hidguid, nullptr, nullptr,
                                       DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hdi == INVALID_HANDLE_VALUE) {
        printf("SetupDiGetClassDevs failed gle=%lu\n", GetLastError());
        return INVALID_HANDLE_VALUE;
    }

    SP_DEVICE_INTERFACE_DATA did = {0};
    did.cbSize = sizeof(did);

    HANDLE found = INVALID_HANDLE_VALUE;
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hdi, nullptr, &hidguid, i, &did); ++i) {
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetailW(hdi, &did, nullptr, 0, &needed, nullptr);
        if (needed == 0) continue;

        PSP_DEVICE_INTERFACE_DETAIL_DATA_W dd =
            (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)malloc(needed);
        if (!dd) continue;
        dd->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(hdi, &did, dd, needed, nullptr, nullptr)) {
            free(dd); continue;
        }

        HANDLE h = CreateFileW(dd->DevicePath,
                              GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) { free(dd); continue; }

        HIDD_ATTRIBUTES attrs = {0};
        attrs.Size = sizeof(attrs);
        if (HidD_GetAttributes(h, &attrs) &&
            attrs.VendorID == VMULTI_VID && attrs.ProductID == VMULTI_PID)
        {
            // Inspect this collection's UsagePage/Usage
            PHIDP_PREPARSED_DATA pp = nullptr;
            if (HidD_GetPreparsedData(h, &pp)) {
                HIDP_CAPS caps = {0};
                if (HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS) {
                    printf("  candidate: UsagePage=0x%04X Usage=0x%04X path=%ls\n",
                           caps.UsagePage, caps.Usage, dd->DevicePath);
                    // Vendor Defined (0xFF00) is the one that takes
                    // REPORTID_CONTROL OUTPUT reports.
                    if (caps.UsagePage == 0xFF00 && caps.Usage == 0x01) {
                        printf("  --> using vendor-defined collection\n");
                        HidD_FreePreparsedData(pp);
                        free(dd);
                        SetupDiDestroyDeviceInfoList(hdi);
                        return h;
                    }
                }
                HidD_FreePreparsedData(pp);
            }
        }
        CloseHandle(h);
        free(dd);
    }
    SetupDiDestroyDeviceInfoList(hdi);
    return INVALID_HANDLE_VALUE;
}

static USHORT toX(int px) {
    int w = GetSystemMetrics(SM_CXSCREEN); if (w <= 0) w = 1;
    long v = (long)px * 0x7FFF / w;
    if (v < 0) v = 0; if (v > 0x7FFF) v = 0x7FFF;
    return (USHORT)v;
}
static USHORT toY(int px) {
    int h = GetSystemMetrics(SM_CYSCREEN); if (h <= 0) h = 1;
    long v = (long)px * 0x7FFF / h;
    if (v < 0) v = 0; if (v > 0x7FFF) v = 0x7FFF;
    return (USHORT)v;
}

// Build CONTROL_REPORT_SIZE = 0x41 (65 byte) packet
//   byte 0: REPORTID_CONTROL
//   byte 1: ReportLength (sizeof VMultiMultiTouchReport)
//   bytes 2..: VMultiMultiTouchReport
static bool sendMtFrame(HANDLE h, TOUCH* contacts, BYTE n) {
    BYTE buf[CONTROL_REPORT_SIZE] = {0};
    buf[0] = REPORTID_CONTROL;
    buf[1] = (BYTE)sizeof(VMultiMultiTouchReport);

    VMultiMultiTouchReport* mt = (VMultiMultiTouchReport*)(buf + sizeof(VMultiControlReportHeader));
    mt->ReportID = REPORTID_MTOUCH;
    // vmulti packs 2 contacts per multitouch report; for >2 contacts, send
    // multiple reports with ActualCount only on the first.
    BYTE sent = 0;
    while (sent < n) {
        memset(mt->Touch, 0, sizeof(mt->Touch));
        mt->Touch[0] = contacts[sent];
        if (sent + 1 < n) mt->Touch[1] = contacts[sent + 1];
        mt->ActualCount = (sent == 0) ? n : 0;
        if (!HidD_SetOutputReport(h, buf, CONTROL_REPORT_SIZE)) {
            printf("HidD_SetOutputReport failed gle=%lu\n", GetLastError());
            return false;
        }
        sent += 2;
    }
    return true;
}

static void scroll1f(HANDLE h, int sx, int sy, int ex, int ey, int steps) {
    TOUCH t = {0};
    t.ContactID = 0;
    t.Status = MULTI_TIPSWITCH_BIT | MULTI_IN_RANGE_BIT | MULTI_CONFIDENCE_BIT;
    t.XValue = toX(sx); t.YValue = toY(sy);
    sendMtFrame(h, &t, 1); Sleep(20);
    for (int i = 1; i <= steps; ++i) {
        int x = sx + (ex - sx) * i / steps;
        int y = sy + (ey - sy) * i / steps;
        t.XValue = toX(x); t.YValue = toY(y);
        sendMtFrame(h, &t, 1); Sleep(10);
    }
    // tip up
    t.Status = 0;
    sendMtFrame(h, &t, 1);
}

static void pinch2f(HANDLE h, int cx, int cy, int s, int e, int steps) {
    TOUCH t[2] = {0};
    for (int k = 0; k < 2; ++k) {
        t[k].ContactID = (BYTE)k;
        t[k].Status = MULTI_TIPSWITCH_BIT | MULTI_IN_RANGE_BIT | MULTI_CONFIDENCE_BIT;
    }
    auto build = [&](int sep) {
        for (int k = 0; k < 2; ++k) {
            int sign = (k == 0) ? -1 : +1;
            t[k].XValue = toX(cx + sign * sep / 2);
            t[k].YValue = toY(cy);
        }
    };
    build(s); sendMtFrame(h, t, 2); Sleep(20);
    for (int i = 1; i <= steps; ++i) {
        int sep = s + (e - s) * i / steps;
        build(sep); sendMtFrame(h, t, 2); Sleep(10);
    }
    // tip up both
    t[0].Status = 0; t[1].Status = 0;
    sendMtFrame(h, t, 2);
}

int main(int argc, char** argv) {
    const char* mode = (argc > 1) ? argv[1] : "both";
    HANDLE h = FindVmultiDevice();
    if (h == INVALID_HANDLE_VALUE) {
        printf("vmulti device not found. Is the driver installed and started?\n");
        return 1;
    }
    int cx = GetSystemMetrics(SM_CXSCREEN) / 2;
    int cy = GetSystemMetrics(SM_CYSCREEN) / 2;
    printf("screen %dx%d, mode=%s\n", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), mode);
    if (strcmp(mode, "scroll") == 0 || strcmp(mode, "both") == 0) {
        printf("scroll: 1-finger swipe\n");
        scroll1f(h, cx, cy + 150, cx, cy - 150, 30);
    }
    if (strcmp(mode, "zoom") == 0 || strcmp(mode, "both") == 0) {
        Sleep(200);
        printf("zoom: 2-finger pinch-out\n");
        pinch2f(h, cx, cy, 80, 320, 30);
    }
    CloseHandle(h);
    printf("done\n");
    return 0;
}
