// driver.c — KMDF entry, device add, queues
#include "driver.h"
#include "descriptor.h"

WDFDEVICE             g_ControlDevice    = NULL;
WDFQUEUE              g_ReadReportQueue  = NULL;
HIDTOUCH_INJECT_FRAME g_LastFrame        = { 0 };
HIDTOUCH_STATS        g_Stats            = { 0 };

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS          status;
    WDFDRIVER         driver;

    WDF_DRIVER_CONFIG_INIT(&config, HidTouchEvtDeviceAdd);
    config.DriverPoolTag = 'TdiH';

    g_Stats.ReportSize = (ULONG)sizeof(HIDTOUCH_INPUT_REPORT);

    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES,
                             &config, &driver);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[hidtouch] WdfDriverCreate failed 0x%X\n", status);
        return status;
    }

    // Singleton control device + manual queue must exist before any FDO can
    // forward HID Reads into it.
    status = HidTouchCreateControlDevice(driver);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[hidtouch] CreateControlDevice failed 0x%X\n", status);
        return status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
HidTouchEvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    UNREFERENCED_PARAMETER(Driver);

    NTSTATUS               status;
    WDFDEVICE              device;
    WDF_OBJECT_ATTRIBUTES  attribs;
    PDEVICE_CONTEXT        ctx;
    WDF_IO_QUEUE_CONFIG    queueConfig;
    int                    stage = 0;

    DbgPrint("[hidtouch] EvtDeviceAdd: enter\n");

    // mshidkmdf is registered as an upper filter (via the INF UpperFilters reg
    // value). It bridges HIDClass to our KMDF stack via IRP_MJ_INTERNAL_DEVICE_CONTROL.
    // We are still the FDO of this root-enumerated device — that's what lets us
    // expose a private device interface that user-mode can CreateFile().

    // (User-mode access to inject reports comes through a separate control
    //  device, not this FDO. mshidkmdf/HIDClass own IRP_MJ_CREATE on this stack.)

    // We sit beneath mshidkmdf (registered as UpperFilter in the INF). HIDClass
    // above mshidkmdf is the power policy owner of the HID stack. We must not
    // be FDO of an FDO stack — a HID minidriver in this layout is treated as a
    // filter so KMDF auto-forwards Power/PnP IRPs to the lower stack. Without
    // this, KMDF bugchecks 0x10D arg1=0x0D (PowerIrp dispatched to wrong
    // device) the first time HIDClass sends SET_POWER down.
    WdfFdoInitSetFilter(DeviceInit);

    // Explicitly disclaim PPO ownership. WdfFdoInitSetFilter() *should* imply
    // this in KMDF 1.11+, but on KMDF 1.9 + Win7 x64 the FDO still self-claims
    // PPO and races with hidclass.sys, hitting WDF_VIOLATION 0x10D arg1=0x0D
    // (WDF_POWER_MULTIPLE_PPO) on first SET_POWER. Belt and suspenders.
    WdfDeviceInitSetPowerPolicyOwnership(DeviceInit, FALSE);

    // Root-enumerated devices answer IRP_MN_QUERY_ID with NULLs by default.
    // HIDClass forwards QUERY_ID to its parent (us) to learn the HID child
    // hardware-ID; we must override that.
    {
        UCHAR minor = IRP_MN_QUERY_ID;
        status = WdfDeviceInitAssignWdmIrpPreprocessCallback(
            DeviceInit, HidTouchEvtWdmPreprocessQueryId,
            IRP_MJ_PNP, &minor, 1);
        if (!NT_SUCCESS(status)) goto fail;
    }

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attribs, DEVICE_CONTEXT);
    attribs.EvtCleanupCallback = HidTouchEvtDeviceCleanup;
    status = WdfDeviceCreate(&DeviceInit, &attribs, &device);
    if (!NT_SUCCESS(status)) goto fail;

    stage = 2;
    ctx = GetDeviceContext(device);
    ctx->Device = device;
    RtlZeroMemory(&ctx->LastFrame, sizeof(ctx->LastFrame));

    // Default queue: HIDClass forwards its requests as INTERNAL_DEVICE_CONTROL.
    stage = 3;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoInternalDeviceControl = HidTouchEvtIoInternalDeviceControl;
    queueConfig.EvtIoDeviceControl         = HidTouchEvtIoDeviceControl;

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, NULL);
    if (!NT_SUCCESS(status)) goto fail;

    // Manual queue on the FDO holds pending HID READ_REPORT IRPs. Must live on
    // the FDO (not the control device) — KMDF requires Forward-To-Queue to stay
    // within the same device, otherwise FDO power IRPs bugcheck 0x10D arg 0xD
    // ("power policy owner received unsolicited power IRP") because FDO state
    // machine can't account for IRPs that were forwarded out to another device.
    //
    // PowerManaged=False so the queue keeps holding parked READs across S0/Sx
    // transitions instead of KMDF auto-completing them.
    //
    // EvtIoCanceledOnQueue: when hidclass cancels a parked read, we must
    // complete it; otherwise hidclass stops issuing new reads.
    stage = 4;
    {
        WDF_IO_QUEUE_CONFIG mq;
        WDF_IO_QUEUE_CONFIG_INIT(&mq, WdfIoQueueDispatchManual);
        mq.PowerManaged       = WdfFalse;
        mq.EvtIoCanceledOnQueue = HidTouchEvtReadCanceled;
        status = WdfIoQueueCreate(device, &mq, WDF_NO_OBJECT_ATTRIBUTES,
                                  &g_ReadReportQueue);
        if (!NT_SUCCESS(status)) goto fail;
    }

    // No device interface: user mode reaches us through the control device
    // symlink \\.\HidTouchInject, not by GUID_DEVINTERFACE enumeration.

    DbgPrint("[hidtouch] EvtDeviceAdd: success\n");
    return STATUS_SUCCESS;

