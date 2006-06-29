/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS kernel
 * FILE:            ntoskrnl/io/irp.c
 * PURPOSE:         Handle IRPs
 *
 * PROGRAMMERS:     Alex Ionescu (alex@relsoft.net)
 *                  David Welch (welch@mcmail.com)
 */

/* INCLUDES ****************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <internal/debug.h>

/* PRIVATE FUNCTIONS  ********************************************************/

VOID
NTAPI
IopFreeIrpKernelApc(IN PKAPC Apc,
                    IN PKNORMAL_ROUTINE *NormalRoutine,
                    IN PVOID *NormalContext,
                    IN PVOID *SystemArgument1,
                    IN PVOID *SystemArgument2)
{
    /* Free the IRP */
    IoFreeIrp(CONTAINING_RECORD(Apc, IRP, Tail.Apc));
}

VOID
NTAPI
IopAbortIrpKernelApc(IN PKAPC Apc)
{
    /* Free the IRP */
    IoFreeIrp(CONTAINING_RECORD(Apc, IRP, Tail.Apc));
}

VOID
NTAPI
IopRemoveThreadIrp(VOID)
{
    KIRQL OldIrql;
    PIRP DeadIrp;
    PETHREAD IrpThread;
    PLIST_ENTRY IrpEntry;
    PIO_ERROR_LOG_PACKET ErrorLogEntry;

    /* First, raise to APC to protect IrpList */
    KeRaiseIrql(APC_LEVEL, &OldIrql);

    /* Get the Thread and check the list */
    IrpThread = PsGetCurrentThread();
    if (IsListEmpty(&IrpThread->IrpList))
    {
        /* It got completed now, so quit */
        KeLowerIrql(OldIrql);
        return;
    }

    /* Get the misbehaving IRP */
    IrpEntry = IrpThread->IrpList.Flink;
    DeadIrp = CONTAINING_RECORD(IrpEntry, IRP, ThreadListEntry);

    /* Disown the IRP! */
    DeadIrp->Tail.Overlay.Thread = NULL;
    InitializeListHead(&DeadIrp->ThreadListEntry);
    RemoveHeadList(&IrpThread->IrpList);

    /* Lower IRQL now */
    KeLowerIrql(OldIrql);

    /* Check if we can send an Error Log Entry*/
    if (DeadIrp->CurrentLocation <= DeadIrp->StackCount)
    {
        /* Allocate an entry */
        ErrorLogEntry = IoAllocateErrorLogEntry(
            IoGetCurrentIrpStackLocation(DeadIrp)->DeviceObject,
                                         sizeof(IO_ERROR_LOG_PACKET));

        /* Write the entry */
        ErrorLogEntry->ErrorCode = 0xBAADF00D; /* FIXME */
        IoWriteErrorLogEntry(ErrorLogEntry);
    }
}

VOID
NTAPI
IopCleanupIrp(IN PIRP Irp,
              IN PFILE_OBJECT FileObject)
{
    PMDL Mdl;

    /* Check if there's an MDL */
    while ((Mdl = Irp->MdlAddress))
    {
        /* Clear all of them */
        Irp->MdlAddress = Mdl->Next;
        IoFreeMdl(Mdl);
    }

    /* Free the buffer */
    if (Irp->Flags & IRP_DEALLOCATE_BUFFER)
    {
        ExFreePoolWithTag(Irp->AssociatedIrp.SystemBuffer, TAG_SYS_BUF);
    }

    /* Derefernce the User Event */
    if (Irp->UserEvent && !(Irp->Flags & IRP_SYNCHRONOUS_API) && FileObject)
    {
        ObDereferenceObject(Irp->UserEvent);
    }

    /* Dereference the File Object */
    if (FileObject) ObDereferenceObject(FileObject);

    /* Free the IRP */
    IoFreeIrp(Irp);
}

