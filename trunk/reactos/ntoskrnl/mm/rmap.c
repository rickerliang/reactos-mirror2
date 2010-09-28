/*
 * COPYRIGHT:       See COPYING in the top directory
 * PROJECT:         ReactOS kernel
 * FILE:            ntoskrnl/mm/rmap.c
 * PURPOSE:         Kernel memory managment functions
 *
 * PROGRAMMERS:     David Welch (welch@cwcom.net)
 */

/* INCLUDES *****************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <debug.h>

#if defined (ALLOC_PRAGMA)
#pragma alloc_text(INIT, MmInitializeRmapList)
#endif

/* TYPES ********************************************************************/

typedef struct _MM_RMAP_ENTRY
{
   struct _MM_RMAP_ENTRY* Next;
   PEPROCESS Process;
   PVOID Address;
#if DBG
   PVOID Caller;
#endif
}
MM_RMAP_ENTRY, *PMM_RMAP_ENTRY;

/* GLOBALS ******************************************************************/

static FAST_MUTEX RmapListLock;
static NPAGED_LOOKASIDE_LIST RmapLookasideList;

/* FUNCTIONS ****************************************************************/

VOID
INIT_FUNCTION
NTAPI
MmInitializeRmapList(VOID)
{
   ExInitializeFastMutex(&RmapListLock);
   ExInitializeNPagedLookasideList (&RmapLookasideList,
                                    NULL,
                                    NULL,
                                    0,
                                    sizeof(MM_RMAP_ENTRY),
                                    TAG_RMAP,
                                    50);
}

NTSTATUS
NTAPI
MmPageOutPhysicalAddress(PFN_NUMBER Page)
{
   PMM_RMAP_ENTRY entry;
   PMEMORY_AREA MemoryArea;
   PMMSUPPORT AddressSpace;
   ULONG Type;
   PVOID Address;
   PEPROCESS Process;
   PMM_PAGEOP PageOp;
   ULONG Offset;
   NTSTATUS Status = STATUS_SUCCESS;

   ExAcquireFastMutex(&RmapListLock);
   entry = MmGetRmapListHeadPage(Page);
   if (entry == NULL)
   {
      ExReleaseFastMutex(&RmapListLock);
      return(STATUS_UNSUCCESSFUL);
   }
   Process = entry->Process;

   if (!ExAcquireRundownProtection(&Process->RundownProtect))
   {
      ExReleaseFastMutex(&RmapListLock);
      return STATUS_PROCESS_IS_TERMINATING;
   }

   Address = entry->Address;
   if ((((ULONG_PTR)Address) & 0xFFF) != 0)
   {
      KeBugCheck(MEMORY_MANAGEMENT);
   }

   if (Address < MmSystemRangeStart)
   {
      Status = ObReferenceObjectByPointer(Process, PROCESS_ALL_ACCESS, NULL, KernelMode);
      ExReleaseFastMutex(&RmapListLock);
      if (!NT_SUCCESS(Status))
      {
         ExReleaseRundownProtection(&Process->RundownProtect);
         return Status;
      }
      AddressSpace = &Process->Vm;
   }
   else
   {
      ExReleaseFastMutex(&RmapListLock);
      AddressSpace = MmGetKernelAddressSpace();
   }

   MmLockAddressSpace(AddressSpace);
   MemoryArea = MmLocateMemoryAreaByAddress(AddressSpace, Address);
   if (MemoryArea == NULL || MemoryArea->DeleteInProgress)
   {
      MmUnlockAddressSpace(AddressSpace);
      ExReleaseRundownProtection(&Process->RundownProtect);
      if (Address < MmSystemRangeStart)
      {
         ObDereferenceObject(Process);
      }
      return(STATUS_UNSUCCESSFUL);
   }
   Type = MemoryArea->Type;
   if (Type == MEMORY_AREA_SECTION_VIEW)
   {
      Offset = (ULONG_PTR)Address - (ULONG_PTR)MemoryArea->StartingAddress
             + MemoryArea->Data.SectionData.ViewOffset;

      /*
       * Get or create a pageop
       */
      PageOp = MmGetPageOp(MemoryArea, NULL, 0,
                           MemoryArea->Data.SectionData.Segment,
                           Offset, MM_PAGEOP_PAGEOUT, TRUE);
      if (PageOp == NULL)
      {
         MmUnlockAddressSpace(AddressSpace);
         ExReleaseRundownProtection(&Process->RundownProtect);
         if (Address < MmSystemRangeStart)
         {
            ObDereferenceObject(Process);
         }
         return(STATUS_UNSUCCESSFUL);
      }

      /*
       * Release locks now we have a page op.
       */
      MmUnlockAddressSpace(AddressSpace);

      /*
       * Do the actual page out work.
       */
      Status = MmPageOutSectionView(AddressSpace, MemoryArea,
                                    Address, PageOp);
   }
   else if (Type == MEMORY_AREA_VIRTUAL_MEMORY)
   {
      PageOp = MmGetPageOp(MemoryArea, Address < MmSystemRangeStart ? Process->UniqueProcessId : NULL,
                           Address, NULL, 0, MM_PAGEOP_PAGEOUT, TRUE);
      if (PageOp == NULL)
      {
         MmUnlockAddressSpace(AddressSpace);
         ExReleaseRundownProtection(&Process->RundownProtect);
         if (Address < MmSystemRangeStart)
         {
            ObDereferenceObject(Process);
         }
         return(STATUS_UNSUCCESSFUL);
      }

      /*
       * Release locks now we have a page op.
       */
      MmUnlockAddressSpace(AddressSpace);

      /*
       * Do the actual page out work.
       */
      Status = MmPageOutVirtualMemory(AddressSpace, MemoryArea,
                                      Address, PageOp);
   }
   else
   {
      KeBugCheck(MEMORY_MANAGEMENT);
   }

   ExReleaseRundownProtection(&Process->RundownProtect);

   if (Address < MmSystemRangeStart)
   {
      ObDereferenceObject(Process);
   }
   return(Status);
}

