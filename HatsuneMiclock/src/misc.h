#ifndef MISC_H_DEF
#define MISC_H_DEF

#include "winheader.h"
#include <string>


class tCriticalSection {
	CRITICAL_SECTION crit;

public:
	tCriticalSection(){ InitializeCriticalSection( &crit ); }
	~tCriticalSection(){ DeleteCriticalSection( &crit ); }
	void enter(){ EnterCriticalSection( &crit ); }
	void leave(){ LeaveCriticalSection( &crit ); }
};


void WriteUsedMemorySize();

HWND myCreateWindowEx( DWORD dwExStyle, WCHAR*classname, WCHAR*caption, DWORD dwStyle, int x, int y, int w, int h, HWND parent, BOOL havemenu );

BOOL Is_WinXP_SP2_or_Later();

LSTATUS ReadRegistoryDW( HKEY hkey, WCHAR*entry, DWORD*data );
LSTATUS ReadRegistorySTR( HKEY hkey, WCHAR*entry, std::wstring &str );

void OpenURL( std::wstring &url );
void OpenURL( WCHAR*url );


#endif