VOID
NTAPI
IopCompleteRequest(IN PKAPC Apc,
                   IN PKNORMAL_ROUTINE* NormalRoutine,
                   IN PVOID* NormalContext,
                   IN PVOID* SystemArgument1,
                   IN PVOID* SystemArgument2)
{
    PFILE_OBJECT FileObject;
    PIRP Irp;
    PMDL Mdl;
    PKEVENT UserEvent;
    BOOLEAN SyncIrp;

    if (Apc) DPRINT("IoSecondStageCompletition with APC: 0x%p\n", Apc);

    /* Get data from the APC */
    FileObject = (PFILE_OBJECT)(*SystemArgument1);
    Irp = CONTAINING_RECORD(Apc, IRP, Tail.Apc);
    DPRINT("IoSecondStageCompletition, 0x%p\n", Irp);

    /* Save the User Event */
    UserEvent = Irp->UserEvent;

    /* Remember if the IRP is Sync or not */
    SyncIrp = Irp->Flags & IRP_SYNCHRONOUS_API ? TRUE : FALSE;

    /* Handle Buffered case first */
    if (Irp->Flags & IRP_BUFFERED_IO)
    {
        /* Check if we have an input buffer and if we suceeded */
        if ((Irp->Flags & IRP_INPUT_OPERATION) && 
            (Irp->IoStatus.Status != STATUS_VERIFY_REQUIRED) &&
            !(NT_ERROR(Irp->IoStatus.Status)))
        {
            /* Copy the buffer back to the user */
            RtlCopyMemory(Irp->UserBuffer,
                          Irp->AssociatedIrp.SystemBuffer,
                          Irp->IoStatus.Information);
        }

        /* Also check if we should de-allocate it */
        if (Irp->Flags & IRP_DEALLOCATE_BUFFER)
        {
            ExFreePoolWithTag(Irp->AssociatedIrp.SystemBuffer, TAG_SYS_BUF);
        }
    }

    /* Now we got rid of these two... */
    Irp->Flags &= ~(IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER);

    /* Check if there's an MDL */
    while ((Mdl = Irp->MdlAddress))
    {
        /* Clear all of them */
        Irp->MdlAddress = Mdl->Next;
        IoFreeMdl(Mdl);
    }

    /* Remove the IRP from the list of Thread Pending IRPs */
    RemoveEntryList(&Irp->ThreadListEntry);
    InitializeListHead(&Irp->ThreadListEntry);

#if 1
    /* Check for Success but allow failure for Async IRPs */
    if (!(NT_ERROR(Irp->IoStatus.Status)) ||
        ((NT_ERROR(Irp->IoStatus.Status)) &&
        (Irp->PendingReturned) && !(SyncIrp) &&
        ((FileObject == NULL) || (FileObject->Flags & FO_SYNCHRONOUS_IO))))
    {
        _SEH_TRY
        {
            /*  Save the IOSB Information */
            *Irp->UserIosb = Irp->IoStatus;
        }
        _SEH_HANDLE
        {
            /* Ignore any error */
        }
        _SEH_END;

        /* Check if there's an event */
        if (UserEvent)
        {
            /* Check if we should signal the File Object Event as well */
            if (FileObject)
            {
                /*
                 * If the File Object is SYNC, then we need to
                 * signal its event too
                 */
                if (FileObject->Flags & FO_SYNCHRONOUS_IO)
                {
                    /* Set the Status */
                    FileObject->FinalStatus = Irp->IoStatus.Status;

                    /* Signal Event */
                    KeSetEvent(&FileObject->Event, 0, FALSE);
                }
            }

            /* Signal the Event */
            KeSetEvent(UserEvent, 0, FALSE);

            /* Dereference the Event if this is an ASYNC IRP */
            if (FileObject && !SyncIrp && UserEvent != &FileObject->Event)
            {
                ObDereferenceObject(UserEvent);
            }
        }
        else if (FileObject)
        {
            /* Set the Status */
            FileObject->FinalStatus = Irp->IoStatus.Status;

            /* Signal the File Object Instead */
            KeSetEvent(&FileObject->Event, 0, FALSE);
        }

        /* Now call the User APC if one was requested */
        if (Irp->Overlay.AsynchronousParameters.UserApcRoutine)
        {
            KeInitializeApc(&Irp->Tail.Apc,
                            KeGetCurrentThread(),
                            CurrentApcEnvironment,
                            IopFreeIrpKernelApc,
                            IopAbortIrpKernelApc,
                            (PKNORMAL_ROUTINE)Irp->
                            Overlay.AsynchronousParameters.UserApcRoutine,
                            Irp->RequestorMode,
                            Irp->
                            Overlay.AsynchronousParameters.UserApcContext);

            KeInsertQueueApc(&Irp->Tail.Apc,
                             Irp->UserIosb,
                             NULL,
                             2);
            Irp = NULL;
        }
        else if (FileObject && FileObject->CompletionContext)
        {
            /* Call the IO Completion Port if we have one, instead */
            Irp->Tail.CompletionKey = FileObject->CompletionContext->Key;
            Irp->Tail.Overlay.PacketType = IrpCompletionPacket;
            KeInsertQueue(FileObject->CompletionContext->Port,
                          &Irp->Tail.Overlay.ListEntry);
            Irp = NULL;
        }
    }
    else
    {
        /*
         * Signal the Events only if PendingReturned and we have a File Object
         */
        if (FileObject && Irp->PendingReturned)
        {
            /* Check for SYNC IRP */
            if (SyncIrp)
            {
                /* Set the status in this case only */
                _SEH_TRY
                {
                    *Irp->UserIosb = Irp->IoStatus;
                }
                _SEH_HANDLE
                {
                    /* Ignore any error */
                }
                _SEH_END;

                /* Signal our event if we have one */
                if (UserEvent)
                {
                    KeSetEvent(UserEvent, 0, FALSE);
                }
                else
                {
                    /* Signal the File's Event instead */
                    KeSetEvent(&FileObject->Event, 0, FALSE);
                }
            }
            else
            {
#if 1
                /* FIXME: This is necessary to fix bug #609 */
                _SEH_TRY
                {
                    *Irp->UserIosb = Irp->IoStatus;
                }
                _SEH_HANDLE
                {
                    /* Ignore any error */
                }
                _SEH_END;
#endif
                /* We'll report the Status to the File Object, not the IRP */
                FileObject->FinalStatus = Irp->IoStatus.Status;

                /* Signal the File Object ONLY if this was Async */
                KeSetEvent(&FileObject->Event, 0, FALSE);
            }
        }

        /* Dereference the Event if it's an ASYNC IRP on a File Object */
        if (UserEvent && !SyncIrp && FileObject)
        {
            if (UserEvent != &FileObject->Event)
            {
                ObDereferenceObject(UserEvent);
            }
        }
    }

    /* Dereference the File Object */
    if (FileObject) ObDereferenceObject(FileObject);

    /* Free the IRP */
    if (Irp) IoFreeIrp(Irp);

#else

    if (NT_SUCCESS(Irp->IoStatus.Status) || Irp->PendingReturned)
    {
        _SEH_TRY
        {
            /*  Save the IOSB Information */
            *Irp->UserIosb = Irp->IoStatus;
        }
        _SEH_HANDLE
        {
            /* Ignore any error */
        }
        _SEH_END;

        if (FileObject)
        {
            if (FileObject->Flags & FO_SYNCHRONOUS_IO)
            {
                /* Set the Status */
                FileObject->FinalStatus = Irp->IoStatus.Status;

                /* FIXME: Remove this check when I/O code is fixed */
                if (UserEvent != &FileObject->Event)
                {
                    /* Signal Event */
                    KeSetEvent(&FileObject->Event, 0, FALSE);
                }
            }
        }

        /* Signal the user event, if one exist */
        if (UserEvent)
        {
            KeSetEvent(UserEvent, 0, FALSE);
        }

        /* Now call the User APC if one was requested */
        if (Irp->Overlay.AsynchronousParameters.UserApcRoutine)
        {
            KeInitializeApc(&Irp->Tail.Apc,
                            KeGetCurrentThread(),
                            CurrentApcEnvironment,
                            IopFreeIrpKernelApc,
                            IopAbortIrpKernelApc,
                            (PKNORMAL_ROUTINE)Irp->
                            Overlay.AsynchronousParameters.UserApcRoutine,
                            Irp->RequestorMode,
                            Irp->
                            Overlay.AsynchronousParameters.UserApcContext);

            KeInsertQueueApc(&Irp->Tail.Apc,
                             Irp->UserIosb,
                             NULL,
                             2);
            Irp = NULL;
        }
        else if (FileObject && FileObject->CompletionContext)
        {
            /* Call the IO Completion Port if we have one, instead */
            IoSetIoCompletion(FileObject->CompletionContext->Port,
                              FileObject->CompletionContext->Key,
                              Irp->
                              Overlay.AsynchronousParameters.UserApcContext,
                              Irp->IoStatus.Status,
                              Irp->IoStatus.Information,
                              FALSE);
            Irp = NULL;
        }
    }

    /* Free the Irp if it hasn't already */
    if (Irp) IoFreeIrp(Irp);

    if (FileObject)
    {
        /* Dereference the user event, if it is an event object */
        /* FIXME: Remove last check when I/O code is fixed */
        if (UserEvent && !SyncIrp && UserEvent != &FileObject->Event)
        {
            ObDereferenceObject(UserEvent);
        }

        /* Dereference the File Object */
        ObDereferenceObject(FileObject);
    }
#endif
}

