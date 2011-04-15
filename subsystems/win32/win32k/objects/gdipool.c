/*
 * PROJECT:         ReactOS win32 kernel mode subsystem
 * LICENSE:         GPL - See COPYING in the top level directory
 * FILE:            subsystems/win32/win32k/objects/gdiobj.c
 * PURPOSE:         Static size allocator for user mode object attributes
 * PROGRAMMERS:     Timo Kreuzer
 */

/* INCLUDES ******************************************************************/

#include <win32k.h>
#define NDEBUG
#include <debug.h>

typedef struct _GDI_POOL_SECTION
{
    LIST_ENTRY leInUseLink;
    LIST_ENTRY leReadyLink;

    PVOID pvBaseAddress;

    ULONG ulCommitBitmap;
    ULONG cAllocCount;

    RTL_BITMAP bitmap;
    ULONG aulBits[1];
} GDI_POOL_SECTION, *PGDI_POOL_SECTION;

typedef struct _GDI_POOL
{
    ULONG ulTag;
    ULONG cjAllocSize;
    ULONG cjSectionSize; // 32 * cjAllocSize, rounded up to pages
    ULONG cSlotsPerSection;
    ULONG cEmptySections;
    EX_PUSH_LOCK pushlock; // for pool growth

    LIST_ENTRY leInUseList;
    LIST_ENTRY leEmptyList;
    LIST_ENTRY leReadyList;
} GDI_POOL;

#define GDI_POOL_ALLOCATION_GRANULARITY 64 * 1024

static
PGDI_POOL_SECTION
GdiPoolAllocateSection(PGDI_POOL pPool)
{
    PGDI_POOL_SECTION pSection;
    PVOID pvBaseAddress;
    SIZE_T cjSize;
    NTSTATUS status;

    /* Allocate a section object */
    cjSize = sizeof(GDI_POOL_SECTION) + pPool->cSlotsPerSection / sizeof(ULONG);
    pSection = EngAllocMem(0, cjSize, pPool->ulTag);
    if (!pSection)
    {
        return NULL;
    }

    /* Reserve user mode memory */
    cjSize = GDI_POOL_ALLOCATION_GRANULARITY;
    pvBaseAddress = NULL;
    status = ZwAllocateVirtualMemory(NtCurrentProcess(),
                                     &pvBaseAddress,
                                     0,
                                     &cjSize,
                                     MEM_RESERVE,
                                     PAGE_READWRITE);
    if (!NT_SUCCESS(status))
    {
        EngFreeMem(pSection);
        return NULL;
    }

    /* Initialize the section */
    pSection->pvBaseAddress = pvBaseAddress;
    pSection->ulCommitBitmap = 0;
    pSection->cAllocCount = 0;
    RtlInitializeBitMap(&pSection->bitmap,
                        pSection->aulBits,
                        pPool->cSlotsPerSection);
    RtlClearAllBits(&pSection->bitmap);

    /* Return the section */
    return pSection;
}

static
VOID
GdiPoolDeleteSection(PGDI_POOL pPool, PGDI_POOL_SECTION pSection)
{
    NTSTATUS status;
    SIZE_T cjSize = 0;

    /* Should not have any allocations */
    ASSERT(pSection->cAllocCount == 0);

    /* Release the virtual memory */
    status = ZwFreeVirtualMemory(NtCurrentProcess(),
                                 &pSection->pvBaseAddress,
                                 &cjSize,
                                 MEM_RELEASE);
    ASSERT(NT_SUCCESS(status));

    /* Free the section object */
    EngFreeMem(pSection);
}

PVOID
NTAPI
GdiPoolAllocate(
    PGDI_POOL pPool)
{
    PGDI_POOL_SECTION pSection;
    ULONG ulIndex, cjOffset, ulPageBit;
    PLIST_ENTRY ple;
    PVOID pvAlloc, pvBaseAddress;
    SIZE_T cjSize;
    NTSTATUS status;

    /* Disable APCs and acquire the pool lock */
    KeEnterCriticalRegion();
    ExAcquirePushLockExclusive(&pPool->pushlock);

    /* Check if we have a ready section */
    if (!IsListEmpty(&pPool->leReadyList))
    {
        /* Get a free section */
        ple = pPool->leReadyList.Flink;
        pSection = CONTAINING_RECORD(ple, GDI_POOL_SECTION, leReadyLink);
        ASSERT(pSection->cAllocCount < pPool->cSlotsPerSection);
    }
    else
    {
        /* No, check if we have something on the empty list */
        if (!IsListEmpty(&pPool->leEmptyList))
        {
            /* Yes, remove it from the empty list */
            ple = RemoveHeadList(&pPool->leEmptyList);
            pSection = CONTAINING_RECORD(ple, GDI_POOL_SECTION, leInUseLink);
        }
        else
        {
            /* No, allocate a new section */
            pSection = GdiPoolAllocateSection(pPool);
            if (!pSection)
            {
                DPRINT1("Couldn't allocate a section\n");
                pvAlloc = NULL;
                goto done;
            }

            /* Insert it into the ready list */
            InsertHeadList(&pPool->leReadyList, &pSection->leReadyLink);
        }

        /* Insert it into the in-use list */
        InsertHeadList(&pPool->leInUseList, &pSection->leInUseLink);
    }

    /* Find and set a single bit */
    ulIndex = RtlFindClearBitsAndSet(&pSection->bitmap, 1, 0);
    ASSERT(ulIndex != MAXULONG);

    /* Calculate the allocation address */
    cjOffset = ulIndex * pPool->cjAllocSize;
    pvAlloc = (PVOID)((ULONG_PTR)pSection->pvBaseAddress + cjOffset);

    /* Check if memory is comitted */
    ulPageBit = 1 << (cjOffset / PAGE_SIZE);
    ulPageBit |= 1 << ((cjOffset + pPool->cjAllocSize - 1) / PAGE_SIZE);
    if ((pSection->ulCommitBitmap & ulPageBit) != ulPageBit)
    {
        /* Commit the pages */
        pvBaseAddress = PAGE_ALIGN(pvAlloc);
        cjSize = ADDRESS_AND_SIZE_TO_SPAN_PAGES(pvAlloc, pPool->cjAllocSize) * PAGE_SIZE;
        status = ZwAllocateVirtualMemory(NtCurrentProcess(),
                                         &pvBaseAddress,
                                         0,
                                         &cjSize,
                                         MEM_COMMIT,
                                         PAGE_READWRITE);

        pSection->ulCommitBitmap |= ulPageBit;
    }

    /* Increase alloc count and check if section is now busy */
    pSection->cAllocCount++;
    if (pSection->cAllocCount == pPool->cSlotsPerSection)
    {
        /* Remove the section from the ready list */
        RemoveEntryList(&pSection->leReadyLink);
    }

done:
    /* Release the pool lock and enable APCs */
    ExReleasePushLockExclusive(&pPool->pushlock);
    KeLeaveCriticalRegion();
DPRINT1("GdiPoolallocate: %p\n", pvAlloc);

    return pvAlloc;
}


