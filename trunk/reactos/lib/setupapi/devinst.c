/*
 * SetupAPI device installer
 *
 * Copyright 2000 Andreas Mohr for CodeWeavers
 *           2005 Herv� Poussineau (hpoussin@reactos.org)
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

#define INITGUID
#include "setupapi_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(setupapi);

/* Unicode constants */
static const WCHAR ClassGUID[]  = {'C','l','a','s','s','G','U','I','D',0};
static const WCHAR Class[]  = {'C','l','a','s','s',0};
static const WCHAR ClassInstall32[]  = {'C','l','a','s','s','I','n','s','t','a','l','l','3','2',0};
static const WCHAR DeviceInstance[]  = {'D','e','v','i','c','e','I','n','s','t','a','n','c','e',0};
static const WCHAR NoDisplayClass[]  = {'N','o','D','i','s','p','l','a','y','C','l','a','s','s',0};
static const WCHAR NoInstallClass[]  = {'N','o','I','s','t','a','l','l','C','l','a','s','s',0};
static const WCHAR NoUseClass[]  = {'N','o','U','s','e','C','l','a','s','s',0};
static const WCHAR NtExtension[]  = {'.','N','T',0};
static const WCHAR NtPlatformExtension[]  = {'.','N','T','x','8','6',0};
static const WCHAR SymbolicLink[]  = {'S','y','m','b','o','l','i','c','L','i','n','k',0};
static const WCHAR Version[]  = {'V','e','r','s','i','o','n',0};
static const WCHAR WinExtension[]  = {'.','W','i','n',0};

/* Registry key and value names */
static const WCHAR ControlClass[] = {'S','y','s','t','e','m','\\',
                                  'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
                                  'C','o','n','t','r','o','l','\\',
                                  'C','l','a','s','s',0};

static const WCHAR DeviceClasses[] = {'S','y','s','t','e','m','\\',
                                  'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
                                  'C','o','n','t','r','o','l','\\',
                                  'D','e','v','i','c','e','C','l','a','s','s','e','s',0};

static const WCHAR EnumKeyName[] = {'S','y','s','t','e','m','\\',
                                  'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
                                  'E','n','u','m',0};


/* FIXME: header mess */
DEFINE_GUID(GUID_NULL,
  0x00000000L, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
typedef DWORD
(CALLBACK* CLASS_INSTALL_PROC) (
    IN DI_FUNCTION InstallFunction,
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL);
typedef BOOL
(WINAPI* DEFAULT_CLASS_INSTALL_PROC) (
    IN HDEVINFO DeviceInfoSet,
    IN OUT PSP_DEVINFO_DATA DeviceInfoData);
typedef DWORD
(CALLBACK* COINSTALLER_PROC) (
    IN DI_FUNCTION InstallFunction,
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL,
    IN OUT PCOINSTALLER_CONTEXT_DATA Context);
typedef BOOL
(WINAPI* PROPERTY_PAGE_PROVIDER) (
    IN PSP_PROPSHEETPAGE_REQUEST PropPageRequest,
    IN LPFNADDPROPSHEETPAGE fAddFunc,
    IN LPARAM lParam);

struct CoInstallerElement
{
    LIST_ENTRY ListEntry;

    HMODULE Module;
    COINSTALLER_PROC Function;
    BOOL DoPostProcessing;
    PVOID PrivateData;
};

/***********************************************************************
 *              SetupDiBuildClassInfoList  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiBuildClassInfoList(
        DWORD Flags,
        LPGUID ClassGuidList,
        DWORD ClassGuidListSize,
        PDWORD RequiredSize)
{
    TRACE("\n");
    return SetupDiBuildClassInfoListExW(Flags, ClassGuidList,
                                        ClassGuidListSize, RequiredSize,
                                        NULL, NULL);
}

/***********************************************************************
 *              SetupDiBuildClassInfoListExA  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiBuildClassInfoListExA(
        DWORD Flags,
        LPGUID ClassGuidList,
        DWORD ClassGuidListSize,
        PDWORD RequiredSize,
        LPCSTR MachineName,
        PVOID Reserved)
{
    LPWSTR MachineNameW = NULL;
    BOOL bResult;

    TRACE("\n");

    if (MachineName)
    {
        MachineNameW = MultiByteToUnicode(MachineName, CP_ACP);
        if (MachineNameW == NULL) return FALSE;
    }

    bResult = SetupDiBuildClassInfoListExW(Flags, ClassGuidList,
                                           ClassGuidListSize, RequiredSize,
                                           MachineNameW, Reserved);

    if (MachineNameW)
        MyFree(MachineNameW);

    return bResult;
}

/***********************************************************************
 *		SetupDiBuildClassInfoListExW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiBuildClassInfoListExW(
        DWORD Flags,
        LPGUID ClassGuidList,
        DWORD ClassGuidListSize,
        PDWORD RequiredSize,
        LPCWSTR MachineName,
        PVOID Reserved)
{
    WCHAR szKeyName[MAX_GUID_STRING_LEN + 1];
    HKEY hClassesKey;
    HKEY hClassKey;
    DWORD dwLength;
    DWORD dwIndex;
    LONG lError;
    DWORD dwGuidListIndex = 0;

    TRACE("0x%lx %p %lu %p %s %p\n", Flags, ClassGuidList,
        ClassGuidListSize, RequiredSize, debugstr_w(MachineName), Reserved);

    if (RequiredSize != NULL)
	*RequiredSize = 0;

    hClassesKey = SetupDiOpenClassRegKeyExW(NULL,
                                            KEY_ENUMERATE_SUB_KEYS,
                                            DIOCR_INSTALLER,
                                            MachineName,
                                            Reserved);
    if (hClassesKey == INVALID_HANDLE_VALUE)
    {
	return FALSE;
    }

    for (dwIndex = 0; ; dwIndex++)
    {
	dwLength = MAX_GUID_STRING_LEN + 1;
	lError = RegEnumKeyExW(hClassesKey,
			       dwIndex,
			       szKeyName,
			       &dwLength,
			       NULL,
			       NULL,
			       NULL,
			       NULL);
	TRACE("RegEnumKeyExW() returns %ld\n", lError);
	if (lError == ERROR_SUCCESS || lError == ERROR_MORE_DATA)
	{
	    TRACE("Key name: %s\n", debugstr_w(szKeyName));

	    if (RegOpenKeyExW(hClassesKey,
			      szKeyName,
			      0,
			      KEY_QUERY_VALUE,
			      &hClassKey))
	    {
		RegCloseKey(hClassesKey);
		return FALSE;
	    }

	    if (!RegQueryValueExW(hClassKey,
				  NoUseClass,
				  NULL,
				  NULL,
				  NULL,
				  NULL))
	    {
		TRACE("'NoUseClass' value found!\n");
		RegCloseKey(hClassKey);
		continue;
	    }

	    if ((Flags & DIBCI_NOINSTALLCLASS) &&
		(!RegQueryValueExW(hClassKey,
				   NoInstallClass,
				   NULL,
				   NULL,
				   NULL,
				   NULL)))
	    {
		TRACE("'NoInstallClass' value found!\n");
		RegCloseKey(hClassKey);
		continue;
	    }

	    if ((Flags & DIBCI_NODISPLAYCLASS) &&
		(!RegQueryValueExW(hClassKey,
				   NoDisplayClass,
				   NULL,
				   NULL,
				   NULL,
				   NULL)))
	    {
		TRACE("'NoDisplayClass' value found!\n");
		RegCloseKey(hClassKey);
		continue;
	    }

	    RegCloseKey(hClassKey);

	    TRACE("Guid: %s\n", debugstr_w(szKeyName));
	    if (dwGuidListIndex < ClassGuidListSize)
	    {
		if (szKeyName[0] == L'{' && szKeyName[37] == L'}')
		{
		    szKeyName[37] = 0;
		}
		TRACE("Guid: %s\n", debugstr_w(&szKeyName[1]));

		UuidFromStringW(&szKeyName[1],
				&ClassGuidList[dwGuidListIndex]);
	    }

	    dwGuidListIndex++;
	}

	if (lError != ERROR_SUCCESS)
	    break;
    }

    RegCloseKey(hClassesKey);

    if (RequiredSize != NULL)
	*RequiredSize = dwGuidListIndex;

    if (ClassGuidListSize < dwGuidListIndex)
    {
	SetLastError(ERROR_INSUFFICIENT_BUFFER);
	return FALSE;
    }

    return TRUE;
}

/***********************************************************************
 *		SetupDiClassGuidsFromNameA  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassGuidsFromNameA(
        LPCSTR ClassName,
        LPGUID ClassGuidList,
        DWORD ClassGuidListSize,
        PDWORD RequiredSize)
{
  return SetupDiClassGuidsFromNameExA(ClassName, ClassGuidList,
                                      ClassGuidListSize, RequiredSize,
                                      NULL, NULL);
}

/***********************************************************************
 *		SetupDiClassGuidsFromNameW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassGuidsFromNameW(
        LPCWSTR ClassName,
        LPGUID ClassGuidList,
        DWORD ClassGuidListSize,
        PDWORD RequiredSize)
{
  return SetupDiClassGuidsFromNameExW(ClassName, ClassGuidList,
                                      ClassGuidListSize, RequiredSize,
                                      NULL, NULL);
}

/***********************************************************************
 *		SetupDiClassGuidsFromNameExA  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassGuidsFromNameExA(
        LPCSTR ClassName,
        LPGUID ClassGuidList,
        DWORD ClassGuidListSize,
        PDWORD RequiredSize,
        LPCSTR MachineName,
        PVOID Reserved)
{
    LPWSTR ClassNameW = NULL;
    LPWSTR MachineNameW = NULL;
    BOOL bResult;

    TRACE("\n");

    ClassNameW = MultiByteToUnicode(ClassName, CP_ACP);
    if (ClassNameW == NULL)
        return FALSE;

    if (MachineNameW)
    {
        MachineNameW = MultiByteToUnicode(MachineName, CP_ACP);
        if (MachineNameW == NULL)
        {
            MyFree(ClassNameW);
            return FALSE;
        }
    }

    bResult = SetupDiClassGuidsFromNameExW(ClassNameW, ClassGuidList,
                                           ClassGuidListSize, RequiredSize,
                                           MachineNameW, Reserved);

    if (MachineNameW)
        MyFree(MachineNameW);

    MyFree(ClassNameW);

    return bResult;
}

/***********************************************************************
 *		SetupDiClassGuidsFromNameExW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassGuidsFromNameExW(
        LPCWSTR ClassName,
        LPGUID ClassGuidList,
        DWORD ClassGuidListSize,
        PDWORD RequiredSize,
        LPCWSTR MachineName,
        PVOID Reserved)
{
    WCHAR szKeyName[MAX_GUID_STRING_LEN + 1];
    WCHAR szClassName[256];
    HKEY hClassesKey;
    HKEY hClassKey;
    DWORD dwLength;
    DWORD dwIndex;
    LONG lError;
    DWORD dwGuidListIndex = 0;

    TRACE("%s %p %lu %p %s %p\n", debugstr_w(ClassName), ClassGuidList,
        ClassGuidListSize, RequiredSize, debugstr_w(MachineName), Reserved);

    if (RequiredSize != NULL)
	*RequiredSize = 0;

    hClassesKey = SetupDiOpenClassRegKeyExW(NULL,
                                            KEY_ENUMERATE_SUB_KEYS,
                                            DIOCR_INSTALLER,
                                            MachineName,
                                            Reserved);
    if (hClassesKey == INVALID_HANDLE_VALUE)
    {
	return FALSE;
    }

    for (dwIndex = 0; ; dwIndex++)
    {
	dwLength = MAX_GUID_STRING_LEN + 1;
	lError = RegEnumKeyExW(hClassesKey,
			       dwIndex,
			       szKeyName,
			       &dwLength,
			       NULL,
			       NULL,
			       NULL,
			       NULL);
	TRACE("RegEnumKeyExW() returns %ld\n", lError);
	if (lError == ERROR_SUCCESS || lError == ERROR_MORE_DATA)
	{
	    TRACE("Key name: %s\n", debugstr_w(szKeyName));

	    if (RegOpenKeyExW(hClassesKey,
			      szKeyName,
			      0,
			      KEY_QUERY_VALUE,
			      &hClassKey))
	    {
		RegCloseKey(hClassesKey);
		return FALSE;
	    }

	    dwLength = 256 * sizeof(WCHAR);
	    if (!RegQueryValueExW(hClassKey,
				  Class,
				  NULL,
				  NULL,
				  (LPBYTE)szClassName,
				  &dwLength))
	    {
		TRACE("Class name: %s\n", debugstr_w(szClassName));

		if (strcmpiW(szClassName, ClassName) == 0)
		{
		    TRACE("Found matching class name\n");

		    TRACE("Guid: %s\n", debugstr_w(szKeyName));
		    if (dwGuidListIndex < ClassGuidListSize)
		    {
			if (szKeyName[0] == L'{' && szKeyName[37] == L'}')
			{
			    szKeyName[37] = 0;
			}
			TRACE("Guid: %s\n", debugstr_w(&szKeyName[1]));

			UuidFromStringW(&szKeyName[1],
					&ClassGuidList[dwGuidListIndex]);
		    }

		    dwGuidListIndex++;
		}
	    }

	    RegCloseKey(hClassKey);
	}

	if (lError != ERROR_SUCCESS)
	    break;
    }

    RegCloseKey(hClassesKey);

    if (RequiredSize != NULL)
	*RequiredSize = dwGuidListIndex;

    if (ClassGuidListSize < dwGuidListIndex)
    {
	SetLastError(ERROR_INSUFFICIENT_BUFFER);
	return FALSE;
    }

    return TRUE;
}

/***********************************************************************
 *              SetupDiClassNameFromGuidA  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassNameFromGuidA(
        const GUID* ClassGuid,
        PSTR ClassName,
        DWORD ClassNameSize,
        PDWORD RequiredSize)
{
  return SetupDiClassNameFromGuidExA(ClassGuid, ClassName,
                                     ClassNameSize, RequiredSize,
                                     NULL, NULL);
}

/***********************************************************************
 *              SetupDiClassNameFromGuidW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassNameFromGuidW(
        const GUID* ClassGuid,
        PWSTR ClassName,
        DWORD ClassNameSize,
        PDWORD RequiredSize)
{
  return SetupDiClassNameFromGuidExW(ClassGuid, ClassName,
                                     ClassNameSize, RequiredSize,
                                     NULL, NULL);
}

/***********************************************************************
 *              SetupDiClassNameFromGuidExA  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassNameFromGuidExA(
        const GUID* ClassGuid,
        PSTR ClassName,
        DWORD ClassNameSize,
        PDWORD RequiredSize,
        PCSTR MachineName,
        PVOID Reserved)
{
    WCHAR ClassNameW[MAX_CLASS_NAME_LEN];
    LPWSTR MachineNameW = NULL;
    BOOL ret;

    if (MachineName)
        MachineNameW = MultiByteToUnicode(MachineName, CP_ACP);
    ret = SetupDiClassNameFromGuidExW(ClassGuid, ClassNameW, MAX_CLASS_NAME_LEN,
     NULL, MachineNameW, Reserved);
    if (ret)
    {
        int len = WideCharToMultiByte(CP_ACP, 0, ClassNameW, -1, ClassName,
         ClassNameSize, NULL, NULL);

        if (!ClassNameSize && RequiredSize)
            *RequiredSize = len;
    }
    MyFree(MachineNameW);
    return ret;
}

/***********************************************************************
 *		SetupDiClassNameFromGuidExW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassNameFromGuidExW(
        const GUID* ClassGuid,
        PWSTR ClassName,
        DWORD ClassNameSize,
        PDWORD RequiredSize,
        PCWSTR MachineName,
        PVOID Reserved)
{
    HKEY hKey;
    DWORD dwLength;
    LONG rc;

    TRACE("%s %p %lu %p %s %p\n", debugstr_guid(ClassGuid), ClassName,
        ClassNameSize, RequiredSize, debugstr_w(MachineName), Reserved);

    hKey = SetupDiOpenClassRegKeyExW(ClassGuid,
                                     KEY_QUERY_VALUE,
                                     DIOCR_INSTALLER,
                                     MachineName,
                                     Reserved);
    if (hKey == INVALID_HANDLE_VALUE)
    {
	return FALSE;
    }

    if (RequiredSize != NULL)
    {
	dwLength = 0;
	rc = RegQueryValueExW(hKey,
			     Class,
			     NULL,
			     NULL,
			     NULL,
			     &dwLength);
	if (rc != ERROR_SUCCESS)
	{
	    SetLastError(rc);
	    RegCloseKey(hKey);
	    return FALSE;
	}

	*RequiredSize = dwLength / sizeof(WCHAR);
    }

    dwLength = ClassNameSize * sizeof(WCHAR);
    rc = RegQueryValueExW(hKey,
			 Class,
			 NULL,
			 NULL,
			 (LPBYTE)ClassName,
			 &dwLength);
    if (rc != ERROR_SUCCESS)
    {
	SetLastError(rc);
	RegCloseKey(hKey);
	return FALSE;
    }

    RegCloseKey(hKey);

    return TRUE;
}

/***********************************************************************
 *		SetupDiCreateDeviceInfoList (SETUPAPI.@)
 */
HDEVINFO WINAPI
SetupDiCreateDeviceInfoList(const GUID *ClassGuid,
			    HWND hwndParent)
{
  return SetupDiCreateDeviceInfoListExW(ClassGuid, hwndParent, NULL, NULL);
}

/***********************************************************************
 *		SetupDiCreateDeviceInfoListExA (SETUPAPI.@)
 */
HDEVINFO WINAPI
SetupDiCreateDeviceInfoListExA(const GUID *ClassGuid,
			       HWND hwndParent,
			       PCSTR MachineName,
			       PVOID Reserved)
{
    LPWSTR MachineNameW = NULL;
    HDEVINFO hDevInfo;

    TRACE("%s %p %s %p\n", debugstr_guid(ClassGuid), hwndParent,
      debugstr_a(MachineName), Reserved);

    if (MachineName)
    {
        MachineNameW = MultiByteToUnicode(MachineName, CP_ACP);
        if (MachineNameW == NULL)
            return (HDEVINFO)INVALID_HANDLE_VALUE;
    }

    hDevInfo = SetupDiCreateDeviceInfoListExW(ClassGuid, hwndParent,
                                              MachineNameW, Reserved);

    if (MachineNameW)
        MyFree(MachineNameW);

    return hDevInfo;
}

static DWORD
GetErrorCodeFromCrCode(const IN CONFIGRET cr)
{
  switch (cr)
  {
    case CR_INVALID_MACHINENAME: return ERROR_INVALID_COMPUTERNAME;
    case CR_OUT_OF_MEMORY: return ERROR_NOT_ENOUGH_MEMORY;
    case CR_SUCCESS: return ERROR_SUCCESS;
    default:
      /* FIXME */
      return ERROR_GEN_FAILURE;
  }

  /* Does not happen */
}

/***********************************************************************
 *		SetupDiCreateDeviceInfoListExW (SETUPAPI.@)
 */
HDEVINFO WINAPI
SetupDiCreateDeviceInfoListExW(const GUID *ClassGuid,
			       HWND hwndParent,
			       PCWSTR MachineName,
			       PVOID Reserved)
{
  struct DeviceInfoSet *list;
  LPWSTR UNCServerName = NULL;
  DWORD size;
  DWORD rc;
  //CONFIGRET cr;
  HDEVINFO ret = (HDEVINFO)INVALID_HANDLE_VALUE;;

  TRACE("%s %p %s %p\n", debugstr_guid(ClassGuid), hwndParent,
      debugstr_w(MachineName), Reserved);

  size = FIELD_OFFSET(struct DeviceInfoSet, szData);
  if (MachineName)
    size += (wcslen(MachineName) + 3) * sizeof(WCHAR);
  list = HeapAlloc(GetProcessHeap(), 0, size);
  if (!list)
  {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    goto cleanup;
  }
  memset(list, 0, sizeof(struct DeviceInfoSet));

  list->magic = SETUP_DEV_INFO_SET_MAGIC;
  memcpy(
    &list->ClassGuid,
    ClassGuid ? ClassGuid : &GUID_NULL,
    sizeof(list->ClassGuid));
  list->InstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS_W);
  list->InstallParams.Flags |= DI_CLASSINSTALLPARAMS;
  list->InstallParams.hwndParent = hwndParent;
  if (MachineName)
  {
    rc = RegConnectRegistryW(MachineName, HKEY_LOCAL_MACHINE, &list->HKLM);
    if (rc != ERROR_SUCCESS)
    {
      SetLastError(rc);
      goto cleanup;
    }
    UNCServerName = HeapAlloc(GetProcessHeap(), 0, (strlenW(MachineName) + 3) * sizeof(WCHAR));
    if (!UNCServerName)
    {
      SetLastError(ERROR_NOT_ENOUGH_MEMORY);
      goto cleanup;
    }

    strcpyW(UNCServerName + 2, MachineName);
    list->szData[0] = list->szData[1] = '\\';
    strcpyW(list->szData + 2, MachineName);
    list->MachineName = list->szData;
  }
  else
  {
    DWORD Size = MAX_PATH;
    list->HKLM = HKEY_LOCAL_MACHINE;
    UNCServerName = HeapAlloc(GetProcessHeap(), 0, (MAX_PATH + 2) * sizeof(WCHAR));
    if (!UNCServerName)
    {
      SetLastError(ERROR_NOT_ENOUGH_MEMORY);
      goto cleanup;
    }
    if (!GetComputerNameW(UNCServerName + 2, &Size))
      goto cleanup;
    list->MachineName = NULL;
  }
#if 0
  UNCServerName[0] = UNCServerName[1] = '\\';
  cr = CM_Connect_MachineW(UNCServerName, &list->hMachine);
  if (cr != CR_SUCCESS)
  {
    SetLastError(GetErrorCodeFromCrCode(cr));
    goto cleanup;
  }
#endif
  InitializeListHead(&list->DriverListHead);
  InitializeListHead(&list->ListHead);

  ret = (HDEVINFO)list;

cleanup:
  if (ret == INVALID_HANDLE_VALUE)
  {
    if (list && list->HKLM != 0 && list->HKLM != HKEY_LOCAL_MACHINE)
      RegCloseKey(list->HKLM);
    HeapFree(GetProcessHeap(), 0, list);
  }
  HeapFree(GetProcessHeap(), 0, UNCServerName);
  return ret;
}

/***********************************************************************
 *		SetupDiEnumDeviceInfo (SETUPAPI.@)
 */
BOOL WINAPI SetupDiEnumDeviceInfo(
        HDEVINFO DeviceInfoSet,
        DWORD MemberIndex,
        PSP_DEVINFO_DATA DeviceInfoData)
{
    BOOL ret = FALSE;

    TRACE("%p, 0x%08lx, %p\n", DeviceInfoSet, MemberIndex, DeviceInfoData);
    if (!DeviceInfoData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoSet && DeviceInfoSet != (HDEVINFO)INVALID_HANDLE_VALUE)
    {
        struct DeviceInfoSet *list = (struct DeviceInfoSet *)DeviceInfoSet;

        if (list->magic != SETUP_DEV_INFO_SET_MAGIC)
            SetLastError(ERROR_INVALID_HANDLE);
        else if (DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
            SetLastError(ERROR_INVALID_USER_BUFFER);
        else
        {
            PLIST_ENTRY ItemList = list->ListHead.Flink;
            while (ItemList != &list->ListHead && MemberIndex-- > 0)
                ItemList = ItemList->Flink;
            if (ItemList == &list->ListHead)
                SetLastError(ERROR_NO_MORE_ITEMS);
            else
            {
                struct DeviceInfoElement *DevInfo = (struct DeviceInfoElement *)ItemList;
                memcpy(&DeviceInfoData->ClassGuid,
                    &DevInfo->ClassGuid,
                    sizeof(GUID));
                DeviceInfoData->DevInst = DevInfo->dnDevInst;
                DeviceInfoData->Reserved = (ULONG_PTR)DevInfo;
                ret = TRUE;
            }
        }
    }
    else
        SetLastError(ERROR_INVALID_HANDLE);
    return ret;
}

/***********************************************************************
 *		SetupDiGetActualSectionToInstallA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetActualSectionToInstallA(
        HINF InfHandle,
        PCSTR InfSectionName,
        PSTR InfSectionWithExt,
        DWORD InfSectionWithExtSize,
        PDWORD RequiredSize,
        PSTR *Extension)
{
    LPWSTR InfSectionNameW = NULL;
    PWSTR InfSectionWithExtW = NULL;
    PWSTR ExtensionW;
    BOOL bResult = FALSE;

    TRACE("\n");

    if (InfSectionName)
    {
        InfSectionNameW = MultiByteToUnicode(InfSectionName, CP_ACP);
        if (InfSectionNameW == NULL) goto end;
    }
    if (InfSectionWithExt)
    {
        InfSectionWithExtW = HeapAlloc(GetProcessHeap(), 0, InfSectionWithExtSize * sizeof(WCHAR));
        if (InfSectionWithExtW == NULL) goto end;
    }

    bResult = SetupDiGetActualSectionToInstallW(InfHandle, InfSectionNameW,
                                                InfSectionWithExt ? InfSectionNameW : NULL,
                                                InfSectionWithExtSize, RequiredSize,
                                                Extension ? &ExtensionW : NULL);

    if (bResult && InfSectionWithExt)
    {
         bResult = WideCharToMultiByte(CP_ACP, 0, InfSectionWithExtW, -1, InfSectionWithExt,
             InfSectionWithExtSize, NULL, NULL) != 0;
    }
    if (bResult && Extension)
    {
        if (ExtensionW == NULL)
            *Extension = NULL;
         else
            *Extension = &InfSectionWithExt[ExtensionW - InfSectionWithExtW];
    }

end:
    if (InfSectionNameW) MyFree(InfSectionNameW);
    if (InfSectionWithExtW) HeapFree(GetProcessHeap(), 0, InfSectionWithExtW);

    return bResult;
}

/***********************************************************************
 *		SetupDiGetActualSectionToInstallW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetActualSectionToInstallW(
        HINF InfHandle,
        PCWSTR InfSectionName,
        PWSTR InfSectionWithExt,
        DWORD InfSectionWithExtSize,
        PDWORD RequiredSize,
        PWSTR *Extension)
{
    WCHAR szBuffer[MAX_PATH];
    DWORD dwLength;
    DWORD dwFullLength;
    LONG lLineCount = -1;

    TRACE("%p %s %p %lu %p %p\n", InfHandle, debugstr_w(InfSectionName),
        InfSectionWithExt, InfSectionWithExtSize, RequiredSize, Extension);

    lstrcpyW(szBuffer, InfSectionName);
    dwLength = lstrlenW(szBuffer);

    if (OsVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
	/* Test section name with '.NTx86' extension */
	lstrcpyW(&szBuffer[dwLength], NtPlatformExtension);
	lLineCount = SetupGetLineCountW(InfHandle, szBuffer);

	if (lLineCount == -1)
	{
	    /* Test section name with '.NT' extension */
	    lstrcpyW(&szBuffer[dwLength], NtExtension);
	    lLineCount = SetupGetLineCountW(InfHandle, szBuffer);
	}
    }
    else
    {
	/* Test section name with '.Win' extension */
	lstrcpyW(&szBuffer[dwLength], WinExtension);
	lLineCount = SetupGetLineCountW(InfHandle, szBuffer);
    }

    if (lLineCount == -1)
    {
	/* Test section name without extension */
	szBuffer[dwLength] = 0;
	lLineCount = SetupGetLineCountW(InfHandle, szBuffer);
    }

    if (lLineCount == -1)
    {
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }

    dwFullLength = lstrlenW(szBuffer);

    if (InfSectionWithExt != NULL && InfSectionWithExtSize != 0)
    {
	if (InfSectionWithExtSize < (dwFullLength + 1))
	{
	    SetLastError(ERROR_INSUFFICIENT_BUFFER);
	    return FALSE;
	}

	lstrcpyW(InfSectionWithExt, szBuffer);
	if (Extension != NULL)
	{
	    *Extension = (dwLength == dwFullLength) ? NULL : &InfSectionWithExt[dwLength];
	}
    }

    if (RequiredSize != NULL)
    {
	*RequiredSize = dwFullLength + 1;
    }

    return TRUE;
}

