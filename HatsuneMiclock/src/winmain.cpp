#define _WIN32_WINNT	0x0500

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <mmsystem.h>

#include "tWindow.h"
#include "iTunesLib.h"
#include "../resource.h"

#pragma comment(lib,"winmm.lib")		// windows multimedia
#pragma comment(lib,"comctl32.lib")		// common control


#define APP_TITLE			 L"Hatsune Miclock"
#define BUF_STRING_SIZE		(4096)		// number of charactors.
#define REG_SUBKEY			L"Software\\Miku39.jp\\HatsuneMiclock"


struct T_MIKU_CONFIG {
	DWORD winx, winy;		// position of window.

	DWORD isTopMost;
	DWORD is12_24;
	DWORD isTransparent;
	DWORD trans_rate;	// 0-255 (0:transparent, 255:non-transparent)

	DWORD speak_type;	// 0:every min, 1:every hour, 2:no speak
}g_config;

struct T_MIKU_CLOCK {
	HINSTANCE	hInst;
	HANDLE		sayevent;
	DWORD		thid;
	tWindow		*pWindow;

	HBITMAP     hMiku;	// handle of Miku image.
	int			w,h;	// size of Miku image.

	HBITMAP		hBoard;	// clock board image.
	int			bx,by;	// position of drawing clock board.
	int			bw,bh;

	HFONT		hFont;	// font of drawing time.
	int			fx,fy;	// position of clock text.

	int			hour,min,sec;	// current time.
	int			oldhour,oldmin;

	bool		inDrag;		// now in dragging.
	int			dragStartX, dragStartY;

	bool		inSpeak;	// now in speaking.

	IiTunes		*iTunes;
} g_miku;


DWORD WINAPI thMikuSaysNowTime(LPVOID v);
void SetWindowTitleToMusicName( HWND hwnd );


int dprintf(WCHAR*format,...)
{
	va_list list;
	va_start( list, format );
	int r;
	WCHAR buf[BUF_STRING_SIZE];
	r = wvsprintf(buf, format, list);
	OutputDebugString(buf);
	return r;
}

/** �����~�N���b�N�̏�����.
 * ���C�u�����A�X���b�h�A���[�N�̏��������s��.
 */
void InitMikuClock()
{
    // read some parameters from resource.
	memset( &g_miku, 0, sizeof(g_miku) );

	TCHAR buf[BUF_STRING_SIZE];

    // position of text.
	LoadString( GetModuleHandle(NULL), IDS_TEXT_XPOSITION,
                buf, sizeof(BUF_STRING_SIZE) );
	g_miku.fx = _wtoi(buf);
	LoadString( GetModuleHandle(NULL), IDS_TEXT_YPOSITION,
                buf, sizeof(BUF_STRING_SIZE) );
	g_miku.fy = _wtoi(buf);

    // position of clock board.
	LoadString( GetModuleHandle(NULL), IDS_BOARD_XPOSITION,
                buf, sizeof(BUF_STRING_SIZE) );
	g_miku.bx = _wtoi(buf);
	LoadString( GetModuleHandle(NULL), IDS_BOARD_YPOSITION,
                buf, sizeof(BUF_STRING_SIZE) );
	g_miku.by = _wtoi(buf);

	// speaking event
	g_miku.sayevent = CreateEvent( NULL, FALSE, FALSE, L"Mik39.jp.MikuSayEvent" );

	// speaking thread
	CreateThread( NULL, 0, thMikuSaysNowTime, NULL, 0, &g_miku.thid );

	// COM
	CoInitializeEx( NULL, COINIT_MULTITHREADED );

	// common control
	INITCOMMONCONTROLSEX ic;
	ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
	ic.dwICC = ICC_UPDOWN_CLASS;// | ICC_LINK_CLASS;
	InitCommonControlsEx(&ic); 
}

/** �����~�N���b�N�̌�Еt��.
 * iTunes����ؒf������.
 */
