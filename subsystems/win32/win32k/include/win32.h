#ifndef __INCLUDE_NAPI_WIN32_H
#define __INCLUDE_NAPI_WIN32_H

/* W32PROCESS flags */
#define W32PF_CONSOLEAPPLICATION      0x00000001
#define W32PF_FORCEOFFFEEDBACK        0x00000002
#define W32PF_STARTGLASS              0x00000004
#define W32PF_WOW                     0x00000008
#define W32PF_READSCREENACCESSGRANTED 0x00000010
#define W32PF_INITIALIZED             0x00000020
#define W32PF_APPSTARTING             0x00000040
#define W32PF_WOW64                   0x00000080
#define W32PF_ALLOWFOREGROUNDACTIVATE 0x00000100
#define W32PF_OWNDCCLEANUP            0x00000200
#define W32PF_SHOWSTARTGLASSCALLED    0x00000400
#define W32PF_FORCEBACKGROUNDPRIORITY 0x00000800
#define W32PF_TERMINATED              0x00001000
#define W32PF_CLASSESREGISTERED       0x00002000
#define W32PF_THREADCONNECTED         0x00004000
#define W32PF_PROCESSCONNECTED        0x00008000
#define W32PF_WAKEWOWEXEC             0x00010000
#define W32PF_WAITFORINPUTIDLE        0x00020000
#define W32PF_IOWINSTA                0x00040000
#define W32PF_CONSOLEFOREGROUND       0x00080000
#define W32PF_OLELOADED               0x00100000
#define W32PF_SCREENSAVER             0x00200000
#define W32PF_IDLESCREENSAVER         0x00400000
#define W32PF_ICONTITLEREGISTERED     0x10000000
// ReactOS
#define W32PF_NOWINDOWGHOSTING       (0x01000000)
#define W32PF_MANUALGUICHECK         (0x02000000)
#define W32PF_CREATEDWINORDC         (0x04000000)

/* THREADINFO Flags */
#define TIF_INCLEANUP               0x00000001
#define TIF_16BIT                   0x00000002
#define TIF_SYSTEMTHREAD            0x00000004
#define TIF_CSRSSTHREAD             0x00000008
#define TIF_TRACKRECTVISIBLE        0x00000010
#define TIF_ALLOWFOREGROUNDACTIVATE 0x00000020
#define TIF_DONTATTACHQUEUE         0x00000040
#define TIF_DONTJOURNALATTACH       0x00000080
#define TIF_WOW64                   0x00000100
#define TIF_INACTIVATEAPPMSG        0x00000200
#define TIF_SPINNING                0x00000400
#define TIF_PALETTEAWARE            0x00000800
#define TIF_SHAREDWOW               0x00001000
#define TIF_FIRSTIDLE               0x00002000
#define TIF_WAITFORINPUTIDLE        0x00004000
#define TIF_MOVESIZETRACKING        0x00008000
#define TIF_VDMAPP                  0x00010000
#define TIF_DOSEMULATOR             0x00020000
#define TIF_GLOBALHOOKER            0x00040000
#define TIF_DELAYEDEVENT            0x00080000
#define TIF_MSGPOSCHANGED           0x00100000
#define TIF_SHUTDOWNCOMPLETE        0x00200000
#define TIF_IGNOREPLAYBACKDELAY     0x00400000
#define TIF_ALLOWOTHERACCOUNTHOOK   0x00800000
#define TIF_GUITHREADINITIALIZED    0x02000000
#define TIF_DISABLEIME              0x04000000
#define TIF_INGETTEXTLENGTH         0x08000000
#define TIF_ANSILENGTH              0x10000000
#define TIF_DISABLEHOOKS            0x20000000

extern BOOL ClientPfnInit;
extern HINSTANCE hModClient;
extern HANDLE hModuleWin;    // This Win32k Instance.
extern PCLS SystemClassList;
extern BOOL RegisteredSysClasses;

typedef struct _WIN32HEAP WIN32HEAP, *PWIN32HEAP;

#include <pshpack1.h>
// FIXME! Move to ntuser.h
typedef struct _TL
{
    struct _TL* next;
    PVOID pobj;
    PVOID pfnFree;
} TL, *PTL;

typedef struct _W32THREAD
{
    PETHREAD pEThread;
    ULONG RefCount;
    PTL ptlW32;
    PVOID pgdiDcattr;
    PVOID pgdiBrushAttr;
    PVOID pUMPDObjs;
    PVOID pUMPDHeap;
    DWORD dwEngAcquireCount;
    PVOID pSemTable;
    PVOID pUMPDObj;
} W32THREAD, *PW32THREAD;

typedef struct _THREADINFO
{
    W32THREAD;
    PTL                 ptl;
    PPROCESSINFO        ppi;
    struct _USER_MESSAGE_QUEUE* MessageQueue;
    struct _KBL*        KeyboardLayout;
    PCLIENTTHREADINFO   pcti;
    struct _DESKTOP*    Desktop;
    PDESKTOPINFO        pDeskInfo;
    PCLIENTINFO         pClientInfo;
    FLONG               TIF_flags;
    LONG                timeLast;
    HANDLE              hDesktop;
    UINT                cPaintsReady; /* Count of paints pending. */
    UINT                cTimersReady; /* Count of timers pending. */
    ULONG               fsHooks;
    PHOOK               sphkCurrent;
    LIST_ENTRY          PtiLink;

    CLIENTTHREADINFO    cti;  // Used only when no Desktop or pcti NULL.
  /* ReactOS */
  LIST_ENTRY WindowListHead;
  LIST_ENTRY W32CallbackListHead;
  BOOLEAN IsExiting;          // Use TIF_INCLEANUP
  SINGLE_LIST_ENTRY  ReferencesList;
} THREADINFO;

#include <poppack.h>

typedef struct _W32HEAP_USER_MAPPING
{
    struct _W32HEAP_USER_MAPPING *Next;
    PVOID KernelMapping;
    PVOID UserMapping;
    ULONG_PTR Limit;
    ULONG Count;
} W32HEAP_USER_MAPPING, *PW32HEAP_USER_MAPPING;

typedef struct _W32PROCESS
{
  PEPROCESS     peProcess;
  DWORD         RefCount;
  ULONG         W32PF_flags;
  PKEVENT       InputIdleEvent;
  DWORD         StartCursorHideTime;
  struct _W32PROCESS * NextStart;
  PVOID         pDCAttrList;
  PVOID         pBrushAttrList;
  DWORD         W32Pid;
  LONG          GDIHandleCount;
  LONG          UserHandleCount;
  PEX_PUSH_LOCK GDIPushLock;  /* Locking Process during access to structure. */
  RTL_AVL_TABLE GDIEngUserMemAllocTable;  /* Process AVL Table. */
  LIST_ENTRY    GDIDcAttrFreeList;
  LIST_ENTRY    GDIBrushAttrFreeList;
} W32PROCESS, *PW32PROCESS;

typedef struct _PROCESSINFO
{
  W32PROCESS;

  PCLS pclsPrivateList;
  PCLS pclsPublicList;

  DWORD dwRegisteredClasses;
  /* ReactOS */
  LIST_ENTRY ClassList;
  LIST_ENTRY MenuListHead;
  FAST_MUTEX PrivateFontListLock;
  LIST_ENTRY PrivateFontListHead;
  FAST_MUTEX DriverObjListLock;
  LIST_ENTRY DriverObjListHead;
  struct _KBL* KeyboardLayout; // THREADINFO only
  W32HEAP_USER_MAPPING HeapMappings;
} PROCESSINFO;

#endif /* __INCLUDE_NAPI_WIN32_H */
