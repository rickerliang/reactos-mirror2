/*
 *  COPYRIGHT:        See COPYING in the top level directory
 *  PROJECT:          ReactOS Win32k subsystem
 *  PURPOSE:          Desktops
 *  FILE:             subsystems/win32/win32k/ntuser/desktop.c
 *  PROGRAMER:        Casper S. Hornstrup (chorns@users.sourceforge.net)
 */

/* INCLUDES ******************************************************************/

#include <win32k.h>
DBG_DEFAULT_CHANNEL(UserDesktop);

static
VOID
IntFreeDesktopHeap(
    IN OUT PDESKTOP Desktop
);

/* GLOBALS *******************************************************************/

/* Currently active desktop */
PDESKTOP InputDesktop = NULL;
HDESK InputDesktopHandle = NULL;
HDC ScreenDeviceContext = NULL;

/* OBJECT CALLBACKS **********************************************************/

NTSTATUS
APIENTRY
IntDesktopObjectParse(IN PVOID ParseObject,
                      IN PVOID ObjectType,
                      IN OUT PACCESS_STATE AccessState,
                      IN KPROCESSOR_MODE AccessMode,
                      IN ULONG Attributes,
                      IN OUT PUNICODE_STRING CompleteName,
                      IN OUT PUNICODE_STRING RemainingName,
                      IN OUT PVOID Context OPTIONAL,
                      IN PSECURITY_QUALITY_OF_SERVICE SecurityQos OPTIONAL,
                      OUT PVOID *Object)
{
    NTSTATUS Status;
    PDESKTOP Desktop;
    OBJECT_ATTRIBUTES ObjectAttributes;
    PLIST_ENTRY NextEntry, ListHead;
    PWINSTATION_OBJECT WinStaObject = (PWINSTATION_OBJECT)ParseObject;
    PUNICODE_STRING DesktopName;
    PBOOLEAN pContext = (PBOOLEAN) Context;

    if(pContext)
        *pContext = FALSE;

    /* Set the list pointers and loop the window station */
    ListHead = &WinStaObject->DesktopListHead;
    NextEntry = ListHead->Flink;
    while (NextEntry != ListHead)
    {
        /* Get the current desktop */
        Desktop = CONTAINING_RECORD(NextEntry, DESKTOP, ListEntry);

        /* Get its name */
        DesktopName = GET_DESKTOP_NAME(Desktop);
        if (DesktopName)
        {
            /* Compare the name */
            if (RtlEqualUnicodeString(RemainingName,
                                      DesktopName,
                                      (Attributes & OBJ_CASE_INSENSITIVE)))
            {
                /* We found a match. Did this come from a create? */
                if (Context)
                {
                    /* Unless OPEN_IF was given, fail with an error */
                    if (!(Attributes & OBJ_OPENIF))
                    {
                        /* Name collision */
                        return STATUS_OBJECT_NAME_COLLISION;
                    }
                    else
                    {
                        /* Otherwise, return with a warning only */
                        Status = STATUS_OBJECT_NAME_EXISTS;
                    }
                }
                else
                {
                    /* This was a real open, so this is OK */
                    Status = STATUS_SUCCESS;
                }

                /* Reference the desktop and return it */
                ObReferenceObject(Desktop);
                *Object = Desktop;
                return Status;
            }
        }

        /* Go to the next desktop */
        NextEntry = NextEntry->Flink;
    }

    /* If we got here but this isn't a create, then fail */
    if (!Context) return STATUS_OBJECT_NAME_NOT_FOUND;

    /* Create the desktop object */
    InitializeObjectAttributes(&ObjectAttributes, RemainingName, 0, NULL, NULL);
    Status = ObCreateObject(KernelMode,
                            ExDesktopObjectType,
                            &ObjectAttributes,
                            KernelMode,
                            NULL,
                            sizeof(DESKTOP),
                            0,
                            0,
                            (PVOID)&Desktop);
    if (!NT_SUCCESS(Status)) return Status;

    /* Initialize shell hook window list and set the parent */
    RtlZeroMemory(Desktop, sizeof(DESKTOP));
    InitializeListHead(&Desktop->ShellHookWindows);
    Desktop->rpwinstaParent = (PWINSTATION_OBJECT)ParseObject;

    /* Put the desktop on the window station's list of associated desktops */
    InsertTailList(&Desktop->rpwinstaParent->DesktopListHead,
                   &Desktop->ListEntry);

    /* Set the desktop object and return success */
    *Object = Desktop;
    *pContext = TRUE;
    return STATUS_SUCCESS;
}

VOID APIENTRY
IntDesktopObjectDelete(PWIN32_DELETEMETHOD_PARAMETERS Parameters)
{
   PDESKTOP Desktop = (PDESKTOP)Parameters->Object;

   TRACE("Deleting desktop object 0x%x\n", Desktop);

   /* Remove the desktop from the window station's list of associcated desktops */
   RemoveEntryList(&Desktop->ListEntry);

   IntFreeDesktopHeap(Desktop);
}

NTSTATUS NTAPI
IntDesktopOkToClose(PWIN32_OKAYTOCLOSEMETHOD_PARAMETERS Parameters)
{
    PTHREADINFO pti;

    pti = PsGetCurrentThreadWin32Thread();

    if( pti == NULL)
    {
        /* This happens when we leak desktop handles */
        return STATUS_SUCCESS;
    }

    /* Do not allow the current desktop or the initial desktop to be closed */
    if( Parameters->Handle == pti->ppi->hdeskStartup ||
        Parameters->Handle == pti->hdesk)
    {
        return STATUS_ACCESS_DENIED;
    }

    return STATUS_SUCCESS;
}

/* PRIVATE FUNCTIONS **********************************************************/

INIT_FUNCTION
NTSTATUS
NTAPI
InitDesktopImpl(VOID)
{
    GENERIC_MAPPING IntDesktopMapping = { DESKTOP_READ,
                                          DESKTOP_WRITE,
                                          DESKTOP_EXECUTE,
                                          DESKTOP_ALL_ACCESS};

    /* Set Desktop Object Attributes */
    ExDesktopObjectType->TypeInfo.DefaultNonPagedPoolCharge = sizeof(DESKTOP);
    ExDesktopObjectType->TypeInfo.GenericMapping = IntDesktopMapping;
    ExDesktopObjectType->TypeInfo.ValidAccessMask = DESKTOP_ALL_ACCESS;
    return STATUS_SUCCESS;
}

static int GetSystemVersionString(LPWSTR buffer)
{
   RTL_OSVERSIONINFOEXW versionInfo;
   int len;

   versionInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);

   if (!NT_SUCCESS(RtlGetVersion((PRTL_OSVERSIONINFOW)&versionInfo)))
      return 0;

   if (versionInfo.dwMajorVersion <= 4)
      len = swprintf(buffer,
                     L"ReactOS Version %d.%d %s Build %d",
                     versionInfo.dwMajorVersion, versionInfo.dwMinorVersion,
                     versionInfo.szCSDVersion, versionInfo.dwBuildNumber&0xFFFF);
   else
      len = swprintf(buffer,
                     L"ReactOS %s (Build %d)",
                     versionInfo.szCSDVersion, versionInfo.dwBuildNumber&0xFFFF);

   return len;
}


