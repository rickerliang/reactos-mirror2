/*
 *  ReactOS W32 Subsystem
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003 ReactOS Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/* $Id: message.c,v 1.59 2004/04/15 23:36:03 weiden Exp $
 *
 * COPYRIGHT:        See COPYING in the top level directory
 * PROJECT:          ReactOS kernel
 * PURPOSE:          Messages
 * FILE:             subsys/win32k/ntuser/message.c
 * PROGRAMER:        Casper S. Hornstrup (chorns@users.sourceforge.net)
 * REVISION HISTORY:
 *       06-06-2001  CSH  Created
 */

/* INCLUDES ******************************************************************/

#include <ddk/ntddk.h>
#include <win32k/win32k.h>
#include <include/msgqueue.h>
#include <include/window.h>
#include <include/winpos.h>
#include <include/focus.h>
#include <include/class.h>
#include <include/error.h>
#include <include/object.h>
#include <include/winsta.h>
#include <include/callback.h>
#include <include/painting.h>
#include <include/input.h>
#include <include/desktop.h>
#include <include/tags.h>
#include <internal/safe.h>

#define NDEBUG
#include <debug.h>

typedef struct
{
  UINT uFlags;
  UINT uTimeout;
  ULONG_PTR uResult;
} DOSENDMESSAGE, *PDOSENDMESSAGE;

/* FUNCTIONS *****************************************************************/

NTSTATUS FASTCALL
IntInitMessageImpl(VOID)
{
  return STATUS_SUCCESS;
}

NTSTATUS FASTCALL
IntCleanupMessageImpl(VOID)
{
  return STATUS_SUCCESS;
}

LRESULT FASTCALL
IntDispatchMessage(MSG* Msg)
{
  LRESULT Result;
  PWINDOW_OBJECT WindowObject;
  /* Process timer messages. */
  if (Msg->message == WM_TIMER)
    {
      if (Msg->lParam)
	{
	  LARGE_INTEGER LargeTickCount;
	  /* FIXME: Call hooks. */

	  /* FIXME: Check for continuing validity of timer. */
	  
          KeQueryTickCount(&LargeTickCount);
	  return IntCallWindowProc((WNDPROC)Msg->lParam,
                                      FALSE,
                                      Msg->hwnd,
                                      Msg->message,
                                      Msg->wParam,
                                      (LPARAM)LargeTickCount.u.LowPart,
                                      -1);
	}
    }

  if( Msg->hwnd == 0 ) return 0;

  /* Get the window object. */
  WindowObject = IntGetWindowObject(Msg->hwnd);
  if(!WindowObject)
    {
      SetLastWin32Error(ERROR_INVALID_WINDOW_HANDLE);
      return 0;
    }
  if(WindowObject->OwnerThread != PsGetCurrentThread())
  {
    IntReleaseWindowObject(WindowObject);
    DPRINT1("Window doesn't belong to the calling thread!\n");
    return 0;
  }
  /* FIXME: Call hook procedures. */

  /* Call the window procedure. */
  if (0xFFFF0000 != ((DWORD) WindowObject->WndProcW & 0xFFFF0000))
    {
      Result = IntCallWindowProc(WindowObject->WndProcW,
                                 FALSE,
                                 Msg->hwnd,
                                 Msg->message,
                                 Msg->wParam,
                                 Msg->lParam,
                                 -1);
    }
  else
    {
      Result = IntCallWindowProc(WindowObject->WndProcA,
                                 TRUE,
                                 Msg->hwnd,
                                 Msg->message,
                                 Msg->wParam,
                                 Msg->lParam,
                                 -1);
    }
  
  IntReleaseWindowObject(WindowObject);
  
  return Result;
}


LRESULT STDCALL
NtUserDispatchMessage(CONST MSG* UnsafeMsg)
{
  NTSTATUS Status;
  MSG Msg;

  Status = MmCopyFromCaller(&Msg, (PVOID) UnsafeMsg, sizeof(MSG));
  if (! NT_SUCCESS(Status))
    {
    SetLastNtError(Status);
    return 0;
    }
  
  return IntDispatchMessage(&Msg);
}


BOOL STDCALL
NtUserTranslateMessage(LPMSG lpMsg,
		       HKL dwhkl)
{
  NTSTATUS Status;
  MSG SafeMsg;

  Status = MmCopyFromCaller(&SafeMsg, lpMsg, sizeof(MSG));
  if(!NT_SUCCESS(Status))
  {
    SetLastNtError(Status);
    return FALSE;
  }

  return IntTranslateKbdMessage(&SafeMsg, dwhkl);
}


