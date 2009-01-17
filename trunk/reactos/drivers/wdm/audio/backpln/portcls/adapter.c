/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS Kernel Streaming
 * FILE:            drivers/multimedia/portcls/adapter.c
 * PURPOSE:         Port Class driver / DriverEntry and IRP handlers
 * PROGRAMMER:      Andrew Greenwood
 *
 * HISTORY:
 *                  27 Jan 07   Created
 */

#include "private.h"
#include <devguid.h>
#include <initguid.h>

/*
    This is called from DriverEntry so that PortCls can take care of some
    IRPs and map some others to the main KS driver. In most cases this will
    be the first function called by an audio driver.

    First 2 parameters are from DriverEntry.

    The AddDevice parameter is a driver-supplied pointer to a function which
    typically then calls PcAddAdapterDevice (see below.)
*/
NTSTATUS NTAPI
PcInitializeAdapterDriver(
    IN  PDRIVER_OBJECT DriverObject,
    IN  PUNICODE_STRING RegistryPathName,
    IN  PDRIVER_ADD_DEVICE AddDevice)
{
    /*
        (* = implement here, otherwise KS default)
        IRP_MJ_CLOSE
        * IRP_MJ_CREATE
        IRP_MJ_DEVICE_CONTROL
        IRP_MJ_FLUSH_BUFFERS
        * IRP_MJ_PNP
        * IRP_MJ_POWER
        IRP_MJ_QUERY_SECURITY
        IRP_MJ_READ
        IRP_MJ_SET_SECURITY
        * IRP_MJ_SYSTEM_CONTROL
        IRP_MJ_WRITE
    */

    //NTSTATUS status;
    //ULONG i;

    DPRINT1("PcInitializeAdapterDriver\n");

#if 0
    /* Set default stub - is this a good idea? */
    DPRINT1("Setting IRP stub\n");
    for ( i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i ++ )
    {
        DriverObject->MajorFunction[i] = IrpStub;
    }
#endif

    /* Our IRP handlers */
    DPRINT1("Setting IRP handlers\n");
    DriverObject->MajorFunction[IRP_MJ_CREATE] = PcDispatchIrp;
    DriverObject->MajorFunction[IRP_MJ_PNP] = PcDispatchIrp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = PcDispatchIrp;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = PcDispatchIrp;

    /* The driver-supplied AddDevice */
    DriverObject->DriverExtension->AddDevice = AddDevice;

    /* KS handles these */
    DPRINT1("Setting KS function handlers\n");
    KsSetMajorFunctionHandler(DriverObject, IRP_MJ_CLOSE);
    KsSetMajorFunctionHandler(DriverObject, IRP_MJ_DEVICE_CONTROL);
    KsSetMajorFunctionHandler(DriverObject, IRP_MJ_FLUSH_BUFFERS);
    KsSetMajorFunctionHandler(DriverObject, IRP_MJ_QUERY_SECURITY);
    KsSetMajorFunctionHandler(DriverObject, IRP_MJ_READ);
    KsSetMajorFunctionHandler(DriverObject, IRP_MJ_SET_SECURITY);
    KsSetMajorFunctionHandler(DriverObject, IRP_MJ_WRITE);

    DPRINT1("PortCls has finished initializing the adapter driver\n");

    return STATUS_SUCCESS;
}

