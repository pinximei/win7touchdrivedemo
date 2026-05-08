// hid.c — HID minidriver request handlers (descriptors, attributes, read, feature)
#include "driver.h"
#include "descriptor.h"

NTSTATUS
HidTouchGetHidDescriptor(_In_ WDFDEVICE Device, _In_ WDFREQUEST Request)
{
    UNREFERENCED_PARAMETER(Device);
    g_Stats.GetHidDescCalls++;
    NTSTATUS status;
    void*    out;
    size_t   outLen;

    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(g_HidTouchHidDescriptor),
                                            &out, &outLen);
    if (!NT_SUCCESS(status)) return status;

    if (outLen < sizeof(g_HidTouchHidDescriptor)) return STATUS_BUFFER_TOO_SMALL;

    RtlCopyMemory(out, &g_HidTouchHidDescriptor, sizeof(g_HidTouchHidDescriptor));
    WdfRequestSetInformation(Request, sizeof(g_HidTouchHidDescriptor));
    return STATUS_SUCCESS;
}

NTSTATUS
HidTouchGetReportDescriptor(_In_ WDFDEVICE Device, _In_ WDFREQUEST Request)
{
    UNREFERENCED_PARAMETER(Device);
    g_Stats.GetReportDescCalls++;
    NTSTATUS status;
    void*    out;
    size_t   outLen;

    status = WdfRequestRetrieveOutputBuffer(Request, HIDTOUCH_REPORT_DESCRIPTOR_SIZE,
                                            &out, &outLen);
    if (!NT_SUCCESS(status)) return status;

    if (outLen < HIDTOUCH_REPORT_DESCRIPTOR_SIZE) return STATUS_BUFFER_TOO_SMALL;

    RtlCopyMemory(out, g_HidTouchReportDescriptor, HIDTOUCH_REPORT_DESCRIPTOR_SIZE);
    WdfRequestSetInformation(Request, HIDTOUCH_REPORT_DESCRIPTOR_SIZE);
    return STATUS_SUCCESS;
}

NTSTATUS
HidTouchGetDeviceAttributes(_In_ WDFREQUEST Request)
{
    g_Stats.GetAttribCalls++;
    PHID_DEVICE_ATTRIBUTES attrs;
    NTSTATUS               status;

    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*attrs),
                                            (PVOID*)&attrs, NULL);
    if (!NT_SUCCESS(status)) return status;

    attrs->Size           = sizeof(HID_DEVICE_ATTRIBUTES);
    attrs->VendorID       = 0xFEED;
    attrs->ProductID      = 0x0001;
    attrs->VersionNumber  = 0x0100;

    WdfRequestSetInformation(Request, sizeof(*attrs));
    return STATUS_SUCCESS;
}

NTSTATUS
HidTouchGetString(_In_ WDFDEVICE Device, _In_ WDFREQUEST Request)
{
    UNREFERENCED_PARAMETER(Device);

    WDF_REQUEST_PARAMETERS params;
    WDF_REQUEST_PARAMETERS_INIT(&params);
    WdfRequestGetParameters(Request, &params);

    ULONG_PTR which = (ULONG_PTR)params.Parameters.DeviceIoControl.Type3InputBuffer
                      & 0xFFFF;

    static const WCHAR mfg[]  = L"TouchDirve Project";
    static const WCHAR prod[] = L"Virtual Multi-Touch Screen";
    static const WCHAR ser[]  = L"TD0001";

    const WCHAR* src = NULL;
    size_t       cb  = 0;

    switch (which) {
    case HID_STRING_ID_IMANUFACTURER: src = mfg;  cb = sizeof(mfg);  break;
    case HID_STRING_ID_IPRODUCT:      src = prod; cb = sizeof(prod); break;
    case HID_STRING_ID_ISERIALNUMBER: src = ser;  cb = sizeof(ser);  break;
    default: return STATUS_INVALID_PARAMETER;
    }

    void*    out;
    size_t   outLen;
    NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, cb, &out, &outLen);
    if (!NT_SUCCESS(status)) return status;
    if (outLen < cb) return STATUS_BUFFER_TOO_SMALL;

    RtlCopyMemory(out, src, cb);
    WdfRequestSetInformation(Request, cb);
    return STATUS_SUCCESS;
}

