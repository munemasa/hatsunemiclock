#ifndef TWINDOW_H_DEF
#define TWINDOW_H_DEF

#include <windows.h>

class tWindow {
    HWND        m_hwnd; // this window handle
    WNDCLASSEX  m_wcex;
	DWORD		m_style;

    int x,y,w,h;
    const WCHAR *m_title;

public:
    inline HWND getWindowHandle() const { return m_hwnd; }
	inline DWORD getStyle() const { return m_style; }

	tWindow(const WCHAR *title, int x, int y, int w, int h, WNDPROC wndproc, HINSTANCE hinst);
	~tWindow();

	int moveWindow(int screenx, int screeny );
	int resizeWindow(int clientw, int clienth);
	int show();
	int setTransparency(int alpha);
	int setTopMost(bool b);
};


#endif