/*
    Typically called by a driver's AddDevice function, which is set when
    calling PcInitializeAdapterDriver. This performs some common driver
    operations, such as creating a device extension.

    The StartDevice parameter is a driver-supplied function which gets
    called in response to IRP_MJ_PNP / IRP_MN_START_DEVICE.
*/
NTSTATUS NTAPI
PcAddAdapterDevice(
    IN  PDRIVER_OBJECT DriverObject,
    IN  PDEVICE_OBJECT PhysicalDeviceObject,
    IN  PCPFNSTARTDEVICE StartDevice,
    IN  ULONG MaxObjects,
    IN  ULONG DeviceExtensionSize)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PDEVICE_OBJECT fdo = NULL;
    PDEVICE_OBJECT PrevDeviceObject;
    PCExtension* portcls_ext;

    DPRINT1("PcAddAdapterDevice called\n");

    if (!DriverObject || !PhysicalDeviceObject || !StartDevice)
    {
        return STATUS_INVALID_PARAMETER;
    }

    /* check if the DeviceExtensionSize is provided */
    if ( DeviceExtensionSize < PORT_CLASS_DEVICE_EXTENSION_SIZE )
    {
        /* driver does not need a device extension */
        if ( DeviceExtensionSize != 0 )
        {
            /* DeviceExtensionSize must be zero*/
            return STATUS_INVALID_PARAMETER;
        }
        /* set size to our extension size */
        DeviceExtensionSize = PORT_CLASS_DEVICE_EXTENSION_SIZE;
    }

    /* create the device */
    status = IoCreateDevice(DriverObject,
                            DeviceExtensionSize,
                            NULL,
                            FILE_DEVICE_KS,
                            FILE_AUTOGENERATED_DEVICE_NAME | FILE_DEVICE_SECURE_OPEN,
                            FALSE,
                            &fdo);

    if (!NT_SUCCESS(status))
    {
        DPRINT("IoCreateDevice() failed with status 0x%08lx\n", status);
        return status;
    }

    /* Obtain the new device extension */
    portcls_ext = (PCExtension*) fdo->DeviceExtension;
    /* initialize the device extension */
    RtlZeroMemory(portcls_ext, DeviceExtensionSize);
    /* allocate create item */
    portcls_ext->CreateItems = AllocateItem(NonPagedPool, MaxObjects * sizeof(KSOBJECT_CREATE_ITEM), TAG_PORTCLASS);

    /* store the physical device object */
    portcls_ext->PhysicalDeviceObject = PhysicalDeviceObject;
    /* set up the start device function */
    portcls_ext->StartDevice = StartDevice;
    /* prepare the subdevice list */
    InitializeListHead(&portcls_ext->SubDeviceList);
    /* prepare the physical connection list */
    InitializeListHead(&portcls_ext->PhysicalConnectionList);

    /* set io flags */
    fdo->Flags |= DO_DIRECT_IO | DO_POWER_PAGABLE;
    /* clear initializing flag */
    fdo->Flags &= ~ DO_DEVICE_INITIALIZING;

    /* allocate the device header */
    status = KsAllocateDeviceHeader(&portcls_ext->KsDeviceHeader, MaxObjects, portcls_ext->CreateItems);
    /* did we succeed */
    if (!NT_SUCCESS(status))
    {
        /* free previously allocated create items */
        FreeItem(portcls_ext->CreateItems, TAG_PORTCLASS);
        /* delete created fdo */
        IoDeleteDevice(fdo);
        /* return error code */
        return status;
    }

    /* attach device to device stack */
    PrevDeviceObject = IoAttachDeviceToDeviceStack(fdo, PhysicalDeviceObject);
    /* did we succeed */
    if (PrevDeviceObject)
    {
        /* store the device object in the device header */
        //KsSetDevicePnpBaseObject(portcls_ext->KsDeviceHeader, PrevDeviceObject, fdo);
        portcls_ext->PrevDeviceObject = PrevDeviceObject;
    }
    else
    {
        /* free the device header */
        KsFreeDeviceHeader(portcls_ext->KsDeviceHeader);
        /* free previously allocated create items */
        FreeItem(portcls_ext->CreateItems, TAG_PORTCLASS);
        /* delete created fdo */
        IoDeleteDevice(fdo);
        /* return error code */
        return STATUS_UNSUCCESSFUL;
    }



    return status;
}

