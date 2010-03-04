/*
 * Icontitle window class.
 *
 * Copyright 1997 Alex Korobka
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <user32.h>

#include <wine/debug.h>

#ifdef __REACTOS__
#define MAKEINTATOMW(atom)  ((LPCWSTR)((ULONG_PTR)((WORD)(atom))))
#define ICONTITLE_CLASS_ATOM MAKEINTATOMW(32772)
#endif

static BOOL bMultiLineTitle;
static HFONT hIconTitleFont;

/*********************************************************************
 * icon title class descriptor
 */
const struct builtin_class_descr ICONTITLE_builtin_class =
{
    (LPCWSTR)ICONTITLE_CLASS_ATOM, /* name */
    0,                    /* style */
    NULL,                 /* procA (winproc is Unicode only) */
    IconTitleWndProc,     /* procW */
    0,                    /* extra */
    IDC_ARROW,            /* cursor */
    0                     /* brush */
};



#ifndef __REACTOS__
/***********************************************************************
 *           ICONTITLE_Create
 */
HWND ICONTITLE_Create( HWND owner )
{
    HWND hWnd;
    HINSTANCE instance = (HINSTANCE)GetWindowLongPtrA( owner, GWLP_HINSTANCE );
    LONG style = WS_CLIPSIBLINGS;

    if (!IsWindowEnabled(owner)) style |= WS_DISABLED;
    if( GetWindowLongPtrA( owner, GWL_STYLE ) & WS_CHILD )
	hWnd = CreateWindowExA( 0, (LPCSTR)ICONTITLE_CLASS_ATOM, NULL,
                                style | WS_CHILD, 0, 0, 1, 1,
                                GetParent(owner), 0, instance, NULL );
    else
	hWnd = CreateWindowExA( 0, (LPCSTR)ICONTITLE_CLASS_ATOM, NULL,
                                style, 0, 0, 1, 1,
                                owner, 0, instance, NULL );
    WIN_SetOwner( hWnd, owner );  /* MDI depends on this */
    SetWindowLongPtrW( hWnd, GWL_STYLE,
                       GetWindowLongPtrW( hWnd, GWL_STYLE ) & ~(WS_CAPTION | WS_BORDER) );
    return hWnd;
}
#endif

/***********************************************************************
 *           ICONTITLE_SetTitlePos
 */
static BOOL ICONTITLE_SetTitlePos( HWND hwnd, HWND owner )
{
    static const WCHAR emptyTitleText[] = {'<','.','.','.','>',0};
    WCHAR str[80];
    HDC hDC;
    HFONT hPrevFont;
    RECT rect;
    INT cx, cy;
    POINT pt;

    int length = GetWindowTextW( owner, str, sizeof(str)/sizeof(WCHAR) );

    while (length && str[length - 1] == ' ') /* remove trailing spaces */
        str[--length] = 0;

    if( !length )
    {
        strcpyW( str, emptyTitleText );
        length = strlenW( str );
    }

    if (!(hDC = GetDC( hwnd ))) return FALSE;

    hPrevFont = SelectObject( hDC, hIconTitleFont );

    SetRect( &rect, 0, 0, GetSystemMetrics(SM_CXICONSPACING) -
             GetSystemMetrics(SM_CXBORDER) * 2,
             GetSystemMetrics(SM_CYBORDER) * 2 );

    DrawTextW( hDC, str, length, &rect, DT_CALCRECT | DT_CENTER | DT_NOPREFIX | DT_WORDBREAK |
               (( bMultiLineTitle ) ? 0 : DT_SINGLELINE) );

    SelectObject( hDC, hPrevFont );
    ReleaseDC( hwnd, hDC );

    cx = rect.right - rect.left +  4 * GetSystemMetrics(SM_CXBORDER);
    cy = rect.bottom - rect.top;

    pt.x = (GetSystemMetrics(SM_CXICON) - cx) / 2;
    pt.y = GetSystemMetrics(SM_CYICON);

    /* point is relative to owner, make it relative to parent */
    MapWindowPoints( owner, GetParent(hwnd), &pt, 1 );

    SetWindowPos( hwnd, owner, pt.x, pt.y, cx, cy, SWP_NOACTIVATE );
    return TRUE;
}

