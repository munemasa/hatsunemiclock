#include "winheader.h"
#include <gdiplus.h>

#include "tNotifyWindow.h"
#include "udmessages.h"

#include "debug.h"


enum {
	IDS_THUMB = 1000,
	IDS_TITLE,
	IDS_DESC,
};

// サムネを張る座標.
#define PICTURE_X	8
#define PICTURE_Y	24

/** ツールチップを作成する.
 */
void tNotifyWindow::CreateToolTip()
{
	DWORD dwStyle = TTS_ALWAYSTIP | TTS_NOPREFIX;
    m_tooltip = CreateWindowEx( 0, TOOLTIPS_CLASS, L"", dwStyle,
                                0, 0, 200,100, m_hwnd, NULL, NULL, NULL );
    TOOLINFO ti;
    ZeroMemory( &ti, sizeof(TOOLINFO) );
    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = m_tooltip;
    ti.hinst = GetModuleHandle(NULL);
    ti.lpszText = L"";
    ti.uId = (UINT_PTR)m_hwnd;
    ti.rect.left = 8;
    ti.rect.right = 8;
    ti.rect.top = 8+64;
    ti.rect.bottom = 8+64;
    SendMessage( m_tooltip, TTM_ADDTOOL, 0, (LPARAM)&ti );
    SendMessage( m_tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, (LPARAM)MAKELONG(100, 0) );
}

void tNotifyWindow::SetToolTip( const WCHAR*str )
{
    TOOLINFO ti;
	std::wstring s;
	s = str;
	s += L"/dummy";	// なぜか最後の / 以降表示してくれないので. 原因をググったけど見つからず.
    ZeroMemory( &ti, sizeof(TOOLINFO) );
    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = m_tooltip;
    ti.hinst = NULL;
	ti.lpszText = (LPWSTR)s.c_str();
    ti.uId = (UINT_PTR)m_hwnd;
    SendMessage( (HWND)m_tooltip, (UINT)TTM_UPDATETIPTEXT, (WPARAM)0, (LPARAM)&ti );
}


static LRESULT CALLBACK NotifyWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	tNotifyWindow*notify;
	int x,y;
	// 1つしかウィンドウ出さないようにしているからstaticでいいか.
	static bool inDrag = false;
	static int dragStartX = 0;
	static int dragStartY = 0;

	switch( message ){
	case WM_CREATE:
		SetTimer( hWnd, 1, 10*1000, NULL );
		break;

	case WM_TIMER:
		dprintf( L"Notify window timed out.\n" );
		PostMessage( hWnd, WM_CLOSE, 0, 0 );
		break;

	case WM_CONTEXTMENU:
		break;

	case WM_PAINT:{
		RECT rect;
		PAINTSTRUCT ps;
		HDC hDC;
		hDC = BeginPaint( hWnd, &ps );
		notify = (tNotifyWindow*)GetWindowLongPtr( hWnd, GWLP_USERDATA );
		if( notify->getNormalFont() ){
			SelectObject( hDC, notify->getNormalFont() );
		}else{
			SelectObject( hDC, GetStockObject( DEFAULT_GUI_FONT ) );
		}
		// タイトルを描く.
		SetBkMode( hDC, TRANSPARENT );
		WCHAR wstr[2048];
		GetWindowText( hWnd, wstr, 2048 );
		TextOut( hDC, PICTURE_X, 4, wstr, wcslen(wstr) );

		// [X]ボタンを描く.
		GetClientRect( hWnd, &rect );
		rect.left = rect.right - 16;
		rect.bottom = rect.top + 16;
		DrawFrameControl( hDC, &rect, DFC_CAPTION, DFCS_CAPTIONCLOSE | DFCS_FLAT );
		EndPaint( hWnd, &ps );
		break;}

	case WM_CTLCOLORSTATIC:
		notify = (tNotifyWindow*)GetWindowLongPtr( hWnd, GWLP_USERDATA );
		return (LRESULT)notify->getBkBrush();
		break;

	case WM_CLOSE:
		inDrag = false;
		notify = (tNotifyWindow*)GetWindowLongPtr( hWnd, GWLP_USERDATA );
		PostMessage( notify->getParentWindowHandle(), WM_NNA_NOTIFY, NNA_CLOSED_NOTIFYWINDOW, 0 );
		delete notify;
		dprintf( L"Delete tNotifyWindow %p\n", notify );
		break;

	case WM_LBUTTONDOWN:{
		RECT rect;
		GetClientRect( hWnd, &rect );
		rect.left = rect.right - 16;
		rect.bottom = rect.top + 16;	// [X]の位置.

		x = LOWORD( lParam );
		y = HIWORD( lParam );
		if( x>=PICTURE_X && x<=PICTURE_X+64 && y>=PICTURE_Y && y<=PICTURE_Y+64 ){
			SetCursor( LoadCursor(NULL, IDC_ARROW) );
			tNotifyWindow*notify;
			notify = (tNotifyWindow*)GetWindowLongPtr( hWnd, GWLP_USERDATA );
			dprintf( L"Thumbnail clicked\n" );
			HINSTANCE r = ShellExecute(NULL, L"open", notify->GetLiveURL().c_str(), NULL, NULL, SW_SHOWNORMAL);
			if( (int)r<=32 ){
				dprintf( L"ShellExecute: %d\n", (int)r );
			}
			KillTimer( hWnd, 1 );
			PostMessage( hWnd, WM_CLOSE, 0, 0 );
		}else if( x>=rect.left && x<=rect.right && y>=rect.top && y<=rect.bottom ){
			PostMessage( hWnd, WM_CLOSE, 0, 0 );
		}else{
			inDrag = true;
			dragStartX = LOWORD(lParam);
			dragStartY = HIWORD(lParam);
			SetCapture( hWnd );
		}
		return 0;
		break;}

	case WM_MOUSEMOVE:
        if( inDrag ){
            POINT point;
            GetCursorPos( &point );
			RECT rect;
			int screenx = point.x-dragStartX, screeny = point.y-dragStartY;
			GetWindowRect( hWnd, &rect );
			SetWindowPos( hWnd, HWND_TOP, screenx, screeny, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
		}else{
			RECT rect;
			GetClientRect( hWnd, &rect );
			rect.left = rect.right - 16;
			rect.bottom = rect.top + 16;	// [X]の位置.

			x = LOWORD( lParam );
			y = HIWORD( lParam );
			if( (x>=PICTURE_X && x<=PICTURE_X+64 && y>=PICTURE_Y && y<=PICTURE_Y+64) ||
				(x>=rect.left && x<=rect.right && y>=rect.top && y<=rect.bottom) ){
				SetCursor( LoadCursor(NULL, IDC_HAND) );
			}else{
				SetCursor( LoadCursor(NULL, IDC_ARROW) );
			}
		}
		return 0;
		break;
    case WM_LBUTTONUP:
        // finish moving window.
        inDrag = false;
        ReleaseCapture();
		return 0;

	default:
		break;
	}
	return DefWindowProc( hWnd, message, wParam, lParam ); 
}


