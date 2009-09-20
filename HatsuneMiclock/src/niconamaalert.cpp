/*
http://dic.nicovideo.jp/a/%E3%83%8B%E3%82%B3%E7%94%9F%E3%82%A2%E3%83%A9%E3%83%BC%E3%83%88%28%E6%9C%AC%E5%AE%B6%29%E3%81%AE%E4%BB%95%E6%A7%98
 */
#include "winheader.h"
#include <string>
#include <process.h>
#include <time.h>

#include "libxml/tree.h"
#include "libxml/parser.h"

#include "tNetwork.h"
#include "tXML.h"
#include "tNotifyWindow.h"
#include "udmessages.h"
#include "niconamaalert.h"

#include "myregexp.h"

#include "stringlib.h"
#include "debug.h"

#include "../resource.h"

#pragma comment(lib,"libxml2_a.lib")

#pragma warning( disable : 4996 ) 

// 1�y�[�W18�ԑg�Ȃ̂ŁA���ꂼ��(nicolive:total_count-1)/18+1��ǂ߂ΑS���ǂ߂邱�ƂɂȂ�.
#define NICONAMA_COMMON_RSS		L"http://live.nicovideo.jp/recent/rss?tab=common&sort=start&p="	// ���
#define NICONAMA_TRY_RSS		L"http://live.nicovideo.jp/recent/rss?tab=try&sort=start&p="	// ����Ă݂�
#define NICONAMA_GAME_RSS		L"http://live.nicovideo.jp/recent/rss?tab=live&sort=start&p="	// �Q�[��
#define NICONAMA_REQUEST_RSS	L"http://live.nicovideo.jp/recent/rss?tab=req&sort=start&p="	// ���N�G�X�g
#define NICONAMA_FACE_RSS		L"http://live.nicovideo.jp/recent/rss?tab=face&sort=start&p="	// ��o��
#define NICONAMA_R18_RSS		L"http://live.nicovideo.jp/recent/rss?tab=r18&sort=start&p="	// R-18
#define NICONAMA_MAX_RSS		(6)

#define NICONAMA_LOGIN1			L"https://secure.nicovideo.jp/secure/login?site=nicolive_antenna"
#define NICONAMA_LOGIN2			L"http://live.nicovideo.jp/api/getalertstatus"
#define NICONAMA_GETINFO		L"http://live.nicovideo.jp/api/getstreaminfo/lv"


//----------------------------------------------------------------------
// prototype
//----------------------------------------------------------------------
int GetBroadcastingInfo( std::wstring broadcastingid, NicoNamaProgram &program );
int GetBroadcastingInfoFromRSS( xmlNodePtr item, NicoNamaRSSProgram &program );


static wchar_t *g_rsslist[NICONAMA_MAX_RSS] = {
	NICONAMA_COMMON_RSS,
	NICONAMA_TRY_RSS,
	NICONAMA_GAME_RSS,
	NICONAMA_REQUEST_RSS,
	NICONAMA_FACE_RSS,
	NICONAMA_R18_RSS,
};

// RSS�ǂݍ��݃X���b�h�Ƃ̒ʐM�p.
struct RssReadRequest {
	std::wstring rssurl;    ///< RSS��URL
	char	*data;          ///< DL�����f�[�^(���Ƃ�free()���邱��)
	DWORD	datalen;        ///< DL�����T�C�Y.
	HANDLE	hThread;        ///< �X���b�h�n���h��.
	int index;
};


void AddRSSProgram( std::map<std::string,NicoNamaRSSProgram>& rss, NicoNamaRSSProgram &program )
{
	typedef std::pair<std::string,NicoNamaRSSProgram> thePair;
	rss.insert( thePair(program.community_id, program) );
}

