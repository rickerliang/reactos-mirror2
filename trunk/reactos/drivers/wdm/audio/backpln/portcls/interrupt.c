#include "private.h"

typedef struct
{
    LIST_ENTRY ListEntry;
    PINTERRUPTSYNCROUTINE SyncRoutine;
    PVOID DynamicContext;
}SYNC_ENTRY, *PSYNC_ENTRY;

typedef struct
{
    IInterruptSyncVtbl *lpVtbl;

    LONG ref;
    KSPIN_LOCK Lock;
    LIST_ENTRY ServiceRoutines;
    PKINTERRUPT Interrupt;
    INTERRUPTSYNCMODE Mode;
    PRESOURCELIST ResourceList;
    ULONG ResourceIndex;

    PINTERRUPTSYNCROUTINE SyncRoutine;
    PVOID DynamicContext;
}IInterruptSyncImpl;


//---------------------------------------------------------------
// IUnknown methods
//



NTSTATUS
NTAPI
IInterruptSync_fnQueryInterface(
    IInterruptSync * iface,
    IN  REFIID refiid,
    OUT PVOID* Output)
{
    IInterruptSyncImpl * This = (IInterruptSyncImpl*)iface;

    DPRINT1("IInterruptSync_fnQueryInterface: This %p\n", This);

    if (IsEqualGUIDAligned(refiid, &IID_IInterruptSync))
    {
        *Output = &This->lpVtbl;
        _InterlockedIncrement(&This->ref);
        return STATUS_SUCCESS;
    }
    DPRINT1("IInterruptSync_fnQueryInterface: This %p UNKNOWN interface requested\n", This);
    return STATUS_UNSUCCESSFUL;
}

ULONG
NTAPI
IInterruptSync_fnAddRef(
    IInterruptSync * iface)
{
    IInterruptSyncImpl * This = (IInterruptSyncImpl*)iface;

    DPRINT1("IInterruptSync_AddRef: This %p\n", This);

    return _InterlockedIncrement(&This->ref);
}

ULONG
NTAPI
IInterruptSync_fnRelease(
    IInterruptSync* iface)
{
    PLIST_ENTRY CurEntry;
    PSYNC_ENTRY Entry;
    IInterruptSyncImpl * This = (IInterruptSyncImpl*)iface;

    _InterlockedDecrement(&This->ref);

    DPRINT1("IInterruptSync_Release: This %p new ref %u\n", This, This->ref);

    if (This->ref == 0)
    {
        while(!IsListEmpty(&This->ServiceRoutines))
        {
            CurEntry = RemoveHeadList(&This->ServiceRoutines);
            Entry = CONTAINING_RECORD(CurEntry, SYNC_ENTRY, ListEntry);
            FreeItem(Entry, TAG_PORTCLASS);
        }

        //This->ResourceList->lpVtbl->Release(This->ResourceList);
        //FreeItem(This, TAG_PORTCLASS);
DPRINT1("IInterruptSync_Release: complete\n");
        return 0;
    }
    /* Return new reference count */
    return This->ref;
}

//---------------------------------------------------------------
// IInterruptSync methods
//


BOOLEAN
NTAPI
IInterruptSynchronizedRoutine(
    IN PVOID  ServiceContext)
{
    IInterruptSyncImpl * This = (IInterruptSyncImpl*)ServiceContext;
    DPRINT1("IInterruptSynchronizedRoutine This %p SyncRoutine%p\n", This, This->SyncRoutine);
    return This->SyncRoutine((IInterruptSync*)&This->lpVtbl, This->DynamicContext);
}