NTSTATUS FASTCALL
IntParseDesktopPath(PEPROCESS Process,
                    PUNICODE_STRING DesktopPath,
                    HWINSTA *hWinSta,
                    HDESK *hDesktop)
{
   OBJECT_ATTRIBUTES ObjectAttributes;
   UNICODE_STRING ObjectName;
   NTSTATUS Status;
   WCHAR wstrWinstaFullName[MAX_PATH], *pwstrWinsta = NULL, *pwstrDesktop = NULL;

   ASSERT(hWinSta);
   ASSERT(hDesktop);
   ASSERT(DesktopPath);

   *hWinSta = NULL;
   *hDesktop = NULL;

   if(DesktopPath->Buffer != NULL && DesktopPath->Length > sizeof(WCHAR))
   {
      /*
       * Parse the desktop path string which can be in the form "WinSta\Desktop"
       * or just "Desktop". In latter case WinSta0 will be used.
       */

      pwstrDesktop = wcschr(DesktopPath->Buffer, L'\\');
      if(pwstrDesktop != NULL)
      {
          *pwstrDesktop = 0; 
          pwstrDesktop++;
          pwstrWinsta = DesktopPath->Buffer;
      }
      else
      {
          pwstrDesktop = DesktopPath->Buffer;
          pwstrWinsta = NULL;
      } 

      TRACE("IntParseDesktopPath pwstrWinsta:%S pwstrDesktop:%S\n", pwstrWinsta, pwstrDesktop);
   }

#if 0
   /* Search the process handle table for (inherited) window station
      handles, use a more appropriate one than WinSta0 if possible. */
   if (!ObFindHandleForObject(Process,
                              NULL,
                              ExWindowStationObjectType,
                              NULL,
                              (PHANDLE)hWinSta))
#endif
   {
       /* We had no luck searching for opened handles, use WinSta0 now */
       if(!pwstrWinsta)
           pwstrWinsta = L"WinSta0";
   }

#if 0
   /* Search the process handle table for (inherited) desktop
      handles, use a more appropriate one than Default if possible. */
   if (!ObFindHandleForObject(Process,
                              NULL,
                              ExDesktopObjectType,
                              NULL,
                              (PHANDLE)hDesktop))
#endif
   {
       /* We had no luck searching for opened handles, use Desktop now */
       if(!pwstrDesktop)
           pwstrDesktop = L"Default";
   }
   
   if(*hWinSta == NULL)
   {
       swprintf(wstrWinstaFullName, L"%wZ\\%ws", &gustrWindowStationsDir, pwstrWinsta);
       RtlInitUnicodeString( &ObjectName, wstrWinstaFullName);

       TRACE("parsed initial winsta: %wZ\n", &ObjectName);

      /* Open the window station */
      InitializeObjectAttributes(&ObjectAttributes,
                                 &ObjectName,
                                 OBJ_CASE_INSENSITIVE,
                                 NULL,
                                 NULL);

      Status = ObOpenObjectByName(&ObjectAttributes,
                                  ExWindowStationObjectType,
                                  KernelMode,
                                  NULL,
                                  WINSTA_ACCESS_ALL,
                                  NULL,
                                  (HANDLE*)hWinSta);

      if(!NT_SUCCESS(Status))
      {
         SetLastNtError(Status);
         ERR("Failed to reference window station %wZ PID: %d!\n", &ObjectName );
         return Status;
      }
   }

   if(*hDesktop == NULL)
   {
      RtlInitUnicodeString(&ObjectName, pwstrDesktop);

      TRACE("parsed initial desktop: %wZ\n", &ObjectName);

      /* Open the desktop object */
      InitializeObjectAttributes(&ObjectAttributes,
                                 &ObjectName,
                                 OBJ_CASE_INSENSITIVE,
                                 *hWinSta,
                                 NULL);

      Status = ObOpenObjectByName(&ObjectAttributes,
                                  ExDesktopObjectType,
                                  KernelMode,
                                  NULL,
                                  DESKTOP_ALL_ACCESS,
                                  NULL,
                                  (HANDLE*)hDesktop);

      if(!NT_SUCCESS(Status))
      {
         *hDesktop = NULL;
         NtClose(*hWinSta);
         *hWinSta = NULL;
         SetLastNtError(Status);
         ERR("Failed to reference desktop %wZ PID: %d!\n", &ObjectName);
         return Status;
      }
   }
   return STATUS_SUCCESS;
}

/*
 * IntValidateDesktopHandle
 *
 * Validates the desktop handle.
 *
 * Remarks
 *    If the function succeeds, the handle remains referenced. If the
 *    fucntion fails, last error is set.
 */

NTSTATUS FASTCALL
IntValidateDesktopHandle(
   HDESK Desktop,
   KPROCESSOR_MODE AccessMode,
   ACCESS_MASK DesiredAccess,
   PDESKTOP *Object)
{
   NTSTATUS Status;

   Status = ObReferenceObjectByHandle(
               Desktop,
               DesiredAccess,
               ExDesktopObjectType,
               AccessMode,
               (PVOID*)Object,
               NULL);

   TRACE("IntValidateDesktopHandle: handle:0x%x obj:0x%x access:0x%x Status:0x%x\n", 
            Desktop, *Object, DesiredAccess, Status);

   if (!NT_SUCCESS(Status))
      SetLastNtError(Status);

   return Status;
}

PDESKTOP FASTCALL
IntGetActiveDesktop(VOID)
{
   return InputDesktop;
}

/*
 * Returns or creates a handle to the desktop object
 */
HDESK FASTCALL
IntGetDesktopObjectHandle(PDESKTOP DesktopObject)
{
   NTSTATUS Status;
   HDESK Ret;

   ASSERT(DesktopObject);

   if (!ObFindHandleForObject(PsGetCurrentProcess(),
                              DesktopObject,
                              ExDesktopObjectType,
                              NULL,
                              (PHANDLE)&Ret))
   {
      Status = ObOpenObjectByPointer(DesktopObject,
                                     0,
                                     NULL,
                                     0,
                                     ExDesktopObjectType,
                                     UserMode,
                                     (PHANDLE)&Ret);
      if(!NT_SUCCESS(Status))
      {
         /* Unable to create a handle */
         ERR("Unable to create a desktop handle\n");
         return NULL;
      }
   }
   else
   {
       ERR("Got handle: %lx\n", Ret);
   }

   return Ret;
}

PUSER_MESSAGE_QUEUE FASTCALL
IntGetFocusMessageQueue(VOID)
{
   PDESKTOP pdo = IntGetActiveDesktop();
   if (!pdo)
   {
      TRACE("No active desktop\n");
      return(NULL);
   }
   return (PUSER_MESSAGE_QUEUE)pdo->ActiveMessageQueue;
}

VOID FASTCALL
IntSetFocusMessageQueue(PUSER_MESSAGE_QUEUE NewQueue)
{
   PUSER_MESSAGE_QUEUE Old;
   PDESKTOP pdo = IntGetActiveDesktop();
   if (!pdo)
   {
      TRACE("No active desktop\n");
      return;
   }
   if(NewQueue != NULL)
   {
      if(NewQueue->Desktop != NULL)
      {
         TRACE("Message Queue already attached to another desktop!\n");
         return;
      }
      IntReferenceMessageQueue(NewQueue);
      (void)InterlockedExchangePointer((PVOID*)&NewQueue->Desktop, pdo);
   }
   Old = (PUSER_MESSAGE_QUEUE)InterlockedExchangePointer((PVOID*)&pdo->ActiveMessageQueue, NewQueue);
   if(Old != NULL)
   {
      (void)InterlockedExchangePointer((PVOID*)&Old->Desktop, 0);
      IntDereferenceMessageQueue(Old);
   }
}

