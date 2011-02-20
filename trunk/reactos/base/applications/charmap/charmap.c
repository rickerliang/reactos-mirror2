/*
 * PROJECT:     ReactOS Character Map
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        base/applications/charmap/charmap.c
 * PURPOSE:     main dialog implementation
 * COPYRIGHT:   Copyright 2007 Ged Murphy <gedmurphy@reactos.org>
 *
 */

#include <precomp.h>

#define ID_ABOUT    0x1

HINSTANCE hInstance;

/* Font-enumeration callback */
static
int
CALLBACK
EnumFontNames(ENUMLOGFONTEXW *lpelfe,
              NEWTEXTMETRICEXW *lpntme,
              DWORD FontType,
              LPARAM lParam)
{
    HWND hwndCombo = (HWND)lParam;
    LPWSTR pszName  = lpelfe->elfLogFont.lfFaceName;

    /* make sure font doesn't already exist in our list */
    if(SendMessageW(hwndCombo,
                    CB_FINDSTRING,
                    0,
                    (LPARAM)pszName) == CB_ERR)
    {
        INT idx;
        BOOL fFixed;
        BOOL fTrueType;

        /* add the font */
        idx = (INT)SendMessageW(hwndCombo,
                                CB_ADDSTRING,
                                0,
                                (LPARAM)pszName);

        /* record the font's attributes (Fixedwidth and Truetype) */
        fFixed = (lpelfe->elfLogFont.lfPitchAndFamily & FIXED_PITCH) ? TRUE : FALSE;
        fTrueType = (lpelfe->elfLogFont.lfOutPrecision == OUT_STROKE_PRECIS) ? TRUE : FALSE;

        /* store this information in the list-item's userdata area */
        SendMessageW(hwndCombo,
                     CB_SETITEMDATA,
                     idx,
                     MAKEWPARAM(fFixed, fTrueType));
    }

    return 1;
}


