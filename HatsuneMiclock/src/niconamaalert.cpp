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
#include "tPlaySound.h"
#include "tNotifyWindow.h"
#include "udmessages.h"
#include "niconamaalert.h"

#include "myregexp.h"

#include "stringlib.h"
#include "debug.h"

#include "../resource.h"

#pragma comment(lib,"libxml2_a.lib")

#pragma warning( disable : 4996 ) 


//----------------------------------------------------------------------
// prototype
//----------------------------------------------------------------------
int GetBroadcastingInfo( const std::string& str, NicoNamaProgram &program );
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

	RssReadRequest(){ hThread = NULL; data = NULL; datalen = 0; index = 0; }
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
            NicoNamaRSSProgram *prog = new NicoNamaRSSProgram;
            GetBroadcastingInfoFromRSS( node, *prog );
            AddRSSProgram( rss, *prog );
			delete prog;
            //dprintf( L"community_id=%S\n", prog.community_id.c_str() );
        }
    }
    return total_num;
}



/** RSS���擾���čX�V�ʒm�𓊂���܂ł����X���b�h.
 * _beginthread()�ɂ���ċN������āARSS��ǂ�Őe�E�B���h�E�ɑ΂���DL�����ʒm�𓊂���.
 * WM_TIMER�ɂ���Đe�E�B���h�E����Ă΂�Ă���.
 */
static void __cdecl thNicoNamaAlertGetRSSAndUpdate(LPVOID v)
{
	NicoNamaAlert *nico = (NicoNamaAlert*)v;
	dprintf( L"Retrieving RSS...\n" );
	nico->GetAllRSS();
	dprintf( L"done. and update.\n" );
	// �ԑg���X�g���X�V������.
	SendMessage( nico->getParentHWND(), WM_NNA_NOTIFY, NNA_UPDATE_RSS, 0 );

	nico->NotifyKeywordMatch();

	_endthread();
}

// �ʒm�^�C�v�������Ă���R�~�����ǂ���.
bool NicoNamaAlert::hasNoticeType( const std::wstring& communityid )
{
	std::string str;
	wstrtostr( communityid, str );
	return hasNoticeType( str );
}
bool NicoNamaAlert::hasNoticeType( const std::string& communityid )
{
	return (bool)m_noticetype.count( communityid );
}


// <chat></chat>����������.
void NicoNamaAlert::ProcessNotify( std::string& str )
{
	// �����J�n�����R�~���𒲂ׂ܂���.
	// [����ID],[�`�����l�����R�~���j�e�BID],[������̃��[�U�[ID]
	tXML xml( str.c_str(), str.length() );

	// <chat>�R�R</chat>�����o���� , �ŕ���.
	xmlNodePtr root = xml.getRootNode();
	if( !root || !root->children || !root->children->content ){
		return;
	}

	std::vector<std::string> data;
	std::string text = (char*)root->children->content;
	data = split( text, std::string(",") );

	const std::string& cid        = data[1];
	const std::string& request_id = data[0];

	int p = (int)((float)rand() / RAND_MAX * 100);
	if( !isRandomPickup() ) p = 10000;

	// data[1] is Community_ID.
	bool isPart    = isParticipant( cid );	// �Q�����Ă�R�~���H.
	bool hasNotice = hasNoticeType( cid );	// �ʒm���X�g�ɂ���H.

	// �����_���s�b�N�A�b�v�� 5% �Œ��.
	if( p<5 || isPart || hasNotice ){
		char*d = xml.FindAttribute( xml.getRootNode(), "date" );
		std::string datestr = d?d:"0";
#ifdef _USE_32BIT_TIME_T
		time_t starttime = atoi( datestr.c_str() );
#else
		time_t starttime = _atoi64( datestr.c_str() );
#endif
		dprintf( L"Community(%S)'s broadcasting started.\n", cid.c_str() );

		NicoNamaProgram program;
		GetBroadcastingInfo( request_id, program );
		program.start		  = starttime;
		program.isparticipant = isPart;
		program.playsound	  = isPart || hasNotice && (getNoticeType( cid ).type & NICO_NOTICETYPE_SOUND);
		AddNoticeQueue( program );
	}
	return;
}

