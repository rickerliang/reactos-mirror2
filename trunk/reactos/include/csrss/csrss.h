#ifndef __INCLUDE_CSRSS_CSRSS_H
#define __INCLUDE_CSRSS_CSRSS_H

#include <windows.h>
#include <ddk/ntddblue.h>
#include <ntos/keyboard.h>

#define CSR_PRIORITY_CLASS_NORMAL	(0x10)
#define CSR_PRIORITY_CLASS_IDLE		(0x20)
#define CSR_PRIORITY_CLASS_HIGH		(0x40)
#define CSR_PRIORITY_CLASS_REALTIME	(0x80)

#define CSR_CSRSS_SECTION_SIZE          (65536)

typedef __declspec(noreturn) VOID CALLBACK(*PCONTROLDISPATCHER)(DWORD);

typedef struct
{
} CSRSS_CONNECT_PROCESS_REQUEST, PCSRSS_CONNECT_PROCESS_REQUEST;

typedef struct
{
} CSRSS_CONNECT_PROCESS_REPLY, PCSRSS_CONNECT_PROCESS_REPLY;

typedef struct
{
   ULONG NewProcessId;
   ULONG Flags;
   PCONTROLDISPATCHER CtrlDispatcher;
} CSRSS_CREATE_PROCESS_REQUEST, *PCSRSS_CREATE_PROCESS_REQUEST;

typedef struct
{
   HANDLE Console;
   HANDLE InputHandle;
   HANDLE OutputHandle;
} CSRSS_CREATE_PROCESS_REPLY, *PCSRSS_CREATE_PROCESS_REPLY;

typedef struct
{
} CSRSS_TERMINATE_PROCESS_REQUEST, PCSRSS_TERMINATE_PROCESS_REQUEST;

typedef struct
{
} CSRSS_TERMINATE_PROCESS_REPLY, PCSRSS_TERMINATE_PROCESS_REPLY;

typedef struct
{
   HANDLE ConsoleHandle;
   ULONG NrCharactersToWrite;
   BYTE Buffer[1];
} CSRSS_WRITE_CONSOLE_REQUEST, *PCSRSS_WRITE_CONSOLE_REQUEST;

typedef struct
{
   ULONG NrCharactersWritten;
} CSRSS_WRITE_CONSOLE_REPLY, *PCSRSS_WRITE_CONSOLE_REPLY;

typedef struct
{
   HANDLE ConsoleHandle;
   WORD NrCharactersToRead;
   WORD nCharsCanBeDeleted;     /* number of chars already in buffer that can be backspaced */
} CSRSS_READ_CONSOLE_REQUEST, *PCSRSS_READ_CONSOLE_REQUEST;

typedef struct
{
   HANDLE EventHandle;
   ULONG NrCharactersRead;
   BYTE Buffer[1];
} CSRSS_READ_CONSOLE_REPLY, *PCSRSS_READ_CONSOLE_REPLY;

typedef struct
{
   PCONTROLDISPATCHER CtrlDispatcher;
} CSRSS_ALLOC_CONSOLE_REQUEST, *PCSRSS_ALLOC_CONSOLE_REQUEST;

typedef struct
{
   HANDLE Console;
   HANDLE InputHandle;
   HANDLE OutputHandle;
} CSRSS_ALLOC_CONSOLE_REPLY, *PCSRSS_ALLOC_CONSOLE_REPLY;

typedef struct
{
} CSRSS_FREE_CONSOLE_REQUEST, *PCSRSS_FREE_CONSOLE_REQUEST;

typedef struct
{
} CSRSS_FREE_CONSOLE_REPLY, *PCSRSS_FREE_CONSOLE_REPLY;

typedef struct
{
   HANDLE ConsoleHandle;
} CSRSS_SCREEN_BUFFER_INFO_REQUEST, *PCSRSS_SCREEN_BUFFER_INFO_REQUEST;

typedef struct
{
   CONSOLE_SCREEN_BUFFER_INFO Info;
} CSRSS_SCREEN_BUFFER_INFO_REPLY, *PCSRSS_SCREEN_BUFFER_INFO_REPLY;

typedef struct
{
   HANDLE ConsoleHandle;
   COORD Position;
} CSRSS_SET_CURSOR_REQUEST, *PCSRSS_SET_CURSOR_REQUEST;

