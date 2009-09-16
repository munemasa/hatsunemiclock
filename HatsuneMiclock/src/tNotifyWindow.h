#ifndef TNOTIFYWIN_H_DEF
#define TNOTIFYWIN_H_DEF

#include "winheader.h"
#include <string>
#include <gdiplus.h>


#define T_NOTIFY_CLASS	L"Miku39_tBalloonClass"
#define T_NOTIFY_WIN_H	(120)


// �j�R���J�n�̒ʒm�E�B���h�E.
class tNotifyWindow {
	static const DWORD m_defStyle = (WS_POPUP | WS_CAPTION | WS_SYSMENU) & ~WS_DLGFRAME;
	static const DWORD m_defExStyle = WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TOPMOST;

	HWND	m_parenthwnd;
	HWND	m_hwnd;
	HWND	m_title;	// ���[�U�[�������̃^�C�g����.
	HWND	m_desc;		// ����.
	HWND	m_thumb;	// �T���l.
	Gdiplus::Bitmap *m_bitmap;	// �T���l�̃r�b�g�}�b�v.
	HBITMAP	m_hBitmap;			// ���̃n���h��.
	HFONT	m_normalfont;
	HFONT	m_boldfont;
	HWND	m_tooltip;
	HBRUSH	m_bkbrush;	// �X�^�e�B�b�N�R���g���[���̔w�i�F.

	std::wstring	m_community_url;
	std::wstring	m_live_url;
	std::wstring	m_soundfilename;	// �Đ�����T�E���h�t�@�C����
	bool			m_playsound;

	void CreateToolTip();
	void SetToolTip( const WCHAR*str );

public:
	bool inDrag;
	int dragStartX;
	int dragStartY;

	static void getInitialPos( int&x, int&y, int&w, int&h );

	tNotifyWindow( HINSTANCE hinst, HWND parent );
	~tNotifyWindow();

	void Show( bool playsound=false, int posx=-1, int posy=-1 );

	inline HWND getWindowHandle() const { return m_hwnd; }
	inline HWND getParentWindowHandle() const { return m_parenthwnd; }
	inline HBRUSH getBkBrush() const { return m_bkbrush; }
	inline HFONT getNormalFont() const { return m_normalfont; }
	inline bool isPlaySound() const { return m_playsound; }

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
