/*
http://dic.nicovideo.jp/a/%E3%83%8B%E3%82%B3%E7%94%9F%E3%82%A2%E3%83%A9%E3%83%BC%E3%83%88%28%E6%9C%AC%E5%AE%B6%29%E3%81%AE%E4%BB%95%E6%A7%98
 */
#include "winheader.h"
#include <string>
#include <process.h>

#include "libxml/tree.h"
#include "libxml/parser.h"

#include "tNetwork.h"
#include "tXML.h"
#include "tNotifyWindow.h"
#include "udmessages.h"
#include "niconamaalert.h"

#include "stringlib.h"
#include "debug.h"

#include "../resource.h"

#pragma comment(lib,"libxml2_a.lib")

#define NICONAMA_COMMON_RSS		L"http://live.nicovideo.jp/recent/rss?tab=common&sort=start&p=1"	// 一般
#define NICONAMA_TRY_RSS		L"http://live.nicovideo.jp/recent/rss?tab=try&sort=start&p=1"	// やってみた
#define NICONAMA_GAME_RSS		L"http://live.nicovideo.jp/recent/rss?tab=live&sort=start&p=1"	// ゲーム
#define NICONAMA_REQUEST_RSS	L"http://live.nicovideo.jp/recent/rss?tab=req&sort=start&p=1"	// リクエスト
#define NICONAMA_FACE_RSS		L"http://live.nicovideo.jp/recent/rss?tab=face&sort=start&p=1"	// 顔出し
#define NICONAMA_R18_RSS		L"http://live.nicovideo.jp/recent/rss?tab=r18&sort=start&p=1"	// R-18

#define NICONAMA_LOGIN1			L"https://secure.nicovideo.jp/secure/login?site=nicolive_antenna"
#define NICONAMA_LOGIN2			L"http://live.nicovideo.jp/api/getalertstatus"
#define NICONAMA_GETINFO		L"http://live.nicovideo.jp/api/getstreaminfo/lv"

//----------------------------------------------------------------------
// prototype
//----------------------------------------------------------------------
int GetBroadcastingInfo( std::wstring broadcastingid, NicoNamaProgram &program );


/** ニコ生アラート受信スレッド.
 *
 * @note CRT使うので _beginthread() で開始.
 */
void thNicoNamaAlert(LPVOID v)
{
	std::wstring str;
	NicoNamaAlert *nico = (NicoNamaAlert*)v;

	dprintf( L"NicoNamaAlert thread started.\n" );
	while(1){
		int r;
		r = nico->receive( str );
		if( r<=0 ) break;

		//dprintf( L"NNA:[%s]\n", str.c_str() );
		if( str.find(L"chat",0)!=std::wstring::npos ){
			// 放送開始したコミュを調べますよ.
			// [放送ID],[チャンネル＆コミュニティID],[放送主のユーザーID]
			std::vector<std::wstring> data;
			int start = str.find(L">",0)+1;
			int num = str.find(L"</chat>",0) - start;

			// <chat>ココ</chat>を取り出して , で分割.
			std::wstring tmp;
			tmp = str.substr( start, num );
			data = split( tmp, std::wstring(L",") );

			int p = (float)rand() / RAND_MAX * 100;
			if( !nico->isRandomPickup() ) p = 10000;
			// ランダムピックアップは 5% 固定で.
			if( p<5 || nico->isParticipant( data.at(1) ) ){
				dprintf( L"Your community(%s)'s broadcasting is started.\n", data[1].c_str() );
				NicoNamaProgram program;
				GetBroadcastingInfo( data[0], program );
				nico->addProgramQueue( program );
			}
		}
	}
	dprintf( L"NicoNamaAlert thread ended.\n" );
	_endthread();
}


char*GetTextFromXMLRoot( tXML&xml, char*node )
{
	char*tmp;
	tmp = xml.FindFirstTextNodeFromRoot( node );
	if( !tmp ) tmp = "";
	return tmp;
}

/** 番組情報を取得する.
 * @param broadcastingid 番組ID
 * @param program 番組情報を保存する先
 */