HWND FASTCALL IntGetDesktopWindow(VOID)
{
   PDESKTOP pdo = IntGetActiveDesktop();
   if (!pdo)
   {
      TRACE("No active desktop\n");
      return NULL;
   }
   return pdo->DesktopWindow;
}

PWND FASTCALL UserGetDesktopWindow(VOID)
{
   PDESKTOP pdo = IntGetActiveDesktop();

   if (!pdo)
   {
      TRACE("No active desktop\n");
      return NULL;
   }

   return UserGetWindowObject(pdo->DesktopWindow);
}

HWND FASTCALL IntGetMessageWindow(VOID)
{
   PDESKTOP pdo = IntGetActiveDesktop();

   if (!pdo)
   {
      TRACE("No active desktop\n");
      return NULL;
   }
   return pdo->spwndMessage->head.h;
}

HWND FASTCALL IntGetCurrentThreadDesktopWindow(VOID)
{
   PTHREADINFO pti = PsGetCurrentThreadWin32Thread();
   PDESKTOP pdo = pti->rpdesk;
   if (NULL == pdo)
   {
      ERR("Thread doesn't have a desktop\n");
      return NULL;
   }
   return pdo->DesktopWindow;
}

/* PUBLIC FUNCTIONS ***********************************************************/

HDC FASTCALL
UserGetDesktopDC(ULONG DcType, BOOL EmptyDC, BOOL ValidatehWnd)
{
    PWND DesktopObject = 0;
    HDC DesktopHDC = 0;

    if (DcType == DC_TYPE_DIRECT)
    {
        DesktopObject = UserGetDesktopWindow();
        DesktopHDC = (HDC)UserGetWindowDC(DesktopObject);
    }
    else
    {
        PMONITOR pMonitor = UserGetPrimaryMonitor();
        DesktopHDC = IntGdiCreateDisplayDC(pMonitor->hDev, DcType, EmptyDC);
    }

    return DesktopHDC;
}

VOID APIENTRY
UserRedrawDesktop()
{
    PWND Window = NULL;
    HRGN hRgn;

    Window = UserGetDesktopWindow();
    hRgn = IntSysCreateRectRgnIndirect(&Window->rcWindow);

    IntInvalidateWindows( Window,
                            hRgn,
                       RDW_FRAME |
                       RDW_ERASE |
                  RDW_INVALIDATE |
                 RDW_ALLCHILDREN);

    GreDeleteObject(hRgn);
}


NTSTATUS FASTCALL
co_IntShowDesktop(PDESKTOP Desktop, ULONG Width, ULONG Height)
{
   CSR_API_MESSAGE Request;

   Request.Type = MAKE_CSR_API(SHOW_DESKTOP, CSR_GUI);
   Request.Data.ShowDesktopRequest.DesktopWindow = Desktop->DesktopWindow;
   Request.Data.ShowDesktopRequest.Width = Width;
   Request.Data.ShowDesktopRequest.Height = Height;

   return co_CsrNotify(&Request);
}

NTSTATUS FASTCALL
IntHideDesktop(PDESKTOP Desktop)
{
#if 0
   CSRSS_API_REQUEST Request;
   CSRSS_API_REPLY Reply;

   Request.Type = CSRSS_HIDE_DESKTOP;
   Request.Data.HideDesktopRequest.DesktopWindow = Desktop->DesktopWindow;

   return NotifyCsrss(&Request, &Reply);
#else

   PWND DesktopWnd;

   DesktopWnd = IntGetWindowObject(Desktop->DesktopWindow);
   if (! DesktopWnd)
   {
      return ERROR_INVALID_WINDOW_HANDLE;
   }
   DesktopWnd->style &= ~WS_VISIBLE;

   return STATUS_SUCCESS;
#endif
}




static
HWND* FASTCALL
UserBuildShellHookHwndList(PDESKTOP Desktop)
{
   ULONG entries=0;
   PSHELL_HOOK_WINDOW Current;
   HWND* list;

   /* FIXME: If we save nb elements in desktop, we dont have to loop to find nb entries */
   LIST_FOR_EACH(Current, &Desktop->ShellHookWindows, SHELL_HOOK_WINDOW, ListEntry)
      entries++;

   if (!entries) return NULL;

   list = ExAllocatePoolWithTag(PagedPool, sizeof(HWND) * (entries + 1), USERTAG_WINDOWLIST); /* alloc one extra for nullterm */
   if (list)
   {
      HWND* cursor = list;

      LIST_FOR_EACH(Current, &Desktop->ShellHookWindows, SHELL_HOOK_WINDOW, ListEntry)
         *cursor++ = Current->hWnd;

      *cursor = NULL; /* Nullterm list */
   }

   return list;
}

/*
 * Send the Message to the windows registered for ShellHook
 * notifications. The lParam contents depend on the Message. See
 * MSDN for more details (RegisterShellHookWindow)
 */
VOID co_IntShellHookNotify(WPARAM Message, LPARAM lParam)
{
   PDESKTOP Desktop = IntGetActiveDesktop();
   HWND* HwndList;

   if (!gpsi->uiShellMsg)
   {

      /* Too bad, this doesn't work. */
#if 0
      UNICODE_STRING Str;
      RtlInitUnicodeString(&Str, L"SHELLHOOK");
      gpsi->uiShellMsg = UserRegisterWindowMessage(&Str);
#endif

      gpsi->uiShellMsg = IntAddAtom(L"SHELLHOOK");

      TRACE("MsgType = %x\n", gpsi->uiShellMsg);
      if (!gpsi->uiShellMsg)
         ERR("LastError: %x\n", EngGetLastError());
   }

   if (!Desktop)
   {
      TRACE("IntShellHookNotify: No desktop!\n");
      return;
   }

   HwndList = UserBuildShellHookHwndList(Desktop);
   if (HwndList)
   {
      HWND* cursor = HwndList;

      for (; *cursor; cursor++)
      {
         TRACE("Sending notify\n");
         co_IntPostOrSendMessage(*cursor,
                                 gpsi->uiShellMsg,
                                 Message,
                                 lParam);
      }

      ExFreePool(HwndList);
   }

}

/*
 * Add the window to the ShellHookWindows list. The windows
 * on that list get notifications that are important to shell
 * type applications.
 *
 * TODO: Validate the window? I'm not sure if sending these messages to
 * an unsuspecting application that is not your own is a nice thing to do.
 */
BOOL IntRegisterShellHookWindow(HWND hWnd)
{
   PTHREADINFO pti = PsGetCurrentThreadWin32Thread();
   PDESKTOP Desktop = pti->rpdesk;
   PSHELL_HOOK_WINDOW Entry;

   TRACE("IntRegisterShellHookWindow\n");

   /* First deregister the window, so we can be sure it's never twice in the
    * list.
    */
   IntDeRegisterShellHookWindow(hWnd);

   Entry = ExAllocatePoolWithTag(PagedPool,
                                 sizeof(SHELL_HOOK_WINDOW),
                                 TAG_WINSTA);

   if (!Entry)
      return FALSE;

   Entry->hWnd = hWnd;

   InsertTailList(&Desktop->ShellHookWindows, &Entry->ListEntry);

   return TRUE;
}

/*
 * Remove the window from the ShellHookWindows list. The windows
 * on that list get notifications that are important to shell
 * type applications.
 */
BOOL IntDeRegisterShellHookWindow(HWND hWnd)
{
   PTHREADINFO pti = PsGetCurrentThreadWin32Thread();
   PDESKTOP Desktop = pti->rpdesk;
   PSHELL_HOOK_WINDOW Current;

   LIST_FOR_EACH(Current, &Desktop->ShellHookWindows, SHELL_HOOK_WINDOW, ListEntry)
   {
      if (Current->hWnd == hWnd)
      {
         RemoveEntryList(&Current->ListEntry);
         ExFreePool(Current);
         return TRUE;
      }
   }

   return FALSE;
}