VOID FASTCALL
IntSendHitTestMessages(PUSER_MESSAGE_QUEUE ThreadQueue, LPMSG Msg)
{
  if(!Msg->hwnd || ThreadQueue->CaptureWindow)
  {
    return;
  }
  
  switch(Msg->message)
  {
    case WM_MOUSEMOVE:
    {
      IntSendMessage(Msg->hwnd, WM_SETCURSOR, (WPARAM)Msg->hwnd, MAKELPARAM(HTCLIENT, Msg->message));
      break;
    }
    case WM_NCMOUSEMOVE:
    {
      IntSendMessage(Msg->hwnd, WM_SETCURSOR, (WPARAM)Msg->hwnd, MAKELPARAM(Msg->wParam, Msg->message));
      break;
    }
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_XBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_XBUTTONDBLCLK:
    {
      WPARAM wParam;
      
      if(!IntGetWindowStationObject(InputWindowStation))
      {
        break;
      }
      wParam = (WPARAM)InputWindowStation->SystemCursor.ButtonsDown;
      ObDereferenceObject(InputWindowStation);
      
      IntSendMessage(Msg->hwnd, WM_MOUSEMOVE, wParam, Msg->lParam);
      IntSendMessage(Msg->hwnd, WM_SETCURSOR, (WPARAM)Msg->hwnd, MAKELPARAM(HTCLIENT, Msg->message));
      break;
    }
    case WM_NCLBUTTONDOWN:
    case WM_NCMBUTTONDOWN:
    case WM_NCRBUTTONDOWN:
    case WM_NCXBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCMBUTTONDBLCLK:
    case WM_NCRBUTTONDBLCLK:
    case WM_NCXBUTTONDBLCLK:
    {
      IntSendMessage(Msg->hwnd, WM_NCMOUSEMOVE, (WPARAM)Msg->wParam, Msg->lParam);
      IntSendMessage(Msg->hwnd, WM_SETCURSOR, (WPARAM)Msg->hwnd, MAKELPARAM(Msg->wParam, Msg->message));
      break;
    }
  }
}

BOOL FASTCALL
IntActivateWindowMouse(PUSER_MESSAGE_QUEUE ThreadQueue, LPMSG Msg, PWINDOW_OBJECT MsgWindow, 
                       USHORT *HitTest)
{
  ULONG Result;
  
  if(*HitTest == (USHORT)HTTRANSPARENT)
  {
    /* eat the message, search again! */
    return TRUE;
  }
  
  Result = IntSendMessage(MsgWindow->Self, WM_MOUSEACTIVATE, (WPARAM)NtUserGetParent(MsgWindow->Self), (LPARAM)MAKELONG(*HitTest, Msg->message));
  switch (Result)
  {
    case MA_NOACTIVATEANDEAT:
      return TRUE;
    case MA_NOACTIVATE:
      break;
    case MA_ACTIVATEANDEAT:
      IntMouseActivateWindow(MsgWindow);
      return TRUE;
    default:
      /* MA_ACTIVATE */
      IntMouseActivateWindow(MsgWindow);
      break;
  }
  
  return FALSE;
}

BOOL FASTCALL
IntTranslateMouseMessage(PUSER_MESSAGE_QUEUE ThreadQueue, LPMSG Msg, USHORT *HitTest, BOOL Remove)
{
  PWINDOW_OBJECT Window;
  
  if(!(Window = IntGetWindowObject(Msg->hwnd)))
  {
    /* FIXME - change the mouse cursor to an arrow, maybe do this a better way */
    IntLoadDefaultCursors(TRUE);
    /* let's just eat the message?! */
    return TRUE;
  }
  
  if(ThreadQueue == Window->MessageQueue &&
     ThreadQueue->CaptureWindow != Window->Self)
  {
    /* only send WM_NCHITTEST messages if we're not capturing the window! */
    *HitTest = IntSendMessage(Window->Self, WM_NCHITTEST, 0, 
                              MAKELONG(Msg->pt.x, Msg->pt.y));
  }
  else
  {
    *HitTest = HTCLIENT;
  }
  
  if(IS_BTN_MESSAGE(Msg->message, DOWN))
  {
    /* generate double click messages, if necessary */
    if ((((*HitTest) != HTCLIENT) || 
        (IntGetClassLong(Window, GCL_STYLE, FALSE) & CS_DBLCLKS)) &&
        MsqIsDblClk(Msg, Remove))
    {
      Msg->message += WM_LBUTTONDBLCLK - WM_LBUTTONDOWN;
    }
  }
  
  if(Msg->message != WM_MOUSEWHEEL)
  {
    
    if ((*HitTest) != HTCLIENT)
    {
      Msg->message += WM_NCMOUSEMOVE - WM_MOUSEMOVE;
      if((Msg->message == WM_NCRBUTTONUP) && 
         (((*HitTest) == HTCAPTION) || ((*HitTest) == HTSYSMENU)))
      {
        Msg->message = WM_CONTEXTMENU;
        Msg->wParam = (WPARAM)Window->Self;
      }
      else
      {
        Msg->wParam = *HitTest;
      }
    }
    else if(ThreadQueue->MoveSize == NULL &&
            ThreadQueue->MenuOwner == NULL)
    {
      Msg->pt.x -= Window->ClientRect.left;
      Msg->pt.y -= Window->ClientRect.top;
      Msg->lParam = MAKELONG(Msg->pt.x, Msg->pt.y);
    }
  }
  
  IntReleaseWindowObject(Window);
  return FALSE;
}


/*
 * Internal version of PeekMessage() doing all the work
 */
