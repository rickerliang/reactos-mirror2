/* $Id: pager.c,v 1.2 2000/07/04 08:52:45 dwelch Exp $
 *
 * COPYRIGHT:    See COPYING in the top level directory
 * PROJECT:      ReactOS kernel
 * FILE:         ntoskrnl/mm/pager.c
 * PURPOSE:      Moves infrequently used data out of memory
 * PROGRAMMER:   David Welch (welch@cwcom.net)
 * UPDATE HISTORY: 
 *               27/05/98: Created
 */

/* INCLUDES ****************************************************************/

#include <ddk/ntddk.h>
#include <internal/ps.h>
#include <internal/mm.h>
#include <internal/mmhal.h>
#include <string.h>
#include <internal/string.h>

#define NDEBUG
#include <internal/debug.h>

/* GLOBALS *******************************************************************/

static HANDLE PagerThreadHandle;
static CLIENT_ID PagerThreadId;
static KEVENT PagerThreadEvent;
static PEPROCESS LastProcess;
static volatile BOOLEAN PagerThreadShouldTerminate;
static volatile ULONG PageCount;

/* FUNCTIONS *****************************************************************/

VOID MmTryPageOutFromProcess(PEPROCESS Process)
{
   MmLockAddressSpace(&Process->AddressSpace);
   PageCount = PageCount - MmTrimWorkingSet(Process, PageCount);
   MmUnlockAddressSpace(&Process->AddressSpace);
}

NTSTATUS MmPagerThreadMain(PVOID Ignored)
{
   NTSTATUS Status;
      
   for(;;)
     {
	Status = KeWaitForSingleObject(&PagerThreadEvent,
				       0,
				       KernelMode,
				       FALSE,
				       NULL);
	if (!NT_SUCCESS(Status))
	  {
	     DbgPrint("PagerThread: Wait failed\n");
	     KeBugCheck(0);
	  }
	if (PagerThreadShouldTerminate)
	  {
	     DbgPrint("PagerThread: Terminating\n");
	     return(STATUS_SUCCESS);
	  }
	
	while (PageCount > 0)
	  {
	     KeAttachProcess(LastProcess);
	     MmTryPageOutFromProcess(LastProcess);
	     KeDetachProcess();
	     if (PageCount != 0)
	       {
		  LastProcess = PsGetNextProcess(LastProcess);
	       }
	  }
     }
}

NTSTATUS MmInitPagerThread(VOID)
{
   NTSTATUS Status;
   
   PageCount = 0;
   LastProcess = PsInitialSystemProcess;
   PagerThreadShouldTerminate = FALSE;
   KeInitializeEvent(&PagerThreadEvent,
		     SynchronizationEvent,
		     FALSE);
   
   Status = PsCreateSystemThread(&PagerThreadHandle,
				 THREAD_ALL_ACCESS,
				 NULL,
				 NULL,
				 &PagerThreadId,
				 MmPagerThreadMain,
				 NULL);
   if (!NT_SUCCESS(Status))
     {
	return(Status);
     }
   
   return(STATUS_SUCCESS);
}