typedef struct
{
} CSRSS_SET_CURSOR_REPLY, *PCSRSS_SET_CURSOR_REPLY;

typedef struct
{
   HANDLE ConsoleHandle;
   CHAR Char;
   COORD Position;
   WORD Length;
} CSRSS_FILL_OUTPUT_REQUEST, *PCSRSS_FILL_OUTPUT_REQUEST;

typedef struct
{
} CSRSS_FILL_OUTPUT_REPLY, *PCSRSS_FILL_OUTPUT_REPLY;

typedef struct
{
   HANDLE ConsoleHandle;
   CHAR Attribute;
   COORD Coord;
   WORD Length;
} CSRSS_FILL_OUTPUT_ATTRIB_REQUEST, *PCSRSS_FILL_OUTPUT_ATTRIB_REQUEST;

typedef struct
{
} CSRSS_FILL_OUTPUT_ATTRIB_REPLY, *PCSRSS_FILL_OUTPUT_ATTRIB_REPLY;

typedef struct
{
   HANDLE ConsoleHandle;
} CSRSS_READ_INPUT_REQUEST, *PCSRSS_READ_INPUT_REQUEST;

typedef struct
{
   INPUT_RECORD Input;
   BOOL MoreEvents;
   HANDLE Event;
} CSRSS_READ_INPUT_REPLY, *PCSRSS_READ_INPUT_REPLY;

typedef struct
{
   HANDLE ConsoleHandle;
   WORD Length;
   COORD Coord;
   CHAR String[1];
} CSRSS_WRITE_CONSOLE_OUTPUT_CHAR_REQUEST, *PCSRSS_WRITE_CONSOLE_OUTPUT_CHAR_REQUEST;

typedef struct
{
   COORD EndCoord;
} CSRSS_WRITE_CONSOLE_OUTPUT_CHAR_REPLY, *PCSRSS_WRITE_CONSOLE_OUTPUT_CHAR_REPLY;

typedef struct
{
   HANDLE ConsoleHandle;
   WORD Length;
   COORD Coord;
   CHAR String[1];
} CSRSS_WRITE_CONSOLE_OUTPUT_ATTRIB_REQUEST, *PCSRSS_WRITE_CONSOLE_OUTPUT_ATTRIB_REQUEST;

typedef struct
{
   COORD EndCoord;
} CSRSS_WRITE_CONSOLE_OUTPUT_ATTRIB_REPLY, *PCSRSS_WRITE_CONSOLE_OUTPUT_ATTRIB_REPLY;

typedef struct
{
   HANDLE ConsoleHandle;
} CSRSS_GET_CURSOR_INFO_REQUEST, *PCSRSS_GET_CURSOR_INFO_REQUEST;

typedef struct
{
   CONSOLE_CURSOR_INFO Info;
} CSRSS_GET_CURSOR_INFO_REPLY, *PCSRSS_GET_CURSOR_INFO_REPLY;

typedef struct
{
   HANDLE ConsoleHandle;
   CONSOLE_CURSOR_INFO Info;
} CSRSS_SET_CURSOR_INFO_REQUEST, *PCSRSS_SET_CURSOR_INFO_REQUEST;

typedef struct
{
} CSRSS_SET_CURSOR_INFO_REPLY, *PCSRSS_SET_CURSOR_INFO_REPLY;

typedef struct
{
   HANDLE ConsoleHandle;
   CHAR Attrib;
} CSRSS_SET_ATTRIB_REQUEST, *PCSRSS_SET_ATTRIB_REQUEST;

typedef struct
{
} CSRSS_SET_ATTRIB_REPLY, *PCSRSS_SET_ATTRIB_REPLY;

typedef struct
{
  HANDLE ConsoleHandle;
  DWORD Mode;
} CSRSS_SET_CONSOLE_MODE_REQUEST, *PCSRSS_SET_CONSOLE_MODE_REQUEST;

typedef struct
{
} CSRSS_SET_CONSOLE_MODE_REPLY, *PCSRSS_SET_CONSOLE_MODE_REPLY;

typedef struct
{
  HANDLE ConsoleHandle;
} CSRSS_GET_CONSOLE_MODE_REQUEST, *PCSRSS_GET_CONSOLE_MODE_REQUEST;