fail:
    DbgPrint("[hidtouch] EvtDeviceAdd: failed at stage %d, status 0x%08X\n",
             stage, status);
    return status;
}

// Called when an FDO is being destroyed. Nothing to clean up — the manual
// queue lives on the control device, which outlives all FDOs.
VOID HidTouchEvtDeviceCleanup(_In_ WDFOBJECT Device)
{
    UNREFERENCED_PARAMETER(Device);
}

// ----------------------------------------------------------------------
// Control device — singleton "\Device\HidTouchInject" with a symlink
// "\DosDevices\HidTouchInject" so user mode can do
//     CreateFile("\\\\.\\HidTouchInject", ...).
// ----------------------------------------------------------------------

static VOID HidTouchControlEvtIoCtl(WDFQUEUE Queue,
                                    WDFREQUEST Request,
                                    size_t OutputBufferLength,
                                    size_t InputBufferLength,
                                    ULONG IoControlCode)
{
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    NTSTATUS status;
    if (IoControlCode == IOCTL_HIDTOUCH_INJECT && g_ReadReportQueue != NULL) {
        status = HidTouchInjectGlobal(Request);
    } else if (IoControlCode == IOCTL_HIDTOUCH_GET_STATS) {
        void*  out;
        size_t outLen;
        status = WdfRequestRetrieveOutputBuffer(Request, sizeof(g_Stats), &out, &outLen);
        if (NT_SUCCESS(status)) {
            RtlCopyMemory(out, &g_Stats, sizeof(g_Stats));
            WdfRequestSetInformation(Request, sizeof(g_Stats));
        }
    } else {
        status = STATUS_NOT_SUPPORTED;
    }
    WdfRequestComplete(Request, status);
}