/* FUNCTIONS *****************************************************************/

/*
 * @implemented
 */
PIRP
NTAPI
IoAllocateIrp(CCHAR StackSize,
              BOOLEAN ChargeQuota)
{
    PIRP Irp = NULL;
    USHORT Size = IoSizeOfIrp(StackSize);
    PKPRCB Prcb;
    UCHAR Flags = 0;
    PNPAGED_LOOKASIDE_LIST List = NULL;
    PP_NPAGED_LOOKASIDE_NUMBER ListType = LookasideSmallIrpList;

    /* Figure out which Lookaside List to use */
    if ((StackSize <= 8) && (ChargeQuota == FALSE))
    {
        /* Set Fixed Size Flag */
        Flags = IRP_ALLOCATED_FIXED_SIZE;

        /* See if we should use big list */
        if (StackSize != 1)
        {
            Size = IoSizeOfIrp(8);
            ListType = LookasideLargeIrpList;
        }

        /* Get the PRCB */
        Prcb = KeGetCurrentPrcb();

        /* Get the P List First */
        List = (PNPAGED_LOOKASIDE_LIST)Prcb->PPLookasideList[ListType].P;

        /* Attempt allocation */
        List->L.TotalAllocates++;
        Irp = (PIRP)InterlockedPopEntrySList(&List->L.ListHead);

        /* Check if the P List failed */
        if (!Irp)
        {
            /* Let the balancer know */
            List->L.AllocateMisses++;

            /* Try the L List */
            List = (PNPAGED_LOOKASIDE_LIST)Prcb->PPLookasideList[ListType].L;
            List->L.TotalAllocates++;
            Irp = (PIRP)InterlockedPopEntrySList(&List->L.ListHead);
        }
    }

    /* Check if we have to use the pool */
    if (!Irp)
    {
        /* Did we try lookaside and fail? */
        if (Flags & IRP_ALLOCATED_FIXED_SIZE) List->L.AllocateMisses++;

        /* Check if we shoudl charge quota */
        if (ChargeQuota)
        {
            /* Irp = ExAllocatePoolWithQuotaTag(NonPagedPool, Size, TAG_IRP); */
            /* FIXME */
            Irp = ExAllocatePoolWithTag(NonPagedPool, Size, TAG_IRP);
        }
        else
        {
            /* Allocate the IRP With no Quota charge */
            Irp = ExAllocatePoolWithTag(NonPagedPool, Size, TAG_IRP);
        }

        /* Make sure it was sucessful */
        if (!Irp) return(NULL);
    }
    else
    {
        /* We have an IRP from Lookaside */
        Flags |= IRP_LOOKASIDE_ALLOCATION;
    }

    /* Set Flag */
    if (ChargeQuota) Flags |= IRP_QUOTA_CHARGED;

    /* Now Initialize it */
    IoInitializeIrp(Irp, Size, StackSize);

    /* Set the Allocation Flags */
    Irp->AllocationFlags = Flags;

    /* Return it */
    return Irp;
}

/*
 * @implemented
 */
