//#define _WIN32_WINNT	0x0500
#include "winheader.h"
#include <string>
#include <time.h>
#include <process.h>
#include <gdiplus.h>
using namespace Gdiplus;

#include "define.h"

#include "tWindow.h"
#include "iTunesLib.h"
#include "tNetwork.h"
#include "tNotifyWindow.h"
#include "tListWindow.h"
#include "tPlaySound.h"

#include "niconamaalert.h"
#include "udmessages.h"
#include "stringlib.h"
#include "misc.h"

#include "../resource.h"

#include "debug.h"

#pragma comment(lib,"gdiplus.lib")
#pragma comment(lib,"winmm.lib")		// windows multimedia
#pragma comment(lib,"comctl32.lib")		// common control
#pragma comment(lib,"pcre.lib")

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")




#define BUF_STRING_SIZE					(4096)	// 文字列バッファの最大文字列長(NUL文字込みで)
#define ITUNES_TRACK_ID_BASE			(50000)	// メニュー項目のID(iTunesプレイリスト用)
#define MENU_TRACKLIST_INSERT_POSITION	(2)		// トラックリストメニューの挿入先.


struct T_MIKU_CONFIG {
	DWORD winx, winy;   // ウィンドウ座標.

	DWORD isTopMost;    // 最前面に表示.
	DWORD is12_24;      // 12時間表記かどうか.
	DWORD isTransparent;// 透過表示.
	DWORD trans_rate;	// 透過率 0-255 (0:透明, 255:非透明)

	DWORD speak_type;	// 喋る頻度(0:毎分, 1:毎時, 2:なし)

	std::wstring	nico_notify_sound;
	std::wstring	nico_id;
	std::wstring	nico_password;		// ここのパスワードは常時符号化済みのもので.
	DWORD			nico_autologin;
	std::wstring	nico_matchkeyword;	// キーワード通知.

	// 番組ウィンドウの大きさ.
	DWORD			nico_listwin_x;
	DWORD			nico_listwin_y;
	DWORD			nico_listwin_w;
	DWORD			nico_listwin_h;

}g_config;


struct T_MIKU_CLOCK {
	WNDPROC		pOldWndProc;
	HINSTANCE	hInst;      // アプリケーションのインスタンス.
	HWND		hToolTip;   // ツールチップのウィンドウハンドル.
	std::wstring cmdline;	// コマンドラインオプション.
	ULONG_PTR	gdiplusToken;	// GDI+のトークン.

	tWindow		*pWindow;   // 自身のウィンドウ.

	HANDLE		sayevent;
	DWORD		thid;       // 時刻を話すスレッドID

	HBITMAP     hMiku;	    // ミク画像のハンドル.
	int			w,h;	    // ミク画像のサイズ.

	HBITMAP		hBoard;	    // 時刻板画像のハンドル.
	int			bx,by;	    // 時刻板を書く座標.
	int			bw,bh;      // 時刻板のサイズ.

	HFONT		hFont;	    // 時刻を書くフォント.
	int			fx,fy;	    // 時刻を書く座標.

	int			hour,min,sec;   // 現在時刻.
	int			oldhour,oldmin;

	bool		inDrag;		// ドラッグ中であるか.
	int			dragStartX, dragStartY; // ドラッグを開始した座標.

	bool		inSpeak;	// 現在時刻を喋り中.

	bool		noiTunes;
	IiTunes			*iTunes;        // iTunes COMのインスタンス.
	DWORD			iTunesEndTime;  // iTunesが終了する時刻.
	long			iTunesVolume;   // iTunesのボリューム.
	std::wstring	currentmusic;
	std::wstring	currentartist;

	NicoNamaAlert	*nico_alert;
	tListWindow		*nico_listwin;	// list of niconama broadcast
} g_miku;


DWORD WINAPI thMikuSaysNowTime(LPVOID v);
void SetWindowTitleToMusicName( HWND hwnd );
void UpdateToolTipHelp( WCHAR*str,... );
LRESULT CALLBACK SubWindowProc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam );


void CheckCommandLineOption()
{
	if( g_miku.cmdline.find(L"/noitunes")!=std::wstring::npos ){
		g_miku.noiTunes = true;
	}else{
		g_miku.noiTunes = false;
	}
}


/** 初音ミクロックの初期化.
 * ライブラリ、スレッド、ワークの初期化を行う.
 * WinMainの最初で呼ぶ.
 */
void InitMikuClock()
{
	srand( GetTickCount() );

	// read some parameters from resource.
	ZeroMemory( &g_miku, sizeof(g_miku) );

	WCHAR buf[BUF_STRING_SIZE];
	ZeroMemory( buf, sizeof(buf) );

    // position of text.
	LoadString( GetModuleHandle(NULL), IDS_TEXT_XPOSITION, buf, BUF_STRING_SIZE );
	g_miku.fx = _wtoi(buf);
	LoadString( GetModuleHandle(NULL), IDS_TEXT_YPOSITION, buf, BUF_STRING_SIZE );
	g_miku.fy = _wtoi(buf);

    // position of clock board.
	LoadString( GetModuleHandle(NULL), IDS_BOARD_XPOSITION, buf, BUF_STRING_SIZE );
	g_miku.bx = _wtoi(buf);
	LoadString( GetModuleHandle(NULL), IDS_BOARD_YPOSITION, buf, BUF_STRING_SIZE );
	g_miku.by = _wtoi(buf);

	// speaking event
	g_miku.sayevent = CreateEvent( NULL, FALSE, FALSE, L"Mik39.jp.MikuSayEvent" );

	// speaking thread
	CreateThread( NULL, 0, thMikuSaysNowTime, NULL, 0, &g_miku.thid );

	// COM
	CoInitializeEx( NULL, COINIT_MULTITHREADED | COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );

	// common control
	INITCOMMONCONTROLSEX ic;
	ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
	ic.dwICC = ICC_UPDOWN_CLASS | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&ic);

	WinsockInit();

	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&g_miku.gdiplusToken, &gdiplusStartupInput, NULL);
}

/** 初音ミクロックの後片付け.
 * WinMainの最後で呼ぶ.
 */
void ExitMikuClock()
{
	GdiplusShutdown( g_miku.gdiplusToken );

	WinsockUninit();

	DisconnectITunesEvent();
	SAFE_RELEASE( g_miku.iTunes );

	CloseHandle( g_miku.sayevent );
	CoUninitialize();
}

// アプリケーショントップレベルウィンドウ作成後に呼ぶ.
void RegisterTaskTrayIcon()
{
	NOTIFYICONDATA trayicon;
	ZeroMemory( &trayicon, sizeof(trayicon) );
	trayicon.cbSize = sizeof(trayicon);
	trayicon.hWnd = g_miku.pWindow->getWindowHandle();
	trayicon.uID = 0;
	trayicon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	wcscpy( trayicon.szTip, L"初音ミクロック" );
	trayicon.hIcon = LoadIcon( g_miku.hInst, MAKEINTRESOURCE(IDI_TASKTRAYICON) );
	trayicon.uCallbackMessage = WM_TTRAY_POPUP;
	trayicon.uVersion = NOTIFYICON_VERSION;

	Shell_NotifyIcon( NIM_ADD, &trayicon );
}

void SetTaskTrayBalloonMessage( std::wstring &wstr )
{
	NOTIFYICONDATA icon;
	ZeroMemory( &icon, sizeof(icon) );
	icon.cbSize = sizeof(icon);
	icon.hWnd = g_miku.pWindow->getWindowHandle();
	icon.uVersion = NOTIFYICON_VERSION;
	icon.uFlags = NIF_INFO;

	// wcsncpy( icon.szInfoTitle, wstr.c_str(), 63 );
	wcsncpy( icon.szInfo, wstr.c_str(), 255 );

	icon.uTimeout = 20*1000;
	icon.dwInfoFlags = NIIF_INFO;

	Shell_NotifyIcon( NIM_MODIFY, &icon );
}