static VOID
IntFreeDesktopHeap(IN OUT PDESKTOP Desktop)
{
    if (Desktop->hsectionDesktop != NULL)
    {
        ObDereferenceObject(Desktop->hsectionDesktop);
        Desktop->hsectionDesktop = NULL;
    }
}
/* SYSCALLS *******************************************************************/

/*
 * NtUserCreateDesktop
 *
 * Creates a new desktop.
 *
 * Parameters
 *    poaAttribs
 *       Object Attributes.
 *
 *    lpszDesktopDevice
 *       Name of the device.
 *
 *    pDeviceMode
 *       Device Mode.
 *
 *    dwFlags
 *       Interaction flags.
 *
 *    dwDesiredAccess
 *       Requested type of access.
 *
 *
 * Return Value
 *    If the function succeeds, the return value is a handle to the newly
 *    created desktop. If the specified desktop already exists, the function
 *    succeeds and returns a handle to the existing desktop. When you are
 *    finished using the handle, call the CloseDesktop function to close it.
 *    If the function fails, the return value is NULL.
 *
 * Status
 *    @implemented
 */

HDESK APIENTRY
NtUserCreateDesktop(
   POBJECT_ATTRIBUTES ObjectAttributes,
   PUNICODE_STRING lpszDesktopDevice,
   LPDEVMODEW lpdmw,
   DWORD dwFlags,
   ACCESS_MASK dwDesiredAccess)
{
   PDESKTOP DesktopObject;
   UNICODE_STRING DesktopName;
   NTSTATUS Status = STATUS_SUCCESS;
   HDESK Desktop;
   CSR_API_MESSAGE Request;
   PVOID DesktopHeapSystemBase = NULL;
   SIZE_T DesktopInfoSize;
   BOOLEAN Context;
   ULONG_PTR HeapSize = 4 * 1024 * 1024; /* FIXME */
   UNICODE_STRING ClassName;
   LARGE_STRING WindowName;
   BOOL NoHooks = FALSE;
   PWND pWnd = NULL;
   CREATESTRUCTW Cs;
   INT i;
   PTHREADINFO ptiCurrent;
   DECLARE_RETURN(HDESK);

   TRACE("Enter NtUserCreateDesktop\n");
   UserEnterExclusive();

   ptiCurrent = PsGetCurrentThreadWin32Thread();
   if (ptiCurrent)
   {
   /* Turn off hooks when calling any CreateWindowEx from inside win32k. */
      NoHooks = (ptiCurrent->TIF_flags & TIF_DISABLEHOOKS);
      ptiCurrent->TIF_flags |= TIF_DISABLEHOOKS;
      ptiCurrent->pClientInfo->dwTIFlags = ptiCurrent->TIF_flags;
   }
   DesktopName.Buffer = NULL;

   /*
    * Try to open already existing desktop
    */
   
   Status = ObOpenObjectByName(
               ObjectAttributes,
               ExDesktopObjectType,
               UserMode,
               NULL,
               dwDesiredAccess,
               (PVOID)&Context,
               (HANDLE*)&Desktop);
   if (!NT_SUCCESS(Status)) RETURN(NULL);

   /* In case the object was not created (eg if it existed), return now */
   if (Context == FALSE)
   {
       TRACE("NtUserCreateDesktop opened desktop %wZ\n", ObjectAttributes->ObjectName);
       RETURN( Desktop);
   }

   /* Capture desktop name */
   _SEH2_TRY
   {
      ProbeForRead( ObjectAttributes, sizeof(OBJECT_ATTRIBUTES),  1);

      Status = IntSafeCopyUnicodeString(&DesktopName, ObjectAttributes->ObjectName);
   }
   _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
   {
      Status = _SEH2_GetExceptionCode();
   }
   _SEH2_END

   if (! NT_SUCCESS(Status))
   {
      ERR("Failed reading Object Attributes from user space.\n");
      SetLastNtError(Status);
      RETURN( NULL);
   }

   /* Reference the desktop */
   Status = ObReferenceObjectByHandle(Desktop,
                                      0,
                                      ExDesktopObjectType,
                                      KernelMode,
                                      (PVOID)&DesktopObject,
                                      NULL);
   if (!NT_SUCCESS(Status)) 
   {
       ERR("Failed to reference desktop object\n");
       RETURN(NULL);
   }

   TRACE("NtUserCreateDesktop created desktop 0x%x with name %wZ\n", DesktopObject, &DesktopName);

   DesktopObject->hsectionDesktop = NULL;
   DesktopObject->pheapDesktop = UserCreateHeap(&DesktopObject->hsectionDesktop,
                                                &DesktopHeapSystemBase,
                                                HeapSize);
   if (DesktopObject->pheapDesktop == NULL)
   {
       ObDereferenceObject(DesktopObject);
       ERR("Failed to create desktop heap!\n");
       RETURN(NULL);
   }

   DesktopInfoSize = sizeof(DESKTOPINFO) + DesktopName.Length;

   DesktopObject->pDeskInfo = RtlAllocateHeap(DesktopObject->pheapDesktop,
                                              HEAP_NO_SERIALIZE,
                                              DesktopInfoSize);

   if (DesktopObject->pDeskInfo == NULL)
   {
       ObDereferenceObject(DesktopObject);
       ERR("Failed to create the DESKTOP structure!\n");
       RETURN(NULL);
   }

   RtlZeroMemory(DesktopObject->pDeskInfo,
                 DesktopInfoSize);

   DesktopObject->pDeskInfo->pvDesktopBase = DesktopHeapSystemBase;
   DesktopObject->pDeskInfo->pvDesktopLimit = (PVOID)((ULONG_PTR)DesktopHeapSystemBase + HeapSize);
   RtlCopyMemory(DesktopObject->pDeskInfo->szDesktopName,
                 DesktopName.Buffer,
                 DesktopName.Length);

   /* Initialize some local (to win32k) desktop state. */
   InitializeListHead(&DesktopObject->PtiList);
   DesktopObject->ActiveMessageQueue = NULL;
   /* Setup Global Hooks. */
   for (i = 0; i < NB_HOOKS; i++)
   {
      InitializeListHead(&DesktopObject->pDeskInfo->aphkStart[i]);
   }


   /*
    * Create a handle for CSRSS and notify CSRSS for Creating Desktop Window.
    */
   Request.Type = MAKE_CSR_API(CREATE_DESKTOP, CSR_GUI);
   Status = CsrInsertObject(Desktop,
                            GENERIC_ALL,
                            (HANDLE*)&Request.Data.CreateDesktopRequest.DesktopHandle);
   if (! NT_SUCCESS(Status))
   {
      ERR("Failed to create desktop handle for CSRSS\n");
      ZwClose(Desktop);
      SetLastNtError(Status);
      RETURN( NULL);
   }

   Status = co_CsrNotify(&Request);
   if (! NT_SUCCESS(Status))
   {
      CsrCloseHandle(Request.Data.CreateDesktopRequest.DesktopHandle);
      ERR("Failed to notify CSRSS about new desktop\n");
      ZwClose(Desktop);
      SetLastNtError(Status);
      RETURN( NULL);
   }
#if 0 // Turn on when server side proc is ready.
   //
   // Create desktop window.
   //
   ClassName.Buffer = ((PWSTR)((ULONG_PTR)(WORD)(gpsi->atomSysClass[ICLS_DESKTOP])));
   ClassName.Length = 0;
   RtlZeroMemory(&WindowName, sizeof(WindowName));

   RtlZeroMemory(&Cs, sizeof(Cs));
   Cs.x = UserGetSystemMetrics(SM_XVIRTUALSCREEN);
   Cs.y = UserGetSystemMetrics(SM_YVIRTUALSCREEN);
   Cs.cx = UserGetSystemMetrics(SM_CXVIRTUALSCREEN);
   Cs.cy = UserGetSystemMetrics(SM_CYVIRTUALSCREEN);
   Cs.style = WS_POPUP|WS_CLIPCHILDREN;
   Cs.hInstance = hModClient; // Experimental mode... Move csr stuff to User32. hModuleWin; // Server side winproc!
   Cs.lpszName = (LPCWSTR) &WindowName;
   Cs.lpszClass = (LPCWSTR) &ClassName;

   pWndDesktop = co_UserCreateWindowEx(&Cs, &ClassName, &WindowName, NULL);
   if (!pWnd)
   {
      ERR("Failed to create Desktop window handle\n");
   }
   else
   {
      DesktopObject->pDeskInfo->spwnd = pWndDesktop;
   }
#endif

   if (!ptiCurrent->rpdesk) IntSetThreadDesktop(Desktop,FALSE);

  /*
     Based on wine/server/window.c in get_desktop_window.
   */

   ClassName.Buffer = ((PWSTR)((ULONG_PTR)(WORD)(gpsi->atomSysClass[ICLS_HWNDMESSAGE])));
   ClassName.Length = 0;
   RtlZeroMemory(&WindowName, sizeof(WindowName));

   RtlZeroMemory(&Cs, sizeof(Cs));
   Cs.cx = Cs.cy = 100;
   Cs.style = WS_POPUP|WS_CLIPCHILDREN;
   Cs.hInstance = hModClient; // hModuleWin; // Server side winproc! Leave it to Timo to not pass on notes!
   Cs.lpszName = (LPCWSTR) &WindowName;
   Cs.lpszClass = (LPCWSTR) &ClassName;

   pWnd = co_UserCreateWindowEx(&Cs, &ClassName, &WindowName, NULL);
   if (!pWnd)
   {
      ERR("Failed to create Message window handle\n");
   }
   else
   {
      DesktopObject->spwndMessage = pWnd;
   }

   /* Now,,,
      if !(WinStaObject->Flags & WSF_NOIO) is (not set) for desktop input output mode (see wiki)
      Create Tooltip. Saved in DesktopObject->spwndTooltip.
      Tooltip dwExStyle: WS_EX_TOOLWINDOW|WS_EX_TOPMOST
      hWndParent are spwndMessage. Use hModuleWin for server side winproc!
      The rest is same as message window.
      http://msdn.microsoft.com/en-us/library/bb760250(VS.85).aspx
   */
   RETURN( Desktop);

CLEANUP:
   if(DesktopName.Buffer != NULL)
   {
       ExFreePoolWithTag(DesktopName.Buffer, TAG_STRING);
   }
   if (!NoHooks && ptiCurrent) 
   {
       ptiCurrent->TIF_flags &= ~TIF_DISABLEHOOKS;
       ptiCurrent->pClientInfo->dwTIFlags = ptiCurrent->TIF_flags;
   }
   TRACE("Leave NtUserCreateDesktop, ret=%i\n",_ret_);
   UserLeave();
   END_CLEANUP;
}