/***********************************************************************
 *		SetupDiGetClassDescriptionA  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetClassDescriptionA(
        const GUID* ClassGuid,
        PSTR ClassDescription,
        DWORD ClassDescriptionSize,
        PDWORD RequiredSize)
{
  return SetupDiGetClassDescriptionExA(ClassGuid, ClassDescription,
                                       ClassDescriptionSize,
                                       RequiredSize, NULL, NULL);
}

/***********************************************************************
 *		SetupDiGetClassDescriptionW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetClassDescriptionW(
        const GUID* ClassGuid,
        PWSTR ClassDescription,
        DWORD ClassDescriptionSize,
        PDWORD RequiredSize)
{
  return SetupDiGetClassDescriptionExW(ClassGuid, ClassDescription,
                                       ClassDescriptionSize,
                                       RequiredSize, NULL, NULL);
}

/***********************************************************************
 *		SetupDiGetClassDescriptionExA  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetClassDescriptionExA(
        const GUID* ClassGuid,
        PSTR ClassDescription,
        DWORD ClassDescriptionSize,
        PDWORD RequiredSize,
        PCSTR MachineName,
        PVOID Reserved)
{
    PWCHAR ClassDescriptionW;
    LPWSTR MachineNameW = NULL;
    BOOL ret;

    TRACE("\n");
    if (ClassDescriptionSize > 0)
    {
        ClassDescriptionW = HeapAlloc(GetProcessHeap(), 0, ClassDescriptionSize * sizeof(WCHAR));
        if (!ClassDescriptionW)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            ret = FALSE;
            goto end;
        }
    }
    else
        ClassDescriptionW = NULL;

    if (MachineName)
    {
        MachineNameW = MultiByteToUnicode(MachineName, CP_ACP);
        if (!MachineNameW)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            ret = FALSE;
            goto end;
        }
    }

    ret = SetupDiGetClassDescriptionExW(ClassGuid, ClassDescriptionW, ClassDescriptionSize * sizeof(WCHAR),
     NULL, MachineNameW, Reserved);
    if (ret)
    {
        int len = WideCharToMultiByte(CP_ACP, 0, ClassDescriptionW, -1, ClassDescription,
         ClassDescriptionSize, NULL, NULL);

        if (!ClassDescriptionSize && RequiredSize)
            *RequiredSize = len;
    }

end:
    HeapFree(GetProcessHeap(), 0, ClassDescriptionW);
    MyFree(MachineNameW);
    return ret;
}

/***********************************************************************
 *		SetupDiGetClassDescriptionExW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetClassDescriptionExW(
        const GUID* ClassGuid,
        PWSTR ClassDescription,
        DWORD ClassDescriptionSize,
        PDWORD RequiredSize,
        PCWSTR MachineName,
        PVOID Reserved)
{
    HKEY hKey;
    DWORD dwLength;

    TRACE("%s %p %lu %p %s %p\n", debugstr_guid(ClassGuid), ClassDescription,
        ClassDescriptionSize, RequiredSize, debugstr_w(MachineName), Reserved);

    hKey = SetupDiOpenClassRegKeyExW(ClassGuid,
                                     KEY_QUERY_VALUE,
                                     DIOCR_INSTALLER,
                                     MachineName,
                                     Reserved);
    if (hKey == INVALID_HANDLE_VALUE)
    {
	WARN("SetupDiOpenClassRegKeyExW() failed (Error %lu)\n", GetLastError());
	return FALSE;
    }

    if (RequiredSize != NULL)
    {
	dwLength = 0;
	if (RegQueryValueExW(hKey,
			     NULL,
			     NULL,
			     NULL,
			     NULL,
			     &dwLength))
	{
	    RegCloseKey(hKey);
	    return FALSE;
	}

	*RequiredSize = dwLength / sizeof(WCHAR);
    }

    dwLength = ClassDescriptionSize * sizeof(WCHAR);
    if (RegQueryValueExW(hKey,
			 NULL,
			 NULL,
			 NULL,
			 (LPBYTE)ClassDescription,
			 &dwLength))
    {
	RegCloseKey(hKey);
	return FALSE;
    }

    RegCloseKey(hKey);

    return TRUE;
}

/***********************************************************************
 *		SetupDiGetClassDevsA (SETUPAPI.@)
 */
HDEVINFO WINAPI SetupDiGetClassDevsA(
       CONST GUID *class,
       LPCSTR enumstr,
       HWND parent,
       DWORD flags)
{
    return SetupDiGetClassDevsExA(class, enumstr, parent,
                                  flags, NULL, NULL, NULL);
}

/***********************************************************************
 *		SetupDiGetClassDevsW (SETUPAPI.@)
 */
HDEVINFO WINAPI SetupDiGetClassDevsW(
       CONST GUID *class,
       LPCWSTR enumstr,
       HWND parent,
       DWORD flags)
{
    return SetupDiGetClassDevsExW(class, enumstr, parent,
                                  flags, NULL, NULL, NULL);
}

/***********************************************************************
 *		SetupDiGetClassDevsExA (SETUPAPI.@)
 */
HDEVINFO WINAPI SetupDiGetClassDevsExA(
       CONST GUID *class,
       LPCSTR enumstr,
       HWND parent,
       DWORD flags,
       HDEVINFO deviceset,
       LPCSTR machine,
       PVOID reserved)
{
    HDEVINFO ret;
    LPWSTR enumstrW = NULL;
    LPWSTR machineW = NULL;

    if (enumstr)
    {
        int len = MultiByteToWideChar(CP_ACP, 0, enumstr, -1, NULL, 0);
        enumstrW = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
        if (!enumstrW)
        {
            ret = (HDEVINFO)INVALID_HANDLE_VALUE;
            goto end;
        }
        MultiByteToWideChar(CP_ACP, 0, enumstr, -1, enumstrW, len);
    }
    if (machine)
    {
        int len = MultiByteToWideChar(CP_ACP, 0, machine, -1, NULL, 0);
        machineW = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
        if (!machineW)
        {
            ret = (HDEVINFO)INVALID_HANDLE_VALUE;
            goto end;
        }
        MultiByteToWideChar(CP_ACP, 0, machine, -1, machineW, len);
    }
    ret = SetupDiGetClassDevsExW(class, enumstrW, parent, flags, deviceset, machineW, reserved);

end:
    HeapFree(GetProcessHeap(), 0, enumstrW);
    HeapFree(GetProcessHeap(), 0, machineW);
    return ret;
}

static BOOL
CreateDeviceInfoElement(
    IN struct DeviceInfoSet *list,
    IN LPCWSTR InstancePath,
    IN LPCGUID pClassGuid,
    OUT struct DeviceInfoElement **pDeviceInfo)
{
    DWORD size;
    CONFIGRET cr;
    struct DeviceInfoElement *deviceInfo;

    *pDeviceInfo = NULL;

    size = FIELD_OFFSET(struct DeviceInfoElement, Data) + (wcslen(InstancePath) + 1) * sizeof(WCHAR);
    deviceInfo = HeapAlloc(GetProcessHeap(), 0, size);
    if (!deviceInfo)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    memset(deviceInfo, 0, size);

    cr = CM_Locate_DevNode_ExW(&deviceInfo->dnDevInst, (DEVINSTID_W)InstancePath, CM_LOCATE_DEVNODE_PHANTOM, list->hMachine);
    if (cr != CR_SUCCESS)
    {
        SetLastError(GetErrorCodeFromCrCode(cr));
        return FALSE;
    }

    deviceInfo->InstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS_W);
    wcscpy(deviceInfo->Data, InstancePath);
    deviceInfo->DeviceName = deviceInfo->Data;
    deviceInfo->UniqueId = wcsrchr(deviceInfo->Data, '\\');
    deviceInfo->DeviceDescription = NULL;
    memcpy(&deviceInfo->ClassGuid, pClassGuid, sizeof(GUID));
    deviceInfo->CreationFlags = 0;
    InitializeListHead(&deviceInfo->DriverListHead);
    InitializeListHead(&deviceInfo->InterfaceListHead);

    *pDeviceInfo = deviceInfo;
    return TRUE;
}

static BOOL
CreateDeviceInterface(
    IN struct DeviceInfoElement* deviceInfo,
    IN LPCWSTR SymbolicLink,
    IN LPCGUID pInterfaceGuid,
    OUT struct DeviceInterface **pDeviceInterface)
{
    struct DeviceInterface *deviceInterface;

    *pDeviceInterface = NULL;

    deviceInterface = HeapAlloc(GetProcessHeap(), 0,
        FIELD_OFFSET(struct DeviceInterface, SymbolicLink) + (wcslen(SymbolicLink) + 1) * sizeof(WCHAR));
    if (!deviceInterface)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    deviceInterface->DeviceInfo = deviceInfo;
    wcscpy(deviceInterface->SymbolicLink, SymbolicLink);
    deviceInterface->Flags = 0; /* FIXME */
    memcpy(&deviceInterface->InterfaceClassGuid, pInterfaceGuid, sizeof(GUID));

    *pDeviceInterface = deviceInterface;
    return TRUE;
}

static LONG SETUP_CreateDevListFromEnumerator(
       struct DeviceInfoSet *list,
       LPCGUID pClassGuid OPTIONAL,
       LPCWSTR Enumerator,
       HKEY hEnumeratorKey) /* handle to Enumerator registry key */
{
    HKEY hDeviceIdKey, hInstanceIdKey;
    WCHAR KeyBuffer[MAX_PATH];
    WCHAR InstancePath[MAX_PATH];
    LPWSTR pEndOfInstancePath; /* Pointer into InstancePath buffer */
    struct DeviceInfoElement *deviceInfo;
    DWORD i = 0, j;
    DWORD dwLength, dwRegType;
    DWORD rc;

    /* Enumerate device IDs (subkeys of hEnumeratorKey) */
    while (TRUE)
    {
        dwLength = sizeof(KeyBuffer) / sizeof(KeyBuffer[0]);
        rc = RegEnumKeyExW(hEnumeratorKey, i, KeyBuffer, &dwLength, NULL, NULL, NULL, NULL);
        if (rc == ERROR_NO_MORE_ITEMS)
            break;
        if (rc != ERROR_SUCCESS)
            return rc;
        i++;

        /* Open device id sub key */
        rc = RegOpenKeyExW(hEnumeratorKey, KeyBuffer, 0, KEY_ENUMERATE_SUB_KEYS, &hDeviceIdKey);
        if (rc != ERROR_SUCCESS)
            return rc;
        wcscpy(InstancePath, Enumerator);
        wcscat(InstancePath, L"\\");
        wcscat(InstancePath, KeyBuffer);
        wcscat(InstancePath, L"\\");
        pEndOfInstancePath = &InstancePath[wcslen(InstancePath)];

        /* Enumerate instance IDs (subkeys of hDeviceIdKey) */
        j = 0;
        while (TRUE)
        {
            GUID KeyGuid;

            dwLength = sizeof(KeyBuffer) / sizeof(KeyBuffer[0]);
            rc = RegEnumKeyExW(hDeviceIdKey, j, KeyBuffer, &dwLength, NULL, NULL, NULL, NULL);
            if (rc == ERROR_NO_MORE_ITEMS)
                break;
            if (rc != ERROR_SUCCESS)
            {
                RegCloseKey(hDeviceIdKey);
                return rc;
            }
            j++;

            /* Open instance id sub key */
            rc = RegOpenKeyExW(hDeviceIdKey, KeyBuffer, 0, KEY_QUERY_VALUE, &hInstanceIdKey);
            if (rc != ERROR_SUCCESS)
            {
                RegCloseKey(hDeviceIdKey);
                return rc;
            }
            *pEndOfInstancePath = '\0';
            wcscat(InstancePath, KeyBuffer);

            /* Read ClassGUID value */
            dwLength = sizeof(KeyBuffer) - sizeof(WCHAR);
            rc = RegQueryValueExW(hInstanceIdKey, ClassGUID, NULL, &dwRegType, (LPBYTE)KeyBuffer, &dwLength);
            RegCloseKey(hInstanceIdKey);
            if (rc == ERROR_FILE_NOT_FOUND)
            {
                if (pClassGuid)
                    /* Skip this bad entry as we can't verify it */
                    continue;
                /* Set a default GUID for this device */
                memcpy(&KeyGuid, &GUID_NULL, sizeof(GUID));
            }
            else if (rc != ERROR_SUCCESS)
            {
                RegCloseKey(hDeviceIdKey);
                return rc;
            }
            else if (dwRegType != REG_SZ)
            {
                RegCloseKey(hDeviceIdKey);
                return ERROR_GEN_FAILURE;
            }
            else
            {
                KeyBuffer[37] = '\0'; /* Replace the } by a NULL character */
                if (UuidFromStringW(&KeyBuffer[1], &KeyGuid) != RPC_S_OK)
                    /* Bad GUID, skip the entry */
                    continue;
            }

            if (pClassGuid && !IsEqualIID(&KeyGuid, pClassGuid))
            {
                /* Skip this entry as it is not the right device class */
                continue;
            }

            /* Add the entry to the list */
            if (!CreateDeviceInfoElement(list, InstancePath, &KeyGuid, &deviceInfo))
            {
                RegCloseKey(hDeviceIdKey);
                return GetLastError();
            }
            TRACE("Adding '%S' to device info set %p\n", InstancePath, list);
            InsertTailList(&list->ListHead, &deviceInfo->ListEntry);
        }
        RegCloseKey(hDeviceIdKey);
    }

    return ERROR_SUCCESS;
}

static LONG SETUP_CreateDevList(
       struct DeviceInfoSet *list,
       PCWSTR MachineName OPTIONAL,
       LPGUID class OPTIONAL,
       PCWSTR Enumerator OPTIONAL)
{
    HKEY HKLM, hEnumKey, hEnumeratorKey;
    WCHAR KeyBuffer[MAX_PATH];
    DWORD i;
    DWORD dwLength;
    DWORD rc;

    if (class && IsEqualIID(class, &GUID_NULL))
        class = NULL;

    /* Open Enum key */
    if (MachineName != NULL)
    {
        rc = RegConnectRegistryW(MachineName, HKEY_LOCAL_MACHINE, &HKLM);
        if (rc != ERROR_SUCCESS)
            return rc;
    }
    else
        HKLM = HKEY_LOCAL_MACHINE;

    rc = RegOpenKeyExW(HKLM,
        EnumKeyName,
        0,
        KEY_ENUMERATE_SUB_KEYS,
        &hEnumKey);
    if (MachineName != NULL) RegCloseKey(HKLM);
    if (rc != ERROR_SUCCESS)
        return rc;

    /* If enumerator is provided, call directly SETUP_CreateDevListFromEnumerator.
     * Else, enumerate all enumerators and call SETUP_CreateDevListFromEnumerator
     * for each one.
     */
    if (Enumerator)
    {
        rc = RegOpenKeyExW(
            hEnumKey,
            Enumerator,
            0,
            KEY_ENUMERATE_SUB_KEYS,
            &hEnumeratorKey);
        RegCloseKey(hEnumKey);
        if (rc != ERROR_SUCCESS)
            return rc;
        rc = SETUP_CreateDevListFromEnumerator(list, class, Enumerator, hEnumeratorKey);
        RegCloseKey(hEnumeratorKey);
        return rc;
    }
    else
    {
        /* Enumerate enumerators */
        i = 0;
        while (TRUE)
        {
            dwLength = sizeof(KeyBuffer) / sizeof(KeyBuffer[0]);
            rc = RegEnumKeyExW(hEnumKey, i, KeyBuffer, &dwLength, NULL, NULL, NULL, NULL);
            if (rc == ERROR_NO_MORE_ITEMS)
                break;
            if (rc != ERROR_SUCCESS)
            {
                RegCloseKey(hEnumKey);
                return rc;
            }
            i++;

            /* Open sub key */
            rc = RegOpenKeyExW(hEnumKey, KeyBuffer, 0, KEY_ENUMERATE_SUB_KEYS, &hEnumeratorKey);
            if (rc != ERROR_SUCCESS)
            {
                RegCloseKey(hEnumKey);
                return rc;
            }

            /* Call SETUP_CreateDevListFromEnumerator */
            rc = SETUP_CreateDevListFromEnumerator(list, class, KeyBuffer, hEnumeratorKey);
            RegCloseKey(hEnumeratorKey);
            if (rc != ERROR_SUCCESS)
            {
                RegCloseKey(hEnumKey);
                return rc;
            }
        }
        RegCloseKey(hEnumKey);
        return ERROR_SUCCESS;
    }
}

#ifndef __REACTOS__
static LONG SETUP_CreateSerialDeviceList(
       struct DeviceInfoSet *list,
       PCWSTR MachineName,
       LPGUID InterfaceGuid,
       PCWSTR DeviceInstanceW)
{
    static const size_t initialSize = 100;
    size_t size;
    WCHAR buf[initialSize];
    LPWSTR devices;
    static const WCHAR devicePrefixW[] = { 'C','O','M',0 };
    LPWSTR ptr;
    struct DeviceInfoElement *deviceInfo;

    if (MachineName)
        WARN("'MachineName' is ignored on Wine!\n");
    if (DeviceInstanceW)
        WARN("'DeviceInstanceW' can't be set on Wine!\n");

    devices = buf;
    size = initialSize;
    while (TRUE)
    {
        if (QueryDosDeviceW(NULL, devices, size) != 0)
            break;
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            size *= 2;
            if (devices != buf)
                HeapFree(GetProcessHeap(), 0, devices);
            devices = HeapAlloc(GetProcessHeap(), 0, size * sizeof(WCHAR));
            if (!devices)
                return ERROR_NOT_ENOUGH_MEMORY;
            *devices = '\0';
        }
        else
        {
            if (devices != buf)
                HeapFree(GetProcessHeap(), 0, devices);
            return GetLastError();
        }
    }

    /* 'devices' is a MULTI_SZ string */
    for (ptr = devices; *ptr; ptr += strlenW(ptr) + 1)
    {
        if (strncmpW(devicePrefixW, ptr, sizeof(devicePrefixW) / sizeof(devicePrefixW[0]) - 1) == 0)
        {
            /* We have found a device */
            struct DeviceInterface *interfaceInfo;
            TRACE("Adding %s to list\n", debugstr_w(ptr));
            /* Step 1. Create a device info element */
            if (!CreateDeviceInfoElement(list, ptr, &GUID_SERENUM_BUS_ENUMERATOR, &deviceInfo))
            {
                if (devices != buf)
                    HeapFree(GetProcessHeap(), 0, devices);
                return GetLastError();
            }
            InsertTailList(&list->ListHead, &deviceInfo->ListEntry);

            /* Step 2. Create an interface list for this element */
            if (!CreateDeviceInterface(deviceInfo, ptr, InterfaceGuid, &interfaceInfo))
            {
                if (devices != buf)
                    HeapFree(GetProcessHeap(), 0, devices);
                return GetLastError();
            }
            InsertTailList(&deviceInfo->InterfaceListHead, &interfaceInfo->ListEntry);
        }
    }
    if (devices != buf)
        HeapFree(GetProcessHeap(), 0, devices);
    return ERROR_SUCCESS;
}

#else /* __REACTOS__ */

static LONG SETUP_CreateInterfaceList(
       struct DeviceInfoSet *list,
       PCWSTR MachineName,
       LPGUID InterfaceGuid,
       PCWSTR DeviceInstanceW /* OPTIONAL */)
{
    HKEY hInterfaceKey;      /* HKLM\SYSTEM\CurrentControlSet\Control\DeviceClasses\{GUID} */
    HKEY hDeviceInstanceKey; /* HKLM\SYSTEM\CurrentControlSet\Control\DeviceClasses\{GUID}\##?#{InstancePath} */
    HKEY hReferenceKey;      /* HKLM\SYSTEM\CurrentControlSet\Control\DeviceClasses\{GUID}\##?#{InstancePath}\#{ReferenceString} */
    HKEY hEnumKey;           /* HKLM\SYSTEM\CurrentControlSet\Enum */
    HKEY hKey;               /* HKLM\SYSTEM\CurrentControlSet\Enum\{Instance\Path} */
    LONG rc;
    WCHAR KeyBuffer[max(MAX_PATH, MAX_GUID_STRING_LEN) + 1];
    PWSTR InstancePath;
    DWORD i, j;
    DWORD dwLength, dwInstancePathLength;
    DWORD dwRegType;
    GUID ClassGuid;
    struct DeviceInfoElement *deviceInfo;

    /* Open registry key related to this interface */
    hInterfaceKey = SetupDiOpenClassRegKeyExW(InterfaceGuid, KEY_ENUMERATE_SUB_KEYS, DIOCR_INTERFACE, MachineName, NULL);
    if (hInterfaceKey == INVALID_HANDLE_VALUE)
        return GetLastError();

    /* Enumerate sub keys of hInterfaceKey */
    i = 0;
    while (TRUE)
    {
        dwLength = sizeof(KeyBuffer) / sizeof(KeyBuffer[0]);
        rc = RegEnumKeyExW(hInterfaceKey, i, KeyBuffer, &dwLength, NULL, NULL, NULL, NULL);
        if (rc == ERROR_NO_MORE_ITEMS)
            break;
        if (rc != ERROR_SUCCESS)
        {
            RegCloseKey(hInterfaceKey);
            return rc;
        }
        i++;

        /* Open sub key */
        rc = RegOpenKeyExW(hInterfaceKey, KeyBuffer, 0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &hDeviceInstanceKey);
        if (rc != ERROR_SUCCESS)
        {
            RegCloseKey(hInterfaceKey);
            return rc;
        }

        /* Read DeviceInstance */
        rc = RegQueryValueExW(hDeviceInstanceKey, DeviceInstance, NULL, &dwRegType, NULL, &dwInstancePathLength);
        if (rc != ERROR_SUCCESS )
        {
            RegCloseKey(hDeviceInstanceKey);
            RegCloseKey(hInterfaceKey);
            return rc;
        }
        if (dwRegType != REG_SZ)
        {
            RegCloseKey(hDeviceInstanceKey);
            RegCloseKey(hInterfaceKey);
            return ERROR_GEN_FAILURE;
        }
        InstancePath = HeapAlloc(GetProcessHeap(), 0, dwInstancePathLength + sizeof(WCHAR));
        if (!InstancePath)
        {
            RegCloseKey(hDeviceInstanceKey);
            RegCloseKey(hInterfaceKey);
            return ERROR_NOT_ENOUGH_MEMORY;
        }
        rc = RegQueryValueExW(hDeviceInstanceKey, DeviceInstance, NULL, NULL, (LPBYTE)InstancePath, &dwInstancePathLength);
        if (rc != ERROR_SUCCESS)
        {
            HeapFree(GetProcessHeap(), 0, InstancePath);
            RegCloseKey(hDeviceInstanceKey);
            RegCloseKey(hInterfaceKey);
            return rc;
        }
        InstancePath[dwInstancePathLength / sizeof(WCHAR)] = '\0';
        TRACE("DeviceInstance %s\n", debugstr_w(InstancePath));

        if (DeviceInstanceW)
        {
            /* Check if device enumerator is not the right one */
            if (wcscmp(DeviceInstanceW, InstancePath) != 0)
            {
                HeapFree(GetProcessHeap(), 0, InstancePath);
                RegCloseKey(hDeviceInstanceKey);
                continue;
            }
        }

        /* Find class GUID associated to the device instance */
        rc = RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            EnumKeyName,
            0, /* Options */
            KEY_ENUMERATE_SUB_KEYS,
            &hEnumKey);
        if (rc != ERROR_SUCCESS)
        {
            HeapFree(GetProcessHeap(), 0, InstancePath);
            RegCloseKey(hDeviceInstanceKey);
            RegCloseKey(hInterfaceKey);
            return rc;
        }
        rc = RegOpenKeyExW(
            hEnumKey,
            InstancePath,
            0, /* Options */
            KEY_QUERY_VALUE,
            &hKey);
        RegCloseKey(hEnumKey);
        if (rc != ERROR_SUCCESS)
        {
            HeapFree(GetProcessHeap(), 0, InstancePath);
            RegCloseKey(hDeviceInstanceKey);
            RegCloseKey(hInterfaceKey);
            return rc;
        }
        dwLength = sizeof(KeyBuffer) - sizeof(WCHAR);
        rc = RegQueryValueExW(hKey, ClassGUID, NULL, NULL, (LPBYTE)KeyBuffer, &dwLength);
        RegCloseKey(hKey);
        if (rc != ERROR_SUCCESS)
        {
            HeapFree(GetProcessHeap(), 0, InstancePath);
            RegCloseKey(hDeviceInstanceKey);
            RegCloseKey(hInterfaceKey);
            return rc;
        }
        KeyBuffer[dwLength / sizeof(WCHAR)] = '\0';
        KeyBuffer[37] = '\0'; /* Replace the } by a NULL character */
        if (UuidFromStringW(&KeyBuffer[1], &ClassGuid) != RPC_S_OK)
        {
            HeapFree(GetProcessHeap(), 0, InstancePath);
            RegCloseKey(hDeviceInstanceKey);
            RegCloseKey(hInterfaceKey);
            return ERROR_GEN_FAILURE;
        }
        TRACE("ClassGUID %s\n", debugstr_guid(&ClassGuid));

        /* If current device doesn't match the list GUID (if any), skip this entry */
        if (!IsEqualIID(&list->ClassGuid, &GUID_NULL) && !IsEqualIID(&list->ClassGuid, &ClassGuid))
        {
            HeapFree(GetProcessHeap(), 0, InstancePath);
            RegCloseKey(hDeviceInstanceKey);
            continue;
        }

        /* Enumerate subkeys of hDeviceInstanceKey (ie "#ReferenceString" in IoRegisterDeviceInterface). Skip entries that don't start with '#' */
        j = 0;
        while (TRUE)
        {
            LPWSTR pSymbolicLink;
            struct DeviceInterface *interfaceInfo;

            dwLength = sizeof(KeyBuffer) / sizeof(KeyBuffer[0]);
            rc = RegEnumKeyExW(hDeviceInstanceKey, j, KeyBuffer, &dwLength, NULL, NULL, NULL, NULL);
            if (rc == ERROR_NO_MORE_ITEMS)
                break;
            if (rc != ERROR_SUCCESS)
            {
                HeapFree(GetProcessHeap(), 0, InstancePath);
                RegCloseKey(hDeviceInstanceKey);
                RegCloseKey(hInterfaceKey);
                return rc;
            }
            j++;
            if (KeyBuffer[0] != '#')
                /* This entry doesn't represent an interesting entry */
                continue;

            /* Open sub key */
            rc = RegOpenKeyExW(hDeviceInstanceKey, KeyBuffer, 0, KEY_QUERY_VALUE, &hReferenceKey);
            if (rc != ERROR_SUCCESS)
            {
                RegCloseKey(hDeviceInstanceKey);
                RegCloseKey(hInterfaceKey);
                return rc;
            }

            /* Read SymbolicLink value */
            rc = RegQueryValueExW(hReferenceKey, SymbolicLink, NULL, &dwRegType, NULL, &dwLength);
            if (rc != ERROR_SUCCESS )
            {
                RegCloseKey(hReferenceKey);
                RegCloseKey(hDeviceInstanceKey);
                RegCloseKey(hInterfaceKey);
                return rc;
            }
            if (dwRegType != REG_SZ)
            {
                RegCloseKey(hReferenceKey);
                RegCloseKey(hDeviceInstanceKey);
                RegCloseKey(hInterfaceKey);
                return ERROR_GEN_FAILURE;
            }

            /* We have found a device */
            /* Step 1. Create a device info element */
            if (!CreateDeviceInfoElement(list, InstancePath, &ClassGuid, &deviceInfo))
            {
                RegCloseKey(hReferenceKey);
                RegCloseKey(hDeviceInstanceKey);
                RegCloseKey(hInterfaceKey);
                return GetLastError();
            }
            TRACE("Adding device %s to list\n", debugstr_w(InstancePath));
            InsertTailList(&list->ListHead, &deviceInfo->ListEntry);

            /* Step 2. Create an interface list for this element */
            pSymbolicLink = HeapAlloc(GetProcessHeap(), 0, (dwLength + 1) * sizeof(WCHAR));
            if (!pSymbolicLink)
            {
                RegCloseKey(hReferenceKey);
                RegCloseKey(hDeviceInstanceKey);
                RegCloseKey(hInterfaceKey);
                return ERROR_NOT_ENOUGH_MEMORY;
            }
            rc = RegQueryValueExW(hReferenceKey, SymbolicLink, NULL, NULL, (LPBYTE)pSymbolicLink, &dwLength);
            pSymbolicLink[dwLength / sizeof(WCHAR)] = '\0';
            RegCloseKey(hReferenceKey);
            if (rc != ERROR_SUCCESS)
            {
                HeapFree(GetProcessHeap(), 0, pSymbolicLink);
                RegCloseKey(hDeviceInstanceKey);
                RegCloseKey(hInterfaceKey);
                return rc;
            }
            if (!CreateDeviceInterface(deviceInfo, pSymbolicLink, InterfaceGuid, &interfaceInfo))
            {
                HeapFree(GetProcessHeap(), 0, pSymbolicLink);
                RegCloseKey(hDeviceInstanceKey);
                RegCloseKey(hInterfaceKey);
                return GetLastError();
            }
            TRACE("Adding interface %s to list\n", debugstr_w(pSymbolicLink));
            HeapFree(GetProcessHeap(), 0, pSymbolicLink);
            InsertTailList(&deviceInfo->InterfaceListHead, &interfaceInfo->ListEntry);
        }
        RegCloseKey(hDeviceInstanceKey);
    }
    RegCloseKey(hInterfaceKey);
    return ERROR_SUCCESS;
}
#endif /* __REACTOS__ */

