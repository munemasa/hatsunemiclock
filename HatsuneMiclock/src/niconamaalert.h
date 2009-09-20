#ifndef NICONAMAALERT_H_DEF
#define NICONAMAALERT_H_DEF

#include "winheader.h"
#include "tNetwork.h"
#include "misc.h"

#include "libxml/tree.h"
#include "libxml/parser.h"

#include <string>
#include <vector>
#include <queue>
#include <list>
#include <map>

#include "define.h"

//----------------------------------------------------------------------
// macros
//----------------------------------------------------------------------
#define NICO_NOTICETYPE_WINDOW		0x00000001		// open window(常時有効)
#define NICO_NOTICETYPE_SOUND		0x00000002		// play sound(未使用)
#define NICO_NOTICETYPE_BROWSER		0x00000004		// run browser
#define NICO_NOTICETYPE_EXEC		0x00000008		// run command


//----------------------------------------------------------------------
// prototype
//----------------------------------------------------------------------
class tNotifyWindow;


//----------------------------------------------------------------------
// define
//----------------------------------------------------------------------
// これは通知ウィンドウに渡す情報.
struct NicoNamaProgram {
	// utf-8
	std::string		request_id;	// lvXXXXXXX
	std::string		title;
	std::string		description;

	std::string		community;	// coXXXX, chXXXX
	std::string		community_name;
	std::string		thumbnail;	// サムネURL

	char*			thumbnail_image;
	DWORD			thumbnail_image_size;

	time_t			start;		// 番組の開始時刻.
	bool			playsound;	// サウンド通知を行うか.
	bool			isparticipant;	// 参加コミュのものかどうか.
	int				posx;		// 表示位置X.
	int				posy;		// 表示位置Y.

	NicoNamaProgram(){ thumbnail_image = NULL; playsound=false; posx=posy=-1; }
	~NicoNamaProgram(){ SAFE_FREE( thumbnail_image ); }

	NicoNamaProgram( const NicoNamaProgram &src ){
		// thumbnail_imageのためのコピーコンストラクタ.
		this->isparticipant		= src.isparticipant;
		this->posx				= src.posx;
		this->posy				= src.posy;
		this->start				= src.start;
		this->playsound			= src.playsound;
		this->request_id		= src.request_id;
		this->title				= src.title;
		this->description		= src.description;
		this->community			= src.community;
		this->community_name	= src.community_name;
		this->thumbnail			= src.thumbnail;
		this->thumbnail_image_size = src.thumbnail_image_size;
		// 画像を新規メモリにコピー
		this->thumbnail_image	= (char*)malloc( this->thumbnail_image_size );
		CopyMemory( this->thumbnail_image, src.thumbnail_image, thumbnail_image_size );
	}
	NicoNamaProgram& operator=( const NicoNamaProgram &src ){
		// 代入演算のとき.
		SAFE_DELETE( this->thumbnail_image );
		this->isparticipant		= src.isparticipant;
		this->posx				= src.posx;
		this->posy				= src.posy;
		this->start				= src.start;
		this->playsound			= src.playsound;
		this->request_id		= src.request_id;
		this->title				= src.title;
		this->description		= src.description;
		this->community			= src.community;
		this->community_name	= src.community_name;
		this->thumbnail			= src.thumbnail;
		this->thumbnail_image_size = src.thumbnail_image_size;
		// 画像を新規メモリにコピー
		this->thumbnail_image = (char*)malloc( this->thumbnail_image_size );
		CopyMemory( this->thumbnail_image, src.thumbnail_image, thumbnail_image_size );
		return *this;
	}
};

// これはRSSで取得できる情報.
struct NicoNamaRSSProgram {
	std::string		title;			// 放送タイトル.
	std::string		link;			// http://live.nicovideo.jp/watch/lvXXXXX
	std::string		guid;			// lvXXXX
	std::string		description;	// 説明
	std::string		category;		// 動画紹介・リクエストとか一般(その他)とか.
	std::string		thumbnail;		// サムネのURL
	std::string		community_name;	// コミュ名.
	std::string		community_id;	// coXXXX
	std::string		owner_name;		// 生主の名前.
	int				num_res;		// コメ数.
	int				view;			// 来場者数.
	bool			member_only;	// メンバーオンリーフラグ.
	int				type;			// 0:unknown 1:community 2:official 3:あと何があるんだろう.
	time_t			pubDate;