/*
 * NtUserOpenDesktop
 *
 * Opens an existing desktop.
 *
 * Parameters
 *    lpszDesktopName
 *       Name of the existing desktop.
 *
 *    dwFlags
 *       Interaction flags.
 *
 *    dwDesiredAccess
 *       Requested type of access.
 *
 * Return Value
 *    Handle to the desktop or zero on failure.
 *
 * Status
 *    @implemented
 */

HDESK APIENTRY
NtUserOpenDesktop(
   POBJECT_ATTRIBUTES ObjectAttributes,
   DWORD dwFlags,
   ACCESS_MASK dwDesiredAccess)
{
   NTSTATUS Status;
   HDESK Desktop;

   Status = ObOpenObjectByName(
               ObjectAttributes,
               ExDesktopObjectType,
               UserMode,
               NULL,
               dwDesiredAccess,
               NULL,
               (HANDLE*)&Desktop);

   if (!NT_SUCCESS(Status))
   {
      ERR("Failed to open desktop\n");
      SetLastNtError(Status);
      return 0;
   }

   TRACE("Opened desktop %S with handle 0x%x\n", ObjectAttributes->ObjectName->Buffer, Desktop);

   return Desktop;
}

/*
 * NtUserOpenInputDesktop
 *
 * Opens the input (interactive) desktop.
 *
 * Parameters
 *    dwFlags
 *       Interaction flags.
 *
 *    fInherit
 *       Inheritance option.
 *
 *    dwDesiredAccess
 *       Requested type of access.
 *
 * Return Value
 *    Handle to the input desktop or zero on failure.
 *
 * Status
 *    @implemented
 */

HDESK APIENTRY
NtUserOpenInputDesktop(
   DWORD dwFlags,
   BOOL fInherit,
   ACCESS_MASK dwDesiredAccess)
{
   PDESKTOP pdesk;
   NTSTATUS Status;
   HDESK hdesk;

   TRACE("Enter NtUserOpenInputDesktop\n");

   /* Get a pointer to the desktop object */
   Status = IntValidateDesktopHandle(InputDesktopHandle, UserMode, 0, &pdesk);
   if (!NT_SUCCESS(Status))
   {
      TRACE("Validation of input desktop handle (0x%X) failed\n", InputDesktop);
      return NULL;
   }

   /* Create a new handle to the object */
   Status = ObOpenObjectByPointer(
               pdesk,
               0,
               NULL,
               dwDesiredAccess,
               ExDesktopObjectType,
               UserMode,
               &hdesk);

   ObDereferenceObject(pdesk);

   if (!NT_SUCCESS(Status))
   {
       ERR("Failed to open input desktop object\n");
       SetLastNtError(Status);
       return NULL;
   }

   TRACE("NtUserOpenInputDesktop returning 0x%x\n",hdesk);
   return hdesk;
}

/*
 * NtUserCloseDesktop
 *
 * Closes a desktop handle.
 *
 * Parameters
 *    hDesktop
 *       Handle to the desktop.
 *
 * Return Value
 *   Status
 *
 * Remarks
 *   The desktop handle can be created with NtUserCreateDesktop or
 *   NtUserOpenDesktop. This function will fail if any thread in the calling
 *   process is using the specified desktop handle or if the handle refers
 *   to the initial desktop of the calling process.
 *
 * Status
 *    @implemented
 */