int AddRSSProgram( std::map<std::string,NicoNamaRSSProgram>& rss, xmlNodePtr channel )
{
    xmlNodePtr node;
    int total_num = 0;
    if( !channel ) return 0;

    for( node=channel->children; node; node=node->next ){
        if( node->type!=XML_ELEMENT_NODE ) continue;
        if( strcmp( (char*)node->name,"total_count" )==0 ){
			if( node->children && node->children->content ){
				total_num = atoi( (char*)node->children->content );
				dprintf( L"total_count=%d\n", total_num );
			}
        }
        if( strcmp( (char*)node->name,"item" )==0 ){
            // �ԑg�X�̏��.
            NicoNamaRSSProgram prog;
            GetBroadcastingInfoFromRSS( node, prog );
            AddRSSProgram( rss, prog );
            //dprintf( L"community_id=%S\n", prog.community_id.c_str() );
        }
    }
    return total_num;
}



/** RSS���擾���čX�V�ʒm�𓊂���܂ł����X���b�h.
 * _beginthread()�ɂ���ċN������āARSS��ǂ�Őe�E�B���h�E�ɑ΂���DL�����ʒm�𓊂���.
 * WM_TIMER�ɂ���Đe�E�B���h�E����Ă΂�Ă���.
 */
static void thNicoNamaAlertGetRSSAndUpdate(LPVOID v)
{
	NicoNamaAlert *nico = (NicoNamaAlert*)v;
	dprintf( L"Retrieving RSS...\n" );
	nico->GetAllRSS();
	dprintf( L"done.\n" );
	// �ԑg���X�g���X�V������.
	SendMessage( nico->getParentHWND(), WM_NNA_NOTIFY, NNA_UPDATE_RSS, 0 );

	nico->NotifyKeywordMatch();

	_endthread();
}

/** �j�R���A���[�g��M�X���b�h.
 *
 * @note CRT�g���̂� _beginthread() �ŊJ�n. �Ăяo���K����__cdecl
 */
static void thNicoNamaAlertReceiver(LPVOID v)
{
	std::wstring str;
	NicoNamaAlert *nico = (NicoNamaAlert*)v;

	// �܂��J�n����RSS������Ă��āA�������̂��̂�ʒm���悤.
	dprintf( L"NicoNamaAlert thread started.\n" );
	dprintf( L"Retrieving RSS...\n" );
	nico->GetAllRSS();
	dprintf( L"done.\n" );

	// RSS�Ŏ擾�����������ɎQ���R�~���̕������̂��̂�ʒm������.
	nico->NotifyNowBroadcasting();
	nico->KeepAlive();	// ��M�J�n����.

	// �L�[���[�h�}�b�`�������̂�ʒm����.
	nico->NotifyKeywordMatch();

	SetTimer( nico->getParentHWND(), TIMER_ID_RSS_UPDATE, NICONAMA_RSS_GET_INTERVAL*60*1000, NULL );

	// �ԑg���X�g���X�V������.
	SendMessage( nico->getParentHWND(), WM_NNA_NOTIFY, NNA_UPDATE_RSS, 0 );

	while(1){
		// ���Ƃ͂����Ǝ�M�ҋ@.
		int r;
		r = nico->Receive( str );
		if( r<=0 ) break;
		if( str.find(L"<chat",0)==std::wstring::npos ) continue;
		dprintf( L"NNA:%s\n", str.c_str() );

		// �����J�n�����R�~���𒲂ׂ܂���.
		// [����ID],[�`�����l�����R�~���j�e�BID],[������̃��[�U�[ID]
		std::string mbstr;
		wstrtostr( str, mbstr );
		tXML xml( mbstr.c_str(), mbstr.length() );

		// <chat>�R�R</chat>�����o���� , �ŕ���.
		xmlNodePtr root = xml.getRootNode();
		if( !root || !root->children || !root->children->content ) continue;

		std::vector<std::wstring> data;
		std::wstring wtmp;
		std::string text = (char*)root->children->content;
		strtowstr( text, wtmp );
		data = split( wtmp, std::wstring(L",") );

		int p = (float)rand() / RAND_MAX * 100;
		if( !nico->isRandomPickup() ) p = 10000;
		bool ispart = nico->isParticipant( data.at(1) );
		// �����_���s�b�N�A�b�v�� 5% �Œ��.
		if( p<5 || ispart ){
			char*d = xml.FindAttribute( xml.getRootNode(), "date" );
			std::string datestr = d?d:"0";
#ifdef _USE_32BIT_TIME_T
			time_t starttime = atoi( datestr.c_str() );
#else
			time_t starttime = _atoi64( datestr.c_str() );
#endif
			dprintf( L"Community(%s)'s broadcasting is started.\n", data[1].c_str() );
			NicoNamaProgram program;
			GetBroadcastingInfo( data[0], program );    // data[0]�͕���ID
			program.start		  = starttime;
			program.isparticipant = ispart;
			program.playsound	  = ispart;
			nico->AddNoticeQueue( program );
		}
	}
	dprintf( L"NicoNamaAlert thread ended.\n" );
	_endthread();
}

