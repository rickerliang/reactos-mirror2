/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            ntoskrnl/mm/ARM3/miarm.h
 * PURPOSE:         ARM Memory Manager Header
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

#define MI_MIN_PAGES_FOR_NONPAGED_POOL_TUNING ((255*1024*1024) >> PAGE_SHIFT)
#define MI_MIN_PAGES_FOR_SYSPTE_TUNING         ((19*1024*1024) >> PAGE_SHIFT)
#define MI_MIN_PAGES_FOR_SYSPTE_BOOST          ((32*1024*1024) >> PAGE_SHIFT)
#define MI_MAX_INIT_NONPAGED_POOL_SIZE         (128 * 1024 * 1024)
#define MI_MAX_NONPAGED_POOL_SIZE              (128 * 1024 * 1024)
#define MI_MAX_FREE_PAGE_LISTS                 4

#define MI_MIN_INIT_PAGED_POOLSIZE             (32 * 1024 * 1024)

#define MI_SESSION_VIEW_SIZE                   (20 * 1024 * 1024)
#define MI_SESSION_POOL_SIZE                   (16 * 1024 * 1024)
#define MI_SESSION_IMAGE_SIZE                  (8 * 1024 * 1024)
#define MI_SESSION_WORKING_SET_SIZE            (4 * 1024 * 1024)
#define MI_SESSION_SIZE                        (MI_SESSION_VIEW_SIZE + \
                                                MI_SESSION_POOL_SIZE + \
                                                MI_SESSION_IMAGE_SIZE + \
                                                MI_SESSION_WORKING_SET_SIZE)

#define MI_SYSTEM_VIEW_SIZE                    (16 * 1024 * 1024)

#define MI_SYSTEM_CACHE_WS_START               (PVOID)0xC0C00000
#define MI_PAGED_POOL_START                    (PVOID)0xE1000000
#define MI_NONPAGED_POOL_END                   (PVOID)0xFFBE0000
#define MI_DEBUG_MAPPING                       (PVOID)0xFFBFF000

#define MI_MIN_SECONDARY_COLORS                 8
#define MI_SECONDARY_COLORS                     64
#define MI_MAX_SECONDARY_COLORS                 1024

#define MM_HIGHEST_VAD_ADDRESS \
    (PVOID)((ULONG_PTR)MM_HIGHEST_USER_ADDRESS - (16 * PAGE_SIZE))

/* Make the code cleaner with some definitions for size multiples */
#define _1KB (1024)
#define _1MB (1000 * _1KB)

/* Size of a PDE directory, and size of a page table */
#define PDE_SIZE (PDE_COUNT * sizeof(MMPDE))
#define PT_SIZE  (PTE_COUNT * sizeof(MMPTE))

/* Architecture specific count of PDEs in a directory, and count of PTEs in a PT */
#ifdef _M_IX86
#define PD_COUNT  1
#define PDE_COUNT 1024
#define PTE_COUNT 1024
#elif _M_ARM
#define PD_COUNT  1
#define PDE_COUNT 4096
#define PTE_COUNT 256
#else
#error Define these please!
#endif

//
// PFN List Sentinel
//
#define LIST_HEAD 0xFFFFFFFF

//
// FIXFIX: These should go in ex.h after the pool merge
//
#define POOL_LISTS_PER_PAGE (PAGE_SIZE / sizeof(LIST_ENTRY))
#define BASE_POOL_TYPE_MASK 1
#define POOL_MAX_ALLOC (PAGE_SIZE - (sizeof(POOL_HEADER) + sizeof(LIST_ENTRY)))

