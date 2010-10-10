/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS System Libraries
 * FILE:            lib/rtl/heap.h
 * PURPOSE:         Run-Time Libary Heap Manager header
 * PROGRAMMER:      Aleksey Bragin
 */

/* INCLUDES ******************************************************************/

#ifndef RTL_HEAP_H
#define RTL_HEAP_H

/* Core heap definitions */
#define HEAP_FREELISTS 128
#define HEAP_SEGMENTS 64

#define HEAP_ENTRY_SIZE ((ULONG)sizeof(HEAP_ENTRY))
#ifdef _WIN64
#define HEAP_ENTRY_SHIFT 4
#else
#define HEAP_ENTRY_SHIFT 3
#endif
#define HEAP_MAX_BLOCK_SIZE ((0x80000 - PAGE_SIZE) >> HEAP_ENTRY_SHIFT)

#define ARENA_INUSE_FILLER     0xBAADF00D
#define ARENA_FREE_FILLER      0xFEEEFEEE
#define HEAP_TAIL_FILL         0xab

// from ntifs.h, should go to another header!
#define HEAP_GLOBAL_TAG                 0x0800
#define HEAP_PSEUDO_TAG_FLAG            0x8000
#define HEAP_TAG_MASK                  (HEAP_MAXIMUM_TAG << HEAP_TAG_SHIFT)

#define HEAP_EXTRA_FLAGS_MASK (HEAP_CAPTURE_STACK_BACKTRACES | \
                               HEAP_SETTABLE_USER_VALUE | \
                               (HEAP_TAG_MASK ^ (0xFF << HEAP_TAG_SHIFT)))

/* Heap entry flags */
#define HEAP_ENTRY_BUSY           0x01
#define HEAP_ENTRY_EXTRA_PRESENT  0x02
#define HEAP_ENTRY_FILL_PATTERN   0x04
#define HEAP_ENTRY_VIRTUAL_ALLOC  0x08
#define HEAP_ENTRY_LAST_ENTRY     0x10
#define HEAP_ENTRY_SETTABLE_FLAG1 0x20
#define HEAP_ENTRY_SETTABLE_FLAG2 0x40
#define HEAP_ENTRY_SETTABLE_FLAG3 0x80
#define HEAP_ENTRY_SETTABLE_FLAGS (HEAP_ENTRY_SETTABLE_FLAG1 | HEAP_ENTRY_SETTABLE_FLAG2 | HEAP_ENTRY_SETTABLE_FLAG3)

/* Signatures */
#define HEAP_SIGNATURE         0xeefeeff
#define HEAP_SEGMENT_SIGNATURE 0xffeeffee

/* Segment flags */
#define HEAP_USER_ALLOCATED    0x1

/* Heap structures */
struct _HEAP_COMMON_ENTRY
{
    union
    {
        struct
        {
            USHORT Size;
            UCHAR Flags;
            UCHAR SmallTagIndex;
        };
        struct
        {
            PVOID SubSegmentCode;
            USHORT PreviousSize;
            union
            {
                UCHAR SegmentOffset;
                UCHAR LFHFlags;
            };
            UCHAR UnusedBytes;
        };
        struct
        {
            USHORT FunctionIndex;
            USHORT ContextValue;
        };
        struct
        {
            ULONG InterceptorValue;
            USHORT UnusedBytesLength;
            UCHAR EntryOffset;
            UCHAR ExtendedBlockSignature;
        };
        struct
        {
            ULONG Code1;
            USHORT Code2;
            UCHAR Code3;
            UCHAR Code4;
        };
        ULONGLONG AgregateCode;
    };
};

typedef struct _HEAP_FREE_ENTRY
{
    struct _HEAP_COMMON_ENTRY;
    LIST_ENTRY FreeList;
}  HEAP_FREE_ENTRY, *PHEAP_FREE_ENTRY;

typedef struct _HEAP_ENTRY
{
    struct _HEAP_COMMON_ENTRY;
}  HEAP_ENTRY, *PHEAP_ENTRY;