typedef struct
{
  DWORD ConsoleMode;
} CSRSS_GET_CONSOLE_MODE_REPLY, *PCSRSS_GET_CONSOLE_MODE_REPLY;

typedef struct
{
  /* may want to add some parameters here someday */
} CSRSS_CREATE_SCREEN_BUFFER_REQUEST, *PCSRSS_CREATE_SCREEN_BUFFER_REQUEST;

typedef struct
{
   HANDLE OutputHandle;  /* handle to newly created screen buffer */
} CSRSS_CREATE_SCREEN_BUFFER_REPLY, *PCSRSS_CREATE_SCREEN_BUFFER_REPLY;

typedef struct
{
   HANDLE OutputHandle;  /* handle to screen buffer to switch to */
} CSRSS_SET_SCREEN_BUFFER_REQUEST, *PCSRSS_SET_SCREEN_BUFFER_REQUEST;

typedef struct
{
} CSRSS_SET_SCREEN_BUFFER_REPLY, *PCSRSS_SET_SCREEN_BUFFER_REPLY;

typedef struct
{
	HANDLE	UniqueThread;
} CSRSS_IDENTIFY_ALERTABLE_THREAD_REQUEST, * PCSRSS_IDENTIFY_ALERTABLE_THREAD_REQUEST;

typedef struct
{
	CLIENT_ID	Cid;
} CSRSS_IDENTIFY_ALERTABLE_THREAD_REPLY, * PCSRSS_IDENTIFY_ALERTABLE_THREAD_REPLY;

typedef struct
{
  HANDLE Console;
  DWORD Length;
  WCHAR Title[1];
} CSRSS_SET_TITLE_REQUEST, *PCSRSS_SET_TITLE_REQUEST;

typedef struct
{
} CSRSS_SET_TITLE_REPLY, *PCSRSS_SET_TITLE_REPLY;

typedef struct
{
   HANDLE ConsoleHandle;
} CSRSS_GET_TITLE_REQUEST, *PCSRSS_GET_TITLE_REQUEST;

typedef struct
{
  HANDLE ConsoleHandle;
  DWORD Length;
  WCHAR Title[1];
} CSRSS_GET_TITLE_REPLY, *PCSRSS_GET_TITLE_REPLY;

typedef struct
{
  HANDLE ConsoleHandle;
  COORD BufferSize;
  COORD BufferCoord;
  SMALL_RECT WriteRegion;
  CHAR_INFO* CharInfo;
} CSRSS_WRITE_CONSOLE_OUTPUT_REQUEST, *PCSRSS_WRITE_CONSOLE_OUTPUT_REQUEST;

typedef struct
{
  SMALL_RECT WriteRegion;
} CSRSS_WRITE_CONSOLE_OUTPUT_REPLY, *PCSRSS_WRITE_CONSOLE_OUTPUT_REPLY;

typedef struct
{
   HANDLE ConsoleInput;
} CSRSS_FLUSH_INPUT_BUFFER_REQUEST, *PCSRSS_FLUSH_INPUT_BUFFER_REQUEST;

typedef struct
{
} CSRSS_FLUSH_INPUT_BUFFER_REPLY, *PCSRSS_FLUSH_INPUT_BUFFER_REPLY;

typedef struct
{
  HANDLE     ConsoleHandle;
  SMALL_RECT ScrollRectangle;
  BOOLEAN    UseClipRectangle;
  SMALL_RECT ClipRectangle;
  COORD      DestinationOrigin;
  CHAR_INFO  Fill;
} CSRSS_SCROLL_CONSOLE_SCREEN_BUFFER_REQUEST, *PCSRSS_SCROLL_CONSOLE_SCREEN_BUFFER_REQUEST;

typedef struct
{
} CSRSS_SCROLL_CONSOLE_SCREEN_BUFFER_REPLY, *PCSRSS_SCROLL_CONSOLE_SCREEN_BUFFER_REPLY;

typedef struct
{
  HANDLE ConsoleHandle;
  DWORD NumCharsToRead;
  COORD ReadCoord;
}CSRSS_READ_CONSOLE_OUTPUT_CHAR_REQUEST, *PCSRSS_READ_CONSOLE_OUTPUT_CHAR_REQUEST;