typedef struct _POOL_DESCRIPTOR
{
    POOL_TYPE PoolType;
    ULONG PoolIndex;
    ULONG RunningAllocs;
    ULONG RunningDeAllocs;
    ULONG TotalPages;
    ULONG TotalBigPages;
    ULONG Threshold;
    PVOID LockAddress;
    PVOID PendingFrees;
    LONG PendingFreeDepth;
    SIZE_T TotalBytes;
    SIZE_T Spare0;
    LIST_ENTRY ListHeads[POOL_LISTS_PER_PAGE];
} POOL_DESCRIPTOR, *PPOOL_DESCRIPTOR;

typedef struct _POOL_HEADER
{
    union
    {
        struct
        {
            USHORT PreviousSize:9;
            USHORT PoolIndex:7;
            USHORT BlockSize:9;
            USHORT PoolType:7;
        };
        ULONG Ulong1;
    };
    union
    {
        ULONG PoolTag;
        struct
        {
            USHORT AllocatorBackTraceIndex;
            USHORT PoolTagHash;
        };
    };
} POOL_HEADER, *PPOOL_HEADER;

//
// Everything depends on this
//
C_ASSERT(sizeof(POOL_HEADER) == 8);
C_ASSERT(sizeof(POOL_HEADER) == sizeof(LIST_ENTRY));

extern ULONG ExpNumberOfPagedPools;
extern POOL_DESCRIPTOR NonPagedPoolDescriptor;
extern PPOOL_DESCRIPTOR ExpPagedPoolDescriptor[16 + 1];
extern PVOID PoolTrackTable;

//
// END FIXFIX
//

typedef enum _MMSYSTEM_PTE_POOL_TYPE
{
    SystemPteSpace,
    NonPagedPoolExpansion,
    MaximumPtePoolTypes
} MMSYSTEM_PTE_POOL_TYPE;

typedef enum _MI_PFN_CACHE_ATTRIBUTE
{
    MiNonCached,
    MiCached,
    MiWriteCombined,
    MiNotMapped
} MI_PFN_CACHE_ATTRIBUTE, *PMI_PFN_CACHE_ATTRIBUTE;

typedef struct _PHYSICAL_MEMORY_RUN
{
    ULONG BasePage;
    ULONG PageCount;
} PHYSICAL_MEMORY_RUN, *PPHYSICAL_MEMORY_RUN;

typedef struct _PHYSICAL_MEMORY_DESCRIPTOR
{
    ULONG NumberOfRuns;
    ULONG NumberOfPages;
    PHYSICAL_MEMORY_RUN Run[1];
} PHYSICAL_MEMORY_DESCRIPTOR, *PPHYSICAL_MEMORY_DESCRIPTOR;

typedef struct _MMCOLOR_TABLES
{
    PFN_NUMBER Flink;
    PVOID Blink;
    PFN_NUMBER Count;
} MMCOLOR_TABLES, *PMMCOLOR_TABLES;

extern MMPTE HyperTemplatePte;
extern MMPTE ValidKernelPde;
extern MMPTE ValidKernelPte;