BOOL FASTCALL
IntPeekMessage(LPMSG Msg,
                HWND Wnd,
                UINT MsgFilterMin,
                UINT MsgFilterMax,
                UINT RemoveMsg)
{
  LARGE_INTEGER LargeTickCount;
  PUSER_MESSAGE_QUEUE ThreadQueue;
  PUSER_MESSAGE Message;
  BOOL Present, RemoveMessages;

  /* The queues and order in which they are checked are documented in the MSDN
     article on GetMessage() */

  ThreadQueue = (PUSER_MESSAGE_QUEUE)PsGetWin32Thread()->MessageQueue;

  /* Inspect RemoveMsg flags */
  /* FIXME: The only flag we process is PM_REMOVE - processing of others must still be implemented */
  RemoveMessages = RemoveMsg & PM_REMOVE;
  
  CheckMessages:
  
  Present = FALSE;
  
  KeQueryTickCount(&LargeTickCount);
  ThreadQueue->LastMsgRead = LargeTickCount.u.LowPart;

  /* Dispatch sent messages here. */
  while (MsqDispatchOneSentMessage(ThreadQueue));
      
  /* Now look for a quit message. */
  
  if (ThreadQueue->QuitPosted)
  {
    /* According to the PSDK, WM_QUIT messages are always returned, regardless
       of the filter specified */
    Msg->hwnd = NULL;
    Msg->message = WM_QUIT;
    Msg->wParam = ThreadQueue->QuitExitCode;
    Msg->lParam = 0;
    if (RemoveMessages)
    {
      ThreadQueue->QuitPosted = FALSE;
    }
    return TRUE;
  }

  /* Now check for normal messages. */
  Present = MsqFindMessage(ThreadQueue,
                           FALSE,
                           RemoveMessages,
                           Wnd,
                           MsgFilterMin,
                           MsgFilterMax,
                           &Message);
  if (Present)
  {
    RtlCopyMemory(Msg, &Message->Msg, sizeof(MSG));
    if (RemoveMessages)
    {
      MsqDestroyMessage(Message);
    }
    goto MessageFound;
  }
  
  /* Check for hardware events. */
  Present = MsqFindMessage(ThreadQueue,
                           TRUE,
                           RemoveMessages,
                           Wnd,
                           MsgFilterMin,
                           MsgFilterMax,
                           &Message);
  if (Present)
  {
    RtlCopyMemory(Msg, &Message->Msg, sizeof(MSG));
    if (RemoveMessages)
    {
      MsqDestroyMessage(Message);
    }
    goto MessageFound;
  }
  
  /* Check for sent messages again. */
  while (MsqDispatchOneSentMessage(ThreadQueue));

  /* Check for paint messages. */
  if (IntGetPaintMessage(Wnd, PsGetWin32Thread(), Msg, RemoveMessages))
  {
    return TRUE;
  }
  
  /* FIXME - get WM_(SYS)TIMER messages */
  
  if(Present)
  {
    MessageFound:
    
    if(RemoveMessages)
    {
      PWINDOW_OBJECT MsgWindow = NULL;;
      
      if(Msg->hwnd && (MsgWindow = IntGetWindowObject(Msg->hwnd)) &&
         Msg->message >= WM_MOUSEFIRST && Msg->message <= WM_MOUSELAST)
      {
        USHORT HitTest;
        
        if(IntTranslateMouseMessage(ThreadQueue, Msg, &HitTest, TRUE))
          /* FIXME - check message filter again, if the message doesn't match anymore,
                     search again */
        {
          IntReleaseWindowObject(MsgWindow);
          /* eat the message, search again */
          goto CheckMessages;
        }
        if(ThreadQueue->CaptureWindow == NULL)
        {
          IntSendHitTestMessages(ThreadQueue, Msg);
          if((Msg->message != WM_MOUSEMOVE && Msg->message != WM_NCMOUSEMOVE) &&
             IS_BTN_MESSAGE(Msg->message, DOWN) &&
             IntActivateWindowMouse(ThreadQueue, Msg, MsgWindow, &HitTest))
          {
            IntReleaseWindowObject(MsgWindow);
            /* eat the message, search again */
            goto CheckMessages;
          }
        }
      }
      else
      {
        IntSendHitTestMessages(ThreadQueue, Msg);
      }
      
      if(MsgWindow)
      {
        IntReleaseWindowObject(MsgWindow);
      }
      
      return TRUE;
    }
    
    USHORT HitTest;
    if((Msg->hwnd && Msg->message >= WM_MOUSEFIRST && Msg->message <= WM_MOUSELAST) &&
       IntTranslateMouseMessage(ThreadQueue, Msg, &HitTest, FALSE))
      /* FIXME - check message filter again, if the message doesn't match anymore,
                 search again */
    {
      /* eat the message, search again */
      goto CheckMessages;
    }
    
    return TRUE;
  }

  return Present;
}

BOOL STDCALL
NtUserPeekMessage(LPMSG UnsafeMsg,
                  HWND Wnd,
                  UINT MsgFilterMin,
                  UINT MsgFilterMax,
                  UINT RemoveMsg)
{
  MSG SafeMsg;
  NTSTATUS Status;
  BOOL Present;
  PWINDOW_OBJECT Window;

  /* Validate input */
  if (NULL != Wnd)
    {
      Window = IntGetWindowObject(Wnd);
      if(!Window)
        Wnd = NULL;
      else
        IntReleaseWindowObject(Window);
    }
  if (MsgFilterMax < MsgFilterMin)
    {
      MsgFilterMin = 0;
      MsgFilterMax = 0;
    }

  Present = IntPeekMessage(&SafeMsg, Wnd, MsgFilterMin, MsgFilterMax, RemoveMsg);
  if (Present)
    {
      Status = MmCopyToCaller(UnsafeMsg, &SafeMsg, sizeof(MSG));
      if (! NT_SUCCESS(Status))
	{
	  /* There is error return documented for PeekMessage().
             Do the best we can */
	  SetLastNtError(Status);
	  return FALSE;
	}
    }

  return Present;
}

