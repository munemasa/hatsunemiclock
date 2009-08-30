#include "tWindow.h"

tWindow::tWindow(const WCHAR *title, int x, int y, int w, int h,
                 WNDPROC wndproc, HINSTANCE hinst)
{
    this->x = x;
    this->y = y;
    this->w = w;
    this->h = h;
    this->m_title = title;

	m_style = WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX;
	//m_style = WS_OVERLAPPEDWINDOW & ~WS_CAPTION & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;

    m_wcex.cbSize = sizeof( WNDCLASSEX );
    m_wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    m_wcex.lpfnWndProc = wndproc;
    m_wcex.cbClsExtra = 0;
    m_wcex.cbWndExtra = 0;
    m_wcex.hInstance = hinst;
    m_wcex.hIcon = NULL;
    m_wcex.hCursor = LoadCursor( NULL, IDC_ARROW );
    m_wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    m_wcex.lpszMenuName = NULL;
    m_wcex.lpszClassName = title;
    m_wcex.hIconSm = NULL;
    RegisterClassEx( &m_wcex );

    RECT rc;
	rc.top = y;
	rc.bottom = y + h;
	rc.left = x;
	rc.right = x + w;
    AdjustWindowRect( &rc, m_style, FALSE );
    m_hwnd = CreateWindowEx( WS_EX_LAYERED, title, title, m_style,
							 CW_USEDEFAULT, CW_USEDEFAULT, rc.right-rc.left, rc.bottom-rc.top, NULL, NULL, hinst,
							 NULL );
}

int tWindow::moveWindow(int screenx, int screeny )
{
	RECT rect;
	GetWindowRect( m_hwnd, &rect );
	SetWindowPos( m_hwnd, HWND_TOP,
		          screenx, screeny, 0, 0,
				  SWP_SHOWWINDOW | SWP_NOSIZE);
	x = screenx;
	y = screeny;
	return 0;
}

int tWindow::resizeWindow(int clientw, int clienth)
{
    RECT rc;
	int x,y,w,h;

	GetWindowRect( this->m_hwnd, &rc );

	x = rc.left;
	y = rc.top;
	rc.bottom = y + clienth;
	rc.right = x + clientw;
	AdjustWindowRect( &rc, this->m_style, FALSE );

	x = rc.left;
	y = rc.top;
	w = rc.right-rc.left;
	h = rc.bottom-rc.top;
	MoveWindow( this->m_hwnd, x, y, w, h, TRUE );
	return 0;
}



int tWindow::show()
{
    ShowWindow( m_hwnd, SW_SHOWDEFAULT );
	UpdateWindow( m_hwnd );
	return 0;
}

// alpha: 0-255
// 0 is transparent, 255 is not transparent.
int tWindow::setTransparency(int alpha)
{
	COLORREF col = 0x00ffffff;
	SetLayeredWindowAttributes( m_hwnd, col, alpha, LWA_ALPHA );
	return 0;
}

int tWindow::setTopMost(bool b)
{
	SetWindowPos( m_hwnd, b?HWND_TOPMOST:HWND_NOTOPMOST,
		          0, 0, 0, 0,
				  SWP_NOMOVE | SWP_NOSIZE);
	return 0;
}


tWindow::~tWindow()
{
    UnregisterClass( m_title, m_wcex.hInstance );
}