int GetBroadcastingInfo( std::wstring broadcastingid, NicoNamaProgram &program )
{
	tHttp http;
	std::wstring url;
	char*data;
	DWORD datalen;
	char*tmp;
    int httpret;

	url = NICONAMA_GETINFO;
	url += broadcastingid;
	httpret = http.request( url.c_str(), &data, &datalen );
    dprintf( L"httpret=%d\n", httpret );

	tXML xml( data, datalen );
	program.request_id		= GetTextFromXMLRoot( xml, "request_id" );
	program.title			= GetTextFromXMLRoot( xml, "title" );
	program.description		= GetTextFromXMLRoot( xml, "description" );
	program.community		= GetTextFromXMLRoot( xml, "default_community" );
	program.community_name	= GetTextFromXMLRoot( xml, "name" );
	program.thumbnail		= GetTextFromXMLRoot( xml, "thumbnail" );

	std::wstring wstr;
	strtowstr( program.thumbnail, wstr );
	httpret = http.request( wstr.c_str(), &program.thumbnail_image, &program.thumbnail_image_size );
    dprintf( L"httpret=%d\n", httpret );
	dprintf( L"thumbnail image size %d bytes.\n", program.thumbnail_image_size );

	free( data );
	return 0;
}

// Shell_NotifyIconのバルーンで表示.
int NicoNamaAlert::ShowNicoNamaInfoByBalloon( NicoNamaProgram &program )
{
	NOTIFYICONDATA icon;
	ZeroMemory( &icon, sizeof(icon) );
	icon.cbSize = sizeof(icon);
	icon.hWnd = m_parenthwnd;
	icon.uVersion = NOTIFYICON_VERSION;
	icon.uFlags = NIF_INFO;

	std::wstring wstr;

	strtowstr( program.title, wstr );
	wcsncpy( icon.szInfoTitle, wstr.c_str(), 63 );

	strtowstr( program.description, wstr );
	wcsncpy( icon.szInfo, wstr.c_str(), 255 );

	icon.uTimeout = 20*1000;
	icon.dwInfoFlags = NIIF_INFO;

	Shell_NotifyIcon( NIM_MODIFY, &icon );
	return 0;
}

/** 自作Windowによって通知する.
 * このメソッドはコメントサーバからの受信スレッドから呼ばれるため、
 * ここで通知ウィンドウを作らずメインスレッドに作らせる。
 * そのためのメッセージ送信。
 */
int NicoNamaAlert::ShowNicoNamaInfoByWindow( NicoNamaProgram &program )
{
	SendMessage( m_parenthwnd, WM_NNA_NOTIFY, NNA_REQUEST_CREATEWINDOW, (LPARAM)&program );
	return 0;
}

/** ニコ生の放送開始通知を表示する.
 * これは呼ばれた瞬間にウィンドウが表示される。
 */
int NicoNamaAlert::ShowNicoNamaInfo( NicoNamaProgram &program )
{
	switch( m_displaytype ){
	case NNA_BALLOON:
		ShowNicoNamaInfoByBalloon( program );
		break;

	case NNA_WINDOW:
		ShowNicoNamaInfoByWindow( program );
		break;

	default:
		break;
	}
	return 0;
}

/** キューに貯まっている次の通知を表示する.
 */
void NicoNamaAlert::ShowNextNotifyWindow()
{
	if( !m_program_queue.empty() ){
		NicoNamaProgram program = m_program_queue.front();
		m_program_queue.pop();
		ShowNicoNamaInfo( program );
	}
}

/** 通知をキューに貯める.
 * 通知ウィンドウがないときには、キューに保存せずに即時に表示する。
 */
void NicoNamaAlert::addProgramQueue( NicoNamaProgram &program )
{
	if( this->m_displaytype==this->NNA_BALLOON ){
		// バルーンタイプのときは即時でいいや.
		ShowNicoNamaInfo( program );
	}else{
		dprintf( L"notify window queue %d.\n", m_program_queue.size() );
		if( FindWindow( T_NOTIFY_CLASS, NULL ) ){
			// すでに通知ウィンドウがあるのでキューに貯めておこう.
			dprintf( L"Already notify window...queueing.\n" );
			m_program_queue.push( program );
		}else{
			ShowNicoNamaInfo( program );
		}
	}
}

/** ニコ生サーバで認証する.
 * @return 0は成功、負の値は失敗
 */
int NicoNamaAlert::Auth( const char*mail, const char*password )
{
	tHttp http;
	char*recvdata;
	DWORD recvlen;
	std::string postdata;
	std::string email;
	std::string pass;
	int r;

	// 第一認証でチケットを取得する.
	email = urlencode( mail );
	postdata = "mail=";
	postdata += email;
	postdata += "&password=";
	postdata += password;
	r = http.request( NICONAMA_LOGIN1, &recvdata, &recvlen, postdata.c_str(), postdata.length() );
	if( r<0 ){
		if( r==-999 ){
			MessageBox( NULL, L"Your Windows system is not supported for WinHttp.", L"Unsupported Windows", MB_OK );
		}
		return -1;
	}else{
		m_ticket = getTicket( recvdata, recvlen );
		free( recvdata );
		if( m_ticket.length()>0 ){
			// 第二認証で各種情報を取得する.
			postdata = "ticket=" + m_ticket;
			http.request( NICONAMA_LOGIN2, &recvdata, &recvlen, postdata.c_str(), postdata.length() );
			getDataFrom2ndAuth( recvdata, recvlen );
			free( recvdata );
			if( m_userid.length()<=0 ){
				dprintf( L"failed 2nd auth\n" );
				return -1;
			}
		}else{
			dprintf( L"cannot retrieve ticket.\n" );
			return -1;
		}
	}
	return 0;
}