PIRP
NTAPI
IoBuildAsynchronousFsdRequest(ULONG MajorFunction,
                              PDEVICE_OBJECT DeviceObject,
                              PVOID Buffer,
                              ULONG Length,
                              PLARGE_INTEGER StartingOffset,
                              PIO_STATUS_BLOCK IoStatusBlock)
{
    PIRP Irp;
    PIO_STACK_LOCATION StackPtr;
    LOCK_OPERATION AccessType;

    /* Allocate IRP */
    if (!(Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE))) return Irp;

    /* Get the Stack */
    StackPtr = IoGetNextIrpStackLocation(Irp);

    /* Write the Major function and then deal with it */
    StackPtr->MajorFunction = (UCHAR)MajorFunction;

    /* Do not handle the following here */
    if (MajorFunction != IRP_MJ_FLUSH_BUFFERS &&
        MajorFunction != IRP_MJ_SHUTDOWN &&
        MajorFunction != IRP_MJ_PNP)
    {
        /* Check if this is Buffered IO */
        if (DeviceObject->Flags & DO_BUFFERED_IO)
        {
            /* Allocate the System Buffer */
            Irp->AssociatedIrp.SystemBuffer =
                ExAllocatePoolWithTag(NonPagedPool, Length, TAG_SYS_BUF);

            /* Set flags */
            Irp->Flags = IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER;

            /* Handle special IRP_MJ_WRITE Case */
            if (MajorFunction == IRP_MJ_WRITE)
            {
                /* Copy the buffer data */
                RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer, Buffer, Length);
            }
            else
            {
                /* Set the Input Operation flag and set this as a User Buffer */
                Irp->Flags |= IRP_INPUT_OPERATION;
                Irp->UserBuffer = Buffer;
            }
        }
        else if (DeviceObject->Flags & DO_DIRECT_IO)
        {
            /* Use an MDL for Direct I/O */
            Irp->MdlAddress = MmCreateMdl(NULL, Buffer, Length);

            if (MajorFunction == IRP_MJ_READ)
            {
                 AccessType = IoWriteAccess;
            }
            else
            {
                 AccessType = IoReadAccess;
            }

            /* Probe and Lock */
            _SEH_TRY
            {
                /* Do the probe */
                MmProbeAndLockPages(Irp->MdlAddress, KernelMode, AccessType);
            }
            _SEH_HANDLE
            {
                /* Free the IRP and its MDL */
                IoFreeMdl(Irp->MdlAddress);
                IoFreeIrp(Irp);
                /* FIXME - pass the exception to the caller? */
                Irp = NULL;
            }
            _SEH_END;

            if (!Irp)
                return NULL;
        }
        else
        {
            /* Neither, use the buffer */
            Irp->UserBuffer = Buffer;
        }

        if (MajorFunction == IRP_MJ_READ)
        {
            StackPtr->Parameters.Read.Length = Length;
            StackPtr->Parameters.Read.ByteOffset = *StartingOffset;
        }
        else if (MajorFunction == IRP_MJ_WRITE)
        {
            StackPtr->Parameters.Write.Length = Length;
            StackPtr->Parameters.Write.ByteOffset = *StartingOffset;
        }
    }

    /* Set the Current Thread and IOSB */
    Irp->UserIosb = IoStatusBlock;
    Irp->Tail.Overlay.Thread = PsGetCurrentThread();

    /* Set the Status Block after all is done */
    return Irp;
}

/*
 * @implemented
 */
PIRP
NTAPI
IoBuildDeviceIoControlRequest (ULONG IoControlCode,
                              PDEVICE_OBJECT DeviceObject,
                              PVOID InputBuffer,
                              ULONG InputBufferLength,
                              PVOID OutputBuffer,
                              ULONG OutputBufferLength,
                              BOOLEAN InternalDeviceIoControl,
                              PKEVENT Event,
                              PIO_STATUS_BLOCK IoStatusBlock)
{
    PIRP Irp;
    PIO_STACK_LOCATION StackPtr;
    ULONG BufferLength;
    LOCK_OPERATION AccessType;

    /* Allocate IRP */
    if (!(Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE))) return Irp;

    /* Get the Stack */
    StackPtr = IoGetNextIrpStackLocation(Irp);

    /* Set the DevCtl Type */
    StackPtr->MajorFunction = InternalDeviceIoControl ?
                              IRP_MJ_INTERNAL_DEVICE_CONTROL :
                              IRP_MJ_DEVICE_CONTROL;

    /* Set the IOCTL Data */
    StackPtr->Parameters.DeviceIoControl.IoControlCode = IoControlCode;
    StackPtr->Parameters.DeviceIoControl.InputBufferLength = InputBufferLength;
    StackPtr->Parameters.DeviceIoControl.OutputBufferLength =
        OutputBufferLength;

    /* Handle the Methods */
    switch (IO_METHOD_FROM_CTL_CODE(IoControlCode))
    {
        case METHOD_BUFFERED:

        /* Select the right Buffer Length */
        BufferLength = InputBufferLength > OutputBufferLength ?
                       InputBufferLength : OutputBufferLength;

        /* Make sure there is one */
        if (BufferLength)
        {
            /* Allocate the System Buffer */
            Irp->AssociatedIrp.SystemBuffer = 
                ExAllocatePoolWithTag(NonPagedPool, BufferLength, TAG_SYS_BUF);

            /* Fail if we couldn't */
            if (Irp->AssociatedIrp.SystemBuffer == NULL)
            {
                IoFreeIrp(Irp);
                return(NULL);
            }

            /* Check if we got a buffer */
            if (InputBuffer)
            {
                /* Copy into the System Buffer */
                RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer,
                              InputBuffer,
                              InputBufferLength);
            }

            /* Write the flags */
            Irp->Flags = IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER;
            if (OutputBuffer) Irp->Flags |= IRP_INPUT_OPERATION;

            /* Save the Buffer */
            Irp->UserBuffer = OutputBuffer;
        }
        else
        {
            /* Clear the Flags and Buffer */
            Irp->Flags = 0;
            Irp->UserBuffer = NULL;
        }
        break;

        case METHOD_IN_DIRECT:
        case METHOD_OUT_DIRECT:

        /* Check if we got an input buffer */
        if (InputBuffer)
        {
            /* Allocate the System Buffer */
            Irp->AssociatedIrp.SystemBuffer =
                ExAllocatePoolWithTag(NonPagedPool,
                                      InputBufferLength,
                                      TAG_SYS_BUF);

            /* Fail if we couldn't */
            if (Irp->AssociatedIrp.SystemBuffer == NULL)
            {
                IoFreeIrp(Irp);
                return(NULL);
            }

            /* Copy into the System Buffer */
            RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer,
                          InputBuffer,
                          InputBufferLength);

            /* Write the flags */
            Irp->Flags = IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER;
        }
        else
        {
            Irp->Flags = 0;
        }

        /* Check if we got an output buffer */
        if (OutputBuffer)
        {
            /* Allocate the System Buffer */
            Irp->MdlAddress = IoAllocateMdl(OutputBuffer,
                                            OutputBufferLength,
                                            FALSE,
                                            FALSE,
                                            Irp);

            /* Fail if we couldn't */
            if (Irp->MdlAddress == NULL)
            {
                IoFreeIrp(Irp);
                return NULL;
            }

            /* Probe and Lock */
            _SEH_TRY
            {
                /* Use the right Access Type */
                if (IO_METHOD_FROM_CTL_CODE(IoControlCode) == METHOD_IN_DIRECT)
                {
                    AccessType = IoReadAccess;
                }
                else
                {
                    AccessType = IoWriteAccess;
                }

                /* Do the probe */
                MmProbeAndLockPages(Irp->MdlAddress, KernelMode, AccessType);
            }
            _SEH_HANDLE
            {
                /* Free the MDL and IRP */
                IoFreeMdl(Irp->MdlAddress);
                IoFreeIrp(Irp);
                /* FIXME - pass the exception to the caller? */
                Irp = NULL;
            }
            _SEH_END;

            if (!Irp)
                return NULL;
        }
        break;

        case METHOD_NEITHER:

        /* Just save the Buffer */
        Irp->UserBuffer = OutputBuffer;
        StackPtr->Parameters.DeviceIoControl.Type3InputBuffer = InputBuffer;
        break;
    }

    /* Now write the Event and IoSB */
    Irp->UserIosb = IoStatusBlock;
    Irp->UserEvent = Event;

    /* Sync IRPs are queued to requestor thread's irp cancel/cleanup list */
    Irp->Tail.Overlay.Thread = PsGetCurrentThread();
    IoQueueThreadIrp(Irp);

    /* Return the IRP */
    return Irp;
}

