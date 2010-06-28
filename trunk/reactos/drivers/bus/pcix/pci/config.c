/*
 * PROJECT:         ReactOS PCI Bus Driver
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            drivers/bus/pci/pci/config.c
 * PURPOSE:         PCI Configuration Space Routines
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

/* INCLUDES *******************************************************************/

#include <pci.h>
#define NDEBUG
#include <debug.h>

/* GLOBALS ********************************************************************/

BOOLEAN PciAssignBusNumbers;

/* FUNCTIONS ******************************************************************/

NTSTATUS
NTAPI
PciQueryForPciBusInterface(IN PPCI_FDO_EXTENSION FdoExtension)
{
    PDEVICE_OBJECT AttachedDevice;
    IO_STATUS_BLOCK IoStatusBlock;
    KEVENT Event;
    NTSTATUS Status;
    PIRP Irp;
    PIO_STACK_LOCATION IoStackLocation;
    PPCI_BUS_INTERFACE_STANDARD PciInterface;
    PAGED_CODE();
    ASSERT(PCI_IS_ROOT_FDO(FdoExtension));

    /* Allocate space for the inteface */
    PciInterface = ExAllocatePoolWithTag(NonPagedPool,
                                         sizeof(PCI_BUS_INTERFACE_STANDARD),
                                         PCI_POOL_TAG);
    if (!PciInterface) return STATUS_INSUFFICIENT_RESOURCES;

    /* Get the device the PDO is attached to, should be the Root (ACPI) */
    AttachedDevice = IoGetAttachedDeviceReference(FdoExtension->PhysicalDeviceObject);

    /* Build an IRP for this request */
    KeInitializeEvent(&Event, SynchronizationEvent, FALSE);
    Irp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP,
                                       AttachedDevice,
                                       NULL,
                                       0,
                                       NULL,
                                       &Event,
                                       &IoStatusBlock);
    if (Irp)
    {
        /* Initialize the default PnP response */
        Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
        Irp->IoStatus.Information = 0;

        /* Make it a Query Interface IRP */
        IoStackLocation = IoGetNextIrpStackLocation(Irp);
        ASSERT(IoStackLocation->MajorFunction == IRP_MJ_PNP);
        IoStackLocation->MinorFunction = IRP_MN_QUERY_INTERFACE;
        IoStackLocation->Parameters.QueryInterface.InterfaceType = &GUID_PCI_BUS_INTERFACE_STANDARD;
        IoStackLocation->Parameters.QueryInterface.Size = sizeof(GUID_PCI_BUS_INTERFACE_STANDARD);
        IoStackLocation->Parameters.QueryInterface.Version = PCI_BUS_INTERFACE_STANDARD_VERSION;
        IoStackLocation->Parameters.QueryInterface.Interface = (PINTERFACE)PciInterface;
        IoStackLocation->Parameters.QueryInterface.InterfaceSpecificData = NULL;

        /* Send it to the root PDO */
        Status = IoCallDriver(AttachedDevice, Irp);
        if (Status == STATUS_PENDING)
        {
            /* Wait for completion */
            KeWaitForSingleObject(&Event,
                                  Executive,
                                  KernelMode,
                                  FALSE,
                                  NULL);
            Status = Irp->IoStatus.Status;
        }

        /* Check if an interface was returned */
        if (!NT_SUCCESS(Status))
        {
            /* No interface was returned by the root PDO */
            FdoExtension->PciBusInterface = NULL;
            ExFreePoolWithTag(PciInterface, 0);
        }
        else
        {
            /* An interface was returned, save it */
            FdoExtension->PciBusInterface = PciInterface;
        }

        /* Dereference the device object because we took a reference earlier */
        ObfDereferenceObject(AttachedDevice);
    }
    else
    {
        /* Failure path, dereference the device object and set failure code */
        if (AttachedDevice) ObfDereferenceObject(AttachedDevice);
        ExFreePoolWithTag(PciInterface, 0);
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Return status code to caller */
    return Status;
}

NTSTATUS
NTAPI
PciGetConfigHandlers(IN PPCI_FDO_EXTENSION FdoExtension)
{
    PBUS_HANDLER BusHandler;
    NTSTATUS Status;
    ASSERT(FdoExtension->BusHandler == NULL);

    /* Check if this is the FDO for the root bus */
    if (PCI_IS_ROOT_FDO(FdoExtension))
    {
        /* Query the PCI Bus Interface that ACPI exposes */
        ASSERT(FdoExtension->PciBusInterface == NULL);
        Status = PciQueryForPciBusInterface(FdoExtension);
        if (!NT_SUCCESS(Status))
        {
            /* No ACPI, so Bus Numbers should be maintained by BIOS */
            ASSERT(!PciAssignBusNumbers);
        }
        else
        {
            /* ACPI detected, PCI Bus Driver will reconfigure bus numbers*/
            PciAssignBusNumbers = TRUE;
        }
    }
    else
    {
        /* Check if the root bus already has the interface set up */
        if (FdoExtension->BusRootFdoExtension->PciBusInterface)
        {
            /* Nothing for this FDO to do */
            return STATUS_SUCCESS;
        }

        /* Fail into case below so we can query the HAL interface */
        Status = STATUS_NOT_SUPPORTED;
    }

    /* If the ACPI PCI Bus Interface couldn't be obtained, try the HAL */
    if (!NT_SUCCESS(Status))
    {
        /* Bus number assignment should be static */
        ASSERT(Status == STATUS_NOT_SUPPORTED);
        ASSERT(!PciAssignBusNumbers);

        /* Call the HAL to obtain the bus handler for PCI */
        BusHandler = HalReferenceHandlerForBus(PCIBus, FdoExtension->BaseBus);
        FdoExtension->BusHandler = BusHandler;

        /* Fail if the HAL does not have a PCI Bus Handler for this bus */
        if (!BusHandler) return STATUS_INVALID_DEVICE_REQUEST;
    }

    /* Appropriate interface was obtained */
    return STATUS_SUCCESS;
}

/* EOF */