// コメントサーバに接続.
int NicoNamaAlert::connectCommentServer()
{
	int r = m_socket.connect( m_commentserver.c_str(), m_port );
	keepalive();
	// 5分くらいで.
	SetTimer( this->m_parenthwnd, TIMER_ID_NICO_KEEPALIVE, 5*60*1000, NULL );
	return r;
}

// 5分に1回くらい呼んでおけば大丈夫かな.
int NicoNamaAlert::keepalive()
{
	int s;
	std::string str;
	str = "<thread thread=\"" + m_thread + "\" res_from=\"-1\" version=\"20061206\"/>\0";
	s  = m_socket.send( str.c_str(), str.length()+1 ); // NUL文字込みで.
	dprintf( L"%d byte sent.\n", s );
	return s;
}

int NicoNamaAlert::getDataFrom2ndAuth( const char*xmltext, int len )
{
	tXML xml( xmltext, len );
	xmlNode *communities;
	xmlNode *ms;
	char*tmp;

	// 毎回ルートからノードを探すよりは、ツリーを降りていきながら順次ピックアップが効率いいけど.
	// この辺はてけとーに.
	tmp = xml.FindFirstTextNodeFromRoot( "user_id" ); if(!tmp)tmp="";
	m_userid	= tmp;
	tmp = xml.FindFirstTextNodeFromRoot( "user_hash" ); if(!tmp)tmp="";
	m_userhash	= tmp;

	m_communities.clear();
	communities = xml.FindFirstNodeFromRoot( "communities" ); if(!communities)return -1;
	//communitiesの子ノードは"community_id"の列挙の一階層なので.
	for( xmlNode*current = communities->children; current; current=current->next ){
		// とりあえずノード名のチェックだけしとこう.
		if( strcmp( (char*)current->name, "community_id" )==0 ){
			if( current->children && current->children->content ){
				// テキストノード前提で.
				std::string str;
				std::wstring wstr;
				str = (char*)current->children->content;
				strtowstr( str, wstr );

				m_communities.push_back( wstr );
				dprintf( L"YourCommunity %s\n", wstr.c_str() );
			}
		}
	}

	ms = xml.FindFirstNodeFromRoot( "ms" );
	tmp = xml.FindFirstTextNode( ms, "addr" ); if(!tmp)tmp="";
	m_commentserver = tmp;
	tmp = xml.FindFirstTextNode( ms, "port" ); if(!tmp)tmp="0";
	m_port = atoi( tmp );
	tmp = xml.FindFirstTextNode( ms, "thread" ); if(!tmp)tmp="";
	m_thread = tmp;
	return 0;
}

std::string NicoNamaAlert::getTicket( const char*xmltext, int len )
{
	char*text;
	tXML xml( xmltext, len );
	std::string s = "";

	text = xml.FindFirstTextNodeFromRoot( "ticket" );
	if( text ){
		s = text;
	}
	return s;
}

int NicoNamaAlert::receive( std::wstring &str )
{
	int r = 0;
	str = L"";
	while(1){
		// きちんとバッファリング処理したほうがいいのだけど面倒なので1バイトずつ読んじゃいますよ.
		char ch;
		if( m_socket.recv( &ch, 1 )<=0 ){
			r = 0;
			break;
		}
		r++;
		if( ch=='\0' ) break;
		str += ch;
	}
	return r;
}

// コミュに参加してる？.
bool NicoNamaAlert::isParticipant( std::wstring&communityid )
{
	std::vector<std::wstring>::iterator it;
	for( it=m_communities.begin(); it!=m_communities.end(); it++ ){
		if( (*it).compare( communityid )==0 ){
			return true;
		}
	}
	return false;
}

NicoNamaAlert::NicoNamaAlert( HINSTANCE hinst, HWND hwnd )
{
	m_hinst = hinst;
	m_parenthwnd = hwnd;
	setRandomPickup( false );
#if 0
	m_displaytype = NNA_BALLOON;
#else
	m_displaytype = NNA_WINDOW;
#endif
}

NicoNamaAlert::~NicoNamaAlert()
{
}
