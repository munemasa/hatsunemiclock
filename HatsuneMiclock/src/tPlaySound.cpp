#include "winheader.h"
#include <string>
#include <process.h>

#include "tPlaySound.h"

#include "debug.h"


static void wrapper_for_destructor(LPVOID v)
{
	wchar_t *file = (wchar_t*)v;
	MCIERROR err;
	std::wstring cmd;
	std::wstring alias;
	wchar_t buf[4096];
	DWORD thid = GetCurrentThreadId();

	alias = L"MediaFile";
	alias.append( _itow(thid,buf,10) );

	cmd = L"open \"";
	cmd.append(file);
	cmd += L"\" type mpegvideo alias " + alias;

	err = mciSendString( cmd.c_str(), buf, 4096, NULL );
	if( err!=0 ){
		dprintf( L"mciSendString open failed %d\n", err );
	}

	cmd = L"play " + alias + L" wait";
	err = mciSendString( cmd.c_str(), buf, 4096, NULL );
	if( err!=0 ){
		dprintf( L"mciSendString play failed %d\n", err );
	}


	cmd = L"stop " + alias;
	err = mciSendString( cmd.c_str(), buf, 4096, NULL );
	if( err!=0 ){
		dprintf( L"mciSendString stop failed %d\n", err );
	}

	cmd = L"close " + alias;
	err = mciSendString( cmd.c_str(), buf, 4096, NULL );
	if( err!=0 ){
		dprintf( L"mciSendString close failed %d\n", err );
	}

	free(v);
}

static void __cdecl thPlaySound(LPVOID v)
{
	wrapper_for_destructor(v);
	_endthread();
}


/** サウンド再生する.
 * 多分、システムが対応していればなんでも再生できると思う。
 * @param file wavでもmp3でもファイル名を渡す.
 */
int tPlaySound( const wchar_t*file )
{
	// 面倒な終了処理を回避するためthread and wait再生で.
	wchar_t *filename = (wchar_t*)calloc( wcslen(file)+1, sizeof(wchar_t) );
	wcscpy( filename, file );
	_beginthread( thPlaySound, 0, (void*)filename );
	return 0;
}