static BOOL FASTCALL
IntWaitMessage(HWND Wnd,
                UINT MsgFilterMin,
                UINT MsgFilterMax)
{
  PUSER_MESSAGE_QUEUE ThreadQueue;
  NTSTATUS Status;
  MSG Msg;

  ThreadQueue = (PUSER_MESSAGE_QUEUE)PsGetWin32Thread()->MessageQueue;

  do
    {
      if (IntPeekMessage(&Msg, Wnd, MsgFilterMin, MsgFilterMax, PM_NOREMOVE))
	{
	  return TRUE;
	}

      /* Nothing found. Wait for new messages. */
      Status = MsqWaitForNewMessages(ThreadQueue);
    }
  while (STATUS_WAIT_0 <= STATUS_WAIT_0 && Status <= STATUS_WAIT_63);

  SetLastNtError(Status);

  return FALSE;
}

BOOL STDCALL
NtUserGetMessage(LPMSG UnsafeMsg,
		 HWND Wnd,
		 UINT MsgFilterMin,
		 UINT MsgFilterMax)
/*
 * FUNCTION: Get a message from the calling thread's message queue.
 * ARGUMENTS:
 *      UnsafeMsg - Pointer to the structure which receives the returned message.
 *      Wnd - Window whose messages are to be retrieved.
 *      MsgFilterMin - Integer value of the lowest message value to be
 *                     retrieved.
 *      MsgFilterMax - Integer value of the highest message value to be
 *                     retrieved.
 */
{
  BOOL GotMessage;
  MSG SafeMsg;
  NTSTATUS Status;
  PWINDOW_OBJECT Window;

  /* Validate input */
  if (NULL != Wnd)
    {
      Window = IntGetWindowObject(Wnd);
      if(!Window)
        Wnd = NULL;
      else
        IntReleaseWindowObject(Window);
    }
  if (MsgFilterMax < MsgFilterMin)
    {
      MsgFilterMin = 0;
      MsgFilterMax = 0;
    }

  do
    {
      GotMessage = IntPeekMessage(&SafeMsg, Wnd, MsgFilterMin, MsgFilterMax, PM_REMOVE);
      if (GotMessage)
	{
	  Status = MmCopyToCaller(UnsafeMsg, &SafeMsg, sizeof(MSG));
	  if (! NT_SUCCESS(Status))
	    {
	      SetLastNtError(Status);
	      return (BOOL) -1;
	    }
	}
      else
	{
	  IntWaitMessage(Wnd, MsgFilterMin, MsgFilterMax);
	}
    }
  while (! GotMessage);

  return WM_QUIT != SafeMsg.message;
}

DWORD
STDCALL
NtUserMessageCall(
  DWORD Unknown0,
  DWORD Unknown1,
  DWORD Unknown2,
  DWORD Unknown3,
  DWORD Unknown4,
  DWORD Unknown5,
  DWORD Unknown6)
{
  UNIMPLEMENTED

  return 0;
}

BOOL STDCALL
NtUserPostMessage(HWND hWnd,
		  UINT Msg,
		  WPARAM wParam,
		  LPARAM lParam)
{
  PWINDOW_OBJECT Window;
  MSG Mesg;
  LARGE_INTEGER LargeTickCount;

  if (WM_QUIT == Msg)
    {
      MsqPostQuitMessage(PsGetWin32Thread()->MessageQueue, wParam);
    }
  else if (hWnd == HWND_BROADCAST)
    {
      HWND *List;
      PWINDOW_OBJECT DesktopWindow;
      ULONG i;

      DesktopWindow = IntGetWindowObject(IntGetDesktopWindow());
      List = IntWinListChildren(DesktopWindow);
      IntReleaseWindowObject(DesktopWindow);
      if (List != NULL)
        {
          for (i = 0; List[i]; i++)
            NtUserPostMessage(List[i], Msg, wParam, lParam);
          ExFreePool(List);
        }      
    }
  else
    {
      Window = IntGetWindowObject(hWnd);
      if (!Window)
        {
	      SetLastWin32Error(ERROR_INVALID_WINDOW_HANDLE);
          return FALSE;
        }
      Mesg.hwnd = hWnd;
      Mesg.message = Msg;
      Mesg.wParam = wParam;
      Mesg.lParam = lParam;
      Mesg.pt.x = PsGetWin32Process()->WindowStation->SystemCursor.x;
      Mesg.pt.y = PsGetWin32Process()->WindowStation->SystemCursor.y;
      KeQueryTickCount(&LargeTickCount);
      Mesg.time = LargeTickCount.u.LowPart;
      MsqPostMessage(Window->MessageQueue, &Mesg);
      IntReleaseWindowObject(Window);
    }

  return TRUE;
}

BOOL STDCALL
NtUserPostThreadMessage(DWORD idThread,
			UINT Msg,
			WPARAM wParam,
			LPARAM lParam)
{
  MSG Mesg;

  PETHREAD peThread;
  PW32THREAD pThread;
  NTSTATUS Status;

  Status = PsLookupThreadByThreadId((void *)idThread,&peThread);
  
  if( Status == STATUS_SUCCESS ) {
    pThread = peThread->Win32Thread;
    if( !pThread || !pThread->MessageQueue )
      {
	ObDereferenceObject( peThread );
	return FALSE;
      }
    Mesg.hwnd = 0;
    Mesg.message = Msg;
    Mesg.wParam = wParam;
    Mesg.lParam = lParam;
    MsqPostMessage(pThread->MessageQueue, &Mesg);
    ObDereferenceObject( peThread );
    return TRUE;
  } else {
    SetLastNtError( Status );
    return FALSE;
  }
}

DWORD STDCALL
NtUserQuerySendMessage(DWORD Unknown0)
{
  UNIMPLEMENTED;

  return 0;
}

