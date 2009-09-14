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

#define NICONAMA_COMMON_RSS		L"http://live.nicovideo.jp/recent/rss?tab=common&sort=start&p=1"	// ���
#define NICONAMA_TRY_RSS		L"http://live.nicovideo.jp/recent/rss?tab=try&sort=start&p=1"	// ����Ă݂�
#define NICONAMA_GAME_RSS		L"http://live.nicovideo.jp/recent/rss?tab=live&sort=start&p=1"	// �Q�[��
#define NICONAMA_REQUEST_RSS	L"http://live.nicovideo.jp/recent/rss?tab=req&sort=start&p=1"	// ���N�G�X�g
#define NICONAMA_FACE_RSS		L"http://live.nicovideo.jp/recent/rss?tab=face&sort=start&p=1"	// ��o��
#define NICONAMA_R18_RSS		L"http://live.nicovideo.jp/recent/rss?tab=r18&sort=start&p=1"	// R-18

#define NICONAMA_LOGIN1			L"https://secure.nicovideo.jp/secure/login?site=nicolive_antenna"
#define NICONAMA_LOGIN2			L"http://live.nicovideo.jp/api/getalertstatus"
#define NICONAMA_GETINFO		L"http://live.nicovideo.jp/api/getstreaminfo/lv"

//----------------------------------------------------------------------
// prototype
//----------------------------------------------------------------------
int GetBroadcastingInfo( std::wstring broadcastingid, NicoNamaProgram &program );


/** �j�R���A���[�g��M�X���b�h.
 *
 * @note CRT�g���̂� _beginthread() �ŊJ�n.
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
			// �����J�n�����R�~���𒲂ׂ܂���.
			// [����ID],[�`�����l�����R�~���j�e�BID],[������̃��[�U�[ID]
			std::vector<std::wstring> data;
			int start = str.find(L">",0)+1;
			int num = str.find(L"</chat>",0) - start;

			// <chat>�R�R</chat>�����o���� , �ŕ���.
			std::wstring tmp;
			tmp = str.substr( start, num );
			data = split( tmp, std::wstring(L",") );

			int p = (float)rand() / RAND_MAX * 100;
			if( !nico->isRandomPickup() ) p = 10000;
			// �����_���s�b�N�A�b�v�� 5% �Œ��.
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

/** �ԑg�����擾����.
 * @param broadcastingid �ԑgID
 * @param program �ԑg����ۑ������
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

// Shell_NotifyIcon�̃o���[���ŕ\��.
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

/** ����Window�ɂ���Ēʒm����.
 * ���̃��\�b�h�̓R�����g�T�[�o����̎�M�X���b�h����Ă΂�邽�߁A
 * �����Œʒm�E�B���h�E����炸���C���X���b�h�ɍ�点��B
 * ���̂��߂̃��b�Z�[�W���M�B
 */
int NicoNamaAlert::ShowNicoNamaInfoByWindow( NicoNamaProgram &program )
{
	SendMessage( m_parenthwnd, WM_NNA_NOTIFY, NNA_REQUEST_CREATEWINDOW, (LPARAM)&program );
	return 0;
}

/** �j�R���̕����J�n�ʒm��\������.
 * ����͌Ă΂ꂽ�u�ԂɃE�B���h�E���\�������B
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

/** �L���[�ɒ��܂��Ă��鎟�̒ʒm��\������.
 */
void NicoNamaAlert::ShowNextNotifyWindow()
{
	if( !m_program_queue.empty() ){
		NicoNamaProgram program = m_program_queue.front();
		m_program_queue.pop();
		ShowNicoNamaInfo( program );
	}
}

/** �ʒm���L���[�ɒ��߂�.
 * �ʒm�E�B���h�E���Ȃ��Ƃ��ɂ́A�L���[�ɕۑ������ɑ����ɕ\������B
 */
void NicoNamaAlert::addProgramQueue( NicoNamaProgram &program )
{
	if( this->m_displaytype==this->NNA_BALLOON ){
		// �o���[���^�C�v�̂Ƃ��͑����ł�����.
		ShowNicoNamaInfo( program );
	}else{
		dprintf( L"notify window queue %d.\n", m_program_queue.size() );
		if( FindWindow( T_NOTIFY_CLASS, NULL ) ){
			// ���łɒʒm�E�B���h�E������̂ŃL���[�ɒ��߂Ă�����.
			dprintf( L"Already notify window...queueing.\n" );
			m_program_queue.push( program );
		}else{
			ShowNicoNamaInfo( program );
		}
	}
}

/** �j�R���T�[�o�ŔF�؂���.
 * @return 0�͐����A���̒l�͎��s
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

	// ���F�؂Ń`�P�b�g���擾����.
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
			// ���F�؂Ŋe������擾����.
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

// �R�����g�T�[�o�ɐڑ�.
int NicoNamaAlert::connectCommentServer()
{
	int r = m_socket.connect( m_commentserver.c_str(), m_port );
	keepalive();
	// 5�����炢��.
	SetTimer( this->m_parenthwnd, TIMER_ID_NICO_KEEPALIVE, 5*60*1000, NULL );
	return r;
}

// 5����1�񂭂炢�Ă�ł����Α��v����.
int NicoNamaAlert::keepalive()
{
	int s;
	std::string str;
	str = "<thread thread=\"" + m_thread + "\" res_from=\"-1\" version=\"20061206\"/>\0";
	s  = m_socket.send( str.c_str(), str.length()+1 ); // NUL�������݂�.
	dprintf( L"%d byte sent.\n", s );
	return s;
}

int NicoNamaAlert::getDataFrom2ndAuth( const char*xmltext, int len )
{
	tXML xml( xmltext, len );
	xmlNode *communities;
	xmlNode *ms;
	char*tmp;

	// ���񃋁[�g����m�[�h��T�����́A�c���[���~��Ă����Ȃ��珇���s�b�N�A�b�v��������������.
	// ���̕ӂ͂Ă��Ɓ[��.
	tmp = xml.FindFirstTextNodeFromRoot( "user_id" ); if(!tmp)tmp="";
	m_userid	= tmp;
	tmp = xml.FindFirstTextNodeFromRoot( "user_hash" ); if(!tmp)tmp="";
	m_userhash	= tmp;

	m_communities.clear();
	communities = xml.FindFirstNodeFromRoot( "communities" ); if(!communities)return -1;
	//communities�̎q�m�[�h��"community_id"�̗񋓂̈�K�w�Ȃ̂�.
	for( xmlNode*current = communities->children; current; current=current->next ){
		// �Ƃ肠�����m�[�h���̃`�F�b�N�������Ƃ���.
		if( strcmp( (char*)current->name, "community_id" )==0 ){
			if( current->children && current->children->content ){
				// �e�L�X�g�m�[�h�O���.
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
		// ������ƃo�b�t�@�����O���������ق��������̂����ǖʓ|�Ȃ̂�1�o�C�g���ǂ񂶂Ⴂ�܂���.
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

// �R�~���ɎQ�����Ă�H.
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
