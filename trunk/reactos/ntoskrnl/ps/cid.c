/* $Id: cid.c,v 1.2 2004/10/12 20:00:40 navaraf Exp $
 *
 * COPYRIGHT:         See COPYING in the top level directory
 * PROJECT:           ReactOS kernel
 * FILE:              ntoskrnl/ps/clientid.c
 * PURPOSE:           Client ID (CID) management
 * PROGRAMMER:        Thomas Weidenmueller <w3seek@reactos.com>
 * REVISION HISTORY:
 *              9/20/2004: Created
 */

/* INCLUDES ******************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <internal/debug.h>

/* GLOBALS ******************************************************************/

/*
 * FIXME - use a global handle table instead!
 */

KSPIN_LOCK CidLock;
LIST_ENTRY CidHead;
KEVENT CidReleaseEvent;
LONG CidCounter = 0;
LARGE_INTEGER ShortDelay, LongDelay;

#define TAG_CIDOBJECT TAG('C', 'I', 'D', 'O')

/* FUNCTIONS *****************************************************************/

VOID INIT_FUNCTION
PsInitClientIDManagment(VOID)
{
   InitializeListHead(&CidHead);
   KeInitializeSpinLock(&CidLock);
   KeInitializeEvent(&CidReleaseEvent, SynchronizationEvent, FALSE);
   ShortDelay.QuadPart = -100LL;
   LongDelay.QuadPart = -100000LL;
}

VOID
PspReferenceCidObject(PCID_OBJECT Object)
{
  InterlockedIncrement(&Object->ref);
}

VOID
PspDereferenceCidObject(PCID_OBJECT Object)
{
  if(InterlockedDecrement(&Object->ref) == 0)
  {
    ExFreePool(Object);
  }
}

NTSTATUS
PsCreateCidHandle(PVOID Object, POBJECT_TYPE ObjectType, PHANDLE Handle)
{
  KIRQL oldIrql;
  PCID_OBJECT cido = ExAllocatePoolWithTag(NonPagedPool,
                                           sizeof(CID_OBJECT),
                                           TAG_CIDOBJECT);
  if(cido != NULL)
  {
    cido->ref = 1;
    cido->Lock = 0;
    cido->Obj.Object = Object;

    KeAcquireSpinLock(&CidLock, &oldIrql);
    cido->Handle = (HANDLE)(++CidCounter);
    InsertTailList(&CidHead, &cido->Entry);
    KeReleaseSpinLock(&CidLock, oldIrql);

    *Handle = cido->Handle;
    return STATUS_SUCCESS;
  }
  
  return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS
PsDeleteCidHandle(HANDLE CidHandle, POBJECT_TYPE ObjectType)
{
  PCID_OBJECT cido, Found = NULL;
  PLIST_ENTRY Current;
  KIRQL oldIrql;
  
  if(CidHandle == NULL)
  {
    return STATUS_INVALID_PARAMETER;
  }
  
  KeAcquireSpinLock(&CidLock, &oldIrql);
  Current = CidHead.Flink;
  while(Current != &CidHead)
  {
    cido = CONTAINING_RECORD(Current, CID_OBJECT, Entry);
    if(cido->Handle == CidHandle)
    {
      RemoveEntryList(&cido->Entry);
      cido->Handle = NULL;
      Found = cido;
      break;
    }
    Current = Current->Flink;
  }
  KeReleaseSpinLock(&CidLock, oldIrql);

  if(Found != NULL)
  {
    PspDereferenceCidObject(Found);
    return STATUS_SUCCESS;
  }

  return STATUS_UNSUCCESSFUL;
}

PCID_OBJECT
PsLockCidHandle(HANDLE CidHandle, POBJECT_TYPE ObjectType)
{
  PCID_OBJECT cido, Found = NULL;
  PLIST_ENTRY Current;
  KIRQL oldIrql;
  
  if(CidHandle == NULL)
  {
    return NULL;
  }
  
  KeAcquireSpinLock(&CidLock, &oldIrql);
  Current = CidHead.Flink;
  while(Current != &CidHead)
  {
    cido = CONTAINING_RECORD(Current, CID_OBJECT, Entry);
    if(cido->Handle == CidHandle)
    {
      Found = cido;
      PspReferenceCidObject(Found);
      break;
    }
    Current = Current->Flink;
  }
  KeReleaseSpinLock(&CidLock, oldIrql);
  
  if(Found != NULL)
  {
    ULONG Attempt = 0;
    for (;;)
    {
      if(InterlockedCompareExchange(&Found->Lock, 1, 0) == 0)
      {
        /* got the lock, bail */
        break;
      }

      if(Attempt++ >= 1)
      {
        /* wait a little longer */
        KeDelayExecutionThread(KernelMode, FALSE, &LongDelay);
      }
      else
      {
        /* try again, just wait shortly */
        KeDelayExecutionThread(KernelMode, FALSE, &ShortDelay);
      }
    }
  }
  
  return Found;
}

VOID
PsUnlockCidObject(PCID_OBJECT CidObject)
{
  InterlockedExchange(&CidObject->Lock, 0);
  PspDereferenceCidObject(CidObject);
}

/* EOF */
