/*
 * Regedit main function
 *
 * Copyright (C) 2002 Robert Dickenson <robd@reactos.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define WIN32_LEAN_AND_MEAN     /* Exclude rarely-used stuff from Windows headers */
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <tchar.h>
#include <process.h>
#include <stdio.h>
#include <fcntl.h>

#include "main.h"
#include "hexedit.h"


BOOL ProcessCmdLine(LPSTR lpCmdLine);


/*******************************************************************************
 * Global Variables:
 */

HINSTANCE hInst;
HWND hFrameWnd;
HWND hStatusBar;
HMENU hMenuFrame;
HMENU hPopupMenus = 0;
UINT nClipboardFormat;
LPCTSTR strClipboardFormat = _T("TODO: SET CORRECT FORMAT");


#define MAX_LOADSTRING  100
TCHAR szTitle[MAX_LOADSTRING];
TCHAR szFrameClass[MAX_LOADSTRING];
TCHAR szChildClass[MAX_LOADSTRING];


/*******************************************************************************
 *
 *
 *   FUNCTION: InitInstance(HANDLE, int)
 *
 *   PURPOSE: Saves instance handle and creates main window
 *
 *   COMMENTS:
 *
 *        In this function, we save the instance handle in a global variable and
 *        create and display the main program window.
 */

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEX wcFrame = {
                             sizeof(WNDCLASSEX),
                             CS_HREDRAW | CS_VREDRAW/*style*/,
                             FrameWndProc,
                             0/*cbClsExtra*/,
                             0/*cbWndExtra*/,
                             hInstance,
                             LoadIcon(hInstance, MAKEINTRESOURCE(IDI_REGEDIT)),
                             LoadCursor(0, IDC_ARROW),
                             0/*hbrBackground*/,
                             0/*lpszMenuName*/,
                             szFrameClass,
                             (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_REGEDIT), IMAGE_ICON,
                                              GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED)
                         };
    ATOM hFrameWndClass = RegisterClassEx(&wcFrame); /* register frame window class */

    WNDCLASSEX wcChild = {
                             sizeof(WNDCLASSEX),
                             CS_HREDRAW | CS_VREDRAW/*style*/,
                             ChildWndProc,
                             0/*cbClsExtra*/,
                             sizeof(HANDLE)/*cbWndExtra*/,
                             hInstance,
                             LoadIcon(hInstance, MAKEINTRESOURCE(IDI_REGEDIT)),
                             LoadCursor(0, IDC_ARROW),
                             0/*hbrBackground*/,
                             0/*lpszMenuName*/,
                             szChildClass,
                             (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_REGEDIT), IMAGE_ICON,
                                              GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED)

                         };
    ATOM hChildWndClass = RegisterClassEx(&wcChild); /* register child windows class */
    hChildWndClass = hChildWndClass; /* warning eater */
    
    RegisterHexEditorClass(hInstance);

    hMenuFrame = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_REGEDIT_MENU));
    hPopupMenus = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_POPUP_MENUS));

    /* Initialize the Windows Common Controls DLL */
    InitCommonControls();

    nClipboardFormat = RegisterClipboardFormat(strClipboardFormat);
    /* if (nClipboardFormat == 0) {
        DWORD dwError = GetLastError();
    } */

    hFrameWnd = CreateWindowEx(WS_EX_WINDOWEDGE, (LPCTSTR)(int)hFrameWndClass, szTitle,
                               WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               NULL, hMenuFrame, hInstance, NULL/*lpParam*/);

    if (!hFrameWnd) {
        return FALSE;
    }

    /* Create the status bar */
    hStatusBar = CreateStatusWindow(WS_VISIBLE|WS_CHILD|WS_CLIPSIBLINGS|SBT_NOBORDERS,
                                    _T(""), hFrameWnd, STATUS_WINDOW);
    if (hStatusBar) {
        /* Create the status bar panes */
        SetupStatusBar(hFrameWnd, FALSE);
        CheckMenuItem(GetSubMenu(hMenuFrame, ID_VIEW_MENU), ID_VIEW_STATUSBAR, MF_BYCOMMAND|MF_CHECKED);
    }
    ShowWindow(hFrameWnd, nCmdShow);
    UpdateWindow(hFrameWnd);
    return TRUE;
}

/******************************************************************************/

void ExitInstance(HINSTANCE hInstance)
{
    UnregisterHexEditorClass(hInstance);
    DestroyMenu(hMenuFrame);
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
    MSG msg;
    HACCEL hAccel;
    /*
        int hCrt;
        FILE *hf;
        AllocConsole();
        hCrt = _open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
        hf = _fdopen(hCrt, "w");
        *stdout = *hf;
        setvbuf(stdout, NULL, _IONBF, 0);
     
    	wprintf(L"command line exit, hInstance = %d\n", hInstance);
    	getch();
    	FreeConsole();
        return 0;
     */

    if (ProcessCmdLine(lpCmdLine)) {
        return 0;
    }

    /* Initialize global strings */
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_REGEDIT_FRAME, szFrameClass, MAX_LOADSTRING);
    LoadString(hInstance, IDC_REGEDIT, szChildClass, MAX_LOADSTRING);

    /* Store instance handle in our global variable */
    hInst = hInstance;

    /* Perform application initialization */
    if (!InitInstance(hInstance, nCmdShow)) {
        return FALSE;
    }
    hAccel = LoadAccelerators(hInstance, (LPCTSTR)IDC_REGEDIT);

    /* Main message loop */
    while (GetMessage(&msg, (HWND)NULL, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, hAccel, &msg) && 
            !IsDialogMessage(hFrameWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    ExitInstance(hInstance);
    return msg.wParam;
}
