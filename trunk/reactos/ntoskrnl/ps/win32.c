/* $Id$
 *
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS kernel
 * FILE:            ntoskrnl/ps/win32.c
 * PURPOSE:         win32k support
 *
 * PROGRAMMERS:     Eric Kohl (ekohl@rz-online.de)
 */

/* INCLUDES ****************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <internal/debug.h>

/* GLOBALS ******************************************************************/

static PKWIN32_PROCESS_CALLOUT PspWin32ProcessCallback = NULL;
static PKWIN32_THREAD_CALLOUT PspWin32ThreadCallback = NULL;
extern PKWIN32_PARSEMETHOD_CALLOUT ExpWindowStationObjectParse;
extern PKWIN32_DELETEMETHOD_CALLOUT ExpWindowStationObjectDelete;
extern PKWIN32_DELETEMETHOD_CALLOUT ExpDesktopObjectDelete;

#ifndef ALEX_CB_REWRITE
typedef struct _NTW32CALL_SAVED_STATE
{
  ULONG_PTR SavedStackLimit;
  PVOID SavedStackBase;
  PVOID SavedInitialStack;
  PVOID CallerResult;
  PULONG CallerResultLength;
  PNTSTATUS CallbackStatus;
  PKTRAP_FRAME SavedTrapFrame;
  PVOID SavedCallbackStack;
  PVOID SavedExceptionStack;
} NTW32CALL_SAVED_STATE, *PNTW32CALL_SAVED_STATE;
#endif

PVOID
STDCALL
KeSwitchKernelStack(
    IN PVOID StackBase,
    IN PVOID StackLimit
);

/* FUNCTIONS ***************************************************************/

/*
 * @implemented
 */
VOID 
STDCALL
PsEstablishWin32Callouts(PWIN32_CALLOUTS_FPNS CalloutData)
{
    PspWin32ProcessCallback = CalloutData->ProcessCallout;
    PspWin32ThreadCallback = CalloutData->ThreadCallout;
    ExpWindowStationObjectParse = CalloutData->WindowStationParseProcedure;
    ExpWindowStationObjectDelete = CalloutData->WindowStationDeleteProcedure;
    ExpDesktopObjectDelete = CalloutData->DesktopDeleteProcedure;
}

NTSTATUS
NTAPI
PsConvertToGuiThread(VOID)
{
    ULONG_PTR NewStack;
    PVOID OldStack;
    PETHREAD Thread = PsGetCurrentThread();
    PEPROCESS Process = PsGetCurrentProcess();
    NTSTATUS Status;
    PAGED_CODE();

    /* Validate the previous mode */
    if (KeGetPreviousMode() == KernelMode)
    {
        DPRINT1("Danger: win32k call being made in kernel-mode?!\n");
        return STATUS_INVALID_PARAMETER;
    }

    /* Make sure win32k is here */
    if (!PspWin32ProcessCallback)
    {
        DPRINT1("Danger: Win32K call attempted but Win32k not ready!\n");
        return STATUS_ACCESS_DENIED;
    }

    /* Make sure it's not already win32 */
    if (Thread->Tcb.ServiceTable != KeServiceDescriptorTable)
    {
        DPRINT1("Danger: Thread is already a win32 thread. Limit bypassed?\n");
        return STATUS_ALREADY_WIN32;
    }

    /* Check if we don't already have a kernel-mode stack */
    if (!Thread->Tcb.LargeStack)
    {
        /* We don't create one */
        NewStack = (ULONG_PTR)MmCreateKernelStack(TRUE) + KERNEL_LARGE_STACK_SIZE;
        if (!NewStack)
        {
            /* Panic in user-mode */
            NtCurrentTeb()->LastErrorValue = ERROR_NOT_ENOUGH_MEMORY;
            return STATUS_NO_MEMORY;
        }

        /* We're about to switch stacks. Enter a critical region */
        KeEnterCriticalRegion();

        /* Switch stacks */
        OldStack = KeSwitchKernelStack((PVOID)NewStack,
                                       (PVOID)(NewStack - KERNEL_STACK_SIZE));

        /* Leave the critical region */
        KeLeaveCriticalRegion();

        /* Delete the old stack */
        MmDeleteKernelStack(OldStack, FALSE);
    }

    /* This check is bizare. Check out win32k later */
    if (!Process->Win32Process)
    {
        /* Now tell win32k about us */
        Status = PspWin32ProcessCallback(Process, TRUE);
        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Danger: Win32k wasn't happy about us!\n");
            return Status;
        }
    }

    /* Set the new service table */
    Thread->Tcb.ServiceTable = KeServiceDescriptorTableShadow;
    ASSERT(Thread->Tcb.Win32Thread == 0);

    /* Tell Win32k about our thread */
    Status = PspWin32ThreadCallback(Thread, PsW32ThreadCalloutInitialize);
    if (!NT_SUCCESS(Status))
    {
        /* Revert our table */
        DPRINT1("Danger: Win32k wasn't happy about us!\n");
        Thread->Tcb.ServiceTable = KeServiceDescriptorTable;
    }

    /* Return status */
    return Status;
}

VOID
NTAPI
PsTerminateWin32Process (PEPROCESS Process)
{
  if (Process->Win32Process == NULL)
    return;

  if (PspWin32ProcessCallback != NULL)
    {
      PspWin32ProcessCallback (Process, FALSE);
    }

  /* don't delete the W32PROCESS structure at this point, wait until the
     EPROCESS structure is being freed */
}


VOID
NTAPI
PsTerminateWin32Thread (PETHREAD Thread)
{
  if (Thread->Tcb.Win32Thread != NULL)
  {
    if (PspWin32ThreadCallback != NULL)
    {
      PspWin32ThreadCallback (Thread, PsW32ThreadCalloutExit);
    }

    /* don't delete the W32THREAD structure at this point, wait until the
       ETHREAD structure is being freed */
  }
}

NTSTATUS
STDCALL
NtW32Call(IN ULONG RoutineIndex,
          IN PVOID Argument,
          IN ULONG ArgumentLength,
          OUT PVOID* Result,
          OUT PULONG ResultLength)
{
    PVOID RetResult;
    ULONG RetResultLength;
    NTSTATUS Status = STATUS_SUCCESS;

    DPRINT("NtW32Call(RoutineIndex %d, Argument %p, ArgumentLength %d)\n",
            RoutineIndex, Argument, ArgumentLength);

    /* must not be called as KernelMode! */
    ASSERT(KeGetPreviousMode() != KernelMode);

    _SEH_TRY
    {
        ProbeForWritePointer(Result);
        ProbeForWriteUlong(ResultLength);
    }
    _SEH_HANDLE
    {
        Status = _SEH_GetExceptionCode();
    }
    _SEH_END;

    if (NT_SUCCESS(Status))
    {
        /* Call kernel function */
        Status = KeUserModeCallback(RoutineIndex,
                                    Argument,
                                    ArgumentLength,
                                    &RetResult,
                                    &RetResultLength);

        if (NT_SUCCESS(Status))
        {
            _SEH_TRY
            {
                *Result = RetResult;
                *ResultLength = RetResultLength;
            }
            _SEH_HANDLE
            {
                Status = _SEH_GetExceptionCode();
            }
            _SEH_END;
        }
    }

    /* Return the result */
    return Status;
}

/* EOF */
