#ifndef TNOTIFYWIN_H_DEF
#define TNOTIFYWIN_H_DEF

#include "winheader.h"
#include <string>
#include <gdiplus.h>


#define T_NOTIFY_CLASS	L"Miku39_tBalloonClass"


// �j�R���J�n�̒ʒm�E�B���h�E.
class tNotifyWindow {
	HWND	m_parenthwnd;
	HWND	m_hwnd;
	HWND	m_title;	// ���[�U�[�������̃^�C�g����.
	HWND	m_desc;		// ����.
	HWND	m_thumb;	// �T���l.
	Gdiplus::Bitmap *m_bitmap;	// �T���l�̃r�b�g�}�b�v.
	HFONT	m_normalfont;
	HFONT	m_boldfont;
	HWND	m_tooltip;
	HBRUSH	m_bkbrush;	// �X�^�e�B�b�N�R���g���[���̔w�i�F.

	std::wstring	m_community_url;
	std::wstring	m_live_url;
	std::wstring	m_soundfilename;	// �Đ�����T�E���h�t�@�C����

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