void ExitMikuClock()
{
	DisconnectITunesEvent();
	if( g_miku.iTunes ) g_miku.iTunes->Release();

	CloseHandle( g_miku.sayevent );
	CoUninitialize();
}

/** ���\�[�X����~�N�摜�����[�h����.
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

/** �E�B���h�E���[�W�������摜�̌^�ɂ���.
 * this must be called after LoadMikuImage() function.
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

/** �ݒ荀�ڂ����W�X�g���ɕۑ�����.
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
	RegCloseKey( hkey );
}

/// ���W�X�g���̓ǂݍ���.
LSTATUS ReadRegistoryDW( HKEY hkey, WCHAR*entry, DWORD*data )
{
	DWORD type, size;
	type = REG_DWORD;
	size = sizeof(DWORD);
	return RegQueryValueEx( hkey, entry, 0, &type, (BYTE*)data, &size );
}

/** ���W�X�g������ݒ荀�ڂ����[�h����.
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

	RegCloseKey( hkey );

    if( g_config.trans_rate < 10 ) g_config.trans_rate = 10;
    if( g_config.trans_rate > 255) g_config.trans_rate = 255;
	if( (int)g_config.winx < -g_miku.w ) g_config.winx = 0;
	if( (int)g_config.winy < -g_miku.h ) g_config.winy = 0;
}

/** �~�N��`��.
 * @param hdc �`���̃f�o�C�X�R���e�L�X�g
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

/** iTunes��T���Đڑ�����.
 * @param hwnd iTunesEvent���󂯂Ƃ�E�B���h�E
 */
void FindITunes( HWND hwnd )
{
	if( g_miku.iTunes==NULL && FindITunes() ){
		CreateITunesCOM( &g_miku.iTunes, hwnd );
		ConnectITunesEvent( g_miku.iTunes );
		SetWindowTitleToMusicName( hwnd );
	}
}


/** �������[�N�̍X�V.
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

/** �����Đ��X���b�h.
 */
DWORD WINAPI thMikuSaysNowTime(LPVOID v)
{
	volatile int hour;
	volatile int min;
	static int harray[24] = {
		IDR_HOUR00, IDR_HOUR01, IDR_HOUR02, IDR_HOUR03, IDR_HOUR04, IDR_HOUR05,	IDR_HOUR06, IDR_HOUR07, IDR_HOUR08, IDR_HOUR09,
		IDR_HOUR10, IDR_HOUR11,	IDR_HOUR12, IDR_HOUR13, IDR_HOUR14, IDR_HOUR15, IDR_HOUR16, IDR_HOUR17,	IDR_HOUR18, IDR_HOUR19,
		IDR_HOUR20, IDR_HOUR21, IDR_HOUR22, IDR_HOUR23,
	};
	static int marray[60] = {
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
			PlaySound( MAKEINTRESOURCE(harray[hour]), GetModuleHandle(NULL), SND_RESOURCE );
			PlaySound( MAKEINTRESOURCE(marray[min]), GetModuleHandle(NULL), SND_RESOURCE );
			g_miku.inSpeak = false;
		}
	}
	ExitThread(0);
}

/** ���ݎ����𒝂点��.
 */
void SpeakMiku()
{
	if( !g_miku.inSpeak ){
		SetEvent( g_miku.sayevent );
	}
}

/** ���ݎ����𒝂点�邩�ǂ����`�F�b�N����.
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

/** �R���e�L�X�g���j���[��\������.
 * @param hwnd �e�E�B���h�E
 */
void ShowContextMenu(HWND hwnd)
{
	HMENU menu;
	HMENU submenu;
	POINT pt;

	menu = LoadMenu( GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_CONTEXTMENU) );
	submenu = GetSubMenu( menu, 0 ); 
	SetMenuDefaultItem( submenu, ID_BTN_WHATTIMEISITNOW, 0 );
	GetCursorPos( &pt );
	TrackPopupMenu( submenu, TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL );
	DestroyMenu( menu );
}

/** �E�B���h�E�^�C�g�����Ȗ��ɕύX����.
 */
