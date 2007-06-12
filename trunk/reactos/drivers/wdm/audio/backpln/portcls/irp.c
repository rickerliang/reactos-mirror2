/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS
 * FILE:            drivers/multimedia/portcls/irp.c
 * PURPOSE:         Port Class driver / IRP Handling
 * PROGRAMMER:      Andrew Greenwood
 *
 * HISTORY:
 *                  27 Jan 07   Created
 */


#include "private.h"
#include <portcls.h>

/*
    A safe place for IRPs to be bounced to, if no handler has been
    set. Whether this is a good idea or not...?
*/
#if 0
static NTAPI
NTSTATUS
IrpStub(
    IN  PDEVICE_OBJECT DeviceObject,
    IN  PIRP Irp)
{
    NTSTATUS status = STATUS_NOT_SUPPORTED;

    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    DPRINT1("IRP Stub called\n");

    return status;
}
#endif

/*
    Handles IRP_MJ_CREATE, which occurs when someone wants to make use of
    a device.
*/
NTAPI
NTSTATUS
PortClsCreate(
    IN  PDEVICE_OBJECT DeviceObject,
    IN  PIRP Irp)
{
    DPRINT1("PortClsCreate called\n");

    /* TODO */

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}


/*
    IRP_MJ_PNP handler
    Used for things like IRP_MN_START_DEVICE
*/
NTAPI
NTSTATUS
PortClsPnp(
    IN  PDEVICE_OBJECT DeviceObject,
    IN  PIRP Irp)
{
    NTSTATUS status;
    PCExtension* portcls_ext;
    PIO_STACK_LOCATION irp_stack;

    DPRINT1("PortClsPnp called\n");

    portcls_ext = (PCExtension*) DeviceObject->DeviceExtension;
    irp_stack = IoGetCurrentIrpStackLocation(Irp);

    ASSERT(portcls_ext);

    /*
        if IRP_MN_START_DEVICE, call the driver's customer start device routine.
        Before we do so, we must create a ResourceList to pass to the Start
        routine.
    */
    if ( irp_stack->MinorFunction == IRP_MN_START_DEVICE )
    {
        IResourceList* resource_list;
        DPRINT("IRP_MN_START_DEVICE\n");

        /* Create the resource list */
        status = PcNewResourceList(
                    &resource_list,
                    NULL,
                    PagedPool,
                    irp_stack->Parameters.StartDevice.AllocatedResourcesTranslated,
                    irp_stack->Parameters.StartDevice.AllocatedResources);

        if ( ! NT_SUCCESS(status) )
        {
            DPRINT("PcNewResourceList failed [0x%8x]\n", status);
            Irp->IoStatus.Status = status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);

            return status;
        }

        /* Assign the resource list to our extension */
        portcls_ext->resources = resource_list;

        ASSERT(portcls_ext->StartDevice);

        /* Call the StartDevice routine */
        DPRINT("Calling StartDevice at 0x%8x\n", portcls_ext->StartDevice);
        status = portcls_ext->StartDevice(DeviceObject, Irp, resource_list);

        if ( ! NT_SUCCESS(status) )
        {
            DPRINT("StartDevice returned a failure code [0x%8x]\n", status);
            resource_list->lpVtbl->Release(resource_list);

            Irp->IoStatus.Status = status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);

            return status;
        }

        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }
    else if ( irp_stack->MinorFunction == IRP_MN_REMOVE_DEVICE )
    {
        DPRINT("IRP_MN_REMOVE_DEVICE\n");
        /* Clean up */
        portcls_ext->resources->lpVtbl->Release(portcls_ext->resources);

        IoDeleteDevice(DeviceObject);

        /* Do not complete? */
        Irp->IoStatus.Status = STATUS_SUCCESS;
    }

    return STATUS_SUCCESS;
}

/*
    Power management. Handles IRP_MJ_POWER
    (not implemented)
*/
NTAPI
NTSTATUS
PortClsPower(
    IN  PDEVICE_OBJECT DeviceObject,
    IN  PIRP Irp)
{
    DPRINT1("PortClsPower called\n");

    /* TODO */

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

/*
    System control. Handles IRP_MJ_SYSTEM_CONTROL
    (not implemented)
*/
NTAPI
NTSTATUS
PortClsSysControl(
    IN  PDEVICE_OBJECT DeviceObject,
    IN  PIRP Irp)
{
    DPRINT1("PortClsSysControl called\n");

    /* TODO */

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}


/*
    ==========================================================================
    API EXPORTS
    ==========================================================================
*/

/*
    Drivers may implement their own IRP handlers. If a driver decides to let
    PortCls handle the IRP, it can do so by calling this.
*/
PORTCLASSAPI NTSTATUS NTAPI
PcDispatchIrp(
    IN  PDEVICE_OBJECT DeviceObject,
    IN  PIRP Irp)
{
    PIO_STACK_LOCATION irp_stack;

    DPRINT("PcDispatchIrp called - handling IRP in PortCls\n");

    irp_stack = IoGetCurrentIrpStackLocation(Irp);

    switch ( irp_stack->MajorFunction )
    {
        /* PortCls */
        case IRP_MJ_CREATE :
            return PortClsCreate(DeviceObject, Irp);

        case IRP_MJ_PNP :
            return PortClsPnp(DeviceObject, Irp);

        case IRP_MJ_POWER :
            return PortClsPower(DeviceObject, Irp);

        case IRP_MJ_SYSTEM_CONTROL :
            return PortClsSysControl(DeviceObject, Irp);

        /* KS - TODO */

#if 0
    KsSetMajorFunctionHandler(DriverObject, IRP_MJ_CLOSE);
    KsSetMajorFunctionHandler(DriverObject, IRP_MJ_DEVICE_CONTROL);
    KsSetMajorFunctionHandler(DriverObject, IRP_MJ_FLUSH_BUFFERS);
    KsSetMajorFunctionHandler(DriverObject, IRP_MJ_QUERY_SECURITY);
    KsSetMajorFunctionHandler(DriverObject, IRP_MJ_READ);
    KsSetMajorFunctionHandler(DriverObject, IRP_MJ_SET_SECURITY);
    KsSetMajorFunctionHandler(DriverObject, IRP_MJ_WRITE);
#endif

        default :
            break;
    };

    /* If we reach here, we just complete the IRP */
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

/*
 * @unimplemented
 */
PORTCLASSAPI NTSTATUS NTAPI
PcCompleteIrp(
    IN  PDEVICE_OBJECT DeviceObject,
    IN  PIRP Irp,
    IN  NTSTATUS Status)
{
    UNIMPLEMENTED;
    return STATUS_UNSUCCESSFUL;
}

/*
 * @unimplemented
 */
PORTCLASSAPI NTSTATUS NTAPI
PcForwardIrpSynchronous(
    IN  PDEVICE_OBJECT DeviceObject,
    IN  PIRP Irp)
{
    UNIMPLEMENTED;
    return STATUS_UNSUCCESSFUL;
}
