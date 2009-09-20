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
#define NICO_NOTICETYPE_WINDOW		0x00000001		// open window(�펞�L��)
#define NICO_NOTICETYPE_SOUND		0x00000002		// play sound(���g�p)
#define NICO_NOTICETYPE_BROWSER		0x00000004		// run browser
#define NICO_NOTICETYPE_EXEC		0x00000008		// run command


//----------------------------------------------------------------------
// prototype
//----------------------------------------------------------------------
class tNotifyWindow;


//----------------------------------------------------------------------
// define
//----------------------------------------------------------------------
// ����͒ʒm�E�B���h�E�ɓn�����.
struct NicoNamaProgram {
	// utf-8
	std::string		request_id;	// lvXXXXXXX
	std::string		title;
	std::string		description;

	std::string		community;	// coXXXX, chXXXX
	std::string		community_name;
	std::string		thumbnail;	// �T���lURL

	char*			thumbnail_image;
	DWORD			thumbnail_image_size;

	time_t			start;		// �ԑg�̊J�n����.
	bool			playsound;	// �T�E���h�ʒm���s����.
	bool			isparticipant;	// �Q���R�~���̂��̂��ǂ���.
	int				posx;		// �\���ʒuX.
	int				posy;		// �\���ʒuY.

	NicoNamaProgram(){ thumbnail_image = NULL; playsound=false; posx=posy=-1; }
	~NicoNamaProgram(){ SAFE_FREE( thumbnail_image ); }

	NicoNamaProgram( const NicoNamaProgram &src ){
		// thumbnail_image�̂��߂̃R�s�[�R���X�g���N�^.
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
		// �摜��V�K�������ɃR�s�[
		this->thumbnail_image	= (char*)malloc( this->thumbnail_image_size );
		CopyMemory( this->thumbnail_image, src.thumbnail_image, thumbnail_image_size );
	}
	NicoNamaProgram& operator=( const NicoNamaProgram &src ){
		// ������Z�̂Ƃ�.
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
		// �摜��V�K�������ɃR�s�[
		this->thumbnail_image = (char*)malloc( this->thumbnail_image_size );
		CopyMemory( this->thumbnail_image, src.thumbnail_image, thumbnail_image_size );
		return *this;
	}
};

// �����RSS�Ŏ擾�ł�����.
struct NicoNamaRSSProgram {
	std::string		title;			// �����^�C�g��.
	std::string		link;			// http://live.nicovideo.jp/watch/lvXXXXX
	std::string		guid;			// lvXXXX
	std::string		description;	// ����
	std::string		category;		// ����Љ�E���N�G�X�g�Ƃ����(���̑�)�Ƃ�.
	std::string		thumbnail;		// �T���l��URL
	std::string		community_name;	// �R�~����.
	std::string		community_id;	// coXXXX
	std::string		owner_name;		// ����̖��O.
	int				num_res;		// �R����.
	int				view;			// ����Ґ�.
	bool			member_only;	// �����o�[�I�����[�t���O.
	int				type;			// 0:unknown 1:community 2:official 3:���Ɖ�������񂾂낤.
	time_t			pubDate;

	NicoNamaRSSProgram(){ pubDate = 0; }
};


// �ʒm���@.
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
		NNA_BALLOON,	// Shell_NotifyIcon�ɂ��o���[���w���v.
		NNA_WINDOW,		// CreateWindow�ō��E�B���h�E.
		NNA_MAX,
	};

private:
	HINSTANCE		m_hinst;
	HWND			m_parenthwnd;
	int				m_displaytype;		///< �A���[�g�̕\�����@.

	std::string		m_ticket;			///< ���F�؂Ŏ擾����`�P�b�g.
	std::string		m_userid;
	std::string		m_userhash;
	std::vector<std::wstring>	m_communities;	///< �Q���R�~����ID���X�g.

	std::string		m_commentserver;	///< �R�����g�T�[�o.
	uint16_t		m_port;				///< �R�����g�T�[�o�̃|�[�g.
	std::string		m_thread;			///< �R�����g�T�[�o�̃X���b�h.
	tSocket			m_socket;			///< �R�����g�T�[�o�ڑ��\�P�b�g.

	std::list<NicoNamaProgram>		m_program_queue;	///< �ʒm�L���[.
	std::list<NicoNamaProgram>		m_recent_program;	///< �ŋߒʒm�����ԑg.
	bool	m_randompickup;

	// �L�[�̓R�~���j�e�BID
	tCriticalSection m_rss_cs;
	std::map<std::string,NicoNamaRSSProgram>	m_rss_program;	///< RSS�ɂ���S�ԑg.

	std::map<std::string,NicoNamaNoticeType>	m_noticetype;	///< �R�~�����Ƃ̒ʒm�^�C�v�w��.
	std::map<std::string,std::string>			m_community_name_cache;	///< �R�~���j�e�B���L���b�V��.

	std::string					m_matchkeyword;		///< �L�[���[�h�ʒm�p.
	std::list<std::string>		m_announcedlist;	///< ��d�ʒm���Ȃ����߂̒ʒm�ς݈ꗗ.

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
