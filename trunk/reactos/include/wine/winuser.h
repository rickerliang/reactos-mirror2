#ifndef __WINE_WINUSER_H
#define __WINE_WINUSER_H

/*
 * Compatibility header
 */

#include <w32api.h>
#include <wingdi.h>
#include_next <winuser.h>

#define WS_EX_TRAYWINDOW 0x80000000L
#ifndef MIM_MENUDATA
#define MIM_MENUDATA     0x00000008
#endif

#define DCX_USESTYLE     0x00010000
#define WS_EX_MANAGED    0x40000000L /* Window managed by the window system */

UINT WINAPI PrivateExtractIconsA(LPCSTR,int,int,int,HICON*,UINT*,UINT,UINT);
UINT WINAPI PrivateExtractIconsW(LPCWSTR,int,int,int,HICON*,UINT*,UINT,UINT);

typedef struct tagCWPSTRUCT *LPCWPSTRUCT;

#define WM_ALTTABACTIVE         0x0029

#endif /* __WINE_WINUSER_H */
