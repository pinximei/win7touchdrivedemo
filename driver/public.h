// public.h — IOCTLs and shared structs between driver and user-mode app
#pragma once

// User-mode callers need <winioctl.h> for CTL_CODE/METHOD_BUFFERED.
// Kernel-mode callers get those from <ntddk.h> already.
#ifndef _KERNEL_MODE
#  include <windows.h>
#  include <winioctl.h>
#endif

#include <initguid.h>

// {A6F2D9C8-1E4B-4F73-9D1C-7B0F5C8E2A41}
DEFINE_GUID(GUID_DEVINTERFACE_HIDTOUCH,
    0xa6f2d9c8, 0x1e4b, 0x4f73, 0x9d, 0x1c, 0x7b, 0x0f, 0x5c, 0x8e, 0x2a, 0x41);

#define HIDTOUCH_MAX_CONTACTS 10

#pragma pack(push, 1)
typedef struct _HIDTOUCH_CONTACT {
    UCHAR  ContactId;     // 0..9
    UCHAR  TipSwitch;     // 0/1 finger touching surface
    UCHAR  InRange;       // 0/1 finger detected (hover allowed if Tip=0,InRange=1)
    UCHAR  Reserved;
    USHORT X;             // 0..32767 logical
    USHORT Y;             // 0..32767 logical
} HIDTOUCH_CONTACT, *PHIDTOUCH_CONTACT;

typedef struct _HIDTOUCH_INJECT_FRAME {
    UCHAR             ContactCount;
    UCHAR             Reserved[3];
    HIDTOUCH_CONTACT  Contacts[HIDTOUCH_MAX_CONTACTS];
} HIDTOUCH_INJECT_FRAME, *PHIDTOUCH_INJECT_FRAME;
#pragma pack(pop)

#define FILE_DEVICE_HIDTOUCH 0x8765

#define IOCTL_HIDTOUCH_INJECT \
    CTL_CODE(FILE_DEVICE_HIDTOUCH, 0x800, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#pragma pack(push, 1)
typedef struct _HIDTOUCH_STATS {
    ULONG InjectCalls;       // # IOCTL_HIDTOUCH_INJECT
    ULONG ReadRequests;      // # IOCTL_HID_READ_REPORT (parked / completed)
    ULONG ReadCompletions;   // # IOCTL_HID_READ_REPORT we completed via inject
    ULONG GetHidDescCalls;
    ULONG GetReportDescCalls;
    ULONG GetAttribCalls;
    ULONG GetStringCalls;
    ULONG GetFeatureCalls;
    ULONG SetFeatureCalls;   // # IOCTL_HID_SET_FEATURE
    ULONG LastDeviceMode;    // last SET_FEATURE Device Mode value
    ULONG WriteReportCalls;  // # IOCTL_HID_WRITE_REPORT
    ULONG ReadForwardOk;     // # READ IRPs successfully WdfRequestForwardToIoQueue'd
    ULONG ReadForwardFail;   // # READ IRPs that failed to forward (last status in LastReadFwdStatus)
    ULONG LastReadFwdStatus; // last failure status from forward
    ULONG InjectQueueEmpty;  // # InjectGlobal calls that found no pending read
    ULONG InjectQueueGot;    // # InjectGlobal calls that pulled a read out (== ReadCompletions)
    ULONG ReadsCanceled;     // # READ IRPs that were cancelled while parked on the manual queue
    ULONG ReportSize;        // sizeof(HIDTOUCH_INPUT_REPORT) -- byte count we send to hidclass
} HIDTOUCH_STATS, *PHIDTOUCH_STATS;
#pragma pack(pop)

#define IOCTL_HIDTOUCH_GET_STATS \
    CTL_CODE(FILE_DEVICE_HIDTOUCH, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS)