tNotifyWindow::tNotifyWindow( HINSTANCE hinst, HWND parent )
{
	m_bitmap = NULL;
	m_parenthwnd = parent;
	m_bkbrush = CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );

	WNDCLASSEX wc;
	ZeroMemory( &wc, sizeof(wc) );
	wc.cbSize = sizeof(wc);
	wc.style = CS_DROPSHADOW;
	wc.lpfnWndProc = NotifyWindowProc;
	wc.hInstance = hinst;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.lpszClassName = T_NOTIFY_CLASS;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	RegisterClassEx( &wc );

	DWORD dwStyle = (WS_POPUP | WS_CAPTION | WS_SYSMENU) & ~WS_DLGFRAME;
	DWORD dwExStyle = WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TOPMOST;
	int x = 0;
	int y = 0;
	int w = 280;
	int h = 120;

    RECT rc;
	rc.top = y;
	rc.bottom = y + h;
	rc.left = x;
	rc.right = x + w;
    AdjustWindowRectEx( &rc, dwStyle, FALSE, dwExStyle );

	// 画面右下のタスクバーの上に表示させよう.
	APPBARDATA bardata;
	ZeroMemory( &bardata, sizeof(bardata) );
	bardata.cbSize = sizeof(bardata);
	SHAppBarMessage( ABM_GETTASKBARPOS, &bardata);

	w = rc.right-rc.left;
	h = rc.bottom-rc.top;
	// 5ドット分の下駄を履かせる. 本当ならウィドウフレームのサイズ取るべきなんだろうけど.
	if( bardata.uEdge==ABE_BOTTOM ){
		x = GetSystemMetrics( SM_CXSCREEN ) - w -5;
		y = bardata.rc.top - h -5;
	}
	if( bardata.uEdge==ABE_RIGHT ){
		x = bardata.rc.left - w -5;
		y = GetSystemMetrics( SM_CYSCREEN ) - h -5;
	}
	m_hwnd = CreateWindowEx( dwExStyle, T_NOTIFY_CLASS, L"", dwStyle,
		                     x, y, w, h, parent, NULL, NULL, NULL );
	COLORREF col = 0x00ffffff;
	BYTE alpha = 224;
	SetLayeredWindowAttributes( m_hwnd, col, alpha, LWA_ALPHA );

	NONCLIENTMETRICS ncm;
	bool issetfont = false;
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	if( SystemParametersInfo( SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0 ) ){
		LOGFONT normal, bold;
		ncm.lfMessageFont.lfHeight = -10;
		normal = ncm.lfMessageFont;
		bold   = ncm.lfMessageFont;
		bold.lfWeight = 600;
		this->m_normalfont = CreateFontIndirect( &normal );
		this->m_boldfont   = CreateFontIndirect( &bold );
		issetfont = true;
	}else{
		// XPだとこっちに来るらしい.
		m_normalfont = NULL;
		m_boldfont   = NULL;
	}

	x = PICTURE_X;
	y = PICTURE_Y;
	w = 64;
	h = 64;
	dwStyle = WS_CHILD | WS_VISIBLE | SS_BITMAP;
	m_thumb = CreateWindowEx( 0, WC_STATIC, L"thumb", dwStyle,
		                      x, y, w, h, m_hwnd, (HMENU)IDS_THUMB, NULL, NULL );
	x = 80;
	y = PICTURE_Y;
	w = 192;
	h = 16;
	dwStyle = WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP;
	dwExStyle = 0;
	m_title = CreateWindowEx( dwExStyle, WC_STATIC, L"title", dwStyle,
		                      x, y, w, h, m_hwnd, (HMENU)IDS_TITLE, NULL, NULL );
	if( issetfont ){
		SendMessage( m_title, WM_SETFONT, (WPARAM)m_boldfont, 0 );
	}else{
		SendMessage( m_title, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0 );
	}

	x = 80;
	y = PICTURE_Y+20;
	w = 192;
	h = 100;
	dwStyle = WS_CHILD | WS_VISIBLE;
	dwExStyle = 0;
	m_desc = CreateWindowEx( dwExStyle, WC_STATIC, L"description", dwStyle,
		                     x, y, w, h, m_hwnd, (HMENU)IDS_DESC, NULL, NULL );
	if( issetfont ){
		SendMessage( m_desc, WM_SETFONT, (WPARAM)m_normalfont, 0 );
	}else{
		SendMessage( m_desc, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0 );
	}

	SetWindowLongPtr( m_hwnd, GWLP_USERDATA, (LONG_PTR)this );
	CreateToolTip();
}