/** RSS������Ă��邾��.
 *_beginthreadex()���g��. __stdcall��.
 */
static unsigned WINAPI thRetrieveRSS(LPVOID v)
{
	RssReadRequest *req = (RssReadRequest*)v;
	tHttp http;
	http.request( req->rssurl.c_str(), &req->data, &req->datalen );
	_endthreadex(0);
	return 0;
}

/** RSS��S���擾.
 * _beginthreadex()�ō��. __stdcall��.
 * �E�F�C�g�Ȃ��ň�C�Ɏ擾���Ă��邯�ǖ��Ȃ�����.
 * �j�R���A���[�g(��a)���m�[�E�F�C�g�łǂ�ǂ����Ă邩�炢�����낤.
 */
static unsigned WINAPI thNicoNamaRetriveAllRSS(LPVOID v)
{
	NicoNamaAlert *nico = (NicoNamaAlert*)v;
	std::map<std::string,NicoNamaRSSProgram> rss_list;
	std::list<RssReadRequest*> reqlist;

    // �ŏ���1�y�[�W�ڂ𓯎��ɃA�N�Z�X���邭�炢�͂������.
	for(int i=0; i<NICONAMA_MAX_RSS; i++ ){
		wchar_t *rss = g_rsslist[i];
		RssReadRequest *rssreq = new RssReadRequest();
		rssreq->index	= i;
		rssreq->rssurl	= rss;
		rssreq->rssurl	+= L"1";
		rssreq->hThread	= (HANDLE)_beginthreadex( NULL, 0, thRetrieveRSS, rssreq, 0, NULL );
		if( rssreq->hThread==(HANDLE)1L ){
			int err = errno;
			delete rssreq;
			break;
		}
		reqlist.push_back( rssreq );
	}

	std::list<RssReadRequest*>::iterator it;
	for( it=reqlist.begin(); it!=reqlist.end(); it++ ){
		// 6�J�e�S�����Ƃɏ�������.
		WaitForSingleObject( (*it)->hThread, INFINITE );

		tXML xml( (*it)->data, (*it)->datalen );
		xmlNodePtr channel;
		int total_num = 0;

		channel = xml.FindFirstNodeFromRoot("channel");
		total_num = ::AddRSSProgram( rss_list, channel );	// 1�y�[�W��.

		int maxpages = (total_num-1)/18+1;	// 1�y�[�W18������̂�.
		// RSS��2�y�[�W�ڂ���S�����.
		for( int cnt=2; cnt<=maxpages; ){
			std::list<RssReadRequest*> subreqlist;

			// (HTTP/1.1�Ȃ̂�)2�y�[�W�����ɗv��.
			for(int j=0; j<2 && cnt<=maxpages; cnt++,j++){
				RssReadRequest *subreq = new RssReadRequest();
				wchar_t tmp[128];
				subreq->rssurl = g_rsslist[(*it)->index];
				subreq->rssurl += _itow( cnt, tmp, 10 );
				subreq->hThread = (HANDLE)_beginthreadex( NULL, 0, thRetrieveRSS, subreq, 0, NULL );
				if( subreq->hThread==(HANDLE)1L ){
					// error occurred
					int err = errno;
					delete subreq;
					break;
				}
				subreqlist.push_back( subreq );
			}

			// �v��������������M��҂��ď���.
			std::list<RssReadRequest*>::iterator subit;
			for( subit=subreqlist.begin(); subit!=subreqlist.end(); subit++ ){
				WaitForSingleObject( (*subit)->hThread, INFINITE );

				tXML xml( (*subit)->data, (*subit)->datalen );
				xmlNodePtr channel;
				channel = xml.FindFirstNodeFromRoot("channel");

				::AddRSSProgram( rss_list, channel );

				CloseHandle( (*subit)->hThread );
				free( (*subit)->data );
				delete (*subit);
			}
		}
		CloseHandle( (*it)->hThread );
		free( (*it)->data );
		delete (*it);
	}
	nico->setRSSProgram( rss_list );
	_endthreadex(0);
	return 0;
}