VOID
NTAPI
MmSetCleanAllRmaps(PFN_NUMBER Page)
{
   PMM_RMAP_ENTRY current_entry;

   ExAcquireFastMutex(&RmapListLock);
   current_entry = MmGetRmapListHeadPage(Page);
   if (current_entry == NULL)
   {
      DPRINT1("MmIsDirtyRmap: No rmaps.\n");
      KeBugCheck(MEMORY_MANAGEMENT);
   }
   while (current_entry != NULL)
   {
      MmSetCleanPage(current_entry->Process, current_entry->Address);
      current_entry = current_entry->Next;
   }
   ExReleaseFastMutex(&RmapListLock);
}

VOID
NTAPI
MmSetDirtyAllRmaps(PFN_NUMBER Page)
{
   PMM_RMAP_ENTRY current_entry;

   ExAcquireFastMutex(&RmapListLock);
   current_entry = MmGetRmapListHeadPage(Page);
   if (current_entry == NULL)
   {
      DPRINT1("MmIsDirtyRmap: No rmaps.\n");
      KeBugCheck(MEMORY_MANAGEMENT);
   }
   while (current_entry != NULL)
   {
      MmSetDirtyPage(current_entry->Process, current_entry->Address);
      current_entry = current_entry->Next;
   }
   ExReleaseFastMutex(&RmapListLock);
}

BOOLEAN
NTAPI
MmIsDirtyPageRmap(PFN_NUMBER Page)
{
   PMM_RMAP_ENTRY current_entry;

   ExAcquireFastMutex(&RmapListLock);
   current_entry = MmGetRmapListHeadPage(Page);
   if (current_entry == NULL)
   {
      ExReleaseFastMutex(&RmapListLock);
      return(FALSE);
   }
   while (current_entry != NULL)
   {
      if (MmIsDirtyPage(current_entry->Process, current_entry->Address))
      {
         ExReleaseFastMutex(&RmapListLock);
         return(TRUE);
      }
      current_entry = current_entry->Next;
   }
   ExReleaseFastMutex(&RmapListLock);
   return(FALSE);
}