/*
 * @implemented
 */
PIRP
NTAPI
IoBuildSynchronousFsdRequest(ULONG MajorFunction,
                             PDEVICE_OBJECT DeviceObject,
                             PVOID Buffer,
                             ULONG Length,
                             PLARGE_INTEGER StartingOffset,
                             PKEVENT Event,
                             PIO_STATUS_BLOCK IoStatusBlock)
{
    PIRP Irp;

    /* Do the big work to set up the IRP */
    Irp = IoBuildAsynchronousFsdRequest(MajorFunction,
                                        DeviceObject,
                                        Buffer,
                                        Length,
                                        StartingOffset,
                                        IoStatusBlock );

    /* Set the Event which makes it Syncronous */
    Irp->UserEvent = Event;

    /* Sync IRPs are queued to requestor thread's irp cancel/cleanup list */
    IoQueueThreadIrp(Irp);
    return(Irp);
}

/*
 * @implemented
 */
BOOLEAN
NTAPI
IoCancelIrp(PIRP Irp)
{
   KIRQL oldlvl;
   PDRIVER_CANCEL CancelRoutine;

   DPRINT("IoCancelIrp(Irp 0x%p)\n",Irp);

   IoAcquireCancelSpinLock(&oldlvl);

   Irp->Cancel = TRUE;

   CancelRoutine = IoSetCancelRoutine(Irp, NULL);
   if (CancelRoutine == NULL)
   {
      IoReleaseCancelSpinLock(oldlvl);
      return(FALSE);
   }

   Irp->CancelIrql = oldlvl;
   CancelRoutine(IoGetCurrentIrpStackLocation(Irp)->DeviceObject, Irp);
   return(TRUE);
}

/*
 * @name IoCancelThreadIo
 */
VOID
NTAPI
IoCancelThreadIo(PETHREAD Thread)
{
    PIRP Irp;
    KIRQL OldIrql;
    ULONG Retries = 3000;
    LARGE_INTEGER Interval;

    /* Raise to APC to protect the IrpList */
    OldIrql = KfRaiseIrql(APC_LEVEL);

    /* Start by cancelling all the IRPs in the current thread queue. */
    LIST_FOR_EACH(Irp, &Thread->IrpList, IRP, ThreadListEntry)
    {
        /* Cancel it */
        IoCancelIrp(Irp);
    }

     /* Wait 100 milliseconds */
    Interval.QuadPart = -1000000;

    /* Wait till all the IRPs are completed or cancelled. */
    while (!IsListEmpty(&Thread->IrpList))
    {
        /* Now we can lower */
        KfLowerIrql(OldIrql);

        /* Wait a short while and then look if all our IRPs were completed. */
        KeDelayExecutionThread(KernelMode, FALSE, &Interval);

        /*
         * Don't stay here forever if some broken driver doesn't complete
         * the IRP.
         */
        if (Retries-- == 0) IopRemoveThreadIrp();

        /* Raise the IRQL Again */
        OldIrql = KfRaiseIrql(APC_LEVEL);
    }
    
    /* We're done, lower the IRQL */
    KfLowerIrql(OldIrql);
}

#ifdef IoCallDriver
#undef IoCallDriver
#endif
/*
 * @implemented
 */
NTSTATUS
NTAPI
IoCallDriver(PDEVICE_OBJECT DeviceObject,
             PIRP Irp)
{
    /* Call fast call */
    return IofCallDriver(DeviceObject, Irp);
}

/*
 * @implemented
 */
#undef IoCompleteRequest
VOID
NTAPI
IoCompleteRequest(PIRP Irp,
                  CCHAR PriorityBoost)
{
    /* Call the fastcall */
    IofCompleteRequest(Irp, PriorityBoost);
}

/*
 * @implemented
 */
VOID
NTAPI
IoEnqueueIrp(IN PIRP Irp)
{
    IoQueueThreadIrp(Irp);
}

/*
 * @implemented
 */