// ウィンドウを削除する前に呼ぶ.
void UnregisterTaskTrayIcon()
{
	NOTIFYICONDATA ttray;
	ZeroMemory( &ttray, sizeof(ttray) );
	ttray.cbSize = sizeof(ttray);
	ttray.hWnd = g_miku.pWindow->getWindowHandle();
	Shell_NotifyIcon( NIM_DELETE, &ttray );
}


/** リソースからミク画像をロードする.
 */
void LoadMikuImage()
{
    g_miku.hMiku = (HBITMAP)LoadImage( GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_MIKUBMP), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );
	g_miku.hBoard = (HBITMAP)LoadImage( GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_BOARD), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );

	BITMAP bmp;
	GetObject( g_miku.hMiku, sizeof(bmp), &bmp );
	dprintf( L"w:%d,h:%d\n",bmp.bmWidth, bmp.bmHeight );
	g_miku.w = bmp.bmWidth;
	g_miku.h = bmp.bmHeight;

	GetObject( g_miku.hBoard, sizeof(bmp), &bmp );
	g_miku.bw = bmp.bmWidth;
	g_miku.bh = bmp.bmHeight;
}

/** ウィンドウリージョンを画像の型にする.
 * LoadMikuImage()の後に呼ばなくてはならない.
 * @param hwnd リージョン設定するウィンドウ.
 */
void SetMikuWindowRegion(HWND hwnd)
{
	HDC hdc;
	HDC regiondc;

	hdc = GetDC( hwnd );
	regiondc = CreateCompatibleDC( hdc );
	SelectObject( regiondc, g_miku.hMiku );

	HRGN rgn = CreateRectRgn( 0, 0, 0, 0 );
	for(int y=0; y<g_miku.h; y++){
		for(int x=0; x<g_miku.w; x++){
			COLORREF col;
			col = GetPixel( regiondc, x, y );
			if( col!=0x00ffffff ){
				HRGN r = CreateRectRgn( x, y, x+1, y+1 );
				CombineRgn( rgn, rgn, r, RGN_OR );
				DeleteObject( r );
			}
		}
	}
	DeleteObject( regiondc );

	// add clock board region
	HRGN r = CreateRectRgn( g_miku.bx, g_miku.by, g_miku.bx+g_miku.bw, g_miku.by+g_miku.bh );
	CombineRgn( rgn, rgn, r, RGN_OR );
	DeleteObject( r );

	SetWindowRgn( hwnd, rgn, TRUE );

	ReleaseDC( hwnd, hdc );
}

/** 設定ダイアログの設定項目をレジストリに保存する.
 */
void SaveToRegistory()
{
	HKEY hkey;
	RECT pt;
    if( g_config.trans_rate < 10 ) g_config.trans_rate = 10;
    if( g_config.trans_rate > 255) g_config.trans_rate = 255;

	RegCreateKeyEx( HKEY_CURRENT_USER, REG_SUBKEY, 0, L"", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkey, NULL );

	GetWindowRect( g_miku.pWindow->getWindowHandle(), &pt );
	RegSetValueEx( hkey, L"XPos",    0, REG_DWORD, (BYTE*)&pt.left, sizeof(pt.left) );
	RegSetValueEx( hkey, L"YPos",    0, REG_DWORD, (BYTE*)&pt.top, sizeof(pt.top) );
	RegSetValueEx( hkey, L"TopMost", 0, REG_DWORD, (BYTE*)&g_config.isTopMost, sizeof(g_config.isTopMost) );
	RegSetValueEx( hkey, L"TransparencyRate", 0, REG_DWORD, (BYTE*)&g_config.trans_rate, sizeof(g_config.trans_rate) );
	RegSetValueEx( hkey, L"Transparent", 0, REG_DWORD, (BYTE*)&g_config.isTransparent, sizeof(g_config.isTransparent) );
	RegSetValueEx( hkey, L"DisplayAMPM", 0, REG_DWORD, (BYTE*)&g_config.is12_24, sizeof(g_config.is12_24) );
	RegSetValueEx( hkey, L"SpeakType", 0, REG_DWORD, (BYTE*)&g_config.speak_type, sizeof(g_config.speak_type) );
	RegSetValueEx( hkey, L"NicoNotifySound", 0, REG_SZ, (BYTE*)g_config.nico_notify_sound.c_str(), sizeof(WCHAR)*(g_config.nico_notify_sound.length()+1) );

	if( g_miku.nico_listwin ){
		HWND h = g_miku.nico_listwin->getWindowHandle();
		if( !IsIconic(h) ){
			GetWindowRect( h, &pt );
			RegSetValueEx( hkey, L"NicoListX", 0, REG_DWORD, (BYTE*)&pt.left, sizeof(pt.left) );
			RegSetValueEx( hkey, L"NicoListY", 0, REG_DWORD, (BYTE*)&pt.top, sizeof(pt.top) );
			DWORD tmp;
			tmp = pt.right - pt.left;
			RegSetValueEx( hkey, L"NicoListW", 0, REG_DWORD, (BYTE*)&tmp, sizeof(tmp) );
			tmp = pt.bottom - pt.top;
			RegSetValueEx( hkey, L"NicoListH", 0, REG_DWORD, (BYTE*)&tmp, sizeof(tmp) );
		}
	}

	RegCloseKey( hkey );
}


/** レジストリから設定項目を全てロードする.
 */
void LoadFromRegistory()
{
	HKEY hkey;
	LONG err;

	RegCreateKeyEx( HKEY_CURRENT_USER, REG_SUBKEY, 0, L"", REG_OPTION_NON_VOLATILE, KEY_READ, NULL,	&hkey, NULL );

	err = ReadRegistoryDW( hkey, L"XPos", &g_config.winx );
	if( err!=ERROR_SUCCESS ){
		g_config.winx = 0;
	}

	err = ReadRegistoryDW( hkey, L"YPos", &g_config.winy );
	if( err!=ERROR_SUCCESS ){
		g_config.winy = 0;
	}

	err = ReadRegistoryDW( hkey, L"TopMost", &g_config.isTopMost );
	if( err!=ERROR_SUCCESS ){
		g_config.isTopMost = FALSE;
	}

	err = ReadRegistoryDW( hkey, L"TransparencyRate", &g_config.trans_rate );
	if( err!=ERROR_SUCCESS ){
		g_config.isTransparent = FALSE;
		g_config.trans_rate = 255;
	}

	err = ReadRegistoryDW( hkey, L"Transparent", &g_config.isTransparent );
	if( err!=ERROR_SUCCESS ){
		g_config.isTransparent = FALSE;
	}

	err = ReadRegistoryDW( hkey, L"DisplayAMPM", &g_config.is12_24 );
	if( err!=ERROR_SUCCESS ){
		g_config.is12_24 = FALSE;
	}

	err = ReadRegistoryDW( hkey, L"SpeakType", &g_config.speak_type );
	if( err!=ERROR_SUCCESS ){
		g_config.speak_type = 0;
	}

	err = ReadRegistoryDW( hkey, L"NicoAutoLogin", &g_config.nico_autologin );
	if( err!=ERROR_SUCCESS ){
		g_config.nico_autologin = false;
	}

	err = ReadRegistorySTR( hkey, L"NicoEMail", g_config.nico_id );
	if( err!=ERROR_SUCCESS ){
		g_config.nico_id = L"";
	}

	err = ReadRegistorySTR( hkey, L"NicoPassword", g_config.nico_password );
	if( err!=ERROR_SUCCESS ){
		g_config.nico_password = L"";
	}

	err = ReadRegistorySTR( hkey, L"NicoNotifySound", g_config.nico_notify_sound );
	if( err!=ERROR_SUCCESS ){
		g_config.nico_notify_sound = L"nc11846.mp3";
	}

	err = ReadRegistoryDW( hkey, L"NicoListX", &g_config.nico_listwin_x);
	if( err!=ERROR_SUCCESS ){
		g_config.nico_listwin_x = 0;
	}
	err = ReadRegistoryDW( hkey, L"NicoListY", &g_config.nico_listwin_y );
	if( err!=ERROR_SUCCESS ){
		g_config.nico_listwin_y = 0;
	}
	err = ReadRegistoryDW( hkey, L"NicoListW", &g_config.nico_listwin_w );
	if( err!=ERROR_SUCCESS ){
		g_config.nico_listwin_w = 640;
	}
	err = ReadRegistoryDW( hkey, L"NicoListH", &g_config.nico_listwin_h );
	if( err!=ERROR_SUCCESS ){
		g_config.nico_listwin_h = 480;
	}

	err = ReadRegistorySTR( hkey, L"NicoMatchKeyword", g_config.nico_matchkeyword );
	if( err!=ERROR_SUCCESS ){
		g_config.nico_matchkeyword = L"";
	}

	RegCloseKey( hkey );

    if( g_config.trans_rate < 10 ) g_config.trans_rate = 10;
    if( g_config.trans_rate > 255) g_config.trans_rate = 255;
	if( (int)g_config.winx < -g_miku.w ) g_config.winx = 0;
	if( (int)g_config.winy < -g_miku.h ) g_config.winy = 0;
}