/***********************************************************************
 *		SetupDiGetClassDevsExW (SETUPAPI.@)
 */
HDEVINFO WINAPI SetupDiGetClassDevsExW(
       CONST GUID *class,
       LPCWSTR enumstr,
       HWND parent,
       DWORD flags,
       HDEVINFO deviceset,
       LPCWSTR machine,
       PVOID reserved)
{
    HDEVINFO hDeviceInfo = INVALID_HANDLE_VALUE;
    struct DeviceInfoSet *list;
    LPGUID pClassGuid;
    LONG rc;

    TRACE("%s %s %p 0x%08lx %p %s %p\n", debugstr_guid(class), debugstr_w(enumstr),
     parent, flags, deviceset, debugstr_w(machine), reserved);

    /* Create the deviceset if not set */
    if (deviceset)
    {
        list = (struct DeviceInfoSet *)deviceset;
        if (list->magic != SETUP_DEV_INFO_SET_MAGIC)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return INVALID_HANDLE_VALUE;
        }
        hDeviceInfo = deviceset;
    }
    else
    {
         hDeviceInfo = SetupDiCreateDeviceInfoListExW(
             flags & DIGCF_DEVICEINTERFACE ? NULL : class,
             NULL, machine, NULL);
         if (hDeviceInfo == INVALID_HANDLE_VALUE)
             return INVALID_HANDLE_VALUE;
         list = (struct DeviceInfoSet *)hDeviceInfo;
    }

    if (IsEqualIID(&list->ClassGuid, &GUID_NULL))
        pClassGuid = NULL;
    else
        pClassGuid = &list->ClassGuid;

    if (flags & DIGCF_PRESENT)
        FIXME(": flag DIGCF_PRESENT ignored\n");
    if (flags & DIGCF_PROFILE)
        FIXME(": flag DIGCF_PROFILE ignored\n");

    if (flags & DIGCF_ALLCLASSES)
    {
        rc = SETUP_CreateDevList(list, machine, pClassGuid, enumstr);
        if (rc != ERROR_SUCCESS)
        {
            SetLastError(rc);
            if (!deviceset)
                SetupDiDestroyDeviceInfoList(hDeviceInfo);
            return INVALID_HANDLE_VALUE;
        }
        return hDeviceInfo;
    }
    else if (flags & DIGCF_DEVICEINTERFACE)
    {
        if (class == NULL)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            if (!deviceset)
                SetupDiDestroyDeviceInfoList(hDeviceInfo);
            return INVALID_HANDLE_VALUE;
        }

#ifndef __REACTOS__
        /* Special case: find serial ports by calling QueryDosDevice */
        if (IsEqualIID(class, &GUID_DEVINTERFACE_COMPORT))
            rc = SETUP_CreateSerialDeviceList(list, machine, (LPGUID)class, enumstr);
        if (IsEqualIID(class, &GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR))
            rc = SETUP_CreateSerialDeviceList(list, machine, (LPGUID)class, enumstr);
        else
        {
            ERR("Wine can only enumerate serial devices at the moment!\n");
            rc = ERROR_INVALID_PARAMETER;
        }
#else /* __REACTOS__ */
        rc = SETUP_CreateInterfaceList(list, machine, (LPGUID)class, enumstr);
#endif /* __REACTOS__ */
        if (rc != ERROR_SUCCESS)
        {
            SetLastError(rc);
            if (!deviceset)
                SetupDiDestroyDeviceInfoList(hDeviceInfo);
            return INVALID_HANDLE_VALUE;
        }
        return hDeviceInfo;
    }
    else
    {
        rc = SETUP_CreateDevList(list, machine, (LPGUID)class, enumstr);
        if (rc != ERROR_SUCCESS)
        {
            SetLastError(rc);
            if (!deviceset)
                SetupDiDestroyDeviceInfoList(hDeviceInfo);
            return INVALID_HANDLE_VALUE;
        }
        return hDeviceInfo;
    }
}

/***********************************************************************
 *		SetupDiGetClassImageIndex (SETUPAPI.@)
 */

static BOOL GetIconIndex(
       IN HKEY hClassKey,
       OUT PINT ImageIndex)
{
    LPWSTR Buffer = NULL;
    DWORD dwRegType, dwLength;
    LONG rc;
    BOOL ret = FALSE;

    /* Read "Icon" registry key */
    rc = RegQueryValueExW(hClassKey, L"Icon", NULL, &dwRegType, NULL, &dwLength);
    if (rc != ERROR_SUCCESS)
    {
        SetLastError(rc);
        goto cleanup;
    } else if (dwRegType != REG_SZ)
    {
        SetLastError(ERROR_INVALID_INDEX);
        goto cleanup;
    }
    Buffer = MyMalloc(dwLength + sizeof(WCHAR));
    if (!Buffer)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        goto cleanup;
    }
    rc = RegQueryValueExW(hClassKey, L"Icon", NULL, NULL, (LPBYTE)Buffer, &dwLength);
    if (rc != ERROR_SUCCESS)
    {
        SetLastError(rc);
        goto cleanup;
    }
    /* make sure the returned buffer is NULL-terminated */
    Buffer[dwLength / sizeof(WCHAR)] = 0;

    /* Transform "Icon" value to a INT */
    *ImageIndex = atoiW(Buffer);
    ret = TRUE;

cleanup:
    MyFree(Buffer);
    return ret;
}