#define MMS_SIZE_WPARAM      -1
#define MMS_SIZE_WPARAMWCHAR -2
#define MMS_SIZE_LPARAMSZ    -3
#define MMS_SIZE_SPECIAL     -4
#define MMS_FLAG_READ        0x01
#define MMS_FLAG_WRITE       0x02
#define MMS_FLAG_READWRITE   (MMS_FLAG_READ | MMS_FLAG_WRITE)
typedef struct tagMSGMEMORY
  {
    UINT Message;
    UINT Size;
    INT Flags;
  } MSGMEMORY, *PMSGMEMORY;

static MSGMEMORY MsgMemory[] =
  {
    { WM_CREATE, sizeof(CREATESTRUCTW), MMS_FLAG_READWRITE },
    { WM_GETMINMAXINFO, sizeof(MINMAXINFO), MMS_FLAG_READWRITE },
    { WM_GETTEXT, MMS_SIZE_WPARAMWCHAR, MMS_FLAG_WRITE },
    { WM_NCCALCSIZE, MMS_SIZE_SPECIAL, MMS_FLAG_READWRITE },
    { WM_NCCREATE, sizeof(CREATESTRUCTW), MMS_FLAG_READWRITE },
    { WM_SETTEXT, MMS_SIZE_LPARAMSZ, MMS_FLAG_READ },
    { WM_STYLECHANGED, sizeof(STYLESTRUCT), MMS_FLAG_READ },
    { WM_STYLECHANGING, sizeof(STYLESTRUCT), MMS_FLAG_READWRITE },
    { WM_WINDOWPOSCHANGED, sizeof(WINDOWPOS), MMS_FLAG_READ },
    { WM_WINDOWPOSCHANGING, sizeof(WINDOWPOS), MMS_FLAG_READWRITE },
  };

static PMSGMEMORY FASTCALL
FindMsgMemory(UINT Msg)
  {
  PMSGMEMORY MsgMemoryEntry;

  /* See if this message type is present in the table */
  for (MsgMemoryEntry = MsgMemory;
       MsgMemoryEntry < MsgMemory + sizeof(MsgMemory) / sizeof(MSGMEMORY);
       MsgMemoryEntry++)
    {
      if (Msg == MsgMemoryEntry->Message)
        {
          return MsgMemoryEntry;
        }
    }

  return NULL;
}

static UINT FASTCALL
MsgMemorySize(PMSGMEMORY MsgMemoryEntry, WPARAM wParam, LPARAM lParam)
{
  if (MMS_SIZE_WPARAM == MsgMemoryEntry->Size)
    {
      return (UINT) wParam;
    }
  else if (MMS_SIZE_WPARAMWCHAR == MsgMemoryEntry->Size)
    {
      return (UINT) (wParam * sizeof(WCHAR));
    }
  else if (MMS_SIZE_LPARAMSZ == MsgMemoryEntry->Size)
    {
      return (UINT) ((wcslen((PWSTR) lParam) + 1) * sizeof(WCHAR));
    }
  else if (MMS_SIZE_SPECIAL == MsgMemoryEntry->Size)
    {
      switch(MsgMemoryEntry->Message)
        {
        case WM_NCCALCSIZE:
          return wParam ? sizeof(NCCALCSIZE_PARAMS) + sizeof(WINDOWPOS) : sizeof(RECT);
          break;
        default:
          assert(FALSE);
          return 0;
          break;
        }
    }
  else
    {
      return MsgMemoryEntry->Size;
    }
}

static NTSTATUS FASTCALL
PackParam(LPARAM *lParamPacked, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  NCCALCSIZE_PARAMS *UnpackedParams;
  NCCALCSIZE_PARAMS *PackedParams;

  *lParamPacked = lParam;
  if (WM_NCCALCSIZE == Msg && wParam)
    {
      UnpackedParams = (NCCALCSIZE_PARAMS *) lParam;
      if (UnpackedParams->lppos != (PWINDOWPOS) (UnpackedParams + 1))
        {
          PackedParams = ExAllocatePoolWithTag(PagedPool,
                                               sizeof(NCCALCSIZE_PARAMS) + sizeof(WINDOWPOS),
                                               TAG_MSG);
          if (NULL == PackedParams)
            {
              DPRINT1("Not enough memory to pack lParam\n");
              return STATUS_NO_MEMORY;
            }
          RtlCopyMemory(PackedParams, UnpackedParams, sizeof(NCCALCSIZE_PARAMS));
          PackedParams->lppos = (PWINDOWPOS) (PackedParams + 1);
          RtlCopyMemory(PackedParams->lppos, UnpackedParams->lppos, sizeof(WINDOWPOS));
          *lParamPacked = (LPARAM) PackedParams;
        }
    }

  return STATUS_SUCCESS;
}

static FASTCALL NTSTATUS
UnpackParam(LPARAM lParamPacked, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  NCCALCSIZE_PARAMS *UnpackedParams;
  NCCALCSIZE_PARAMS *PackedParams;
  PWINDOWPOS UnpackedWindowPos;

  if (lParamPacked == lParam)
    {
      return STATUS_SUCCESS;
    }

  if (WM_NCCALCSIZE == Msg && wParam)
    {
      PackedParams = (NCCALCSIZE_PARAMS *) lParamPacked;
      UnpackedParams = (NCCALCSIZE_PARAMS *) lParam;
      UnpackedWindowPos = UnpackedParams->lppos;
      RtlCopyMemory(UnpackedParams, PackedParams, sizeof(NCCALCSIZE_PARAMS));
      UnpackedParams->lppos = UnpackedWindowPos;
      RtlCopyMemory(UnpackedWindowPos, PackedParams + 1, sizeof(WINDOWPOS));
      ExFreePool((PVOID) lParamPacked);

      return STATUS_SUCCESS;
    }

  assert(FALSE);

  return STATUS_INVALID_PARAMETER;
}