void NicoNamaAlert::DoReceive()
{
	std::string str;
	while(1){
		// ���Ƃ͂����Ǝ�M�ҋ@.
		int r;
		r = Receive( str );
		if( r<=0 ) break;
		if( str.find("<chat",0)==std::string::npos ) continue;
		//dprintf( L"NNA:%s\n", str.c_str() );

		m_xml_cs.enter();
		ProcessNotify( str );
		m_xml_cs.leave();
	}
}

/** �j�R���A���[�g��M�X���b�h.
 *
 * @note CRT�g���̂� _beginthreadex() �ŊJ�n. �Ăяo���K����stdcall
 */
static unsigned __stdcall thNicoNamaAlertReceiver(LPVOID v)
{
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

	nico->DoReceive();

	dprintf( L"NicoNamaAlert thread ended.\n" );
	_endthreadex(0);
	return 0;
}

/** RSS������Ă��邾��.
 *_beginthreadex()���g��. __stdcall��.
 */
static unsigned __stdcall thRetrieveRSS(LPVOID v)
{
	RssReadRequest *req = (RssReadRequest*)v;
	tHttp *http = new tHttp;
	http->request( req->rssurl.c_str(), &req->data, &req->datalen );
	delete http;
	_endthreadex(0);
	return 0;
}

class nico_test {
public:
	nico_test(){ dprintf(L"*** construct nico_test\n"); }
	~nico_test(){ dprintf(L"*** destruct nico_test\n"); }
};


/** RSS��S���擾.
 * �E�F�C�g�Ȃ��ň�C�Ɏ擾���Ă��邯�ǖ��Ȃ�����.
 * �j�R���A���[�g(��a)���m�[�E�F�C�g�łǂ�ǂ����Ă邩�炢�����낤.
 *
 * �֐��̃G�s���[�O�Ńf�X�g���N�^�Ă�ł��炤���߂̃��b�p�[�֐��B
 */