char*GetTextFromXMLRoot( tXML&xml, char*node )
{
	char*tmp;
	tmp = xml.FindFirstTextNodeFromRoot( node );
	if( !tmp ) tmp = "";
	return tmp;
}

static inline char*get_textcontent( xmlNodePtr node )
{
	if( node && node->children && node->children->content ){
		return (char*)node->children->content;
	}
	return "";
}

int GetBroadcastingInfoFromRSS( xmlNodePtr item, NicoNamaRSSProgram &program )
{
	xmlNodePtr node;
	for( node=item->children; node; node=node->next ){
		// �����񂲂Ƃɕ��ނ���̂��ĂȂ񂩂������@�Ȃ����ȁ[�Ƃ����v��.
		// switch-case�݂����Ȑ^�����ł���Ƃǂ�Ȃɕ֗���.
		// for(i=0;...){ if(strcmp(str,match[i])==0) break; }
		// switch(i){ case 0:... } �͖��� i �����߂Ă�̂�������̋C�����邵.
		if( node->type!=XML_ELEMENT_NODE ) continue;

		if( strcmp( (char*)node->name, "title" )==0 ){
			program.title = get_textcontent( node );

		}else if( strcmp( (char*)node->name, "link" )==0 ){
			program.link = get_textcontent( node );

		}else if( strcmp( (char*)node->name, "guid" )==0 ){
			program.guid = get_textcontent( node );

		}else if( strcmp( (char*)node->name, "pubDate" )==0 ){
			// ���xRFC822�`���Ƃ��̎����t�H�[�}�b�g��time_t�ɕϊ�����֐���p�ӂ��悤.
			// ���̂Ƃ���j�R����RSS���o���Ă�`���݂̂�.
			char *month[]={	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL };
			struct tm tm;
			char buf[128];
			int i;
			ZeroMemory( &tm, sizeof(tm) );
			sscanf( get_textcontent( node ),
					"%*s %d %s %d %d:%d:%d %*s",
					&tm.tm_mday, buf, &tm.tm_year,
					&tm.tm_hour, &tm.tm_min, &tm.tm_sec );
			tm.tm_year -= 1900;
			for(i=0; month[i]!=NULL; i++){
				if( strcmp( month[i],buf )==0 ){
					break;
				}
			}
			tm.tm_mon = i;
			program.pubDate = mktime( &tm );

		}else if( strcmp( (char*)node->name, "description" )==0 ){
			program.description = get_textcontent( node );

		}else if( strcmp( (char*)node->name, "category" )==0 ){
			if( !program.category.length() ){
				program.category = get_textcontent( node );
			}

		}else if( strcmp( (char*)node->name, "thumbnail" )==0 ){
			xmlAttrPtr prop = node->properties;
			for( ; prop; prop = prop->next ){
				if( prop->type==XML_ATTRIBUTE_NODE && strcmp((char*)prop->name,"url")==0 ){
					program.thumbnail = (char*)prop->children->content;
				}
			}

		}else if( strcmp( (char*)node->name, "creator" )==0 ){
			// none

		}else if( strcmp( (char*)node->name, "community_name" )==0 ){
			program.community_name = get_textcontent( node );

		}else if( strcmp( (char*)node->name, "community_id" )==0 ){
			program.community_id = get_textcontent( node );

		}else if( strcmp( (char*)node->name, "num_res" )==0 ){
			program.num_res = atoi( get_textcontent( node ) );

		}else if( strcmp( (char*)node->name, "view" )==0 ){
			program.view = atoi( get_textcontent( node ) );

		}else if( strcmp( (char*)node->name, "member_only" )==0 ){
			char*tmp = get_textcontent( node );
			if( strcmp( tmp,"true" )==0 ){
				program.member_only = true;
			}else{
				program.member_only = false;
			}

		}else if( strcmp( (char*)node->name, "type" )==0 ){
			char*tmp = get_textcontent( node );
			if( strcmp(tmp,"community")==0 ){
				program.type = 1;
			}else if( strcmp(tmp,"official")==0 ){
				program.type = 2;
			}else{
				program.type = 0;
			}

		}else if( strcmp( (char*)node->name, "owner_name" )==0 ){
			program.owner_name = get_textcontent( node );
		}
	}
	return 0;
}