LRESULT FASTCALL
IntSendMessage(HWND hWnd,
               UINT Msg,
               WPARAM wParam,
               LPARAM lParam)
{
  LRESULT Result = 0;
  if(IntSendMessageTimeout(hWnd, Msg, wParam, lParam, SMTO_NORMAL, 0, &Result))
  {
    return Result;
  }
  return 0;
}

static LRESULT FASTCALL
IntSendMessageTimeoutSingle(HWND hWnd,
                            UINT Msg,
                            WPARAM wParam,
                            LPARAM lParam,
                            UINT uFlags,
                            UINT uTimeout,
                            ULONG_PTR *uResult)
{
  LRESULT Result;
  NTSTATUS Status;
  PWINDOW_OBJECT Window;
  PMSGMEMORY MsgMemoryEntry;
  INT lParamBufferSize;
  LPARAM lParamPacked;
  PW32THREAD Win32Thread;

  /* FIXME: Call hooks. */
  Window = IntGetWindowObject(hWnd);
  if (!Window)
    {
      SetLastWin32Error(ERROR_INVALID_WINDOW_HANDLE);
      return FALSE;
    }

  Win32Thread = PsGetWin32Thread();

  if (NULL != Win32Thread &&
      Window->MessageQueue == Win32Thread->MessageQueue)
    {
      if (Win32Thread->IsExiting)
        {
          /* Never send messages to exiting threads */
          IntReleaseWindowObject(Window);
          return FALSE;
        }

      /* See if this message type is present in the table */
      MsgMemoryEntry = FindMsgMemory(Msg);
      if (NULL == MsgMemoryEntry)
        {
          lParamBufferSize = -1;
        }
      else
        {
          lParamBufferSize = MsgMemorySize(MsgMemoryEntry, wParam, lParam);
        }

      if (! NT_SUCCESS(PackParam(&lParamPacked, Msg, wParam, lParam)))
        {
          IntReleaseWindowObject(Window);
          DPRINT1("Failed to pack message parameters\n");
          return FALSE;
        }
      if (0xFFFF0000 != ((DWORD) Window->WndProcW & 0xFFFF0000))
        {
          Result = IntCallWindowProc(Window->WndProcW, FALSE, hWnd, Msg, wParam,
                                     lParamPacked,lParamBufferSize);
        }
      else
        {
          Result = IntCallWindowProc(Window->WndProcA, TRUE, hWnd, Msg, wParam,
                                     lParamPacked,lParamBufferSize);
        }
      if (! NT_SUCCESS(UnpackParam(lParamPacked, Msg, wParam, lParam)))
        {
          IntReleaseWindowObject(Window);
          DPRINT1("Failed to unpack message parameters\n");
          if(uResult)
            *uResult = Result;
          return TRUE;
        }

      IntReleaseWindowObject(Window);
      if(uResult)
        *uResult = Result;
      return TRUE;
    }
  
  if(uFlags & SMTO_ABORTIFHUNG && MsqIsHung(Window->MessageQueue))
  {
    IntReleaseWindowObject(Window);
    /* FIXME - Set a LastError? */
    return FALSE;
  }
  
  Status = MsqSendMessage(Window->MessageQueue, hWnd, Msg, wParam, lParam, 
                          uTimeout, (uFlags & SMTO_BLOCK), &Result);
  if(Status == STATUS_TIMEOUT)
  {
    IntReleaseWindowObject(Window);
    SetLastWin32Error(ERROR_TIMEOUT);
    return FALSE;
  }

  IntReleaseWindowObject(Window);
  if(uResult)
    *uResult = Result;
  return TRUE;
}

LRESULT FASTCALL
IntSendMessageTimeout(HWND hWnd,
                      UINT Msg,
                      WPARAM wParam,
                      LPARAM lParam,
                      UINT uFlags,
                      UINT uTimeout,
                      ULONG_PTR *uResult)
{
  PWINDOW_OBJECT DesktopWindow;
  HWND *Children;
  HWND *Child;

  if (HWND_BROADCAST != hWnd)
    {
      return IntSendMessageTimeoutSingle(hWnd, Msg, wParam, lParam, uFlags, uTimeout, uResult);
    }

  DesktopWindow = IntGetWindowObject(IntGetDesktopWindow());
  if (NULL == DesktopWindow)
    {
      SetLastWin32Error(ERROR_INTERNAL_ERROR);
      return 0;
    }
  Children = IntWinListChildren(DesktopWindow);
  IntReleaseWindowObject(DesktopWindow);
  if (NULL == Children)
    {
      return 0;
    }

  for (Child = Children; NULL != *Child; Child++)
    {
      IntSendMessageTimeoutSingle(*Child, Msg, wParam, lParam, uFlags, uTimeout, uResult);
    }

  ExFreePool(Children);

  return (LRESULT) TRUE;
}