tNotifyWindow::~tNotifyWindow()
{
	DeleteObject( m_bkbrush );
	DeleteObject( m_normalfont );
	DeleteObject( m_boldfont );
	DestroyWindow( m_title );
	DestroyWindow( m_desc );
	DestroyWindow( m_thumb );
	DestroyWindow( m_hwnd );
	SAFE_DELETE( m_bitmap );
	dprintf( L"Deleted tNotifyWindow.\n" );
}

void tNotifyWindow::Show()
{
	ShowWindow( m_hwnd, SW_SHOW );
	UpdateWindow( m_hwnd );
}

int tNotifyWindow::SetWindowTitle( std::wstring &title )
{
	SetWindowText( m_hwnd, title.c_str() );
	return 0;
}

int tNotifyWindow::SetDescription( std::wstring &desc )
{
	SetWindowText( m_desc, desc.c_str() );
	return 0;
}
int tNotifyWindow::SetTitle( std::wstring &title )
{
	SetWindowText( m_title, title.c_str() );
	return 0;
}


int tNotifyWindow::SetThumbnail( char*jpegimage, int sz )
{
	HGLOBAL mem = GlobalAlloc( GHND, sz );
	void*ptr;
	if( mem==NULL ){
		dprintf( L"GlobalAlloc: out of memory\n" );
		return -1;
	}
	ptr = GlobalLock( mem );
	if( ptr==NULL ){
		GlobalFree( mem );
		dprintf( L"GlobalLock failed\n" );
		return -1;
	}
	CopyMemory( ptr, jpegimage, sz );

	IStream *stream = NULL;
	if( CreateStreamOnHGlobal( mem, FALSE, &stream )==S_OK ){
		HBITMAP hBitmap;
		SAFE_DELETE( m_bitmap );
		m_bitmap = Gdiplus::Bitmap::FromStream( stream );
		Gdiplus::Color col(0,0,0);
		m_bitmap->GetHBITMAP( col, &hBitmap );
		stream->Release();
		SendMessage( m_thumb, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap );
	}
	GlobalUnlock( mem );
	GlobalFree( mem );
	return 0;
}
