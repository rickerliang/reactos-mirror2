/* $Id$
 *
 * Compatibility header
 *
 * This header is wrapper to allow compilation of Wine DLLs under ReactOS
 * build system. It contains definitions commonly refered to as Wineisms
 * and definitions that are missing in w32api.
 */

#include_next <shlguid.h>

#ifndef __WINE_SHLGUID_H
#define __WINE_SHLGUID_H

DEFINE_SHLGUID(CLSID_ShellDesktop,      0x00021400L, 0, 0);
DEFINE_SHLGUID(CLSID_ShellLink,         0x00021401L, 0, 0);

/* command group ids */
DEFINE_SHLGUID(CGID_Explorer,           0x000214D0L, 0, 0);
DEFINE_SHLGUID(CGID_ShellDocView,       0x000214D1L, 0, 0);

DEFINE_SHLGUID(IID_IExtractIconA,       0x000214EBL, 0, 0);
DEFINE_SHLGUID(IID_IExtractIconW,       0x000214FAL, 0, 0);
DEFINE_SHLGUID(IID_IContextMenu,        0x000214E4L, 0, 0);
DEFINE_SHLGUID(IID_IContextMenu2,       0x000214F4L, 0, 0);
DEFINE_SHLGUID(IID_ICommDlgBrowser,     0x000214F1L, 0, 0);
DEFINE_SHLGUID(IID_IShellBrowser,       0x000214E2L, 0, 0);
DEFINE_SHLGUID(IID_IShellView,          0x000214E3L, 0, 0);
DEFINE_SHLGUID(IID_IShellFolder,        0x000214E6L, 0, 0);
DEFINE_SHLGUID(IID_IShellExtInit,       0x000214E8L, 0, 0);
DEFINE_SHLGUID(IID_IPersistFolder,      0x000214EAL, 0, 0);
DEFINE_SHLGUID(IID_IShellLinkA,         0x000214EEL, 0, 0);
DEFINE_SHLGUID(IID_IEnumIDList,         0x000214F2L, 0, 0);
DEFINE_SHLGUID(IID_IShellLinkW,         0x000214F9L, 0, 0);
DEFINE_SHLGUID(IID_IShellExecuteHookA,  0x000214F5L, 0, 0);
DEFINE_SHLGUID(IID_IShellExecuteHookW,  0x000214FBL, 0, 0);
DEFINE_SHLGUID(IID_IQueryInfo,          0x00021500L, 0, 0);

DEFINE_GUID(SID_STopLevelBrowser, 0x4C96BE40L, 0x915C, 0x11CF, 0x99, 0xD3, 0x00, 0xAA, 0x00, 0x4A, 0xE8, 0x37);

DEFINE_GUID(CLSID_NetworkPlaces, 0x208D2C60, 0x3AEA, 0x1069, 0xA2, 0xD7, 0x08, 0x00, 0x2B, 0x30, 0x30, 0x9D);
DEFINE_GUID(CLSID_MyComputer, 0x20D04FE0, 0x3AEA, 0x1069, 0xA2, 0xD8, 0x08, 0x00, 0x2B, 0x30, 0x30, 0x9D);
DEFINE_GUID(CLSID_Internet, 0x871C5380, 0x42A0, 0x1069, 0xA2, 0xEA, 0x08, 0x00, 0x2B, 0x30, 0x30, 0x9D);
DEFINE_GUID(CLSID_ShellFSFolder, 0xF3364BA0, 0x65B9, 0x11CE, 0xA9, 0xBA, 0x00, 0xAA, 0x00, 0x4A, 0xE8, 0x37);
DEFINE_GUID(CLSID_RecycleBin, 0x645FF040, 0x5081, 0x101B, 0x9F, 0x08, 0x00, 0xAA, 0x00, 0x2F, 0x95, 0x4E);
DEFINE_GUID(CLSID_ControlPanel, 0x21EC2020, 0x3AEA, 0x1069, 0xA2, 0xDD, 0x08, 0x00, 0x2B, 0x30, 0x30, 0x9D);
DEFINE_GUID(CLSID_Printers, 0x2227A280, 0x3AEA, 0x1069, 0xA2, 0xDE, 0x08, 0x00, 0x2B, 0x30, 0x30, 0x9D);
DEFINE_GUID(CLSID_MyDocuments, 0x450d8fba, 0xad25, 0x11d0, 0x98, 0xa8, 0x08, 0x00, 0x36, 0x1b, 0x11, 0x03);
DEFINE_GUID(CLSID_FolderShortcut, 0x0AFACED1, 0xE828, 0x11D1, 0x91, 0x87, 0xB5, 0x32, 0xF1, 0xE9, 0x57, 0x5D);


DEFINE_GUID(IID_IQueryAssociations, 0xc46ca590, 0x3c3f, 0x11d2, 0xbe, 0xe6, 0x00, 0x00, 0xf8, 0x05, 0xca, 0x57);

DEFINE_GUID(CLSID_DragDropHelper, 0x4657278a, 0x411b, 0x11d2, 0x83, 0x9a, 0x0, 0xc0, 0x4f, 0xd9, 0x18, 0xd0);

DEFINE_GUID(CLSID_AutoComplete, 0x00bb2763, 0x6a77, 0x11d0, 0xa5, 0x35, 0x00, 0xc0, 0x4f, 0xd7, 0xd0, 0x62);

DEFINE_GUID(IID_IInputObject,       0x068284FAA, 0x6A48, 0x11D0, 0x8C, 0x78, 0x00, 0xC0, 0x4F, 0xD9, 0x18, 0xB4);
DEFINE_GUID(IID_IInputObjectSite,   0x0F1DB8392, 0x7331, 0x11D0, 0x8C, 0x99, 0x00, 0xA0, 0xC9, 0x2D, 0xBF, 0xE8);

DEFINE_GUID(IID_IShellLinkDataList, 0x45e2b4ae, 0xb1c3, 0x11d0, 0xb9, 0x2f, 0x00, 0xa0, 0xc9, 0x03, 0x12, 0xe1);

DEFINE_SHLGUID(IID_IShellPropSheetExt,  0x000214E9L, 0, 0);

#endif /* __WINE_SHLGUID_H */
