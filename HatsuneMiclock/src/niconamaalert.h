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
// macros & defines
//----------------------------------------------------------------------
#define NICO_NOTICETYPE_WINDOW		0x00000001		// open window(今は常時有効)
#define NICO_NOTICETYPE_SOUND		0x00000002		// play sound(今は常時フラグが立つ)
#define NICO_NOTICETYPE_BROWSER		0x00000004		// run browser
#define NICO_NOTICETYPE_EXEC		0x00000008		// run command

// 1ページ18番組なので、それぞれ(nicolive:total_count-1)/18+1回読めば全部読めることになる.
#define NICONAMA_COMMON_RSS		L"http://live.nicovideo.jp/recent/rss?tab=common&sort=start&p="	// 一般
#define NICONAMA_TRY_RSS		L"http://live.nicovideo.jp/recent/rss?tab=try&sort=start&p="	// やってみた
#define NICONAMA_GAME_RSS		L"http://live.nicovideo.jp/recent/rss?tab=live&sort=start&p="	// ゲーム
#define NICONAMA_REQUEST_RSS	L"http://live.nicovideo.jp/recent/rss?tab=req&sort=start&p="	// リクエスト
#define NICONAMA_FACE_RSS		L"http://live.nicovideo.jp/recent/rss?tab=face&sort=start&p="	// 顔出し
#define NICONAMA_R18_RSS		L"http://live.nicovideo.jp/recent/rss?tab=r18&sort=start&p="	// R-18
#define NICONAMA_MAX_RSS		(6)

#define NICONAMA_LOGIN1			L"https://secure.nicovideo.jp/secure/login?site=nicolive_antenna"
#define NICONAMA_LOGIN2			L"http://live.nicovideo.jp/api/getalertstatus"
#define NICONAMA_GETINFO		L"http://live.nicovideo.jp/api/getstreaminfo/lv"


//----------------------------------------------------------------------
// prototype
//----------------------------------------------------------------------
class tNotifyWindow;


//----------------------------------------------------------------------
// class & structure
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
		SAFE_DELETE( this->thumbnail_image );	// 古い画像は解放.
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
	HANDLE			m_thReceiver;		///< 受信スレッドのハンドル.

	int				m_displaytype;		///< アラートの表示方法.

	std::string		m_ticket;			///< 第一認証で取得するチケット.
	std::string		m_userid;
	std::string		m_userhash;

	std::string		m_commentserver;	///< コメントサーバのホスト名.
	uint16_t		m_port;				///< コメントサーバのポート.
	std::string		m_thread;			///< コメントサーバのスレッド.
	tSocket			m_socket;			///< コメントサーバ接続ソケット.

	std::vector<std::wstring>	m_communities;		///< 参加コミュのIDリスト.
	std::list<NicoNamaProgram>	m_program_queue;	///< 通知キュー.
	std::list<NicoNamaProgram>	m_recent_program;	///< 最近通知した番組(全体).
	std::list<NicoNamaProgram>	m_recent_commu_prog;///< 最近通知した番組(参加コミュのみ).
	bool	m_randompickup;

    /* libxmlはスレッド単位にワークを作っていて
     * RSS取得スレッドを作るたびにそのワークが増える一方。
     * 定期的に掃除するため、xmlの処理をやっていないのを
     * 見計らってやるためのクリティカルセクション。
     */
	tCriticalSection m_xml_cs;

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

	void ProcessNotify( std::string& str );

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
		// せめて書き換え中は返らないように.
		m_rss_cs.enter();
		std::map<std::string,NicoNamaRSSProgram>& r = m_rss_program;
		m_rss_cs.leave();
		return r;
	}

	void setNoticeType( std::string& co, NicoNamaNoticeType& type ){ m_noticetype[ co ] = type; }
	NicoNamaNoticeType& getNoticeType( const std::string& co ){ return m_noticetype[ co ]; }

	inline std::string& getMatchKeyword(){ return m_matchkeyword; }
	inline void setMatchKeyword( std::string& str ){ m_matchkeyword = str; }

	inline HWND getParentHWND() const { return m_parenthwnd; }
	inline void SetDisplayType( int t ){ m_displaytype = t; }
	inline void SetRandomPickup( bool b ){ m_randompickup = b; }
	inline bool isRandomPickup() const { return m_randompickup; }
	inline std::vector< std::wstring >& getCommunities(){ return m_communities; }

	std::wstring getCommunityName( const std::string& commu_id );
	std::wstring getCommunityName( const std::wstring& commu_id );

	inline std::list<NicoNamaProgram>& getRecentList(){ return m_recent_program; }
	inline std::list<NicoNamaProgram>& getRecentCommunityList(){ return m_recent_commu_prog; }
	inline std::map<std::string,NicoNamaNoticeType>& getNoticeTypeList(){ return m_noticetype; }

	int Auth( const char*mail, const char*password );
	int ConnectCommentServer();
	int KeepAlive();
	int Receive( std::string &str );
	void DoReceive();

	bool isParticipant( const std::string& str );
	bool isParticipant( const std::wstring&communityid );
	bool isParticipant( const WCHAR*communityid );
	bool hasNoticeType( const std::wstring&communityid );
	bool hasNoticeType( const std::string& communityid );

	int ShowNicoNamaNotice( NicoNamaProgram &program );
	void ShowNextNoticeWindow();
	void AddNoticeQueue( NicoNamaProgram &program, bool nostack=false );

	HANDLE StartThread();

	void wrapper_retrieve_rss_for_destructor();
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
