#include "winheader.h"
#include "tNetwork.h"
#include "stringlib.h"

#include "debug.h"

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"winhttp.lib")

#define HTTP_USER_AGENT		L"HatsuneMiclock/0.5"

// httpで取得したファイルをD:\test-XXXX.txtに保存する.
//#define HTTP_STORE_FILE


int WinsockInit()
{
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
 
    wVersionRequested = MAKEWORD( 2, 2 );
 
    err = WSAStartup( wVersionRequested, &wsaData );
    if ( err != 0 ) {
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.                                  */
        return 1;
    }
 
    /* Confirm that the WinSock DLL supports 2.2.*/
    /* Note that if the DLL supports versions greater    */
    /* than 2.2 in addition to 2.2, it will still return */
    /* 2.2 in wVersion since that is the version we      */
    /* requested.                                        */
 
    if ( LOBYTE( wsaData.wVersion ) != 2 ||
         HIBYTE( wsaData.wVersion ) != 2 ) {
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.                                  */
        WSACleanup( );
        return 1; 
    }
    /* The WinSock DLL is acceptable. Proceed. */
    return 0;
}

int WinsockUninit()
{
	return WSACleanup();
}


tHttp::tHttp()
{
	m_hConnect = NULL;
	m_hRequest = NULL;
	m_hSession = NULL;

	m_host = NULL;
	m_path = NULL;
}

tHttp::~tHttp()
{
	cleanup();
	SAFE_FREE( m_host );
	SAFE_FREE( m_path );
}

// bufsizeの文字数(wchar_t)分のメモリを確保.
bool tHttp::allocbuffer( int bufsize )
{
	SAFE_FREE( m_host );
	SAFE_FREE( m_path );

	m_host = (WCHAR*)calloc( sizeof(WCHAR) * (bufsize+1), 1 );
	m_path = (WCHAR*)calloc( sizeof(WCHAR) * (bufsize+1), 1 );
	if( !m_host || !m_path ) return false;
	return true;
}

/** HTTP GET/POSTする.
 * @param url URL
 * @param data 取得するデータポインタを受けとる先.
 * @param datalen データサイズを受けとる先.
 * @param postdata form-urlencodedのデータ(default=NULL).
 * @param postlen postdataの長さ(default=0).
 * @return 失敗時は負を、成功時はHTTPステータスコードを返す(-999のときは未対応プラットフォーム).
 * @note dataは呼び出し側でfree()すること. ブロック型.
 */