NTSTATUS
HidTouchReadReport(_In_ PDEVICE_CONTEXT Ctx, _In_ WDFREQUEST Request)
{
    UNREFERENCED_PARAMETER(Ctx);
    g_Stats.ReadRequests++;
    if (g_ReadReportQueue == NULL) return STATUS_DEVICE_NOT_READY;
    NTSTATUS status = WdfRequestForwardToIoQueue(Request, g_ReadReportQueue);
    if (!NT_SUCCESS(status)) {
        g_Stats.ReadForwardFail++;
        g_Stats.LastReadFwdStatus = (ULONG)status;
        DbgPrint("[hidtouch] ForwardToIoQueue FAILED status=0x%08X\n", status);
        return status;
    }
    g_Stats.ReadForwardOk++;
    return STATUS_PENDING;
}

// Tracked Device Mode feature; updated on SET_FEATURE so Win7 can put us
// into multi-touch mode (typically writes 2 = MULTI_INPUT_DESIRED).
static UCHAR g_DeviceMode       = 0x02;  // start in multi-touch mode
static UCHAR g_DeviceIdentifier = 0x00;

NTSTATUS
HidTouchGetFeature(_In_ WDFREQUEST Request)
{
    g_Stats.GetFeatureCalls++;

    PHID_XFER_PACKET pkt = (PHID_XFER_PACKET)WdfRequestWdmGetIrp(Request)->UserBuffer;
    if (!pkt || !pkt->reportBuffer || pkt->reportBufferLen < 1)
        return STATUS_INVALID_BUFFER_SIZE;

    UCHAR id = pkt->reportId ? pkt->reportId : pkt->reportBuffer[0];

    if (id == HIDTOUCH_REPORT_ID_MAXCOUNT) {
        if (pkt->reportBufferLen < sizeof(HIDTOUCH_MAXCOUNT_REPORT))
            return STATUS_INVALID_BUFFER_SIZE;
        HIDTOUCH_MAXCOUNT_REPORT* rep = (HIDTOUCH_MAXCOUNT_REPORT*)pkt->reportBuffer;
        rep->ReportId = HIDTOUCH_REPORT_ID_MAXCOUNT;
        rep->MaxCount = HIDTOUCH_MAX_CONTACTS;
        WdfRequestSetInformation(Request, sizeof(*rep));
        return STATUS_SUCCESS;
    }
    if (id == HIDTOUCH_REPORT_ID_CONFIG) {
        if (pkt->reportBufferLen < sizeof(HIDTOUCH_CONFIG_REPORT))
            return STATUS_INVALID_BUFFER_SIZE;
        HIDTOUCH_CONFIG_REPORT* rep = (HIDTOUCH_CONFIG_REPORT*)pkt->reportBuffer;
        rep->ReportId        = HIDTOUCH_REPORT_ID_CONFIG;
        rep->DeviceMode      = g_DeviceMode;
        rep->DeviceIdentifier= g_DeviceIdentifier;
        WdfRequestSetInformation(Request, sizeof(*rep));
        return STATUS_SUCCESS;
    }
    return STATUS_INVALID_PARAMETER;
}

NTSTATUS
HidTouchSetFeature(_In_ WDFREQUEST Request)
{
    g_Stats.SetFeatureCalls++;

    PHID_XFER_PACKET pkt = (PHID_XFER_PACKET)WdfRequestWdmGetIrp(Request)->UserBuffer;
    if (!pkt || !pkt->reportBuffer || pkt->reportBufferLen < 1)
        return STATUS_INVALID_BUFFER_SIZE;

    UCHAR id = pkt->reportId ? pkt->reportId : pkt->reportBuffer[0];
    if (id == HIDTOUCH_REPORT_ID_CONFIG) {
        if (pkt->reportBufferLen < sizeof(HIDTOUCH_CONFIG_REPORT))
            return STATUS_INVALID_BUFFER_SIZE;
        HIDTOUCH_CONFIG_REPORT* rep = (HIDTOUCH_CONFIG_REPORT*)pkt->reportBuffer;
        g_DeviceMode       = rep->DeviceMode;
        g_DeviceIdentifier = rep->DeviceIdentifier;
        g_Stats.LastDeviceMode = g_DeviceMode;
        DbgPrint("[hidtouch] SET_FEATURE: DeviceMode=%u\n", g_DeviceMode);
        return STATUS_SUCCESS;
    }
    return STATUS_INVALID_PARAMETER;
}