NTSTATUS
FASTCALL
IofCallDriver(PDEVICE_OBJECT DeviceObject,
              PIRP Irp)
{
    PDRIVER_OBJECT DriverObject;
    PIO_STACK_LOCATION Param;

    /* Get the Driver Object */
    DriverObject = DeviceObject->DriverObject;

    /* Set the Stack Location */
    IoSetNextIrpStackLocation(Irp);

    /* Get the current one */
    Param = IoGetCurrentIrpStackLocation(Irp);

    /* Get the Device Object */
    Param->DeviceObject = DeviceObject;

    /* Call it */
    return DriverObject->MajorFunction[Param->MajorFunction](DeviceObject,
                                                             Irp);
}

#ifdef IoCompleteRequest
#undef IoCompleteRequest
#endif
/*
 * @implemented
 */
VOID
FASTCALL
IofCompleteRequest(PIRP Irp,
                   CCHAR PriorityBoost)
{
    PIO_STACK_LOCATION StackPtr;
    PDEVICE_OBJECT DeviceObject;
    PFILE_OBJECT FileObject = Irp->Tail.Overlay.OriginalFileObject;
    PETHREAD Thread = Irp->Tail.Overlay.Thread;
    NTSTATUS Status;
    PMDL Mdl;

    ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    ASSERT(!Irp->CancelRoutine);
    ASSERT(Irp->IoStatus.Status != STATUS_PENDING);

    /* Get the Current Stack */
    StackPtr = IoGetCurrentIrpStackLocation(Irp);
    IoSkipCurrentIrpStackLocation(Irp);

    /* Loop the Stacks and complete the IRPs */
    for (;Irp->CurrentLocation <= (Irp->StackCount + 1); StackPtr++)
    {
        /* Set Pending Returned */
        Irp->PendingReturned = StackPtr->Control & SL_PENDING_RETURNED;

        /*
         * Completion routines expect the current irp stack location to be the same as when
         * IoSetCompletionRoutine was called to set them. A side effect is that completion
         * routines set by highest level drivers without their own stack location will receive
         * an invalid current stack location (at least it should be considered as invalid).
         * Since the DeviceObject argument passed is taken from the current stack, this value
         * is also invalid (NULL).
         */
        if (Irp->CurrentLocation == (Irp->StackCount + 1))
        {
            DeviceObject = NULL;
        }
        else
        {
            DeviceObject = IoGetCurrentIrpStackLocation(Irp)->DeviceObject;
        }

        /* Check if there is a Completion Routine to Call */
        if ((NT_SUCCESS(Irp->IoStatus.Status) &&
             (StackPtr->Control & SL_INVOKE_ON_SUCCESS)) ||
            (!NT_SUCCESS(Irp->IoStatus.Status) &&
             (StackPtr->Control & SL_INVOKE_ON_ERROR)) ||
            (Irp->Cancel && (StackPtr->Control & SL_INVOKE_ON_CANCEL)))
        {
            /* Call it */
            Status = StackPtr->CompletionRoutine(DeviceObject,
                                                 Irp,
                                                 StackPtr->Context);

            /* Don't touch the Packet if this was returned. It might be gone! */
            if (Status == STATUS_MORE_PROCESSING_REQUIRED) return;
        }
        else
        {
            if ((Irp->CurrentLocation <= Irp->StackCount) && (Irp->PendingReturned))
            {
                IoMarkIrpPending(Irp);
            }
        }

        /* Move to next stack */
        IoSkipCurrentIrpStackLocation(Irp);
    }

    /* Windows NT File System Internals, page 165 */
    if (Irp->Flags & IRP_ASSOCIATED_IRP)
    {
        ULONG MasterIrpCount;
        PIRP MasterIrp = Irp->AssociatedIrp.MasterIrp;

        /* This should never happen! */
        ASSERT(IsListEmpty(&Irp->ThreadListEntry));

        /* Decrement and get the old count */
        MasterIrpCount = InterlockedDecrement(&MasterIrp->AssociatedIrp.IrpCount);

        /* Free MDLs and IRP */
        while ((Mdl = Irp->MdlAddress))
        {
            Irp->MdlAddress = Mdl->Next;
            IoFreeMdl(Mdl);
        }
        IoFreeIrp(Irp);

        /* Complete the Master IRP */
        if (!MasterIrpCount) IofCompleteRequest(MasterIrp, PriorityBoost);
        return;
    }

    /* Windows NT File System Internals, page 165 */
    if (Irp->Flags & (IRP_PAGING_IO|IRP_CLOSE_OPERATION))
    {
        /* This should never happen! */
        ASSERT(IsListEmpty(&Irp->ThreadListEntry));

        /* Handle a Close Operation or Sync Paging I/O (see page 165) */
        if (Irp->Flags & (IRP_SYNCHRONOUS_PAGING_IO | IRP_CLOSE_OPERATION))
        {
            /* Set the I/O Status and Signal the Event */
            *Irp->UserIosb = Irp->IoStatus;
            KeSetEvent(Irp->UserEvent, PriorityBoost, FALSE);

            /* Free the IRP for a Paging I/O Only, Close is handled by us */
            if (Irp->Flags & IRP_SYNCHRONOUS_PAGING_IO)
            {
                IoFreeIrp(Irp);
            }
        }
        else
        {
#if 0
            /* Page 166 */
            KeInitializeApc(&Irp->Tail.Apc
                            &Irp->Tail.Overlay.Thread->Tcb,
                            Irp->ApcEnvironment,
                            IopCompletePageWrite,
                            NULL,
                            NULL,
                            KernelMode,
                            NULL);
            KeInsertQueueApc(&Irp->Tail.Apc,
                             NULL,
                             NULL,
                             PriorityBoost);
#else
            /* Not implemented yet. */
            ASSERT(FALSE);
#endif
        }
        return;
    }

    /* Unlock MDL Pages, page 167. */
    Mdl = Irp->MdlAddress;
    while (Mdl)
    {
        MmUnlockPages(Mdl);
        Mdl = Mdl->Next;
    }

    /* Check if we should exit because of a Deferred I/O (page 168) */
    if (Irp->Flags & IRP_DEFER_IO_COMPLETION && !Irp->PendingReturned)
    {
        return;
    }

    /* Now queue the special APC */
    if (!Irp->Cancel)
    {
        KeInitializeApc(&Irp->Tail.Apc,
                        &Thread->Tcb,
                        Irp->ApcEnvironment,
                        IopCompleteRequest,
                        NULL,
                        (PKNORMAL_ROUTINE) NULL,
                        KernelMode,
                        NULL);
        KeInsertQueueApc(&Irp->Tail.Apc,
                         FileObject,
                         NULL, /* This is used for REPARSE stuff */
                         PriorityBoost);
    }
    else
    {
        /* The IRP just got cancelled... does a thread still own it? */
        if ((Thread = Irp->Tail.Overlay.Thread))
        {
            /* Yes! There is still hope! */
            KeInitializeApc(&Irp->Tail.Apc,
                            &Thread->Tcb,
                            Irp->ApcEnvironment,
                            IopCompleteRequest,
                            NULL,
                            (PKNORMAL_ROUTINE) NULL,
                            KernelMode,
                            NULL);
            KeInsertQueueApc(&Irp->Tail.Apc,
                             FileObject,
                             NULL, /* This is used for REPARSE stuff */
                             PriorityBoost);
        }
        else
        {
            /* Nothing left for us to do, kill it */
            IopCleanupIrp(Irp, FileObject);
        }
    }
}

