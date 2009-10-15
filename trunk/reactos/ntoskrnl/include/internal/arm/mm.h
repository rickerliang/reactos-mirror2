#ifndef __NTOSKRNL_INCLUDE_INTERNAL_ARM_MM_H
#define __NTOSKRNL_INCLUDE_INTERNAL_ARM_MM_H

//
// Number of bits corresponding to the area that a PDE entry represents (1MB)
//
#define PDE_SHIFT 20
#define PDE_SIZE  (1 << PDE_SHIFT)


//
// FIXFIX: This is all wrong now!!!
//


//
// Number of bits corresponding to the area that a coarse page table entry represents (4KB)
//
#define PTE_SHIFT 12
//#define PAGE_SIZE (1 << PTE_SHIFT) // FIXME: This conflicts with ndk/arm/mmtypes.h which does #define PAGE_SIZE 0x1000 -- use PTE_SIZE here instead?

//
// Number of bits corresponding to the area that a coarse page table occupies (1KB)
//
#define CPT_SHIFT 10
#define CPT_SIZE  (1 << CPT_SHIFT)



//
// FIXFIX: This is all wrong now!!!
//
typedef union _ARM_PTE
{
    union
    {
        struct
        {
            ULONG Type:2;
            ULONG Unused:30;
        } Fault;
        struct
        {
            ULONG Type:2;
            ULONG Ignored:2;
            ULONG Reserved:1;
            ULONG Domain:4;
            ULONG Ignored1:1;
            ULONG BaseAddress:22;
        } Coarse;
        struct
        {
            ULONG Type:2;
            ULONG Buffered:1;
            ULONG Cached:1;
            ULONG Reserved:1;
            ULONG Domain:4;
            ULONG Ignored:1;
            ULONG Access:2;
            ULONG Ignored1:8;
            ULONG BaseAddress:12;
        } Section;
        struct
        {
            ULONG Type:2;
            ULONG Reserved:3;
            ULONG Domain:4;
            ULONG Ignored:3;
            ULONG BaseAddress:20;
        } Fine;
    } L1;
    union
    {
        struct
        {
            ULONG Type:2;
            ULONG Unused:30;
        } Fault;
        struct
        {
            ULONG Type:2;
            ULONG Buffered:1;
            ULONG Cached:1;
            ULONG Access0:2;
            ULONG Access1:2;
            ULONG Access2:2;
            ULONG Access3:2;
            ULONG Ignored:4;
            ULONG BaseAddress:16;
        } Large;
        struct
        {
            ULONG Type:2;
            ULONG Buffered:1;
            ULONG Cached:1;
            ULONG Access0:2;
            ULONG Access1:2;
            ULONG Access2:2;
            ULONG Access3:2;
            ULONG BaseAddress:20;
        } Small;
        struct
        {
            ULONG Type:2;
            ULONG Buffered:1;
            ULONG Cached:1;
            ULONG Access0:2;
            ULONG Ignored:4;
            ULONG BaseAddress:22;
        } Tiny; 
    } L2;
    ULONG AsUlong;
} ARM_PTE, *PARM_PTE;

typedef struct _ARM_TRANSLATION_TABLE
{
    ARM_PTE Pte[4096];
} ARM_TRANSLATION_TABLE, *PARM_TRANSLATION_TABLE;

typedef struct _ARM_COARSE_PAGE_TABLE
{
    ARM_PTE Pte[256];
    ULONG Padding[768];
} ARM_COARSE_PAGE_TABLE, *PARM_COARSE_PAGE_TABLE;

typedef enum _ARM_L1_PTE_TYPE
{
    FaultPte,
    CoarsePte,
    SectionPte,
    FinePte
} ARM_L1_PTE_TYPE;

typedef enum _ARM_L2_PTE_TYPE
{
    LargePte = 1,
    SmallPte,
    TinyPte
} ARM_L2_PTE_TYPE;

typedef enum _ARM_PTE_ACCESS
{
    FaultAccess,
    SupervisorAccess,
    SharedAccess,
    UserAccess
} ARM_PTE_ACCESS;

typedef enum _ARM_DOMAIN
{
    FaultDomain,
    ClientDomain,
    InvalidDomain,
    ManagerDomain
} ARM_DOMAIN;


//
// FIXFIX: This is all wrong now!!!
//

//
// Take 0x80812345 and extract:
// PTE_BASE[0x808][0x12]
//
#define MiGetPteAddress(x)         \
    (PMMPTE)(PTE_BASE + \
             (((ULONG)(x) >> 20) << 12) + \
             ((((ULONG)(x) >> 12) & 0xFF) << 2))

#define MiGetPdeAddress(x)         \
    (PMMPDE_HARDWARE)(PDE_BASE + \
             (((ULONG)(x) >> 20) << 2))

#define MiGetPdeOffset(x) (((ULONG)(x)) >> 22)

#define PTE_BASE    0xC0000000
#define PTE_TOP    0xC03FFFFF
#define PDE_BASE    0xC1000000
#define HYPER_SPACE 0xC1100000

struct _EPROCESS;
PULONG MmGetPageDirectory(VOID);


//
// FIXME: THESE ARE WRONG ATM.
//
#define MiAddressToPde(x) \
((PMMPTE)(((((ULONG)(x)) >> 22) << 2) + PDE_BASE))
#define MiAddressToPte(x) \
((PMMPTE)(((((ULONG)(x)) >> 12) << 2) + PTE_BASE))
#define MiAddressToPteOffset(x) \
((((ULONG)(x)) << 10) >> 22)

//
// Convert a PTE into a corresponding address
//
#define MiPteToAddress(PTE) ((PVOID)((ULONG)(PTE) << 10))

#define ADDR_TO_PAGE_TABLE(v) (((ULONG)(v)) / (1024 * PAGE_SIZE))
#define ADDR_TO_PDE_OFFSET(v) ((((ULONG)(v)) / (1024 * PAGE_SIZE)))
#define ADDR_TO_PTE_OFFSET(v)  ((((ULONG)(v)) % (1024 * PAGE_SIZE)) / PAGE_SIZE)

#define MI_MAKE_LOCAL_PAGE(x)      ((x)->u.Hard.NonGlobal = 1)
#define MI_MAKE_DIRTY_PAGE(x)      
#define MI_MAKE_OWNER_PAGE(x)      ((x)->u.Hard.Access = 1) // FIXFIX
#define MI_MAKE_WRITE_PAGE(x)      ((x)->u.Hard.ExtendedAccess = 1) // FIXFIX
#define MI_PAGE_DISABLE_CACHE(x)   ((x)->u.Hard.Cached = 0)
#define MI_PAGE_WRITE_THROUGH(x)   ((x)->u.Hard.Buffered = 0)
#define MI_PAGE_WRITE_COMBINED(x)  ((x)->u.Hard.Buffered = 1)
#define MI_IS_PAGE_WRITEABLE(x)    ((x)->u.Hard.ExtendedAccess == 0)
#define MI_IS_PAGE_COPY_ON_WRITE(x)FALSE
#define MI_IS_PAGE_DIRTY(x)        TRUE

/* Easy accessing PFN in PTE */
#define PFN_FROM_PTE(v) ((v)->u.Hard.PageFrameNumber)

#endif
