#ifndef __INCLUDE_DDK_PSTYPES_H
#define __INCLUDE_DDK_PSTYPES_H

#include <kernel32/atom.h>
#include <internal/hal.h>

#ifndef TLS_MINIMUM_AVAILABLE
#define TLS_MINIMUM_AVAILABLE 	(64)
#endif
#ifndef MAX_PATH
#define MAX_PATH 	(260)
#endif

typedef NTSTATUS (*PKSTART_ROUTINE)(PVOID StartContext);

typedef struct _STACK_INFORMATION
{
	PVOID 	BaseAddress;
	PVOID	UpperAddress;
} STACK_INFORMATION, *PSTACK_INFORMATION;

typedef struct linux_sigcontext {
        int     sc_gs;
        int     sc_fs;
        int     sc_es;
        int     sc_ds;
        int     sc_edi;
        int     sc_esi;
        int     sc_ebp;
        int     sc_esp;
        int     sc_ebx;
        int     sc_edx;
        int     sc_ecx;
        int     sc_eax;
        int     sc_trapno;
        int     sc_err;
        int     sc_eip;
        int     sc_cs;
        int     sc_eflags;
        int     sc_esp_at_signal;
        int     sc_ss;
        int     sc_387;
        int     sc_mask;
        int     sc_cr2;
} TRAP_FRAME, *PTRAP_FRAME;

typedef ULONG THREADINFOCLASS;

typedef struct _CURDIR
{
	UNICODE_STRING	DosPath;
	HANDLE		Handle;
} CURDIR, *PCURDIR;

typedef struct _RTL_DRIVE_LETTER_CURDIR
{
	USHORT	Flags;
	USHORT	Length;
	ULONG	TimeStamp;
	UNICODE_STRING	DosPath;
} RTL_DRIVE_LETTER_CURDIR, *PRTL_DRIVE_LETTER_CURDIR;

