/*
 * provides new shell item service
 *
 * Copyright 2007 Johannes Anderwald (janderwald@reactos.org)
 * Copyright 2009 Andrew Hill
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

#ifndef _SHV_ITEM_NEW_H_
#define _SHV_ITEM_NEW_H_

class CNewMenu :
	public CComCoClass<CNewMenu, &CLSID_NewMenu>,
	public CComObjectRootEx<CComMultiThreadModelNoCS>,
	public IObjectWithSite,
	public IContextMenu2,
	public IShellExtInit
{
private:
	enum SHELLNEW_TYPE
	{
		SHELLNEW_TYPE_INVALID = -1,
		SHELLNEW_TYPE_COMMAND = 1,
		SHELLNEW_TYPE_DATA = 2,
		SHELLNEW_TYPE_FILENAME = 4,
		SHELLNEW_TYPE_NULLFILE = 8
	};

	struct SHELLNEW_ITEM
	{
		SHELLNEW_TYPE					Type;
		LPWSTR							szExt;
		LPWSTR							szTarget;
		LPWSTR							szDesc;
		LPWSTR							szIcon;
		SHELLNEW_ITEM					*Next;
	};

    LPWSTR szPath;
    SHELLNEW_ITEM *s_SnHead;
	CComPtr<IUnknown>					fSite;
public:
	CNewMenu();
	~CNewMenu();
	SHELLNEW_ITEM *LoadItem(LPWSTR szKeyName);
    void UnloadItem(SHELLNEW_ITEM *item);
	void UnloadShellItems();
	BOOL LoadShellNewItems();
	UINT InsertShellNewItems(HMENU hMenu, UINT idFirst, UINT idMenu);
	HRESULT DoShellNewCmd(LPCMINVOKECOMMANDINFO lpcmi);
	HRESULT DoMeasureItem(HWND hWnd, MEASUREITEMSTRUCT *lpmis);
	HRESULT DoDrawItem(HWND hWnd, DRAWITEMSTRUCT *drawItem);
	void DoNewFolder(IShellView *psv);

	// IObjectWithSite
	virtual HRESULT STDMETHODCALLTYPE SetSite(IUnknown *pUnkSite);
	virtual HRESULT STDMETHODCALLTYPE GetSite(REFIID riid, void **ppvSite);

	// IContextMenu
	virtual HRESULT WINAPI QueryContextMenu(HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags);
	virtual HRESULT WINAPI InvokeCommand(LPCMINVOKECOMMANDINFO lpcmi);
	virtual HRESULT WINAPI GetCommandString(UINT_PTR idCommand,UINT uFlags, UINT *lpReserved, LPSTR lpszName, UINT uMaxNameLen);

	// IContextMenu2
	virtual HRESULT WINAPI HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam);

	// IShellExtInit
	virtual HRESULT STDMETHODCALLTYPE Initialize(LPCITEMIDLIST pidlFolder, IDataObject *pdtobj, HKEY hkeyProgID);

DECLARE_REGISTRY_RESOURCEID(IDR_NEWMENU)
DECLARE_NOT_AGGREGATABLE(CNewMenu)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CNewMenu)
	COM_INTERFACE_ENTRY_IID(IID_IObjectWithSite, IObjectWithSite)
	COM_INTERFACE_ENTRY_IID(IID_IContextMenu2, IContextMenu2)
	COM_INTERFACE_ENTRY_IID(IID_IContextMenu, IContextMenu)
	COM_INTERFACE_ENTRY_IID(IID_IShellExtInit, IShellExtInit)
END_COM_MAP()
};

#endif // _SHV_ITEM_NEW_H_