#ifdef _WIN64
C_ASSERT(sizeof(HEAP_ENTRY) == 16);
#else
C_ASSERT(sizeof(HEAP_ENTRY) == 8);
#endif
C_ASSERT((1 << HEAP_ENTRY_SHIFT) == sizeof(HEAP_ENTRY));

typedef struct _HEAP_TAG_ENTRY
{
    ULONG Allocs;
    ULONG Frees;
    ULONG Size;
    USHORT TagIndex;
    USHORT CreatorBackTraceIndex;
    WCHAR TagName[24];
} HEAP_TAG_ENTRY, *PHEAP_TAG_ENTRY;

typedef struct _HEAP_PSEUDO_TAG_ENTRY
{
    ULONG Allocs;
    ULONG Frees;
    ULONG Size;
} HEAP_PSEUDO_TAG_ENTRY, *PHEAP_PSEUDO_TAG_ENTRY;

typedef struct _HEAP_COUNTERS
{
    ULONG TotalMemoryReserved;
    ULONG TotalMemoryCommitted;
    ULONG TotalMemoryLargeUCR;
    ULONG TotalSizeInVirtualBlocks;
    ULONG TotalSegments;
    ULONG TotalUCRs;
    ULONG CommittOps;
    ULONG DeCommitOps;
    ULONG LockAcquires;
    ULONG LockCollisions;
    ULONG CommitRate;
    ULONG DecommittRate;
    ULONG CommitFailures;
    ULONG InBlockCommitFailures;
    ULONG CompactHeapCalls;
    ULONG CompactedUCRs;
    ULONG InBlockDeccommits;
    ULONG InBlockDeccomitSize;
} HEAP_COUNTERS, *PHEAP_COUNTERS;

typedef struct _HEAP_TUNING_PARAMETERS
{
    ULONG CommittThresholdShift;
    ULONG MaxPreCommittThreshold;
} HEAP_TUNING_PARAMETERS, *PHEAP_TUNING_PARAMETERS;

typedef struct _HEAP
{
    HEAP_ENTRY Entry;
    ULONG SegmentSignature;
    ULONG SegmentFlags;
    LIST_ENTRY SegmentListEntry;
    struct _HEAP *Heap;
    PVOID BaseAddress;
    ULONG NumberOfPages;
    PHEAP_ENTRY FirstEntry;
    PHEAP_ENTRY LastValidEntry;
    ULONG NumberOfUnCommittedPages;
    ULONG NumberOfUnCommittedRanges;
    USHORT SegmentAllocatorBackTraceIndex;
    USHORT Reserved;
    LIST_ENTRY UCRSegmentList;
    ULONG Flags;
    ULONG ForceFlags;
    ULONG CompatibilityFlags;
    ULONG EncodeFlagMask;
    HEAP_ENTRY Encoding;
    ULONG PointerKey;
    ULONG Interceptor;
    ULONG VirtualMemoryThreshold;
    ULONG Signature;
    ULONG SegmentReserve;
    ULONG SegmentCommit;
    ULONG DeCommitFreeBlockThreshold;
    ULONG DeCommitTotalFreeThreshold;
    ULONG TotalFreeSize;
    ULONG MaximumAllocationSize;
    USHORT ProcessHeapsListIndex;
    USHORT HeaderValidateLength;
    PVOID HeaderValidateCopy;
    USHORT NextAvailableTagIndex;
    USHORT MaximumTagIndex;
    PHEAP_TAG_ENTRY TagEntries;
    LIST_ENTRY UCRList;
    ULONG AlignRound;
    ULONG AlignMask;
    LIST_ENTRY VirtualAllocdBlocks;
    LIST_ENTRY SegmentList;
    struct _HEAP_SEGMENT *Segments[HEAP_SEGMENTS]; //FIXME: non-Vista
    USHORT AllocatorBackTraceIndex;
    ULONG NonDedicatedListLength;
    PVOID BlocksIndex;
    PVOID UCRIndex;
    PHEAP_PSEUDO_TAG_ENTRY PseudoTagEntries;
    LIST_ENTRY FreeLists[HEAP_FREELISTS]; //FIXME: non-Vista
    union
    {
        ULONG FreeListsInUseUlong[HEAP_FREELISTS / (sizeof(ULONG) * 8)]; //FIXME: non-Vista
        UCHAR FreeListsInUseBytes[HEAP_FREELISTS / (sizeof(UCHAR) * 8)]; //FIXME: non-Vista
    } u;
    PHEAP_LOCK LockVariable;
    PRTL_HEAP_COMMIT_ROUTINE CommitRoutine;
    PVOID FrontEndHeap;
    USHORT FrontHeapLockCount;
    UCHAR FrontEndHeapType;
    HEAP_COUNTERS Counters;
    HEAP_TUNING_PARAMETERS TuningParameters;
} HEAP, *PHEAP;