BOOL APIENTRY
NtUserCloseDesktop(HDESK hDesktop)
{
   PDESKTOP pdesk;
   NTSTATUS Status;
   PTHREADINFO pti;
   DECLARE_RETURN(BOOL);

   pti = PsGetCurrentThreadWin32Thread();

   TRACE("NtUserCloseDesktop called (0x%x)\n", hDesktop);
   UserEnterExclusive();

   if( hDesktop == pti->hdesk || hDesktop == pti->ppi->hdeskStartup)
   {
       ERR("Attempted to close thread desktop\n");
       EngSetLastError(ERROR_BUSY);
       RETURN(FALSE);
   }

   Status = IntValidateDesktopHandle( hDesktop, UserMode, 0, &pdesk);
   if (!NT_SUCCESS(Status))
   {
      ERR("Validation of desktop handle (0x%X) failed\n", hDesktop);
      RETURN(FALSE);
   }

   ObDereferenceObject(pdesk);

   Status = ZwClose(hDesktop);
   if (!NT_SUCCESS(Status))
   {
      ERR("Failed to close desktop handle 0x%x\n", hDesktop);
      SetLastNtError(Status);
      RETURN(FALSE);
   }

   RETURN(TRUE);

CLEANUP:
   TRACE("Leave NtUserCloseDesktop, ret=%i\n",_ret_);
   UserLeave();
   END_CLEANUP;
}




/*
 * NtUserPaintDesktop
 *
 * The NtUserPaintDesktop function fills the clipping region in the
 * specified device context with the desktop pattern or wallpaper. The
 * function is provided primarily for shell desktops.
 *
 * Parameters
 *    hDC
 *       Handle to the device context.
 *
 * Status
 *    @implemented
 */

BOOL APIENTRY
NtUserPaintDesktop(HDC hDC)
{
   RECTL Rect;
   HBRUSH DesktopBrush, PreviousBrush;
   HWND hWndDesktop;
   BOOL doPatBlt = TRUE;
   PWND WndDesktop;
    static WCHAR s_wszSafeMode[] = L"Safe Mode";
   int len;
   COLORREF color_old;
   UINT align_old;
   int mode_old;
   PTHREADINFO pti = PsGetCurrentThreadWin32Thread();
   PWINSTATION_OBJECT WinSta = pti->rpdesk->rpwinstaParent;
   DECLARE_RETURN(BOOL);

   UserEnterExclusive();
   TRACE("Enter NtUserPaintDesktop\n");

   GdiGetClipBox(hDC, &Rect);

   hWndDesktop = IntGetDesktopWindow();

   WndDesktop = UserGetWindowObject(hWndDesktop);
   if (!WndDesktop)
   {
      RETURN(FALSE);
   }

    if (!UserGetSystemMetrics(SM_CLEANBOOT))
    {
        DesktopBrush = (HBRUSH)WndDesktop->pcls->hbrBackground;

        /*
        * Paint desktop background
        */
        if (WinSta->hbmWallpaper != NULL)
        {
            SIZE sz;
            int x, y;
            HDC hWallpaperDC;

            sz.cx = WndDesktop->rcWindow.right - WndDesktop->rcWindow.left;
            sz.cy = WndDesktop->rcWindow.bottom - WndDesktop->rcWindow.top;

            if (WinSta->WallpaperMode == wmStretch ||
                WinSta->WallpaperMode == wmTile)
            {
                x = 0;
                y = 0;
            }
            else
            {
                /* Find the upper left corner, can be negtive if the bitmap is bigger then the screen */
                x = (sz.cx / 2) - (WinSta->cxWallpaper / 2);
                y = (sz.cy / 2) - (WinSta->cyWallpaper / 2);
            }

            hWallpaperDC = NtGdiCreateCompatibleDC(hDC);
            if(hWallpaperDC != NULL)
            {
                HBITMAP hOldBitmap;

                /* Fill in the area that the bitmap is not going to cover */
                if (x > 0 || y > 0)
                {
                   /* FIXME: Clip out the bitmap
                                               can be replaced with "NtGdiPatBlt(hDC, x, y, WinSta->cxWallpaper, WinSta->cyWallpaper, PATCOPY | DSTINVERT);"
                                               once we support DSTINVERT */
                  PreviousBrush = NtGdiSelectBrush(hDC, DesktopBrush);
                  NtGdiPatBlt(hDC, Rect.left, Rect.top, Rect.right, Rect.bottom, PATCOPY);
                  NtGdiSelectBrush(hDC, PreviousBrush);
                }

                /*Do not fill the background after it is painted no matter the size of the picture */
                doPatBlt = FALSE;

                hOldBitmap = NtGdiSelectBitmap(hWallpaperDC, WinSta->hbmWallpaper);

                if (WinSta->WallpaperMode == wmStretch)
                {
                    if(Rect.right && Rect.bottom)
                        NtGdiStretchBlt(hDC,
                                    x,
                                    y,
                                    sz.cx,
                                    sz.cy,
                                    hWallpaperDC,
                                    0,
                                    0,
                                    WinSta->cxWallpaper,
                                    WinSta->cyWallpaper,
                                    SRCCOPY,
                                    0);

                }
                else if (WinSta->WallpaperMode == wmTile)
                {
                    /* Paint the bitmap across the screen then down */
                    for(y = 0; y < Rect.bottom; y += WinSta->cyWallpaper)
                    {
                        for(x = 0; x < Rect.right; x += WinSta->cxWallpaper)
                        {
                            NtGdiBitBlt(hDC,
                                        x,
                                        y,
                                        WinSta->cxWallpaper,
                                        WinSta->cyWallpaper,
                                        hWallpaperDC,
                                        0,
                                        0,
                                        SRCCOPY,
                                        0,
                                        0);
                        }
                    }
                }
                else
                {
                    NtGdiBitBlt(hDC,
                                x,
                                y,
                                WinSta->cxWallpaper,
                                WinSta->cyWallpaper,
                                hWallpaperDC,
                                0,
                                0,
                                SRCCOPY,
                                0,
                                0);
                }
                NtGdiSelectBitmap(hWallpaperDC, hOldBitmap);
                NtGdiDeleteObjectApp(hWallpaperDC);
            }
        }
    }
    else
    {
        /* Black desktop background in Safe Mode */
        DesktopBrush = StockObjects[BLACK_BRUSH];
    }

    /* Back ground is set to none, clear the screen */
    if (doPatBlt)
    {
      PreviousBrush = NtGdiSelectBrush(hDC, DesktopBrush);
      NtGdiPatBlt(hDC, Rect.left, Rect.top, Rect.right, Rect.bottom, PATCOPY);
      NtGdiSelectBrush(hDC, PreviousBrush);
    }

   /*
    * Display system version on the desktop background
    */

   if (g_PaintDesktopVersion||UserGetSystemMetrics(SM_CLEANBOOT))
   {
      static WCHAR s_wszVersion[256] = {0};
      RECTL rect;

      if (*s_wszVersion)
      {
         len = wcslen(s_wszVersion);
      }
      else
      {
         len = GetSystemVersionString(s_wszVersion);
      }

      if (len)
      {
         if (!UserSystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0))
         {
            rect.right = UserGetSystemMetrics(SM_CXSCREEN);
            rect.bottom = UserGetSystemMetrics(SM_CYSCREEN);
         }

         color_old = IntGdiSetTextColor(hDC, RGB(255,255,255));
         align_old = IntGdiSetTextAlign(hDC, TA_RIGHT);
         mode_old = IntGdiSetBkMode(hDC, TRANSPARENT);

            if(!UserGetSystemMetrics(SM_CLEANBOOT))
            {
                GreExtTextOutW(hDC, rect.right-16, rect.bottom-48, 0, NULL, s_wszVersion, len, NULL, 0);
            }
            else
            {
                /* Safe Mode */
                /* Version information text in top center */
                IntGdiSetTextAlign(hDC, TA_CENTER|TA_TOP);
                GreExtTextOutW(hDC, (rect.right+rect.left)/2, rect.top, 0, NULL, s_wszVersion, len, NULL, 0);
                /* Safe Mode text in corners */
                len = wcslen(s_wszSafeMode);
                IntGdiSetTextAlign(hDC, TA_RIGHT|TA_TOP);
                GreExtTextOutW(hDC, rect.right, rect.top, 0, NULL, s_wszSafeMode, len, NULL, 0);
                IntGdiSetTextAlign(hDC, TA_RIGHT|TA_BASELINE);
                GreExtTextOutW(hDC, rect.right, rect.bottom, 0, NULL, s_wszSafeMode, len, NULL, 0);
                IntGdiSetTextAlign(hDC, TA_LEFT|TA_TOP);
                GreExtTextOutW(hDC, rect.left, rect.top, 0, NULL, s_wszSafeMode, len, NULL, 0);
                IntGdiSetTextAlign(hDC, TA_LEFT|TA_BASELINE);
                GreExtTextOutW(hDC, rect.left, rect.bottom, 0, NULL, s_wszSafeMode, len, NULL, 0);

            }


         IntGdiSetBkMode(hDC, mode_old);
         IntGdiSetTextAlign(hDC, align_old);
         IntGdiSetTextColor(hDC, color_old);
      }
   }

   RETURN(TRUE);