extern ULONG MmSizeOfNonPagedPoolInBytes;
extern ULONG MmMaximumNonPagedPoolInBytes;
extern PFN_NUMBER MmMaximumNonPagedPoolInPages;
extern PFN_NUMBER MmSizeOfPagedPoolInPages;
extern PVOID MmNonPagedSystemStart;
extern PVOID MmNonPagedPoolStart;
extern PVOID MmNonPagedPoolExpansionStart;
extern PVOID MmNonPagedPoolEnd;
extern ULONG MmSizeOfPagedPoolInBytes;
extern PVOID MmPagedPoolStart;
extern PVOID MmPagedPoolEnd;
extern PVOID MmSessionBase;
extern ULONG MmSessionSize;
extern PMMPTE MmFirstReservedMappingPte, MmLastReservedMappingPte;
extern PMMPTE MiFirstReservedZeroingPte;
extern MI_PFN_CACHE_ATTRIBUTE MiPlatformCacheAttributes[2][MmMaximumCacheType];
extern PPHYSICAL_MEMORY_DESCRIPTOR MmPhysicalMemoryBlock;
extern ULONG MmBootImageSize;
extern PMMPTE MmSystemPtesStart[MaximumPtePoolTypes];
extern PMMPTE MmSystemPtesEnd[MaximumPtePoolTypes];
extern PMEMORY_ALLOCATION_DESCRIPTOR MxFreeDescriptor;
extern MEMORY_ALLOCATION_DESCRIPTOR MxOldFreeDescriptor;
extern ULONG MxPfnAllocation;
extern MM_PAGED_POOL_INFO MmPagedPoolInfo;
extern RTL_BITMAP MiPfnBitMap;
extern KGUARDED_MUTEX MmPagedPoolMutex;
extern PVOID MmPagedPoolStart;
extern PVOID MmPagedPoolEnd;
extern PVOID MmNonPagedSystemStart;
extern PVOID MiSystemViewStart;
extern ULONG MmSystemViewSize;
extern PVOID MmSessionBase;
extern PVOID MiSessionSpaceEnd;
extern ULONG MmSizeOfPagedPoolInBytes;
extern PMMPTE MmSystemPagePtes;
extern PVOID MmSystemCacheStart;
extern PVOID MmSystemCacheEnd;
extern MMSUPPORT MmSystemCacheWs;
extern SIZE_T MmAllocatedNonPagedPool;
extern ULONG_PTR MmSubsectionBase;
extern ULONG MmSpecialPoolTag;
extern PVOID MmHyperSpaceEnd;
extern PMMWSL MmSystemCacheWorkingSetList;
extern ULONG MmMinimumNonPagedPoolSize;
extern ULONG MmMinAdditionNonPagedPoolPerMb;
extern ULONG MmDefaultMaximumNonPagedPool;
extern ULONG MmMaxAdditionNonPagedPoolPerMb;
extern ULONG MmSecondaryColors;
extern ULONG MmSecondaryColorMask;
extern ULONG MmNumberOfSystemPtes;
extern ULONG MmMaximumNonPagedPoolPercent;
extern ULONG MmLargeStackSize;
extern PMMCOLOR_TABLES MmFreePagesByColor[FreePageList + 1];
extern ULONG MmProductType;
extern MM_SYSTEMSIZE MmSystemSize;
extern PKEVENT MiLowMemoryEvent;
extern PKEVENT MiHighMemoryEvent;
extern PKEVENT MiLowPagedPoolEvent;
extern PKEVENT MiHighPagedPoolEvent;
extern PKEVENT MiLowNonPagedPoolEvent;
extern PKEVENT MiHighNonPagedPoolEvent;
extern PFN_NUMBER MmLowMemoryThreshold;
extern PFN_NUMBER MmHighMemoryThreshold;
extern PFN_NUMBER MiLowPagedPoolThreshold;
extern PFN_NUMBER MiHighPagedPoolThreshold;
extern PFN_NUMBER MiLowNonPagedPoolThreshold;
extern PFN_NUMBER MiHighNonPagedPoolThreshold;
extern PFN_NUMBER MmMinimumFreePages;
extern PFN_NUMBER MmPlentyFreePages;

#define MI_PFN_TO_PFNENTRY(x)     (&MmPfnDatabase[1][x])
#define MI_PFNENTRY_TO_PFN(x)     (x - MmPfnDatabase[1])

NTSTATUS
NTAPI
MmArmInitSystem(
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
);

NTSTATUS
NTAPI
MiInitMachineDependent(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
);

VOID
NTAPI
MiComputeColorInformation(
    VOID
);

VOID
NTAPI
MiMapPfnDatabase(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
);

VOID
NTAPI
MiInitializeColorTables(
    VOID
);

VOID
NTAPI
MiInitializePfnDatabase(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
);

BOOLEAN
NTAPI
MiInitializeMemoryEvents(
    VOID
);
    
PFN_NUMBER
NTAPI
MxGetNextPage(
    IN PFN_NUMBER PageCount
);