VOID
NTAPI
GdiPoolFree(
    PGDI_POOL pPool,
    PVOID pvAlloc)
{
    PLIST_ENTRY ple;
    PGDI_POOL_SECTION pSection;
    ULONG_PTR cjOffset;
    ULONG ulIndex;
DPRINT1("GdiPoolFree: %p\n", pvAlloc);

    /* Disable APCs and acquire the pool lock */
    KeEnterCriticalRegion();
    ExAcquirePushLockExclusive(&pPool->pushlock);

    /* Loop all used sections */
    for (ple = pPool->leInUseList.Flink;
         ple != &pPool->leInUseList;
         ple = ple->Flink)
    {
        /* Get the pointer to the section */
        pSection = CONTAINING_RECORD(ple, GDI_POOL_SECTION, leInUseLink);

        /* Calculate offset */
        cjOffset = (ULONG_PTR)pvAlloc - (ULONG_PTR)pSection->pvBaseAddress;

        /* Check if the allocation is from this section */
        if (cjOffset < pPool->cjSectionSize)
        {
            /* Calculate the index of the allocation */
            ulIndex = cjOffset / pPool->cjAllocSize;

            /* Mark it as free */
            ASSERT(RtlTestBit(&pSection->bitmap, ulIndex) == TRUE);
            RtlClearBit(&pSection->bitmap, ulIndex);

            /* Decrease allocation count */
            pSection->cAllocCount--;

            /* Check if the section got valid now */
            if (pSection->cAllocCount == pPool->cSlotsPerSection - 1)
            {
                /* Insert it into the ready list */
                InsertTailList(&pPool->leReadyList, &pSection->leReadyLink);
            }
            /* Check if it got empty now */
            else if (pSection->cAllocCount == 0)
            {
                /* Remove the section from the lists */
                RemoveEntryList(&pSection->leInUseLink);
                RemoveEntryList(&pSection->leReadyLink);

                if (pPool->cEmptySections > 1)
                {
                    /* Delete the section */
                    GdiPoolDeleteSection(pPool, pSection);
                }
                else
                {
                    /* Insert it into the empty list */
                    InsertHeadList(&pPool->leEmptyList, &pSection->leInUseLink);
                    pPool->cEmptySections++;
                }
            }

            goto done;
        }
    }

    ASSERT(FALSE);
    // KeBugCheck()

done:
    /* Release the pool lock and enable APCs */
    ExReleasePushLockExclusive(&pPool->pushlock);
    KeLeaveCriticalRegion();
}

PGDI_POOL
NTAPI
GdiPoolCreate(
    ULONG cjAllocSize,
    ULONG ulTag)
{
    PGDI_POOL pPool;

    /* Allocate a pool object */
    pPool = EngAllocMem(0, sizeof(GDI_POOL), 'lopG');
    if (!pPool) return NULL;

    /* Initialize the object */
    ExInitializePushLock(&pPool->pushlock);
    InitializeListHead(&pPool->leInUseList);
    InitializeListHead(&pPool->leReadyList);
    InitializeListHead(&pPool->leEmptyList);
    pPool->cEmptySections = 0;
    pPool->cjAllocSize = cjAllocSize;
    pPool->ulTag = ulTag;
    pPool->cjSectionSize = GDI_POOL_ALLOCATION_GRANULARITY;
    pPool->cSlotsPerSection = pPool->cjSectionSize / cjAllocSize;

    return pPool;
}

VOID
NTAPI
GdiPoolDestroy(PGDI_POOL pPool)
{
    PGDI_POOL_SECTION pSection;
    PLIST_ENTRY ple;

    /* Loop all empty sections, removing them */
    while ((ple = RemoveHeadList(&pPool->leEmptyList)))
    {
        /* Delete the section */
        pSection = CONTAINING_RECORD(ple, GDI_POOL_SECTION, leInUseLink);
        GdiPoolDeleteSection(pPool, pSection);
    }

    /* Loop all ready sections, removing them */
    while ((ple = RemoveHeadList(&pPool->leInUseList)))
    {
        /* Delete the section */
        pSection = CONTAINING_RECORD(ple, GDI_POOL_SECTION, leInUseLink);
        GdiPoolDeleteSection(pPool, pSection);
    }

    EngFreeMem(pPool);
}