typedef struct
{
  COORD EndCoord;
  CHAR String[1];
}CSRSS_READ_CONSOLE_OUTPUT_CHAR_REPLY, *PCSRSS_READ_CONSOLE_OUTPUT_CHAR_REPLY;

typedef struct
{
  HANDLE ConsoleHandle;
  DWORD NumAttrsToRead;
  COORD ReadCoord;
}CSRSS_READ_CONSOLE_OUTPUT_ATTRIB_REQUEST, *PCSRSS_READ_CONSOLE_OUTPUT_ATTRIB_REQUEST;

typedef struct
{
  COORD EndCoord;
  CHAR String[1];
}CSRSS_READ_CONSOLE_OUTPUT_ATTRIB_REPLY, *PCSRSS_READ_CONSOLE_OUTPUT_ATTRIB_REPLY;

typedef struct
{
  HANDLE ConsoleHandle;
}CSRSS_GET_NUM_INPUT_EVENTS_REQUEST, *PCSRSS_GET_NUM_INPUT_EVENTS_REQUEST;

typedef struct
{
   DWORD NumInputEvents;
}CSRSS_GET_NUM_INPUT_EVENTS_REPLY, *PCSRSS_GET_NUM_INPUT_EVENTS_REPLY;

typedef struct
{
  DWORD ProcessId;
} CSRSS_REGISTER_SERVICES_PROCESS_REQUEST, *PCSRSS_REGISTER_SERVICES_PROCESS_REQUEST;

typedef struct
{
} CSRSS_REGISTER_SERVICES_PROCESS_REPLY, *PCSRSS_REGISTER_SERVICES_PROCESS_REPLY;

typedef struct
{
  UINT Flags;
  DWORD Reserved;
} CSRSS_EXIT_REACTOS_REQUEST, *PCSRSS_EXIT_REACTOS_REQUEST;

typedef struct
{
} CSRSS_EXIT_REACTOS_REPLY, *PCSRSS_EXIT_REACTOS_REPLY;

typedef struct
{
  DWORD Level;
  DWORD Flags;
} CSRSS_SET_SHUTDOWN_PARAMETERS_REQUEST, *PCSRSS_SET_SHUTDOWN_PARAMETERS_REQUEST;

typedef struct
{
} CSRSS_SET_SHUTDOWN_PARAMETERS_REPLY, *PCSRSS_SET_SHUTDOWN_PARAMETERS_REPLY;

typedef struct
{
} CSRSS_GET_SHUTDOWN_PARAMETERS_REQUEST, *PCSRSS_GET_SHUTDOWN_PARAMETERS_REQUEST;

typedef struct
{
  DWORD Level;
  DWORD Flags;
} CSRSS_GET_SHUTDOWN_PARAMETERS_REPLY, *PCSRSS_GET_SHUTDOWN_PARAMETERS_REPLY;

typedef struct
{
  HANDLE ConsoleHandle;
  DWORD Length;
  INPUT_RECORD* InputRecord;
} CSRSS_PEEK_CONSOLE_INPUT_REQUEST, *PCSRSS_PEEK_CONSOLE_INPUT_REQUEST;

typedef struct
{
  DWORD Length;
} CSRSS_PEEK_CONSOLE_INPUT_REPLY, *PCSRSS_PEEK_CONSOLE_INPUT_REPLY;

typedef struct
{
  HANDLE ConsoleHandle;
  COORD BufferSize;
  COORD BufferCoord;
  SMALL_RECT ReadRegion;
  CHAR_INFO* CharInfo;
} CSRSS_READ_CONSOLE_OUTPUT_REQUEST, *PCSRSS_READ_CONSOLE_OUTPUT_REQUEST;

typedef struct
{
  SMALL_RECT ReadRegion;
} CSRSS_READ_CONSOLE_OUTPUT_REPLY, *PCSRSS_READ_CONSOLE_OUTPUT_REPLY;

typedef struct
{
  HANDLE ConsoleHandle;
  DWORD Length;
  INPUT_RECORD* InputRecord;
} CSRSS_WRITE_CONSOLE_INPUT_REQUEST, *PCSRSS_WRITE_CONSOLE_INPUT_REQUEST;

typedef struct
{
  DWORD Length;
} CSRSS_WRITE_CONSOLE_INPUT_REPLY, *PCSRSS_WRITE_CONSOLE_INPUT_REPLY;