NTSTATUS
NTAPI
IInterruptSync_fnCallSynchronizedRoutine(
    IN IInterruptSync * iface,
    IN PINTERRUPTSYNCROUTINE Routine,
    IN PVOID DynamicContext)
{
    KIRQL OldIrql;
    IInterruptSyncImpl * This = (IInterruptSyncImpl*)iface;

    DPRINT1("IInterruptSync_fnCallSynchronizedRoutine This %p Routine %p DynamicContext %p\n", This, Routine, DynamicContext);

    if (!This->Interrupt)
    {
        DPRINT("IInterruptSync_CallSynchronizedRoutine %p no interrupt connected\n", This);
        if (KeGetCurrentIrql() > DISPATCH_LEVEL)
            return STATUS_UNSUCCESSFUL;

        KeAcquireSpinLock(&This->Lock, &OldIrql);
        This->SyncRoutine = Routine;
        This->DynamicContext = DynamicContext;
        IInterruptSynchronizedRoutine((PVOID)This);
        KeReleaseSpinLock(&This->Lock, OldIrql);

        return STATUS_SUCCESS;
    }

    This->SyncRoutine = Routine;
    This->DynamicContext = DynamicContext;

    return KeSynchronizeExecution(This->Interrupt, IInterruptSynchronizedRoutine, (PVOID)This);
}

NTAPI
PKINTERRUPT
IInterruptSync_fnGetKInterrupt(
    IN IInterruptSync * iface)
{
    IInterruptSyncImpl * This = (IInterruptSyncImpl*)iface;
    DPRINT1("IInterruptSynchronizedRoutine\n");

    return This->Interrupt;
}

BOOLEAN
NTAPI
IInterruptServiceRoutine(
    IN PKINTERRUPT  Interrupt,
    IN PVOID  ServiceContext)
{
    PLIST_ENTRY CurEntry;
    PSYNC_ENTRY Entry;
    NTSTATUS Status;
    BOOL Success;
    IInterruptSyncImpl * This = (IInterruptSyncImpl*)ServiceContext;

    DPRINT1("IInterruptServiceRoutine\n");

    if (This->Mode == InterruptSyncModeNormal)
    {
        CurEntry = This->ServiceRoutines.Flink;
        while (CurEntry != &This->ServiceRoutines)
        {
            Entry = CONTAINING_RECORD(CurEntry, SYNC_ENTRY, ListEntry);
            Status = Entry->SyncRoutine((IInterruptSync*)This, Entry->DynamicContext);
            if (NT_SUCCESS(Status))
            {
                return TRUE;
            }
            CurEntry = CurEntry->Flink;
        }
        return FALSE;
    }
    else if (This->Mode == InterruptSyncModeAll)
    {
        CurEntry = This->ServiceRoutines.Flink;
        while (CurEntry != &This->ServiceRoutines)
        {
            Entry = CONTAINING_RECORD(CurEntry, SYNC_ENTRY, ListEntry);
            Status = Entry->SyncRoutine((IInterruptSync*)This, Entry->DynamicContext);
            CurEntry = CurEntry->Flink;
        }
        DPRINT("Returning TRUE with mode InterruptSyncModeAll\n");
        return TRUE; //FIXME
    }
    else if (This->Mode == InterruptSyncModeRepeat)
    {
        do
        {
            Success = FALSE;
            CurEntry = This->ServiceRoutines.Flink;
            while (CurEntry != &This->ServiceRoutines)
            {
                Entry = CONTAINING_RECORD(CurEntry, SYNC_ENTRY, ListEntry);
                Status = Entry->SyncRoutine((IInterruptSync*)This, Entry->DynamicContext);
                if (NT_SUCCESS(Status))
                    Success = TRUE;
                CurEntry = CurEntry->Flink;
            }
        }while(Success);
        DPRINT("Returning TRUE with mode InterruptSyncModeRepeat\n");
        return TRUE; //FIXME
    }
    else
    {
        DPRINT("Unknown mode %u\n", This->Mode);
        return FALSE; //FIXME
    }
}


NTSTATUS
NTAPI
IInterruptSync_fnConnect(
    IN IInterruptSync * iface)
{
    IInterruptSyncImpl * This = (IInterruptSyncImpl*)iface;
    NTSTATUS Status;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor;

    DPRINT1("IInterruptSync_fnConnect\n");

    Descriptor = This->ResourceList->lpVtbl->FindTranslatedEntry(This->ResourceList, CmResourceTypeInterrupt, This->ResourceIndex);
    if (!Descriptor)
        return STATUS_UNSUCCESSFUL;

    if (IsListEmpty(&This->ServiceRoutines))
        return STATUS_UNSUCCESSFUL;

    Status = IoConnectInterrupt(&This->Interrupt, 
                                IInterruptServiceRoutine,
                                (PVOID)This,
                                &This->Lock, Descriptor->u.Interrupt.Vector, 
                                Descriptor->u.Interrupt.Level,
                                Descriptor->u.Interrupt.Level, //FIXME
                                LevelSensitive, //FIXME
                                TRUE, //FIXME
                                Descriptor->u.Interrupt.Affinity, 
                                FALSE);

    return Status;
}