/** ミクを描く.
 * 時計も描く.
 * どんな環境でも動くようにひたすらBitBlt.
 * @param hdc 描画先のデバイスコンテキスト
 */
void DrawMiku(HDC hdc)
{
	HDC dc;

	// create device
	dc = CreateCompatibleDC( hdc );
	// draw Miku
	SelectObject( dc, g_miku.hMiku );
	BitBlt( hdc, 0, 0, g_miku.w, g_miku.h, dc, 0, 0, SRCCOPY );

	// draw clock board
	SelectObject(dc, g_miku.hBoard);
	BitBlt( hdc, g_miku.bx, g_miku.by, g_miku.bw, g_miku.bh, dc, 0, 0, SRCCOPY );
	// delete device
	DeleteDC( dc );

	WCHAR timestr[BUF_STRING_SIZE];
	if( g_config.is12_24 ){
		wsprintf(timestr, L"%02d:%02d:%02d", g_miku.hour % 12, g_miku.min, g_miku.sec);
	}else{
		wsprintf(timestr, L"%02d:%02d:%02d", g_miku.hour, g_miku.min, g_miku.sec);
	}
	SetBkMode(hdc, TRANSPARENT);
	TextOut(hdc, g_miku.fx, g_miku.fy, timestr, wcslen(timestr) );
}

/** iTunesを探して接続する.
 * 時刻更新処理と一緒に呼ばれている.
 * @param hwnd iTunesEventを受けとるウィンドウ
 */
void FindITunes( HWND hwnd )
{
	if( g_miku.noiTunes ) return;

	if( g_miku.iTunes==NULL && FindITunes() ){
        // 終了中のiTunesを見つけないようにテキトーな手段で弾く.
		if( GetTickCount() - g_miku.iTunesEndTime < 10*1000 ) return;
		CreateITunesCOM( &g_miku.iTunes, hwnd );
		ConnectITunesEvent( g_miku.iTunes );
		SetWindowTitleToMusicName( hwnd );
	}
}


/** 時刻ワークの更新.
 */
void UpdateMikuClock()
{
	SYSTEMTIME t;
	GetLocalTime(&t);
	g_miku.oldhour = g_miku.hour;
	g_miku.oldmin = g_miku.min;
	g_miku.hour = t.wHour;
	g_miku.min = t.wMinute;
	g_miku.sec = t.wSecond;
}

/** 音声再生スレッド.
 * ここでのCRTは使用禁止.
 */
DWORD WINAPI thMikuSaysNowTime(LPVOID v)
{
	int hour;
	int min;
	static const int harray[24] = {
		IDR_HOUR00, IDR_HOUR01, IDR_HOUR02, IDR_HOUR03, IDR_HOUR04, IDR_HOUR05,
		IDR_HOUR06,	IDR_HOUR07, IDR_HOUR08, IDR_HOUR09,	IDR_HOUR10, IDR_HOUR11,
		IDR_HOUR12, IDR_HOUR13,	IDR_HOUR14, IDR_HOUR15, IDR_HOUR16, IDR_HOUR17,
		IDR_HOUR18, IDR_HOUR19,	IDR_HOUR20, IDR_HOUR21, IDR_HOUR22, IDR_HOUR23,
	};
	static const int marray[60] = {
		IDR_MIN00, IDR_MIN01, IDR_MIN02, IDR_MIN03, IDR_MIN04, IDR_MIN05, IDR_MIN06, IDR_MIN07, IDR_MIN08, IDR_MIN09,
		IDR_MIN10, IDR_MIN11, IDR_MIN12, IDR_MIN13, IDR_MIN14, IDR_MIN15, IDR_MIN16, IDR_MIN17, IDR_MIN18, IDR_MIN19,
		IDR_MIN20, IDR_MIN21, IDR_MIN22, IDR_MIN23, IDR_MIN24, IDR_MIN25, IDR_MIN26, IDR_MIN27, IDR_MIN28, IDR_MIN29,
		IDR_MIN30, IDR_MIN31, IDR_MIN32, IDR_MIN33, IDR_MIN34, IDR_MIN35, IDR_MIN36, IDR_MIN37, IDR_MIN38, IDR_MIN39,
		IDR_MIN40, IDR_MIN41, IDR_MIN42, IDR_MIN43, IDR_MIN44, IDR_MIN45, IDR_MIN46, IDR_MIN47, IDR_MIN48, IDR_MIN49,
		IDR_MIN50, IDR_MIN51, IDR_MIN52, IDR_MIN53, IDR_MIN54, IDR_MIN55, IDR_MIN56, IDR_MIN57, IDR_MIN58, IDR_MIN59,
	};

	while(1){
		WaitForSingleObject( g_miku.sayevent, INFINITE );

		hour = g_miku.hour;
		min = g_miku.min;
		if( hour>=0 && hour<24 && min>=0 && min<60 ){
			g_miku.inSpeak = true;
			if( g_config.is12_24 ){
				hour %= 12;
			}
			PlaySound( MAKEINTRESOURCE(harray[hour]), GetModuleHandle(NULL), SND_RESOURCE|SND_NOSTOP );
			PlaySound( MAKEINTRESOURCE(marray[min]), GetModuleHandle(NULL), SND_RESOURCE|SND_NOSTOP );
			g_miku.inSpeak = false;
		}
	}
	ExitThread(0);
	return 0;
}

/** 現在時刻を喋らせる.
 */
void SpeakMiku()
{
	if( !g_miku.inSpeak ){
		SetEvent( g_miku.sayevent );
	}
}

/** 現在時刻を喋らせるかどうかチェックする.
 */
void CheckMikuSpeak(HWND hwnd)
{
	if( g_miku.oldhour==0 && g_miku.oldmin==0 ) return;

    bool b = false;

    switch( g_config.speak_type ){
    case 0: // every minute
        b = true;
        break;
    case 1: // every hour
        if( g_miku.min==0 ) b = true;
        break;
    default:
        break;
    }

	if( g_miku.oldmin != g_miku.min && b ) {
        if( !g_miku.inSpeak ){
			SetEvent( g_miku.sayevent );
        }
	}
}

/** iTunesで現在再生中の曲名を取得.
 * @param out 曲名を渡す.
 * @param n outの配列の要素数.
 */
WCHAR*GetCurrentTrackName( WCHAR*out, int n )
{
	if( g_miku.iTunes ){
		IITTrack *iTrack;
		g_miku.iTunes->get_CurrentTrack( &iTrack );
		if( iTrack ){
			BSTR name;
			iTrack->get_Name( &name );
			ZeroMemory( out, sizeof(WCHAR)*n );
			wcsncpy( out, name, n-1 );
			SAFE_RELEASE( iTrack );
            return out;
		}
	}
	return NULL;
}

/** 現在再生中の曲インデックスを取得.
 */
long GetCurrentTrackPlaylistIndex( long *id )
{
    *id = -1;
	if( g_miku.iTunes ){
		IITTrack *iTrack;
		g_miku.iTunes->get_CurrentTrack( &iTrack );
		if( iTrack ){
			iTrack->get_Index( id );
			SAFE_RELEASE( iTrack );
		}
	}
	return *id;
}

/** ポップアップメニューを表示する.
 * @param hwnd 親ウィンドウ
 */