static NTSTATUS FASTCALL
CopyMsgToKernelMem(MSG *KernelModeMsg, MSG *UserModeMsg)
{
  NTSTATUS Status;
  PMSGMEMORY MsgMemoryEntry;
  PVOID KernelMem;
  UINT Size;

  *KernelModeMsg = *UserModeMsg;

  /* See if this message type is present in the table */
  MsgMemoryEntry = FindMsgMemory(UserModeMsg->message);
  if (NULL == MsgMemoryEntry)
    {
      /* Not present, no copying needed */
      return STATUS_SUCCESS;
    }

  /* Determine required size */
  Size = MsgMemorySize(MsgMemoryEntry, UserModeMsg->wParam, UserModeMsg->lParam);

  if (0 != Size)
    {
      /* Allocate kernel mem */
      KernelMem = ExAllocatePoolWithTag(PagedPool, Size, TAG_MSG);
      if (NULL == KernelMem)
        {
          DPRINT1("Not enough memory to copy message to kernel mem\n");
          return STATUS_NO_MEMORY;
        }
      KernelModeMsg->lParam = (LPARAM) KernelMem;

      /* Copy data if required */
      if (0 != (MsgMemoryEntry->Flags & MMS_FLAG_READ))
        {
          Status = MmCopyFromCaller(KernelMem, (PVOID) UserModeMsg->lParam, Size);
          if (! NT_SUCCESS(Status))
            {
              DPRINT1("Failed to copy message to kernel: invalid usermode buffer\n");
              ExFreePool(KernelMem);
              return Status;
            }
        }
      else
        {
          /* Make sure we don't pass any secrets to usermode */
          RtlZeroMemory(KernelMem, Size);
        }
    }
  else
    {
      KernelModeMsg->lParam = 0;
    }

  return STATUS_SUCCESS;
}

/* This function posts a message if the destination's message queue belongs to
   another thread, otherwise it sends the message. It does not support broadcast
   messages! */
LRESULT FASTCALL
IntPostOrSendMessage(HWND hWnd,
                     UINT Msg,
                     WPARAM wParam,
                     LPARAM lParam)
{
  LRESULT Result;
  PWINDOW_OBJECT Window;
  
  if(hWnd == HWND_BROADCAST)
  {
    return 0;
  }
  
  Window = IntGetWindowObject(hWnd);
  if(!Window)
  {
    SetLastWin32Error(ERROR_INVALID_WINDOW_HANDLE);
    return 0;
  }
  
  if(Window->MessageQueue != PsGetWin32Thread()->MessageQueue)
  {
    Result = NtUserPostMessage(hWnd, Msg, wParam, lParam);
  }
  else
  {
    if(!IntSendMessageTimeoutSingle(hWnd, Msg, wParam, lParam, SMTO_NORMAL, 0, &Result))
    {
      Result = 0;
    }
  }
  
  IntReleaseWindowObject(Window);
  
  return Result;
}

static NTSTATUS FASTCALL
CopyMsgToUserMem(MSG *UserModeMsg, MSG *KernelModeMsg)
{
  NTSTATUS Status;
  PMSGMEMORY MsgMemoryEntry;
  UINT Size;

  /* See if this message type is present in the table */
  MsgMemoryEntry = FindMsgMemory(UserModeMsg->message);
  if (NULL == MsgMemoryEntry)
    {
      /* Not present, no copying needed */
      return STATUS_SUCCESS;
    }

  /* Determine required size */
  Size = MsgMemorySize(MsgMemoryEntry, UserModeMsg->wParam, UserModeMsg->lParam);

  if (0 != Size)
    {
      /* Copy data if required */
      if (0 != (MsgMemoryEntry->Flags & MMS_FLAG_WRITE))
        {
          Status = MmCopyToCaller((PVOID) UserModeMsg->lParam, (PVOID) KernelModeMsg->lParam, Size);
          if (! NT_SUCCESS(Status))
            {
              DPRINT1("Failed to copy message from kernel: invalid usermode buffer\n");
              ExFreePool((PVOID) KernelModeMsg->lParam);
              return Status;
            }
        }

      ExFreePool((PVOID) KernelModeMsg->lParam);
    }

  return STATUS_SUCCESS;
}