/* Initialize the font-list by enumeration all system fonts */
static
VOID
FillFontStyleComboList(HWND hwndCombo)
{
    HDC hdc;
    LOGFONTW lf;

    /* FIXME: for fun, draw each font in its own style */
    HFONT hFont = GetStockObject(DEFAULT_GUI_FONT);
    SendMessageW(hwndCombo,
                 WM_SETFONT,
                 (WPARAM)hFont,
                 0);

    ZeroMemory(&lf, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;

    hdc = GetDC(hwndCombo);

    /* store the list of fonts in the combo */
    EnumFontFamiliesExW(hdc,
                        &lf,
                        (FONTENUMPROCW)EnumFontNames,
                        (LPARAM)hwndCombo,
                        0);

    ReleaseDC(hwndCombo,
              hdc);

    SendMessageW(hwndCombo,
                 CB_SETCURSEL,
                 0,
                 0);
}


static
VOID
ChangeMapFont(HWND hDlg)
{
    HWND hCombo;
    HWND hMap;
    LPWSTR lpFontName;
    INT Len;

    hCombo = GetDlgItem(hDlg, IDC_FONTCOMBO);

    Len = GetWindowTextLengthW(hCombo);

    if (Len != 0)
    {
        lpFontName = HeapAlloc(GetProcessHeap(),
                               0,
                               (Len + 1) * sizeof(WCHAR));

        if (lpFontName)
        {
            SendMessageW(hCombo,
                         WM_GETTEXT,
                         Len + 1,
                         (LPARAM)lpFontName);

            hMap = GetDlgItem(hDlg, IDC_FONTMAP);

            SendMessageW(hMap,
                         FM_SETFONT,
                         0,
                         (LPARAM)lpFontName);
        }

        HeapFree(GetProcessHeap(),
                 0,
                 lpFontName);
    }
}

// Copy collected characters into the clipboard
static
void
CopyCharacters(HWND hDlg)
{
    HWND hText = GetDlgItem(hDlg, IDC_TEXTBOX);
    DWORD dwStart, dwEnd;

    // Acquire selection limits
    SendMessage(hText, EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);

    // Test if the whose text is unselected
    if(dwStart == dwEnd) {
        
        // Select the whole text
        SendMessageW(hText, EM_SETSEL, 0, -1);

        // Copy text
        SendMessageW(hText, WM_COPY, 0, 0);

        // Restore previous values
        SendMessageW(hText, EM_SETSEL, (WPARAM)dwStart, (LPARAM)dwEnd);

    } else {

        // Copy text
        SendMessageW(hText, WM_COPY, 0, 0);
    }
}

// Recover charset for the given font
static
BYTE
GetFontMetrics(HWND hWnd, HFONT hFont)
{
    TEXTMETRIC tmFont;
    HGDIOBJ    hOldObj;
    HDC        hDC;

    hDC = GetDC(hWnd);
    hOldObj = SelectObject(hDC, hFont);
    GetTextMetrics(hDC, &tmFont);
    SelectObject(hDC, hOldObj);
    ReleaseDC(hWnd, hDC);

    return tmFont.tmCharSet;
}

// Select a new character
static
VOID
AddCharToSelection(HWND hDlg, WCHAR ch)
{
    HWND    hMap = GetDlgItem(hDlg, IDC_FONTMAP);
    HWND    hText = GetDlgItem(hDlg, IDC_TEXTBOX);
    HFONT   hFont;
    LOGFONT lFont;
    CHARFORMAT cf;

    // Retrieve current character selected
    if (ch == 0)
    {
        ch = (WCHAR) SendMessageW(hMap, FM_GETCHAR, 0, 0);
        if (!ch)
            return;
    }

    // Retrieve current selected font
    hFont = (HFONT)SendMessage(hMap, FM_GETHFONT, 0, 0);

    // Recover LOGFONT structure from hFont
    if (!GetObject(hFont, sizeof(LOGFONT), &lFont))
        return;

    // Recover font properties of Richedit control
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    SendMessage(hText, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    // Apply properties of the new font
    cf.bCharSet = GetFontMetrics(hText, hFont);

    // Update font name
    wcscpy(cf.szFaceName, lFont.lfFaceName);

    // Update font properties
    SendMessage(hText, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    // Send selected character to Richedit
    SendMessage(hText, WM_CHAR, (WPARAM)ch, 0);
}


static
INT_PTR
CALLBACK
DlgProc(HWND hDlg,
        UINT Message,
        WPARAM wParam,
        LPARAM lParam)
{
    static HICON hSmIcon;
    static HICON hBgIcon;
    LPWSTR lpAboutText = NULL;

    switch(Message)
    {
        case WM_INITDIALOG:
        {
            HMENU hSysMenu;
            DWORD evMask;

            hSmIcon = LoadImageW(hInstance,
                                 MAKEINTRESOURCEW(IDI_ICON),
                                 IMAGE_ICON,
                                 16,
                                 16,
                                 0);
            if (hSmIcon)
            {
                 SendMessageW(hDlg,
                              WM_SETICON,
                              ICON_SMALL,
                              (LPARAM)hSmIcon);
            }

            hBgIcon = LoadImageW(hInstance,
                                 MAKEINTRESOURCEW(IDI_ICON),
                                 IMAGE_ICON,
                                 32,
                                 32,
                                 0);
            if (hBgIcon)
            {
                SendMessageW(hDlg,
                             WM_SETICON,
                             ICON_BIG,
                             (LPARAM)hBgIcon);
            }

            FillFontStyleComboList(GetDlgItem(hDlg,
                                              IDC_FONTCOMBO));

            ChangeMapFont(hDlg);
            hSysMenu = GetSystemMenu(hDlg,
                                     FALSE);
            if (hSysMenu != NULL)
            {
                if (LoadStringW(hInstance,
                                IDS_ABOUT,
                                lpAboutText,
                                0))
                {
                    AppendMenuW(hSysMenu,
                                MF_SEPARATOR,
                                0,
                                NULL);
                    AppendMenuW(hSysMenu,
                                MF_STRING,
                                ID_ABOUT,
                                lpAboutText);
                }
            }

            // Configure Richedi control for sending notification changes.
            evMask = SendDlgItemMessage(hDlg, IDC_TEXTBOX, EM_GETEVENTMASK, 0, 0);
            evMask |= ENM_CHANGE;
            SendDlgItemMessage(hDlg, IDC_TEXTBOX, EM_SETEVENTMASK, 0, (LPARAM)evMask);

            return TRUE;
        }

        case WM_COMMAND:
        {
            switch(LOWORD(wParam))
            {
                case IDC_FONTMAP:
                    switch (HIWORD(wParam))
                    {
                        case FM_SETCHAR:
                            AddCharToSelection(hDlg, LOWORD(lParam));
                            break;
                    }
                    break;

                case IDC_FONTCOMBO:
                    if (HIWORD(wParam) == CBN_SELCHANGE)
                    {
                        ChangeMapFont(hDlg);
                    }
                    break;

                case IDC_SELECT:
                    AddCharToSelection(hDlg, 0);
                    break;

                case IDC_TEXTBOX:
                    switch (HIWORD(wParam)) {
                    case EN_CHANGE:
                        if (GetWindowTextLength(GetDlgItem(hDlg, IDC_TEXTBOX)) == 0)
                            EnableWindow(GetDlgItem(hDlg, IDC_COPY), FALSE);
                        else
                            EnableWindow(GetDlgItem(hDlg, IDC_COPY), TRUE);
                        break;
                    }
                    break;

                case IDC_COPY:
                    CopyCharacters(hDlg);
                    break;

                case IDOK:
                    if (hSmIcon)
                        DestroyIcon(hSmIcon);
                    if (hBgIcon)
                        DestroyIcon(hBgIcon);
                    EndDialog(hDlg, 0);
                    break;
            }
        }
        break;

        case WM_SYSCOMMAND:
        {
            switch(wParam)
            {
                case ID_ABOUT:
                    ShowAboutDlg(hDlg);
                break;
            }
        }
        break;

        case WM_CLOSE:
            if (hSmIcon)
                DestroyIcon(hSmIcon);
            if (hBgIcon)
                DestroyIcon(hBgIcon);
            EndDialog(hDlg, 0);
            break;

        default:
            return FALSE;
    }

    return FALSE;
}


INT
WINAPI
wWinMain(HINSTANCE hInst,
         HINSTANCE hPrev,
         LPWSTR Cmd,
         int iCmd)
{
    INITCOMMONCONTROLSEX iccx;
    INT Ret = 1;
    HMODULE hRichEd20;

    hInstance = hInst;

    iccx.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccx.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&iccx);

    if (RegisterMapClasses(hInstance))
    {
        hRichEd20 = LoadLibraryW(L"RICHED20.DLL");

        if (hRichEd20 != NULL)
        {
            Ret = DialogBoxW(hInstance,
                             MAKEINTRESOURCEW(IDD_CHARMAP),
                             NULL,
                             DlgProc) >= 0;

            FreeLibrary(hRichEd20);
        }
        UnregisterMapClasses(hInstance);
    }

    return Ret;
}
