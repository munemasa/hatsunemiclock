#include "winheader.h"
#include <shlwapi.h>

#include "stringlib.h"
#include "misc.h"
#include "debug.h"

#pragma comment(lib,"shlwapi.lib")


void WriteUsedMemorySize()
{
#ifdef DEBUG

#endif
}

void myListView_GetItemText( HWND h, int i, int isub, std::wstring&str )
{
	WCHAR wbuf[1024];
	ListView_GetItemText( h, i, isub, wbuf, 1024 );
	str = wbuf;
}
void myListView_GetItemText( HWND h, int i, int isub, std::string&str )
{
	std::wstring wstr;
	WCHAR wbuf[1024];
	ListView_GetItemText( h, i, isub, wbuf, 1024 );
	wstr = wbuf;
	wstrtostr( wstr, str );
}



// クライアント領域がw,hになるようにウィンドウを作成する...はずだけどサイズ合わないな.
HWND myCreateWindowEx( DWORD dwExStyle, WCHAR*classname, WCHAR*caption, DWORD dwStyle, int x, int y, int w, int h, HWND parent, BOOL havemenu )
{
    RECT rc;
	rc.top = y;
	rc.bottom = y + h;
	rc.left = x;
	rc.right = x + w;
    AdjustWindowRectEx( &rc, dwStyle, havemenu, dwExStyle );

	return CreateWindowEx( dwExStyle, classname, caption, dwStyle,
		                   x, y, w, h, parent, NULL, NULL, NULL );
}

void OpenURL( std::wstring &url )
{
	HINSTANCE r = ShellExecute(NULL, NULL, url.c_str(), NULL, NULL, SW_SHOWNORMAL);
	if( (int)r<=32 ){
		// 失敗したときはexplorerで.
		dprintf( L"ShellExecute: %d\n", (int)r );
		r = ShellExecute(NULL, L"open", L"explorer", url.c_str(), NULL, SW_SHOWNORMAL );
	}
}

void OpenURL( WCHAR*url )
{
	HINSTANCE r = ShellExecute(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL);
	if( (int)r<=32 ){
		// 失敗したときはexplorerで.
		dprintf( L"ShellExecute: %d\n", (int)r );
		r = ShellExecute(NULL, L"open", L"explorer", url, NULL, SW_SHOWNORMAL );
	}
}


BOOL Is_WinXP_SP2_or_Later()
{
	OSVERSIONINFOEX osvi;
	DWORDLONG dwlConditionMask = 0;
	int op=VER_GREATER_EQUAL;

	// Initialize the OSVERSIONINFOEX structure.
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	osvi.dwMajorVersion = 5;
	osvi.dwMinorVersion = 1;
	osvi.wServicePackMajor = 2;
	osvi.wServicePackMinor = 0;

	// Initialize the condition mask.
	VER_SET_CONDITION( dwlConditionMask, VER_MAJORVERSION, op );
	VER_SET_CONDITION( dwlConditionMask, VER_MINORVERSION, op );
	VER_SET_CONDITION( dwlConditionMask, VER_SERVICEPACKMAJOR, op );
	VER_SET_CONDITION( dwlConditionMask, VER_SERVICEPACKMINOR, op );

	// Perform the test.
	return VerifyVersionInfo(
	&osvi, 
	VER_MAJORVERSION | VER_MINORVERSION | 
	VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
	dwlConditionMask);
}

// key の subkey を削除.
LSTATUS DeleteRegistorySubKey( WCHAR*key, WCHAR*subkey )
{
	HKEY hKey;
	LSTATUS st;
	RegOpenKeyEx( HKEY_CURRENT_USER, key, 0, KEY_ALL_ACCESS, &hKey);
	st = SHDeleteKey(hKey, subkey );
	RegCloseKey(hKey);
	return st;
}


/// レジストリの読み込み.
LSTATUS ReadRegistoryDW( HKEY hkey, WCHAR*entry, DWORD*data )
{
	DWORD type, size;
	type = REG_DWORD;
	size = sizeof(DWORD);
	return RegQueryValueEx( hkey, entry, 0, &type, (BYTE*)data, &size );
}

LSTATUS ReadRegistorySTR( HKEY hkey, WCHAR*entry, std::wstring &str )
{
	LSTATUS r;
	DWORD type, size;

	type = REG_SZ;
	RegQueryValueEx( hkey, entry, 0, &type, NULL, &size );
	if( size==0 ) return !ERROR_SUCCESS;

	// sizeは単位バイトだけどそのまま使用してもサイズ的に困らないし.
	WCHAR *buf = new WCHAR[ size ];
	r = RegQueryValueEx( hkey, entry, 0, &type, (BYTE*)buf, &size );
	str = buf;
	delete [] buf;
	return r;
}
