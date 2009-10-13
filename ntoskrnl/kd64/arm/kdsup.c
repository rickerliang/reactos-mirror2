/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            ntoskrnl/kd64/arm/kdsup.c
 * PURPOSE:         KD support routines for ARM
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

/* INCLUDES *****************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <debug.h>

#undef UNIMPLEMENTED
#define UNIMPLEMENTED KdpDprintf("%s is unimplemented\n", __FUNCTION__)

/* FUNCTIONS *****************************************************************/

VOID
NTAPI
KdpGetStateChange(IN PDBGKD_MANIPULATE_STATE64 State,
                  IN PCONTEXT Context)
{
    UNIMPLEMENTED;
    while (TRUE);
}

VOID
NTAPI
KdpSetContextState(IN PDBGKD_WAIT_STATE_CHANGE64 WaitStateChange,
                   IN PCONTEXT Context)
{
    UNIMPLEMENTED;
    while (TRUE);
}

VOID
NTAPI
KdpSysGetVersion(IN PDBGKD_GET_VERSION64 Version)
{
    UNIMPLEMENTED;
    while (TRUE);
}

NTSTATUS
NTAPI
KdpSysReadMsr(IN ULONG Msr,
              OUT PLARGE_INTEGER MsrValue)
{
    UNIMPLEMENTED;
    while (TRUE);
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS
NTAPI
KdpSysWriteMsr(IN ULONG Msr,
               IN PLARGE_INTEGER MsrValue)
{
    UNIMPLEMENTED;
    while (TRUE);
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS
NTAPI
KdpSysReadBusData(IN ULONG BusDataType,
                  IN ULONG BusNumber,
                  IN ULONG SlotNumber,
                  IN PVOID Buffer,
                  IN ULONG Offset,
                  IN ULONG Length,
                  OUT PULONG ActualLength)
{
    UNIMPLEMENTED;
    while (TRUE);
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS
NTAPI
KdpSysWriteBusData(IN ULONG BusDataType,
                   IN ULONG BusNumber,
                   IN ULONG SlotNumber,
                   IN PVOID Buffer,
                   IN ULONG Offset,
                   IN ULONG Length,
                   OUT PULONG ActualLength)
{
    UNIMPLEMENTED;
    while (TRUE);
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS
NTAPI
KdpSysReadControlSpace(IN ULONG Processor,
                       IN ULONG64 BaseAddress,
                       IN PVOID Buffer,
                       IN ULONG Length,
                       OUT PULONG ActualLength)
{
    UNIMPLEMENTED;
    while (TRUE);
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS
NTAPI
KdpSysWriteControlSpace(IN ULONG Processor,
                        IN ULONG64 BaseAddress,
                        IN PVOID Buffer,
                        IN ULONG Length,
                        OUT PULONG ActualLength)
{
    UNIMPLEMENTED;
    while (TRUE);
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS
NTAPI
KdpSysReadIoSpace(IN ULONG InterfaceType,
                  IN ULONG BusNumber,
                  IN ULONG AddressSpace,
                  IN ULONG64 IoAddress,
                  IN PVOID DataValue,
                  IN ULONG DataSize,
                  OUT PULONG ActualDataSize)
{
    UNIMPLEMENTED;
    while (TRUE);
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS
NTAPI
KdpSysWriteIoSpace(IN ULONG InterfaceType,
                   IN ULONG BusNumber,
                   IN ULONG AddressSpace,
                   IN ULONG64 IoAddress,
                   IN PVOID DataValue,
                   IN ULONG DataSize,
                   OUT PULONG ActualDataSize)
{
    UNIMPLEMENTED;
    while (TRUE);
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS
NTAPI
KdpSysCheckLowMemory(IN ULONG Flags)
{
    UNIMPLEMENTED;
    while (TRUE);
    return STATUS_UNSUCCESSFUL;
}
