/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            ntoskrnl/mm/ARM3/pfnlist.c
 * PURPOSE:         ARM Memory Manager PFN List Manipulation
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

/* INCLUDES *******************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <debug.h>

#line 15 "ARM³::PFNLIST"
#define MODULE_INVOLVED_IN_ARM3
#include "../ARM3/miarm.h"

/* GLOBALS ********************************************************************/

MMPFNLIST MmZeroedPageListHead;
MMPFNLIST MmFreePageListHead;
MMPFNLIST MmStandbyPageListHead;
MMPFNLIST MmModifiedPageListHead;
MMPFNLIST MmModifiedNoWritePageListHead;

/* FUNCTIONS ******************************************************************/

VOID
NTAPI
MiInsertInListTail(IN PMMPFNLIST ListHead,
                   IN PMMPFN Entry)
{
    PFN_NUMBER OldBlink, EntryIndex = MiGetPfnEntryIndex(Entry);

    /* Get the back link */
    OldBlink = ListHead->Blink;
    if (OldBlink != LIST_HEAD)
    {
        /* Set the back pointer to point to us now */
        MiGetPfnEntry(OldBlink)->u1.Flink = EntryIndex;
    }
    else
    {
        /* Set the list to point to us */
        ListHead->Flink = EntryIndex;
    }
    
    /* Set the entry to point to the list head forwards, and the old page backwards */
    Entry->u1.Flink = LIST_HEAD;
    Entry->u2.Blink = OldBlink;
    
    /* And now the head points back to us, since we are last */
    ListHead->Blink = EntryIndex;
    ListHead->Total++;
}

VOID
NTAPI
MiRemoveFromList(IN PMMPFN Entry)
{
    PFN_NUMBER OldFlink, OldBlink;
    PMMPFNLIST ListHead;
    
    /* Find the list for this */
    if (Entry->u3.e1.PageLocation == ZeroedPageList)
    {
        ListHead = &MmZeroedPageListHead;
    }
    else if (Entry->u3.e1.PageLocation == FreePageList)
    {
        ListHead = &MmFreePageListHead;
    }
    else
    {
        ListHead = NULL;
        ASSERT(ListHead != NULL);
    }
    
    /* Get the forward and back pointers */
    OldFlink = Entry->u1.Flink;
    OldBlink = Entry->u2.Blink;
    
    /* Check if the next entry is the list head */
    if (OldFlink != LIST_HEAD)
    {
        /* It is not, so set the backlink of the actual entry, to our backlink */
        MiGetPfnEntry(OldFlink)->u2.Blink = OldBlink;
    }
    else
    {
        /* Set the list head's backlink instead */
        ListHead->Blink = OldFlink;
    }
    
    /* Check if the back entry is the list head */
    if (OldBlink != LIST_HEAD)
    {
        /* It is not, so set the backlink of the actual entry, to our backlink */
        MiGetPfnEntry(OldBlink)->u1.Flink = OldFlink;
    }
    else
    {
        /* Set the list head's backlink instead */
        ListHead->Flink = OldFlink;
    }
    
    /* We are not on a list anymore */
    ListHead->Total--;
    Entry->u1.Flink = Entry->u2.Blink = 0;
}

PMMPFN
NTAPI
MiRemoveHeadList(IN PMMPFNLIST ListHead)
{
    PFN_NUMBER Entry, Flink;
    PMMPFN Pfn1;
    
    /* Get the entry that's currently first on the list */
    Entry = ListHead->Flink;
    Pfn1 = MiGetPfnEntry(Entry);
    
    /* Make the list point to the entry following the first one */
    Flink = Pfn1->u1.Flink;
    ListHead->Flink = Flink;

    /* Check if the next entry is actually the list head */
    if (ListHead->Flink != LIST_HEAD)
    {
        /* It isn't, so therefore whoever is coming next points back to the head */
        MiGetPfnEntry(Flink)->u2.Blink = LIST_HEAD;
    }
    else
    {
        /* Then the list is empty, so the backlink should point back to us */
        ListHead->Blink = LIST_HEAD;
    }
  
    /* We are not on a list anymore */
    Pfn1->u1.Flink = Pfn1->u2.Blink = 0;
    ListHead->Total--;
    
    /* Return the head element */
    return Pfn1;
}

VOID
NTAPI
MiInsertPageInFreeList(IN PFN_NUMBER PageFrameIndex)
{
    MMLISTS ListName;
    PMMPFNLIST ListHead;
    PFN_NUMBER LastPage;
    PMMPFN Pfn1, Blink;
    ULONG Color;
    PMMCOLOR_TABLES ColorHead;

    /* Make sure the page index is valid */
    ASSERT((PageFrameIndex != 0) &&
           (PageFrameIndex <= MmHighestPhysicalPage) &&
           (PageFrameIndex >= MmLowestPhysicalPage));

    /* Get the PFN entry */
    Pfn1 = MI_PFN_TO_PFNENTRY(PageFrameIndex);

    /* Sanity checks that a right kind of page is being inserted here */
    ASSERT(Pfn1->u4.MustBeCached == 0);
    ASSERT(Pfn1->u3.e1.Rom != 1);
    ASSERT(Pfn1->u3.e1.RemovalRequested == 0);
    ASSERT(Pfn1->u4.VerifierAllocation == 0);
    ASSERT(Pfn1->u3.e2.ReferenceCount == 0);

    /* Get the free page list and increment its count */
    ListHead = &MmFreePageListHead;
    ListName = FreePageList;
    ListHead->Total++;

    /* Get the last page on the list */
    LastPage = ListHead->Blink;
    if (LastPage != -1)
    {
        /* Link us with the previous page, so we're at the end now */
        MI_PFN_TO_PFNENTRY(LastPage)->u1.Flink = PageFrameIndex;
    }
    else
    {
        /* The list is empty, so we are the first page */
        ListHead->Flink = PageFrameIndex;
    }

    /* Now make the list head point back to us (since we go at the end) */
    ListHead->Blink = PageFrameIndex;
    
    /* And initialize our own list pointers */
    Pfn1->u1.Flink = -1;
    Pfn1->u2.Blink = LastPage;

    /* Set the list name and default priority */
    Pfn1->u3.e1.PageLocation = ListName;
    Pfn1->u4.Priority = 3;
    
    /* Clear some status fields */
    Pfn1->u4.InPageError = 0;
    Pfn1->u4.AweAllocation = 0;

    /* FIXME: More work to be done regarding page accounting */

    /* Get the page color */
    Color = PageFrameIndex & MmSecondaryColorMask;

    /* Get the first page on the color list */
    ColorHead = &MmFreePagesByColor[ListName][Color];
    if (ColorHead->Flink == -1)
    {
        /* The list is empty, so we are the first page */
        Pfn1->u4.PteFrame = -1;
        ColorHead->Flink = PageFrameIndex;
    }
    else
    {
        /* Get the previous page */
        Blink = (PMMPFN)ColorHead->Blink;
        
        /* Make it link to us */
        Pfn1->u4.PteFrame = MI_PFNENTRY_TO_PFN(Blink);
        Blink->OriginalPte.u.Long = PageFrameIndex;
    }
    
    /* Now initialize our own list pointers */
    ColorHead->Blink = Pfn1;
    Pfn1->OriginalPte.u.Long = -1;
    
    /* And increase the count in the colored list */
    ColorHead->Count++;
    
    /* FIXME: Notify zero page thread if enough pages are on the free list now */
}

/* EOF */