BOOL WINAPI SetupDiGetClassImageIndex(
       IN PSP_CLASSIMAGELIST_DATA ClassImageListData,
       IN CONST GUID *ClassGuid,
       OUT PINT ImageIndex)
{
    struct ClassImageList *list;
    BOOL ret = FALSE;

    TRACE("%p %s %p\n", ClassImageListData, debugstr_guid(ClassGuid), ImageIndex);

    if (!ClassImageListData || !ClassGuid || !ImageIndex)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (ClassImageListData->cbSize != sizeof(SP_CLASSIMAGELIST_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if ((list = (struct ClassImageList *)ClassImageListData->Reserved) == NULL)
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (list->magic != SETUP_CLASS_IMAGE_LIST_MAGIC)
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (!ImageIndex)
        SetLastError(ERROR_INVALID_PARAMETER);
    else
    {
        HKEY hKey = INVALID_HANDLE_VALUE;
        INT iconIndex;

        /* Read Icon registry entry into Buffer */
        hKey = SetupDiOpenClassRegKeyExW(ClassGuid, KEY_QUERY_VALUE, DIOCR_INTERFACE, list->MachineName, NULL);
        if (hKey == INVALID_HANDLE_VALUE)
            goto cleanup;
        if (!GetIconIndex(hKey, &iconIndex))
            goto cleanup;

        if (iconIndex >= 0)
        {
            SetLastError(ERROR_INVALID_INDEX);
            goto cleanup;
        }

        *ImageIndex = -iconIndex;
        ret = TRUE;

cleanup:
        if (hKey != INVALID_HANDLE_VALUE)
            RegCloseKey(hKey);
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiGetClassImageList(SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetClassImageList(
       OUT PSP_CLASSIMAGELIST_DATA ClassImageListData)
{
    return SetupDiGetClassImageListExW(ClassImageListData, NULL, NULL);
}

/***********************************************************************
 *		SetupDiGetClassImageListExA(SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetClassImageListExA(
       OUT PSP_CLASSIMAGELIST_DATA ClassImageListData,
       IN PCSTR MachineName OPTIONAL,
       IN PVOID Reserved)
{
    PWSTR MachineNameW = NULL;
    BOOL ret;

    if (MachineName)
    {
        MachineNameW = MultiByteToUnicode(MachineName, CP_ACP);
        if (MachineNameW == NULL)
            return FALSE;
    }

    ret = SetupDiGetClassImageListExW(ClassImageListData, MachineNameW, Reserved);

    if (MachineNameW)
        MyFree(MachineNameW);

    return ret;
}

/***********************************************************************
 *		SetupDiGetClassImageListExW(SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetClassImageListExW(
       OUT PSP_CLASSIMAGELIST_DATA ClassImageListData,
       IN PCWSTR MachineName OPTIONAL,
       IN PVOID Reserved)
{
    BOOL ret = FALSE;

    TRACE("%p %p %p\n", ClassImageListData, debugstr_w(MachineName), Reserved);

    if (!ClassImageListData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (ClassImageListData->cbSize != sizeof(SP_CLASSIMAGELIST_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (Reserved)
        SetLastError(ERROR_INVALID_PARAMETER);
    else
    {
        struct ClassImageList *list = NULL;
        DWORD size;

        size = FIELD_OFFSET(struct ClassImageList, szData);
        if (MachineName)
            size += (wcslen(MachineName) + 3) * sizeof(WCHAR);
        list = HeapAlloc(GetProcessHeap(), 0, size);
        if (!list)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            goto cleanup;
        }
        list->magic = SETUP_CLASS_IMAGE_LIST_MAGIC;
        if (MachineName)
        {
            list->szData[0] = list->szData[1] = '\\';
            strcpyW(list->szData + 2, MachineName);
            list->MachineName = list->szData;
        }
        else
        {
            list->MachineName = NULL;
        }

        ClassImageListData->Reserved = (DWORD)list; /* FIXME: 64 bit portability issue */
        ret = TRUE;

cleanup:
        if (!ret)
            MyFree(list);
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiLoadClassIcon(SETUPAPI.@)
 */
BOOL WINAPI SetupDiLoadClassIcon(
       IN CONST GUID *ClassGuid,
       OUT HICON *LargeIcon OPTIONAL,
       OUT PINT MiniIconIndex OPTIONAL)
{
    BOOL ret = FALSE;

    if (!ClassGuid)
        SetLastError(ERROR_INVALID_PARAMETER);
    else
    {
        LPWSTR Buffer = NULL;
        LPCWSTR DllName;
        INT iconIndex;
        HKEY hKey = INVALID_HANDLE_VALUE;

        hKey = SetupDiOpenClassRegKey(ClassGuid, KEY_QUERY_VALUE);
        if (hKey == INVALID_HANDLE_VALUE)
            goto cleanup;

        if (!GetIconIndex(hKey, &iconIndex))
            goto cleanup;

        if (iconIndex > 0)
        {
            /* Look up icon in dll specified by Installer32 or EnumPropPages32 key */
            PWCHAR Comma;
            LONG rc;
            DWORD dwRegType, dwLength;
            rc = RegQueryValueExW(hKey, L"Installer32", NULL, &dwRegType, NULL, &dwLength);
            if (rc == ERROR_SUCCESS && dwRegType == REG_SZ)
            {
                Buffer = MyMalloc(dwLength + sizeof(WCHAR));
                if (Buffer == NULL)
                {
                    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                    goto cleanup;
                }
                rc = RegQueryValueExW(hKey, L"Installer32", NULL, NULL, (LPBYTE)Buffer, &dwLength);
                if (rc != ERROR_SUCCESS)
                {
                    SetLastError(rc);
                    goto cleanup;
                }
                /* make sure the returned buffer is NULL-terminated */
                Buffer[dwLength / sizeof(WCHAR)] = 0;
            }
            else if
                (ERROR_SUCCESS == (rc = RegQueryValueExW(hKey, L"EnumPropPages32", NULL, &dwRegType, NULL, &dwLength))
                && dwRegType == REG_SZ)
            {
                Buffer = MyMalloc(dwLength + sizeof(WCHAR));
                if (Buffer == NULL)
                {
                    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                    goto cleanup;
                }
                rc = RegQueryValueExW(hKey, L"EnumPropPages32", NULL, NULL, (LPBYTE)Buffer, &dwLength);
                if (rc != ERROR_SUCCESS)
                {
                    SetLastError(rc);
                    goto cleanup;
                }
                /* make sure the returned buffer is NULL-terminated */
                Buffer[dwLength / sizeof(WCHAR)] = 0;
            }
            else
            {
                /* Unable to find where to load the icon */
                SetLastError(ERROR_FILE_NOT_FOUND);
                goto cleanup;
            }
            Comma = strchrW(Buffer, ',');
            if (!Comma)
            {
                SetLastError(ERROR_GEN_FAILURE);
                goto cleanup;
            }
            *Comma = '\0';
            DllName = Buffer;
        }
        else
        {
            /* Look up icon in setupapi.dll */
            DllName = L"setupapi.dll";
            iconIndex = -iconIndex;
        }

        TRACE("Icon index %d, dll name %s\n", iconIndex, debugstr_w(DllName));
        if (LargeIcon)
        {
            if (1 != ExtractIconEx(DllName, iconIndex, LargeIcon, NULL, 1))
            {
                SetLastError(ERROR_INVALID_INDEX);
                goto cleanup;
            }
        }
        if (MiniIconIndex)
            *MiniIconIndex = iconIndex;
        ret = TRUE;

cleanup:
        if (hKey != INVALID_HANDLE_VALUE)
            RegCloseKey(hKey);
        MyFree(Buffer);
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiEnumDeviceInterfaces (SETUPAPI.@)
 */
BOOL WINAPI SetupDiEnumDeviceInterfaces(
       HDEVINFO DeviceInfoSet,
       PSP_DEVINFO_DATA DeviceInfoData,
       CONST GUID * InterfaceClassGuid,
       DWORD MemberIndex,
       PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData)
{
    BOOL ret = FALSE;

    TRACE("%p, %p, %s, %ld, %p\n", DeviceInfoSet, DeviceInfoData,
     debugstr_guid(InterfaceClassGuid), MemberIndex, DeviceInterfaceData);

    if (!DeviceInterfaceData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInterfaceData->cbSize != sizeof(SP_DEVICE_INTERFACE_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (DeviceInfoSet && DeviceInfoSet != (HDEVINFO)INVALID_HANDLE_VALUE)
    {
        struct DeviceInfoSet *list = (struct DeviceInfoSet *)DeviceInfoSet;

        if (list->magic == SETUP_DEV_INFO_SET_MAGIC)
        {
            PLIST_ENTRY ItemList = list->ListHead.Flink;
            BOOL Found = FALSE;
            while (ItemList != &list->ListHead && !Found)
            {
                PLIST_ENTRY InterfaceListEntry;
                struct DeviceInfoElement *DevInfo = (struct DeviceInfoElement *)ItemList;
                if (DeviceInfoData && (struct DeviceInfoElement *)DeviceInfoData->Reserved != DevInfo)
                {
                    /* We are not searching for this element */
                    ItemList = ItemList->Flink;
                    continue;
                }
                InterfaceListEntry = DevInfo->InterfaceListHead.Flink;
                while (InterfaceListEntry != &DevInfo->InterfaceListHead && !Found)
                {
                    struct DeviceInterface *DevItf = (struct DeviceInterface *)InterfaceListEntry;
                    if (!IsEqualIID(&DevItf->InterfaceClassGuid, InterfaceClassGuid))
                    {
                        InterfaceListEntry = InterfaceListEntry->Flink;
                        continue;
                    }
                    if (MemberIndex-- == 0)
                    {
                        /* return this item */
                        memcpy(&DeviceInterfaceData->InterfaceClassGuid,
                            &DevItf->InterfaceClassGuid,
                            sizeof(GUID));
                        DeviceInterfaceData->Flags = 0; /* FIXME */
                        DeviceInterfaceData->Reserved = (ULONG_PTR)DevItf;
                        Found = TRUE;
                    }
                    InterfaceListEntry = InterfaceListEntry->Flink;
                }
                ItemList = ItemList->Flink;
            }
            if (!Found)
                SetLastError(ERROR_NO_MORE_ITEMS);
            else
                ret = TRUE;
        }
        else
            SetLastError(ERROR_INVALID_HANDLE);
    }
    else
        SetLastError(ERROR_INVALID_HANDLE);
    return ret;
}

static VOID ReferenceInfFile(struct InfFileDetails* infFile)
{
    InterlockedIncrement(&infFile->References);
}

static VOID DereferenceInfFile(struct InfFileDetails* infFile)
{
    if (InterlockedDecrement(&infFile->References) == 0)
    {
        SetupCloseInfFile(infFile->hInf);
        HeapFree(GetProcessHeap(), 0, infFile);
    }
}

static BOOL DestroyDriverInfoElement(struct DriverInfoElement* driverInfo)
{
    DereferenceInfFile(driverInfo->InfFileDetails);
    HeapFree(GetProcessHeap(), 0, driverInfo->MatchingId);
    HeapFree(GetProcessHeap(), 0, driverInfo);
    return TRUE;
}

static BOOL DestroyDeviceInfoElement(struct DeviceInfoElement* deviceInfo)
{
    PLIST_ENTRY ListEntry;
    struct DriverInfoElement *driverInfo;

    while (!IsListEmpty(&deviceInfo->DriverListHead))
    {
        ListEntry = RemoveHeadList(&deviceInfo->DriverListHead);
        driverInfo = (struct DriverInfoElement *)ListEntry;
        if (!DestroyDriverInfoElement(driverInfo))
            return FALSE;
    }
    while (!IsListEmpty(&deviceInfo->InterfaceListHead))
    {
        ListEntry = RemoveHeadList(&deviceInfo->InterfaceListHead);
        HeapFree(GetProcessHeap(), 0, ListEntry);
    }
    HeapFree(GetProcessHeap(), 0, deviceInfo);
    return TRUE;
}

static BOOL DestroyDeviceInfoSet(struct DeviceInfoSet* list)
{
    PLIST_ENTRY ListEntry;
    struct DeviceInfoElement *deviceInfo;

    while (!IsListEmpty(&list->ListHead))
    {
        ListEntry = RemoveHeadList(&list->ListHead);
        deviceInfo = (struct DeviceInfoElement *)ListEntry;
        if (!DestroyDeviceInfoElement(deviceInfo))
            return FALSE;
    }
    if (list->HKLM != HKEY_LOCAL_MACHINE)
        RegCloseKey(list->HKLM);
    CM_Disconnect_Machine(list->hMachine);
    HeapFree(GetProcessHeap(), 0, list);
    return TRUE;
}

/***********************************************************************
 *		SetupDiDestroyDeviceInfoList (SETUPAPI.@)
 */
BOOL WINAPI SetupDiDestroyDeviceInfoList(HDEVINFO devinfo)
{
    BOOL ret = FALSE;

    TRACE("%p\n", devinfo);
    if (devinfo && devinfo != (HDEVINFO)INVALID_HANDLE_VALUE)
    {
        struct DeviceInfoSet *list = (struct DeviceInfoSet *)devinfo;

        if (list->magic == SETUP_DEV_INFO_SET_MAGIC)
            ret = DestroyDeviceInfoSet(list);
        else
            SetLastError(ERROR_INVALID_HANDLE);
    }
    else
        SetLastError(ERROR_INVALID_HANDLE);

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiGetDeviceInterfaceDetailA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetDeviceInterfaceDetailA(
      HDEVINFO DeviceInfoSet,
      PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
      PSP_DEVICE_INTERFACE_DETAIL_DATA_A DeviceInterfaceDetailData,
      DWORD DeviceInterfaceDetailDataSize,
      PDWORD RequiredSize,
      PSP_DEVINFO_DATA DeviceInfoData)
{
    PSP_DEVICE_INTERFACE_DETAIL_DATA_W DeviceInterfaceDetailDataW = NULL;
    DWORD sizeW = 0, sizeA;
    BOOL ret = FALSE;

    TRACE("%p %p %p %lu %p %p\n", DeviceInfoSet,
        DeviceInterfaceData, DeviceInterfaceDetailData,
        DeviceInterfaceDetailDataSize, RequiredSize, DeviceInfoData);

    if (DeviceInterfaceDetailData->cbSize != sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (DeviceInterfaceDetailData == NULL && DeviceInterfaceDetailDataSize != 0)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInterfaceDetailData != NULL && DeviceInterfaceDetailDataSize < FIELD_OFFSET(SP_DEVICE_INTERFACE_DETAIL_DATA_A, DevicePath) + 1)
        SetLastError(ERROR_INVALID_PARAMETER);
    else
    {
        if (DeviceInterfaceDetailData != NULL)
        {
            sizeW = FIELD_OFFSET(SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath)
                + (DeviceInterfaceDetailDataSize - FIELD_OFFSET(SP_DEVICE_INTERFACE_DETAIL_DATA_A, DevicePath)) * sizeof(WCHAR);
            DeviceInterfaceDetailDataW = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)HeapAlloc(GetProcessHeap(), 0, sizeW);
            if (!DeviceInterfaceDetailDataW)
            {
                SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            }
        }
        if (!DeviceInterfaceDetailData || (DeviceInterfaceDetailData && DeviceInterfaceDetailDataW))
        {
            DeviceInterfaceDetailDataW->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            ret = SetupDiGetDeviceInterfaceDetailW(
                DeviceInfoSet,
                DeviceInterfaceData,
                DeviceInterfaceDetailDataW,
                sizeW,
                &sizeW,
                DeviceInfoData);
            sizeA = (sizeW - FIELD_OFFSET(SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath)) / sizeof(WCHAR)
                + FIELD_OFFSET(SP_DEVICE_INTERFACE_DETAIL_DATA_A, DevicePath);
            if (RequiredSize)
                *RequiredSize = sizeA;
            if (ret && DeviceInterfaceDetailData && DeviceInterfaceDetailDataSize <= sizeA)
            {
                if (!WideCharToMultiByte(
                    CP_ACP, 0,
                    DeviceInterfaceDetailDataW->DevicePath, -1,
                    DeviceInterfaceDetailData->DevicePath, DeviceInterfaceDetailDataSize - FIELD_OFFSET(SP_DEVICE_INTERFACE_DETAIL_DATA_A, DevicePath),
                    NULL, NULL))
                {
                    ret = FALSE;
                }
            }
        }
        HeapFree(GetProcessHeap(), 0, DeviceInterfaceDetailDataW);
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiGetDeviceInterfaceDetailW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetDeviceInterfaceDetailW(
      HDEVINFO DeviceInfoSet,
      PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
      PSP_DEVICE_INTERFACE_DETAIL_DATA_W DeviceInterfaceDetailData,
      DWORD DeviceInterfaceDetailDataSize,
      PDWORD RequiredSize,
      PSP_DEVINFO_DATA DeviceInfoData)
{
    BOOL ret = FALSE;

    TRACE("%p %p %p %lu %p %p\n", DeviceInfoSet,
        DeviceInterfaceData, DeviceInterfaceDetailData,
        DeviceInterfaceDetailDataSize, RequiredSize, DeviceInfoData);

    if (!DeviceInfoSet || !DeviceInterfaceData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoSet == (HDEVINFO)INVALID_HANDLE_VALUE)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (((struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (DeviceInterfaceData->cbSize != sizeof(SP_DEVICE_INTERFACE_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (DeviceInterfaceDetailData->cbSize != sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (DeviceInfoData && DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (DeviceInterfaceDetailData == NULL && DeviceInterfaceDetailDataSize != 0)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInterfaceDetailData != NULL && DeviceInterfaceDetailDataSize < FIELD_OFFSET(SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath) + sizeof(WCHAR))
        SetLastError(ERROR_INVALID_PARAMETER);
    else
    {
        struct DeviceInterface *deviceInterface = (struct DeviceInterface *)DeviceInterfaceData->Reserved;
        LPCWSTR devName = deviceInterface->SymbolicLink;
        DWORD sizeRequired = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) +
            (lstrlenW(devName) + 1) * sizeof(WCHAR);

        if (sizeRequired > DeviceInterfaceDetailDataSize)
        {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            if (RequiredSize)
                *RequiredSize = sizeRequired;
        }
        else
        {
            wcscpy(DeviceInterfaceDetailData->DevicePath, devName);
            TRACE("DevicePath is %s\n", debugstr_w(DeviceInterfaceDetailData->DevicePath));
            if (DeviceInfoData)
            {
                memcpy(&DeviceInfoData->ClassGuid,
                    &deviceInterface->DeviceInfo->ClassGuid,
                    sizeof(GUID));
                DeviceInfoData->DevInst = deviceInterface->DeviceInfo->dnDevInst;
                DeviceInfoData->Reserved = (ULONG_PTR)deviceInterface->DeviceInfo;
            }
            ret = TRUE;
        }
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiGetDeviceRegistryPropertyA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetDeviceRegistryPropertyA(
        HDEVINFO  devinfo,
        PSP_DEVINFO_DATA  DeviceInfoData,
        DWORD   Property,
        PDWORD  PropertyRegDataType,
        PBYTE   PropertyBuffer,
        DWORD   PropertyBufferSize,
        PDWORD  RequiredSize)
{
    BOOL bResult;
    BOOL bIsStringProperty;
    DWORD RegType;
    DWORD RequiredSizeA, RequiredSizeW;
    DWORD PropertyBufferSizeW;
    PBYTE PropertyBufferW;

    TRACE("%p %p %ld %p %p %ld %p\n", devinfo, DeviceInfoData,
        Property, PropertyRegDataType, PropertyBuffer, PropertyBufferSize,
        RequiredSize);

    PropertyBufferSizeW = PropertyBufferSize * 2;
    PropertyBufferW = HeapAlloc(GetProcessHeap(), 0, PropertyBufferSizeW);

    bResult = SetupDiGetDeviceRegistryPropertyW(
        devinfo,
        DeviceInfoData,
        Property,
        &RegType,
        PropertyBufferW,
        PropertyBufferSizeW,
        &RequiredSizeW);

    if (bResult || GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        bIsStringProperty = (RegType == REG_SZ || RegType == REG_MULTI_SZ || RegType == REG_EXPAND_SZ);

        if (bIsStringProperty)
           RequiredSizeA = RequiredSizeW / sizeof(WCHAR);
        else
            RequiredSizeA = RequiredSizeW;
        if (RequiredSize)
            *RequiredSize = RequiredSizeA;
        if (PropertyRegDataType)
            *PropertyRegDataType = RegType;
    }

    if (!bResult)
    {
        HeapFree(GetProcessHeap(), 0, PropertyBufferW);
        return bResult;
    }

    if (RequiredSizeA <= PropertyBufferSize)
    {
        if (bIsStringProperty && PropertyBufferSize > 0)
        {
            if (WideCharToMultiByte(CP_ACP, 0, (LPWSTR)PropertyBufferW, RequiredSizeW / sizeof(WCHAR), (LPSTR)PropertyBuffer, PropertyBufferSize, NULL, NULL) == 0)
            {
                /* Last error is already set by WideCharToMultiByte */
                bResult = FALSE;
            }
        }
        else
            memcpy(PropertyBuffer, PropertyBufferW, RequiredSizeA);
    }
    else
    {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        bResult = FALSE;
    }

    HeapFree(GetProcessHeap(), 0, PropertyBufferW);
    return bResult;
}

/***********************************************************************
 *		SetupDiGetDeviceRegistryPropertyW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetDeviceRegistryPropertyW(
        HDEVINFO  DeviceInfoSet,
        PSP_DEVINFO_DATA  DeviceInfoData,
        DWORD   Property,
        PDWORD  PropertyRegDataType,
        PBYTE   PropertyBuffer,
        DWORD   PropertyBufferSize,
        PDWORD  RequiredSize)
{
    HKEY hEnumKey, hKey;
    DWORD rc;
    BOOL ret = FALSE;

    TRACE("%p %p %ld %p %p %ld %p\n", DeviceInfoSet, DeviceInfoData,
        Property, PropertyRegDataType, PropertyBuffer, PropertyBufferSize,
        RequiredSize);

    if (!DeviceInfoSet || DeviceInfoSet == (HDEVINFO)INVALID_HANDLE_VALUE)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (((struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (!DeviceInfoData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (Property >= SPDRP_MAXIMUM_PROPERTY)
        SetLastError(ERROR_INVALID_PARAMETER);
    else
    {
        struct DeviceInfoSet *list = (struct DeviceInfoSet *)DeviceInfoSet;
        struct DeviceInfoElement *DevInfo = (struct DeviceInfoElement *)DeviceInfoData->Reserved;

        switch (Property)
        {
            case SPDRP_CAPABILITIES:
            case SPDRP_CLASS:
            case SPDRP_CLASSGUID:
            case SPDRP_COMPATIBLEIDS:
            case SPDRP_CONFIGFLAGS:
            case SPDRP_DEVICEDESC:
            case SPDRP_DRIVER:
            case SPDRP_FRIENDLYNAME:
            case SPDRP_HARDWAREID:
            case SPDRP_LOCATION_INFORMATION:
            case SPDRP_LOWERFILTERS:
            case SPDRP_MFG:
            case SPDRP_SECURITY:
            case SPDRP_SERVICE:
            case SPDRP_UI_NUMBER:
            case SPDRP_UI_NUMBER_DESC_FORMAT:
            case SPDRP_UPPERFILTERS:
            {
                LPCWSTR RegistryPropertyName;
                DWORD BufferSize;

                switch (Property)
                {
                    case SPDRP_CAPABILITIES:
                        RegistryPropertyName = L"Capabilities"; break;
                    case SPDRP_CLASS:
                        RegistryPropertyName = L"Class"; break;
                    case SPDRP_CLASSGUID:
                        RegistryPropertyName = L"ClassGUID"; break;
                    case SPDRP_COMPATIBLEIDS:
                        RegistryPropertyName = L"CompatibleIDs"; break;
                    case SPDRP_CONFIGFLAGS:
                        RegistryPropertyName = L"ConfigFlags"; break;
                    case SPDRP_DEVICEDESC:
                        RegistryPropertyName = L"DeviceDesc"; break;
                    case SPDRP_DRIVER:
                        RegistryPropertyName = L"Driver"; break;
                    case SPDRP_FRIENDLYNAME:
                        RegistryPropertyName = L"FriendlyName"; break;
                    case SPDRP_HARDWAREID:
                        RegistryPropertyName = L"HardwareID"; break;
                    case SPDRP_LOCATION_INFORMATION:
                        RegistryPropertyName = L"LocationInformation"; break;
                    case SPDRP_LOWERFILTERS:
                        RegistryPropertyName = L"LowerFilters"; break;
                    case SPDRP_MFG:
                        RegistryPropertyName = L"Mfg"; break;
                    case SPDRP_SECURITY:
                        RegistryPropertyName = L"Security"; break;
                    case SPDRP_SERVICE:
                        RegistryPropertyName = L"Service"; break;
                    case SPDRP_UI_NUMBER:
                        RegistryPropertyName = L"UINumber"; break;
                    case SPDRP_UI_NUMBER_DESC_FORMAT:
                        RegistryPropertyName = L"UINumberDescFormat"; break;
                    case SPDRP_UPPERFILTERS:
                        RegistryPropertyName = L"UpperFilters"; break;
                    default:
                        /* Should not happen */
                        RegistryPropertyName = NULL; break;
                }

                /* Open registry key name */
                rc = RegOpenKeyExW(
                    list->HKLM,
                    EnumKeyName,
                    0, /* Options */
                    KEY_ENUMERATE_SUB_KEYS,
                    &hEnumKey);
                if (rc != ERROR_SUCCESS)
                {
                    SetLastError(rc);
                    break;
                }
                rc = RegOpenKeyExW(
                    hEnumKey,
                    DevInfo->Data,
                    0, /* Options */
                    KEY_QUERY_VALUE,
                    &hKey);
                RegCloseKey(hEnumKey);
                if (rc != ERROR_SUCCESS)
                {
                    SetLastError(rc);
                    break;
                }
                /* Read registry entry */
                BufferSize = PropertyBufferSize;
                rc = RegQueryValueExW(
                    hKey,
                    RegistryPropertyName,
                    NULL, /* Reserved */
                    PropertyRegDataType,
                    PropertyBuffer,
                    &BufferSize);
                if (RequiredSize)
                    *RequiredSize = BufferSize;
                switch(rc) {
                    case ERROR_SUCCESS:
                        if (PropertyBuffer != NULL || BufferSize == 0)
                            ret = TRUE;
                        else
                            SetLastError(ERROR_INSUFFICIENT_BUFFER);
                        break;
                    case ERROR_MORE_DATA:
                        SetLastError(ERROR_INSUFFICIENT_BUFFER);
                        break;
                    default:
                        SetLastError(rc);
                }
                RegCloseKey(hKey);
                break;
            }

            case SPDRP_PHYSICAL_DEVICE_OBJECT_NAME:
            {
                DWORD required = (wcslen(DevInfo->Data) + 1) * sizeof(WCHAR);

                if (PropertyRegDataType)
                    *PropertyRegDataType = REG_SZ;
                if (RequiredSize)
                    *RequiredSize = required;
                if (PropertyBufferSize >= required)
                {
                    wcscpy((LPWSTR)PropertyBuffer, DevInfo->Data);
                    ret = TRUE;
                }
                else
                    SetLastError(ERROR_INSUFFICIENT_BUFFER);
                break;
            }

            /*case SPDRP_BUSTYPEGUID:
            case SPDRP_LEGACYBUSTYPE:
            case SPDRP_BUSNUMBER:
            case SPDRP_ENUMERATOR_NAME:
            case SPDRP_SECURITY_SDS:
            case SPDRP_DEVTYPE:
            case SPDRP_EXCLUSIVE:
            case SPDRP_CHARACTERISTICS:
            case SPDRP_ADDRESS:
            case SPDRP_DEVICE_POWER_DATA:*/
#if (WINVER >= 0x501)
            /*case SPDRP_REMOVAL_POLICY:
            case SPDRP_REMOVAL_POLICY_HW_DEFAULT:
            case SPDRP_REMOVAL_POLICY_OVERRIDE:
            case SPDRP_INSTALL_STATE:*/
#endif

            default:
            {
                ERR("Property 0x%lx not implemented\n", Property);
                SetLastError(ERROR_NOT_SUPPORTED);
            }
        }
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiSetDeviceRegistryPropertyA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiSetDeviceRegistryPropertyA(
        IN HDEVINFO DeviceInfoSet,
        IN OUT PSP_DEVINFO_DATA DeviceInfoData,
        IN DWORD Property,
        IN CONST BYTE *PropertyBuffer,
        IN DWORD PropertyBufferSize)
{
    FIXME("%p %p 0x%lx %p 0x%lx\n", DeviceInfoSet, DeviceInfoData,
        Property, PropertyBuffer, PropertyBufferSize);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

/***********************************************************************
 *		SetupDiSetDeviceRegistryPropertyW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiSetDeviceRegistryPropertyW(
        IN HDEVINFO DeviceInfoSet,
        IN OUT PSP_DEVINFO_DATA DeviceInfoData,
        IN DWORD Property,
        IN const BYTE *PropertyBuffer,
        IN DWORD PropertyBufferSize)
{
    struct DeviceInfoSet *list;
    BOOL ret = FALSE;

    TRACE("%p %p 0x%lx %p 0x%lx\n", DeviceInfoSet, DeviceInfoData,
        Property, PropertyBuffer, PropertyBufferSize);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_HANDLE);
    else if ((list = (struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (DeviceInfoData)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        switch (Property)
        {
            case SPDRP_COMPATIBLEIDS:
            case SPDRP_CONFIGFLAGS:
            case SPDRP_FRIENDLYNAME:
            case SPDRP_HARDWAREID:
            case SPDRP_LOCATION_INFORMATION:
            case SPDRP_LOWERFILTERS:
            case SPDRP_SECURITY:
            case SPDRP_SERVICE:
            case SPDRP_UI_NUMBER_DESC_FORMAT:
            case SPDRP_UPPERFILTERS:
            {
                LPCWSTR RegistryPropertyName;
                DWORD RegistryDataType;
                HKEY hKey;
                LONG rc;

                switch (Property)
                {
                    case SPDRP_COMPATIBLEIDS:
                        RegistryPropertyName = L"CompatibleIDs";
                        RegistryDataType = REG_MULTI_SZ;
                        break;
                    case SPDRP_CONFIGFLAGS:
                        RegistryPropertyName = L"ConfigFlags";
                        RegistryDataType = REG_DWORD;
                        break;
                    case SPDRP_FRIENDLYNAME:
                        RegistryPropertyName = L"FriendlyName";
                        RegistryDataType = REG_SZ;
                        break;
                    case SPDRP_HARDWAREID:
                        RegistryPropertyName = L"HardwareID";
                        RegistryDataType = REG_MULTI_SZ;
                        break;
                    case SPDRP_LOCATION_INFORMATION:
                        RegistryPropertyName = L"LocationInformation";
                        RegistryDataType = REG_SZ;
                        break;
                    case SPDRP_LOWERFILTERS:
                        RegistryPropertyName = L"LowerFilters";
                        RegistryDataType = REG_MULTI_SZ;
                        break;
                    case SPDRP_SECURITY:
                        RegistryPropertyName = L"Security";
                        RegistryDataType = REG_BINARY;
                        break;
                    case SPDRP_SERVICE:
                        RegistryPropertyName = L"Service";
                        RegistryDataType = REG_SZ;
                        break;
                    case SPDRP_UI_NUMBER_DESC_FORMAT:
                        RegistryPropertyName = L"UINumberDescFormat";
                        RegistryDataType = REG_SZ;
                        break;
                    case SPDRP_UPPERFILTERS:
                        RegistryPropertyName = L"UpperFilters";
                        RegistryDataType = REG_MULTI_SZ;
                        break;
                    default:
                        /* Should not happen */
                        RegistryPropertyName = NULL;
                        RegistryDataType = REG_BINARY;
                        break;
                }
                /* Open device registry key */
                hKey = SetupDiOpenDevRegKey(DeviceInfoSet, DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_SET_VALUE);
                if (hKey != INVALID_HANDLE_VALUE)
                {
                    /* Write new data */
                    rc = RegSetValueExW(
                        hKey,
                        RegistryPropertyName,
                        0, /* Reserved */
                        RegistryDataType,
                        PropertyBuffer,
                        PropertyBufferSize);
                    if (rc == ERROR_SUCCESS)
                        ret = TRUE;
                    else
                        SetLastError(rc);
                    RegCloseKey(hKey);
                }
                break;
            }

            /*case SPDRP_CHARACTERISTICS:
            case SPDRP_DEVTYPE:
            case SPDRP_EXCLUSIVE:*/
#if (WINVER >= 0x501)
            //case SPDRP_REMOVAL_POLICY_OVERRIDE:
#endif
            //case SPDRP_SECURITY_SDS:

            default:
            {
                ERR("Property 0x%lx not implemented\n", Property);
                SetLastError(ERROR_NOT_SUPPORTED);
            }
        }
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiInstallClassA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiInstallClassA(
        HWND hwndParent,
        PCSTR InfFileName,
        DWORD Flags,
        HSPFILEQ FileQueue)
{
    UNICODE_STRING FileNameW;
    BOOL Result;

    if (!RtlCreateUnicodeStringFromAsciiz(&FileNameW, InfFileName))
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    Result = SetupDiInstallClassW(hwndParent, FileNameW.Buffer, Flags, FileQueue);

    RtlFreeUnicodeString(&FileNameW);

    return Result;
}

static HKEY CreateClassKey(HINF hInf)
{
    WCHAR FullBuffer[MAX_PATH];
    WCHAR Buffer[MAX_PATH];
    DWORD RequiredSize;
    HKEY hClassKey;

    Buffer[0] = '\\';
    if (!SetupGetLineTextW(NULL,
			   hInf,
			   Version,
			   ClassGUID,
			   &Buffer[1],
			   MAX_PATH - 1,
			   &RequiredSize))
    {
        return INVALID_HANDLE_VALUE;
    }

    lstrcpyW(FullBuffer, ControlClass);
    lstrcatW(FullBuffer, Buffer);


    if (!SetupGetLineTextW(NULL,
			       hInf,
			       Version,
			       Class,
			       Buffer,
			       MAX_PATH,
			       &RequiredSize))
    {
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, FullBuffer);
        return INVALID_HANDLE_VALUE;
    }

    if (ERROR_SUCCESS != RegCreateKeyExW(HKEY_LOCAL_MACHINE,
			    FullBuffer,
			    0,
			    NULL,
			    REG_OPTION_NON_VOLATILE,
			    KEY_SET_VALUE,
			    NULL,
			    &hClassKey,
             NULL))
    {
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, FullBuffer);
        return INVALID_HANDLE_VALUE;
    }

    if (ERROR_SUCCESS != RegSetValueExW(hClassKey,
		       Class,
		       0,
		       REG_SZ,
		       (LPBYTE)Buffer,
             RequiredSize * sizeof(WCHAR)))
    {
        RegCloseKey(hClassKey);
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, FullBuffer);
        return INVALID_HANDLE_VALUE;
    }

    return hClassKey;
}

/***********************************************************************
 *		SetupDiInstallClassW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiInstallClassW(
        HWND hwndParent,
        PCWSTR InfFileName,
        DWORD Flags,
        HSPFILEQ FileQueue)
{
    WCHAR SectionName[MAX_PATH];
    DWORD SectionNameLength = 0;
    HINF hInf;
    BOOL bFileQueueCreated = FALSE;
    HKEY hClassKey;

    TRACE("%p %s 0x%lx %p\n", hwndParent, debugstr_w(InfFileName),
        Flags, FileQueue);

    FIXME("not fully implemented\n");

    if ((Flags & DI_NOVCP) && (FileQueue == NULL || FileQueue == INVALID_HANDLE_VALUE))
    {
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }

    /* Open the .inf file */
    hInf = SetupOpenInfFileW(InfFileName,
			     NULL,
			     INF_STYLE_WIN4,
			     NULL);
    if (hInf == INVALID_HANDLE_VALUE)
    {

	return FALSE;
    }

    /* Create or open the class registry key 'HKLM\\CurrentControlSet\\Class\\{GUID}' */
    hClassKey = CreateClassKey(hInf);
    if (hClassKey == INVALID_HANDLE_VALUE)
    {
	SetupCloseInfFile(hInf);
	return FALSE;
    }



    /* Try to append a layout file */
#if 0
    SetupOpenAppendInfFileW(NULL, hInf, NULL);
#endif

    /* Retrieve the actual section name */
    SetupDiGetActualSectionToInstallW(hInf,
				      ClassInstall32,
				      SectionName,
				      MAX_PATH,
				      &SectionNameLength,
				      NULL);

#if 0
    if (!(Flags & DI_NOVCP))
    {
	FileQueue = SetupOpenFileQueue();
	if (FileQueue == INVALID_HANDLE_VALUE)
	{
	    SetupCloseInfFile(hInf);
       RegCloseKey(hClassKey);
	    return FALSE;
	}

	bFileQueueCreated = TRUE;

    }
#endif

    SetupInstallFromInfSectionW(NULL,
				hInf,
				SectionName,
				SPINST_REGISTRY,
				hClassKey,
				NULL,
				0,
				NULL,
				NULL,
				INVALID_HANDLE_VALUE,
				NULL);

    /* FIXME: More code! */

    if (bFileQueueCreated)
	SetupCloseFileQueue(FileQueue);

    SetupCloseInfFile(hInf);

    RegCloseKey(hClassKey);
    return TRUE;
}


/***********************************************************************
 *		SetupDiOpenClassRegKey  (SETUPAPI.@)
 */
HKEY WINAPI SetupDiOpenClassRegKey(
        const GUID* ClassGuid,
        REGSAM samDesired)
{
    return SetupDiOpenClassRegKeyExW(ClassGuid, samDesired,
                                     DIOCR_INSTALLER, NULL, NULL);
}


/***********************************************************************
 *		SetupDiOpenClassRegKeyExA  (SETUPAPI.@)
 */
HKEY WINAPI SetupDiOpenClassRegKeyExA(
        const GUID* ClassGuid OPTIONAL,
        REGSAM samDesired,
        DWORD Flags,
        PCSTR MachineName OPTIONAL,
        PVOID Reserved)
{
    PWSTR MachineNameW = NULL;
    HKEY hKey;

    TRACE("\n");

    if (MachineName)
    {
        MachineNameW = MultiByteToUnicode(MachineName, CP_ACP);
        if (MachineNameW == NULL)
            return INVALID_HANDLE_VALUE;
    }

    hKey = SetupDiOpenClassRegKeyExW(ClassGuid, samDesired,
                                     Flags, MachineNameW, Reserved);

    if (MachineNameW)
        MyFree(MachineNameW);

    return hKey;
}


/***********************************************************************
 *		SetupDiOpenClassRegKeyExW  (SETUPAPI.@)
 */
HKEY WINAPI SetupDiOpenClassRegKeyExW(
        const GUID* ClassGuid OPTIONAL,
        REGSAM samDesired,
        DWORD Flags,
        PCWSTR MachineName OPTIONAL,
        PVOID Reserved)
{
    LPWSTR lpGuidString;
    LPWSTR lpFullGuidString;
    DWORD dwLength;
    HKEY HKLM;
    HKEY hClassesKey;
    HKEY hClassKey;
    DWORD rc;
    LPCWSTR lpKeyName;

    TRACE("%s 0x%lx 0x%lx %s %p\n", debugstr_guid(ClassGuid), samDesired,
        Flags, debugstr_w(MachineName), Reserved);

    if (Flags == DIOCR_INSTALLER)
    {
        lpKeyName = ControlClass;
    }
    else if (Flags == DIOCR_INTERFACE)
    {
        lpKeyName = DeviceClasses;
    }
    else
    {
        ERR("Invalid Flags parameter!\n");
        SetLastError(ERROR_INVALID_FLAGS);
        return INVALID_HANDLE_VALUE;
    }

    if (MachineName != NULL)
    {
        rc = RegConnectRegistryW(MachineName, HKEY_LOCAL_MACHINE, &HKLM);
        if (rc != ERROR_SUCCESS)
        {
            SetLastError(rc);
            return INVALID_HANDLE_VALUE;
        }
    }
    else
        HKLM = HKEY_LOCAL_MACHINE;

    rc = RegOpenKeyExW(HKLM,
		      lpKeyName,
		      0,
		      ClassGuid ? KEY_ENUMERATE_SUB_KEYS : samDesired,
		      &hClassesKey);
    if (MachineName != NULL) RegCloseKey(HKLM);
    if (rc != ERROR_SUCCESS)
    {
	SetLastError(rc);
	return INVALID_HANDLE_VALUE;
    }

    if (ClassGuid == NULL)
        return hClassesKey;

    if (UuidToStringW((UUID*)ClassGuid, &lpGuidString) != RPC_S_OK)
    {
	SetLastError(ERROR_GEN_FAILURE);
	RegCloseKey(hClassesKey);
	return INVALID_HANDLE_VALUE;
    }

    dwLength = lstrlenW(lpGuidString);
    lpFullGuidString = HeapAlloc(GetProcessHeap(), 0, (dwLength + 3) * sizeof(WCHAR));
    if (!lpFullGuidString)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        RpcStringFreeW(&lpGuidString);
        return INVALID_HANDLE_VALUE;
    }
    lpFullGuidString[0] = '{';
    memcpy(&lpFullGuidString[1], lpGuidString, dwLength * sizeof(WCHAR));
    lpFullGuidString[dwLength + 1] = '}';
    lpFullGuidString[dwLength + 2] = '\0';
    RpcStringFreeW(&lpGuidString);

    rc = RegOpenKeyExW(hClassesKey,
		      lpFullGuidString,
		      0,
		      samDesired,
		      &hClassKey);
    if (rc != ERROR_SUCCESS)
    {
	SetLastError(rc);
	HeapFree(GetProcessHeap(), 0, lpFullGuidString);
	RegCloseKey(hClassesKey);
	return INVALID_HANDLE_VALUE;
    }

    HeapFree(GetProcessHeap(), 0, lpFullGuidString);
    RegCloseKey(hClassesKey);

    return hClassKey;
}

/***********************************************************************
 *		SetupDiOpenDeviceInterfaceW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiOpenDeviceInterfaceW(
       HDEVINFO DeviceInfoSet,
       PCWSTR DevicePath,
       DWORD OpenFlags,
       PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData)
{
    FIXME("%p %s %08lx %p\n",
        DeviceInfoSet, debugstr_w(DevicePath), OpenFlags, DeviceInterfaceData);
    return FALSE;
}

/***********************************************************************
 *		SetupDiOpenDeviceInterfaceA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiOpenDeviceInterfaceA(
       HDEVINFO DeviceInfoSet,
       PCSTR DevicePath,
       DWORD OpenFlags,
       PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData)
{
    LPWSTR DevicePathW = NULL;
    BOOL bResult;

    TRACE("%p %s %08lx %p\n", DeviceInfoSet, debugstr_a(DevicePath), OpenFlags, DeviceInterfaceData);

    DevicePathW = MultiByteToUnicode(DevicePath, CP_ACP);
    if (DevicePathW == NULL)
        return FALSE;

    bResult = SetupDiOpenDeviceInterfaceW(DeviceInfoSet,
        DevicePathW, OpenFlags, DeviceInterfaceData);

    MyFree(DevicePathW);

    return bResult;
}

/***********************************************************************
 *		SetupDiSetClassInstallParamsA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiSetClassInstallParamsA(
       HDEVINFO  DeviceInfoSet,
       PSP_DEVINFO_DATA DeviceInfoData,
       PSP_CLASSINSTALL_HEADER ClassInstallParams,
       DWORD ClassInstallParamsSize)
{
    FIXME("%p %p %x %lu\n",DeviceInfoSet, DeviceInfoData,
          ClassInstallParams->InstallFunction, ClassInstallParamsSize);
    return FALSE;
}

/***********************************************************************
 *		SetupDiSetClassInstallParamsW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiSetClassInstallParamsW(
       HDEVINFO  DeviceInfoSet,
       PSP_DEVINFO_DATA DeviceInfoData,
       PSP_CLASSINSTALL_HEADER ClassInstallParams,
       DWORD ClassInstallParamsSize)
{
    FIXME("%p %p %x %lu\n",DeviceInfoSet, DeviceInfoData,
          ClassInstallParams->InstallFunction, ClassInstallParamsSize);
    return FALSE;
}

static DWORD
GetFunctionPointer(
        IN PWSTR InstallerName,
        OUT HMODULE* ModulePointer,
        OUT PVOID* FunctionPointer)
{
    HMODULE hModule = NULL;
    LPSTR FunctionNameA = NULL;
    PWCHAR Comma;
    DWORD rc;

    *ModulePointer = NULL;
    *FunctionPointer = NULL;

    Comma = strchrW(InstallerName, ',');
    if (!Comma)
    {
        rc = ERROR_INVALID_PARAMETER;
        goto cleanup;
    }

    /* Load library */
    *Comma = '\0';
    hModule = LoadLibraryW(InstallerName);
    *Comma = ',';
    if (!hModule)
    {
        rc = GetLastError();
        goto cleanup;
    }

    /* Skip comma spaces */
    while (*Comma == ',' || isspaceW(*Comma))
        Comma++;

    /* W->A conversion for function name */
    FunctionNameA = UnicodeToMultiByte(Comma, CP_ACP);
    if (!FunctionNameA)
    {
        rc = GetLastError();
        goto cleanup;
    }

    /* Search function */
    *FunctionPointer = GetProcAddress(hModule, FunctionNameA);
    if (!*FunctionPointer)
    {
        rc = GetLastError();
        goto cleanup;
    }

    *ModulePointer = hModule;
    rc = ERROR_SUCCESS;

cleanup:
    if (rc != ERROR_SUCCESS && hModule)
        FreeLibrary(hModule);
    MyFree(FunctionNameA);
    return rc;
}

static DWORD
FreeFunctionPointer(
        IN HMODULE ModulePointer,
        IN PVOID FunctionPointer)
{
    if (ModulePointer == NULL)
        return ERROR_SUCCESS;
    if (FreeLibrary(ModulePointer))
       return ERROR_SUCCESS;
    else
       return GetLastError();
}

/***********************************************************************
 *		SetupDiCallClassInstaller (SETUPAPI.@)
 */
BOOL WINAPI SetupDiCallClassInstaller(
       IN DI_FUNCTION InstallFunction,
       IN HDEVINFO DeviceInfoSet,
       IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL)
{
    BOOL ret = FALSE;

    TRACE("%u %p %p\n", InstallFunction, DeviceInfoSet, DeviceInfoData);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoSet == (HDEVINFO)INVALID_HANDLE_VALUE)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (((struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (((struct DeviceInfoSet *)DeviceInfoSet)->HKLM != HKEY_LOCAL_MACHINE)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (DeviceInfoData && DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        SP_DEVINSTALL_PARAMS_W InstallParams;
#define CLASS_COINSTALLER  0x1
#define DEVICE_COINSTALLER 0x2
#define CLASS_INSTALLER    0x4
        UCHAR CanHandle = 0;
        DEFAULT_CLASS_INSTALL_PROC DefaultHandler = NULL;

        switch (InstallFunction)
        {
            case DIF_ALLOW_INSTALL:
                CanHandle = CLASS_COINSTALLER | CLASS_INSTALLER;
                break;
            case DIF_DESTROYPRIVATEDATA:
                CanHandle = CLASS_INSTALLER;
                break;
            case DIF_INSTALLDEVICE:
                CanHandle = CLASS_COINSTALLER | DEVICE_COINSTALLER | CLASS_INSTALLER;
                DefaultHandler = SetupDiInstallDevice;
                break;
            case DIF_INSTALLDEVICEFILES:
                CanHandle = CLASS_COINSTALLER | CLASS_INSTALLER;
                DefaultHandler = SetupDiInstallDriverFiles;
                break;
            case DIF_INSTALLINTERFACES:
                CanHandle = CLASS_COINSTALLER | DEVICE_COINSTALLER | CLASS_INSTALLER;
                DefaultHandler = SetupDiInstallDeviceInterfaces;
                break;
            case DIF_NEWDEVICEWIZARD_FINISHINSTALL:
                CanHandle = CLASS_COINSTALLER | DEVICE_COINSTALLER | CLASS_INSTALLER;
                break;
            case DIF_NEWDEVICEWIZARD_POSTANALYZE:
                CanHandle = CLASS_COINSTALLER | CLASS_INSTALLER;
                break;
            case DIF_NEWDEVICEWIZARD_PREANALYZE:
                CanHandle = CLASS_COINSTALLER | CLASS_INSTALLER;
                break;
            case DIF_REGISTER_COINSTALLERS:
                CanHandle = CLASS_COINSTALLER | CLASS_INSTALLER;
                DefaultHandler = SetupDiRegisterCoDeviceInstallers;
                break;
            case DIF_SELECTBESTCOMPATDRV:
                CanHandle = CLASS_COINSTALLER | CLASS_INSTALLER;
                DefaultHandler = SetupDiSelectBestCompatDrv;
                break;
            default:
                ERR("Install function %u not supported\n", InstallFunction);
                SetLastError(ERROR_NOT_SUPPORTED);
        }

        InstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS_W);
        if (!SetupDiGetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &InstallParams))
            /* Don't process this call, as a parameter is invalid */
            CanHandle = 0;

        if (CanHandle != 0)
        {
            LIST_ENTRY ClassCoInstallersListHead;
            LIST_ENTRY DeviceCoInstallersListHead;
            HMODULE ClassInstallerLibrary = NULL;
            CLASS_INSTALL_PROC ClassInstaller = NULL;
            COINSTALLER_CONTEXT_DATA Context;
            PLIST_ENTRY ListEntry;
            HKEY hKey;
            DWORD dwRegType, dwLength;
            DWORD rc = NO_ERROR;

            InitializeListHead(&ClassCoInstallersListHead);
            InitializeListHead(&DeviceCoInstallersListHead);

            if (CanHandle & DEVICE_COINSTALLER)
            {
                hKey = SetupDiOpenDevRegKey(DeviceInfoSet, DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_QUERY_VALUE);
                if (hKey != INVALID_HANDLE_VALUE)
                {
                    rc = RegQueryValueExW(hKey, L"CoInstallers32", NULL, &dwRegType, NULL, &dwLength);
                    if (rc == ERROR_SUCCESS && dwRegType == REG_MULTI_SZ)
                    {
                        LPWSTR KeyBuffer = HeapAlloc(GetProcessHeap(), 0, dwLength);
                        if (KeyBuffer != NULL)
                        {
                            rc = RegQueryValueExW(hKey, L"CoInstallers32", NULL, NULL, (LPBYTE)KeyBuffer, &dwLength);
                            if (rc == ERROR_SUCCESS)
                            {
                                LPWSTR ptr;
                                for (ptr = KeyBuffer; *ptr; ptr += strlenW(ptr) + 1)
                                {
                                    /* Add coinstaller to DeviceCoInstallersListHead list */
                                    struct CoInstallerElement *coinstaller;
                                    TRACE("Got device coinstaller '%s'\n", debugstr_w(ptr));
                                    coinstaller = HeapAlloc(GetProcessHeap(), 0, sizeof(struct CoInstallerElement));
                                    if (!coinstaller)
                                        continue;
                                    memset(coinstaller, 0, sizeof(struct CoInstallerElement));
                                    if (GetFunctionPointer(ptr, &coinstaller->Module, (PVOID*)&coinstaller->Function) == ERROR_SUCCESS)
                                        InsertTailList(&DeviceCoInstallersListHead, &coinstaller->ListEntry);
                                    else
                                        HeapFree(GetProcessHeap(), 0, coinstaller);
                                }
                            }
                            HeapFree(GetProcessHeap(), 0, KeyBuffer);
                        }
                    }
                    RegCloseKey(hKey);
                }
            }
            if (CanHandle & CLASS_COINSTALLER)
            {
                rc = RegOpenKeyEx(
                    HKEY_LOCAL_MACHINE,
                    L"SYSTEM\\CurrentControlSet\\Control\\CoDeviceInstallers",
                    0, /* Options */
                    KEY_QUERY_VALUE,
                    &hKey);
                if (rc == ERROR_SUCCESS)
                {
                    LPWSTR lpGuidString;
                    if (UuidToStringW((UUID*)&DeviceInfoData->ClassGuid, &lpGuidString) == RPC_S_OK)
                    {
                        rc = RegQueryValueExW(hKey, lpGuidString, NULL, &dwRegType, NULL, &dwLength);
                        if (rc == ERROR_SUCCESS && dwRegType == REG_MULTI_SZ)
                        {
                            LPWSTR KeyBuffer = HeapAlloc(GetProcessHeap(), 0, dwLength);
                            if (KeyBuffer != NULL)
                            {
                                rc = RegQueryValueExW(hKey, lpGuidString, NULL, NULL, (LPBYTE)KeyBuffer, &dwLength);
                                if (rc == ERROR_SUCCESS)
                                {
                                    LPWSTR ptr;
                                    for (ptr = KeyBuffer; *ptr; ptr += strlenW(ptr) + 1)
                                    {
                                        /* Add coinstaller to ClassCoInstallersListHead list */
                                        struct CoInstallerElement *coinstaller;
                                        TRACE("Got class coinstaller '%s'\n", debugstr_w(ptr));
                                        coinstaller = HeapAlloc(GetProcessHeap(), 0, sizeof(struct CoInstallerElement));
                                        if (!coinstaller)
                                            continue;
                                        memset(coinstaller, 0, sizeof(struct CoInstallerElement));
                                        if (GetFunctionPointer(ptr, &coinstaller->Module, (PVOID*)&coinstaller->Function) == ERROR_SUCCESS)
                                            InsertTailList(&ClassCoInstallersListHead, &coinstaller->ListEntry);
                                        else
                                            HeapFree(GetProcessHeap(), 0, coinstaller);
                                    }
                                }
                                HeapFree(GetProcessHeap(), 0, KeyBuffer);
                            }
                        }
                        RpcStringFreeW(&lpGuidString);
                    }
                    RegCloseKey(hKey);
                }
            }
            if ((CanHandle & CLASS_INSTALLER) && !(InstallParams.FlagsEx & DI_FLAGSEX_CI_FAILED))
            {
                hKey = SetupDiOpenClassRegKey(&DeviceInfoData->ClassGuid, KEY_QUERY_VALUE);
                if (hKey != INVALID_HANDLE_VALUE)
                {
                    rc = RegQueryValueExW(hKey, L"Installer32", NULL, &dwRegType, NULL, &dwLength);
                    if (rc == ERROR_SUCCESS && dwRegType == REG_SZ)
                    {
                        LPWSTR KeyBuffer = HeapAlloc(GetProcessHeap(), 0, dwLength);
                        if (KeyBuffer != NULL)
                        {
                            rc = RegQueryValueExW(hKey, L"Installer32", NULL, NULL, (LPBYTE)KeyBuffer, &dwLength);
                            if (rc == ERROR_SUCCESS)
                            {
                                /* Get ClassInstaller function pointer */
                                TRACE("Got class installer '%s'\n", debugstr_w(KeyBuffer));
                                if (GetFunctionPointer(KeyBuffer, &ClassInstallerLibrary, (PVOID*)&ClassInstaller) != ERROR_SUCCESS)
                                {
                                    InstallParams.FlagsEx |= DI_FLAGSEX_CI_FAILED;
                                    SetupDiSetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &InstallParams);
                                }
                            }
                            HeapFree(GetProcessHeap(), 0, KeyBuffer);
                        }
                    }
                    RegCloseKey(hKey);
                }
            }

            /* Call Class co-installers */
            Context.PostProcessing = FALSE;
            rc = NO_ERROR;
            ListEntry = ClassCoInstallersListHead.Flink;
            while (rc == NO_ERROR && ListEntry != &ClassCoInstallersListHead)
            {
                struct CoInstallerElement *coinstaller = (struct CoInstallerElement *)ListEntry;
                rc = (*coinstaller->Function)(InstallFunction, DeviceInfoSet, DeviceInfoData, &Context);
                coinstaller->PrivateData = Context.PrivateData;
                if (rc == ERROR_DI_POSTPROCESSING_REQUIRED)
                {
                    coinstaller->DoPostProcessing = TRUE;
                    rc = NO_ERROR;
                }
                ListEntry = ListEntry->Flink;
            }

            /* Call Device co-installers */
            ListEntry = DeviceCoInstallersListHead.Flink;
            while (rc == NO_ERROR && ListEntry != &DeviceCoInstallersListHead)
            {
                struct CoInstallerElement *coinstaller = (struct CoInstallerElement *)ListEntry;
                rc = (*coinstaller->Function)(InstallFunction, DeviceInfoSet, DeviceInfoData, &Context);
                coinstaller->PrivateData = Context.PrivateData;
                if (rc == ERROR_DI_POSTPROCESSING_REQUIRED)
                {
                    coinstaller->DoPostProcessing = TRUE;
                    rc = NO_ERROR;
                }
                ListEntry = ListEntry->Flink;
            }

            /* Call Class installer */
            if (ClassInstaller)
            {
                rc = (*ClassInstaller)(InstallFunction, DeviceInfoSet, DeviceInfoData);
                FreeFunctionPointer(ClassInstallerLibrary, ClassInstaller);
            }
            else
                rc = ERROR_DI_DO_DEFAULT;

            /* Call default handler */
            if (rc == ERROR_DI_DO_DEFAULT)
            {
                if (DefaultHandler && !(InstallParams.Flags & DI_NODI_DEFAULTACTION))
                {
                    if ((*DefaultHandler)(DeviceInfoSet, DeviceInfoData))
                        rc = NO_ERROR;
                    else
                        rc = GetLastError();
                }
                else
                    rc = NO_ERROR;
            }

            /* Call Class co-installers that required postprocessing */
            Context.PostProcessing = TRUE;
            ListEntry = ClassCoInstallersListHead.Flink;
            while (ListEntry != &ClassCoInstallersListHead)
            {
                struct CoInstallerElement *coinstaller = (struct CoInstallerElement *)ListEntry;
                if (coinstaller->DoPostProcessing)
                {
                    Context.InstallResult = rc;
                    Context.PrivateData = coinstaller->PrivateData;
                    rc = (*coinstaller->Function)(InstallFunction, DeviceInfoSet, DeviceInfoData, &Context);
                }
                FreeFunctionPointer(coinstaller->Module, coinstaller->Function);
                ListEntry = ListEntry->Flink;
            }

            /* Call Device co-installers that required postprocessing */
            ListEntry = DeviceCoInstallersListHead.Flink;
            while (ListEntry != &DeviceCoInstallersListHead)
            {
                struct CoInstallerElement *coinstaller = (struct CoInstallerElement *)ListEntry;
                if (coinstaller->DoPostProcessing)
                {
                    Context.InstallResult = rc;
                    Context.PrivateData = coinstaller->PrivateData;
                    rc = (*coinstaller->Function)(InstallFunction, DeviceInfoSet, DeviceInfoData, &Context);
                }
                FreeFunctionPointer(coinstaller->Module, coinstaller->Function);
                ListEntry = ListEntry->Flink;
            }

            /* Free allocated memory */
            while (!IsListEmpty(&ClassCoInstallersListHead))
            {
                ListEntry = RemoveHeadList(&ClassCoInstallersListHead);
                HeapFree(GetProcessHeap(), 0, ListEntry);
            }
            while (!IsListEmpty(&DeviceCoInstallersListHead))
            {
                ListEntry = RemoveHeadList(&DeviceCoInstallersListHead);
                HeapFree(GetProcessHeap(), 0, ListEntry);
            }

            ret = (rc == NO_ERROR);
        }
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiGetDeviceInfoListDetailW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetDeviceInfoListDetailW(
        IN HDEVINFO DeviceInfoSet,
        OUT PSP_DEVINFO_LIST_DETAIL_DATA_W DeviceInfoListDetailData)
{
    struct DeviceInfoSet *list;
    BOOL ret = FALSE;

    TRACE("%p %p\n", DeviceInfoSet, DeviceInfoListDetailData);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_HANDLE);
    else if ((list = (struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (!DeviceInfoListDetailData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoListDetailData->cbSize != sizeof(SP_DEVINFO_LIST_DETAIL_DATA_W))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        memcpy(
            &DeviceInfoListDetailData->ClassGuid,
            &list->ClassGuid,
            sizeof(GUID));
        DeviceInfoListDetailData->RemoteMachineHandle = list->hMachine;
        if (list->MachineName)
            strcpyW(DeviceInfoListDetailData->RemoteMachineName, list->MachineName + 2);
        else
            DeviceInfoListDetailData->RemoteMachineName[0] = 0;

        ret = TRUE;
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiGetDeviceInstallParamsA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetDeviceInstallParamsA(
       IN HDEVINFO DeviceInfoSet,
       IN PSP_DEVINFO_DATA DeviceInfoData,
       OUT PSP_DEVINSTALL_PARAMS_A DeviceInstallParams)
{
    SP_DEVINSTALL_PARAMS_W deviceInstallParamsW;
    BOOL ret = FALSE;

    TRACE("%p %p %p\n", DeviceInfoSet, DeviceInfoData, DeviceInstallParams);

    if (DeviceInstallParams == NULL)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInstallParams->cbSize != sizeof(SP_DEVINSTALL_PARAMS_A))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        deviceInstallParamsW.cbSize = sizeof(SP_DEVINSTALL_PARAMS_W);
        ret = SetupDiGetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &deviceInstallParamsW);

        if (ret)
        {
            /* Do W->A conversion */
            memcpy(
                DeviceInstallParams,
                &deviceInstallParamsW,
                FIELD_OFFSET(SP_DEVINSTALL_PARAMS_W, DriverPath));
            if (WideCharToMultiByte(CP_ACP, 0, deviceInstallParamsW.DriverPath, -1,
                DeviceInstallParams->DriverPath, MAX_PATH, NULL, NULL) == 0)
            {
                DeviceInstallParams->DriverPath[0] = '\0';
                ret = FALSE;
            }
        }
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiGetDeviceInstallParamsW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetDeviceInstallParamsW(
       IN HDEVINFO DeviceInfoSet,
       IN PSP_DEVINFO_DATA DeviceInfoData,
       OUT PSP_DEVINSTALL_PARAMS_W DeviceInstallParams)
{
    struct DeviceInfoSet *list;
    BOOL ret = FALSE;

    TRACE("%p %p %p\n", DeviceInfoSet, DeviceInfoData, DeviceInstallParams);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_HANDLE);
    else if ((list = (struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (DeviceInfoData && DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (!DeviceInstallParams)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInstallParams->cbSize != sizeof(SP_DEVINSTALL_PARAMS_W))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        PSP_DEVINSTALL_PARAMS_W Source;

        if (DeviceInfoData)
            Source = &((struct DeviceInfoElement *)DeviceInfoData->Reserved)->InstallParams;
        else
            Source = &list->InstallParams;
        memcpy(DeviceInstallParams, Source, Source->cbSize);
        ret = TRUE;
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiSetDeviceInstallParamsW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiSetDeviceInstallParamsW(
       IN HDEVINFO DeviceInfoSet,
       IN PSP_DEVINFO_DATA DeviceInfoData,
       IN PSP_DEVINSTALL_PARAMS_W DeviceInstallParams)
{
    struct DeviceInfoSet *list;
    BOOL ret = FALSE;

    TRACE("%p %p %p\n", DeviceInfoSet, DeviceInfoData, DeviceInstallParams);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_HANDLE);
    else if ((list = (struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (DeviceInfoData && DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (!DeviceInstallParams)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInstallParams->cbSize != sizeof(SP_DEVINSTALL_PARAMS_W))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        PSP_DEVINSTALL_PARAMS_W Destination;

        /* FIXME: Validate parameters */

        if (DeviceInfoData)
            Destination = &((struct DeviceInfoElement *)DeviceInfoData->Reserved)->InstallParams;
        else
            Destination = &list->InstallParams;
        memcpy(Destination, DeviceInstallParams, DeviceInstallParams->cbSize);
        ret = TRUE;
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiGetDeviceInstanceIdA(SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetDeviceInstanceIdA(
        IN HDEVINFO DeviceInfoSet,
        IN PSP_DEVINFO_DATA DeviceInfoData,
        OUT PSTR DeviceInstanceId OPTIONAL,
        IN DWORD DeviceInstanceIdSize,
        OUT PDWORD RequiredSize OPTIONAL)
{
    PWSTR DeviceInstanceIdW = NULL;
    BOOL ret = FALSE;

    TRACE("%p %p %p %lu %p\n", DeviceInfoSet, DeviceInfoData,
          DeviceInstanceId, DeviceInstanceIdSize, RequiredSize);

    if (!DeviceInstanceId && DeviceInstanceIdSize > 0)
        SetLastError(ERROR_INVALID_PARAMETER);
    else
    {
        if (DeviceInstanceIdSize != 0)
        {
            DeviceInstanceIdW = MyMalloc(DeviceInstanceIdSize * sizeof(WCHAR));
            if (DeviceInstanceIdW == NULL)
                return FALSE;
        }

        ret = SetupDiGetDeviceInstanceIdW(DeviceInfoSet, DeviceInfoData,
                                          DeviceInstanceIdW, DeviceInstanceIdSize,
                                          RequiredSize);

        if (ret && DeviceInstanceIdW != NULL)
        {
            if (WideCharToMultiByte(CP_ACP, 0, DeviceInstanceIdW, -1,
                DeviceInstanceId, DeviceInstanceIdSize, NULL, NULL) == 0)
            {
                DeviceInstanceId[0] = '\0';
                ret = FALSE;
            }
        }
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiGetDeviceInstanceIdW(SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetDeviceInstanceIdW(
        IN HDEVINFO DeviceInfoSet,
        IN PSP_DEVINFO_DATA DeviceInfoData,
        OUT PWSTR DeviceInstanceId OPTIONAL,
        IN DWORD DeviceInstanceIdSize,
        OUT PDWORD RequiredSize OPTIONAL)
{
    BOOL ret = FALSE;

    TRACE("%p %p %p %lu %p\n", DeviceInfoSet, DeviceInfoData,
          DeviceInstanceId, DeviceInstanceIdSize, RequiredSize);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (((struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (!DeviceInfoData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (!DeviceInstanceId && DeviceInstanceIdSize > 0)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInstanceId && DeviceInstanceIdSize == 0)
        SetLastError(ERROR_INVALID_PARAMETER);
    else
    {
        struct DeviceInfoElement *DevInfo = (struct DeviceInfoElement *)DeviceInfoData->Reserved;
        DWORD required;

        required = (wcslen(DevInfo->DeviceName) + 1) * sizeof(WCHAR);
        if (RequiredSize)
            *RequiredSize = required;

        if (required <= DeviceInstanceIdSize)
        {
            wcscpy(DeviceInstanceId, DevInfo->DeviceName);
            ret = TRUE;
        }
        else
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiGetClassDevPropertySheetsA(SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetClassDevPropertySheetsA(
        IN HDEVINFO DeviceInfoSet,
        IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL,
        IN LPPROPSHEETHEADERA PropertySheetHeader,
        IN DWORD PropertySheetHeaderPageListSize,
        OUT PDWORD RequiredSize OPTIONAL,
        IN DWORD PropertySheetType)
{
    PROPSHEETHEADERW psh;
    BOOL ret = FALSE;

    TRACE("%p %p %p 0%lx %p 0x%lx\n", DeviceInfoSet, DeviceInfoData,
        PropertySheetHeader, PropertySheetHeaderPageListSize,
        RequiredSize, PropertySheetType);

    psh.dwFlags = PropertySheetHeader->dwFlags;
    psh.phpage = PropertySheetHeader->phpage;
    psh.nPages = PropertySheetHeader->nPages;

    ret = SetupDiGetClassDevPropertySheetsW(DeviceInfoSet, DeviceInfoData, PropertySheetHeader ? &psh : NULL,
                                            PropertySheetHeaderPageListSize, RequiredSize,
                                            PropertySheetType);
    if (ret)
    {
        PropertySheetHeader->nPages = psh.nPages;
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

struct ClassDevPropertySheetsData
{
    HPROPSHEETPAGE *PropertySheetPages;
    DWORD MaximumNumberOfPages;
    DWORD NumberOfPages;
};

static BOOL WINAPI GetClassDevPropertySheetsCallback(
        IN HPROPSHEETPAGE hPropSheetPage,
        IN OUT LPARAM lParam)
{
    struct ClassDevPropertySheetsData *PropPageData;

    PropPageData = (struct ClassDevPropertySheetsData *)lParam;

    if (PropPageData->NumberOfPages < PropPageData->MaximumNumberOfPages)
    {
        *PropPageData->PropertySheetPages = hPropSheetPage;
        PropPageData->PropertySheetPages++;
    }

    PropPageData->NumberOfPages++;
    return TRUE;
}

/***********************************************************************
 *		SetupDiGetClassDevPropertySheetsW(SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetClassDevPropertySheetsW(
        IN HDEVINFO DeviceInfoSet,
        IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL,
        IN OUT LPPROPSHEETHEADERW PropertySheetHeader,
        IN DWORD PropertySheetHeaderPageListSize,
        OUT PDWORD RequiredSize OPTIONAL,
        IN DWORD PropertySheetType)
{
    struct DeviceInfoSet *list;
    BOOL ret = FALSE;

    TRACE("%p %p %p 0%lx %p 0x%lx\n", DeviceInfoSet, DeviceInfoData,
        PropertySheetHeader, PropertySheetHeaderPageListSize,
        RequiredSize, PropertySheetType);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (((struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if ((list = (struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (!PropertySheetHeader)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (PropertySheetHeader->dwFlags & PSH_PROPSHEETPAGE)
        SetLastError(ERROR_INVALID_FLAGS);
    else if (DeviceInfoData && DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (!DeviceInfoData && IsEqualIID(&list->ClassGuid, &GUID_NULL))
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (!PropertySheetHeader)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (PropertySheetType != DIGCDP_FLAG_ADVANCED
          && PropertySheetType != DIGCDP_FLAG_BASIC
          && PropertySheetType != DIGCDP_FLAG_REMOTE_ADVANCED
          && PropertySheetType != DIGCDP_FLAG_REMOTE_BASIC)
        SetLastError(ERROR_INVALID_PARAMETER);
    else
    {
        HKEY hKey = INVALID_HANDLE_VALUE;
        SP_PROPSHEETPAGE_REQUEST Request;
        LPWSTR PropPageProvider = NULL;
        HMODULE hModule = NULL;
        PROPERTY_PAGE_PROVIDER pPropPageProvider = NULL;
        struct ClassDevPropertySheetsData PropPageData;
        DWORD dwLength, dwRegType;
        DWORD rc;

        if (DeviceInfoData)
            hKey = SetupDiOpenDevRegKey(DeviceInfoSet, DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_QUERY_VALUE);
        else
        {
            hKey = SetupDiOpenClassRegKeyExW(&list->ClassGuid, KEY_QUERY_VALUE,
                DIOCR_INSTALLER, list->MachineName + 2, NULL);
        }
        if (hKey == INVALID_HANDLE_VALUE)
            goto cleanup;

        rc = RegQueryValueExW(hKey, L"EnumPropPages32", NULL, &dwRegType, NULL, &dwLength);
        if (rc == ERROR_FILE_NOT_FOUND)
        {
            /* No registry key. As it is optional, don't say it's a bad error */
            if (RequiredSize)
                *RequiredSize = 0;
            ret = TRUE;
            goto cleanup;
        }
        else if (rc != ERROR_SUCCESS && dwRegType != REG_SZ)
        {
            SetLastError(rc);
            goto cleanup;
        }

        PropPageProvider = HeapAlloc(GetProcessHeap(), 0, dwLength + sizeof(WCHAR));
        if (!PropPageProvider)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            goto cleanup;
        }
        rc = RegQueryValueExW(hKey, L"EnumPropPages32", NULL, NULL, (LPBYTE)PropPageProvider, &dwLength);
        if (rc != ERROR_SUCCESS)
        {
            SetLastError(rc);
            goto cleanup;
        }
        PropPageProvider[dwLength / sizeof(WCHAR)] = 0;

        rc = GetFunctionPointer(PropPageProvider, &hModule, (PVOID*)&pPropPageProvider);
        if (rc != ERROR_SUCCESS)
        {
            SetLastError(ERROR_INVALID_PROPPAGE_PROVIDER);
            goto cleanup;
        }

        Request.cbSize = sizeof(SP_PROPSHEETPAGE_REQUEST);
        Request.PageRequested = SPPSR_ENUM_ADV_DEVICE_PROPERTIES;
        Request.DeviceInfoSet = DeviceInfoSet;
        Request.DeviceInfoData = DeviceInfoData;
        PropPageData.PropertySheetPages = &PropertySheetHeader->phpage[PropertySheetHeader->nPages];
        PropPageData.MaximumNumberOfPages = PropertySheetHeaderPageListSize - PropertySheetHeader->nPages;
        PropPageData.NumberOfPages = 0;
        ret = pPropPageProvider(&Request, GetClassDevPropertySheetsCallback, (LPARAM)&PropPageData);
        if (!ret)
            goto cleanup;

        if (RequiredSize)
            *RequiredSize = PropPageData.NumberOfPages + PropertySheetHeader->nPages;
        if (PropPageData.NumberOfPages <= PropPageData.MaximumNumberOfPages)
        {
            PropertySheetHeader->nPages += PropPageData.NumberOfPages;
            ret = TRUE;
        }
        else
        {
            PropertySheetHeader->nPages += PropPageData.MaximumNumberOfPages;
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
        }

cleanup:
        if (hKey != INVALID_HANDLE_VALUE)
            RegCloseKey(hKey);
        HeapFree(GetProcessHeap(), 0, PropPageProvider);
        FreeFunctionPointer(hModule, pPropPageProvider);
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiCreateDevRegKeyA (SETUPAPI.@)
 */
HKEY WINAPI SetupDiCreateDevRegKeyA(
        IN HDEVINFO DeviceInfoSet,
        IN PSP_DEVINFO_DATA DeviceInfoData,
        IN DWORD Scope,
        IN DWORD HwProfile,
        IN DWORD KeyType,
        IN HINF InfHandle OPTIONAL,
        IN PCSTR InfSectionName OPTIONAL)
{
    PCWSTR InfSectionNameW = NULL;
    HKEY ret = INVALID_HANDLE_VALUE;

    if (InfSectionName)
    {
        InfSectionNameW = MultiByteToUnicode(InfSectionName, CP_ACP);
        if (InfSectionNameW == NULL) return INVALID_HANDLE_VALUE;
    }

    ret = SetupDiCreateDevRegKeyW(DeviceInfoSet,
                                  DeviceInfoData,
                                  Scope,
                                  HwProfile,
                                  KeyType,
                                  InfHandle,
                                  InfSectionNameW);

    if (InfSectionNameW != NULL)
        MyFree((PVOID)InfSectionNameW);

    return ret;
}

/***********************************************************************
 *		SetupDiCreateDevRegKeyW (SETUPAPI.@)
 */
HKEY WINAPI SetupDiCreateDevRegKeyW(
        IN HDEVINFO DeviceInfoSet,
        IN PSP_DEVINFO_DATA DeviceInfoData,
        IN DWORD Scope,
        IN DWORD HwProfile,
        IN DWORD KeyType,
        IN HINF InfHandle OPTIONAL,
        IN PCWSTR InfSectionName OPTIONAL)
{
    struct DeviceInfoSet *list;
    HKEY ret = INVALID_HANDLE_VALUE;

    TRACE("%p %p %lu %lu %lu %p %s\n", DeviceInfoSet, DeviceInfoData,
          Scope, HwProfile, KeyType, InfHandle, debugstr_w(InfSectionName));

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_HANDLE);
    else if ((list = (struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (!DeviceInfoData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (Scope != DICS_FLAG_GLOBAL && Scope != DICS_FLAG_CONFIGSPECIFIC)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (KeyType != DIREG_DEV && KeyType != DIREG_DRV)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (InfHandle && !InfSectionName)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (!InfHandle && InfSectionName)
        SetLastError(ERROR_INVALID_PARAMETER);
    else
    {
        LPWSTR lpGuidString = NULL;
        LPWSTR DriverKey = NULL; /* {GUID}\Index */
        LPWSTR pDeviceInstance; /* Points into DriverKey, on the Index field */
        DWORD Index; /* Index used in the DriverKey name */
        DWORD rc;
        HKEY hClassKey = INVALID_HANDLE_VALUE;
        HKEY hDeviceKey = INVALID_HANDLE_VALUE;
        HKEY hKey = INVALID_HANDLE_VALUE;

        if (Scope == DICS_FLAG_CONFIGSPECIFIC)
        {
            FIXME("DICS_FLAG_CONFIGSPECIFIC case unimplemented\n");
            goto cleanup;
        }

        if (KeyType == DIREG_DEV)
        {
            FIXME("DIREG_DEV case unimplemented\n");
        }
        else /* KeyType == DIREG_DRV */
        {
            if (UuidToStringW((UUID*)&DeviceInfoData->ClassGuid, &lpGuidString) != RPC_S_OK)
                goto cleanup;
            /* The driver key is in HKLM\System\CurrentControlSet\Control\Class\{GUID}\Index */
            DriverKey = HeapAlloc(GetProcessHeap(), 0, (wcslen(lpGuidString) + 7) * sizeof(WCHAR) + sizeof(UNICODE_STRING));
            if (!DriverKey)
            {
                SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                goto cleanup;
            }
            wcscpy(DriverKey, L"{");
            wcscat(DriverKey, lpGuidString);
            wcscat(DriverKey, L"}\\");
            pDeviceInstance = &DriverKey[wcslen(DriverKey)];
            rc = RegOpenKeyExW(list->HKLM,
                ControlClass,
                0,
                KEY_CREATE_SUB_KEY,
                &hClassKey);
            if (rc != ERROR_SUCCESS)
            {
                SetLastError(rc);
                goto cleanup;
            }

            /* Try all values for Index between 0 and 9999 */
            Index = 0;
            while (Index <= 9999)
            {
                DWORD Disposition;
                wsprintf(pDeviceInstance, L"%04lu", Index);
                rc = RegCreateKeyEx(hClassKey,
                    DriverKey,
                    0,
                    NULL,
                    REG_OPTION_NON_VOLATILE,
#if _WIN32_WINNT >= 0x502
                    KEY_READ | KEY_WRITE,
#else
                    KEY_ALL_ACCESS,
#endif
                    NULL,
                    &hKey,
                    &Disposition);
                if (rc != ERROR_SUCCESS)
                {
                    SetLastError(rc);
                    goto cleanup;
                }
                if (Disposition == REG_CREATED_NEW_KEY)
                    break;
                RegCloseKey(hKey);
                hKey = INVALID_HANDLE_VALUE;
                Index++;
            }
            if (Index > 9999)
            {
                /* Unable to create more than 9999 devices within the same class */
                SetLastError(ERROR_GEN_FAILURE);
                goto cleanup;
            }

            /* Open device key, to write Driver value */
            hDeviceKey = SetupDiOpenDevRegKey(DeviceInfoSet, DeviceInfoData, Scope, HwProfile, DIREG_DEV, KEY_SET_VALUE);
            if (hDeviceKey == INVALID_HANDLE_VALUE)
                goto cleanup;
            rc = RegSetValueEx(hDeviceKey, L"Driver", 0, REG_SZ, (const BYTE *)DriverKey, (wcslen(DriverKey) + 1) * sizeof(WCHAR));
            if (rc != ERROR_SUCCESS)
            {
                SetLastError(rc);
                goto cleanup;
            }
        }

        /* Do installation of the specified section */
        if (InfHandle)
        {
            FIXME("Need to install section %s in file %p\n",
                debugstr_w(InfSectionName), InfHandle);
        }
        ret = hKey;

cleanup:
        if (lpGuidString)
            RpcStringFreeW(&lpGuidString);
        HeapFree(GetProcessHeap(), 0, DriverKey);
        if (hClassKey != INVALID_HANDLE_VALUE)
            RegCloseKey(hClassKey);
        if (hDeviceKey != INVALID_HANDLE_VALUE)
            RegCloseKey(hDeviceKey);
        if (hKey != INVALID_HANDLE_VALUE && hKey != ret)
            RegCloseKey(hKey);
    }

    TRACE("Returning 0x%p\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiOpenDevRegKey (SETUPAPI.@)
 */
HKEY WINAPI SetupDiOpenDevRegKey(
       HDEVINFO DeviceInfoSet,
       PSP_DEVINFO_DATA DeviceInfoData,
       DWORD Scope,
       DWORD HwProfile,
       DWORD KeyType,
       REGSAM samDesired)
{
    struct DeviceInfoSet *list;
    HKEY ret = INVALID_HANDLE_VALUE;

    TRACE("%p %p %lu %lu %lu 0x%lx\n", DeviceInfoSet, DeviceInfoData,
          Scope, HwProfile, KeyType, samDesired);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_HANDLE);
    else if ((list = (struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (!DeviceInfoData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (Scope != DICS_FLAG_GLOBAL && Scope != DICS_FLAG_CONFIGSPECIFIC)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (KeyType != DIREG_DEV && KeyType != DIREG_DRV)
        SetLastError(ERROR_INVALID_PARAMETER);
    else
    {
        HKEY hKey = INVALID_HANDLE_VALUE;
        HKEY hRootKey = INVALID_HANDLE_VALUE;
        struct DeviceInfoElement *deviceInfo = (struct DeviceInfoElement *)DeviceInfoData->Reserved;
        LPWSTR DriverKey = NULL;
        DWORD dwLength = 0;
        DWORD dwRegType;
        DWORD rc;

        if (Scope == DICS_FLAG_CONFIGSPECIFIC)
        {
            FIXME("DICS_FLAG_CONFIGSPECIFIC case unimplemented\n");
        }
        else /* Scope == DICS_FLAG_GLOBAL */
        {
            rc = RegOpenKeyExW(
                list->HKLM,
                EnumKeyName,
                0, /* Options */
                KEY_ENUMERATE_SUB_KEYS,
                &hRootKey);
            if (rc != ERROR_SUCCESS)
            {
                SetLastError(rc);
                goto cleanup;
            }
            rc = RegOpenKeyExW(
                hRootKey,
                deviceInfo->DeviceName,
                0, /* Options */
                KeyType == DIREG_DEV ? samDesired : KEY_QUERY_VALUE,
                &hKey);
            RegCloseKey(hRootKey);
            hRootKey = INVALID_HANDLE_VALUE;
            if (rc != ERROR_SUCCESS)
            {
                SetLastError(rc);
                goto cleanup;
            }
            if (KeyType == DIREG_DEV)
            {
                /* We're done. Just return the hKey handle */
                ret = hKey;
                goto cleanup;
            }
            /* Read the 'Driver' key */
            rc = RegQueryValueExW(hKey, L"Driver", NULL, &dwRegType, NULL, &dwLength);
            if (rc != ERROR_SUCCESS)
            {
                SetLastError(rc);
                goto cleanup;
            }
            if (dwRegType != REG_SZ)
            {
                SetLastError(ERROR_GEN_FAILURE);
                goto cleanup;
            }
            DriverKey = HeapAlloc(GetProcessHeap(), 0, dwLength);
            if (!DriverKey)
            {
                SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                goto cleanup;
            }
            rc = RegQueryValueExW(hKey, L"Driver", NULL, &dwRegType, (LPBYTE)DriverKey, &dwLength);
            if (rc != ERROR_SUCCESS)
            {
                SetLastError(rc);
                goto cleanup;
            }
            RegCloseKey(hKey);
            hKey = INVALID_HANDLE_VALUE;
            /* Need to open the driver key */
            rc = RegOpenKeyExW(
                list->HKLM,
                ControlClass,
                0, /* Options */
                KEY_ENUMERATE_SUB_KEYS,
                &hRootKey);
            if (rc != ERROR_SUCCESS)
            {
                SetLastError(rc);
                goto cleanup;
            }
            rc = RegOpenKeyExW(
                hRootKey,
                DriverKey,
                0, /* Options */
                samDesired,
                &hKey);
            if (rc != ERROR_SUCCESS)
            {
                SetLastError(rc);
                goto cleanup;
            }
            ret = hKey;
        }
cleanup:
        if (hRootKey != INVALID_HANDLE_VALUE)
            RegCloseKey(hRootKey);
        if (hKey != INVALID_HANDLE_VALUE && hKey != ret)
            RegCloseKey(hKey);
    }

    TRACE("Returning 0x%p\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiCreateDeviceInfoA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiCreateDeviceInfoA(
       HDEVINFO DeviceInfoSet,
       PCSTR DeviceName,
       CONST GUID *ClassGuid,
       PCSTR DeviceDescription,
       HWND hwndParent,
       DWORD CreationFlags,
       PSP_DEVINFO_DATA DeviceInfoData)
{
    LPWSTR DeviceNameW = NULL;
    LPWSTR DeviceDescriptionW = NULL;
    BOOL bResult;

    TRACE("\n");

    if (DeviceName)
    {
        DeviceNameW = MultiByteToUnicode(DeviceName, CP_ACP);
        if (DeviceNameW == NULL) return FALSE;
    }
    if (DeviceDescription)
    {
        DeviceDescriptionW = MultiByteToUnicode(DeviceDescription, CP_ACP);
        if (DeviceDescriptionW == NULL)
        {
            if (DeviceNameW) MyFree(DeviceNameW);
            return FALSE;
        }
    }

    bResult = SetupDiCreateDeviceInfoW(DeviceInfoSet, DeviceNameW,
                                       ClassGuid, DeviceDescriptionW,
                                       hwndParent, CreationFlags,
                                       DeviceInfoData);

    if (DeviceNameW) MyFree(DeviceNameW);
    if (DeviceDescriptionW) MyFree(DeviceDescriptionW);

    return bResult;
}

/***********************************************************************
 *		SetupDiCreateDeviceInfoW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiCreateDeviceInfoW(
       HDEVINFO DeviceInfoSet,
       PCWSTR DeviceName,
       CONST GUID *ClassGuid,
       PCWSTR DeviceDescription,
       HWND hwndParent,
       DWORD CreationFlags,
       PSP_DEVINFO_DATA DeviceInfoData)
{
    struct DeviceInfoSet *list;
    BOOL ret = FALSE;

    TRACE("%p %S %s %S %p %lx %p\n", DeviceInfoSet, DeviceName,
        debugstr_guid(ClassGuid), DeviceDescription,
        hwndParent, CreationFlags, DeviceInfoData);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_HANDLE);
    else if ((list = (struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (!ClassGuid)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (!IsEqualIID(&list->ClassGuid, &GUID_NULL) && !IsEqualIID(&list->ClassGuid, ClassGuid))
        SetLastError(ERROR_CLASS_MISMATCH);
    else if (CreationFlags & ~(DICD_GENERATE_ID | DICD_INHERIT_CLASSDRVS))
    {
        TRACE("Unknown flags: 0x%08lx\n", CreationFlags & ~(DICD_GENERATE_ID | DICD_INHERIT_CLASSDRVS));
        SetLastError(ERROR_INVALID_FLAGS);
    }
    else
    {
        SP_DEVINFO_DATA DevInfo;

        if (CreationFlags & DICD_GENERATE_ID)
        {
            /* Generate a new unique ID for this device */
            SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
            FIXME("not implemented\n");
        }
        else
        {
            /* Device name is fully qualified. Try to open it */
            BOOL rc;

            DevInfo.cbSize = sizeof(SP_DEVINFO_DATA);
            rc = SetupDiOpenDeviceInfoW(
                DeviceInfoSet,
                DeviceName,
                NULL, /* hwndParent */
                CreationFlags & DICD_INHERIT_CLASSDRVS ? DIOD_INHERIT_CLASSDRVS : 0,
                &DevInfo);

            if (rc)
            {
                /* SetupDiOpenDeviceInfoW has already added
                 * the device info to the device info set
                 */
                SetLastError(ERROR_DEVINST_ALREADY_EXISTS);
            }
            else if (GetLastError() == ERROR_FILE_NOT_FOUND)
            {
                struct DeviceInfoElement *deviceInfo;

                if (CreateDeviceInfoElement(list, DeviceName, ClassGuid, &deviceInfo))
                {
                    InsertTailList(&list->ListHead, &deviceInfo->ListEntry);

                    if (!DeviceInfoData)
                        ret = TRUE;
                    else
                    {
                        if (DeviceInfoData->cbSize != sizeof(PSP_DEVINFO_DATA))
                        {
                            SetLastError(ERROR_INVALID_USER_BUFFER);
                        }
                        else
                        {
                            memcpy(&DeviceInfoData->ClassGuid, ClassGuid, sizeof(GUID));
                            DeviceInfoData->DevInst = deviceInfo->dnDevInst;
                            DeviceInfoData->Reserved = (ULONG_PTR)deviceInfo;
                            ret = TRUE;
                        }
                    }
                }
            }
        }
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		Helper functions for SetupDiBuildDriverInfoList
 */
static BOOL
AddDriverToList(
    IN PLIST_ENTRY DriverListHead,
    IN DWORD DriverType, /* SPDIT_CLASSDRIVER or SPDIT_COMPATDRIVER */
    IN LPGUID ClassGuid,
    IN INFCONTEXT ContextDevice,
    IN struct InfFileDetails *InfFileDetails,
    IN LPCWSTR InfFile,
    IN LPCWSTR ProviderName,
    IN LPCWSTR ManufacturerName,
    IN LPCWSTR MatchingId,
    FILETIME DriverDate,
    DWORDLONG DriverVersion,
    IN DWORD Rank)
{
    struct DriverInfoElement *driverInfo = NULL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    DWORD RequiredSize = 128; /* Initial buffer size */
    BOOL Result = FALSE;
    PLIST_ENTRY PreviousEntry;
    LPWSTR InfInstallSection = NULL;
    BOOL ret = FALSE;

    driverInfo = HeapAlloc(GetProcessHeap(), 0, sizeof(struct DriverInfoElement));
    if (!driverInfo)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        goto cleanup;
    }
    memset(driverInfo, 0, sizeof(struct DriverInfoElement));

    driverInfo->Details.cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);
    driverInfo->Details.Reserved = (ULONG_PTR)driverInfo;

    /* Copy InfFileName field */
    wcsncpy(driverInfo->Details.InfFileName, InfFile, MAX_PATH - 1);
    driverInfo->Details.InfFileName[MAX_PATH - 1] = '\0';

    /* Fill InfDate field */
    /* FIXME: hFile = CreateFile(driverInfo->Details.InfFileName,
        GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        goto cleanup;
    Result = GetFileTime(hFile, NULL, NULL, &driverInfo->Details.InfDate);
    if (!Result)
        goto cleanup;*/

    /* Fill SectionName field */
    Result = SetupGetStringFieldW(
        &ContextDevice,
        1,
        driverInfo->Details.SectionName, LINE_LEN,
        NULL);
    if (!Result)
        goto cleanup;

    /* Fill DrvDescription field */
    Result = SetupGetStringFieldW(
        &ContextDevice,
        0, /* Field index */
        driverInfo->Details.DrvDescription, LINE_LEN,
        NULL);

    /* Copy MatchingId information */
    driverInfo->MatchingId = HeapAlloc(GetProcessHeap(), 0, (wcslen(MatchingId) + 1) * sizeof(WCHAR));
    if (!driverInfo->MatchingId)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        goto cleanup;
    }
    RtlCopyMemory(driverInfo->MatchingId, MatchingId, (wcslen(MatchingId) + 1) * sizeof(WCHAR));

    /* Get inf install section */
    Result = FALSE;
    RequiredSize = 128; /* Initial buffer size */
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    while (!Result && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        HeapFree(GetProcessHeap(), 0, InfInstallSection);
        InfInstallSection = HeapAlloc(GetProcessHeap(), 0, RequiredSize * sizeof(WCHAR));
        if (!InfInstallSection)
            goto cleanup;
        Result = SetupGetStringFieldW(
            &ContextDevice,
            1, /* Field index */
            InfInstallSection, RequiredSize,
            &RequiredSize);
    }
    if (!Result)
        goto cleanup;

    TRACE("Adding driver '%S' [%S/%S] (Rank 0x%lx)\n",
        driverInfo->Details.DrvDescription, InfFile, InfInstallSection, Rank);

    driverInfo->DriverRank = Rank;
    memcpy(&driverInfo->DriverDate, &DriverDate, sizeof(FILETIME));
    memcpy(&driverInfo->ClassGuid, ClassGuid, sizeof(GUID));
    driverInfo->Info.DriverType = DriverType;
    driverInfo->Info.Reserved = (ULONG_PTR)driverInfo;
    wcsncpy(driverInfo->Info.Description, driverInfo->Details.DrvDescription, LINE_LEN - 1);
    driverInfo->Info.Description[LINE_LEN - 1] = '\0';
    wcsncpy(driverInfo->Info.MfgName, ManufacturerName, LINE_LEN - 1);
    driverInfo->Info.MfgName[LINE_LEN - 1] = '\0';
    if (ProviderName)
    {
        wcsncpy(driverInfo->Info.ProviderName, ProviderName, LINE_LEN - 1);
        driverInfo->Info.ProviderName[LINE_LEN - 1] = '\0';
    }
    else
        driverInfo->Info.ProviderName[0] = '\0';
    driverInfo->Info.DriverDate = DriverDate;
    driverInfo->Info.DriverVersion = DriverVersion;
    ReferenceInfFile(InfFileDetails);
    driverInfo->InfFileDetails = InfFileDetails;

    /* Insert current driver in driver list, according to its rank */
    PreviousEntry = DriverListHead->Flink;
    while (PreviousEntry != DriverListHead)
    {
        struct DriverInfoElement *CurrentDriver;
        CurrentDriver = CONTAINING_RECORD(PreviousEntry, struct DriverInfoElement, ListEntry);
        if (CurrentDriver->DriverRank > Rank ||
            (CurrentDriver->DriverRank == Rank && CurrentDriver->DriverDate.QuadPart > driverInfo->DriverDate.QuadPart))
        {
            /* Insert before the current item */
            InsertHeadList(PreviousEntry, &driverInfo->ListEntry);
            break;
        }
        PreviousEntry = PreviousEntry->Flink;
    }
    if (PreviousEntry == DriverListHead)
    {
        /* Insert at the end of the list */
        InsertTailList(DriverListHead, &driverInfo->ListEntry);
    }

    ret = TRUE;

cleanup:
    if (!ret)
    {
        if (driverInfo)
            HeapFree(GetProcessHeap(), 0, driverInfo->MatchingId);
        HeapFree(GetProcessHeap(), 0, driverInfo);
    }
    if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);
    HeapFree(GetProcessHeap(), 0, InfInstallSection);

    return ret;
}

static BOOL
GetVersionInformationFromInfFile(
    IN HINF hInf,
    OUT LPGUID ClassGuid,
    OUT LPWSTR* pProviderName,
    OUT FILETIME* DriverDate,
    OUT DWORDLONG* DriverVersion)
{
    DWORD RequiredSize;
    WCHAR guidW[MAX_GUID_STRING_LEN + 1];
    LPWSTR DriverVer = NULL;
    LPWSTR ProviderName = NULL;
    LPWSTR pComma; /* Points into DriverVer */
    LPWSTR pVersion = NULL; /* Points into DriverVer */
    SYSTEMTIME SystemTime;
    BOOL Result;
    BOOL ret = FALSE; /* Final result */

    /* Get class Guid */
    if (!SetupGetLineTextW(
        NULL, /* Context */
        hInf,
        L"Version", L"ClassGUID",
        guidW, sizeof(guidW),
        NULL /* Required size */))
    {
        goto cleanup;
    }
    guidW[37] = '\0'; /* Replace the } by a NULL character */
    if (UuidFromStringW(&guidW[1], ClassGuid) != RPC_S_OK)
    {
        SetLastError(ERROR_GEN_FAILURE);
        goto cleanup;
    }

    /* Get provider name */
    Result = SetupGetLineTextW(
        NULL, /* Context */
        hInf, L"Version", L"Provider",
        NULL, 0,
        &RequiredSize);
    if (Result)
    {
        /* We know know the needed buffer size */
        ProviderName = HeapAlloc(GetProcessHeap(), 0, RequiredSize * sizeof(WCHAR));
        if (!ProviderName)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            goto cleanup;
        }
        Result = SetupGetLineTextW(
            NULL, /* Context */
            hInf, L"Version", L"Provider",
            ProviderName, RequiredSize,
            &RequiredSize);
    }
    if (!Result)
        goto cleanup;
    *pProviderName = ProviderName;

    /* Read the "DriverVer" value */
    Result = SetupGetLineTextW(
        NULL, /* Context */
        hInf, L"Version", L"DriverVer",
        NULL, 0,
        &RequiredSize);
    if (Result)
    {
        /* We know know the needed buffer size */
        DriverVer = HeapAlloc(GetProcessHeap(), 0, RequiredSize * sizeof(WCHAR));
        if (!DriverVer)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            goto cleanup;
        }
        Result = SetupGetLineTextW(
            NULL, /* Context */
            hInf, L"Version", L"DriverVer",
            DriverVer, RequiredSize,
            &RequiredSize);
    }
    if (!Result)
        goto cleanup;

    /* Get driver date and driver version, by analyzing the "DriverVer" value */
    pComma = wcschr(DriverVer, ',');
    if (pComma != NULL)
    {
        *pComma = UNICODE_NULL;
        pVersion = pComma + 1;
    }
    /* Get driver date version. Invalid date = 00/00/00 */
    memset(DriverDate, 0, sizeof(FILETIME));
    if (wcslen(DriverVer) == 10
        && (DriverVer[2] == '-' || DriverVer[2] == '/')
        && (DriverVer[5] == '-' || DriverVer[5] == '/'))
    {
        memset(&SystemTime, 0, sizeof(SYSTEMTIME));
        DriverVer[2] = DriverVer[5] = UNICODE_NULL;
        SystemTime.wMonth = ((DriverVer[0] - '0') * 10) + DriverVer[1] - '0';
        SystemTime.wDay  = ((DriverVer[3] - '0') * 10) + DriverVer[4] - '0';
        SystemTime.wYear = ((DriverVer[6] - '0') * 1000) + ((DriverVer[7] - '0') * 100) + ((DriverVer[8] - '0') * 10) + DriverVer[9] - '0';
        SystemTimeToFileTime(&SystemTime, DriverDate);
    }
    /* Get driver version. Invalid version = 0.0.0.0 */
    *DriverVersion = 0;
    if (pVersion)
    {
        WORD Major, Minor = 0, Revision = 0, Build = 0;
        LPWSTR pMinor = NULL, pRevision = NULL, pBuild = NULL;
        LARGE_INTEGER fullVersion;

        pMinor = strchrW(pVersion, '.');
        if (pMinor)
        {
            *pMinor = 0;
            pRevision = strchrW(++pMinor, '.');
            Minor = atoiW(pMinor);
        }
        if (pRevision)
        {
            *pRevision = 0;
            pBuild = strchrW(++pRevision, '.');
            Revision = atoiW(pRevision);
        }
        if (pBuild)
        {
            *pBuild = 0;
            pBuild++;
            Build = atoiW(pBuild);
        }
        Major = atoiW(pVersion);
        fullVersion.u.HighPart = Major << 16 | Minor;
        fullVersion.u.LowPart = Revision << 16 | Build;
        memcpy(DriverVersion, &fullVersion, sizeof(LARGE_INTEGER));
    }

    ret = TRUE;

cleanup:
    if (!ret)
        HeapFree(GetProcessHeap(), 0, ProviderName);
    HeapFree(GetProcessHeap(), 0, DriverVer);

    return ret;
}

/***********************************************************************
 *		SetupDiBuildDriverInfoList (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiBuildDriverInfoList(
    IN HDEVINFO DeviceInfoSet,
    IN OUT PSP_DEVINFO_DATA DeviceInfoData OPTIONAL,
    IN DWORD DriverType)
{
    struct DeviceInfoSet *list;
    SP_DEVINSTALL_PARAMS_W InstallParams;
    PVOID Buffer = NULL;
    struct InfFileDetails *currentInfFileDetails = NULL;
    LPWSTR ProviderName = NULL;
    LPWSTR ManufacturerName = NULL;
    WCHAR ManufacturerSection[LINE_LEN + 1];
    LPWSTR HardwareIDs = NULL;
    LPWSTR CompatibleIDs = NULL;
    LPWSTR FullInfFileName = NULL;
    FILETIME DriverDate;
    DWORDLONG DriverVersion = 0;
    DWORD RequiredSize;
    BOOL ret = FALSE;

    TRACE("%p %p %ld\n", DeviceInfoSet, DeviceInfoData, DriverType);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_HANDLE);
    else if ((list = (struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (list->HKLM != HKEY_LOCAL_MACHINE)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (DriverType != SPDIT_CLASSDRIVER && DriverType != SPDIT_COMPATDRIVER)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DriverType == SPDIT_CLASSDRIVER && DeviceInfoData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DriverType == SPDIT_COMPATDRIVER && !DeviceInfoData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoData && DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        BOOL Result;

        InstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
        Result = SetupDiGetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &InstallParams);
        if (!Result)
            goto done;

        if (DriverType == SPDIT_COMPATDRIVER)
        {
            /* Get hardware IDs list */
            Result = FALSE;
            RequiredSize = 512; /* Initial buffer size */
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            while (!Result && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                HeapFree(GetProcessHeap(), 0, HardwareIDs);
                HardwareIDs = HeapAlloc(GetProcessHeap(), 0, RequiredSize);
                if (!HardwareIDs)
                {
                    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                    goto done;
                }
                Result = SetupDiGetDeviceRegistryPropertyW(
                    DeviceInfoSet,
                    DeviceInfoData,
                    SPDRP_HARDWAREID,
                    NULL,
                    (PBYTE)HardwareIDs,
                    RequiredSize,
                    &RequiredSize);
            }
            if (!Result)
                goto done;

            /* Get compatible IDs list */
            Result = FALSE;
            RequiredSize = 512; /* Initial buffer size */
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            while (!Result && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                HeapFree(GetProcessHeap(), 0, CompatibleIDs);
                CompatibleIDs = HeapAlloc(GetProcessHeap(), 0, RequiredSize);
                if (!CompatibleIDs)
                {
                    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                    goto done;
                }
                Result = SetupDiGetDeviceRegistryPropertyW(
                    DeviceInfoSet,
                    DeviceInfoData,
                    SPDRP_COMPATIBLEIDS,
                    NULL,
                    (PBYTE)CompatibleIDs,
                    RequiredSize,
                    &RequiredSize);
                if (!Result && GetLastError() == ERROR_FILE_NOT_FOUND)
                {
                    /* No compatible ID for this device */
                    HeapFree(GetProcessHeap(), 0, CompatibleIDs);
                    CompatibleIDs = NULL;
                    Result = TRUE;
                }
            }
            if (!Result)
                goto done;
        }

        /* Enumerate .inf files */
        Result = FALSE;
        RequiredSize = 32768; /* Initial buffer size */
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        while (!Result && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            HeapFree(GetProcessHeap(), 0, Buffer);
            Buffer = HeapAlloc(GetProcessHeap(), 0, RequiredSize * sizeof(WCHAR));
            if (!Buffer)
            {
                Result = FALSE;
                SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                break;
            }
            Result = SetupGetInfFileListW(
                *InstallParams.DriverPath ? InstallParams.DriverPath : NULL,
                INF_STYLE_WIN4,
                Buffer, RequiredSize,
                &RequiredSize);
        }
        if (!Result && GetLastError() == ERROR_FILE_NOT_FOUND)
        {
            /* No .inf file in specified directory. So, we should
             * success as we created an empty driver info list.
             */
            ret = TRUE;
            goto done;
        }
        if (Result)
        {
            LPCWSTR filename;
            LPWSTR pFullFilename;

            if (*InstallParams.DriverPath)
            {
                DWORD len;
                len = GetFullPathNameW(InstallParams.DriverPath, 0, NULL, NULL);
                if (len == 0)
                    goto done;
                FullInfFileName = HeapAlloc(GetProcessHeap(), 0, len + MAX_PATH);
                if (!FullInfFileName)
                    goto done;
                len = GetFullPathNameW(InstallParams.DriverPath, len, FullInfFileName, NULL);
                if (len == 0)
                    goto done;
                if (*FullInfFileName && FullInfFileName[wcslen(FullInfFileName) - 1] != '\\')
                    wcscat(FullInfFileName, L"\\");
                pFullFilename = &FullInfFileName[wcslen(FullInfFileName)];
            }
            else
            {
                FullInfFileName = HeapAlloc(GetProcessHeap(), 0, MAX_PATH);
                if (!FullInfFileName)
                    goto done;
                pFullFilename = &FullInfFileName[0];
            }

            for (filename = (LPCWSTR)Buffer; *filename; filename += wcslen(filename) + 1)
            {
                INFCONTEXT ContextManufacturer, ContextDevice;
                GUID ClassGuid;

                wcscpy(pFullFilename, filename);
                TRACE("Opening file %S\n", FullInfFileName);

                currentInfFileDetails = HeapAlloc(
                    GetProcessHeap(),
                    0,
                    FIELD_OFFSET(struct InfFileDetails, FullInfFileName) + wcslen(FullInfFileName) * sizeof(WCHAR) + UNICODE_NULL);
                if (!currentInfFileDetails)
                    continue;
                memset(currentInfFileDetails, 0, sizeof(struct InfFileDetails));
                wcscpy(currentInfFileDetails->FullInfFileName, FullInfFileName);

                currentInfFileDetails->hInf = SetupOpenInfFileW(FullInfFileName, NULL, INF_STYLE_WIN4, NULL);
                ReferenceInfFile(currentInfFileDetails);
                if (currentInfFileDetails->hInf == INVALID_HANDLE_VALUE)
                {
                    HeapFree(GetProcessHeap(), 0, currentInfFileDetails);
                    currentInfFileDetails = NULL;
                    continue;
                }

                if (!GetVersionInformationFromInfFile(
                    currentInfFileDetails->hInf,
                    &ClassGuid,
                    &ProviderName,
                    &DriverDate,
                    &DriverVersion))
                {
                    SetupCloseInfFile(currentInfFileDetails->hInf);
                    HeapFree(GetProcessHeap(), 0, currentInfFileDetails->hInf);
                    currentInfFileDetails = NULL;
                    continue;
                }

                if (DriverType == SPDIT_CLASSDRIVER)
                {
                    /* Check if the ClassGuid in this .inf file is corresponding with our needs */
                    if (!IsEqualIID(&list->ClassGuid, &GUID_NULL) && !IsEqualIID(&list->ClassGuid, &ClassGuid))
                    {
                        goto next;
                    }
                }

                /* Get the manufacturers list */
                Result = SetupFindFirstLineW(currentInfFileDetails->hInf, L"Manufacturer", NULL, &ContextManufacturer);
                while (Result)
                {
                    Result = SetupGetStringFieldW(
                        &ContextManufacturer,
                        0, /* Field index */
                        NULL, 0,
                        &RequiredSize);
                    if (Result)
                    {
                        /* We got the needed size for the buffer */
                        ManufacturerName = HeapAlloc(GetProcessHeap(), 0, RequiredSize * sizeof(WCHAR));
                        if (!ManufacturerName)
                        {
                            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                            goto done;
                        }
                        Result = SetupGetStringFieldW(
                            &ContextManufacturer,
                            0, /* Field index */
                            ManufacturerName, RequiredSize,
                            &RequiredSize);
                    }
                    /* Get manufacturer section name */
                    Result = SetupGetStringFieldW(
                        &ContextManufacturer,
                        1, /* Field index */
                        ManufacturerSection, LINE_LEN,
                        &RequiredSize);
                    if (Result)
                    {
                        ManufacturerSection[RequiredSize] = 0; /* Final NULL char */
                        /* Add (possible) extension to manufacturer section name */
                        Result = SetupDiGetActualSectionToInstallW(
                            currentInfFileDetails->hInf, ManufacturerSection, ManufacturerSection, LINE_LEN, NULL, NULL);
                        if (Result)
                        {
                            TRACE("Enumerating devices in manufacturer %S\n", ManufacturerSection);
                            Result = SetupFindFirstLineW(currentInfFileDetails->hInf, ManufacturerSection, NULL, &ContextDevice);
                        }
                    }
                    while (Result)
                    {
                        if (DriverType == SPDIT_CLASSDRIVER)
                        {
                            /* FIXME: read [ControlFlags] / ExcludeFromSelect */
                            if (!AddDriverToList(
                                &list->DriverListHead,
                                DriverType,
                                &ClassGuid,
                                ContextDevice,
                                currentInfFileDetails,
                                filename,
                                ProviderName,
                                ManufacturerName,
                                NULL,
                                DriverDate, DriverVersion,
                                0))
                            {
                                break;
                            }
                        }
                        else /* DriverType = SPDIT_COMPATDRIVER */
                        {
                            /* 1. Get all fields */
                            DWORD FieldCount = SetupGetFieldCount(&ContextDevice);
                            DWORD DriverRank;
                            DWORD i;
                            LPCWSTR currentId;
                            BOOL DriverAlreadyAdded;

                            for (i = 2; i <= FieldCount; i++)
                            {
                                LPWSTR DeviceId = NULL;
                                Result = FALSE;
                                RequiredSize = 128; /* Initial buffer size */
                                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                                while (!Result && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
                                {
                                    HeapFree(GetProcessHeap(), 0, DeviceId);
                                    DeviceId = HeapAlloc(GetProcessHeap(), 0, RequiredSize * sizeof(WCHAR));
                                    if (!DeviceId)
                                    {
                                        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                                        goto done;
                                    }
                                    Result = SetupGetStringFieldW(
                                        &ContextDevice,
                                        i,
                                        DeviceId, RequiredSize,
                                        &RequiredSize);
                                }
                                if (!Result)
                                {
                                    HeapFree(GetProcessHeap(), 0, DeviceId);
                                    goto done;
                                }
                                DriverAlreadyAdded = FALSE;
                                for (DriverRank = 0, currentId = (LPCWSTR)HardwareIDs; !DriverAlreadyAdded && *currentId; currentId += wcslen(currentId) + 1, DriverRank++)
                                {
                                    if (wcsicmp(DeviceId, currentId) == 0)
                                    {
                                        AddDriverToList(
                                            &((struct DeviceInfoElement *)DeviceInfoData->Reserved)->DriverListHead,
                                            DriverType,
                                            &ClassGuid,
                                            ContextDevice,
                                            currentInfFileDetails,
                                            filename,
                                            ProviderName,
                                            ManufacturerName,
                                            currentId,
                                            DriverDate, DriverVersion,
                                            DriverRank  + (i == 2 ? 0 : 0x1000 + i - 3));
                                        DriverAlreadyAdded = TRUE;
                                    }
                                }
                                if (CompatibleIDs)
                                {
                                    for (DriverRank = 0, currentId = (LPCWSTR)CompatibleIDs; !DriverAlreadyAdded && *currentId; currentId += wcslen(currentId) + 1, DriverRank++)
                                    {
                                        if (wcsicmp(DeviceId, currentId) == 0)
                                        {
                                            AddDriverToList(
                                                &((struct DeviceInfoElement *)DeviceInfoData->Reserved)->DriverListHead,
                                                DriverType,
                                                &ClassGuid,
                                                ContextDevice,
                                                currentInfFileDetails,
                                                filename,
                                                ProviderName,
                                                ManufacturerName,
                                                currentId,
                                                DriverDate, DriverVersion,
                                                DriverRank + (i == 2 ? 0x2000 : 0x3000 + i - 3));
                                            DriverAlreadyAdded = TRUE;
                                        }
                                    }
                                }
                                HeapFree(GetProcessHeap(), 0, DeviceId);
                            }
                        }
                        Result = SetupFindNextLine(&ContextDevice, &ContextDevice);
                    }

                    HeapFree(GetProcessHeap(), 0, ManufacturerName);
                    ManufacturerName = NULL;
                    Result = SetupFindNextLine(&ContextManufacturer, &ContextManufacturer);
                }

                ret = TRUE;
next:
                HeapFree(GetProcessHeap(), 0, ProviderName);
                ProviderName = NULL;

                DereferenceInfFile(currentInfFileDetails);
                currentInfFileDetails = NULL;
            }
            ret = TRUE;
        }
    }

done:
    if (ret)
    {
        if (DeviceInfoData)
        {
            InstallParams.Flags |= DI_DIDCOMPAT;
            InstallParams.FlagsEx |= DI_FLAGSEX_DIDCOMPATINFO;
        }
        else
        {
            InstallParams.Flags |= DI_DIDCLASS;
            InstallParams.FlagsEx |= DI_FLAGSEX_DIDINFOLIST;
        }
        ret = SetupDiSetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &InstallParams);
    }

    HeapFree(GetProcessHeap(), 0, ProviderName);
    HeapFree(GetProcessHeap(), 0, ManufacturerName);
    HeapFree(GetProcessHeap(), 0, HardwareIDs);
    HeapFree(GetProcessHeap(), 0, CompatibleIDs);
    HeapFree(GetProcessHeap(), 0, FullInfFileName);
    if (currentInfFileDetails)
        DereferenceInfFile(currentInfFileDetails);
    HeapFree(GetProcessHeap(), 0, Buffer);

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiDeleteDeviceInfo (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiDeleteDeviceInfo(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData)
{
    TRACE("%p %p\n", DeviceInfoSet, DeviceInfoData);

    FIXME("not implemented\n");
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}


/***********************************************************************
 *		SetupDiDestroyDriverInfoList (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiDestroyDriverInfoList(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData,
    IN DWORD DriverType)
{
    struct DeviceInfoSet *list;
    BOOL ret = FALSE;

    TRACE("%p %p 0x%lx\n", DeviceInfoSet, DeviceInfoData, DriverType);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_HANDLE);
    else if ((list = (struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (DriverType != SPDIT_CLASSDRIVER && DriverType != SPDIT_COMPATDRIVER)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DriverType == SPDIT_COMPATDRIVER && !DeviceInfoData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoData && DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        PLIST_ENTRY ListEntry;
        struct DriverInfoElement *driverInfo;
        SP_DEVINSTALL_PARAMS_W InstallParams;

        InstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS_W);
        if (!SetupDiGetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &InstallParams))
            goto done;

        if (!DeviceInfoData)
            /* Fall back to destroying class driver list */
            DriverType = SPDIT_CLASSDRIVER;

        if (DriverType == SPDIT_CLASSDRIVER)
        {
            while (!IsListEmpty(&list->DriverListHead))
            {
                 ListEntry = RemoveHeadList(&list->DriverListHead);
                 driverInfo = (struct DriverInfoElement *)ListEntry;
                 DestroyDriverInfoElement(driverInfo);
            }
            InstallParams.Reserved = 0;
            InstallParams.Flags &= ~(DI_DIDCLASS | DI_MULTMFGS);
            InstallParams.FlagsEx &= ~DI_FLAGSEX_DIDINFOLIST;
            ret = SetupDiSetDeviceInstallParamsW(DeviceInfoSet, NULL, &InstallParams);
        }
        else
        {
            SP_DEVINSTALL_PARAMS_W InstallParamsSet;
            struct DeviceInfoElement *deviceInfo;

            InstallParamsSet.cbSize = sizeof(SP_DEVINSTALL_PARAMS_W);
            if (!SetupDiGetDeviceInstallParamsW(DeviceInfoSet, NULL, &InstallParamsSet))
                goto done;
            deviceInfo = (struct DeviceInfoElement *)DeviceInfoData->Reserved;
            while (!IsListEmpty(&deviceInfo->DriverListHead))
            {
                 ListEntry = RemoveHeadList(&deviceInfo->DriverListHead);
                 driverInfo = (struct DriverInfoElement *)ListEntry;
                 if ((PVOID)InstallParamsSet.Reserved == driverInfo)
                 {
                     InstallParamsSet.Reserved = 0;
                     SetupDiSetDeviceInstallParamsW(DeviceInfoSet, NULL, &InstallParamsSet);
                 }
                 DestroyDriverInfoElement(driverInfo);
            }
            InstallParams.Reserved = 0;
            InstallParams.Flags &= ~DI_DIDCOMPAT;
            InstallParams.FlagsEx &= ~DI_FLAGSEX_DIDCOMPATINFO;
            ret = SetupDiSetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &InstallParams);
        }
    }

done:
    TRACE("Returning %d\n", ret);
    return ret;
}


/***********************************************************************
 *		SetupDiOpenDeviceInfoA (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiOpenDeviceInfoA(
    IN HDEVINFO DeviceInfoSet,
    IN PCSTR DeviceInstanceId,
    IN HWND hwndParent OPTIONAL,
    IN DWORD OpenFlags,
    OUT PSP_DEVINFO_DATA DeviceInfoData OPTIONAL)
{
    LPWSTR DeviceInstanceIdW = NULL;
    BOOL bResult;

    TRACE("%p %s %p %lx %p\n", DeviceInfoSet, DeviceInstanceId, hwndParent, OpenFlags, DeviceInfoData);

    DeviceInstanceIdW = MultiByteToUnicode(DeviceInstanceId, CP_ACP);
    if (DeviceInstanceIdW == NULL)
        return FALSE;

    bResult = SetupDiOpenDeviceInfoW(DeviceInfoSet,
        DeviceInstanceIdW, hwndParent, OpenFlags, DeviceInfoData);

    MyFree(DeviceInstanceIdW);

    return bResult;
}


/***********************************************************************
 *		SetupDiOpenDeviceInfoW (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiOpenDeviceInfoW(
    IN HDEVINFO DeviceInfoSet,
    IN PCWSTR DeviceInstanceId,
    IN HWND hwndParent OPTIONAL,
    IN DWORD OpenFlags,
    OUT PSP_DEVINFO_DATA DeviceInfoData OPTIONAL)
{
    struct DeviceInfoSet *list;
    HKEY hEnumKey, hKey;
    DWORD rc;
    BOOL ret = FALSE;

    TRACE("%p %S %p %lx %p\n", DeviceInfoSet, DeviceInstanceId, hwndParent, OpenFlags, DeviceInfoData);

    if (OpenFlags & DIOD_CANCEL_REMOVE)
        FIXME("DIOD_CANCEL_REMOVE flag not implemented\n");

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_HANDLE);
    else if ((list = (struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (!DeviceInstanceId)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (OpenFlags & ~(DIOD_CANCEL_REMOVE | DIOD_INHERIT_CLASSDRVS))
    {
        TRACE("Unknown flags: 0x%08lx\n", OpenFlags & ~(DIOD_CANCEL_REMOVE | DIOD_INHERIT_CLASSDRVS));
        SetLastError(ERROR_INVALID_FLAGS);
    }
    else if (DeviceInfoData && DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        struct DeviceInfoElement *deviceInfo = NULL;
        /* Search if device already exists in DeviceInfoSet.
         *    If yes, return the existing element
         *    If no, create a new element using informations in registry
         */
        PLIST_ENTRY ItemList = list->ListHead.Flink;
        while (ItemList != &list->ListHead)
        {
            // TODO
            //if (good one)
            //    break;
            FIXME("not implemented\n");
            ItemList = ItemList->Flink;
        }

        if (deviceInfo)
        {
            /* good one found */
            ret = TRUE;
        }
        else
        {
            /* Open supposed registry key */
            rc = RegOpenKeyExW(
                list->HKLM,
                EnumKeyName,
                0, /* Options */
                KEY_ENUMERATE_SUB_KEYS,
                &hEnumKey);
            if (rc != ERROR_SUCCESS)
            {
                SetLastError(rc);
                return FALSE;
            }
            rc = RegOpenKeyExW(
                hEnumKey,
                DeviceInstanceId,
                0, /* Options */
                KEY_QUERY_VALUE,
                &hKey);
            RegCloseKey(hEnumKey);
            if (rc != ERROR_SUCCESS)
            {
                if (rc == ERROR_FILE_NOT_FOUND)
                    rc = ERROR_NO_SUCH_DEVINST;
                SetLastError(rc);
                return FALSE;
            }

            /* FIXME: try to get ClassGUID from registry, instead of
             * sending GUID_NULL to CreateDeviceInfoElement
             */
            if (!CreateDeviceInfoElement(list, DeviceInstanceId, &GUID_NULL, &deviceInfo))
            {
                RegCloseKey(hKey);
                return FALSE;
            }
            InsertTailList(&list->ListHead, &deviceInfo->ListEntry);

            RegCloseKey(hKey);
            ret = TRUE;
        }

        if (ret && deviceInfo && DeviceInfoData)
        {
            memcpy(&DeviceInfoData->ClassGuid, &deviceInfo->ClassGuid, sizeof(GUID));
            DeviceInfoData->DevInst = deviceInfo->dnDevInst;
            DeviceInfoData->Reserved = (ULONG_PTR)deviceInfo;
        }
    }

    return ret;
}


/***********************************************************************
 *		SetupDiEnumDriverInfoA (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiEnumDriverInfoA(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL,
    IN DWORD DriverType,
    IN DWORD MemberIndex,
    OUT PSP_DRVINFO_DATA_A DriverInfoData)
{
    SP_DRVINFO_DATA_V2_W driverInfoData2W;
    BOOL ret = FALSE;

    TRACE("%p %p 0x%lx %ld %p\n", DeviceInfoSet, DeviceInfoData,
        DriverType, MemberIndex, DriverInfoData);

    if (DriverInfoData == NULL)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DriverInfoData->cbSize != sizeof(SP_DRVINFO_DATA_V1_A) && DriverInfoData->cbSize != sizeof(SP_DRVINFO_DATA_V2_A))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        driverInfoData2W.cbSize = sizeof(SP_DRVINFO_DATA_V2_W);
        ret = SetupDiEnumDriverInfoW(DeviceInfoSet, DeviceInfoData,
            DriverType, MemberIndex, &driverInfoData2W);

        if (ret)
        {
            /* Do W->A conversion */
            DriverInfoData->DriverType = driverInfoData2W.DriverType;
            DriverInfoData->Reserved = driverInfoData2W.Reserved;
            if (WideCharToMultiByte(CP_ACP, 0, driverInfoData2W.Description, -1,
                DriverInfoData->Description, LINE_LEN, NULL, NULL) == 0)
            {
                DriverInfoData->Description[0] = '\0';
                ret = FALSE;
            }
            if (WideCharToMultiByte(CP_ACP, 0, driverInfoData2W.MfgName, -1,
                DriverInfoData->MfgName, LINE_LEN, NULL, NULL) == 0)
            {
                DriverInfoData->MfgName[0] = '\0';
                ret = FALSE;
            }
            if (WideCharToMultiByte(CP_ACP, 0, driverInfoData2W.ProviderName, -1,
                DriverInfoData->ProviderName, LINE_LEN, NULL, NULL) == 0)
            {
                DriverInfoData->ProviderName[0] = '\0';
                ret = FALSE;
            }
            if (DriverInfoData->cbSize == sizeof(SP_DRVINFO_DATA_V2_A))
            {
                /* Copy more fields */
                DriverInfoData->DriverDate = driverInfoData2W.DriverDate;
                DriverInfoData->DriverVersion = driverInfoData2W.DriverVersion;
            }
        }
    }

    TRACE("Returning %d\n", ret);
    return ret;
}


/***********************************************************************
 *		SetupDiEnumDriverInfoW (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiEnumDriverInfoW(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL,
    IN DWORD DriverType,
    IN DWORD MemberIndex,
    OUT PSP_DRVINFO_DATA_W DriverInfoData)
{
    PLIST_ENTRY ListHead;
    BOOL ret = FALSE;

    TRACE("%p %p 0x%lx %ld %p\n", DeviceInfoSet, DeviceInfoData,
        DriverType, MemberIndex, DriverInfoData);

    if (!DeviceInfoSet || !DriverInfoData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoSet == (HDEVINFO)INVALID_HANDLE_VALUE)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (((struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (DriverType != SPDIT_CLASSDRIVER && DriverType != SPDIT_COMPATDRIVER)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DriverType == SPDIT_CLASSDRIVER && DeviceInfoData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DriverType == SPDIT_COMPATDRIVER && !DeviceInfoData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DriverInfoData->cbSize != sizeof(SP_DRVINFO_DATA_V1_W) && DriverInfoData->cbSize != sizeof(SP_DRVINFO_DATA_V2_W))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        struct DeviceInfoElement *devInfo = (struct DeviceInfoElement *)DeviceInfoData->Reserved;
        PLIST_ENTRY ItemList;
        if (DriverType == SPDIT_CLASSDRIVER ||
            devInfo->CreationFlags & DICD_INHERIT_CLASSDRVS)
        {
            ListHead = &((struct DeviceInfoSet *)DeviceInfoSet)->DriverListHead;
        }
        else
        {
            ListHead = &devInfo->DriverListHead;
        }

        ItemList = ListHead->Flink;
        while (ItemList != ListHead && MemberIndex-- > 0)
            ItemList = ItemList->Flink;
        if (ItemList == ListHead)
            SetLastError(ERROR_NO_MORE_ITEMS);
        else
        {
            struct DriverInfoElement *DrvInfo = (struct DriverInfoElement *)ItemList;

            memcpy(
                &DriverInfoData->DriverType,
                &DrvInfo->Info.DriverType,
                DriverInfoData->cbSize - FIELD_OFFSET(SP_DRVINFO_DATA_W, DriverType));
            ret = TRUE;
        }
    }

    TRACE("Returning %d\n", ret);
    return ret;
}


/***********************************************************************
 *		SetupDiGetSelectedDriverA (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiGetSelectedDriverA(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL,
    OUT PSP_DRVINFO_DATA_A DriverInfoData)
{
    SP_DRVINFO_DATA_V2_W driverInfoData2W;
    BOOL ret = FALSE;

    if (DriverInfoData == NULL)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DriverInfoData->cbSize != sizeof(SP_DRVINFO_DATA_V1_A) && DriverInfoData->cbSize != sizeof(SP_DRVINFO_DATA_V2_A))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        driverInfoData2W.cbSize = sizeof(SP_DRVINFO_DATA_V2_W);

        ret = SetupDiGetSelectedDriverW(DeviceInfoSet,
                                        DeviceInfoData,
                                        &driverInfoData2W);

        if (ret)
        {
            /* Do W->A conversion */
            DriverInfoData->DriverType = driverInfoData2W.DriverType;
            DriverInfoData->Reserved = driverInfoData2W.Reserved;
            if (WideCharToMultiByte(CP_ACP, 0, driverInfoData2W.Description, -1,
                DriverInfoData->Description, LINE_LEN, NULL, NULL) == 0)
            {
                DriverInfoData->Description[0] = '\0';
                ret = FALSE;
            }
            if (WideCharToMultiByte(CP_ACP, 0, driverInfoData2W.MfgName, -1,
                DriverInfoData->MfgName, LINE_LEN, NULL, NULL) == 0)
            {
                DriverInfoData->MfgName[0] = '\0';
                ret = FALSE;
            }
            if (WideCharToMultiByte(CP_ACP, 0, driverInfoData2W.ProviderName, -1,
                DriverInfoData->ProviderName, LINE_LEN, NULL, NULL) == 0)
            {
                DriverInfoData->ProviderName[0] = '\0';
                ret = FALSE;
            }
            if (DriverInfoData->cbSize == sizeof(SP_DRVINFO_DATA_V2_A))
            {
                /* Copy more fields */
                DriverInfoData->DriverDate = driverInfoData2W.DriverDate;
                DriverInfoData->DriverVersion = driverInfoData2W.DriverVersion;
            }
        }
    }

    return ret;
}


/***********************************************************************
 *		SetupDiGetSelectedDriverW (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiGetSelectedDriverW(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL,
    OUT PSP_DRVINFO_DATA_W DriverInfoData)
{
    BOOL ret = FALSE;

    TRACE("%p %p %p\n", DeviceInfoSet, DeviceInfoData, DriverInfoData);

    if (!DeviceInfoSet || !DriverInfoData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoSet == (HDEVINFO)INVALID_HANDLE_VALUE)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (((struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (DeviceInfoData && DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (DriverInfoData->cbSize != sizeof(SP_DRVINFO_DATA_V1_W) && DriverInfoData->cbSize != sizeof(SP_DRVINFO_DATA_V2_W))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        SP_DEVINSTALL_PARAMS InstallParams;

        InstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
        if (SetupDiGetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &InstallParams))
        {
            struct DriverInfoElement *driverInfo;
            driverInfo = (struct DriverInfoElement *)InstallParams.Reserved;
            if (driverInfo == NULL)
                SetLastError(ERROR_NO_DRIVER_SELECTED);
            else
            {
                memcpy(
                    &DriverInfoData->DriverType,
                    &driverInfo->Info.DriverType,
                    DriverInfoData->cbSize - FIELD_OFFSET(SP_DRVINFO_DATA_W, DriverType));
                ret = TRUE;
            }
        }
    }

    TRACE("Returning %d\n", ret);
    return ret;
}


/***********************************************************************
 *		SetupDiSetSelectedDriverA (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiSetSelectedDriverA(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL,
    IN OUT PSP_DRVINFO_DATA_A DriverInfoData OPTIONAL)
{
    SP_DRVINFO_DATA_V1_W DriverInfoDataW;
    PSP_DRVINFO_DATA_W pDriverInfoDataW = NULL;
    BOOL ret = FALSE;

    if (DriverInfoData != NULL)
    {
        if (DriverInfoData->cbSize != sizeof(SP_DRVINFO_DATA_V2_A) &&
            DriverInfoData->cbSize != sizeof(SP_DRVINFO_DATA_V1_A));
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        DriverInfoDataW.cbSize = sizeof(SP_DRVINFO_DATA_V1_W);
        DriverInfoDataW.Reserved = DriverInfoData->Reserved;

        if (DriverInfoDataW.Reserved == 0)
        {
            DriverInfoDataW.DriverType = DriverInfoData->DriverType;

            /* convert the strings to unicode */
            if (!MultiByteToWideChar(CP_ACP,
                                     0,
                                     DriverInfoData->Description,
                                     LINE_LEN,
                                     DriverInfoDataW.Description,
                                     LINE_LEN) ||
                !MultiByteToWideChar(CP_ACP,
                                     0,
                                     DriverInfoData->ProviderName,
                                     LINE_LEN,
                                     DriverInfoDataW.ProviderName,
                                     LINE_LEN))
            {
                return FALSE;
            }
        }

        pDriverInfoDataW = (PSP_DRVINFO_DATA_W)&DriverInfoDataW;
    }

    ret = SetupDiSetSelectedDriverW(DeviceInfoSet,
                                    DeviceInfoData,
                                    pDriverInfoDataW);

    if (ret && pDriverInfoDataW != NULL)
    {
        DriverInfoData->Reserved = DriverInfoDataW.Reserved;
    }

    return ret;
}


/***********************************************************************
 *		SetupDiSetSelectedDriverW (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiSetSelectedDriverW(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL,
    IN OUT PSP_DRVINFO_DATA_W DriverInfoData OPTIONAL)
{
    BOOL ret = FALSE;

    TRACE("%p %p %p\n", DeviceInfoSet, DeviceInfoData, DriverInfoData);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoSet == (HDEVINFO)INVALID_HANDLE_VALUE)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (((struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (DeviceInfoData && DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (DriverInfoData && DriverInfoData->cbSize != sizeof(SP_DRVINFO_DATA_V1_W) && DriverInfoData->cbSize != sizeof(SP_DRVINFO_DATA_V2_W))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        struct DriverInfoElement **pDriverInfo;
        PLIST_ENTRY ListHead, ItemList;

        if (DeviceInfoData)
        {
            pDriverInfo = (struct DriverInfoElement **)&((struct DeviceInfoElement *)DeviceInfoData->Reserved)->InstallParams.Reserved;
            ListHead = &((struct DeviceInfoElement *)DeviceInfoData->Reserved)->DriverListHead;
        }
        else
        {
            pDriverInfo = (struct DriverInfoElement **)&((struct DeviceInfoSet *)DeviceInfoSet)->InstallParams.Reserved;
            ListHead = &((struct DeviceInfoSet *)DeviceInfoSet)->DriverListHead;
        }

        if (!DriverInfoData)
        {
            *pDriverInfo = NULL;
            ret = TRUE;
        }
        else
        {
            /* Search selected driver in list */
            ItemList = ListHead->Flink;
            while (ItemList != ListHead)
            {
                if (DriverInfoData->Reserved != 0)
                {
                    if (DriverInfoData->Reserved == (ULONG_PTR)ItemList)
                        break;
                }
                else
                {
                    /* The caller wants to compare only DriverType, Description and ProviderName fields */
                    struct DriverInfoElement *driverInfo = (struct DriverInfoElement *)ItemList;
                    if (driverInfo->Info.DriverType == DriverInfoData->DriverType
                        && wcscmp(driverInfo->Info.Description, DriverInfoData->Description) == 0
                        && wcscmp(driverInfo->Info.ProviderName, DriverInfoData->ProviderName) == 0)
                    {
                        break;
                    }
                }
            }
            if (ItemList == ListHead)
                SetLastError(ERROR_INVALID_PARAMETER);
            else
            {
                *pDriverInfo = (struct DriverInfoElement *)ItemList;
                DriverInfoData->Reserved = (ULONG_PTR)ItemList;
                ret = TRUE;
                TRACE("Choosing driver whose rank is 0x%lx\n",
                    ((struct DriverInfoElement *)ItemList)->DriverRank);
                if (DeviceInfoData)
                    memcpy(&DeviceInfoData->ClassGuid, &(*pDriverInfo)->ClassGuid, sizeof(GUID));
            }
        }
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiGetDriverInfoDetailA (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiGetDriverInfoDetailA(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL,
    IN PSP_DRVINFO_DATA_A DriverInfoData,
    OUT PSP_DRVINFO_DETAIL_DATA_A DriverInfoDetailData OPTIONAL,
    IN DWORD DriverInfoDetailDataSize,
    OUT PDWORD RequiredSize OPTIONAL)
{
    SP_DRVINFO_DATA_V2_W DriverInfoDataW;
    PSP_DRVINFO_DETAIL_DATA_W DriverInfoDetailDataW = NULL;
    DWORD BufSize = 0;
    DWORD HardwareIDLen = 0;
    BOOL ret = FALSE;

    /* do some sanity checks, the unicode version might do more thorough checks */
    if (DriverInfoData == NULL ||
        (DriverInfoDetailData == NULL && DriverInfoDetailDataSize != 0) ||
        (DriverInfoDetailData != NULL &&
         (DriverInfoDetailDataSize < FIELD_OFFSET(SP_DRVINFO_DETAIL_DATA_A, HardwareID) + sizeof(CHAR) ||
          DriverInfoDetailData->cbSize != sizeof(SP_DRVINFO_DETAIL_DATA_A))))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto Cleanup;
    }

    /* make sure we support both versions of the SP_DRVINFO_DATA structure */
    if (DriverInfoData->cbSize == sizeof(SP_DRVINFO_DATA_V1_A))
    {
        DriverInfoDataW.cbSize = sizeof(SP_DRVINFO_DATA_V1_W);
    }
    else if (DriverInfoData->cbSize == sizeof(SP_DRVINFO_DATA_V2_A))
    {
        DriverInfoDataW.cbSize = sizeof(SP_DRVINFO_DATA_V2_W);
    }
    else
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto Cleanup;
    }
    DriverInfoDataW.DriverType = DriverInfoData->DriverType;
    DriverInfoDataW.Reserved = DriverInfoData->Reserved;

    /* convert the strings to unicode */
    if (MultiByteToWideChar(CP_ACP,
                            0,
                            DriverInfoData->Description,
                            LINE_LEN,
                            DriverInfoDataW.Description,
                            LINE_LEN) &&
        MultiByteToWideChar(CP_ACP,
                            0,
                            DriverInfoData->MfgName,
                            LINE_LEN,
                            DriverInfoDataW.MfgName,
                            LINE_LEN) &&
        MultiByteToWideChar(CP_ACP,
                            0,
                            DriverInfoData->ProviderName,
                            LINE_LEN,
                            DriverInfoDataW.ProviderName,
                            LINE_LEN))
    {
        if (DriverInfoDataW.cbSize == sizeof(SP_DRVINFO_DATA_V2_W))
        {
            DriverInfoDataW.DriverDate = ((PSP_DRVINFO_DATA_V2_A)DriverInfoData)->DriverDate;
            DriverInfoDataW.DriverVersion = ((PSP_DRVINFO_DATA_V2_A)DriverInfoData)->DriverVersion;
        }

        if (DriverInfoDetailData != NULL)
        {
            /* calculate the unicode buffer size from the ansi buffer size */
            HardwareIDLen = DriverInfoDetailDataSize - FIELD_OFFSET(SP_DRVINFO_DETAIL_DATA_A, HardwareID);
            BufSize = FIELD_OFFSET(SP_DRVINFO_DETAIL_DATA_W, HardwareID) +
                      (HardwareIDLen * sizeof(WCHAR));

            DriverInfoDetailDataW = MyMalloc(BufSize);
            if (DriverInfoDetailDataW == NULL)
            {
                SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                goto Cleanup;
            }

            /* initialize the buffer */
            ZeroMemory(DriverInfoDetailDataW,
                       BufSize);
            DriverInfoDetailDataW->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA_W);
        }

        /* call the unicode version */
        ret = SetupDiGetDriverInfoDetailW(DeviceInfoSet,
                                          DeviceInfoData,
                                          &DriverInfoDataW,
                                          DriverInfoDetailDataW,
                                          BufSize,
                                          RequiredSize);

        if (ret)
        {
            if (DriverInfoDetailDataW != NULL)
            {
                /* convert the SP_DRVINFO_DETAIL_DATA_W structure to ansi */
                DriverInfoDetailData->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA_A);
                DriverInfoDetailData->InfDate = DriverInfoDetailDataW->InfDate;
                DriverInfoDetailData->Reserved = DriverInfoDetailDataW->Reserved;
                if (WideCharToMultiByte(CP_ACP,
                                        0,
                                        DriverInfoDetailDataW->SectionName,
                                        LINE_LEN,
                                        DriverInfoDetailData->SectionName,
                                        LINE_LEN,
                                        NULL,
                                        NULL) &&
                    WideCharToMultiByte(CP_ACP,
                                        0,
                                        DriverInfoDetailDataW->InfFileName,
                                        MAX_PATH,
                                        DriverInfoDetailData->InfFileName,
                                        MAX_PATH,
                                        NULL,
                                        NULL) &&
                    WideCharToMultiByte(CP_ACP,
                                        0,
                                        DriverInfoDetailDataW->DrvDescription,
                                        LINE_LEN,
                                        DriverInfoDetailData->DrvDescription,
                                        LINE_LEN,
                                        NULL,
                                        NULL) &&
                    WideCharToMultiByte(CP_ACP,
                                        0,
                                        DriverInfoDetailDataW->HardwareID,
                                        HardwareIDLen,
                                        DriverInfoDetailData->HardwareID,
                                        HardwareIDLen,
                                        NULL,
                                        NULL))
                {
                    DWORD len, cnt = 0;
                    DWORD hwidlen = HardwareIDLen;
                    CHAR *s = DriverInfoDetailData->HardwareID;

                    /* count the strings in the list */
                    while (*s != '\0')
                    {
                        len = lstrlenA(s) + 1;
                        if (hwidlen > len)
                        {
                            cnt++;
                            s += len;
                            hwidlen -= len;
                        }
                        else
                        {
                            /* looks like the string list wasn't terminated... */
                            SetLastError(ERROR_INVALID_USER_BUFFER);
                            ret = FALSE;
                            break;
                        }
                    }

                    /* make sure CompatIDsOffset points to the second string in the
                       list, if present */
                    if (cnt > 1)
                    {
                        DriverInfoDetailData->CompatIDsOffset = lstrlenA(DriverInfoDetailData->HardwareID) + 1;
                        DriverInfoDetailData->CompatIDsLength = (DWORD)(s - DriverInfoDetailData->HardwareID) -
                                                                DriverInfoDetailData->CompatIDsOffset + 1;
                    }
                    else
                    {
                        DriverInfoDetailData->CompatIDsOffset = 0;
                        DriverInfoDetailData->CompatIDsLength = 0;
                    }
                }
                else
                {
                    ret = FALSE;
                }
            }

            if (RequiredSize != NULL)
            {
                *RequiredSize = FIELD_OFFSET(SP_DRVINFO_DETAIL_DATA_A, HardwareID) +
                                (((*RequiredSize) - FIELD_OFFSET(SP_DRVINFO_DETAIL_DATA_W, HardwareID)) / sizeof(WCHAR));
            }
        }
    }

Cleanup:
    if (DriverInfoDetailDataW != NULL)
    {
        MyFree(DriverInfoDetailDataW);
    }

    return ret;
}

/***********************************************************************
 *		SetupDiGetDriverInfoDetailW (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiGetDriverInfoDetailW(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL,
    IN PSP_DRVINFO_DATA_W DriverInfoData,
    OUT PSP_DRVINFO_DETAIL_DATA_W DriverInfoDetailData OPTIONAL,
    IN DWORD DriverInfoDetailDataSize,
    OUT PDWORD RequiredSize OPTIONAL)
{
    BOOL ret = FALSE;

    TRACE("%p %p %p %p %lu %p\n", DeviceInfoSet, DeviceInfoData,
        DriverInfoData, DriverInfoDetailData,
        DriverInfoDetailDataSize, RequiredSize);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoSet == (HDEVINFO)INVALID_HANDLE_VALUE)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (((struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (DeviceInfoData && DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (!DriverInfoData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (!DriverInfoDetailData && DriverInfoDetailDataSize != 0)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DriverInfoDetailData && DriverInfoDetailDataSize < sizeof(SP_DRVINFO_DETAIL_DATA_W))
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DriverInfoDetailData && DriverInfoDetailData->cbSize != sizeof(SP_DRVINFO_DETAIL_DATA_W))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (DriverInfoData->Reserved == 0)
        SetLastError(ERROR_NO_DRIVER_SELECTED);
    else
    {
        struct DriverInfoElement *driverInfoElement;
        driverInfoElement = (struct DriverInfoElement *)DriverInfoData->Reserved;

        memcpy(
            DriverInfoDetailData,
            &driverInfoElement->Details,
            driverInfoElement->Details.cbSize);
        /* FIXME: copy HardwareIDs/CompatibleIDs if buffer is big enough
         * Don't forget to set CompatIDsOffset and CompatIDsLength fields.
         */
        ret = TRUE;
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiSelectBestCompatDrv (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiSelectBestCompatDrv(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData)
{
    SP_DRVINFO_DATA_W drvInfoData;
    BOOL ret;

    TRACE("%p %p\n", DeviceInfoSet, DeviceInfoData);

    /* Drivers are sorted by rank in the driver list, so
     * the first driver in the list is the best one.
     */
    drvInfoData.cbSize = sizeof(SP_DRVINFO_DATA_W);
    ret = SetupDiEnumDriverInfoW(
        DeviceInfoSet,
        DeviceInfoData,
        SPDIT_COMPATDRIVER,
        0, /* Member index */
        &drvInfoData);

    if (ret)
    {
        ret = SetupDiSetSelectedDriverW(
            DeviceInfoSet,
            DeviceInfoData,
            &drvInfoData);
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiInstallDriverFiles (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiInstallDriverFiles(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL)
{
    BOOL ret = FALSE;

    TRACE("%p %p\n", DeviceInfoSet, DeviceInfoData);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoSet == (HDEVINFO)INVALID_HANDLE_VALUE)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (((struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (DeviceInfoData && DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else if (DeviceInfoData && ((struct DeviceInfoElement *)DeviceInfoData->Reserved)->InstallParams.Reserved == 0)
        SetLastError(ERROR_NO_DRIVER_SELECTED);
    else if (!DeviceInfoData && ((struct DeviceInfoSet *)DeviceInfoSet)->InstallParams.Reserved == 0)
        SetLastError(ERROR_NO_DRIVER_SELECTED);
    else
    {
        SP_DEVINSTALL_PARAMS_W InstallParams;
        struct DriverInfoElement *SelectedDriver;
        WCHAR SectionName[MAX_PATH];
        DWORD SectionNameLength = 0;

        InstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS_W);
        ret = SetupDiGetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &InstallParams);
        if (!ret)
            goto done;

        SelectedDriver = (struct DriverInfoElement *)InstallParams.Reserved;
        if (!SelectedDriver)
        {
            SetLastError(ERROR_NO_DRIVER_SELECTED);
            goto done;
        }

        ret = SetupDiGetActualSectionToInstallW(
            SelectedDriver->InfFileDetails->hInf,
            SelectedDriver->Details.SectionName,
            SectionName, MAX_PATH, &SectionNameLength, NULL);
        if (!ret)
            goto done;

        if (!InstallParams.InstallMsgHandler)
        {
            InstallParams.InstallMsgHandler = SetupDefaultQueueCallbackW;
            InstallParams.InstallMsgHandlerContext = SetupInitDefaultQueueCallback(InstallParams.hwndParent);
            SetupDiSetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &InstallParams);
        }
        ret = SetupInstallFromInfSectionW(InstallParams.hwndParent,
            SelectedDriver->InfFileDetails->hInf, SectionName,
            SPINST_FILES, NULL, NULL, SP_COPY_NEWER,
            InstallParams.InstallMsgHandler, InstallParams.InstallMsgHandlerContext,
            DeviceInfoSet, DeviceInfoData);
    }

done:
    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiRegisterCoDeviceInstallers (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiRegisterCoDeviceInstallers(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData)
{
    BOOL ret = FALSE; /* Return value */

    TRACE("%p %p\n", DeviceInfoSet, DeviceInfoData);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoSet == (HDEVINFO)INVALID_HANDLE_VALUE)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (((struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (!DeviceInfoData)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
    {
        SP_DEVINSTALL_PARAMS_W InstallParams;
        struct DriverInfoElement *SelectedDriver;
        BOOL Result;
        DWORD DoAction;
        WCHAR SectionName[MAX_PATH];
        DWORD SectionNameLength = 0;
        HKEY hKey = INVALID_HANDLE_VALUE;

        InstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS_W);
        Result = SetupDiGetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &InstallParams);
        if (!Result)
            goto cleanup;

        SelectedDriver = (struct DriverInfoElement *)InstallParams.Reserved;
        if (SelectedDriver == NULL)
        {
            SetLastError(ERROR_NO_DRIVER_SELECTED);
            goto cleanup;
        }

        /* Get .CoInstallers section name */
        Result = SetupDiGetActualSectionToInstallW(
            SelectedDriver->InfFileDetails->hInf,
            SelectedDriver->Details.SectionName,
            SectionName, MAX_PATH, &SectionNameLength, NULL);
        if (!Result || SectionNameLength > MAX_PATH - wcslen(L".CoInstallers") - 1)
            goto cleanup;
        wcscat(SectionName, L".CoInstallers");

        /* Open/Create driver key information */
#if _WIN32_WINNT >= 0x502
        hKey = SetupDiOpenDevRegKey(DeviceInfoSet, DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ | KEY_WRITE);
#else
        hKey = SetupDiOpenDevRegKey(DeviceInfoSet, DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_ALL_ACCESS);
#endif
        if (hKey == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND)
            hKey = SetupDiCreateDevRegKeyW(DeviceInfoSet, DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DRV, NULL, NULL);
        if (hKey == INVALID_HANDLE_VALUE)
            goto cleanup;

        /* Install .CoInstallers section */
        DoAction = SPINST_REGISTRY;
        if (!(InstallParams.Flags & DI_NOFILECOPY))
        {
            DoAction |= SPINST_FILES;
            if (!InstallParams.InstallMsgHandler)
            {
                InstallParams.InstallMsgHandler = SetupDefaultQueueCallbackW;
                InstallParams.InstallMsgHandlerContext = SetupInitDefaultQueueCallback(InstallParams.hwndParent);
                SetupDiSetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &InstallParams);
            }
        }
        Result = SetupInstallFromInfSectionW(InstallParams.hwndParent,
            SelectedDriver->InfFileDetails->hInf, SectionName,
            DoAction, hKey, NULL, SP_COPY_NEWER,
            InstallParams.InstallMsgHandler, InstallParams.InstallMsgHandlerContext,
            DeviceInfoSet, DeviceInfoData);
        if (!Result)
            goto cleanup;

        ret = TRUE;

cleanup:
        if (hKey != INVALID_HANDLE_VALUE)
            RegCloseKey(hKey);
    }

    TRACE("Returning %d\n", ret);
    return ret;
}

/***********************************************************************
 *		SetupDiInstallDeviceInterfaces (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiInstallDeviceInterfaces(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData)
{
    TRACE("%p %p\n", DeviceInfoSet, DeviceInfoData);

    FIXME("SetupDiInstallDeviceInterfaces not implemented. Doing nothing\n");
    //SetLastError(ERROR_GEN_FAILURE);
    //return FALSE;
    return TRUE;
}

BOOL
InfIsFromOEMLocation(
    IN PCWSTR FullName,
    OUT LPBOOL IsOEMLocation)
{
    PWCHAR last;

    last = strrchrW(FullName, '\\');
    if (!last)
    {
        /* No directory specified */
        *IsOEMLocation = FALSE;
    }
    else
    {
        WCHAR Windir[MAX_PATH];
        UINT ret;

        ret = GetWindowsDirectory(Windir, MAX_PATH);
        if (ret == 0 || ret >= MAX_PATH)
        {
            SetLastError(ERROR_GEN_FAILURE);
            return FALSE;
        }

        if (strncmpW(FullName, Windir, last - FullName) == 0)
        {
            /* The path is %SYSTEMROOT%\Inf */
            *IsOEMLocation = FALSE;
        }
        else
        {
            /* The file is in another place */
            *IsOEMLocation = TRUE;
        }
    }
    return TRUE;
}

/***********************************************************************
 *		SetupDiInstallDevice (SETUPAPI.@)
 */
BOOL WINAPI
SetupDiInstallDevice(
    IN HDEVINFO DeviceInfoSet,
    IN PSP_DEVINFO_DATA DeviceInfoData)
{
    struct DeviceInfoElement *DevInfo = (struct DeviceInfoElement *)DeviceInfoData->Reserved;
    SP_DEVINSTALL_PARAMS_W InstallParams;
    struct DriverInfoElement *SelectedDriver;
    SYSTEMTIME DriverDate;
    WCHAR SectionName[MAX_PATH];
    WCHAR Buffer[32];
    DWORD SectionNameLength = 0;
    BOOL Result = FALSE;
    INFCONTEXT ContextService;
    INT Flags;
    ULONG DoAction;
    DWORD RequiredSize;
    LPCWSTR AssociatedService = NULL;
    LPWSTR pSectionName = NULL;
    WCHAR ClassName[MAX_CLASS_NAME_LEN];
    GUID ClassGuid;
    LPWSTR lpGuidString = NULL, lpFullGuidString = NULL;
    BOOL RebootRequired = FALSE;
    HKEY hKey = INVALID_HANDLE_VALUE;
    HKEY hClassKey = INVALID_HANDLE_VALUE;
    BOOL NeedtoCopyFile;
    LARGE_INTEGER fullVersion;
    LONG rc;
    BOOL ret = FALSE; /* Return value */

    TRACE("%p %p\n", DeviceInfoSet, DeviceInfoData);

    if (!DeviceInfoSet)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (DeviceInfoSet == (HDEVINFO)INVALID_HANDLE_VALUE)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (((struct DeviceInfoSet *)DeviceInfoSet)->magic != SETUP_DEV_INFO_SET_MAGIC)
        SetLastError(ERROR_INVALID_HANDLE);
    else if (DeviceInfoData && DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
        SetLastError(ERROR_INVALID_USER_BUFFER);
    else
        Result = TRUE;

    if (!Result)
    {
        /* One parameter is bad */
        goto cleanup;
    }

    InstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS_W);
    Result = SetupDiGetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &InstallParams);
    if (!Result)
        goto cleanup;

    if (InstallParams.FlagsEx & DI_FLAGSEX_SETFAILEDINSTALL)
    {
        /* FIXME: set FAILEDINSTALL in ConfigFlags registry value */
        goto cleanup;
    }

    SelectedDriver = (struct DriverInfoElement *)InstallParams.Reserved;
    if (SelectedDriver == NULL)
    {
        SetLastError(ERROR_NO_DRIVER_SELECTED);
        goto cleanup;
    }

    FileTimeToSystemTime(&SelectedDriver->Info.DriverDate, &DriverDate);

    Result = SetupDiGetActualSectionToInstallW(
        SelectedDriver->InfFileDetails->hInf,
        SelectedDriver->Details.SectionName,
        SectionName, MAX_PATH, &SectionNameLength, NULL);
    if (!Result || SectionNameLength > MAX_PATH - 9)
        goto cleanup;
    pSectionName = &SectionName[wcslen(SectionName)];

    /* Get information from [Version] section */
    if (!SetupDiGetINFClassW(SelectedDriver->Details.InfFileName, &ClassGuid, ClassName, MAX_CLASS_NAME_LEN, &RequiredSize))
        goto cleanup;
    /* Format ClassGuid to a string */
    if (UuidToStringW((UUID*)&ClassGuid, &lpGuidString) != RPC_S_OK)
        goto cleanup;
    RequiredSize = lstrlenW(lpGuidString);
    lpFullGuidString = HeapAlloc(GetProcessHeap(), 0, (RequiredSize + 3) * sizeof(WCHAR));
    if (!lpFullGuidString)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        goto cleanup;
    }
    lpFullGuidString[0] = '{';
    memcpy(&lpFullGuidString[1], lpGuidString, RequiredSize * sizeof(WCHAR));
    lpFullGuidString[RequiredSize + 1] = '}';
    lpFullGuidString[RequiredSize + 2] = '\0';

    /* Open/Create driver key information */
#if _WIN32_WINNT >= 0x502
    hKey = SetupDiOpenDevRegKey(DeviceInfoSet, DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ | KEY_WRITE);
#else
    hKey = SetupDiOpenDevRegKey(DeviceInfoSet, DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_ALL_ACCESS);
#endif
    if (hKey == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND)
        hKey = SetupDiCreateDevRegKeyW(DeviceInfoSet, DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DRV, NULL, NULL);
    if (hKey == INVALID_HANDLE_VALUE)
        goto cleanup;

    /* Install main section */
    DoAction = SPINST_REGISTRY;
    if (!(InstallParams.Flags & DI_NOFILECOPY))
    {
        DoAction |= SPINST_FILES;
        if (!InstallParams.InstallMsgHandler)
        {
            InstallParams.InstallMsgHandler = SetupDefaultQueueCallbackW;
            InstallParams.InstallMsgHandlerContext = SetupInitDefaultQueueCallback(InstallParams.hwndParent);
            SetupDiSetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &InstallParams);
        }
    }
    *pSectionName = '\0';
    Result = SetupInstallFromInfSectionW(InstallParams.hwndParent,
        SelectedDriver->InfFileDetails->hInf, SectionName,
        DoAction, hKey, NULL, SP_COPY_NEWER,
        InstallParams.InstallMsgHandler, InstallParams.InstallMsgHandlerContext,
        DeviceInfoSet, DeviceInfoData);
    if (!Result)
        goto cleanup;
    if (!(InstallParams.Flags & DI_NOFILECOPY) && !(InstallParams.Flags & DI_NOVCP))
    {
        if (Result && InstallParams.InstallMsgHandler == SetupDefaultQueueCallbackW)
        {
            /* Delete resources allocated by SetupInitDefaultQueueCallback */
            SetupTermDefaultQueueCallback(InstallParams.InstallMsgHandlerContext);
        }
    }
    InstallParams.Flags |= DI_NOFILECOPY;
    SetupDiSetDeviceInstallParamsW(DeviceInfoSet, DeviceInfoData, &InstallParams);

    /* Write information to driver key */
    *pSectionName = UNICODE_NULL;
    memcpy(&fullVersion, &SelectedDriver->Info.DriverVersion, sizeof(LARGE_INTEGER));
    TRACE("Write information to driver key\n");
    TRACE("DriverDate      : '%u-%u-%u'\n", DriverDate.wMonth, DriverDate.wDay, DriverDate.wYear);
    TRACE("DriverDesc      : '%S'\n", SelectedDriver->Info.Description);
    TRACE("DriverVersion   : '%u.%u.%u.%u'\n", fullVersion.HighPart >> 16, fullVersion.HighPart & 0xffff, fullVersion.LowPart >> 16, fullVersion.LowPart & 0xffff);
    TRACE("InfPath         : '%S'\n", SelectedDriver->Details.InfFileName);
    TRACE("InfSection      : '%S'\n", SelectedDriver->Details.SectionName);
    TRACE("InfSectionExt   : '%S'\n", &SectionName[wcslen(SelectedDriver->Details.SectionName)]);
    TRACE("MatchingDeviceId: '%S'\n", SelectedDriver->MatchingId);
    TRACE("ProviderName    : '%S'\n", SelectedDriver->Info.ProviderName);
    swprintf(Buffer, L"%u-%u-%u", DriverDate.wMonth, DriverDate.wDay, DriverDate.wYear);
    rc = RegSetValueEx(hKey, L"DriverDate", 0, REG_SZ, (const BYTE *)Buffer, (wcslen(Buffer) + 1) * sizeof(WCHAR));
    if (rc == ERROR_SUCCESS)
        rc = RegSetValueEx(hKey, L"DriverDateData", 0, REG_BINARY, (const BYTE *)&SelectedDriver->Info.DriverDate, sizeof(FILETIME));
    if (rc == ERROR_SUCCESS)
        rc = RegSetValueEx(hKey, L"DriverDesc", 0, REG_SZ, (const BYTE *)SelectedDriver->Info.Description, (wcslen(SelectedDriver->Info.Description) + 1) * sizeof(WCHAR));
    if (rc == ERROR_SUCCESS)
    {
        swprintf(Buffer, L"%u.%u.%u.%u", fullVersion.HighPart >> 16, fullVersion.HighPart & 0xffff, fullVersion.LowPart >> 16, fullVersion.LowPart & 0xffff);
        rc = RegSetValueEx(hKey, L"DriverVersion", 0, REG_SZ, (const BYTE *)Buffer, (wcslen(Buffer) + 1) * sizeof(WCHAR));
    }
    if (rc == ERROR_SUCCESS)
        rc = RegSetValueEx(hKey, L"InfPath", 0, REG_SZ, (const BYTE *)SelectedDriver->Details.InfFileName, (wcslen(SelectedDriver->Details.InfFileName) + 1) * sizeof(WCHAR));
    if (rc == ERROR_SUCCESS)
        rc = RegSetValueEx(hKey, L"InfSection", 0, REG_SZ, (const BYTE *)SelectedDriver->Details.SectionName, (wcslen(SelectedDriver->Details.SectionName) + 1) * sizeof(WCHAR));
    if (rc == ERROR_SUCCESS)
        rc = RegSetValueEx(hKey, L"InfSectionExt", 0, REG_SZ, (const BYTE *)&SectionName[wcslen(SelectedDriver->Details.SectionName)], (wcslen(SectionName) - wcslen(SelectedDriver->Details.SectionName) + 1) * sizeof(WCHAR));
    if (rc == ERROR_SUCCESS)
        rc = RegSetValueEx(hKey, L"MatchingDeviceId", 0, REG_SZ, (const BYTE *)SelectedDriver->MatchingId, (wcslen(SelectedDriver->MatchingId) + 1) * sizeof(WCHAR));
    if (rc == ERROR_SUCCESS)
        rc = RegSetValueEx(hKey, L"ProviderName", 0, REG_SZ, (const BYTE *)SelectedDriver->Info.ProviderName, (wcslen(SelectedDriver->Info.ProviderName) + 1) * sizeof(WCHAR));
    if (rc != ERROR_SUCCESS)
    {
       SetLastError(rc);
       goto cleanup;
    }
    RegCloseKey(hKey);
    hKey = INVALID_HANDLE_VALUE;

    /* FIXME: Process .LogConfigOverride section */

    /* Install .Services section */
    wcscpy(pSectionName, L".Services");
    Result = SetupFindFirstLineW(SelectedDriver->InfFileDetails->hInf, SectionName, NULL, &ContextService);
    while (Result)
    {
        LPWSTR ServiceName = NULL;
        LPWSTR ServiceSection = NULL;

        Result = SetupGetStringFieldW(
            &ContextService,
            1, /* Field index */
            NULL, 0,
            &RequiredSize);
        if (!Result)
            goto nextfile;
        if (RequiredSize > 0)
        {
            /* We got the needed size for the buffer */
            ServiceName = HeapAlloc(GetProcessHeap(), 0, RequiredSize * sizeof(WCHAR));
            if (!ServiceName)
            {
                SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                goto nextfile;
            }
            Result = SetupGetStringFieldW(
                &ContextService,
                1, /* Field index */
                ServiceName, RequiredSize,
                &RequiredSize);
            if (!Result)
                goto nextfile;
        }
        Result = SetupGetIntField(
            &ContextService,
            2, /* Field index */
            &Flags);
        if (!Result)
        {
            /* The field may be empty. Ignore the error */
            Flags = 0;
        }
        Result = SetupGetStringFieldW(
            &ContextService,
            3, /* Field index */
            NULL, 0,
            &RequiredSize);
        if (!Result)
        {
            if (GetLastError() == ERROR_INVALID_PARAMETER)
            {
                /* This first is probably missing. It is not
                 * required, so ignore the error */
                RequiredSize = 0;
                Result = TRUE;
            }
            else
                goto nextfile;
        }
        if (RequiredSize > 0)
        {
            /* We got the needed size for the buffer */
            ServiceSection = HeapAlloc(GetProcessHeap(), 0, RequiredSize * sizeof(WCHAR));
            if (!ServiceSection)
            {
                SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                goto nextfile;
            }
            Result = SetupGetStringFieldW(
                &ContextService,
                3, /* Field index */
                ServiceSection, RequiredSize,
                &RequiredSize);
            if (!Result)
                goto nextfile;

            SetLastError(ERROR_SUCCESS);
            Result = SetupInstallServicesFromInfSectionExW(
                SelectedDriver->InfFileDetails->hInf,
                ServiceSection, Flags, DeviceInfoSet, DeviceInfoData, ServiceName, NULL);
        }
        if (Result && (Flags & SPSVCINST_ASSOCSERVICE))
        {
            AssociatedService = ServiceName;
            ServiceName = NULL;
            if (GetLastError() == ERROR_SUCCESS_REBOOT_REQUIRED)
                RebootRequired = TRUE;
        }
nextfile:
        HeapFree(GetProcessHeap(), 0, ServiceName);
        HeapFree(GetProcessHeap(), 0, ServiceSection);
        if (!Result)
            goto cleanup;
        Result = SetupFindNextLine(&ContextService, &ContextService);
    }

    /* Copy .inf file to Inf\ directory (if needed) */
    Result = InfIsFromOEMLocation(SelectedDriver->InfFileDetails->FullInfFileName, &NeedtoCopyFile);
    if (!Result)
        goto cleanup;
    if (NeedtoCopyFile)
    {
        Result = SetupCopyOEMInfW(
            SelectedDriver->InfFileDetails->FullInfFileName,
            NULL,
            SPOST_NONE,
            SP_COPY_NOOVERWRITE,
            NULL, 0,
            NULL,
            NULL);
        if (!Result)
            goto cleanup;
        /* FIXME: create a new struct InfFileDetails, and set it to SelectedDriver->InfFileDetails,
         * to release use of current InfFile */
    }

    /* Open device registry key */
    hKey = SetupDiOpenDevRegKey(DeviceInfoSet, DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_SET_VALUE);
    if (hKey == INVALID_HANDLE_VALUE)
        goto cleanup;

    /* Install .HW section */
    wcscpy(pSectionName, L".HW");
    Result = SetupInstallFromInfSectionW(InstallParams.hwndParent,
        SelectedDriver->InfFileDetails->hInf, SectionName,
        SPINST_REGISTRY, hKey, NULL, 0,
        InstallParams.InstallMsgHandler, InstallParams.InstallMsgHandlerContext,
        DeviceInfoSet, DeviceInfoData);
    if (!Result)
        goto cleanup;

    /* Write information to enum key */
    TRACE("Write information to enum key\n");
    TRACE("Class           : '%S'\n", ClassName);
    TRACE("ClassGUID       : '%S'\n", lpFullGuidString);
    TRACE("DeviceDesc      : '%S'\n", SelectedDriver->Info.Description);
    TRACE("Mfg             : '%S'\n", SelectedDriver->Info.MfgName);
    TRACE("Service         : '%S'\n", AssociatedService);
    rc = RegSetValueEx(hKey, L"Class", 0, REG_SZ, (const BYTE *)ClassName, (wcslen(ClassName) + 1) * sizeof(WCHAR));
    if (rc == ERROR_SUCCESS)
        rc = RegSetValueEx(hKey, L"ClassGUID", 0, REG_SZ, (const BYTE *)lpFullGuidString, (wcslen(lpFullGuidString) + 1) * sizeof(WCHAR));
    if (rc == ERROR_SUCCESS)
        rc = RegSetValueEx(hKey, L"DeviceDesc", 0, REG_SZ, (const BYTE *)SelectedDriver->Info.Description, (wcslen(SelectedDriver->Info.Description) + 1) * sizeof(WCHAR));
    if (rc == ERROR_SUCCESS)
        rc = RegSetValueEx(hKey, L"Mfg", 0, REG_SZ, (const BYTE *)SelectedDriver->Info.MfgName, (wcslen(SelectedDriver->Info.MfgName) + 1) * sizeof(WCHAR));
    if (rc == ERROR_SUCCESS && *AssociatedService)
        rc = RegSetValueEx(hKey, L"Service", 0, REG_SZ, (const BYTE *)AssociatedService, (wcslen(AssociatedService) + 1) * sizeof(WCHAR));
    if (rc != ERROR_SUCCESS)
    {
       SetLastError(rc);
       goto cleanup;
    }

    /* Start the device */
    if (!RebootRequired && !(InstallParams.Flags & (DI_NEEDRESTART | DI_NEEDREBOOT | DI_DONOTCALLCONFIGMG)))
    {
        PLUGPLAY_CONTROL_RESET_DEVICE_DATA ResetDeviceData;
        NTSTATUS Status;
        RtlInitUnicodeString(&ResetDeviceData.DeviceInstance, DevInfo->DeviceName);
        Status = NtPlugPlayControl(PlugPlayControlResetDevice, &ResetDeviceData, sizeof(PLUGPLAY_CONTROL_RESET_DEVICE_DATA));
        ret = NT_SUCCESS(Status);
    }
    else
        ret = TRUE;

cleanup:
    /* End of installation */
    if (hClassKey != INVALID_HANDLE_VALUE)
        RegCloseKey(hClassKey);
    if (hKey != INVALID_HANDLE_VALUE)
        RegCloseKey(hKey);
    if (lpGuidString)
        RpcStringFreeW(&lpGuidString);
    HeapFree(GetProcessHeap(), 0, (LPWSTR)AssociatedService);
    HeapFree(GetProcessHeap(), 0, lpFullGuidString);

    TRACE("Returning %d\n", ret);
    return ret;
}