LRESULT ShowPopupMenu(HWND hwnd)
{
	// メニューは同時に複数表示されないので、static型で確保しちゃおう.
	static std::list<NicoNamaProgram> recentniconama;

	HMENU menu;
    HMENU submenu;
	HMENU hRecentBroadcasting;	// ニコ生アラート通知履歴.
    POINT pt;
	MENUITEMINFO iteminfo;
	std::wstring menuitemname;

    menu = LoadMenu( GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_CONTEXTMENU) );
    submenu = GetSubMenu( menu, 0 ); 
    SetMenuDefaultItem( submenu, ID_BTN_WHATTIMEISITNOW, 0 );

	ZeroMemory( &iteminfo, sizeof(iteminfo) );
	iteminfo.cbSize = sizeof(iteminfo);

	// トラックリストメニューを追加.
	// あらかじめリソースで用意してあるとサブメニューのビジュアルスタイルが旧型っぽくなるので.
	HMENU tracklistmenu = CreatePopupMenu();
	iteminfo.fMask = MIIM_STRING | MIIM_SUBMENU;

	menuitemname = GetStringFromResource( IDS_MENU_TRACKLIST );
	iteminfo.dwTypeData = (LPWSTR)menuitemname.c_str();
	iteminfo.cch = (UINT)menuitemname.length();
    iteminfo.hSubMenu = tracklistmenu;

	WCHAR currenttrackname[1024];
    ZeroMemory( currenttrackname, sizeof(currenttrackname) );
    if( GetCurrentTrackName( currenttrackname, 1024 ) ){
		WCHAR buf[BUF_STRING_SIZE];
		wsprintf( buf, L"%s", currenttrackname );
        iteminfo.dwTypeData = buf;
        iteminfo.cch = (UINT)wcslen( buf );
    }
    InsertMenuItem( submenu, MENU_TRACKLIST_INSERT_POSITION, TRUE, &iteminfo );

	// 最近のニコ生放送リストを追加.
	if( g_miku.nico_alert ){
		recentniconama = g_miku.nico_alert->getRecentList();

		hRecentBroadcasting = CreatePopupMenu();
		std::list<NicoNamaProgram>::iterator it;
		int i;
		for( i=1,it=recentniconama.begin(); it!=recentniconama.end(); it++,i++){
			std::wstring wstr;
			iteminfo.fMask = MIIM_ID | MIIM_STRING | MIIM_DATA;
			iteminfo.wID = i;
			iteminfo.dwItemData = (ULONG_PTR)&(*it);	// NicoNamaProgramへのポインタ入れておこう.

			struct tm * starttime = localtime( &(*it).start );
			wchar_t timebuf[1024];
			strtowstr( (*it).title, wstr );
			wsprintf( timebuf, L" (%d:%02d)", starttime->tm_hour, starttime->tm_min );
			wstr = wstr + timebuf;
			iteminfo.dwTypeData = (LPWSTR)wstr.c_str();
			iteminfo.cch = (UINT)wstr.length();
			InsertMenuItem( hRecentBroadcasting, 0, TRUE, &iteminfo );
		}

		menuitemname = GetStringFromResource( IDS_MENU_NICO_RECENT );
		iteminfo.fMask = MIIM_STRING | MIIM_SUBMENU;
		iteminfo.hSubMenu = hRecentBroadcasting;
		iteminfo.dwTypeData = (LPWSTR)menuitemname.c_str();
		iteminfo.cch = (UINT)menuitemname.length();
		InsertMenuItem( submenu, MENU_TRACKLIST_INSERT_POSITION+1, TRUE, &iteminfo );
	}

	GetCursorPos( &pt );

	int r;
    r = TrackPopupMenuEx( submenu, TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, hwnd, NULL );
	/* WM_COMMANDからではメニューアイテムに関連付けしたデータを取得できないので、
	 * TPM_RETURNCMDを指定して、こちらで処理するように。
	 * 今までWM_COMMANDで処理していたものを作り直すのも面倒なので、
	 * WM_COMMANDをPostMessage()する。
	 */
	if(r){
		PostMessage( hwnd, WM_COMMAND, r, 0 );
	}
	if( r>0 && r<=NICO_MAX_RECENT_PROGRAM ){
		/* recentniconamaに直接触ってもいいような気もするけど、
		 * listだとインデックスから直に項目に飛べないから、
		 * アイテムからデータ取ってこよう。
		 */
		iteminfo.cbSize = sizeof(iteminfo);
		iteminfo.fMask = MIIM_DATA;
		GetMenuItemInfo( hRecentBroadcasting, r, FALSE, &iteminfo );
		NicoNamaProgram *prog = (NicoNamaProgram*)iteminfo.dwItemData;
		dprintf( L"Select %S(%S)\n", prog->title.c_str(), prog->request_id.c_str() );

		std::wstring url = NICO_LIVE_URL;
		std::wstring tmp;
		strtowstr( prog->request_id, tmp );
		url += tmp;
		ShellExecute(NULL, NULL, url.c_str(), NULL, NULL, SW_SHOWNORMAL);
	}

	DestroyMenu( menu );
	return 0;
}


/** ウィンドウタイトルを再生中の曲名に変更する.
 * iTunesで再生開始されたときに呼び出される。
 * @param hwnd 変更するウィンドウ
 */
void SetWindowTitleToMusicName( HWND hwnd )
{
	g_miku.currentartist = L"";
	g_miku.currentmusic = L"";
    if( g_miku.iTunes ){
        IITTrack *iTrack;
        ITPlayerState playerState;

        g_miku.iTunes->get_PlayerState( &playerState );
		if( playerState==ITPlayerStateStopped )	return;	// 再生していない.

        g_miku.iTunes->get_CurrentTrack( &iTrack );
        if( iTrack ){
            BSTR name, artist;
            WCHAR windowname[BUF_STRING_SIZE];

			// update window title.
            iTrack->get_Name( &name );
			if( name ){
				g_miku.currentmusic = name;
				wsprintf( windowname, L"%s::%s", name, APP_TITLE );
				SetWindowText( hwnd, windowname );
			}else{
				// 曲名がないとな.
				SetWindowText( hwnd, APP_TITLE );
				g_miku.currentmusic = L"";
			}

            // update tooltip help message.
			iTrack->get_Artist( &artist );
			if( artist ){
				g_miku.currentartist = artist;
				UpdateToolTipHelp( L"%s (%s)", g_miku.currentmusic.c_str(), artist );
			}else{
				UpdateToolTipHelp( L"%s", g_miku.currentmusic.c_str() );
				g_miku.currentartist = L"";
			}

			SAFE_RELEASE( iTrack );
        }
    }
}


/** ツールチップを作成する.
 * @param hwnd 親ウィンドウハンドル
 */
void CreateToolTipHelp( HWND hwnd )
{
	DWORD dwStyle = TTS_ALWAYSTIP | TTS_NOPREFIX | TTS_BALLOON;
    g_miku.hToolTip = CreateWindowEx( 0, TOOLTIPS_CLASS, L"", dwStyle,
                                      0, 0, 100,100, hwnd, NULL, g_miku.hInst, NULL );
    TOOLINFO ti;
    ZeroMemory( &ti, sizeof(TOOLINFO) );
    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = g_miku.hToolTip;
    ti.hinst = g_miku.hInst;
    ti.lpszText = L"";
    ti.uId = (UINT_PTR)hwnd;
    ti.rect.left = 0;
    ti.rect.right = 100;
    ti.rect.top = 0;
    ti.rect.bottom = 32;
    SendMessage( g_miku.hToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti );
    SendMessage( g_miku.hToolTip, TTM_SETDELAYTIME, TTDT_INITIAL, (LPARAM)MAKELONG(100, 0) );
}


/** ツールチップヘルプメッセージを更新する.
 */
