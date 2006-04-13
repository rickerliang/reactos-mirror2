/*
 * Copyright 2001 Andreas Mohr
 * Copyright 2005 Herv� Poussineau
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

#ifndef __SETUPAPI_PRIVATE_H
#define __SETUPAPI_PRIVATE_H

#include <assert.h>
#include <fcntl.h>
#include <share.h>
#include <wchar.h>

#define WIN32_NO_STATUS
#include <windows.h>
#include <cfgmgr32.h>
#include <fdi.h>
#include <regstr.h>
#include <setupapi.h>
#include <shlobj.h>
#include <wine/debug.h>
#include <wine/unicode.h>
#define NTOS_MODE_USER
#include <ndk/ntndk.h>

#include <pnp_c.h>
#include "rpc_private.h"
#include "resource.h"

#define SETUP_DEV_INFO_SET_MAGIC 0xd00ff057
#define SETUP_CLASS_IMAGE_LIST_MAGIC 0xd00ff058

struct DeviceInterface /* Element of DeviceInfoElement.InterfaceListHead */
{
    LIST_ENTRY ListEntry;

    struct DeviceInfoElement* DeviceInfo;
    GUID InterfaceClassGuid;

    
    /* SPINT_ACTIVE : the interface is active/enabled
     * SPINT_DEFAULT: the interface is the default interface for the device class
     * SPINT_REMOVED: the interface is removed
     */
    DWORD Flags;

    WCHAR SymbolicLink[ANYSIZE_ARRAY]; /* \\?\ACPI#PNP0501#4&2658d0a0&0#{GUID} */
};

/* We don't want to open the .inf file to read only one information in it, so keep a handle to it once it
 * has been already loaded once. Keep also a reference counter */
struct InfFileDetails
{
    HINF hInf;
    LONG References;

    /* May contain no directory if the file is already in %SYSTEMROOT%\Inf */
    WCHAR FullInfFileName[ANYSIZE_ARRAY];
};

struct DriverInfoElement /* Element of DeviceInfoSet.DriverListHead and DeviceInfoElement.DriverListHead */
{
    LIST_ENTRY ListEntry;

    DWORD DriverRank;
    ULARGE_INTEGER DriverDate;
    SP_DRVINFO_DATA_V2_W Info;
    SP_DRVINFO_DETAIL_DATA_W Details;
    GUID ClassGuid;
    LPWSTR MatchingId;
    struct InfFileDetails *InfFileDetails;
};

struct ClassInstallParams
{
    PSP_PROPCHANGE_PARAMS PropChange;
};

struct DeviceInfoElement /* Element of DeviceInfoSet.ListHead */
{
    LIST_ENTRY ListEntry;
    DEVINST dnDevInst; /* Used in CM_* functions */

    /* Reserved Field points to a struct DriverInfoElement */
    SP_DEVINSTALL_PARAMS_W InstallParams;

    /* Information about devnode:
     * - DeviceName:
     *       "Root\*PNP0501" for example.
     *       It doesn't contain the unique ID for the device
     *       (points into the Data field at the end of the structure)
     *       WARNING: no NULL char exist between DeviceName and UniqueId
     *       in Data field!
     * - UniqueId
     *       "5&1be2108e&0" or "0000"
     *       If DICD_GENERATE_ID is specified in creation flags,
     *       this unique ID is autogenerated using 4 digits, base 10
     *       (points into the Data field at the end of the structure)
     * - DeviceDescription
     *       String which identifies the device. Can be NULL. If not NULL,
     *       points into the Data field at the end of the structure
     * - ClassGuid
     *       Identifies the class of this device. It is GUID_NULL if the
     *       device has not been installed
     * - CreationFlags
     *       Is a combination of:
     *       - DICD_GENERATE_ID
     *              the unique ID needs to be generated
     *       - DICD_INHERIT_CLASSDRVS
     *              inherit driver of the device info set (== same pointer)
     */
    PCWSTR DeviceName;
    PCWSTR UniqueId;
    PCWSTR DeviceDescription;
    GUID ClassGuid;
    DWORD CreationFlags;

