#ifndef NICONAMAALERT_H_DEF
#define NICONAMAALERT_H_DEF

#include "winheader.h"
#include "tNetwork.h"

#include "libxml/tree.h"
#include "libxml/parser.h"

#include <string>
#include <vector>
#include <queue>
#include <list>


#define	NICO_COMMUNITY_URL	L"http://ch.nicovideo.jp/community/"
#define NICO_CHANNEL_URL	L"http://ch.nicovideo.jp/channel/"
#define NICO_LIVE_URL		L"http://live.nicovideo.jp/watch/"

#define NICO_MAX_RECENT_PROGRAM		(5)		// アラート履歴の最大数.

//----------------------------------------------------------------------
// prototype
//----------------------------------------------------------------------
void thNicoNamaAlert(LPVOID v);
class tNotifyWindow;


//----------------------------------------------------------------------
// define
//----------------------------------------------------------------------
struct NicoNamaProgram {
	// utf-8
	std::string		request_id;	// lvXXXXXXX
	std::string		title;
	std::string		description;

	std::string		community;	// coXXXX, chXXXX
	std::string		community_name;
	std::string		thumbnail;

	char*			thumbnail_image;
	DWORD			thumbnail_image_size;

	time_t			start;  // 番組の開始時刻.

	NicoNamaProgram(){ thumbnail_image = NULL; }
	~NicoNamaProgram(){ if( thumbnail_image ) free( thumbnail_image ); }

	NicoNamaProgram( const NicoNamaProgram &src ){
		// thumbnail_imageのためのコピーコンストラクタ.
		this->request_id		= src.request_id;
		this->title				= src.title;
		this->description		= src.description;
		this->community			= src.community;
		this->community_name	= src.community_name;
		this->thumbnail			= src.thumbnail;
		this->thumbnail_image_size = src.thumbnail_image_size;
		this->start				= src.start;
		// 画像を新規メモリにコピー
		this->thumbnail_image	= (char*)malloc( this->thumbnail_image_size );
		CopyMemory( this->thumbnail_image, src.thumbnail_image, thumbnail_image_size );
	}
	NicoNamaProgram& operator=( const NicoNamaProgram &src ){
		// 代入演算のとき.
		SAFE_DELETE( this->thumbnail_image );
		this->request_id		= src.request_id;
		this->title				= src.title;
		this->description		= src.description;
		this->community			= src.community;
		this->community_name	= src.community_name;
		this->thumbnail			= src.thumbnail;
		this->thumbnail_image_size = src.thumbnail_image_size;
		this->start				= src.start;
		// 画像を新規メモリにコピー
		this->thumbnail_image = (char*)malloc( this->thumbnail_image_size );
		CopyMemory( this->thumbnail_image, src.thumbnail_image, thumbnail_image_size );
		return *this;
	}
};


class NicoNamaAlert {
public:
	enum {
		NNA_BALLOON,	// Shell_NotifyIconによるバルーンヘルプ.
		NNA_WINDOW,		// CreateWindowで作るウィンドウ.
		NNA_MAX,
	};

private:
	HINSTANCE		m_hinst;
	HWND			m_parenthwnd;
	int				m_displaytype;		// アラートの表示方法.

	std::string		m_ticket;			// 第一認証で取得するチケット.
	std::string		m_userid;
	std::string		m_userhash;
	std::vector< std::wstring >	m_communities;	// 参加コミュのID.


	std::string		m_commentserver;	// コメントサーバ.
	uint16_t		m_port;				// コメントサーバのポート.
	std::string		m_thread;			// コメントサーバのスレッド.
	tSocket			m_socket;			// コメントサーバ接続ソケット.

	std::queue<NicoNamaProgram>		m_program_queue;	// 番組の通知キュー.
	std::list<NicoNamaProgram>		m_recent_program;	// 最近通知した番組.
	bool	m_randompickup;

	std::string getTicket( const char*xml, int len );
	int getDataFrom2ndAuth( const char*xml, int len );

	int ShowNicoNamaInfoByBalloon( NicoNamaProgram &program );
	int ShowNicoNamaInfoByWindow( NicoNamaProgram &program );

public:
	NicoNamaAlert( HINSTANCE hinst, HWND hwnd );
	~NicoNamaAlert();

	inline HWND getParentHWND() const { return m_parenthwnd; }
	inline void setDisplayType( int t ){ m_displaytype = t; }
	inline void setRandomPickup( bool b ){ m_randompickup = b; }
	inline bool isRandomPickup() const { return m_randompickup; }

	int Auth( const char*mail, const char*password );
	int connectCommentServer();
	int keepalive();
	int receive( std::wstring &str );

	bool isParticipant( std::wstring&communityid );
	int ShowNicoNamaInfo( NicoNamaProgram &program );
	void addProgramQueue( NicoNamaProgram &program );
	void ShowNextNotifyWindow();
};


#endif
