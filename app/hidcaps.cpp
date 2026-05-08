// hidcaps.cpp -- enumerate all HID devices, dump HardwareID + parsed Caps.
// Goal: see if OS tagged any of our touch collections as SYSTEM_TOUCHSCREEN.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "user32.lib")

int main() {
    GUID hidguid;
    HidD_GetHidGuid(&hidguid);
    HDEVINFO hdi = SetupDiGetClassDevs(&hidguid, nullptr, nullptr,
                                       DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    SP_DEVICE_INTERFACE_DATA did = {0}; did.cbSize = sizeof(did);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hdi, nullptr, &hidguid, i, &did); ++i) {
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetailW(hdi, &did, nullptr, 0, &needed, nullptr);
        if (!needed) continue;
        PSP_DEVICE_INTERFACE_DETAIL_DATA_W dd =
            (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)malloc(needed);
        if (!dd) continue;
        dd->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        SP_DEVINFO_DATA dev = {0}; dev.cbSize = sizeof(dev);
        if (!SetupDiGetDeviceInterfaceDetailW(hdi, &did, dd, needed, nullptr, &dev)) {
            free(dd); continue;
        }

        // Get HardwareID
        WCHAR hwid[1024] = {0};
        SetupDiGetDeviceRegistryPropertyW(hdi, &dev, SPDRP_HARDWAREID, nullptr,
                                         (PBYTE)hwid, sizeof(hwid), nullptr);

        // Try to open and probe caps
        HANDLE h = CreateFileW(dd->DevicePath,
                              GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, 0, nullptr);
        wprintf(L"\n=== HID instance %u ===\n", i);
        wprintf(L"  hwid: %ls\n", hwid);
        wprintf(L"  path: %ls\n", dd->DevicePath);

        if (h == INVALID_HANDLE_VALUE) {
            wprintf(L"  CreateFile failed gle=%lu\n", GetLastError());
            free(dd); continue;
        }
        HIDD_ATTRIBUTES attrs = {0}; attrs.Size = sizeof(attrs);
        if (HidD_GetAttributes(h, &attrs)) {
            wprintf(L"  VID=0x%04X PID=0x%04X Ver=0x%04X\n",
                   attrs.VendorID, attrs.ProductID, attrs.VersionNumber);
        }
        PHIDP_PREPARSED_DATA pp = nullptr;
        if (HidD_GetPreparsedData(h, &pp)) {
            HIDP_CAPS caps = {0};
            if (HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS) {
                wprintf(L"  UsagePage=0x%04X Usage=0x%04X\n",
                       caps.UsagePage, caps.Usage);
                wprintf(L"  InputReportByteLen=%u  FeatureReportByteLen=%u\n",
                       caps.InputReportByteLength, caps.FeatureReportByteLength);
                wprintf(L"  NumberInputButtonCaps=%u  NumberInputValueCaps=%u\n",
                       caps.NumberInputButtonCaps, caps.NumberInputValueCaps);

                // Check key Usages we expect on a Touch Screen TLC
                if (caps.UsagePage == 0x0D && caps.Usage == 0x04) {
                    wprintf(L"  ** Touch Screen TLC **\n");
                }
            }
            HidD_FreePreparsedData(pp);
        }
        CloseHandle(h);
        free(dd);
    }
    SetupDiDestroyDeviceInfoList(hdi);
    return 0;
}