typedef struct _HEAP_SEGMENT
{
    HEAP_ENTRY Entry;
    ULONG SegmentSignature;
    ULONG SegmentFlags;
    LIST_ENTRY SegmentListEntry;
    PHEAP Heap;
    PVOID BaseAddress;
    ULONG NumberOfPages;
    PHEAP_ENTRY FirstEntry;
    PHEAP_ENTRY LastValidEntry;
    ULONG NumberOfUnCommittedPages;
    ULONG NumberOfUnCommittedRanges;
    USHORT SegmentAllocatorBackTraceIndex;
    USHORT Reserved;
    LIST_ENTRY UCRSegmentList;
    PHEAP_ENTRY LastEntryInSegment; //FIXME: non-Vista
} HEAP_SEGMENT, *PHEAP_SEGMENT;

typedef struct _HEAP_UCR_DESCRIPTOR
{
    LIST_ENTRY ListEntry;
    LIST_ENTRY SegmentEntry;
    PVOID Address;
    ULONG Size;
} HEAP_UCR_DESCRIPTOR, *PHEAP_UCR_DESCRIPTOR;

typedef struct _HEAP_ENTRY_EXTRA
{
     union
     {
          struct
          {
               USHORT AllocatorBackTraceIndex;
               USHORT TagIndex;
               ULONG_PTR Settable;
          };
          UINT64 ZeroInit;
     };
} HEAP_ENTRY_EXTRA, *PHEAP_ENTRY_EXTRA;

typedef HEAP_ENTRY_EXTRA HEAP_FREE_ENTRY_EXTRA, *PHEAP_FREE_ENTRY_EXTRA;

typedef struct _HEAP_VIRTUAL_ALLOC_ENTRY
{
    LIST_ENTRY Entry;
    HEAP_ENTRY_EXTRA ExtraStuff;
    ULONG CommitSize;
    ULONG ReserveSize;
    HEAP_ENTRY BusyBlock;
} HEAP_VIRTUAL_ALLOC_ENTRY, *PHEAP_VIRTUAL_ALLOC_ENTRY;

/* Global variables */
extern HEAP_LOCK RtlpProcessHeapsListLock;
extern BOOLEAN RtlpPageHeapEnabled;

/* Functions declarations */

/* heap.c */
PHEAP_FREE_ENTRY NTAPI
RtlpCoalesceFreeBlocks (PHEAP Heap,
                        PHEAP_FREE_ENTRY FreeEntry,
                        PSIZE_T FreeSize,
                        BOOLEAN Remove);

PHEAP_ENTRY_EXTRA NTAPI
RtlpGetExtraStuffPointer(PHEAP_ENTRY HeapEntry);

/* heapdbg.c */
HANDLE NTAPI
RtlpPageHeapCreate(ULONG Flags,
                   PVOID Addr,
                   SIZE_T TotalSize,
                   SIZE_T CommitSize,
                   PVOID Lock,
                   PRTL_HEAP_PARAMETERS Parameters);

#endif