typedef struct _RTL_USER_PROCESS_PARAMETERS
{
	ULONG			MaximumLength;		// 00h
	ULONG			Length;			// 04h
	ULONG			Flags;			// 08h
	ULONG			DebugFlags;		// 0Ch
	HANDLE			ConsoleHandle;		// 10h
	ULONG			ConsoleFlags;		// 14h
	HANDLE			StandardInput;		// 18h
	HANDLE			StandardOutput;		// 1Ch
	HANDLE			StandardError;		// 20h
	CURDIR			CurrentDirectory;	// 24h
	UNICODE_STRING		DllPath;		// 30h
	UNICODE_STRING		ImagePathName;		// 38h
	UNICODE_STRING		CommandLine;		// 40h
	PVOID			Environment;		// 48h
	ULONG			StartingX;		// 4Ch
	ULONG			StartingY;		// 50h
	ULONG			CountX;			// 54h
	ULONG			CountY;			// 58h
	ULONG			CountCharsX;		// 5Ch
	ULONG			CountCharsY;		// 60h
	ULONG			FillAttribute;		// 64h
	ULONG			WindowFlags;		// 68h
	ULONG			ShowWindowFlags;	// 6Ch
	UNICODE_STRING		WindowTitle;		// 70h
	UNICODE_STRING		DesktopInfo;		// 78h
	UNICODE_STRING		ShellInfo;		// 80h
	UNICODE_STRING		RuntimeData;		// 88h
	RTL_DRIVE_LETTER_CURDIR	DLCurrentDirectory[32];	// 90h
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;


typedef struct _LDR
{
	UCHAR	Initialized;
	UCHAR	InInitializationOrderModuleList;
	PVOID	InLoadOrderModuleList;
	PVOID	InMemoryOrderModuleList;
} LDR, *PLDR;


typedef struct _PEB
{
	UCHAR			InheritedAddressSpace;      // 00
	UCHAR			ReadImageFileExecOptions;   // 01h
	UCHAR			BeingDebugged;              // 02h
	LONG			ImageBaseAddress;           // 03h
	LDR			Ldr;                        // 07h

	PRTL_USER_PROCESS_PARAMETERS	ProcessParameters;	// 10h

	WORD			NumberOfProcessors;         // 11h
	WORD			NtGlobalFlag;               // 13h

	HANDLE			ProcessHeap;                // 19h
	ATOMTABLE		LocalAtomTable;             // 1Dh
	LPCRITICAL_SECTION	CriticalSection;            // 35h
	DWORD			CriticalSectionTimeout;     // 39h
	WORD			MajorVersion;               // 3Dh
	WORD			MinorVersion;               // 3Fh
	WORD			BuildNumber;                // 41h
	WORD			PlatformId;                 // 43h
} PEB, *PPEB;

typedef struct _CLIENT_ID
{
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

typedef struct _NT_TIB {
    struct _EXCEPTION_REGISTRATION_RECORD* ExceptionList;  // 00h
    PVOID StackBase;                                       // 04h
    PVOID StackLimit;                                      // 08h
    PVOID SubSystemTib;                                    // 0Ch
    union {
        PVOID FiberData;                                   // 10h
        ULONG Version;                                     // 10h
    } Fib;
    PVOID ArbitraryUserPointer;                            // 14h
    struct _NT_TIB *Self;                                  // 18h
} NT_TIB, *PNT_TIB;

typedef struct _NT_TEB
{
	NT_TIB			Tib;              // 00h
	ULONG			reserved1;        // 1Ch  ???
	CLIENT_ID		Cid;              // 20h
	ULONG			reserved2;        // 28h  ???
	ULONG			reserved3;        // 2Ch  ???
	PPEB			Peb;             // 30h 
	DWORD			LastErrorCode;    // 34h

	HANDLE			RPCHandle;        // 36
	PVOID			TlsData[TLS_MINIMUM_AVAILABLE]; // 40
	DWORD			dwTlsIndex;       // 230
	NTSTATUS		LastStatusValue;  // 242
	DWORD			LockCount;        // 244
	UCHAR			HardErrorMode;    // 248

	/* reactos only ??? (Eric Kohl) */
	PVOID			StackCommit;
	PVOID			StackCommitMax;
	PVOID			StackReserved;
} NT_TEB, *PNT_TEB;

struct _KPROCESS;

typedef struct _KAPC_STATE
{
   LIST_ENTRY ApcListHead[2];
   struct _KPROCESS* Process;
   ULONG KernelApcInProgress;
   ULONG KernelApcPending;
   USHORT UserApcPending;
} KAPC_STATE, *PKAPC_STATE;

typedef struct _KTHREAD
{
   DISPATCHER_HEADER DispatcherHeader;    // For waiting for the thread
   LIST_ENTRY        MutantListHead;
   PVOID             InitialStack;
   ULONG             StackLimit;
   NT_TEB*           Teb;
   PVOID             TlsArray;
   PVOID             KernelStack;
   UCHAR             DebugActive;
   UCHAR             State;
   UCHAR             Alerted[2];
   UCHAR             Iopl;
   UCHAR             NpxState;
   UCHAR             Saturation;
   KPRIORITY         Priority;
   KAPC_STATE        ApcState;
   ULONG             ContextSwitches;
   ULONG             WaitStatus;
   KIRQL             WaitIrql;
   ULONG             WaitMode;
   UCHAR             WaitNext;
   UCHAR             WaitReason;
   PKWAIT_BLOCK      WaitBlockList;
   LIST_ENTRY        WaitListEntry;
   ULONG             WaitTime;
   KPRIORITY         BasePriority;
   UCHAR             DecrementCount;
   UCHAR             PriorityDecrement;
   UCHAR             Quantum;
   KWAIT_BLOCK       WaitBlock[4];
   PVOID             LegoData;         // ??
   LONG              KernelApcDisable;
   KAFFINITY         UserAffinity;
   UCHAR             SystemAffinityActive;
   UCHAR             Pad;
   PKQUEUE           Queue;    
   KSPIN_LOCK        ApcQueueLock;
   KTIMER            Timer;
   LIST_ENTRY        QueueListEntry;
   KAFFINITY         Affinity;
   UCHAR             Preempted;
   UCHAR             ProcessReadyQueue;
   UCHAR             KernelStackResident;
   UCHAR             NextProcessor;
   PVOID             CallbackStack;
   BOOL              Win32Thread;
   PVOID             TrapFrame;
   PVOID             ApcStatePointer;      // Is actually eight bytes
   UCHAR             EnableStackSwap;
   UCHAR             LargeStack;
   UCHAR             ResourceIndex;
   UCHAR             PreviousMode;
   TIME              KernelTime;
   TIME              UserTime;
   KAPC_STATE        SavedApcState;
   UCHAR             Alertable;
   UCHAR             ApcQueueable;
   ULONG             AutoAlignment;
   PVOID             StackBase;
   KAPC              SuspendApc;
   KSEMAPHORE        SuspendSemaphore;
   LIST_ENTRY        ThreadListEntry;
   CHAR             FreezeCount;
   ULONG             SuspendCount;
   UCHAR             IdealProcessor;
   UCHAR             DisableBoost;
   LIST_ENTRY        ProcessThreadListEntry;        // Added by Phillip Susi for list of threads in a process

   /* Provisionally added by David Welch */
   hal_thread_state                   Context;
   KDPC              TimerDpc;			// Added by Phillip Susi for internal KeAddThreadTimeout() impl.
} KTHREAD, *PKTHREAD;

// According to documentation the stack should have a commited [ 1 page ] and
// a reserved part [ 1 M ] but can be specified otherwise in the image file.

typedef struct _INITIAL_TEB
{
	PVOID StackBase;
	PVOID StackLimit;
	PVOID StackCommit;
	PVOID StackCommitMax;
	PVOID StackReserved;
} INITIAL_TEB, *PINITIAL_TEB;






// TopLevelIrp can be one of the following values:
// FIXME I belong somewhere else

#define 	FSRTL_FSP_TOP_LEVEL_IRP			(0x01)
#define 	FSRTL_CACHE_TOP_LEVEL_IRP		(0x02)
#define 	FSRTL_MOD_WRITE_TOP_LEVEL_IRP		(0x03)
#define		FSRTL_FAST_IO_TOP_LEVEL_IRP		(0x04)
#define		FSRTL_MAX_TOP_LEVEL_IRP_FLAG		(0x04)

typedef struct _TOP_LEVEL_IRP
{
	PIRP TopLevelIrp;
	ULONG TopLevelIrpConst;
} TOP_LEVEL_IRP;

typedef struct
{
   PACCESS_TOKEN Token;                              // 0x0
   UCHAR Unknown1;                                   // 0x4
   UCHAR Unknown2;                                   // 0x5
   UCHAR Pad[2];                                     // 0x6
   SECURITY_IMPERSONATION_LEVEL Level;               // 0x8
} PS_IMPERSONATION_INFO, *PPS_IMPERSONATION_INFO;

typedef struct _ETHREAD 
{
   KTHREAD			Tcb;
   TIME			CreateTime;
   TIME			ExitTime;
   NTSTATUS		ExitStatus;
   LIST_ENTRY		PostBlockList;
   LIST_ENTRY		TerminationPortList;  
   ULONG			ActiveTimerListLock;
   PVOID			ActiveTimerListHead;
   CLIENT_ID		Cid;
   PLARGE_INTEGER		LpcReplySemaphore;
   PVOID			LpcReplyMessage;
   PLARGE_INTEGER		LpcReplyMessageId;
   PPS_IMPERSONATION_INFO     ImpersonationInfo;
   LIST_ENTRY		IrpList; //
   TOP_LEVEL_IRP		TopLevelIrp;
   ULONG			ReadClusterSize;
	UCHAR			ForwardClusterOnly;
	UCHAR			DisablePageFaultClustering;
	UCHAR			DeadThread;
	UCHAR			HasTerminated;
	ACCESS_MASK		GrantedAccess;
	struct _EPROCESS*	ThreadsProcess;
	PKSTART_ROUTINE		StartAddress;
	LPTHREAD_START_ROUTINE  Win32StartAddress; // Should Specify a win32 start func
	UCHAR 			LpcExitThreadCalled;
        UCHAR 			HardErrorsAreDisabled;
        UCHAR                   LpcReceivedMsgIdValid;
        UCHAR                   ActiveImpersonationInfo;
        ULONG                   PerformanceCountHigh;

   /*
    * Added by David Welch (welch@cwcom.net)
    */
   struct _EPROCESS* OldProcess;

} ETHREAD, *PETHREAD;


typedef struct _KPROCESS 
{
   DISPATCHER_HEADER 	DispatcherHeader;
   PVOID		PageTableDirectory; // FIXME: I should point to a PTD
   TIME			ElapsedTime;
   TIME			KernelTime;
   TIME			UserTime;
   LIST_ENTRY		InMemoryList;  
   LIST_ENTRY		SwappedOutList;   	
   KSPIN_LOCK		SpinLock;
   KAFFINITY		Affinity;
   ULONG		StackCount;
   KPRIORITY		BasePriority;
   ULONG		DefaultThreadQuantum;
   UCHAR		ProcessState;
   ULONG		ThreadSeed;
   UCHAR		DisableBoost;
   
   /*
    * Added by David Welch (welch@mcmail.com)
    */
   LIST_ENTRY           MemoryAreaList;
   HANDLE_TABLE         HandleTable;
   LIST_ENTRY           ProcessListEntry;

   LIST_ENTRY           ThreadListHead;     // Added by Phillip Susi for list of threads in process

} KPROCESS, *PKPROCESS;

typedef struct _EPROCESS
{
   KPROCESS Pcb;
   NTSTATUS ExitStatus;
   KEVENT LockEvent;
   ULONG LockCount;
   TIME CreateTime;
   TIME ExitTime;
   PVOID LockOwner;
   ULONG UniqueProcessId;
   LIST_ENTRY ActiveProcessLinks;
   ULONG QuotaPeakPoolUsage[2];
   ULONG QuotaPoolUsage[2];
   ULONG PagefileUsage;
   ULONG CommitCharge;
   ULONG PeakPagefileUsage;
   ULONG PeakVirtualUsage;
   LARGE_INTEGER VirtualSize;
   PVOID Vm;                // Actually 48 bytes
   PVOID LastProtoPteFault;
   PVOID DebugPort;
   PVOID ExceptionPort;
   PVOID ObjectTable;
   PVOID Token;
   KMUTEX WorkingSetLock;
   PVOID WorkingSetPage;
   UCHAR ProcessOutswapEnabled;
   UCHAR ProcessOutswapped;
   UCHAR AddressSpaceInitialized;
   UCHAR AddressSpaceDeleted;
   KMUTEX AddressCreationLock;
   PVOID ForkInProgress;
   PVOID VmOperation;
   PKEVENT VmOperationEvent;
   PVOID PageDirectoryPte;
   LARGE_INTEGER LastFaultCount;
   PVOID VadRoot;
   PVOID VadHint;
   PVOID CloneRoot;
   ULONG NumberOfPrivatePages;
   ULONG NumberOfLockedPages;
   UCHAR ForkWasSuccessFul;
   UCHAR ExitProcessCalled;
   UCHAR CreateProcessReported;
   HANDLE SectionHandle;
   PPEB Peb;
   PVOID SectionBaseAddress;
   PVOID QuotaBlock;
   NTSTATUS LastThreadExitStatus;
   LARGE_INTEGER WorkingSetWatch;         //
   ULONG InheritedFromUniqueProcessId;
   ACCESS_MASK GrantedAccess;
   ULONG DefaultHardErrorProcessing;
   PVOID LdtInformation;
   ULONG VadFreeHint;
   PVOID VdmObjects;
   KMUTANT ProcessMutant;
   CHAR ImageFileName[16];
   LARGE_INTEGER VmTrimFaultValue;
   PVOID Win32Process;        // Actually 12 bytes
   PVOID Win32WindowStation;
} EPROCESS, *PEPROCESS;

#define PROCESS_STATE_TERMINATED (1)
#define PROCESS_STATE_ACTIVE     (2)

#endif /* __INCLUDE_DDK_PSTYPES_H */
