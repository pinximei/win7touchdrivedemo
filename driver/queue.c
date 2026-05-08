// queue.c — request dispatch (HID minidriver path + private IOCTL path)
#include "driver.h"

VOID
HidTouchEvtIoInternalDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
    )
{
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    WDFDEVICE       device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT ctx    = GetDeviceContext(device);
    NTSTATUS        status;
    BOOLEAN         pending = FALSE;

    switch (IoControlCode) {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
        status = HidTouchGetHidDescriptor(device, Request);
        break;
    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
        status = HidTouchGetReportDescriptor(device, Request);
        break;
    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
        status = HidTouchGetDeviceAttributes(Request);
        break;
    case IOCTL_HID_GET_STRING:
        status = HidTouchGetString(device, Request);
        break;
    case IOCTL_HID_READ_REPORT:
        status = HidTouchReadReport(ctx, Request);
        if (status == STATUS_PENDING) pending = TRUE;
        break;
    case IOCTL_HID_GET_FEATURE:
        status = HidTouchGetFeature(Request);
        break;
    case IOCTL_HID_SET_FEATURE:
        status = HidTouchSetFeature(Request);
        break;
    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    if (!pending) {
        WdfRequestComplete(Request, status);
    }
}

VOID
HidTouchEvtIoDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
    )
{
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    UNREFERENCED_PARAMETER(Queue);
    NTSTATUS status;

    switch (IoControlCode) {
    case IOCTL_HIDTOUCH_INJECT:
        // Forward through the same global path as the control device.
        status = HidTouchInjectGlobal(Request);
        break;
    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    WdfRequestComplete(Request, status);
}
