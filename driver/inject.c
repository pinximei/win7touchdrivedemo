// inject.c — Convert app-supplied frame into a HID input report and complete
//            the next pending HID READ_REPORT.
#include "driver.h"
#include "descriptor.h"

VOID
HidTouchEvtReadCanceled(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request)
{
    UNREFERENCED_PARAMETER(Queue);
    g_Stats.ReadsCanceled++;
    WdfRequestComplete(Request, STATUS_CANCELLED);
}


static VOID
BuildHidReport(
    _In_ const HIDTOUCH_INJECT_FRAME* Frame,
    _Out_ HIDTOUCH_INPUT_REPORT*      Report
    )
{
    RtlZeroMemory(Report, sizeof(*Report));
    Report->ReportId = HIDTOUCH_REPORT_ID_INPUT;

    UCHAR n = Frame->ContactCount;
    if (n > HIDTOUCH_MAX_CONTACTS) n = HIDTOUCH_MAX_CONTACTS;

    for (UCHAR i = 0; i < n; ++i) {
        const HIDTOUCH_CONTACT* in = &Frame->Contacts[i];
        HIDTOUCH_CONTACT_REPORT* out = &Report->Contacts[i];

        UCHAR status = 0;
        if (in->TipSwitch) status |= 0x01;
        if (in->InRange)   status |= 0x02;
        status |= 0x04; // Confidence: always 1

        out->Status    = status;
        out->ContactId = in->ContactId;
        out->X         = in->X;
        out->Y         = in->Y;
    }

    Report->ContactCount = n;
}

// Legacy entry: forwards to the global path so old callers still work.
NTSTATUS
HidTouchInject(_In_ PDEVICE_CONTEXT Ctx, _In_ WDFREQUEST Request)
{
    UNREFERENCED_PARAMETER(Ctx);
    return HidTouchInjectGlobal(Request);
}

// Variant called from the singleton control device IOCTL path. Doesn't
// dereference any per-FDO context — uses the globals set up by EvtDeviceAdd.
NTSTATUS
HidTouchInjectGlobal(_In_ WDFREQUEST Request)
{
    HIDTOUCH_INJECT_FRAME* frame;
    size_t                 inLen;
    NTSTATUS               status;

    if (g_ReadReportQueue == NULL) return STATUS_DEVICE_NOT_READY;

    status = WdfRequestRetrieveInputBuffer(Request, sizeof(*frame),
                                           (PVOID*)&frame, &inLen);
    if (!NT_SUCCESS(status)) return status;
    if (inLen < sizeof(*frame)) return STATUS_INVALID_BUFFER_SIZE;

    g_LastFrame = *frame;
    g_Stats.InjectCalls++;

    WDFREQUEST readRequest;
    status = WdfIoQueueRetrieveNextRequest(g_ReadReportQueue, &readRequest);
    if (!NT_SUCCESS(status)) {
        g_Stats.InjectQueueEmpty++;
        return STATUS_SUCCESS;
    }
    g_Stats.InjectQueueGot++;
    g_Stats.ReadCompletions++;

    HIDTOUCH_INPUT_REPORT* out;
    size_t                 outLen;
    status = WdfRequestRetrieveOutputBuffer(readRequest, sizeof(*out),
                                            (PVOID*)&out, &outLen);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(readRequest, status);
        return STATUS_SUCCESS;
    }
    if (outLen < sizeof(*out)) {
        WdfRequestComplete(readRequest, STATUS_BUFFER_TOO_SMALL);
        return STATUS_SUCCESS;
    }

    BuildHidReport(frame, out);
    WdfRequestSetInformation(readRequest, sizeof(*out));
    WdfRequestComplete(readRequest, STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
