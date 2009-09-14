#ifndef TNOTIFYWIN_H_DEF
#define TNOTIFYWIN_H_DEF

#include "winheader.h"
#include <string>
#include <gdiplus.h>


#define T_NOTIFY_CLASS	L"Miku39_tBalloonClass"


// ニコ生開始の通知ウィンドウ.
class tNotifyWindow {
	HWND	m_parenthwnd;
	HWND	m_hwnd;
	HWND	m_title;	// ユーザー生放送のタイトル名.
	HWND	m_desc;		// 説明.
	HWND	m_thumb;	// サムネ.
	Gdiplus::Bitmap *m_bitmap;	// サムネのビットマップ.
	HFONT	m_normalfont;
	HFONT	m_boldfont;
	HWND	m_tooltip;
	HBRUSH	m_bkbrush;	// スタティックコントロールの背景色.

	std::wstring	m_community_url;
	std::wstring	m_live_url;
	std::wstring	m_soundfilename;	// 再生するサウンドファイル名

	void CreateToolTip();
	void SetToolTip( const WCHAR*str );

public:
	tNotifyWindow( HINSTANCE hinst, HWND parent );
	~tNotifyWindow();

	void Show( bool playsound=false );

	inline HWND getWindowHandle() const { return m_hwnd; }
	inline HWND getParentWindowHandle() const { return m_parenthwnd; }
	inline HBRUSH getBkBrush() const { return m_bkbrush; }
	inline HFONT getNormalFont() const { return m_normalfont; }

	int SetWindowTitle( std::wstring &title );
	int SetDescription( std::wstring &desc );
	int SetTitle( std::wstring &title );
	int SetThumbnail( char*jpegimage, int sz );
	inline void SetCommunityURL( std::wstring &str){ m_community_url = str; }
	inline void SetLiveURL( std::wstring &str ){ m_live_url = str; SetToolTip( str.c_str() ); }
	inline std::wstring& GetCommunityURL(){ return m_community_url; }
	inline std::wstring& GetLiveURL(){ return m_live_url; }
	inline void setSoundFile( std::wstring &str ){ m_soundfilename = str; }
};


#endif