CLEANUP:
   TRACE("Leave NtUserPaintDesktop, ret=%i\n",_ret_);
   UserLeave();
   END_CLEANUP;
}


/*
 * NtUserSwitchDesktop
 *
 * Sets the current input (interactive) desktop.
 *
 * Parameters
 *    hDesktop
 *       Handle to desktop.
 *
 * Return Value
 *    Status
 *
 * Status
 *    @unimplemented
 */

BOOL APIENTRY
NtUserSwitchDesktop(HDESK hdesk)
{
   PDESKTOP pdesk;
   NTSTATUS Status;
   DECLARE_RETURN(BOOL);

   UserEnterExclusive();
   TRACE("Enter NtUserSwitchDesktop(0x%x)\n", hdesk);

   Status = IntValidateDesktopHandle( hdesk, UserMode, 0, &pdesk);
   if (!NT_SUCCESS(Status))
   {
      ERR("Validation of desktop handle (0x%X) failed\n", hdesk);
      RETURN(FALSE);
   }

   /*
    * Don't allow applications switch the desktop if it's locked, unless the caller
    * is the logon application itself
    */
   if((pdesk->rpwinstaParent->Flags & WSS_LOCKED) &&
      LogonProcess != PsGetCurrentProcessWin32Process())
   {
      ObDereferenceObject(pdesk);
      ERR("Switching desktop 0x%x denied because the window station is locked!\n", hdesk);
      RETURN(FALSE);
   }

   if(pdesk->rpwinstaParent != InputWindowStation)
   {
      ObDereferenceObject(pdesk);
      ERR("Switching desktop 0x%x denied because desktop doesn't belong to the interactive winsta!\n", hdesk);
      RETURN(FALSE);
   }

   /* FIXME: Fail if the process is associated with a secured
             desktop such as Winlogon or Screen-Saver */
   /* FIXME: Connect to input device */

   /* Set the active desktop in the desktop's window station. */
   InputWindowStation->ActiveDesktop = pdesk;

   /* Set the global state. */
   InputDesktop = pdesk;
   InputDesktopHandle = hdesk;

   ObDereferenceObject(pdesk);

   RETURN(TRUE);

CLEANUP:
   TRACE("Leave NtUserSwitchDesktop, ret=%i\n",_ret_);
   UserLeave();
   END_CLEANUP;
}

/*
 * NtUserGetThreadDesktop
 *
 * Status
 *    @implemented
 */

HDESK APIENTRY
NtUserGetThreadDesktop(DWORD dwThreadId, DWORD Unknown1)
{
   NTSTATUS Status;
   PETHREAD Thread;
   PDESKTOP DesktopObject;
   HDESK Ret, hThreadDesktop;
   OBJECT_HANDLE_INFORMATION HandleInformation;
   DECLARE_RETURN(HDESK);

   UserEnterExclusive();
   TRACE("Enter NtUserGetThreadDesktop\n");

   if(!dwThreadId)
   {
      EngSetLastError(ERROR_INVALID_PARAMETER);
      RETURN(0);
   }

   Status = PsLookupThreadByThreadId((HANDLE)(DWORD_PTR)dwThreadId, &Thread);
   if(!NT_SUCCESS(Status))
   {
      EngSetLastError(ERROR_INVALID_PARAMETER);
      RETURN(0);
   }

   if(Thread->ThreadsProcess == PsGetCurrentProcess())
   {
      /* Just return the handle, we queried the desktop handle of a thread running
         in the same context */
      Ret = ((PTHREADINFO)Thread->Tcb.Win32Thread)->hdesk;
      ObDereferenceObject(Thread);
      RETURN(Ret);
   }

   /* Get the desktop handle and the desktop of the thread */
   if(!(hThreadDesktop = ((PTHREADINFO)Thread->Tcb.Win32Thread)->hdesk) ||
         !(DesktopObject = ((PTHREADINFO)Thread->Tcb.Win32Thread)->rpdesk))
   {
      ObDereferenceObject(Thread);
      ERR("Desktop information of thread 0x%x broken!?\n", dwThreadId);
      RETURN(NULL);
   }

   /* We could just use DesktopObject instead of looking up the handle, but latter
      may be a bit safer (e.g. when the desktop is being destroyed */
   /* Switch into the context of the thread we're trying to get the desktop from,
      so we can use the handle */
   KeAttachProcess(&Thread->ThreadsProcess->Pcb);
   Status = ObReferenceObjectByHandle(hThreadDesktop,
                                      GENERIC_ALL,
                                      ExDesktopObjectType,
                                      UserMode,
                                      (PVOID*)&DesktopObject,
                                      &HandleInformation);
   KeDetachProcess();

   /* The handle couldn't be found, there's nothing to get... */
   if(!NT_SUCCESS(Status))
   {
      ObDereferenceObject(Thread);
      RETURN(NULL);
   }

   /* Lookup our handle table if we can find a handle to the desktop object,
      if not, create one */
   Ret = IntGetDesktopObjectHandle(DesktopObject);

   /* All done, we got a valid handle to the desktop */
   ObDereferenceObject(DesktopObject);
   ObDereferenceObject(Thread);
   RETURN(Ret);

CLEANUP:
   TRACE("Leave NtUserGetThreadDesktop, ret=%i\n",_ret_);
   UserLeave();
   END_CLEANUP;
}

static NTSTATUS
IntUnmapDesktopView(IN PDESKTOP pdesk)
{
    PPROCESSINFO ppi;
    PW32HEAP_USER_MAPPING HeapMapping, *PrevLink;
    NTSTATUS Status = STATUS_SUCCESS;

    TRACE("IntUnmapDesktopView called for desktop object %p\n", pdesk);

    ppi = PsGetCurrentProcessWin32Process();
    PrevLink = &ppi->HeapMappings.Next;

    /* Unmap if we're the last thread using the desktop */
    HeapMapping = *PrevLink;
    while (HeapMapping != NULL)
    {
        if (HeapMapping->KernelMapping == (PVOID)pdesk->pheapDesktop)
        {
            if (--HeapMapping->Count == 0)
            {
                *PrevLink = HeapMapping->Next;

                TRACE("ppi 0x%x unmapped heap of desktop 0x%x\n", ppi, pdesk);
                Status = MmUnmapViewOfSection(PsGetCurrentProcess(),
                                              HeapMapping->UserMapping);

                ObDereferenceObject(pdesk);

                UserHeapFree(HeapMapping);
                break;
            }
        }

        PrevLink = &HeapMapping->Next;
        HeapMapping = HeapMapping->Next;
    }

    return Status;
}