VOID
NTAPI
MmInsertRmap(PFN_NUMBER Page, PEPROCESS Process,
             PVOID Address)
{
   PMM_RMAP_ENTRY current_entry;
   PMM_RMAP_ENTRY new_entry;
   ULONG PrevSize;

   Address = (PVOID)PAGE_ROUND_DOWN(Address);

   new_entry = ExAllocateFromNPagedLookasideList(&RmapLookasideList);
   if (new_entry == NULL)
   {
      KeBugCheck(MEMORY_MANAGEMENT);
   }
   new_entry->Address = Address;
   new_entry->Process = (PEPROCESS)Process;
#if DBG
#ifdef __GNUC__
   new_entry->Caller = __builtin_return_address(0);
#else
   new_entry->Caller = _ReturnAddress();
#endif
#endif

   if (MmGetPfnForProcess(Process, Address) != Page)
   {
      DPRINT1("Insert rmap (%d, 0x%.8X) 0x%.8X which doesn't match physical "
              "address 0x%.8X\n", Process->UniqueProcessId, Address,
              MmGetPfnForProcess(Process, Address) << PAGE_SHIFT,
              Page << PAGE_SHIFT);
      KeBugCheck(MEMORY_MANAGEMENT);
   }

   ExAcquireFastMutex(&RmapListLock);
   current_entry = MmGetRmapListHeadPage(Page);
   new_entry->Next = current_entry;
#if DBG
   while (current_entry)
   {
      if (current_entry->Address == new_entry->Address && current_entry->Process == new_entry->Process)
      {
          DbgPrint("MmInsertRmap tries to add a second rmap entry for address %p\n    current caller ",
                   current_entry->Address);
          DbgPrint("%p", new_entry->Caller);
          DbgPrint("\n    previous caller ");
          DbgPrint("%p", current_entry->Caller);
          DbgPrint("\n");
          KeBugCheck(MEMORY_MANAGEMENT);
      }
      current_entry = current_entry->Next;
   }
#endif
   MmSetRmapListHeadPage(Page, new_entry);
   ExReleaseFastMutex(&RmapListLock);
   if (Process == NULL)
   {
      Process = PsInitialSystemProcess;
   }
   if (Process)
   {
      PrevSize = InterlockedExchangeAddUL(&Process->Vm.WorkingSetSize, PAGE_SIZE);
      if (PrevSize >= Process->Vm.PeakWorkingSetSize)
      {
         Process->Vm.PeakWorkingSetSize = PrevSize + PAGE_SIZE;
      }
   }
}

VOID
NTAPI
MmDeleteAllRmaps(PFN_NUMBER Page, PVOID Context,
                 VOID (*DeleteMapping)(PVOID Context, PEPROCESS Process,
                                       PVOID Address))
{
   PMM_RMAP_ENTRY current_entry;
   PMM_RMAP_ENTRY previous_entry;
   PEPROCESS Process;

   ExAcquireFastMutex(&RmapListLock);
   current_entry = MmGetRmapListHeadPage(Page);
   if (current_entry == NULL)
   {
      DPRINT1("MmDeleteAllRmaps: No rmaps.\n");
      KeBugCheck(MEMORY_MANAGEMENT);
   }
   MmSetRmapListHeadPage(Page, NULL);
   ExReleaseFastMutex(&RmapListLock);
   while (current_entry != NULL)
   {
      previous_entry = current_entry;
      current_entry = current_entry->Next;
      if (DeleteMapping)
      {
         DeleteMapping(Context, previous_entry->Process,
                       previous_entry->Address);
      }
      Process = previous_entry->Process;
      ExFreeToNPagedLookasideList(&RmapLookasideList, previous_entry);
      if (Process == NULL)
      {
         Process = PsInitialSystemProcess;
      }
      if (Process)
      {
         (void)InterlockedExchangeAddUL(&Process->Vm.WorkingSetSize, -PAGE_SIZE);
      }
   }
}

VOID
NTAPI
MmDeleteRmap(PFN_NUMBER Page, PEPROCESS Process,
             PVOID Address)
{
   PMM_RMAP_ENTRY current_entry, previous_entry;

   ExAcquireFastMutex(&RmapListLock);
   previous_entry = NULL;
   current_entry = MmGetRmapListHeadPage(Page);

   while (current_entry != NULL)
   {
      if (current_entry->Process == (PEPROCESS)Process &&
            current_entry->Address == Address)
      {
         if (previous_entry == NULL)
         {
            MmSetRmapListHeadPage(Page, current_entry->Next);
         }
         else
         {
            previous_entry->Next = current_entry->Next;
         }
         ExReleaseFastMutex(&RmapListLock);
         ExFreeToNPagedLookasideList(&RmapLookasideList, current_entry);
         if (Process == NULL)
         {
            Process = PsInitialSystemProcess;
         }
         if (Process)
         {
            (void)InterlockedExchangeAddUL(&Process->Vm.WorkingSetSize, -PAGE_SIZE);
         }
         return;
      }
      previous_entry = current_entry;
      current_entry = current_entry->Next;
   }
   KeBugCheck(MEMORY_MANAGEMENT);
}
