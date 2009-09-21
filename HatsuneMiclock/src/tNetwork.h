#ifndef TNETWORK_H_DEF
#define TNETWORK_H_DEF

#include "winheader.h"
#include <winhttp.h>
#include <string>

// function prototype.
int WinsockInit();
int WinsockUninit();
std::string urlencode(const std::string &str);
std::string urlencode(const char*str);


class tHttp {
	HINTERNET		m_hSession;
	HINTERNET		m_hConnect;
	HINTERNET		m_hRequest;
	URL_COMPONENTS	m_urlcomp;

	WCHAR			*m_host;
	WCHAR			*m_path;

	bool allocbuffer( int bufsize );
	void cleanup();

public:
	tHttp();
	~tHttp();

	// 返却したポインタを呼び出しもとで解放していればメモリリークなし.
	int request( const WCHAR*url, char**data, DWORD*datalen, const char*postdata=NULL, DWORD postlen=0 );
};


class tSocket {
	SOCKET		m_socket;

public:
	SOCKET open();
	int connect( const char*hostname, uint16_t port );
	int connect( const sockaddr*name, int namelen );
	int send( const char*buf, int len );
	int recv( char*buf, int len );
	int close();

	tSocket();
	~tSocket();
};



#endif
