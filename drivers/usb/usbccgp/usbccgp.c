/*
 * PROJECT:     ReactOS Universal Serial Bus Bulk Enhanced Host Controller Interface
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        drivers/usb/usbccgp/usbccgp.c
 * PURPOSE:     USB  device driver.
 * PROGRAMMERS:
 *              Michael Martin (michael.martin@reactos.org)
 *              Johannes Anderwald (johannes.anderwald@reactos.org)
 *              Cameron Gutman
 */

#include "usbccgp.h"

//
// driver verifier
//
DRIVER_ADD_DEVICE USBCCGP_AddDevice;

NTSTATUS
NTAPI
USBCCGP_AddDevice(
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT PhysicalDeviceObject)
{
    NTSTATUS Status;
    PDEVICE_OBJECT DeviceObject;
    PFDO_DEVICE_EXTENSION FDODeviceExtension;

    // lets create the device 
    Status = IoCreateDevice(DriverObject, sizeof(FDO_DEVICE_EXTENSION), NULL, FILE_DEVICE_USB, FILE_AUTOGENERATED_DEVICE_NAME, FALSE, &DeviceObject);
    if (!NT_SUCCESS(Status))
    {
        // failed to create device 
        DPRINT1("USBCCGP_AddDevice failed to create device with %x\n", Status);
        return Status;
    }

    // get device extension 
    FDODeviceExtension = (PFDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    // init device extension 
    RtlZeroMemory(FDODeviceExtension, sizeof(FDO_DEVICE_EXTENSION));
    FDODeviceExtension->Common.IsFDO = TRUE;
    FDODeviceExtension->DriverObject = DriverObject;
    FDODeviceExtension->PhysicalDeviceObject = PhysicalDeviceObject;
    InitializeListHead(&FDODeviceExtension->ResetPortListHead);
    InitializeListHead(&FDODeviceExtension->CyclePortListHead);
    KeInitializeSpinLock(&FDODeviceExtension->Lock);

    FDODeviceExtension->NextDeviceObject = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);
    if (!FDODeviceExtension->NextDeviceObject)
    {
        // failed to attach 
        DPRINT1("USBCCGP_AddDevice failed to attach device\n");
        IoDeleteDevice(DeviceObject);
        return STATUS_DEVICE_REMOVED;
    }

    // set device flags 
    DeviceObject->Flags |= DO_BUFFERED_IO | DO_POWER_PAGABLE;

    // device is initialized 
    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    // device initialized 
    return Status;
}

NTSTATUS
NTAPI
USBCCGP_CreateClose(
    PDEVICE_OBJECT DeviceObject, 
    PIRP Irp)
{
    PCOMMON_DEVICE_EXTENSION DeviceExtension;
    PFDO_DEVICE_EXTENSION FDODeviceExtension;

    // get common device extension 
    DeviceExtension = (PCOMMON_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    // is it a fdo 
    if (DeviceExtension->IsFDO)
    {
        // forward and forget 
        IoSkipCurrentIrpStackLocation(Irp);

        // get fdo 
        FDODeviceExtension = (PFDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

        // call lower driver 
        return IoCallDriver(FDODeviceExtension->NextDeviceObject, Irp);
    }
    else
    {
        // pdo not supported 
        Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_NOT_SUPPORTED;
    }
}

NTSTATUS
NTAPI
USBCCGP_Dispatch(
    PDEVICE_OBJECT DeviceObject, 
    PIRP Irp)
{
    PCOMMON_DEVICE_EXTENSION DeviceExtension;
    PIO_STACK_LOCATION IoStack;

    // get common device extension 
    DeviceExtension = (PCOMMON_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    // get current stack location 
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    if (IoStack->MajorFunction == IRP_MJ_CREATE || IoStack->MajorFunction == IRP_MJ_CLOSE)
    {
        // dispatch to default handler 
        return USBCCGP_CreateClose(DeviceObject, Irp);
    }

    if (DeviceExtension->IsFDO)
    {
        // handle request for FDO 
        return FDO_Dispatch(DeviceObject, Irp);
    }
    else
    {
        // handle request for PDO 
        return PDO_Dispatch(DeviceObject, Irp);
    }
}

NTSTATUS
NTAPI
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath)
{

    // initialize driver object
    DPRINT("[USBCCGP] DriverEntry\n");
    DriverObject->DriverExtension->AddDevice = USBCCGP_AddDevice;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = USBCCGP_Dispatch;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = USBCCGP_Dispatch;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = USBCCGP_Dispatch;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = USBCCGP_Dispatch;
    DriverObject->MajorFunction[IRP_MJ_POWER] = USBCCGP_Dispatch;
    DriverObject->MajorFunction[IRP_MJ_PNP] = USBCCGP_Dispatch;

    // FIMXE query GenericCompositeUSBDeviceString 

    return STATUS_SUCCESS;
}
