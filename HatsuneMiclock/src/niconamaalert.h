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

#define NICO_MAX_RECENT_PROGRAM		(5)		// �A���[�g�����̍ő吔.

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

	time_t			start;  // �ԑg�̊J�n����.

	NicoNamaProgram(){ thumbnail_image = NULL; }
	~NicoNamaProgram(){ if( thumbnail_image ) free( thumbnail_image ); }

	NicoNamaProgram( const NicoNamaProgram &src ){
		// thumbnail_image�̂��߂̃R�s�[�R���X�g���N�^.
		this->request_id		= src.request_id;
		this->title				= src.title;
		this->description		= src.description;
		this->community			= src.community;
		this->community_name	= src.community_name;
		this->thumbnail			= src.thumbnail;
		this->thumbnail_image_size = src.thumbnail_image_size;
		this->start				= src.start;
		// �摜��V�K�������ɃR�s�[
		this->thumbnail_image	= (char*)malloc( this->thumbnail_image_size );
		CopyMemory( this->thumbnail_image, src.thumbnail_image, thumbnail_image_size );
	}
	NicoNamaProgram& operator=( const NicoNamaProgram &src ){
		// ������Z�̂Ƃ�.
		SAFE_DELETE( this->thumbnail_image );
		this->request_id		= src.request_id;
		this->title				= src.title;
		this->description		= src.description;
		this->community			= src.community;
		this->community_name	= src.community_name;
		this->thumbnail			= src.thumbnail;
		this->thumbnail_image_size = src.thumbnail_image_size;
		this->start				= src.start;
		// �摜��V�K�������ɃR�s�[
		this->thumbnail_image = (char*)malloc( this->thumbnail_image_size );
		CopyMemory( this->thumbnail_image, src.thumbnail_image, thumbnail_image_size );
		return *this;
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
	int				m_displaytype;		// �A���[�g�̕\�����@.

	std::string		m_ticket;			// ���F�؂Ŏ擾����`�P�b�g.
	std::string		m_userid;
	std::string		m_userhash;
	std::vector< std::wstring >	m_communities;	// �Q���R�~����ID.


	std::string		m_commentserver;	// �R�����g�T�[�o.
	uint16_t		m_port;				// �R�����g�T�[�o�̃|�[�g.
	std::string		m_thread;			// �R�����g�T�[�o�̃X���b�h.
	tSocket			m_socket;			// �R�����g�T�[�o�ڑ��\�P�b�g.

	std::queue<NicoNamaProgram>		m_program_queue;	// �ԑg�̒ʒm�L���[.
	std::list<NicoNamaProgram>		m_recent_program;	// �ŋߒʒm�����ԑg.
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