/***********************************************************************
 *           ICONTITLE_Paint
 */
static BOOL ICONTITLE_Paint( HWND hwnd, HWND owner, HDC hDC, BOOL bActive )
{
    RECT rect;
    HFONT hPrevFont;
    HBRUSH hBrush = 0;
    COLORREF textColor = 0;

    if( bActive )
    {
	hBrush = GetSysColorBrush(COLOR_ACTIVECAPTION);
	textColor = GetSysColor(COLOR_CAPTIONTEXT);
    }
    else
    {
        if( GetWindowLongPtrA( hwnd, GWL_STYLE ) & WS_CHILD )
	{
	    hBrush = (HBRUSH) GetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND);
	    if( hBrush )
	    {
		INT level;
		LOGBRUSH logBrush;
		GetObjectA( hBrush, sizeof(logBrush), &logBrush );
		level = GetRValue(logBrush.lbColor) +
			   GetGValue(logBrush.lbColor) +
			      GetBValue(logBrush.lbColor);
		if( level < (0x7F * 3) )
		    textColor = RGB( 0xFF, 0xFF, 0xFF );
	    }
	    else
		hBrush = GetStockObject( WHITE_BRUSH );
	}
	else
	{
	    hBrush = GetStockObject( BLACK_BRUSH );
	    textColor = RGB( 0xFF, 0xFF, 0xFF );
	}
    }

    GetClientRect( hwnd, &rect );
    DPtoLP( hDC, (LPPOINT)&rect, 2 );
    FillRect( hDC, &rect, hBrush );

    hPrevFont = SelectObject( hDC, hIconTitleFont );
    if( hPrevFont )
    {
	WCHAR buffer[80];

        INT length = GetWindowTextW( owner, buffer, sizeof(buffer)/sizeof(buffer[0]) );
        SetTextColor( hDC, textColor );
        SetBkMode( hDC, TRANSPARENT );

        DrawTextW( hDC, buffer, length, &rect, DT_CENTER | DT_NOPREFIX |
                   DT_WORDBREAK | ((bMultiLineTitle) ? 0 : DT_SINGLELINE) );

	SelectObject( hDC, hPrevFont );
    }
    return (hPrevFont != 0);
}

/***********************************************************************
 *           IconTitleWndProc
 */
LRESULT WINAPI IconTitleWndProc( HWND hWnd, UINT msg,
                                 WPARAM wParam, LPARAM lParam )
{
    HWND owner = GetWindow( hWnd, GW_OWNER );

    if (!IsWindow(hWnd)) return 0;

    switch( msg )
    {
        case WM_CREATE:
            if (!hIconTitleFont)
            {
                LOGFONTA logFont;
                SystemParametersInfoA( SPI_GETICONTITLELOGFONT, 0, &logFont, 0 );
                SystemParametersInfoA( SPI_GETICONTITLEWRAP, 0, &bMultiLineTitle, 0 );
                hIconTitleFont = CreateFontIndirectA( &logFont );
            }
            return (hIconTitleFont ? 0 : -1);
	case WM_NCHITTEST:
	     return HTCAPTION;
	case WM_NCMOUSEMOVE:
	case WM_NCLBUTTONDBLCLK:
	     return SendMessageW( owner, msg, wParam, lParam );
	case WM_ACTIVATE:
	     if( wParam ) SetActiveWindow( owner );
             return 0;
	case WM_CLOSE:
	     return 0;
	case WM_SHOWWINDOW:
            if (wParam) ICONTITLE_SetTitlePos( hWnd, owner );
	     return 0;
	case WM_ERASEBKGND:
            if( GetWindowLongPtrW( owner, GWL_STYLE ) & WS_CHILD )
                lParam = SendMessageW( owner, WM_ISACTIVEICON, 0, 0 );
            else
                lParam = (owner == GetActiveWindow());
            if( ICONTITLE_Paint( hWnd, owner, (HDC)wParam, (BOOL)lParam ) )
                ValidateRect( hWnd, NULL );
            return 1;
    }
    return DefWindowProcW( hWnd, msg, wParam, lParam );
}