	NicoNamaRSSProgram(){ pubDate = 0; }
};


// 通知方法.
struct NicoNamaNoticeType {
	std::wstring	command;
	DWORD			type;
	NicoNamaNoticeType(){
		command = L"";
		type = 0;
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
	int				m_displaytype;		///< アラートの表示方法.

	std::string		m_ticket;			///< 第一認証で取得するチケット.
	std::string		m_userid;
	std::string		m_userhash;
	std::vector<std::wstring>	m_communities;	///< 参加コミュのIDリスト.

	std::string		m_commentserver;	///< コメントサーバ.
	uint16_t		m_port;				///< コメントサーバのポート.
	std::string		m_thread;			///< コメントサーバのスレッド.
	tSocket			m_socket;			///< コメントサーバ接続ソケット.

	std::list<NicoNamaProgram>		m_program_queue;	///< 通知キュー.
	std::list<NicoNamaProgram>		m_recent_program;	///< 最近通知した番組.
	bool	m_randompickup;

	// キーはコミュニティID
	tCriticalSection m_rss_cs;
	std::map<std::string,NicoNamaRSSProgram>	m_rss_program;	///< RSSにある全番組.

	std::map<std::string,NicoNamaNoticeType>	m_noticetype;	///< コミュごとの通知タイプ指定.
	std::map<std::string,std::string>			m_community_name_cache;	///< コミュニティ名キャッシュ.

	std::string					m_matchkeyword;		///< キーワード通知用.
	std::list<std::string>		m_announcedlist;	///< 二重通知しないための通知済み一覧.

	std::string getTicket( const char*xml, int len );
	int getDataFrom2ndAuth( const char*xml, int len );

	int ShowNicoNamaNoticeByBalloon( NicoNamaProgram &program );
	int ShowNicoNamaNoticeByWindow( NicoNamaProgram &program );

	bool isAlreadyAnnounced( std::string& request_id );

public:
	void setRSSProgram( std::map<std::string,NicoNamaRSSProgram>& src ){
		m_rss_cs.enter();
		m_rss_program.clear();
		for(std::map<std::string,NicoNamaRSSProgram>::iterator i = src.begin(); i != src.end(); ++i){
		   m_rss_program[i->first] = i->second;
		}
		m_rss_cs.leave();
	}
	std::map<std::string,NicoNamaRSSProgram>& getRSSProgram(){
		m_rss_cs.enter();
		std::map<std::string,NicoNamaRSSProgram>& r = m_rss_program;
		m_rss_cs.leave();
		return r;
	}

	void setNoticeType( std::string& co, NicoNamaNoticeType& type ){ m_noticetype[ co ] = type; }
	NicoNamaNoticeType& getNoticeType( std::string& co ){ return m_noticetype[ co ]; }

	inline std::string& getMatchKeyword(){ return m_matchkeyword; }
	inline void setMatchKeyword( std::string& str ){ m_matchkeyword = str; }

	inline HWND getParentHWND() const { return m_parenthwnd; }
	inline void SetDisplayType( int t ){ m_displaytype = t; }
	inline void SetRandomPickup( bool b ){ m_randompickup = b; }
	inline bool isRandomPickup() const { return m_randompickup; }
	inline std::vector< std::wstring >& getCommunities(){ return m_communities; }
	std::wstring getCommunityName( std::wstring& commu_id );

	inline std::list<NicoNamaProgram>& getRecentList(){ return m_recent_program; }

	int Auth( const char*mail, const char*password );
	int ConnectCommentServer();
	int KeepAlive();
	int Receive( std::wstring &str );

	bool isParticipant( std::wstring&communityid );
	int ShowNicoNamaNotice( NicoNamaProgram &program );
	void ShowNextNoticeWindow();
	void AddNoticeQueue( NicoNamaProgram &program, bool nostack=false );

	HANDLE StartThread();

	int GetAllRSSAndUpdate();
	int GetAllRSS();
	int NotifyNowBroadcasting();
	int NotifyKeywordMatch();

	int Load();
	int Save();

	NicoNamaAlert( HINSTANCE hinst, HWND hwnd );
	~NicoNamaAlert();
};


#endif
