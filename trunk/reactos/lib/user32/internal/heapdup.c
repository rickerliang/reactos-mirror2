#include <windows.h>
#include <user32/heapdup.h>

LPVOID HEAP_strdupAtoW(HANDLE  hHeap,DWORD  dwFlags,	LPCSTR lpszAsciiString )
{	
	int i;
	INT len = lstrlenA(lpszAsciiString);
	LPWSTR lpszUnicodeString = HeapAlloc( GetProcessHeap(), 0, (len + 1)*2 );
	for(i=0;i<len;i++)
	 	lpszUnicodeString[i] = lpszAsciiString[i];
	lpszUnicodeString[i] = 0;
	return lpszUnicodeString;
}


//FIXME should use multi byte strings instead

LPVOID HEAP_strdupWtoA(HANDLE  hHeap,DWORD  dwFlags,	LPCWSTR lpszUnicodeString )
{
	int i;
	INT len = lstrlenW(lpszUnicodeString);
	LPSTR lpszAsciiString = HeapAlloc( GetProcessHeap(), 0, (len + 1) );
	for(i=0;i<len;i++)
		lpszAsciiString[i] = lpszUnicodeString[i];
	lpszAsciiString[i] = 0;
	return lpszAsciiString;
}

//FIXME should use multi byte strings instead

LPVOID HEAP_wcsdup(HANDLE  hHeap, DWORD  dwFlags, LPCWSTR lpszUnicodeString )
{
	int i;
	INT len = lstrlenW(lpszUnicodeString);
	LPWSTR lpszString = HeapAlloc( GetProcessHeap(), 0, (len + 1)*2 );
	for(i=0;i<len;i++)
		lpszString[i] = lpszUnicodeString[i];
	lpszString[i] = 0;
	return lpszString;
}

int lstrcpynWtoA( LPSTR ptr1, LPWSTR ptr2, int n )
{
	int i;
	for(i=0;i<n;i++) {
	 	ptr1[i] = ptr2[i];
		if ( ptr1[i] == 0 )
			break;
	}
	return i;
}

int lstrcpynAtoW( LPWSTR ptr1, LPSTR ptr2, int n )
{
	int i;
	for(i=0;i<n;i++) {
	 	ptr1[i] = ptr2[i];
		if ( ptr1[i] == 0 )
			break;
	}
	return i;
}

int lstrcpyWtoA( LPSTR ptr1, LPWSTR ptr2 )
{
	int n = lstrlenW(ptr2);
	return lstrcpynWtoA(ptr1,ptr2,n);
}

int lstrcpyAtoW( LPWSTR ptr1, LPSTR ptr2 )
{
	int n = lstrlenA(ptr2);
	return lstrcpynAtoW(ptr1,ptr2,n);
}

int lpstrncpyA( LPSTR ptr1,LPSTR ptr2, int n)
{
	int i;
	for(i=0;i<n;i++) {
	 	ptr1[i] = ptr2[i];
		if ( ptr1[i] == 0 )
			break;
	}
}
int lpstrncpyW( LPWSTR ptr1,LPWSTR ptr2, int n)
{
	int i;
	for(i=0;i<n;i++) {
	 	ptr1[i] = ptr2[i];
		if ( ptr1[i] == 0 )
			break;
	}
}

LPSTR HEAP_strdupA(LPSTR ptr)
{
	 INT len = lstrlenA(ptr);
	 LPSTR lpszString = HeapAlloc( GetProcessHeap(), 0, (len + 1) );
	 if ( lpszString != NULL )
	 	lstrcpyA(lpszString,ptr);
	 
	 return lpszString;
}
LPWSTR HEAP_strdupW(LPWSTR ptr)
{
	 INT len = lstrlenW(ptr);
	 LPWSTR lpszString = HeapAlloc( GetProcessHeap(), 0, (len + 1)*2 );
	 if ( lpszString != NULL )
	 	lstrcpyW(lpszString,ptr);
	 
	 return lpszString;
}