LRESULT FASTCALL
IntDoSendMessage(HWND Wnd,
		 UINT Msg,
		 WPARAM wParam,
		 LPARAM lParam,
		 PDOSENDMESSAGE dsm,
	         PNTUSERSENDMESSAGEINFO UnsafeInfo)
{
  LRESULT Result;
  NTSTATUS Status;
  PWINDOW_OBJECT Window;
  NTUSERSENDMESSAGEINFO Info;
  MSG UserModeMsg;
  MSG KernelModeMsg;

  RtlZeroMemory(&Info, sizeof(NTUSERSENDMESSAGEINFO));

  /* FIXME: Call hooks. */
  if (HWND_BROADCAST != Wnd)
    {
      Window = IntGetWindowObject(Wnd);
      if (NULL == Window)
        {
          /* Tell usermode to not touch this one */
          Info.HandledByKernel = TRUE;
          MmCopyToCaller(UnsafeInfo, &Info, sizeof(NTUSERSENDMESSAGEINFO));
          SetLastWin32Error(ERROR_INVALID_WINDOW_HANDLE);
          return 0;
        }
    }

  /* FIXME: Check for an exiting window. */

  /* See if the current thread can handle the message */
  if (HWND_BROADCAST != Wnd && NULL != PsGetWin32Thread() &&
      Window->MessageQueue == PsGetWin32Thread()->MessageQueue)
    {
      /* Gather the information usermode needs to call the window proc directly */
      Info.HandledByKernel = FALSE;
      if (0xFFFF0000 != ((DWORD) Window->WndProcW & 0xFFFF0000))
        {
          if (0xFFFF0000 != ((DWORD) Window->WndProcA & 0xFFFF0000))
            {
              /* Both Unicode and Ansi winprocs are real, see what usermode prefers */
              Status = MmCopyFromCaller(&(Info.Ansi), &(UnsafeInfo->Ansi),
                                        sizeof(BOOL));
              if (! NT_SUCCESS(Status))
                {
                  Info.Ansi = ! Window->Unicode;
                }
              Info.Proc = (Info.Ansi ? Window->WndProcA : Window->WndProcW);
            }
          else
            {
              /* Real Unicode winproc */
              Info.Ansi = FALSE;
              Info.Proc = Window->WndProcW;
            }
        }
      else
        {
          /* Must have real Ansi winproc */
          Info.Ansi = TRUE;
          Info.Proc = Window->WndProcA;
        }
      IntReleaseWindowObject(Window);
    }
  else
    {
      /* Must be handled by other thread */
      if (HWND_BROADCAST != Wnd)
        {
          IntReleaseWindowObject(Window);
        }
      Info.HandledByKernel = TRUE;
      UserModeMsg.hwnd = Wnd;
      UserModeMsg.message = Msg;
      UserModeMsg.wParam = wParam;
      UserModeMsg.lParam = lParam;
      Status = CopyMsgToKernelMem(&KernelModeMsg, &UserModeMsg);
      if (! NT_SUCCESS(Status))
        {
          MmCopyToCaller(UnsafeInfo, &Info, sizeof(NTUSERSENDMESSAGEINFO));
          SetLastWin32Error(ERROR_INVALID_PARAMETER);
          return (dsm ? 0 : -1);
        }
      if(!dsm)
      {
        Result = IntSendMessage(KernelModeMsg.hwnd, KernelModeMsg.message,
                                KernelModeMsg.wParam, KernelModeMsg.lParam);
      }
      else
      {
        Result = IntSendMessageTimeout(KernelModeMsg.hwnd, KernelModeMsg.message,
                                       KernelModeMsg.wParam, KernelModeMsg.lParam,
                                       dsm->uFlags, dsm->uTimeout, &dsm->uResult);
      }
      Status = CopyMsgToUserMem(&UserModeMsg, &KernelModeMsg);
      if (! NT_SUCCESS(Status))
        {
          MmCopyToCaller(UnsafeInfo, &Info, sizeof(NTUSERSENDMESSAGEINFO));
          SetLastWin32Error(ERROR_INVALID_PARAMETER);
          return(dsm ? 0 : -1);
        }
    }

  Status = MmCopyToCaller(UnsafeInfo, &Info, sizeof(NTUSERSENDMESSAGEINFO));
  if (! NT_SUCCESS(Status))
    {
      SetLastWin32Error(ERROR_INVALID_PARAMETER);
    }

  return Result;
}

LRESULT STDCALL
NtUserSendMessageTimeout(HWND hWnd,
			 UINT Msg,
			 WPARAM wParam,
			 LPARAM lParam,
			 UINT uFlags,
			 UINT uTimeout,
			 ULONG_PTR *uResult,
	                 PNTUSERSENDMESSAGEINFO UnsafeInfo)
{
  DOSENDMESSAGE dsm;
  LRESULT Result;

  dsm.uFlags = uFlags;
  dsm.uTimeout = uTimeout;
  Result = IntDoSendMessage(hWnd, Msg, wParam, lParam, &dsm, UnsafeInfo);
  if(uResult)
  {
    NTSTATUS Status;
    
    Status = MmCopyToCaller(uResult, &dsm.uResult, sizeof(ULONG_PTR));
    if(!NT_SUCCESS(Status))
    {
      SetLastWin32Error(ERROR_INVALID_PARAMETER);
      return FALSE;
    }
  }
  return Result;
}

LRESULT STDCALL
NtUserSendMessage(HWND Wnd,
		  UINT Msg,
		  WPARAM wParam,
		  LPARAM lParam,
                  PNTUSERSENDMESSAGEINFO UnsafeInfo)
{
  return IntDoSendMessage(Wnd, Msg, wParam, lParam, NULL, UnsafeInfo);
}

BOOL STDCALL
NtUserSendMessageCallback(HWND hWnd,
			  UINT Msg,
			  WPARAM wParam,
			  LPARAM lParam,
			  SENDASYNCPROC lpCallBack,
			  ULONG_PTR dwData)
{
  UNIMPLEMENTED;

  return 0;
}

BOOL STDCALL
NtUserSendNotifyMessage(HWND hWnd,
			UINT Msg,
			WPARAM wParam,
			LPARAM lParam)
{
  UNIMPLEMENTED;

  return 0;
}

BOOL STDCALL
NtUserWaitMessage(VOID)
{

  return IntWaitMessage(NULL, 0, 0);
}

DWORD STDCALL
NtUserGetQueueStatus(BOOL ClearChanges)
{
   PUSER_MESSAGE_QUEUE Queue;
   DWORD Result;

   Queue = PsGetWin32Thread()->MessageQueue;

   IntLockMessageQueue(Queue);

   Result = MAKELONG(Queue->ChangedBits, Queue->WakeBits);
   if (ClearChanges)
   {
      Queue->ChangedBits = 0;
   }

   IntUnLockMessageQueue(Queue);

   return Result;
}

BOOL STDCALL
IntInitMessagePumpHook()
{
	PsGetCurrentThread()->Win32Thread->MessagePumpHookValue++;
	return TRUE;
}

BOOL STDCALL
IntUninitMessagePumpHook()
{
	if (PsGetCurrentThread()->Win32Thread->MessagePumpHookValue <= 0)
	{
		return FALSE;
	}
	PsGetCurrentThread()->Win32Thread->MessagePumpHookValue--;
	return TRUE;
}

/* EOF */