typedef struct
{
} CSRSS_GET_INPUT_HANDLE_REQUEST, *PCSRSS_GET_INPUT_HANDLE_REQUEST;

typedef struct
{
  HANDLE InputHandle;
} CSRSS_GET_INPUT_HANDLE_REPLY, *PCSRSS_GET_INPUT_HANDLE_REPLY;

typedef struct
{
} CSRSS_GET_OUTPUT_HANDLE_REQUEST, *PCSRSS_GET_OUTPUT_HANDLE_REQUEST;

typedef struct
{
  HANDLE OutputHandle;
} CSRSS_GET_OUTPUT_HANDLE_REPLY, *PCSRSS_GET_OUTPUT_HANDLE_REPLY;

typedef struct
{
  HANDLE Handle;
} CSRSS_CLOSE_HANDLE_REQUEST, *PCSRSS_CLOSE_HANDLE_REQUEST;

typedef struct
{
} CSRSS_CLOSE_HANDLE_REPLY, *PCSRSS_CLOSE_HANDLE_REPLY;

typedef struct
{
  HANDLE Handle;
} CSRSS_VERIFY_HANDLE_REQUEST, *PCSRSS_VERIFY_HANDLE_REQUEST;

typedef struct
{
} CSRSS_VERIFY_HANDLE_REPLY, *PCSRSS_VERIFY_HANDLE_REPLY;

typedef struct
{
  HANDLE Handle;
  DWORD ProcessId;
} CSRSS_DUPLICATE_HANDLE_REQUEST, *PCSRSS_DUPLICATE_HANDLE_REQUEST;

typedef struct
{
  HANDLE Handle;
} CSRSS_DUPLICATE_HANDLE_REPLY, *PCSRSS_DUPLICATE_HANDLE_REPLY;

#define CONSOLE_HARDWARE_STATE_GET 0
#define CONSOLE_HARDWARE_STATE_SET 1

#define CONSOLE_HARDWARE_STATE_GDI_MANAGED 0
#define CONSOLE_HARDWARE_STATE_DIRECT      1

typedef struct
{
  HANDLE ConsoleHandle;
  DWORD SetGet; /* 0=get; 1=set */
  DWORD State;
} CSRSS_SETGET_CONSOLE_HW_STATE_REQUEST, *PCSRSS_SETGET_CONSOLE_HW_STATE_REQUEST;

typedef struct
{
  HANDLE ConsoleHandle;
  DWORD SetGet; /* 0=get; 1=set */
  DWORD State;
} CSRSS_SETGET_CONSOLE_HW_STATE_REPLY, *PCSRSS_SETGET_CONSOLE_HW_STATE_REPLY;

typedef struct
{
  HANDLE ConsoleHandle;
  HWND   WindowHandle;
} CSRSS_CONSOLE_WINDOW, *PCSRSS_CONSOLE_WINDOW;

typedef struct
{
  WCHAR DesktopName[1];
} CSRSS_CREATE_DESKTOP_REQUEST, *PCSRSS_CREATE_DESKTOP_REQUEST;

typedef struct
{
} CSRSS_CREATE_DESKTOP_REPLY, *PCSRSS_CREATE_DESKTOP_REPLY;

typedef struct
{
  HWND DesktopWindow;
  ULONG Width;
  ULONG Height;
} CSRSS_SHOW_DESKTOP_REQUEST, *PCSRSS_SHOW_DESKTOP_REQUEST;

typedef struct
{
} CSRSS_SHOW_DESKTOP_REPLY, *PCSRSS_SHOW_DESKTOP_REPLY;

typedef struct
{
  HWND DesktopWindow;
} CSRSS_HIDE_DESKTOP_REQUEST, *PCSRSS_HIDE_DESKTOP_REQUEST;

typedef struct
{
} CSRSS_HIDE_DESKTOP_REPLY, *PCSRSS_HIDE_DESKTOP_REPLY;

#define CSRSS_MAX_WRITE_CONSOLE_REQUEST       \
      (MAX_MESSAGE_DATA - sizeof(ULONG) - sizeof(CSRSS_WRITE_CONSOLE_REQUEST))

#define CSRSS_MAX_SET_TITLE_REQUEST           (MAX_MESSAGE_DATA - sizeof( HANDLE ) - sizeof( DWORD ) - sizeof( ULONG ) - sizeof( LPC_MESSAGE ))