static NTSTATUS
IntMapDesktopView(IN PDESKTOP pdesk)
{
    PPROCESSINFO ppi;
    PW32HEAP_USER_MAPPING HeapMapping, *PrevLink;
    PVOID UserBase = NULL;
    SIZE_T ViewSize = 0;
    LARGE_INTEGER Offset;
    NTSTATUS Status;

    TRACE("IntMapDesktopView called for desktop object 0x%x\n", pdesk);

    ppi = PsGetCurrentProcessWin32Process();
    PrevLink = &ppi->HeapMappings.Next;

    /* Find out if another thread already mapped the desktop heap */
    HeapMapping = *PrevLink;
    while (HeapMapping != NULL)
    {
        if (HeapMapping->KernelMapping == (PVOID)pdesk->pheapDesktop)
        {
            HeapMapping->Count++;
            return STATUS_SUCCESS;
        }

        PrevLink = &HeapMapping->Next;
        HeapMapping = HeapMapping->Next;
    }

    /* We're the first, map the heap */
    Offset.QuadPart = 0;
    Status = MmMapViewOfSection(pdesk->hsectionDesktop,
                                PsGetCurrentProcess(),
                                &UserBase,
                                0,
                                0,
                                &Offset,
                                &ViewSize,
                                ViewUnmap,
                                SEC_NO_CHANGE,
                                PAGE_EXECUTE_READ); /* Would prefer PAGE_READONLY, but thanks to RTL heaps... */
    if (!NT_SUCCESS(Status))
    {
        ERR("Failed to map desktop\n");
        return Status;
    }

    TRACE("ppi 0x%x mapped heap of desktop 0x%x\n", ppi, pdesk);

    /* Add the mapping */
    HeapMapping = UserHeapAlloc(sizeof(W32HEAP_USER_MAPPING));
    if (HeapMapping == NULL)
    {
        MmUnmapViewOfSection(PsGetCurrentProcess(), UserBase);
        ERR("UserHeapAlloc() failed!\n");
        return STATUS_NO_MEMORY;
    }

    HeapMapping->Next = NULL;
    HeapMapping->KernelMapping = (PVOID)pdesk->pheapDesktop;
    HeapMapping->UserMapping = UserBase;
    HeapMapping->Limit = ViewSize;
    HeapMapping->Count = 1;
    *PrevLink = HeapMapping;

    ObReferenceObject(pdesk);

    return STATUS_SUCCESS;
}

BOOL
IntSetThreadDesktop(IN HDESK hDesktop,
                    IN BOOL FreeOnFailure)
{
    PDESKTOP pdesk = NULL, pdeskOld;
    HDESK hdeskOld;
    PTHREADINFO pti;
    NTSTATUS Status;
    PCLIENTTHREADINFO pctiOld, pctiNew;
    PCLIENTINFO pci;

    ASSERT(NtCurrentTeb());

    TRACE("IntSetThreadDesktop hDesktop:0x%x, FOF:%d\n",hDesktop, FreeOnFailure);

    pti = PsGetCurrentThreadWin32Thread();
    pci = pti->pClientInfo;

    /* If the caller gave us a desktop, ensure it is valid */
    if(hDesktop != NULL)
    {
        /* Validate the new desktop. */
        Status = IntValidateDesktopHandle( hDesktop, UserMode, 0, &pdesk);
        if (!NT_SUCCESS(Status))
        {
            ERR("Validation of desktop handle (0x%X) failed\n", hDesktop);
            return FALSE;
        }

        if (pti->rpdesk == pdesk)
        {
            /* Nothing to do */
            ObDereferenceObject(pdesk);
            return TRUE;
        }
    }

    /* Make sure that we don't own any window in the current desktop */
    if (!IsListEmpty(&pti->WindowListHead))
    {
        if(pdesk)
            ObDereferenceObject(pdesk);
        ERR("Attempted to change thread desktop although the thread has windows!\n");
        EngSetLastError(ERROR_BUSY);
        return FALSE;
    }

    /* Before doing the switch, map the new desktop heap and allocate the new pcti */
    if(pdesk != NULL)
    {
        Status = IntMapDesktopView(pdesk);
        if (!NT_SUCCESS(Status))
        {
            ERR("Failed to map desktop heap!\n");
            ObDereferenceObject(pdesk);
            SetLastNtError(Status);
            return FALSE;
        }

        pctiNew = DesktopHeapAlloc( pdesk, sizeof(CLIENTTHREADINFO));
        if(pctiNew == NULL)
        {
            ERR("Failed to allocate new pcti\n");
            IntUnmapDesktopView(pdesk);
            ObDereferenceObject(pdesk);
            EngSetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
    }

    /* free all classes or move them to the shared heap */
    if(pti->rpdesk != NULL)
    {
        if(!IntCheckProcessDesktopClasses(pti->rpdesk, FreeOnFailure))
        {
            ERR("Failed to move process classes to shared heap!\n");
            if(pdesk)
            {
                DesktopHeapFree(pdesk, pctiNew);
                IntUnmapDesktopView(pdesk);
                ObDereferenceObject(pdesk);
            }
            return FALSE;
        }
    }
     
    pdeskOld = pti->rpdesk;
    hdeskOld = pti->hdesk;
    pctiOld = pti->pcti;

    /* do the switch */   
    if(pdesk != NULL)
    {
        pti->rpdesk = pdesk;
        pti->hdesk = hDesktop;
        pti->pDeskInfo = pti->rpdesk->pDeskInfo;
        pti->pcti = pctiNew;

        pci->ulClientDelta = DesktopHeapGetUserDelta();
        pci->pDeskInfo = (PVOID)((ULONG_PTR)pti->pDeskInfo - pci->ulClientDelta);
        pci->pClientThreadInfo = (PVOID)((ULONG_PTR)pti->pcti - pci->ulClientDelta);

        /* initialize the new pcti */
        if(pctiOld != NULL)
        {
            RtlCopyMemory(pctiNew, pctiOld, sizeof(CLIENTTHREADINFO));
        }
        else
        {
            RtlZeroMemory(pctiNew, sizeof(CLIENTTHREADINFO));
        }
    }
    else
    {
        pti->rpdesk = NULL;
        pti->hdesk = NULL;
        pti->pDeskInfo = NULL;
        pti->pcti = NULL;
        pci->ulClientDelta = 0;
        pci->pDeskInfo = NULL;
        pci->pClientThreadInfo = NULL;
    }
    
    /* clean up the old desktop */
    if(pdeskOld != NULL)
    {
        RemoveEntryList(&pti->PtiLink);
        DesktopHeapFree(pdeskOld, pctiOld);
        IntUnmapDesktopView(pdeskOld);
        ObDereferenceObject(pdeskOld);
        ZwClose(hdeskOld);
    }

    if(pdesk)
    {
        InsertTailList(&pdesk->PtiList, &pti->PtiLink);
    }

    TRACE("IntSetThreadDesktop: pti 0x%x ppi 0x%x switched from object 0x%x to 0x%x\n", pti, pti->ppi, pdeskOld, pdesk);

    return TRUE;
}

/*
 * NtUserSetThreadDesktop
 *
 * Status
 *    @implemented
 */

BOOL APIENTRY
NtUserSetThreadDesktop(HDESK hDesktop)
{
   BOOL ret;

   UserEnterExclusive();

   ret = IntSetThreadDesktop(hDesktop, FALSE);

   UserLeave();

   return ret;
}

/* EOF */