void NicoNamaAlert::wrapper_retrieve_rss_for_destructor()
{
	nico_test nnnnn;
	std::map<std::string,NicoNamaRSSProgram> rss_list;
	RssReadRequest reqlist[ NICONAMA_MAX_RSS ];
	tXML *xml = NULL;

    // �ŏ���1�y�[�W�ڂ𓯎��ɃA�N�Z�X���邭�炢�͂������.
	for(int i=0; i<NICONAMA_MAX_RSS; i++ ){
		wchar_t *rss = g_rsslist[i];
		reqlist[i].index	= i;
		reqlist[i].rssurl	= rss;
		reqlist[i].rssurl	+= L"1";
		reqlist[i].hThread	= (HANDLE)_beginthreadex( NULL, 0, thRetrieveRSS, &reqlist[i], 0, NULL );
		if( reqlist[i].hThread==(HANDLE)0L ){
			int err = errno;
		}
	}

	for(int i=0; i<NICONAMA_MAX_RSS; i++ ){
		if( reqlist[i].hThread==0 ) continue;
		// 6�J�e�S�����Ƃɏ�������.
		WaitForSingleObject( reqlist[i].hThread, INFINITE );

		int total_num = 0;
		int index = reqlist[i].index;

		xml = new tXML( reqlist[i].data, reqlist[i].datalen );
		xmlNodePtr channel;
		channel = xml->FindFirstNodeFromRoot("channel");
		total_num = ::AddRSSProgram( rss_list, channel );	// 1�y�[�W��.
		delete xml;

		CloseHandle( reqlist[i].hThread );
		free( reqlist[i].data );

		int maxpages = (total_num-1)/18+1;	// 1�y�[�W18������̂�.
		// RSS��2�y�[�W�ڂ���S�����.
		for( int cnt=2; cnt<=maxpages; ){
			// (HTTP/1.1�Ȃ̂�)2�y�[�W�����ɗv��.
#define SIMULTANEOUS_HTTP		(2)
			RssReadRequest subreqlist[ SIMULTANEOUS_HTTP ];

			for(int j=0; j<SIMULTANEOUS_HTTP && cnt<=maxpages; cnt++,j++){
				wchar_t tmp[128];
				subreqlist[j].rssurl = g_rsslist[ index ];
				subreqlist[j].rssurl += _itow( cnt, tmp, 10 );
				subreqlist[j].hThread = (HANDLE)_beginthreadex( NULL, 0, thRetrieveRSS, &subreqlist[j], 0, NULL );
				if( subreqlist[j].hThread==(HANDLE)0L ){
					// error occurred
					int err = errno;
				}
			}

			// �v��������������M��҂��ď���.
			for( int k=0; k<SIMULTANEOUS_HTTP; k++ ){
				if( subreqlist[k].hThread==0 ) continue;
				WaitForSingleObject( subreqlist[k].hThread, INFINITE );

				xml = new tXML( subreqlist[k].data, subreqlist[k].datalen );
				xmlNodePtr channel;
				channel = xml->FindFirstNodeFromRoot("channel");
				::AddRSSProgram( rss_list, channel );
				delete xml;

				CloseHandle( subreqlist[k].hThread );
				free( subreqlist[k].data );
			}
		}
	}
	setRSSProgram( rss_list );
	m_xml_cs.enter();
	tXML::cleanup();
	m_xml_cs.leave();
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
int GetBroadcastingInfo( const std::string& str, NicoNamaProgram &program )
{
	tHttp http;
	std::wstring url, broadcastingid;
	char*data;
	DWORD datalen;
    int httpret;
	strtowstr( str, broadcastingid );

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

	httpret = http.request( program.thumbnail, &program.thumbnail_image, &program.thumbnail_image_size );
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


/** 1000�~���b�̃E�F�C�g�������Ă���URL���J��.
 * �����J�n�ʒm�����đ�URL���J���ƁA
 * �����������āA���܂ɍ폜���ꂽ�����݂��Ȃ��ԑg�ƌ�����̂ŁB
 */
static void __cdecl thDelayedOpenURL(LPVOID v)
{
	WCHAR*url = (WCHAR*)v;
	Sleep(1000);
	OpenURL( url );
	free( url );
	_endthread();
}


/** ����Window�ɂ���Ēʒm����.
 * ���̃��\�b�h�̓R�����g�T�[�o����̎�M�X���b�h����Ă΂�邽�߁A
 * �����Œʒm�E�B���h�E����炸���C���X���b�h�ɍ�点��B
 * ���̂��߂̃��b�Z�[�W���M�B
 */
int NicoNamaAlert::ShowNicoNamaNoticeByWindow( NicoNamaProgram &program )
{
	SendMessage( m_parenthwnd, WM_NNA_NOTIFY, NNA_REQUEST_CREATEWINDOW, (LPARAM)&program );

	if( m_noticetype.count( program.community ) ){
		NicoNamaNoticeType& alert = m_noticetype[ program.community ];
		if( alert.type & NICO_NOTICETYPE_BROWSER ){
			std::wstring url;
			std::wstring tmp;
			strtowstr( program.request_id, tmp );
			url = NICO_LIVE_URL + tmp;
			WCHAR *p = (WCHAR*)calloc( sizeof(WCHAR)*(url.length()+1), 1 );
			wcscpy( p, url.c_str() );
			_beginthread( thDelayedOpenURL, 0, p );
			//OpenURL( url );
		}
		if( program.playsound && (alert.type & NICO_NOTICETYPE_SOUND) ){
			tPlaySound( m_defaultsound.c_str() );
		}
		if( alert.type & NICO_NOTICETYPE_CLIPBOARD ){
			HGLOBAL hGlobal = GlobalAlloc( GHND|GMEM_SHARE, program.request_id.length()+1 );
			char*p = (char*)GlobalLock( hGlobal );
			strcpy( p, program.request_id.c_str() );
			GlobalUnlock( hGlobal );
			OpenClipboard( NULL );
			EmptyClipboard();
			SetClipboardData( CF_TEXT, hGlobal );
			CloseClipboard();
		}
		if( alert.type & NICO_NOTICETYPE_EXEC ){
			std::wstring wstr;
			std::wstring cmd;
			strtowstr( program.request_id, wstr );
			ShellExecute( getParentHWND(), L"open", alert.command.c_str(), wstr.c_str(), NULL, SW_SHOWNORMAL );
		}
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
 * �ʒm�����ɂ��L�^����.
 * @param nostack �L���[�ɐς܂Ȃ��ő��A�\�����b�Z�[�W�𑗐M����.
 */
void NicoNamaAlert::AddNoticeQueue( NicoNamaProgram &program, bool nostack )
{
	bool b = false;
	if( this->m_displaytype==this->NNA_BALLOON ){
		// �o���[���^�C�v�̂Ƃ��͑����ł�����.
		ShowNicoNamaNotice( program );
		b = true;

	}else{
		if( !nostack && FindWindow( T_NOTIFY_CLASS, NULL ) ){
			// ���łɒʒm�E�B���h�E������̂ŃL���[�ɒ��߂Ă�����.
			dprintf( L"Already notify window...queueing.\n" );
			if( program.isparticipant ){
				// �Q���R�~���̂��̂͐擪�ɓ˂�����.
				m_program_queue.push_front( program );
				b = true;
			}else if( m_program_queue.size() < 10 ){
				// �L���[�ɒ��߂���̂�10�܂łɐ���.
				m_program_queue.push_back( program );
				b = true;
			}
		}else{
			ShowNicoNamaNotice( program );
			b = true;
		}
	}

	if( b ){
		if( program.isparticipant ){
			m_recent_commu_prog.push_back( program );
		}
		m_recent_program.push_back( program );
	}
	if( m_recent_program.size() > NICO_MAX_RECENT_PROGRAM ){
		m_recent_program.pop_front();
	}
	if( m_recent_commu_prog.size() > NICO_MAX_RECENT_PROGRAM ){
		m_recent_commu_prog.pop_front();
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
	static int cnt=0;

	wrapper_retrieve_rss_for_destructor();

	cnt++;
	dprintf(L"* %d times to read RSS.\n",cnt);
	return 0;
}

#if 0	// old
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

			//m_now_broadcasting[ prog.community ] = prog;

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
#endif

/** �ڑ����̎Q���R�~���������ԑg�̒ʒm�p.
 * �ڑ�����1�񂾂��Ă΂��.
 */
int NicoNamaAlert::NotifyNowBroadcasting()
{
	if( m_rss_program.empty() ) return -1;

	bool first = true;
	int x,y,w,h;
	tNotifyWindow::getInitialPos(x,y,w,h);

	for( std::map<std::string,NicoNamaRSSProgram>::iterator it=m_rss_program.begin(); it!=m_rss_program.end(); it++ ){
		std::string cid = (*it).first;
		bool isPart    = isParticipant( cid );
		if( isPart || hasNoticeType( cid ) ){
			NicoNamaRSSProgram& rssprog = m_rss_program[ cid ];
			NicoNamaProgram prog;

			prog.community		= rssprog.community_id;
			prog.community_name = rssprog.community_name;
			prog.description	= rssprog.description;
			prog.playsound		= first;
			prog.request_id		= rssprog.guid;
			prog.start			= rssprog.pubDate;
			prog.thumbnail		= rssprog.thumbnail;
			prog.title			= rssprog.title;
			prog.isparticipant  = isPart;

			first = false;

			tHttp http;
			int httpret = http.request( prog.thumbnail, &prog.thumbnail_image, &prog.thumbnail_image_size );
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
	m_thReceiver = (HANDLE)_beginthreadex( NULL, 0, thNicoNamaAlertReceiver, this, 0, NULL );
	if( m_thReceiver==0 ){
		dprintf(L"StartThread failed\n");
	}
	return m_thReceiver;
}

// WM_TIMER�ɂ���Ď��s.
int NicoNamaAlert::GetAllRSSAndUpdate()
{
	dprintf( L"RSS Update...\n" );
	_beginthread( thNicoNamaAlertGetRSSAndUpdate, 0, this );
	return 0;
}

// �R�����g�T�[�o����e�L�X�g����M����.
int NicoNamaAlert::Receive( std::string &str )
{
	int r = 0;
	str = "";
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
bool NicoNamaAlert::isParticipant( const std::wstring&communityid )
{
	std::vector<std::wstring>::iterator it;
	for( it=m_communities.begin(); it!=m_communities.end(); it++ ){
		if( (*it).compare( communityid )==0 ){
			return true;
		}
	}
	return false;
}
bool NicoNamaAlert::isParticipant( const std::string& str )
{
	std::wstring communityid;
	strtowstr( str, communityid );
	return isParticipant( communityid );
}
bool NicoNamaAlert::isParticipant( const WCHAR*communityid )
{
	std::wstring wstr = communityid;
	return isParticipant( wstr );
}

/** �R�~���j�e�B�����擾����.
 * �L���b�V����RSS���E�F�u�̏��ŒT���B
 * @param commu_id �R�~���j�e�BID(coXXXX,chXXXX)
 * @return �R�~���j�e�B����Ԃ�.
 */
std::wstring NicoNamaAlert::getCommunityName( const std::wstring& commu_id )
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
	m_rss_cs.enter();
	tXML *html = new tXML(data, datalen, true);
	char*title = html->FindFirstTextNodeFromRoot("title");
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
	delete html;
	m_rss_cs.leave();
	free(data);
	return ret;
}
std::wstring NicoNamaAlert::getCommunityName( const std::string& commu_id )
{
	std::wstring wstr;
	strtowstr( commu_id, wstr );
	return getCommunityName( wstr );
}

// �ʒm�^�C�v�����W�X�g�����烍�[�h.
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
		if( RegEnumKeyEx( hkey, i, subkey, &l, NULL, NULL, NULL, NULL )==ERROR_NO_MORE_ITEMS ){
			delete [] subkey;
			break;
		}

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

// �ʒm�^�C�v�����W�X�g���ɃZ�[�u.
int NicoNamaAlert::Save()
{
	HKEY hkey;
	std::wstring key;
	std::wstring value;
	std::wstring data;

	DeleteRegistorySubKey( REG_SUBKEY, L"Communities" );

	// �A���[�g�^�C�v�̕ۑ�.
	// HKCU\Software\Miku39.jp\HatsuneMiclock\Communities\coXXXX
	std::map<std::string,NicoNamaNoticeType>::iterator it;
	for(it=m_noticetype.begin(); it!=m_noticetype.end(); it++){
		NicoNamaNoticeType& alert = (*it).second;
		if( alert.type==0 ) continue;

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

// ���łɒʒm�����H(�L�[���[�h�}�b�`�p)
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
				if( hasNoticeType( wstr_commuid ) ) continue;	// �ʒm���X�g�o�^�ς݂̂��̂��ΏۊO.
				if( isAlreadyAnnounced( p.guid ) ) continue;	// ���łɒʒm�������̂��ΏۊO.

				m_announcedlist.push_back( p.guid );

				prog.community		= p.community_id;
                prog.community_name = p.community_name;
                prog.description	= p.description;
                prog.request_id		= p.guid;
                prog.start			= p.pubDate;
                prog.thumbnail		= p.thumbnail;
                prog.title			= p.title;
                prog.playsound		= false;	// �L�[���[�h�}�b�`�ł͉��͖炳�Ȃ�.
				prog.isparticipant  = false;

                tHttp http;
                int httpret = http.request( prog.thumbnail, &prog.thumbnail_image, &prog.thumbnail_image_size );
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
			(*i) = "X";	// �폜������̃}�[�L���O.
		}
	}
	m_announcedlist.remove( "X" );
	return 0;
}

void NicoNamaAlert::GoCommunity( const std::string& cid )
{
	std::wstring wstr;
	strtowstr( cid, wstr );
	return GoCommunity( wstr );
}
void NicoNamaAlert::GoCommunity( const std::wstring& cid )
{
	std::wstring url;
	if( cid.find(L"ch")!=std::wstring::npos ){
		// channel
		url = NICO_CHANNEL_URL + cid;
	}else{
		// community
		url = NICO_COMMUNITY_URL + cid;
	}
	OpenURL( url );
}

NicoNamaAlert::NicoNamaAlert( HINSTANCE hinst, HWND hwnd )
{
	m_thReceiver = 0;
	m_matchkeyword = "";
	m_announcedlist.clear();
	m_hinst = hinst;
	m_parenthwnd = hwnd;
	SetRandomPickup( false );
	m_displaytype = NNA_WINDOW;
}

NicoNamaAlert::~NicoNamaAlert()
{
	m_socket.close();
	if( m_thReceiver ){
		WaitForSingleObject( m_thReceiver, INFINITE );
		CloseHandle( m_thReceiver );
	}
}