    /* If CreationFlags contains DICD_INHERIT_CLASSDRVS, this list is invalid */
    /* If the driver is not searched/detected, this list is empty */
    LIST_ENTRY DriverListHead; /* List of struct DriverInfoElement */

    /* List of interfaces implemented by this device */
    LIST_ENTRY InterfaceListHead; /* List of struct DeviceInterface */

    /* Used by SetupDiGetClassInstallParamsW/SetupDiSetClassInstallParamsW */
    struct ClassInstallParams ClassInstallParams;

    WCHAR Data[ANYSIZE_ARRAY];
};

struct DeviceInfoSet /* HDEVINFO */
{
    DWORD magic; /* SETUP_DEV_INFO_SET_MAGIC */
    GUID ClassGuid; /* If != GUID_NULL, only devices of this class can be in the device info set */
    HKEY HKLM; /* Local or distant HKEY_LOCAL_MACHINE registry key */
    HMACHINE hMachine; /* Used in CM_* functions */

    /* Reserved Field points to a struct DriverInfoElement */
    SP_DEVINSTALL_PARAMS_W InstallParams;

    /* If the driver is not searched/detected, this list is empty */
    LIST_ENTRY DriverListHead; /* List of struct DriverInfoElement */

    LIST_ENTRY ListHead; /* List of struct DeviceInfoElement */
    struct DeviceInfoElement *SelectedDevice;

    /* Used by SetupDiGetClassInstallParamsW/SetupDiSetClassInstallParamsW */
    struct ClassInstallParams ClassInstallParams;

    /* Contains the name of the remote computer ('\\COMPUTERNAME' for example),
     * or NULL if related to local machine. Points into szData field at the
     * end of the structure */
    PCWSTR MachineName;
    WCHAR szData[ANYSIZE_ARRAY];
};

struct ClassImageList
{
    DWORD magic; /* SETUP_CLASS_IMAGE_LIST_MAGIC */

    /* Contains the name of the remote computer ('\\COMPUTERNAME' for example),
     * or NULL if related to local machine. Points into szData field at the
     * end of the structure */
    PCWSTR MachineName;
    WCHAR szData[ANYSIZE_ARRAY];
};

extern HINSTANCE hInstance;
#define RC_STRING_MAX_SIZE 256

#define REG_INSTALLEDFILES "System\\CurrentControlSet\\Control\\InstalledFiles"
#define REGPART_RENAME "\\Rename"
#define REG_VERSIONCONFLICT "Software\\Microsoft\\VersionConflictManager"

/* string substitutions */

struct inf_file;
extern const WCHAR *DIRID_get_string( HINF hinf, int dirid );
extern unsigned int PARSER_string_substA( struct inf_file *file, const WCHAR *text,
                                          char *buffer, unsigned int size );
extern unsigned int PARSER_string_substW( struct inf_file *file, const WCHAR *text,
                                          WCHAR *buffer, unsigned int size );
extern const WCHAR *PARSER_get_src_root( HINF hinf );
extern WCHAR *PARSER_get_dest_dir( INFCONTEXT *context );

/* support for Ascii queue callback functions */

struct callback_WtoA_context
{
    void               *orig_context;
    PSP_FILE_CALLBACK_A orig_handler;
};

UINT CALLBACK QUEUE_callback_WtoA( void *context, UINT notification, UINT_PTR, UINT_PTR );

/* from msvcrt/sys/stat.h */
#define _S_IWRITE 0x0080
#define _S_IREAD  0x0100

extern HINSTANCE hInstance;
extern OSVERSIONINFOW OsVersionInfo;

DWORD WINAPI CaptureAndConvertAnsiArg(LPCSTR pSrc, LPWSTR *pDst);

BOOL GetStringField( PINFCONTEXT context, DWORD index, PWSTR *value);

#endif /* __SETUPAPI_PRIVATE_H */