#define CSRSS_MAX_WRITE_CONSOLE_OUTPUT_CHAR   (MAX_MESSAGE_DATA - sizeof( ULONG ) - sizeof( CSRSS_WRITE_CONSOLE_OUTPUT_CHAR_REQUEST ))

#define CSRSS_MAX_WRITE_CONSOLE_OUTPUT_ATTRIB   ((MAX_MESSAGE_DATA - sizeof( ULONG ) - sizeof( CSRSS_WRITE_CONSOLE_OUTPUT_ATTRIB_REQUEST )) / 2)

#define CSRSS_MAX_READ_CONSOLE_REQUEST        (MAX_MESSAGE_DATA - sizeof( ULONG ) - sizeof( CSRSS_READ_CONSOLE_REQUEST ))

#define CSRSS_MAX_READ_CONSOLE_OUTPUT_CHAR    (MAX_MESSAGE_DATA - sizeof(ULONG) - sizeof(HANDLE) - sizeof(DWORD) - sizeof(CSRSS_READ_CONSOLE_OUTPUT_CHAR_REQUEST))

#define CSRSS_MAX_READ_CONSOLE_OUTPUT_ATTRIB  (MAX_MESSAGE_DATA - sizeof(ULONG) - sizeof(HANDLE) - sizeof(DWORD) - sizeof(CSRSS_READ_CONSOLE_OUTPUT_ATTRIB_REQUEST))

/* WCHARs, not bytes! */
#define CSRSS_MAX_TITLE_LENGTH          80

#define CSRSS_CREATE_PROCESS                (0x0)
#define CSRSS_TERMINATE_PROCESS             (0x1)
#define CSRSS_WRITE_CONSOLE                 (0x2)
#define CSRSS_READ_CONSOLE                  (0x3)
#define CSRSS_ALLOC_CONSOLE                 (0x4)
#define CSRSS_FREE_CONSOLE                  (0x5)
#define CSRSS_CONNECT_PROCESS               (0x6)
#define CSRSS_SCREEN_BUFFER_INFO            (0x7)
#define CSRSS_SET_CURSOR                    (0x8)
#define CSRSS_FILL_OUTPUT                   (0x9)
#define CSRSS_READ_INPUT                    (0xA)
#define CSRSS_WRITE_CONSOLE_OUTPUT_CHAR     (0xB)
#define CSRSS_WRITE_CONSOLE_OUTPUT_ATTRIB   (0xC)
#define CSRSS_FILL_OUTPUT_ATTRIB            (0xD)
#define CSRSS_GET_CURSOR_INFO               (0xE)
#define CSRSS_SET_CURSOR_INFO               (0xF)
#define CSRSS_SET_ATTRIB                    (0x10)
#define CSRSS_GET_CONSOLE_MODE              (0x11)
#define CSRSS_SET_CONSOLE_MODE              (0x12)
#define CSRSS_CREATE_SCREEN_BUFFER          (0x13)
#define CSRSS_SET_SCREEN_BUFFER             (0x14)
#define CSRSS_SET_TITLE                     (0x15)
#define CSRSS_GET_TITLE                     (0x16)
#define CSRSS_WRITE_CONSOLE_OUTPUT          (0x17)
#define CSRSS_FLUSH_INPUT_BUFFER            (0x18)
#define CSRSS_SCROLL_CONSOLE_SCREEN_BUFFER  (0x19)
#define CSRSS_READ_CONSOLE_OUTPUT_CHAR      (0x1A)
#define CSRSS_READ_CONSOLE_OUTPUT_ATTRIB    (0x1B)
#define CSRSS_GET_NUM_INPUT_EVENTS          (0x1C)
#define CSRSS_REGISTER_SERVICES_PROCESS     (0x1D)
#define CSRSS_EXIT_REACTOS                  (0x1E)
#define CSRSS_GET_SHUTDOWN_PARAMETERS       (0x1F)
#define CSRSS_SET_SHUTDOWN_PARAMETERS       (0x20)
#define CSRSS_PEEK_CONSOLE_INPUT            (0x21)
#define CSRSS_READ_CONSOLE_OUTPUT           (0x22)
#define CSRSS_WRITE_CONSOLE_INPUT           (0x23)
#define CSRSS_GET_INPUT_HANDLE              (0x24)
#define CSRSS_GET_OUTPUT_HANDLE             (0x25)
#define CSRSS_CLOSE_HANDLE                  (0x26)
#define CSRSS_VERIFY_HANDLE                 (0x27)
#define CSRSS_DUPLICATE_HANDLE		    (0x28)
#define CSRSS_SETGET_CONSOLE_HW_STATE       (0x29)
#define CSRSS_GET_CONSOLE_WINDOW            (0x2A)
#define CSRSS_CREATE_DESKTOP                (0x2B)
#define CSRSS_SHOW_DESKTOP                  (0x2C)
#define CSRSS_HIDE_DESKTOP                  (0x2D)

