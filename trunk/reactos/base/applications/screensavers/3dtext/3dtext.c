/*
 *  Copyright 2000 Jeff Molofee http://nehe.gamedev.net/ (Original code)
 *  Copyright 2006 Eric Kohl
 *  Copyright 2007 Marc Piulachs (marc.piulachs@codexchange.net) - minor modifications , converted to screensaver
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <windows.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <tchar.h>

#include "resource.h"
#include "3dtext.h"

static HGLRC hRC;		// Permanent Rendering Context
static HDC hDC;		// Private GDI Device Context

GLuint base;			// Base Display List For The Font Set
GLfloat rot;			// Used To Rotate The Text

#define APPNAME _T("3DText")

HINSTANCE hInstance;
BOOL fullscreen = FALSE;

// Build Our Bitmap Font
GLvoid BuildFont(GLvoid)
{
    // Address Buffer For Font Storage
    GLYPHMETRICSFLOAT gmf[256];
    // Windows Font Handle
    HFONT font;

    // Storage For 256 Characters
    base = glGenLists(256);

    font = CreateFont(-12,
                      0,				// Width Of Font
                      0,				// Angle Of Escapement
                      0,				// Orientation Angle
                      FW_BOLD,				// Font Weight
                      FALSE,				// Italic
                      FALSE,				// Underline
                      FALSE,				// Strikeout
                      DEFAULT_CHARSET,			// Character Set Identifier
                      OUT_TT_PRECIS,			// Output Precision
                      CLIP_DEFAULT_PRECIS,		// Clipping Precision
                      ANTIALIASED_QUALITY,		// Output Quality
                      FF_DONTCARE|DEFAULT_PITCH,	// Family And Pitch
                      _T("Tahoma"));			// Font Name

    // Selects The Font We Created
    SelectObject(hDC, font);

    wglUseFontOutlines(hDC,				// Select The Current DC
                       0,				// Starting Character
                       255,				// Number Of Display Lists To Build
                       base,				// Starting Display Lists
                       0.0f,				// Deviation From The True Outlines
                       0.2f,				// Font Thickness In The Z Direction
                       WGL_FONT_POLYGONS,		// Use Polygons, Not Lines
                       gmf);				// Address Of Buffer To Recieve Data
}

// Delete The Font
GLvoid KillFont(GLvoid)
{
    // Delete all 256 characters
    glDeleteLists(base, 256);
}

// Custom GL "Print" Routine
GLvoid glPrint(LPTSTR text)
{
    // If there's no text, do nothing
    if (text == NULL)
        return;

    // Pushes The Display List Bits
    glPushAttrib(GL_LIST_BIT);

    // Sets The Base Character to 32
    glListBase(base);

    // Draws The Display List Text
    glCallLists(_tcslen(text),
#ifdef UNICODE
                GL_UNSIGNED_SHORT,
#else
                GL_UNSIGNED_BYTE,
#endif
                text);

    // Pops The Display List Bits
    glPopAttrib();
}

// Will Be Called Right After The GL Window Is Created
GLvoid InitGL(GLsizei Width, GLsizei Height)
{
    // Clear The Background Color To Black
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // Enables Clearing Of The Depth Buffer
    glClearDepth(1.0);

    // The Type Of Depth Test To Do
    glDepthFunc(GL_LESS);

    // Enables Depth Testing
    glEnable(GL_DEPTH_TEST);

    // Enables Smooth Color Shading
    glShadeModel(GL_SMOOTH);

    // Select The Projection Matrix
    glMatrixMode(GL_PROJECTION);

    // Reset The Projection Matrix
    glLoadIdentity();

    // Calculate The Aspect Ratio Of The Window
    gluPerspective(45.0f, (GLfloat)Width / (GLfloat)Height, 0.1f, 100.0f);

    // Select The Modelview Matrix
    glMatrixMode(GL_MODELVIEW);

    // Build The Font
    BuildFont();

    // Enable Default Light (Quick And Dirty)
    glEnable(GL_LIGHT0);

    // Enable Lighting
    glEnable(GL_LIGHTING);

    // Enable Coloring Of Material
    glEnable(GL_COLOR_MATERIAL);
}

// Handles Window Resizing
GLvoid ReSizeGLScene(GLsizei Width, GLsizei Height)	
{
    // Is Window Too Small (Divide By Zero Error)
    if (Height == 0)
    {
        // If So Make It One Pixel Tall
        Height = 1;
    }

    // Reset The Current Viewport And Perspective Transformation
    glViewport(0, 0, Width, Height);

    // Select The Projection Matrix
    glMatrixMode(GL_PROJECTION);

    // Reset The Projection Matrix
    glLoadIdentity();

    // Calculate The Aspect Ratio Of The Window
    gluPerspective(45.0f, (GLfloat)Width / (GLfloat)Height, 0.1f, 100.0f);

    // Select The Modelview Matrix
    glMatrixMode(GL_MODELVIEW);
}

// Handles Rendering
GLvoid DrawGLScene(GLvoid)
{
    // Clear The Screen And The Depth Buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Reset The View
    glLoadIdentity();

    // Move One Unit Into The Screen
    glTranslatef(0.0f, 0.0f, -10.0f);

    // Rotate On The X Axis
    glRotatef(rot, 1.0f, 0.0f, 0.0f);

    // Rotate On The Y Axis
    glRotatef(rot * 1.2f, 0.0f, 1.0f, 0.0f);

    // Rotate On The Z Axis
    glRotatef(rot * 1.4f, 0.0f, 0.0f, 1.0f);

    // Move to the Left and Down before drawing
    glTranslatef(-3.5f, 0.0f, 0.0f);

    // Pulsing Colors Based On The Rotation
    glColor3f((1.0f * (cos(rot / 20.0f))),
              (1.0f * (sin(rot / 25.0f))),
              (1.0f - 0.5f * (cos(rot / 17.0f))));

    // Print GL Text To The Screen
    glPrint(m_Text);

    // Make The Text Blue
    glColor3f(0.0f, 0.0f, 1.0f);

    // Increase The Rotation Variable
    rot += 0.1f;
}

LRESULT CALLBACK
WndProc(HWND hWnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
{
    static POINT ptLast;
    static POINT ptCursor;
    static BOOL  fFirstTime = TRUE;
    RECT Screen;							// Used Later On To Get The Size Of The Window
    GLuint PixelFormat;					// Pixel Format Storage
    static PIXELFORMATDESCRIPTOR pfd=		// Pixel Format Descriptor
    {
        sizeof(PIXELFORMATDESCRIPTOR),		// Size Of This Pixel Format Descriptor
        1,									// Version Number (?)
        PFD_DRAW_TO_WINDOW |				// Format Must Support Window
        PFD_SUPPORT_OPENGL |				// Format Must Support OpenGL
        PFD_DOUBLEBUFFER,					// Must Support Double Buffering
        PFD_TYPE_RGBA,						// Request An RGBA Format
        16,									// Select A 16Bit Color Depth
        0, 0, 0, 0, 0, 0,					// Color Bits Ignored (?)
        0,									// No Alpha Buffer
        0,									// Shift Bit Ignored (?)
        0,									// No Accumulation Buffer
        0, 0, 0, 0,							// Accumulation Bits Ignored (?)
        16,									// 16Bit Z-Buffer (Depth Buffer)  
        0,									// No Stencil Buffer
        0,									// No Auxiliary Buffer (?)
        PFD_MAIN_PLANE,						// Main Drawing Layer
        0,									// Reserved (?)
        0, 0, 0								// Layer Masks Ignored (?)
    };

    switch (message)
    {
        case WM_CREATE:
            // Gets A Device Context For The Window
            hDC = GetDC(hWnd);

            // Finds The Closest Match To The Pixel Format We Set Above
            PixelFormat = ChoosePixelFormat(hDC, &pfd);

            // No Matching Pixel Format?
            if (!PixelFormat)
            {
                MessageBox(0, _TEXT("Can't Find A Suitable PixelFormat."), _TEXT("Error"),MB_OK | MB_ICONERROR);

                // This Sends A 'Message' Telling The Program To Quit
                PostQuitMessage(0);
                break;
            }

            // Can We Set The Pixel Mode?
            if (!SetPixelFormat(hDC, PixelFormat, &pfd))
            {
                MessageBox(0, _TEXT("Can't Set The PixelFormat."), _TEXT("Error"), MB_OK | MB_ICONERROR);

                // This Sends A 'Message' Telling The Program To Quit
                PostQuitMessage(0);
                break;
            }

            // Grab A Rendering Context
            hRC = wglCreateContext(hDC);

            // Did We Get One?
            if (!hRC)
            {
                MessageBox(0, _TEXT("Can't Create A GL Rendering Context."), _TEXT("Error"), MB_OK | MB_ICONERROR);

                // This Sends A 'Message' Telling The Program To Quit
                PostQuitMessage(0);
                break;
            }

            // Can We Make The RC Active?
            if (!wglMakeCurrent(hDC, hRC))
            {
                MessageBox(0, _TEXT("Can't Activate GLRC."), _TEXT("Error"), MB_OK | MB_ICONERROR);

                // This Sends A 'Message' Telling The Program To Quit
                PostQuitMessage(0);
                break;
            }

            // Grab Screen Info For The Current Window
            GetClientRect(hWnd, &Screen);

            // Initialize The GL Screen Using Screen Info
            InitGL(Screen.right, Screen.bottom);
            break;

        case WM_DESTROY:
        case WM_CLOSE:
            // Disable Fullscreen Mode
            ChangeDisplaySettings(NULL, 0);

            // Deletes The Font Display List
            KillFont();

            // Make The DC Current
            wglMakeCurrent(hDC, NULL);

            // Kill The RC
            wglDeleteContext(hRC);

            // Free The DC
            ReleaseDC(hWnd, hDC);

            // Quit The Program
            PostQuitMessage(0);
            break;

        case WM_PAINT:
            DrawGLScene();
            SwapBuffers(hDC);
            break;

        case WM_NOTIFY:
        case WM_SYSKEYDOWN:
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            break;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEMOVE:
            // If we've got a parent then we must be a preview
            if (GetParent(hWnd) != 0)
                return 0;

            if (fFirstTime)
            {
                GetCursorPos(&ptLast);
                fFirstTime = FALSE;
            }

            GetCursorPos(&ptCursor);

            // if the mouse has moved more than 3 pixels then exit
            if (abs(ptCursor.x - ptLast.x) >= 3 || abs(ptCursor.y - ptLast.y) >= 3)
                PostMessage(hWnd, WM_CLOSE, 0, 0);

            ptLast = ptCursor;
            return 0;

        case WM_SIZE: // Resizing The Screen
            // Resize To The New Window Size
            ReSizeGLScene(LOWORD(lParam), HIWORD(lParam));
            break;

        default:
            // Pass Windows Messages
            return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

VOID InitSaver(HWND hwndParent)
{
    WNDCLASS wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.style            = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc      = WndProc;
    wc.lpszClassName    = APPNAME;
    RegisterClass(&wc);

    if (hwndParent != 0)
    {
        RECT rect;

        GetClientRect(hwndParent, &rect);
        CreateWindow(APPNAME, APPNAME,
                     WS_VISIBLE | WS_CHILD,
                     0, 0,
                     rect.right,
                     rect.bottom,
                     hwndParent, 0,
                     hInstance, NULL);
        fullscreen = FALSE;
    }
    else
    {
        HWND hwnd;
        hwnd = CreateWindow(APPNAME, APPNAME,
                            WS_VISIBLE | WS_POPUP | WS_EX_TOPMOST,
                            0, 0,
                            GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                            HWND_DESKTOP, 0,
                            hInstance, NULL);
        ShowWindow(hwnd, SW_SHOWMAXIMIZED);
        ShowCursor(FALSE);
        fullscreen = TRUE;
    }
}

//
// Look for any options Windows has passed to us:
//
//  -a <hwnd>   (set password)
//  -s          (screensave)
//  -p <hwnd>   (preview)
//  -c <hwnd>   (configure)
//
VOID ParseCommandLine(LPTSTR szCmdLine, UCHAR *chOption, HWND *hwndParent)
{
    TCHAR ch = *szCmdLine++;

    if (ch == _T('-') || ch == _T('/'))
        ch = *szCmdLine++;

    //convert to lower case
    if (ch >= _T('A') && ch <= _T('Z'))
        ch += _T('a') - _T('A');

    *chOption = ch;
    ch = *szCmdLine++;

    if (ch == _T(':'))
        ch = *szCmdLine++;

    while (ch == _T(' ') || ch == _T('\t'))
        ch = *szCmdLine++;

    if (_istdigit(ch))
    {
        unsigned int i = _ttoi(szCmdLine - 1);
        *hwndParent = (HWND)i;
    }
    else
    {
        *hwndParent = NULL;
    }
}


//
// Dialogbox procedure for Configuration window
//
BOOL CALLBACK ConfigDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
            SetDlgItemText(hwnd, IDC_MESSAGE_TEXT, m_Text);
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                    GetDlgItemText(hwnd, IDC_MESSAGE_TEXT, m_Text, MAX_PATH);
                    SaveSettings();
                    EndDialog(hwnd, IDOK);
                    break;

                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    break;
            }
            return FALSE;

        case WM_CLOSE:
            EndDialog(hwnd, 0);
            break;

        default:
            return FALSE;
    }

    return TRUE;
}

VOID Configure(VOID)
{
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_CONFIG), NULL , (DLGPROC)ConfigDlgProc);
}

INT CALLBACK
_tWinMain(HINSTANCE hInst,
          HINSTANCE hPrev,
          LPTSTR lpCmdLine,
          INT iCmdShow)
{
    HWND hwndParent = 0;
    UCHAR chOption;
    MSG Message;

    hInstance = hInst;

    ParseCommandLine(lpCmdLine, &chOption, &hwndParent);

    LoadSettings();

    switch (chOption)
    {
        case _T('s'):
            InitSaver(0);
            break;

        case _T('p'):
            InitSaver(hwndParent);
            break;

        case _T('c'):
        default:
            Configure();
            return 0;
    }

    while (GetMessage(&Message, 0, 0, 0))
        DispatchMessage(&Message);

    return Message.wParam;
}