/** �ԑg�����擾����.
 * getstreaminfo������炦����݂̂��Z�b�g����̂ŁA���̃����o�̐ݒ�͕ʂ̂Ƃ���ŁB
 * @param broadcastingid �ԑgID
 * @param program �ԑg����ۑ������
 */
int GetBroadcastingInfo( std::wstring broadcastingid, NicoNamaProgram &program )
{
	tHttp http;
	std::wstring url;
	char*data;
	DWORD datalen;
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
int NicoNamaAlert::ShowNicoNamaNoticeByBalloon( NicoNamaProgram &program )
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
int NicoNamaAlert::ShowNicoNamaNoticeByWindow( NicoNamaProgram &program )
{
	SendMessage( m_parenthwnd, WM_NNA_NOTIFY, NNA_REQUEST_CREATEWINDOW, (LPARAM)&program );

	NicoNamaNoticeType& alert = m_noticetype[ program.community ];
	if( alert.type & NICO_NOTICETYPE_BROWSER ){
		std::wstring url;
		std::wstring tmp;
		strtowstr( program.request_id, tmp );
		url = NICO_LIVE_URL + tmp;
		OpenURL( url );
	}
	if( alert.type & NICO_NOTICETYPE_SOUND ){
	}
	if( alert.type & NICO_NOTICETYPE_EXEC ){
		std::wstring wstr;
		std::wstring cmd;
		strtowstr( program.request_id, wstr );

		ShellExecute( getParentHWND(), L"open", alert.command.c_str(), wstr.c_str(), NULL, SW_SHOWNORMAL );
	}
	return 0;
}

/** �j�R���̕����J�n�ʒm��\������.
 * ����͌Ă΂ꂽ�u�ԂɃE�B���h�E���\�������B
 */
int NicoNamaAlert::ShowNicoNamaNotice( NicoNamaProgram &program )
{
	switch( m_displaytype ){
	case NNA_BALLOON:
		ShowNicoNamaNoticeByBalloon( program );
		break;

	case NNA_WINDOW:
		ShowNicoNamaNoticeByWindow( program );
		break;

	default:
		break;
	}
	return 0;
}

/** �L���[�ɒ��܂��Ă��鎟�̒ʒm��\������.
 */
void NicoNamaAlert::ShowNextNoticeWindow()
{
	if( !m_program_queue.empty() ){
		NicoNamaProgram program = m_program_queue.front();
		m_program_queue.pop_front();
		ShowNicoNamaNotice( program );
	}
}

/** �ʒm���L���[�ɒ��߂�.
 * �ʒm�E�B���h�E���Ȃ��Ƃ��ɂ́A�L���[�ɕۑ������ɑ����ɕ\������B
 * @param nostack �L���[�ɐς܂Ȃ��ő��A�\�����b�Z�[�W�𑗐M����.
 */
void NicoNamaAlert::AddNoticeQueue( NicoNamaProgram &program, bool nostack )
{
	if( this->m_displaytype==this->NNA_BALLOON ){
		// �o���[���^�C�v�̂Ƃ��͑����ł�����.
		ShowNicoNamaNotice( program );
	}else{
		dprintf( L"notify window queue %d.\n", m_program_queue.size() );
		if( !nostack && FindWindow( T_NOTIFY_CLASS, NULL ) ){
			// ���łɒʒm�E�B���h�E������̂ŃL���[�ɒ��߂Ă�����.
			dprintf( L"Already notify window...queueing.\n" );
			if( program.isparticipant ){
				// �Q���R�~���̂��̂͐擪�ɓ˂�����.
				m_program_queue.push_front( program );
			}else if( m_program_queue.size() < 10 ){
				// �L���[�ɒ��߂���̂�10�܂łɐ���.
				m_program_queue.push_back( program );
			}
		}else{
			ShowNicoNamaNotice( program );
		}
	}

	m_recent_program.push_back( program );
	if( m_recent_program.size() > NICO_MAX_RECENT_PROGRAM ){
		m_recent_program.pop_front();
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
			getDataFrom2ndAuth( recvdata, recvlen );	// can obtain your community list.
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
int NicoNamaAlert::ConnectCommentServer()
{
	int r = m_socket.connect( m_commentserver.c_str(), m_port );
	// 3���Ő؂���̂�2����1�񂭂炢.
	SetTimer( this->m_parenthwnd, TIMER_ID_NICO_KEEPALIVE, 2*60*1000, NULL );
	return r;
}

// 5����1�񂭂炢�Ă�ł����Α��v����.
int NicoNamaAlert::KeepAlive()
{
	int s;
	std::string str;
	str = "<thread thread=\"" + m_thread + "\" res_from=\"-1\" version=\"20061206\"/>\0";
	s  = m_socket.send( str.c_str(), str.length()+1 ); // NUL�������݂�.
	dprintf( L"%d byte sent.\n", s );
	return s;
}

// �����R�~���̃��X�g������.
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


// �S����RSS������Ă���̂͂Ȃ��Ȃ����Ԃ�����.
int NicoNamaAlert::GetAllRSS()
{
	HANDLE thread;
	thread = (HANDLE)_beginthreadex( NULL, 0, thNicoNamaRetriveAllRSS, this, 0, NULL );
	WaitForSingleObject( thread, INFINITE );
	CloseHandle( thread );
	return 0;
}

/** �ڑ����̎Q���R�~���������ԑg�̒ʒm�p.
 * �ڑ�����1�񂾂��Ă΂��.
 */
int NicoNamaAlert::NotifyNowBroadcasting()
{
	if( m_rss_program.empty() ) return -1;

	bool first = true;
	int x,y,w,h;
	tNotifyWindow::getInitialPos(x,y,w,h);

	std::vector<std::wstring>::iterator it;
	for( it=m_communities.begin(); it!=m_communities.end(); it++ ){
		std::string mbstr;
		wstrtostr( (*it), mbstr );
		NicoNamaRSSProgram rssprog = m_rss_program[ mbstr ];
		if( rssprog.pubDate ){
			NicoNamaProgram prog;

			prog.community		= rssprog.community_id;
			prog.community_name = rssprog.community_name;
			prog.description	= rssprog.description;
			prog.playsound		= first;
			prog.request_id		= rssprog.guid;
			prog.start			= rssprog.pubDate;
			prog.thumbnail		= rssprog.thumbnail;
			prog.title			= rssprog.title;

			first = false;

			std::wstring wstr;
			strtowstr( prog.thumbnail, wstr );
			tHttp http;
			int httpret = http.request( wstr.c_str(), &prog.thumbnail_image, &prog.thumbnail_image_size );
			dprintf( L"httpret=%d\n", httpret );
			dprintf( L"thumbnail image size %d bytes.\n", prog.thumbnail_image_size );

			if( y<0 ){
				prog.playsound = true;
				AddNoticeQueue( prog, false );
			}else{
				prog.posx = x;
				prog.posy = y;
				AddNoticeQueue( prog, true );
				y -= h+5;
			}
		}
	}
	return 0;
}


HANDLE NicoNamaAlert::StartThread()
{
	Load();
	return (HANDLE)_beginthread( thNicoNamaAlertReceiver, 0, this );
}

// WM_TIMER�ɂ���Ď��s.
int NicoNamaAlert::GetAllRSSAndUpdate()
{
	_beginthread( thNicoNamaAlertGetRSSAndUpdate, 0, this );
	return 0;
}

int NicoNamaAlert::Receive( std::wstring &str )
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

/** �R�~���j�e�B�����擾����.
 * �L���b�V����RSS���E�F�u�̏��ŒT���B
 * @param commu_id �R�~���j�e�BID(coXXXX,chXXXX)
 * @return �R�~���j�e�B����Ԃ�.
 */
std::wstring NicoNamaAlert::getCommunityName( std::wstring& commu_id )
{
	std::wstring ret;
	std::string mb_commu_id;
	std::string mbbuf;
	wstrtostr( commu_id, mb_commu_id );

	// �L���b�V������T��.
	mbbuf = m_community_name_cache[ mb_commu_id ];
	if( mbbuf.length() ){
		strtowstr( mbbuf, ret );
		return ret;
	}

	// RSS����T��.
	std::map<std::string,NicoNamaRSSProgram> &rss = getRSSProgram();
	NicoNamaRSSProgram prog = rss[ mb_commu_id ];
	if( prog.community_name.length() ){
		m_community_name_cache[ mb_commu_id ] = prog.community_name;
		strtowstr( prog.community_name, ret );
		return ret;
	}

	// ������Ȃ��Ȃ璼�ڃR�~���̃y�[�W������.
	tHttp http;
	std::wstring url;
	url = commu_id.find( L"ch" )!=std::wstring::npos ? NICO_CHANNEL_URL : NICO_COMMUNITY_URL;
	url += commu_id;

	char*data;
	DWORD datalen;
	int httpret;
	httpret = http.request( url.c_str(), &data, &datalen );
	tXML html(data, datalen, true);
	char*title = html.FindFirstTextNodeFromRoot("title");
	if( title ){
		while( *title=='\x0a' || *title=='\x0d' ){
			// �s���ɂ�����s�R�[�h�𖳎�����.
			title++;
		}
		mbbuf = title;
		strtowstr( mbbuf, ret );
		size_t pos = ret.rfind( L"-�j�R�j�R" );	// �����̎ז�����菜��.
		if( pos!=std::wstring::npos ){
			ret = ret.substr(0,pos);
		}
		wstrtostr( ret, mbbuf );
		m_community_name_cache[ mb_commu_id ] = mbbuf;
	}
	free(data);
	return ret;
}


int NicoNamaAlert::Load()
{
	HKEY hkey;
	HKEY hsubkey;
	int i;
	// �A���[�g�^�C�v�̓ǂݍ���.
	RegCreateKeyEx( HKEY_CURRENT_USER, REG_COMMUNITY_SUBKEY, 0, L"", REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &hkey, NULL );

	DWORD len;
	RegQueryInfoKey( hkey, NULL, NULL, NULL, NULL, &len, NULL, NULL, NULL, NULL, NULL, NULL );
	dprintf(L"subkey length is %d\n", len);
	for(i=0; ;i++){
		std::string co;
		std::string coname;
		WCHAR *subkey = new WCHAR[len+1];
		DWORD l = len+1;
		if( RegEnumKeyEx( hkey, i, subkey, &l, NULL, NULL, NULL, NULL )==ERROR_NO_MORE_ITEMS ) break;

		std::wstring wstr = REG_COMMUNITY_SUBKEY L"\\";
		wstr += subkey;	// create HKCU\Software\Miku39.jp\HatsuneMiclock\Communities\coXXXX
		RegCreateKeyEx( HKEY_CURRENT_USER, wstr.c_str(), 0, L"", REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &hsubkey, NULL );

		NicoNamaNoticeType alert;
		if( ReadRegistoryDW( hsubkey, L"Type", &alert.type )!=ERROR_SUCCESS ) goto regclose;
		if( ReadRegistorySTR( hsubkey, L"Command", alert.command )!=ERROR_SUCCESS ) goto regclose;
		if( ReadRegistorySTR( hsubkey, L"Name", wstr )!=ERROR_SUCCESS ) goto regclose;

		wstrtostr( wstr, coname );
		wstrtostr( std::wstring(subkey), co );

		m_community_name_cache[ co ] = coname;
		m_noticetype[ co ] = alert;
		dprintf( L"%s loaded.\n", subkey );

	  regclose:
		RegCloseKey( hsubkey );
		delete [] subkey;
	}

	RegCloseKey( hkey );
	return 0;
}


int NicoNamaAlert::Save()
{
	HKEY hkey;
	std::wstring key;
	std::wstring value;
	std::wstring data;


	// �A���[�g�^�C�v�̕ۑ�.
	// HKCU\Software\Miku39.jp\HatsuneMiclock\Communities\coXXXX
	std::map<std::string,NicoNamaNoticeType>::iterator it;
	for(it=m_noticetype.begin(); it!=m_noticetype.end(); it++){
		NicoNamaNoticeType& alert = (*it).second;
		strtowstr( (*it).first, value );

		key = REG_COMMUNITY_SUBKEY L"\\" + value;
		RegCreateKeyEx( HKEY_CURRENT_USER, key.c_str(), 0, L"", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkey, NULL );

		std::wstring cname = getCommunityName( value );
		RegSetValueEx( hkey, L"Name", 0, REG_SZ, (BYTE*)cname.c_str(), sizeof(WCHAR)*(cname.length()+1) );
		RegSetValueEx( hkey, L"Type", 0, REG_DWORD, (BYTE*)&alert.type, sizeof(alert.type) );
		RegSetValueEx( hkey, L"Command", 0, REG_SZ, (BYTE*)alert.command.c_str(), sizeof(WCHAR)*(alert.command.length()+1) );

		RegCloseKey( hkey );
	}

	return 0;
}

bool NicoNamaAlert::isAlreadyAnnounced( std::string& request_id )
{
	std::list<std::string>::iterator it;
	for(it=m_announcedlist.begin(); it!=m_announcedlist.end(); it++){
		if( (*it)==request_id ) return true;
	}
	return false;
}

/** �L�[���[�h�Ƀ}�b�`�������̂�ʒm����.
 */
int NicoNamaAlert::NotifyKeywordMatch()
{
	std::map<std::string,NicoNamaRSSProgram>::iterator it;
	std::string keyword = getMatchKeyword();
	myPCRE re( keyword.c_str(), PCRE_UTF8|PCRE_MULTILINE );

	if( keyword.length() && !re.hasError() ){
		for(it=m_rss_program.begin(); it!=m_rss_program.end(); it++){
			NicoNamaRSSProgram& p = (*it).second;

			if( re.match( p.description.c_str() ) || re.match( p.title.c_str() ) ){
				NicoNamaProgram prog;
				std::wstring wstr_commuid;
				strtowstr( p.community_id, wstr_commuid );

				if( isParticipant( wstr_commuid ) ) continue;	// �Q���R�~���͒ʒm�ΏۊO.
				if( isAlreadyAnnounced( p.guid ) ) continue;	// ���łɒʒm�������̂��ΏۊO.
				m_announcedlist.push_back( p.guid );

				prog.community		= p.community_id;
                prog.community_name = p.community_name;
                prog.description	= p.description;
                prog.request_id		= p.guid;
                prog.start			= p.pubDate;
                prog.thumbnail		= p.thumbnail;
                prog.title			= p.title;
                prog.playsound		= false;
				prog.isparticipant  = false;

                std::wstring wstr;
                strtowstr( prog.thumbnail, wstr );
                tHttp http;
                int httpret = http.request( wstr.c_str(), &prog.thumbnail_image, &prog.thumbnail_image_size );
                dprintf( L"httpret=%d\n", httpret );
                dprintf( L"thumbnail image size %d bytes.\n", prog.thumbnail_image_size );
				AddNoticeQueue( prog );
			}
		}
	}

	// RSS�ɂȂ��ʒm�ςݔԑg���폜.
	for( std::list<std::string>::iterator i=m_announcedlist.begin(); i!=m_announcedlist.end(); i++ ){
		bool exist = false;
		for(it=m_rss_program.begin(); it!=m_rss_program.end(); it++){
			if( (*i)==(*it).second.guid ) exist = true;
		}
		if( !exist ){
			(*i) = "";	// �폜������̃}�[�L���O.
		}
	}
	m_announcedlist.remove( "" );
	return 0;
}

NicoNamaAlert::NicoNamaAlert( HINSTANCE hinst, HWND hwnd )
{
	m_matchkeyword = "";
	m_announcedlist.clear();
	m_hinst = hinst;
	m_parenthwnd = hwnd;
	SetRandomPickup( false );
	m_displaytype = NNA_WINDOW;
}

NicoNamaAlert::~NicoNamaAlert()
{
	m_rss_program.clear();
}