void UpdateToolTipHelp( WCHAR*str,... )
{
	va_list list;
	va_start( list, str );
	int r;
	WCHAR buf[8192];
	r = wvsprintf(buf, str, list);
    va_end( list );

	LRESULT lResult;
    TOOLINFO ti;
    ZeroMemory( &ti, sizeof(TOOLINFO) );
    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = g_miku.hToolTip;
    ti.hinst = g_miku.hInst;
    ti.lpszText = buf;
    ti.uId = (UINT_PTR)g_miku.pWindow->getWindowHandle();
    ti.rect.left = 0;
    ti.rect.right = 100;
    ti.rect.top = 0;
    ti.rect.bottom = 32;
    lResult = SendMessage( (HWND)g_miku.hToolTip, (UINT)TTM_UPDATETIPTEXT, (WPARAM)0, (LPARAM)&ti );
}

/// 再生とポーズを切り替える.
LRESULT iTunesPlayOrStop()
{
    if( g_miku.iTunes ){
        ITPlayerState playerState;
        g_miku.iTunes->get_PlayerState( &playerState );
        if( playerState==ITPlayerStateStopped ){
            g_miku.iTunes->Play();
        }else{
            g_miku.iTunes->Pause();
        }
    }
	return 0;
}

/** ニコ生アラートを表示する.
 * @param program 放送についてのの情報(アラート表示中、保持する必要ないので即捨てて良し)
 */
void CreateNicoNamaNotify( NicoNamaProgram*program )
{
	tNotifyWindow *notifywin = new tNotifyWindow( g_miku.hInst, g_miku.pWindow->getWindowHandle() );
	dprintf( L"Create tNotifyWindow %p\n", notifywin );

	std::wstring str;

	std::string s = program->community + " - " + program->community_name;
	strtowstr( s, str );
	notifywin->SetWindowTitle( str );

	strtowstr( program->title, str );
	notifywin->SetTitle( str );

	strtowstr( program->description, str );
	notifywin->SetDescription( str );

	notifywin->SetThumbnail( program->thumbnail_image, program->thumbnail_image_size ); 

	std::wstring tmp;
	strtowstr( program->request_id, tmp );
	str = NICO_LIVE_URL + tmp;
	notifywin->SetLiveURL( str );

	strtowstr( program->community, tmp );
    str = tmp.find( L"ch",0 )!=std::wstring::npos ? NICO_CHANNEL_URL : NICO_COMMUNITY_URL;
    str += tmp;
	notifywin->SetCommunityURL( str );
	notifywin->setSoundFile( g_config.nico_notify_sound );
	notifywin->Show( program->playsound, program->posx, program->posy );
}

/** ニコ生アラートからの通知処理.
 */
LRESULT OnNicoNamaNotify( HWND hwnd, WPARAM wparam, LPARAM lparam )
{
	switch( wparam ){
	case NNA_REQUEST_CREATEWINDOW:
		CreateNicoNamaNotify( (NicoNamaProgram*)lparam );
		break;

	case NNA_CLOSED_NOTIFYWINDOW:
		if( g_miku.nico_alert ) g_miku.nico_alert->ShowNextNoticeWindow();
		break;

	case NNA_UPDATE_RSS:
		g_miku.nico_listwin->SetBoadcastingList( g_miku.nico_alert->getRSSProgram() );
		break;

	default:
		break;
	}
	return 0;
}

/** iTunesイベントの処理.
 */
LRESULT OnITunesEvent( HWND hwnd, WPARAM wparam, LPARAM lparam )
{
    switch( wparam ){
    case ITEventDatabaseChanged:
		dprintf( L"ITEventDatabaseChanged\n" );
        break;

    case ITEventPlayerPlay:
		dprintf( L"ITEventPlayerPlay\n" );
        SetWindowTitleToMusicName( hwnd );
        break;

    case ITEventPlayerStop:
		dprintf( L"ITEventPlayerStop\n" );
        SetWindowText( hwnd, APP_TITLE );
        UpdateToolTipHelp(L"");
        break;

    case ITEventPlayerPlayingTrackChanged:
		dprintf( L"ITEventPlayerPlayingTrackChanged\n" );
        break;

	case ITEventUserInterfaceEnabled:
		dprintf( L"ITEventUserInterfaceEnabled\n" );
		break;

    case ITEventCOMCallsDisabled:
        g_miku.iTunesEndTime = GetTickCount();

        dprintf( L"ITEventCOMCallsDisabled\n" );
        SetWindowText( hwnd, APP_TITLE );
        UpdateToolTipHelp(L"");

        DisconnectITunesEvent();
		SAFE_RELEASE( g_miku.iTunes );
        break;

    case ITEventCOMCallsEnabled:
        dprintf( L"ITEventCOMCallsEnabled\n" );
        break;

    case ITEventQuitting:
		dprintf( L"ITEventQuitting\n" );
        break;

    case ITEventAboutToPromptUserToQuit:
		dprintf( L"ITEventAboutToPromptUserToQuit\n" );
        break;

    case ITEventSoundVolumeChanged:
		dprintf( L"ITEventSoundVolumeChanged\n" );
        break;

    default:
        break;
    }
	return 0;
}

/** 設定ダイアログに現状を反映させる.
 * @param hwnd ダイアログのウィンドウハンドル
 */
void SetupSettingDialog( HWND hwnd )
{
    HWND parts;
    parts = GetDlgItem( hwnd, IDC_CHK_TOPMOST );
    Button_SetCheck( parts, g_config.isTopMost );

    parts = GetDlgItem( hwnd, IDC_CHK_AMPM );
    Button_SetCheck( parts, g_config.is12_24 );

    parts = GetDlgItem( hwnd, IDC_CHK_TRANSPARENCY );
    Button_SetCheck( parts, g_config.isTransparent );

    // display alpha value, convert to 0-100%
    WCHAR wstr[BUF_STRING_SIZE];
    int rate;
    rate = g_config.trans_rate * 100 / 255;
    wsprintf(wstr, L"%d", rate );
    SetDlgItemText( hwnd, IDC_EDIT_TRANSPARENCY, wstr );

	SetDlgItemText( hwnd, IDC_SOUNDFILE, g_config.nico_notify_sound.c_str() );

    HWND radio[3];
    radio[0] = GetDlgItem( hwnd, IDC_SPEAK_EVERYMIN );
    radio[1] = GetDlgItem( hwnd, IDC_SPEAK_EVERYHOUR );
    radio[2] = GetDlgItem( hwnd, IDC_NOSPEAK );
    Button_SetCheck( radio[0], 0 );
    Button_SetCheck( radio[1], 0 );
    Button_SetCheck( radio[2], 0 );
    switch(g_config.speak_type){
    case 0: // every min.
    default:
        Button_SetCheck( radio[0], 1 );
        break;
    case 1: // every hour.
        Button_SetCheck( radio[1], 1 );
        break;
    case 2: //no speaking
        Button_SetCheck( radio[2], 1 );
        break;
    }
}

/** 設定を適用する.
 * 設定ダイアログの内容をconfigに反映させる
 * @param hwnd ダイアログのウィンドウハンドル
 */