int tHttp::request( const WCHAR*url, char**data, DWORD*datalen, const char*postdata, DWORD postlen )
{
	int r = 0;
	BOOL b;
	char *buffer = NULL;
	DWORD buffersize;

	*data = NULL;
	*datalen = 0;

	if( !WinHttpCheckPlatform() ){
		dprintf( L"unsupported platform.\n" );
		return -999;
	}

	// session
	m_hSession = WinHttpOpen( HTTP_USER_AGENT, 
                              WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, 
                              WINHTTP_NO_PROXY_BYPASS, 
                              0);
	if( m_hSession==NULL ) return -1;

	WinHttpSetTimeouts( m_hSession, 15*1000 /*DNS*/, 15*1000 /*Connect*/, 15*1000 /*Send*/, 15*1000 /*Recv*/ );

	// URL解析.
	ZeroMemory( &m_urlcomp, sizeof(m_urlcomp) );
	m_urlcomp.dwStructSize = sizeof(m_urlcomp);

	int bufsize = (int)wcslen(url);
	allocbuffer( bufsize );

	m_urlcomp.lpszHostName = m_host;
	m_urlcomp.lpszUrlPath = m_path;
	m_urlcomp.dwHostNameLength = m_urlcomp.dwUrlPathLength = bufsize;

	b = WinHttpCrackUrl( url, 0, 0, &m_urlcomp );
	if( !b ){
		DWORD err = GetLastError();
		dprintf( L"WinHttpCrackUrl:%d\n", err );
		return -1;
	}
	dprintf(L"server=%s:%d,", m_urlcomp.lpszHostName, m_urlcomp.nPort );
	dprintf(L"path=%s\n", m_urlcomp.lpszUrlPath );
	dprintf(L"scheme=%d\n", m_urlcomp.nScheme );

	// connect
    m_hConnect = WinHttpConnect( m_hSession, 
								 m_host,
								 m_urlcomp.nPort, 
                                 0);
	if( m_hConnect==NULL ){
		r = -1; goto end;
	}

	// request
    m_hRequest = WinHttpOpenRequest( m_hConnect, 
		                             postdata?L"POST":L"GET", 
									 m_path, 
                                     NULL, 
                                     WINHTTP_NO_REFERER, 
                                     WINHTTP_DEFAULT_ACCEPT_TYPES,
									 m_urlcomp.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0 );
	if( m_hRequest==NULL ){
		r = -1; goto end;
	}

	b = WinHttpAddRequestHeaders( m_hRequest,
		                         L"Content-Type: application/x-www-form-urlencoded", -1,
		    					  WINHTTP_ADDREQ_FLAG_ADD|WINHTTP_ADDREQ_FLAG_REPLACE );

	// send
    b = WinHttpSendRequest( m_hRequest,							// hRequest
                            WINHTTP_NO_ADDITIONAL_HEADERS,		// pwszHeaders
                            0,									// dwHeadersLength
                            (LPVOID)postdata,					// lpOptional
							postlen,							// dwOptionalLength
							postlen,							// dwTotalLength
							0);									// dwContext
	if( !b ){
		r = -1; goto end;
	}

	// receive
	b = WinHttpReceiveResponse( m_hRequest, NULL);
	if( !b ){
		r = -1; goto end;
	}

	DWORD dwBufferLength;
	WinHttpQueryHeaders( m_hRequest,
		                 WINHTTP_QUERY_STATUS_CODE,
						 WINHTTP_HEADER_NAME_BY_INDEX,
						 WINHTTP_NO_OUTPUT_BUFFER, &dwBufferLength,
						 WINHTTP_NO_HEADER_INDEX );
	DWORD error = GetLastError();
	if(ERROR_INSUFFICIENT_BUFFER != error) {
		dprintf(L"*** Unexpected Error @ %d ***\n", error );
		r = -1;
		goto end;
	}
	WCHAR *lpOutBuffer = (WCHAR*)malloc( dwBufferLength );
	WinHttpQueryHeaders( m_hRequest,
		                 WINHTTP_QUERY_STATUS_CODE,
						 WINHTTP_HEADER_NAME_BY_INDEX,
						 lpOutBuffer, &dwBufferLength,
						 WINHTTP_NO_HEADER_INDEX );
	r = _wtoi( lpOutBuffer );
	free( lpOutBuffer );

	buffersize = 0;
	DWORD dwSize;
    do {
        // 利用可能なデータがあるかチェックする
        if( !WinHttpQueryDataAvailable( m_hRequest, &dwSize ) ){
            dprintf( L"Error %u in WinHttpQueryDataAvailable.\n", GetLastError() );
			SAFE_FREE( buffer );
			buffersize = 0;
			r = -1;
			break;
        }
		//dprintf( L"data available %d bytes.\n", dwSize );
		if( dwSize==0 ) break;

        // バッファをdwSize分だけ増やす.
		buffersize += dwSize;
		char*tmp = (char*)realloc( buffer, buffersize );
		if( tmp ){
	        buffer = tmp;
		}

		if( tmp==NULL ){
			// realloc失敗は元のバッファをfreeしないと.
            dprintf( L"Out of memory\n");
			SAFE_FREE( buffer );
			buffersize = 0;
			r = -1;
			break;
		}else{
            // Read the data.
			DWORD dwDownloaded;
			if( !WinHttpReadData( m_hRequest, &buffer[buffersize-dwSize], dwSize, &dwDownloaded ) ){
                dprintf( L"Error %u in WinHttpReadData.\n", GetLastError());
			}else{
				//dprintf( L"%d byte data readed.\n", dwDownloaded );
				buffersize -= dwSize-dwDownloaded;
			}
        }

    } while( dwSize>0 );
	dprintf( L"%d bytes readed.\n", buffersize );

#if defined(DEBUG) && defined(HTTP_STORE_FILE)
	static int fcnt = 0;
	if( buffer && buffersize>0 ){
		char fname[4096];
		sprintf( fname, "d:\\test-%d.txt", fcnt );
		fcnt++;
		FILE*file = fopen( fname, "wb" );
		if( file ){
			fwrite( buffer, 1, buffersize, file );
			fclose(file);
		}
	}
#endif

	*data = buffer;
	*datalen = buffersize;

  end:
	cleanup();
	return r;
}

void tHttp::cleanup()
{
	if( m_hRequest ) WinHttpCloseHandle( m_hRequest );
	if( m_hConnect ) WinHttpCloseHandle( m_hConnect );
	if( m_hSession ) WinHttpCloseHandle( m_hSession );
	m_hRequest = m_hConnect = m_hSession = NULL;
}

tSocket::tSocket()
{
	m_socket = INVALID_SOCKET;
}

tSocket::~tSocket()
{
	close();
}

SOCKET tSocket::open()
{
	m_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	return m_socket;
}

int tSocket::connect( const char*hostname, uint16_t port )
{
	struct hostent *hent;
	struct sockaddr_in saddr;
	ZeroMemory( &saddr, sizeof(saddr) );

	hent = gethostbyname( hostname );
	if( hent==NULL ){
		dprintf( L"gethostbyname() failed.\n" );
		return -1;
	}

	dprintf( L"connect to %S:%d.\n", hostname, port );
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons( port );
	saddr.sin_addr.s_addr = (uint32_t)(*(uint32_t*)hent->h_addr_list[0] );
	return connect( (struct sockaddr*)&saddr, sizeof(saddr) );
}

int tSocket::connect( const sockaddr*name, int namelen )
{
	if( m_socket==INVALID_SOCKET ) open();
	struct sockaddr_in *saddr = (struct sockaddr_in*)name;
	dprintf( L"connect to %S:%d.\n", inet_ntoa( saddr->sin_addr ), ntohs( saddr->sin_port) );
	return ::connect( m_socket, name, namelen );
}

int tSocket::send( const char*buf, int len )
{
	if( m_socket==INVALID_SOCKET ) return -1;
	return ::send( m_socket, buf, len, 0 );
}

int tSocket::recv( char*buf, int len )
{
	if( m_socket==INVALID_SOCKET ) return -1;
	return ::recv( m_socket, buf, len, 0 );
}

int tSocket::close()
{
	if( m_socket==INVALID_SOCKET ) return -1;
	return ::closesocket( m_socket );
}