/*
 * @unimplemented
 */
BOOLEAN
NTAPI
IoForwardIrpSynchronously(IN PDEVICE_OBJECT DeviceObject,
                          IN PIRP Irp)
{
    UNIMPLEMENTED;
    return FALSE;
}

/*
 * @implemented
 */
VOID
NTAPI
IoFreeIrp(PIRP Irp)
{
    PNPAGED_LOOKASIDE_LIST List;
    PP_NPAGED_LOOKASIDE_NUMBER ListType =  LookasideSmallIrpList;
    PKPRCB Prcb;
    
    /* If this was a pool alloc, free it with the pool */
    if (!(Irp->AllocationFlags & IRP_ALLOCATED_FIXED_SIZE))
    {
        /* Free it */
        ExFreePool(Irp);
    }
    else
    {
        /* Check if this was a Big IRP */
        if (Irp->StackCount != 1)
        {
            ListType = LookasideLargeIrpList;
        }

        /* Get the PRCB */
        Prcb = KeGetCurrentPrcb();

        /* Use the P List */
        List = (PNPAGED_LOOKASIDE_LIST)Prcb->PPLookasideList[ListType].P;
        List->L.TotalFrees++;

        /* Check if the Free was within the Depth or not */
        if (ExQueryDepthSList(&List->L.ListHead) >= List->L.Depth)
        {
            /* Let the balancer know */
            List->L.FreeMisses++;

            /* Use the L List */
            List = (PNPAGED_LOOKASIDE_LIST)Prcb->PPLookasideList[ListType].L;
            List->L.TotalFrees++;

            /* Check if the Free was within the Depth or not */
            if (ExQueryDepthSList(&List->L.ListHead) >= List->L.Depth)
            {
                /* All lists failed, use the pool */
                List->L.FreeMisses++;
                ExFreePool(Irp);
                Irp = NULL;
            }
        }

        /* The free was within dhe Depth */
        if (Irp)
        {
           InterlockedPushEntrySList(&List->L.ListHead,
                                     (PSINGLE_LIST_ENTRY)Irp);
        }
    }
}

/*
 * @implemented
 */
PEPROCESS NTAPI
IoGetRequestorProcess(IN PIRP Irp)
{
    return(Irp->Tail.Overlay.Thread->ThreadsProcess);
}

/*
 * @implemented
 */
ULONG
NTAPI
IoGetRequestorProcessId(IN PIRP Irp)
{
    return (ULONG)(IoGetRequestorProcess(Irp)->UniqueProcessId);
}

/*
 * @implemented
 */
NTSTATUS
NTAPI
IoGetRequestorSessionId(IN PIRP Irp,
                        OUT PULONG pSessionId)
{
    *pSessionId = IoGetRequestorProcess(Irp)->Session;

    return STATUS_SUCCESS;
}

/*
 * @implemented
 */
PIRP
NTAPI
IoGetTopLevelIrp(VOID)
{
    return (PIRP)PsGetCurrentThread()->TopLevelIrp;
}

/*
 * @implemented
 */
VOID
NTAPI
IoInitializeIrp(PIRP Irp,
                USHORT PacketSize,
                CCHAR StackSize)
{
    ASSERT(Irp != NULL);

    /* Clear it */
    RtlZeroMemory(Irp, PacketSize);

    /* Set the Header and other data */
    Irp->Type = IO_TYPE_IRP;
    Irp->Size = PacketSize;
    Irp->StackCount = StackSize;
    Irp->CurrentLocation = StackSize + 1;
    Irp->ApcEnvironment =  KeGetCurrentThread()->ApcStateIndex;
    Irp->Tail.Overlay.CurrentStackLocation = (PIO_STACK_LOCATION)(Irp + 1) + StackSize;

    /* Initialize the Thread List */
    InitializeListHead(&Irp->ThreadListEntry);
}

/*
 * @implemented
 */
BOOLEAN
NTAPI
IoIsOperationSynchronous(IN PIRP Irp)
{
    /* Check the flags */
    if ((Irp->Flags & IRP_SYNCHRONOUS_PAGING_IO) ||
        (Irp->Flags & IRP_SYNCHRONOUS_API) ||
        (IoGetCurrentIrpStackLocation(Irp)->FileObject->Flags &
        FO_SYNCHRONOUS_IO))
    {
        /* Synch API or Paging I/O is OK, as is Sync File I/O */
        return TRUE;
    }

    /* Otherwise, it is an asynchronous operation. */
    return FALSE;
}