void ApplySetting(HWND hwnd)
{
    HWND parts;

    // check TopMost setting
    if( IsDlgButtonChecked( hwnd, IDC_CHK_TOPMOST )==BST_CHECKED ){
        g_miku.pWindow->setTopMost(true);
        g_config.isTopMost = TRUE;
    }else{
        g_miku.pWindow->setTopMost(false);
        g_config.isTopMost = FALSE;
    }

    // calculate alpha value
    int tmp;
    WCHAR buf[BUF_STRING_SIZE];
    GetDlgItemText( hwnd, IDC_EDIT_TRANSPARENCY, buf, BUF_STRING_SIZE );
    tmp = _wtoi( buf );
    tmp = 256 * tmp / 100;
    if( tmp > 255 ) tmp = 255;

	// notify sound file
	GetDlgItemText( hwnd, IDC_SOUNDFILE, buf, BUF_STRING_SIZE );
	g_config.nico_notify_sound = buf;

    // check transparency
    if( IsDlgButtonChecked( hwnd, IDC_CHK_TRANSPARENCY )==BST_CHECKED ){
        g_miku.pWindow->setTransparency(tmp);
        g_config.isTransparent = TRUE;
        g_config.trans_rate = tmp;
    }else{
        g_miku.pWindow->setTransparency(255);
        g_config.isTransparent = FALSE;
        g_config.trans_rate = tmp;
    }

    parts = GetDlgItem( hwnd, IDC_CHK_AMPM );
    g_config.is12_24 = Button_GetCheck( parts );

    g_config.speak_type = 0;
    parts = GetDlgItem( hwnd, IDC_SPEAK_EVERYMIN );
    if( Button_GetCheck( parts ) ){
        g_config.speak_type = 0;
    }
    parts = GetDlgItem( hwnd, IDC_SPEAK_EVERYHOUR );
    if( Button_GetCheck( parts ) ){
        g_config.speak_type = 1;
    }
    parts = GetDlgItem( hwnd, IDC_NOSPEAK );
    if( Button_GetCheck( parts ) ){
        g_config.speak_type = 2;
    }

    SaveToRegistory();
}
void SaveNicoIDPassword()
{
	HKEY hkey;
	RegCreateKeyEx( HKEY_CURRENT_USER, REG_SUBKEY, 0, L"", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkey, NULL );
	RegSetValueEx( hkey, L"NicoEMail",    0, REG_SZ, (BYTE*)g_config.nico_id.c_str(),       sizeof(WCHAR)*(g_config.nico_id.length()+1) );
	RegSetValueEx( hkey, L"NicoPassword", 0, REG_SZ, (BYTE*)g_config.nico_password.c_str(), sizeof(WCHAR)*(g_config.nico_password.length()+1) );
	RegSetValueEx( hkey, L"NicoAutoLogin", 0, REG_DWORD, (BYTE*)&g_config.nico_autologin, sizeof(g_config.nico_autologin) );
	RegCloseKey( hkey );
	dprintf( L"Niconico account information was saved.\n" );
}

INT_PTR CALLBACK DlgNicoIDPASSProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	HWND parts;
	switch(msg){
    case WM_INITDIALOG:
		parts = GetDlgItem( hDlgWnd, IDC_REMEMBERME );
		Button_SetCheck( parts, g_config.nico_autologin );
		SetDlgItemText( hDlgWnd, IDC_EDIT_NOCO_EMAIL, g_config.nico_id.c_str() );
		{
			std::wstring buf = g_config.nico_password;
			rot47( buf );	// decode
			SetDlgItemText( hDlgWnd, IDC_NICO_PASSWORD, buf.c_str() );
		}
        return TRUE;

	case WM_COMMAND:
        switch(LOWORD(wp)){
		case IDOK:{
			WCHAR buf[BUF_STRING_SIZE];
			GetDlgItemText( hDlgWnd, IDC_EDIT_NOCO_EMAIL, buf, BUF_STRING_SIZE );
			g_config.nico_id = buf;
			GetDlgItemText( hDlgWnd, IDC_NICO_PASSWORD, buf, BUF_STRING_SIZE );
			{
				std::wstring wbuf = buf;
				rot47( wbuf );	// encode
				g_config.nico_password = wbuf;
			}
			g_config.nico_autologin = IsDlgButtonChecked( hDlgWnd, IDC_REMEMBERME )==BST_CHECKED;

			if( IsDlgButtonChecked( hDlgWnd, IDC_CHK_NICOIDPASS_SAVE )==BST_CHECKED ){
				// 保存するにチェックがある.
				SaveNicoIDPassword();
			}
            EndDialog(hDlgWnd, IDOK);
			break;}
		case IDCANCEL:
			EndDialog(hDlgWnd, IDCANCEL);
			break;

		default:
            return FALSE;
        }
		return TRUE;

	case WM_KEYDOWN:
		if( wp==VK_RETURN ){
	        PostMessage( hDlgWnd, WM_COMMAND, IDOK, 0 );
		}
		break;

	case WM_CLOSE:
        PostMessage( hDlgWnd, WM_COMMAND, IDCANCEL, 0 );
        return TRUE;

	default:
		return FALSE;
	}
	return TRUE;
}

INT_PTR CALLBACK DlgAboutProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg){
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        switch(LOWORD(wp)){
        case IDYES:
            EndDialog(hDlgWnd, IDOK);
            break;
        default:
            return FALSE;
        }
        break;
    case WM_CLOSE:
        PostMessage( hDlgWnd, WM_COMMAND, IDYES, 0 );
        return TRUE;

    default:
        return FALSE;
    }
    return TRUE;
}


INT_PTR CALLBACK DlgSettingProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg){
    case WM_INITDIALOG:
        SetupSettingDialog( hDlgWnd );
        return TRUE;

    case WM_COMMAND:
        switch(LOWORD(wp)){
        case IDYES:
            ApplySetting( hDlgWnd );
            EndDialog( hDlgWnd, IDOK );
            break;
        case IDCANCEL:
            EndDialog( hDlgWnd, IDOK );
            break;

		case IDC_BTN_SELECT_SNDFILE:{
			OPENFILENAME openfile;
			ZeroMemory( &openfile, sizeof(openfile) );
			openfile.lStructSize = sizeof(openfile);
			openfile.hwndOwner = hDlgWnd;
			openfile.lpstrFilter = L"MP3,WAV\0*.mp3;*.wav\0\0";

			WCHAR buf[BUF_STRING_SIZE];
			GetDlgItemText( hDlgWnd, IDC_SOUNDFILE, buf, BUF_STRING_SIZE );
			openfile.lpstrFile = buf;
			openfile.nMaxFile = BUF_STRING_SIZE;
			openfile.Flags = OFN_FILEMUSTEXIST | OFN_LONGNAMES;

			if( GetOpenFileName( &openfile ) ){
				dprintf( L"select: %s\n", buf );
				SetDlgItemText( hDlgWnd, IDC_SOUNDFILE, buf );
			}else{
				dprintf( L"You don't select the file\n" );
			}
			return TRUE;
			break;}

        default:
            break;
        }
        return FALSE;
        break;

    case WM_CLOSE:
        PostMessage( hDlgWnd, WM_COMMAND, IDCANCEL, 0 );
        return TRUE;

    default:
        return FALSE;
    }
    return TRUE;
}

/** ニコ生のコメントサーバに接続する.
 */
void NicoNamaLogin( HWND hWnd )
{
	std::string userid, userpassword;

	// SHIFTキー押しながらだと、ログインダイアログを出す.
	if( (g_miku.nico_alert==NULL && !g_config.nico_autologin) ||
		(g_config.nico_id.length()==0 || g_config.nico_password.length()==0) ||
		(GetKeyState(VK_SHIFT)&0x8000) ){
		INT_PTR dlgret;
		dlgret = DialogBox( g_miku.hInst, MAKEINTRESOURCE(IDD_NICO_IDPASS), hWnd, DlgNicoIDPASSProc );
		if( dlgret==IDCANCEL ) return;
		SAFE_DELETE( g_miku.nico_alert );
	}

	if( g_miku.nico_alert ){
		std::wstring text = GetStringFromResource(IDS_ERR_ALREADY_CONNECTED);
		std::wstring cap  = GetStringFromResource(IDS_ERR_CAPTION);
		MessageBox( hWnd, text.c_str(), cap.c_str(), MB_OK|MB_ICONEXCLAMATION );
		return;
	}

	g_miku.nico_alert = new NicoNamaAlert( g_miku.hInst, g_miku.pWindow->getWindowHandle() );
	g_miku.nico_alert->SetDisplayType( NicoNamaAlert::NNA_WINDOW );
	if( g_miku.cmdline.find(L"/balloon")!=std::wstring::npos ){
		g_miku.nico_alert->SetDisplayType( NicoNamaAlert::NNA_BALLOON );
	}

	wstrtostr( g_config.nico_id, userid );
	wstrtostr( g_config.nico_password, userpassword );
	rot47( userpassword );	// 復号化.

	int r = g_miku.nico_alert->Auth( userid.c_str(), userpassword.c_str() );
	if( r<0 ){
		dprintf( L"Failed NicoAuth\n" );
		SAFE_DELETE( g_miku.nico_alert );
		g_config.nico_autologin = 0;

		std::wstring text	 = GetStringFromResource(IDS_ERR_AUTH_FAILED);
		std::wstring caption = GetStringFromResource(IDS_ERR_AUTH_FAILED_CAPTION);
		MessageBox( hWnd, text.c_str(), caption.c_str(), MB_OK|MB_ICONEXCLAMATION );
		return;
	}

	if( g_miku.nico_alert->ConnectCommentServer()==0 ){
		if( g_miku.nico_listwin ) g_miku.nico_listwin->setNicoNamaAlert( g_miku.nico_alert );
		std::string str;
		wstrtostr( g_config.nico_matchkeyword, str );
		g_miku.nico_alert->setMatchKeyword( str );
		g_miku.nico_alert->SetRandomPickup( g_miku.cmdline.find( L"/randompickup", 0 )!=std::wstring::npos?true:false );
		g_miku.nico_alert->StartThread();
		SetTaskTrayBalloonMessage( GetStringFromResource(IDS_NICO_CONNECTED) );
		dprintf( L"Logged in to Niconama.\n" );
	}else{
		SAFE_DELETE( g_miku.nico_alert );

		std::wstring text = GetStringFromResource(IDS_ERR_CONNECT_FAILED);
		std::wstring cap  = GetStringFromResource(IDS_ERR_CONNECT_FAILED_CAPTION);
		MessageBox( hWnd, text.c_str(), cap.c_str(), MB_OK|MB_ICONEXCLAMATION );
	}
}

