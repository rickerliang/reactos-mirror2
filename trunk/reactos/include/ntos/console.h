/*
 * COPYRIGHT:    See COPYING in the top level directory
 * PROJECT:      ReactOS kernel
 * FILE:         include/console.h
 * PURPOSE:      Console declarations used by all the parts of the 
 *               system
 * PROGRAMMER:   David Welch <welch@cwcom.net>
 * UPDATE HISTORY: 
 *               27/06/00: Created
 */


#ifndef __INCLUDE_CONSOLE_H
#define __INCLUDE_CONSOLE_H

/* GetConsoleMode */
#define ENABLE_LINE_INPUT	(2)
#define ENABLE_ECHO_INPUT	(4)
#define ENABLE_PROCESSED_INPUT	(1)
#define ENABLE_WINDOW_INPUT	(8)
#define ENABLE_MOUSE_INPUT	(16)
#define ENABLE_PROCESSED_OUTPUT	(1)
#define ENABLE_WRAP_AT_EOL_OUTPUT	(2)

typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
  COORD      dwSize; 
  COORD      dwCursorPosition; 
  WORD       wAttributes; 
  SMALL_RECT srWindow; 
  COORD      dwMaximumWindowSize; 
} CONSOLE_SCREEN_BUFFER_INFO, *PCONSOLE_SCREEN_BUFFER_INFO ; 

typedef struct _CONSOLE_CURSOR_INFO {
  DWORD  dwSize; 
  BOOL   bVisible; 
} CONSOLE_CURSOR_INFO, *PCONSOLE_CURSOR_INFO; 

#endif /* __INCLUDE_CONSOLE_H */