PPHYSICAL_MEMORY_DESCRIPTOR
NTAPI
MmInitializeMemoryLimits(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN PBOOLEAN IncludeType
);
                         
PFN_NUMBER
NTAPI
MiPagesInLoaderBlock(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN PBOOLEAN IncludeType
);
                     
VOID
FASTCALL
MiSyncARM3WithROS(
    IN PVOID AddressStart,
    IN PVOID AddressEnd
);
                         
NTSTATUS
NTAPI
MmArmAccessFault(
    IN BOOLEAN StoreInstruction,
    IN PVOID Address,
    IN KPROCESSOR_MODE Mode,
    IN PVOID TrapInformation
);

VOID
NTAPI
MiInitializeNonPagedPool(
    VOID
);

VOID
NTAPI
MiInitializeNonPagedPoolThresholds(
    VOID
);

VOID
NTAPI
MiInitializePoolEvents(
    VOID
);

VOID                      //
NTAPI                     //
InitializePool(           //
    IN POOL_TYPE PoolType,// FIXFIX: This should go in ex.h after the pool merge
    IN ULONG Threshold    //
);                        //

VOID
NTAPI
MiInitializeSystemPtes(
    IN PMMPTE StartingPte,
    IN ULONG NumberOfPtes,
    IN MMSYSTEM_PTE_POOL_TYPE PoolType
);

PMMPTE
NTAPI
MiReserveSystemPtes(
    IN ULONG NumberOfPtes,
    IN MMSYSTEM_PTE_POOL_TYPE SystemPtePoolType
);

VOID
NTAPI
MiReleaseSystemPtes(
    IN PMMPTE StartingPte,
    IN ULONG NumberOfPtes,
    IN MMSYSTEM_PTE_POOL_TYPE SystemPtePoolType
);


PFN_NUMBER
NTAPI
MiFindContiguousPages(
    IN PFN_NUMBER LowestPfn,
    IN PFN_NUMBER HighestPfn,
    IN PFN_NUMBER BoundaryPfn,
    IN PFN_NUMBER SizeInPages,
    IN MEMORY_CACHING_TYPE CacheType
);

PVOID
NTAPI
MiCheckForContiguousMemory(
    IN PVOID BaseAddress,
    IN PFN_NUMBER BaseAddressPages,
    IN PFN_NUMBER SizeInPages,
    IN PFN_NUMBER LowestPfn,
    IN PFN_NUMBER HighestPfn,
    IN PFN_NUMBER BoundaryPfn,
    IN MI_PFN_CACHE_ATTRIBUTE CacheAttribute
);

PMDL
NTAPI
MiAllocatePagesForMdl(
    IN PHYSICAL_ADDRESS LowAddress,
    IN PHYSICAL_ADDRESS HighAddress,
    IN PHYSICAL_ADDRESS SkipBytes,
    IN SIZE_T TotalBytes,
    IN MI_PFN_CACHE_ATTRIBUTE CacheAttribute,
    IN ULONG Flags
);

PVOID
NTAPI
MiMapLockedPagesInUserSpace(
    IN PMDL Mdl,
    IN PVOID BaseVa,
    IN MEMORY_CACHING_TYPE CacheType,
    IN PVOID BaseAddress
);

VOID
NTAPI
MiUnmapLockedPagesInUserSpace(
    IN PVOID BaseAddress,
    IN PMDL Mdl
);

VOID
NTAPI
MiInsertInListTail(
    IN PMMPFNLIST ListHead,
    IN PMMPFN Entry
);

VOID
NTAPI
MiUnlinkFreeOrZeroedPage(
    IN PMMPFN Entry
);

PMMPFN
NTAPI
MiRemoveHeadList(
    IN PMMPFNLIST ListHead
);


VOID
NTAPI
MiInsertPageInFreeList(
    IN PFN_NUMBER PageFrameIndex
);

/* EOF */