NTSTATUS HidTouchCreateControlDevice(WDFDRIVER Driver)
{
    PWDFDEVICE_INIT init;
    NTSTATUS        status;

    DECLARE_CONST_UNICODE_STRING(ntName,  L"\\Device\\HidTouchInject");
    DECLARE_CONST_UNICODE_STRING(dosName, L"\\DosDevices\\HidTouchInject");
    DECLARE_CONST_UNICODE_STRING(sddl,    L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;WD)");

    init = WdfControlDeviceInitAllocate(Driver, &sddl);
    if (!init) return STATUS_INSUFFICIENT_RESOURCES;

    status = WdfDeviceInitAssignName(init, &ntName);
    if (!NT_SUCCESS(status)) { WdfDeviceInitFree(init); return status; }

    WdfDeviceInitSetExclusive(init, FALSE);

    WDFDEVICE dev;
    status = WdfDeviceCreate(&init, WDF_NO_OBJECT_ATTRIBUTES, &dev);
    if (!NT_SUCCESS(status)) { if (init) WdfDeviceInitFree(init); return status; }

    status = WdfDeviceCreateSymbolicLink(dev, &dosName);
    if (!NT_SUCCESS(status)) return status;

    // IOCTL queue (parallel) for IOCTL_HIDTOUCH_INJECT / GET_STATS.
    WDF_IO_QUEUE_CONFIG q;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q, WdfIoQueueDispatchParallel);
    q.EvtIoDeviceControl = HidTouchControlEvtIoCtl;
    status = WdfIoQueueCreate(dev, &q, WDF_NO_OBJECT_ATTRIBUTES, NULL);
    if (!NT_SUCCESS(status)) return status;

    // Manual queue for parked HID READs lives on the FDO (see EvtDeviceAdd),
    // not on this control device, to comply with KMDF cross-device IRP
    // forwarding rules.

    WdfControlFinishInitializing(dev);
    g_ControlDevice = dev;
    return STATUS_SUCCESS;
}

// Hardware ID for our root-enumerated child PDO that mshidkmdf attaches to.
// vmulti uses "djpnewton\VMulti"; we use a similar simple two-component ID.
// hidclass.sys later generates the real HID\Vid_xxxx&Pid_yyyy node based on
// our HID descriptor. Multi-sz, double-NUL terminated.
static const WCHAR g_HidTouchHwIds[] = L"TouchDirve\\HidTouch\0\0";
#define HIDTOUCH_HWIDS_BYTES sizeof(g_HidTouchHwIds)

NTSTATUS
HidTouchEvtWdmPreprocessQueryId(_In_ WDFDEVICE Device, _Inout_ PIRP Irp)
{
    PIO_STACK_LOCATION sp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS           status;

    // We only care about queries for our (the parent FDO's) own DeviceID and
    // HardwareIDs, which the bus driver below us would otherwise leave NULL.
    // Don't attempt to inspect the previous IO_STACK_LOCATION — KMDF has
    // already moved through the stack and reading prev is unsafe.
    //
    // The ContainerID query is sent down to FDOs and we should ignore it,
    // letting the lower stack handle it. Same for any other type.
    if (sp->Parameters.QueryId.IdType == BusQueryDeviceID ||
        sp->Parameters.QueryId.IdType == BusQueryHardwareIDs)
    {
        // Only override if the lower stack has not already filled this in
        // (information=0 means uninitialized). The PnP manager forwards the
        // same IRP down our stack; we want to fill it once at the FDO level.
        if (Irp->IoStatus.Information == 0) {
            PWCHAR buf = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool,
                                                       HIDTOUCH_HWIDS_BYTES, 'TdiH');
            if (buf) {
                RtlCopyMemory(buf, g_HidTouchHwIds, HIDTOUCH_HWIDS_BYTES);
                Irp->IoStatus.Information = (ULONG_PTR)buf;
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_INSUFFICIENT_RESOURCES;
            }
            Irp->IoStatus.Status = status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return status;
        }
    }

    // Other QUERY_ID types or already-filled buffer: pass back to KMDF.
    IoSkipCurrentIrpStackLocation(Irp);
    return WdfDeviceWdmDispatchPreprocessedIrp(Device, Irp);
}