NTSTATUS
NTAPI
PciDriverDispatch(
    IN  PDEVICE_OBJECT DeviceObject,
    IN  PIRP Irp)
{
    NTSTATUS Status;

    ISubdevice * SubDevice;
    PCExtension* DeviceExt;
    SUBDEVICE_ENTRY * Entry;
    KSDISPATCH_TABLE DispatchTable;

    DPRINT1("PortClsSysControl called\n");

    SubDevice = (ISubdevice*)Irp->Tail.Overlay.DriverContext[3];
    DeviceExt = (PCExtension*)DeviceObject->DeviceExtension;

    if (!SubDevice || !DeviceExt)
    {
        return STATUS_UNSUCCESSFUL;
    }

    Entry = AllocateItem(NonPagedPool, sizeof(SUBDEVICE_ENTRY), TAG_PORTCLASS);
    if (!Entry)
        return STATUS_INSUFFICIENT_RESOURCES;

    /* initialize DispatchTable */
    RtlZeroMemory(&DispatchTable, sizeof(KSDISPATCH_TABLE));
    /* FIXME
     * initialize DispatchTable pointer
     * which call in turn ISubDevice
     */


    Status = KsAllocateObjectHeader(&Entry->ObjectHeader, 1, NULL, Irp, &DispatchTable);
    if (!NT_SUCCESS(Status))
    {
        FreeItem(Entry, TAG_PORTCLASS);
        return Status;
    }


    InsertTailList(&DeviceExt->SubDeviceList, &Entry->Entry);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS NTAPI
PcRegisterSubdevice(
    IN  PDEVICE_OBJECT DeviceObject,
    IN  PWCHAR Name,
    IN  PUNKNOWN Unknown)
{
    PCExtension* DeviceExt;
    NTSTATUS Status;
    ISubdevice *SubDevice;
    UNICODE_STRING ReferenceString;
    UNICODE_STRING SymbolicLinkName;

    DPRINT1("PcRegisterSubdevice DeviceObject %p Name %S Unknown %p\n", DeviceObject, Name, Unknown);

    if (!DeviceObject || !Name || !Unknown)
    {
        DPRINT("PcRegisterSubdevice invalid parameter\n");
        return STATUS_INVALID_PARAMETER;
    }

    DeviceExt = (PCExtension*)DeviceObject->DeviceExtension;
    if (!DeviceExt)
        return STATUS_UNSUCCESSFUL;

    Status = Unknown->lpVtbl->QueryInterface(Unknown, &IID_ISubdevice, (LPVOID)&SubDevice);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("No ISubdevice interface\n");
        /* the provided port driver doesnt support ISubdevice */
        return STATUS_INVALID_PARAMETER;
    }
#if KS_IMPLEMENTED
    Status = KsAddObjectCreateItemToDeviceHeader(DeviceExt->KsDeviceHeader, PciDriverDispatch, (PVOID)SubDevice, Name, NULL);
    if (!NT_SUCCESS(Status))
    {
        /* failed to attach */
        SubDevice->lpVtbl->Release(SubDevice);
        DPRINT1("KsAddObjectCreateItemToDeviceHeader failed with %x\n", Status);
        return Status;
    }
#endif

    /* FIXME retrieve guid from subdescriptor */

    RtlInitUnicodeString(&ReferenceString, Name);
    /* register device interface */
    Status = IoRegisterDeviceInterface(DeviceExt->PhysicalDeviceObject, 
                                       &GUID_DEVCLASS_SOUND, //FIXME
                                       &ReferenceString, 
                                       &SymbolicLinkName);
    if (NT_SUCCESS(Status))
    {
        Status = IoSetDeviceInterfaceState(&SymbolicLinkName, TRUE);
        RtlFreeUnicodeString(&SymbolicLinkName);
    }

    DPRINT1("PcRegisterSubdevice Status %x\n", Status);

    /// HACK
    /// IoRegisterDeviceInterface fails with
    /// STATUS_OBJECT_PATH_NOT_FOUND
    /// return Status;
    return STATUS_SUCCESS;
}