LRESULT OnContextMenu( HWND hWnd, WPARAM wParam, LPARAM lParam )
{
	// WM_COMMANDはコンテキストメニューにしか使ってない.
    WORD id = LOWORD(wParam);
	WORD notifycode = HIWORD(wParam);

	switch( id ){
    case ID_BTN_QUIT:
        DestroyWindow( hWnd );
        break;

    case ID_BTN_ABOUT:
        DialogBox( g_miku.hInst, MAKEINTRESOURCE(IDD_ABOUT_DIALOG), hWnd, DlgAboutProc );
        break;

    case ID_BTN_SETTING:
        DialogBox( g_miku.hInst, MAKEINTRESOURCE(IDD_SETTING_DIALOG), hWnd, DlgSettingProc );
        break;

    case ID_BTN_WHATTIMEISITNOW:
        SpeakMiku();
        break;

	case ID_BTN_3MIN:
		SetTimer( hWnd, TIMER_ID_3MIN, TIMER_3MIN*60*1000, NULL );
		break;

	case ID_BTN_NOCOLOGIN:
		NicoNamaLogin( hWnd );
		break;

	case ID_BTN_BROADCASTING_LIST:
		if( g_miku.nico_listwin ) g_miku.nico_listwin->Show( g_miku.nico_alert );
		break;

	default:
        if( id>ITUNES_TRACK_ID_BASE ){
            IITPlaylist *iPlaylist;

            if( g_miku.iTunes==NULL ) break;

            id -= ITUNES_TRACK_ID_BASE;
            g_miku.iTunes->get_CurrentPlaylist( &iPlaylist );
            if( iPlaylist==NULL ) break;

			IITTrackCollection *iTrackCollection;
            iPlaylist->get_Tracks( &iTrackCollection );
            if( iTrackCollection ){
                IITTrack *iTrack;
                iTrackCollection->get_Item( id, &iTrack );
                if( iTrack ){
                    iTrack->Play();
                    SAFE_RELEASE( iTrack );
                }
                SAFE_RELEASE( iTrackCollection );
            }
            SAFE_RELEASE( iPlaylist );
        }
        break;
    }
    return S_OK;
}

/** ポップアップメニューが選択されているときの処理.
 * 40000～がリソースエディタで勝手に割り当てられて、
 * 50000～51000をiTunesプレイリスト用に使用中。
 */
LRESULT OnMenuSelect( HWND hwnd, WPARAM wparam, LPARAM lparam )
{
	HMENU menu = (HMENU)lparam;
	int idx = LOWORD(wparam);
	int flg = HIWORD(wparam);
#if 0
	dprintf( L"menu index:%d\n", idx );
	dprintf( L"wparam:%p\n", wparam );
	dprintf( L"lparam:%p\n", lparam );
#endif
	if( idx==MENU_TRACKLIST_INSERT_POSITION && (flg & MF_POPUP) ){
        /* idx==MENU_TRACKLIST_INSERT_POSITION (Track List) が選択されたら、
         * 現在のプレイリストでサブメニュー項目を構築する.
         */
		HMENU tracklistmenu = GetSubMenu( menu, idx );
		if( g_miku.iTunes==NULL ) return 0;
		if( tracklistmenu==NULL ) return 0;
		if( GetMenuItemCount(tracklistmenu)!=0 ){
            dprintf( L"Tracklist was already created.\n" );
            return 0;
        }

        // メニューの高さをスクリーン高の半分に制限.
        MENUINFO info;
		ZeroMemory( &info, sizeof(info) );
        info.cbSize = sizeof(info);
        info.cyMax = GetSystemMetrics( SM_CYSCREEN ) / 2;
        info.fMask = MIM_MAXHEIGHT;
        SetMenuInfo( tracklistmenu, &info );

		MENUITEMINFO iteminfo;
		long playlistidx;
		GetCurrentTrackPlaylistIndex( &playlistidx );
		ZeroMemory( &iteminfo, sizeof(iteminfo) );
		iteminfo.cbSize = sizeof(iteminfo);

		IITPlaylist *iPlaylist;
		g_miku.iTunes->get_CurrentPlaylist( &iPlaylist );
		if( iPlaylist ){
			IITTrackCollection *iTrackCollection;
			iPlaylist->get_Tracks( &iTrackCollection );
			if( iTrackCollection ){
				long num;
				iTrackCollection->get_Count( &num );
                // 最大1000曲までに制限.
				for( int i=1; i<=num && i<=1000; i++ ){
					IITTrack *iTrack;
					iTrackCollection->get_ItemByPlayOrder( i, &iTrack );
					//iTrackCollection->get_Item( i, &iTrack );
					if( iTrack ){
						MENUITEMINFO iteminfo;
						ZeroMemory( &iteminfo, sizeof(iteminfo) );
						iteminfo.cbSize = sizeof(iteminfo);
						iteminfo.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;
						long idx=0;
						iTrack->get_Index( &idx );
						iteminfo.wID = ITUNES_TRACK_ID_BASE + idx;
						// 再生中のものはハイライトとデフォルトを付ける
						iteminfo.fState = playlistidx==idx ? MFS_HILITE|MFS_DEFAULT : 0;
						iTrack->get_Name( &iteminfo.dwTypeData );
						iteminfo.cch = (UINT)wcslen( iteminfo.dwTypeData );

						InsertMenuItem( tracklistmenu, i-1, 1, &iteminfo );
						SAFE_RELEASE( iTrack );
					}
				}
				SAFE_RELEASE( iTrackCollection );
			}
			SAFE_RELEASE( iPlaylist );
		}
	}
	return 0;
}


LRESULT OnMouseWheel( HWND hwnd, WPARAM wparam, LPARAM lparam )
{
	short distance = (SHORT)HIWORD( wparam ) / WHEEL_DELTA;
	int vk = LOWORD( wparam );
	int x = HIWORD( lparam );
	int y = LOWORD( lparam );
	dprintf( L"distance=%d\n", distance );
	if( g_miku.iTunes==NULL ) return 0;

	long volume;
	if( g_miku.iTunes->get_SoundVolume( &volume )==S_OK ){
		volume += distance;
		volume = MIN( 100, volume );
		volume = MAX(   0, volume );
		dprintf( L"volume=%d\n", volume );
		g_miku.iTunes->put_SoundVolume( volume );
		g_miku.iTunesVolume = volume;
		UpdateToolTipHelp( L"Volume:%d", volume );

        /* チップヘルプをVolumeに変えたあと曲名に戻すためのトリガとして.
           チップヘルプが消えた瞬間をトラップできればいいのだけど.
         */
		SetTimer( hwnd, TIMER_ID_REFRESH_TIPHELP, 5000, NULL );
	}
	return 0;
}