/* Keep in sync with definition below. */
#define CSRSS_REQUEST_HEADER_SIZE (sizeof(LPC_MESSAGE) + sizeof(ULONG))

typedef struct
{
  LPC_MESSAGE Header;
  ULONG Type;
  union
  {
    CSRSS_CREATE_PROCESS_REQUEST CreateProcessRequest;
    CSRSS_CONNECT_PROCESS_REQUEST ConnectRequest;
    CSRSS_WRITE_CONSOLE_REQUEST WriteConsoleRequest;
    CSRSS_READ_CONSOLE_REQUEST ReadConsoleRequest;
    CSRSS_ALLOC_CONSOLE_REQUEST AllocConsoleRequest;
    CSRSS_SCREEN_BUFFER_INFO_REQUEST ScreenBufferInfoRequest;
    CSRSS_SET_CURSOR_REQUEST SetCursorRequest;
    CSRSS_FILL_OUTPUT_REQUEST FillOutputRequest;
    CSRSS_READ_INPUT_REQUEST ReadInputRequest;
    CSRSS_WRITE_CONSOLE_OUTPUT_CHAR_REQUEST WriteConsoleOutputCharRequest;
    CSRSS_WRITE_CONSOLE_OUTPUT_ATTRIB_REQUEST WriteConsoleOutputAttribRequest;
    CSRSS_FILL_OUTPUT_ATTRIB_REQUEST FillOutputAttribRequest;
    CSRSS_SET_CURSOR_INFO_REQUEST SetCursorInfoRequest;
    CSRSS_GET_CURSOR_INFO_REQUEST GetCursorInfoRequest;
    CSRSS_SET_ATTRIB_REQUEST SetAttribRequest;
    CSRSS_SET_CONSOLE_MODE_REQUEST SetConsoleModeRequest;
    CSRSS_GET_CONSOLE_MODE_REQUEST GetConsoleModeRequest;
    CSRSS_CREATE_SCREEN_BUFFER_REQUEST CreateScreenBufferRequest;
    CSRSS_SET_SCREEN_BUFFER_REQUEST SetScreenBufferRequest;
    CSRSS_SET_TITLE_REQUEST SetTitleRequest;
    CSRSS_GET_TITLE_REQUEST GetTitleRequest;
    CSRSS_WRITE_CONSOLE_OUTPUT_REQUEST WriteConsoleOutputRequest;
    CSRSS_FLUSH_INPUT_BUFFER_REQUEST FlushInputBufferRequest;
    CSRSS_SCROLL_CONSOLE_SCREEN_BUFFER_REQUEST 
    ScrollConsoleScreenBufferRequest;
    CSRSS_READ_CONSOLE_OUTPUT_CHAR_REQUEST ReadConsoleOutputCharRequest;
    CSRSS_READ_CONSOLE_OUTPUT_ATTRIB_REQUEST ReadConsoleOutputAttribRequest;
    CSRSS_GET_NUM_INPUT_EVENTS_REQUEST GetNumInputEventsRequest;
    CSRSS_REGISTER_SERVICES_PROCESS_REQUEST RegisterServicesProcessRequest;
    CSRSS_EXIT_REACTOS_REQUEST ExitReactosRequest;
    CSRSS_SET_SHUTDOWN_PARAMETERS_REQUEST SetShutdownParametersRequest;
    CSRSS_GET_SHUTDOWN_PARAMETERS_REQUEST GetShutdownParametersRequest;
    CSRSS_PEEK_CONSOLE_INPUT_REQUEST PeekConsoleInputRequest;
    CSRSS_READ_CONSOLE_OUTPUT_REQUEST ReadConsoleOutputRequest;
    CSRSS_WRITE_CONSOLE_INPUT_REQUEST WriteConsoleInputRequest;
    CSRSS_CLOSE_HANDLE_REQUEST CloseHandleRequest;
    CSRSS_VERIFY_HANDLE_REQUEST VerifyHandleRequest;
    CSRSS_DUPLICATE_HANDLE_REQUEST DuplicateHandleRequest;
    CSRSS_SETGET_CONSOLE_HW_STATE_REQUEST ConsoleHardwareStateRequest;
    CSRSS_CONSOLE_WINDOW ConsoleWindowRequest;
    CSRSS_CREATE_DESKTOP_REQUEST CreateDesktopRequest;
    CSRSS_SHOW_DESKTOP_REQUEST ShowDesktopRequest;
    CSRSS_HIDE_DESKTOP_REQUEST HideDesktopRequest;
  } Data;
} CSRSS_API_REQUEST, *PCSRSS_API_REQUEST;

