// driver.h — KMDF driver-private declarations
#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>
#include "public.h"

// Per-device extension (FDO context).
typedef struct _DEVICE_CONTEXT {
    WDFDEVICE  Device;
    // (Pending HID Reads no longer live here — see g_ReadReportQueue. Kept
    //  to keep callers compiling; not used.)
    WDFQUEUE   ReadReportQueue;
    HIDTOUCH_INJECT_FRAME LastFrame;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

// Single control device shared across all FDO instances; lets user-mode
// apps reach us via a fixed \\.\HidTouchInject device name regardless of
// what HIDClass/mshidkmdf do to the device stack.
extern WDFDEVICE       g_ControlDevice;
extern WDFQUEUE        g_ReadReportQueue;
extern HIDTOUCH_INJECT_FRAME g_LastFrame;
extern HIDTOUCH_STATS  g_Stats;
NTSTATUS HidTouchCreateControlDevice(WDFDRIVER Driver);

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

// driver.c
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD HidTouchEvtDeviceAdd;
EVT_WDFDEVICE_WDM_IRP_PREPROCESS HidTouchEvtWdmPreprocessQueryId;
EVT_WDF_OBJECT_CONTEXT_CLEANUP HidTouchEvtDeviceCleanup;

// queue.c
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL HidTouchEvtIoInternalDeviceControl;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL          HidTouchEvtIoDeviceControl;

// hid.c
NTSTATUS HidTouchGetHidDescriptor(_In_ WDFDEVICE Device, _In_ WDFREQUEST Request);
NTSTATUS HidTouchGetReportDescriptor(_In_ WDFDEVICE Device, _In_ WDFREQUEST Request);
NTSTATUS HidTouchGetDeviceAttributes(_In_ WDFREQUEST Request);
NTSTATUS HidTouchGetString(_In_ WDFDEVICE Device, _In_ WDFREQUEST Request);
NTSTATUS HidTouchReadReport(_In_ PDEVICE_CONTEXT Ctx, _In_ WDFREQUEST Request);
NTSTATUS HidTouchGetFeature(_In_ WDFREQUEST Request);
NTSTATUS HidTouchSetFeature(_In_ WDFREQUEST Request);

// inject.c
NTSTATUS HidTouchInject(_In_ PDEVICE_CONTEXT Ctx, _In_ WDFREQUEST Request);
NTSTATUS HidTouchInjectGlobal(_In_ WDFREQUEST Request);
EVT_WDF_IO_QUEUE_IO_CANCELED_ON_QUEUE HidTouchEvtReadCanceled;