/*
 * @unimplemented
 */
BOOLEAN
NTAPI
IoIsValidNameGraftingBuffer(IN PIRP Irp,
                            IN PREPARSE_DATA_BUFFER ReparseBuffer)
{
    UNIMPLEMENTED;
    return FALSE;
}

/*
 * @implemented
 */
PIRP
NTAPI
IoMakeAssociatedIrp(PIRP Irp,
                    CCHAR StackSize)
{
   PIRP AssocIrp;

   /* Allocate the IRP */
   AssocIrp = IoAllocateIrp(StackSize, FALSE);
   if (AssocIrp == NULL) return NULL;

   /* Set the Flags */
   AssocIrp->Flags |= IRP_ASSOCIATED_IRP;

   /* Set the Thread */
   AssocIrp->Tail.Overlay.Thread = Irp->Tail.Overlay.Thread;

   /* Associate them */
   AssocIrp->AssociatedIrp.MasterIrp = Irp;

   return AssocIrp;
}

/*
 * @implemented
 */
NTSTATUS
NTAPI
IoPageRead(PFILE_OBJECT FileObject,
           PMDL Mdl,
           PLARGE_INTEGER Offset,
           PKEVENT Event,
           PIO_STATUS_BLOCK StatusBlock)
{
    PIRP Irp;
    PIO_STACK_LOCATION StackPtr;
    PDEVICE_OBJECT DeviceObject;

    /* Get the Device Object */
    DeviceObject = IoGetRelatedDeviceObject(FileObject);

    /* Allocate IRP */
    Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);

    /* Get the Stack */
    StackPtr = IoGetNextIrpStackLocation(Irp);

    /* Create the IRP Settings */
    Irp->MdlAddress = Mdl;
    Irp->UserBuffer = MmGetMdlVirtualAddress(Mdl);
    Irp->UserIosb = StatusBlock;
    Irp->UserEvent = Event;
    Irp->RequestorMode = KernelMode;
    Irp->Flags = IRP_PAGING_IO |
                 IRP_NOCACHE |
                 IRP_SYNCHRONOUS_PAGING_IO |
                 IRP_INPUT_OPERATION;
    Irp->Tail.Overlay.OriginalFileObject = FileObject;
    Irp->Tail.Overlay.Thread = PsGetCurrentThread();

    /* Set the Stack Settings */
    StackPtr->Parameters.Read.Length = MmGetMdlByteCount(Mdl);
    StackPtr->Parameters.Read.ByteOffset = *Offset;
    StackPtr->MajorFunction = IRP_MJ_READ;
    StackPtr->FileObject = FileObject;

    /* Call the Driver */
    return IofCallDriver(DeviceObject, Irp);
}

/*
 * @implemented
 */
VOID
NTAPI
IoQueueThreadIrp(IN PIRP Irp)
{
    KIRQL OldIrql;

    /* Raise to APC */
    OldIrql = KfRaiseIrql(APC_LEVEL);

   /*
    * Synchronous irp's are queued to requestor thread. If they are not
    * completed when the thread exits, they are canceled (cleaned up).
    * - Gunnar
    */
    InsertTailList(&Irp->Tail.Overlay.Thread->IrpList, &Irp->ThreadListEntry);

    /* Lower back */
    KfLowerIrql(OldIrql);
}

/*
 * @implemented
 * Reference: Chris Cant's "Writing WDM Device Drivers"
 */
VOID
NTAPI
IoReuseIrp(IN OUT PIRP Irp,
           IN NTSTATUS Status)
{
    UCHAR AllocationFlags;

    /* Get the old flags */
    AllocationFlags = Irp->AllocationFlags;

    /* Reinitialize the IRP */
    IoInitializeIrp(Irp, Irp->Size, Irp->StackCount);

    /* Duplicate the data */
    Irp->IoStatus.Status = Status;
    Irp->AllocationFlags = AllocationFlags;
}

/*
 * @implemented
 */
VOID
NTAPI
IoSetTopLevelIrp(IN PIRP Irp)
{
    PsGetCurrentThread()->TopLevelIrp = (ULONG)Irp;
}

/*
 * @implemented
 */
NTSTATUS
NTAPI
IoSynchronousPageWrite(PFILE_OBJECT FileObject,
                       PMDL Mdl,
                       PLARGE_INTEGER Offset,
                       PKEVENT Event,
                       PIO_STATUS_BLOCK StatusBlock)
{
    PIRP Irp;
    PIO_STACK_LOCATION StackPtr;
    PDEVICE_OBJECT DeviceObject;

    /* Get the Device Object */
    DeviceObject = IoGetRelatedDeviceObject(FileObject);

    /* Allocate IRP */
    Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);

    /* Get the Stack */
    StackPtr = IoGetNextIrpStackLocation(Irp);

    /* Create the IRP Settings */
    Irp->MdlAddress = Mdl;
    Irp->UserBuffer = MmGetMdlVirtualAddress(Mdl);
    Irp->UserIosb = StatusBlock;
    Irp->UserEvent = Event;
    Irp->RequestorMode = KernelMode;
    Irp->Flags = IRP_PAGING_IO | IRP_NOCACHE | IRP_SYNCHRONOUS_PAGING_IO;
    Irp->Tail.Overlay.OriginalFileObject = FileObject;
    Irp->Tail.Overlay.Thread = PsGetCurrentThread();

    /* Set the Stack Settings */
    StackPtr->Parameters.Write.Length = MmGetMdlByteCount(Mdl);
    StackPtr->Parameters.Write.ByteOffset = *Offset;
    StackPtr->MajorFunction = IRP_MJ_WRITE;
    StackPtr->FileObject = FileObject;

    /* Call the Driver */
    return IofCallDriver(DeviceObject, Irp);
}

/* EOF */