LRESULT OnTimer( HWND hWnd, WPARAM wParam, LPARAM lParam )
{
	switch( wParam ){
	case TIMER_ID_DRAW:
		UpdateMikuClock();
		InvalidateRect( hWnd, NULL, TRUE );
		CheckMikuSpeak( hWnd );
		FindITunes( hWnd );
		break;

	case TIMER_ID_REFRESH_TIPHELP:
		if( g_miku.iTunes ){
			ITPlayerState st;
			g_miku.iTunes->get_PlayerState( &st );
			if( st==ITPlayerStateStopped ){
				UpdateToolTipHelp( L"" );
			}else{
				UpdateToolTipHelp( L"%s (%s)", g_miku.currentmusic.c_str(), g_miku.currentartist.c_str() );
			}
		}else{
			UpdateToolTipHelp( L"" );
		}
		KillTimer( hWnd, TIMER_ID_REFRESH_TIPHELP );
		break;
	case TIMER_ID_NICO_KEEPALIVE:
		if( g_miku.nico_alert ) g_miku.nico_alert->KeepAlive();
		break;

	case TIMER_ID_3MIN:
		if( g_miku.inSpeak ){
			// 再生中ならあとまわしにする.
			SetTimer( hWnd, TIMER_ID_3MIN, 2000, NULL );
			break;
		}
		dprintf( L"3min progress.\n" );
		if( !PlaySound( MAKEINTRESOURCE(IDR_3MIN_PROGRESS), GetModuleHandle(NULL), SND_RESOURCE|SND_ASYNC ) ){
			dprintf( L"PlaySound failed\n" );
		}
		KillTimer( hWnd, TIMER_ID_3MIN );
		break;

	case TIMER_ID_RSS_UPDATE:
		if( g_miku.nico_alert ){
			// RSS更新スレッドを起動.
			g_miku.nico_alert->GetAllRSSAndUpdate();
		}
		break;
	}
	return 0;
}

LRESULT OnLeftButtonDoubleClock( HWND hwnd, WPARAM wparam, LPARAM lparam )
{
	iTunesPlayOrStop();
	return 0;
}

LRESULT OnPaint( HWND hWnd )
{
    PAINTSTRUCT ps;
    HDC hdc;
    hdc = BeginPaint( hWnd, &ps );
    DrawMiku( hdc );
    EndPaint( hWnd, &ps );
	return 0;
}

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	static UINT ttraymsg;

	switch( message ){
    case WM_CREATE:
        CreateToolTipHelp( hWnd );
        SetTimer( hWnd, TIMER_ID_DRAW, 1000, NULL );

		ttraymsg = RegisterWindowMessage(L"TaskbarCreated");
        break;

    case WM_PAINT:		return OnPaint( hWnd ); break;
    case WM_TIMER:		return OnTimer( hWnd, wParam, lParam ); break;
    // show menu
	case WM_CONTEXTMENU:return ShowPopupMenu( hWnd ); break;
	case WM_MENUSELECT:	return OnMenuSelect( hWnd, wParam, lParam ); break;

	case WM_INITMENUPOPUP:
		break;
	case WM_UNINITMENUPOPUP:
		break;
    // drive popup menu.
    case WM_COMMAND:		return OnContextMenu( hWnd, wParam, lParam ); break;

	case WM_LBUTTONDBLCLK:	return OnLeftButtonDoubleClock( hWnd, wParam, lParam ); break;
    case WM_MOUSEWHEEL:		return OnMouseWheel( hWnd, wParam, lParam ); break;
    case WM_ITUNES:			return OnITunesEvent( hWnd, wParam, lParam ); break;
	case WM_NNA_NOTIFY:		return OnNicoNamaNotify( hWnd, wParam, lParam); break;

	case WM_TTRAY_POPUP:
		switch( lParam ){
		case WM_RBUTTONUP:
			SetForegroundWindow( hWnd );
			ShowPopupMenu( hWnd );
			PostMessage( hWnd, WM_NULL, 0, 0 );
			break;
		}
		break;

    case WM_LBUTTONDOWN:
        // begin moving window.
        g_miku.inDrag = true;
        g_miku.dragStartX = LOWORD(lParam);
        g_miku.dragStartY = HIWORD(lParam);
        SetCapture( hWnd );
		return 0;
        break;
    case WM_MOUSEMOVE:
        // window is moving.
        if( g_miku.inDrag ){
            POINT point;
            GetCursorPos( &point );
            g_miku.pWindow->moveWindow( point.x-g_miku.dragStartX, point.y-g_miku.dragStartY );
        }else{

		}
		return 0;
        break;
    case WM_LBUTTONUP:
        // finish moving window.
        g_miku.inDrag = false;
        ReleaseCapture();
		return 0;
        break;

	case WM_DESTROY:
        SaveToRegistory();
        PostQuitMessage( 0 );
        break;

    default:
		if( message==ttraymsg ){
			dprintf( L"Re-register tasktray icon.\n" );
			RegisterTaskTrayIcon();
		}
		break;
    }

    return DefWindowProc( hWnd, message, wParam, lParam );
}


#include "myregexp.h"
void testpcre()
{
	int b;
	pcre_config( PCRE_CONFIG_UTF8, &b );
	if( b ){ dprintf(L"pcre support utf-8\n"); } else { dprintf(L"pcre do not support utf-8\n"); }

	std::string utf8_pattern;
	std::wstring wstr = L"(初音|ミク)";
	std::wstring wstr2;
	wstrtostr( wstr, utf8_pattern );
	strtowstr( utf8_pattern, wstr2 );

	myPCRE re( utf8_pattern.c_str(), PCRE_UTF8|PCRE_MULTILINE );

	std::wstring str[6] = {
		L"初音ミク",
		L"初音",
		L"ミク",
		L"初 音 ミ ク",
		L"部分一致初音ミク部分一致",
		L"なし",
	};

	for(int i=0;i<6;i++){
		std::string utf8;
		wstrtostr( str[i], utf8 );
		if( re.match( utf8.c_str() ) ){
			dprintf( L"%s\n", str[i].c_str() );
		}
	}

}


int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
	testpcre();

	InitMikuClock();
	g_miku.cmdline = lpCmdLine;
	CheckCommandLineOption();

	LoadMikuImage();
    UpdateMikuClock();

    g_miku.hInst = hInstance;
    g_miku.pWindow = new tWindow( APP_TITLE, 100, 100, 256, 256, WS_POPUP|WS_SYSMENU|WS_MINIMIZEBOX, WndProc, hInstance );
    g_miku.pWindow->resizeWindow( g_miku.w, g_miku.h );
    SetMikuWindowRegion( g_miku.pWindow->getWindowHandle() );

    LoadFromRegistory();
    g_miku.pWindow->moveWindow( g_config.winx, g_config.winy );
    g_miku.pWindow->setTransparency( g_config.isTransparent?g_config.trans_rate:255 );
    g_miku.pWindow->setTopMost( g_config.isTopMost );
    g_miku.pWindow->show();

	g_miku.nico_listwin = new tListWindow( hInstance, g_miku.pWindow->getWindowHandle() );
	MoveWindow( g_miku.nico_listwin->getWindowHandle(), g_config.nico_listwin_x, g_config.nico_listwin_y, g_config.nico_listwin_w, g_config.nico_listwin_h, TRUE );
	//g_miku.nico_listwin->Show();

	RegisterTaskTrayIcon();

	if( g_config.nico_autologin ){
		NicoNamaLogin( g_miku.pWindow->getWindowHandle() );
	}

	// Main message loop
    MSG msg = {0};
    BOOL bRet;
    while( (bRet = GetMessage( &msg, NULL, 0, 0 )) != 0){
        if (bRet == -1){
            // handle the error and possibly exit
            dprintf(L"Error occurred while GetMessage()\n");
            break;
        }else{
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

	UnregisterTaskTrayIcon();

	SAFE_DELETE( g_miku.nico_listwin );
	SAFE_DELETE( g_miku.nico_alert );
    SAFE_DELETE( g_miku.pWindow );

    ExitMikuClock();
	return (int)msg.wParam;
}