void SetWindowTitleToMusicName( HWND hwnd )
{
	if( g_miku.iTunes ){
		IITTrack *iTrack;
		g_miku.iTunes->get_CurrentTrack( &iTrack );
		if( iTrack ){
			BSTR name;
			WCHAR windowname[BUF_STRING_SIZE];
			iTrack->get_Name( &name );
			wsprintf( windowname, L"%s::%s", name, APP_TITLE );
			SetWindowText( hwnd, windowname );
		}
	}
}

/** iTunes�C�x���g�̏���.
 */
void ProcessITunesEvent( HWND hwnd, WPARAM wparam, LPARAM lparam )
{
	switch( wparam ){
    case ITEventDatabaseChanged:
        break;

	case ITEventPlayerPlay:
		SetWindowTitleToMusicName( hwnd );
		break;

    case ITEventPlayerStop:
		SetWindowText( hwnd, APP_TITLE );
        break;

	case ITEventPlayerPlayingTrackChanged:
		break;

	case ITEventUserInterfaceEnabled:
		break;

	case ITEventCOMCallsDisabled:
		break;

	case ITEventCOMCallsEnabled:
		break;

	case ITEventQuitting:
		break;

	case ITEventAboutToPromptUserToQuit:
		break;

	case ITEventSoundVolumeChanged:
		break;

	default:
		break;
	}
}

/** �ݒ�_�C�A���O�Ɍ���𔽉f������.
 * @param hwnd �_�C�A���O�̃E�B���h�E�n���h��
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

/** �ݒ��K�p����.
 * �ݒ�_�C�A���O�̓��e��config�ɔ��f������
 * @param hwnd �_�C�A���O�̃E�B���h�E�n���h��
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


LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch( message ){
	case WM_CREATE:
		SetTimer( hWnd, 1, 1000, NULL );
		break;

    case WM_PAINT:
        hdc = BeginPaint( hWnd, &ps );
        DrawMiku( hdc );
        EndPaint( hWnd, &ps );
        break;

	case WM_TIMER:
		UpdateMikuClock();
		InvalidateRect( hWnd, NULL, TRUE );
		CheckMikuSpeak( hWnd );
		FindITunes( hWnd );
		break;

	case WM_LBUTTONDBLCLK:  // double click
		SpeakMiku();
		break;

	case WM_LBUTTONDOWN:
		// begin moving window.
		g_miku.inDrag = true;
		g_miku.dragStartX = LOWORD(lParam);
		g_miku.dragStartY = HIWORD(lParam);
		SetCapture( hWnd );
		break;
	case WM_MOUSEMOVE:
		// window is moving.
		if( g_miku.inDrag ){
			POINT point;
			GetCursorPos( &point );
			g_miku.pWindow->moveWindow( point.x-g_miku.dragStartX, point.y-g_miku.dragStartY );
		}
		break;
	case WM_LBUTTONUP:
		// finish moving window.
		g_miku.inDrag = false;
		ReleaseCapture();
		break;

	case WM_RBUTTONDOWN:
		// show menu
		ShowContextMenu( hWnd );
		break;

    case WM_DESTROY:
		SaveToRegistory();
        PostQuitMessage( 0 );
        break;

	case WM_COMMAND:
		// drive popup menu.
		switch( LOWORD(wParam) ){
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
		default:
			break;
		}
		break;

	case WM_ITUNES:
		ProcessITunesEvent( hWnd, wParam, lParam );
		break;

	default:
		break;
    }

	return DefWindowProc( hWnd, message, wParam, lParam );
}

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
	InitMikuClock();
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

	// Main message loop
    MSG msg = {0};
    BOOL bRet;
    while( (bRet = GetMessage( &msg, NULL, 0, 0 )) != 0){
        if (bRet == -1){
            // handle the error and possibly exit
			OutputDebugString(L"Error occurred while GetMessage()\n");
			break;

		}else{
            TranslateMessage(&msg); 
            DispatchMessage(&msg); 
        }
    }

	ExitMikuClock();

	delete g_miku.pWindow;
	return (int)msg.wParam;
}