VOID
NTAPI
IInterruptSync_fnDisconnect(
    IN IInterruptSync * iface)
{
    IInterruptSyncImpl * This = (IInterruptSyncImpl*)iface;
    DPRINT1("IInterruptSync_fnDisconnect\n");

    if (!This->Interrupt)
    {
        DPRINT("IInterruptSync_Disconnect %p no interrupt connected\n", This);
        return;
    }

    IoDisconnectInterrupt(This->Interrupt);
    This->Interrupt = NULL;
}

NTSTATUS
NTAPI
IInterruptSync_fnRegisterServiceRoutine(
    IN IInterruptSync * iface,
    IN      PINTERRUPTSYNCROUTINE   Routine,
    IN      PVOID                   DynamicContext,
    IN      BOOLEAN                 First)
{
    PSYNC_ENTRY NewEntry;
    IInterruptSyncImpl * This = (IInterruptSyncImpl*)iface;

DPRINT1("IInterruptSync_fnRegisterServiceRoutine\n");

    NewEntry = AllocateItem(NonPagedPool, sizeof(SYNC_ENTRY), TAG_PORTCLASS);
    if (!NewEntry)
        return STATUS_INSUFFICIENT_RESOURCES;

    NewEntry->SyncRoutine = Routine;
    NewEntry->DynamicContext = DynamicContext;

    if (First)
        InsertHeadList(&This->ServiceRoutines, &NewEntry->ListEntry);
    else
        InsertTailList(&This->ServiceRoutines, &NewEntry->ListEntry);

    return STATUS_SUCCESS;
}

static IInterruptSyncVtbl vt_IInterruptSyncVtbl = 
{
    /* IUnknown methods */
    IInterruptSync_fnQueryInterface,
    IInterruptSync_fnAddRef,
    IInterruptSync_fnRelease,
    /* IInterruptSync methods */
    IInterruptSync_fnCallSynchronizedRoutine,
    IInterruptSync_fnGetKInterrupt,
    IInterruptSync_fnConnect,
    IInterruptSync_fnDisconnect,
    IInterruptSync_fnRegisterServiceRoutine
};

/*
 * @unimplemented
 */
NTSTATUS NTAPI
PcNewInterruptSync(
    OUT PINTERRUPTSYNC* OutInterruptSync,
    IN  PUNKNOWN OuterUnknown OPTIONAL,
    IN  PRESOURCELIST ResourceList,
    IN  ULONG ResourceIndex,
    IN  INTERRUPTSYNCMODE Mode)
{
    IInterruptSyncImpl * This;

    DPRINT1("PcNewInterruptSync entered\n");

    if (!OutInterruptSync || !ResourceList || Mode > InterruptSyncModeRepeat || Mode < 0)
        return STATUS_INVALID_PARAMETER;

    if (ResourceIndex > ResourceList->lpVtbl->NumberOfEntriesOfType(ResourceList, CmResourceTypeInterrupt))
        return STATUS_INVALID_PARAMETER;


     ResourceList->lpVtbl->AddRef(ResourceList);

    This = AllocateItem(NonPagedPool, sizeof(IInterruptSyncImpl), TAG_PORTCLASS);
    if (!This)
        return STATUS_INSUFFICIENT_RESOURCES;

    /* initialize object */
    This->lpVtbl = &vt_IInterruptSyncVtbl;
    This->ref = 1;
    This->Mode = Mode;
    This->ResourceIndex = ResourceIndex;
    This->ResourceList = ResourceList;
    InitializeListHead(&This->ServiceRoutines);
    KeInitializeSpinLock(&This->Lock);

    *OutInterruptSync = (PINTERRUPTSYNC)&This->lpVtbl;
    DPRINT1("PcNewInterruptSync success %p\n", *OutInterruptSync);
    return STATUS_SUCCESS;
}