typedef struct
{
  LPC_MESSAGE Header;
  NTSTATUS Status;
  union
  {
    CSRSS_CREATE_PROCESS_REPLY CreateProcessReply;
    CSRSS_CONNECT_PROCESS_REPLY ConnectReply;
    CSRSS_WRITE_CONSOLE_REPLY WriteConsoleReply;
    CSRSS_READ_CONSOLE_REPLY ReadConsoleReply;
    CSRSS_ALLOC_CONSOLE_REPLY AllocConsoleReply;
    CSRSS_SCREEN_BUFFER_INFO_REPLY ScreenBufferInfoReply;
    CSRSS_READ_INPUT_REPLY ReadInputReply;
    CSRSS_WRITE_CONSOLE_OUTPUT_CHAR_REPLY WriteConsoleOutputCharReply;
    CSRSS_WRITE_CONSOLE_OUTPUT_ATTRIB_REPLY WriteConsoleOutputAttribReply;
    CSRSS_GET_CURSOR_INFO_REPLY GetCursorInfoReply;
    CSRSS_GET_CONSOLE_MODE_REPLY GetConsoleModeReply;
    CSRSS_CREATE_SCREEN_BUFFER_REPLY CreateScreenBufferReply;
    CSRSS_GET_TITLE_REPLY GetTitleReply;
    CSRSS_WRITE_CONSOLE_OUTPUT_REPLY WriteConsoleOutputReply;
    CSRSS_READ_CONSOLE_OUTPUT_CHAR_REPLY ReadConsoleOutputCharReply;
    CSRSS_READ_CONSOLE_OUTPUT_ATTRIB_REPLY ReadConsoleOutputAttribReply;
    CSRSS_GET_NUM_INPUT_EVENTS_REPLY GetNumInputEventsReply;
    CSRSS_SET_SHUTDOWN_PARAMETERS_REPLY SetShutdownParametersReply;
    CSRSS_GET_SHUTDOWN_PARAMETERS_REPLY GetShutdownParametersReply;
    CSRSS_PEEK_CONSOLE_INPUT_REPLY PeekConsoleInputReply;
    CSRSS_READ_CONSOLE_OUTPUT_REPLY ReadConsoleOutputReply;
    CSRSS_WRITE_CONSOLE_INPUT_REPLY WriteConsoleInputReply;
    CSRSS_GET_INPUT_HANDLE_REPLY GetInputHandleReply;
    CSRSS_GET_OUTPUT_HANDLE_REPLY GetOutputHandleReply;
    CSRSS_DUPLICATE_HANDLE_REPLY DuplicateHandleReply;
    CSRSS_SETGET_CONSOLE_HW_STATE_REPLY ConsoleHardwareStateReply;
    CSRSS_CONSOLE_WINDOW ConsoleWindowReply;
    CSRSS_CREATE_DESKTOP_REPLY CreateDesktopReply;
    CSRSS_SHOW_DESKTOP_REPLY ShowDesktopReply;
    CSRSS_HIDE_DESKTOP_REPLY HideDesktopReply;
  } Data;
} CSRSS_API_REPLY, *PCSRSS_API_REPLY;

#endif /* __INCLUDE_CSRSS_CSRSS_H */
