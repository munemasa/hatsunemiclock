#include <windows.h>
#include <string>
#include <vector>
#include "stringlib.h"

/* mbs‚Íutf-8‚ÅAwcs‚Íutf-16‚Å */

std::wstring GetStringFromResource( UINT resourceid )
{
	const int BUF_STRING_SIZE = 8192;
	std::wstring str;
	WCHAR buf[BUF_STRING_SIZE];
	ZeroMemory( buf, sizeof(buf) );
	LoadString( GetModuleHandle(NULL), resourceid, buf, BUF_STRING_SIZE );
	str = buf;
	return str;
}



// std::string‚ğdelim‚Åsplit‚µ‚Ävector<string>‚ğ•Ô‚·.
// “n‚³‚ê‚½str‚Í”j‰ó‚³‚ê‚é.
std::vector<std::string> split(std::string&str, std::string&delim)
{
	std::vector<std::string> result;
    int cutAt;
    while( (cutAt = str.find_first_of(delim)) != str.npos ){
        if(cutAt > 0){
            result.push_back(str.substr(0, cutAt));
        }
        str = str.substr(cutAt + 1);
    }
    if(str.length() > 0){
        result.push_back(str);
    }
	return result;
}

std::vector<std::wstring> split(std::wstring&str, std::wstring&delim)
{
	std::vector<std::wstring> result;
    int cutAt;
    while( (cutAt = str.find_first_of(delim)) != str.npos ){
        if(cutAt > 0){
            result.push_back(str.substr(0, cutAt));
        }
        str = str.substr(cutAt + 1);
    }
    if(str.length() > 0){
        result.push_back(str);
    }
	return result;
}


std::string urlencode(const char*str)
{
	std::string s = str;
	return urlencode( s );
}

std::string urlencode(const std::string &str)
{
    std::string os;
    for( int i = 0; i < str.size(); i++ ) {
        char c = str[i];
        if( (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~' ){
            os += c;
        }else{
            char s[128];
            sprintf(s, "%%%02x", c & 0xff);
            os += s;
        }
    }
    return os;
}

// wstring¨string(utf-8)
void wstrtostr(const std::wstring &src, std::string &dest)
{
	// UTF-8‚ÍÅ‘å6ƒoƒCƒg‚É‚È‚é‚Ì‚Å. locale‚ÉˆË‘¶‚·‚é‚ñ‚ÅMB_CUR_MAXg‚í‚¸.
	char *mbs = new char[src.length() * 6 + 1];
	int r;
	ZeroMemory( mbs, sizeof(char) * (src.length()*6+1) );
	r = WideCharToMultiByte( CP_UTF8, 0,
		                     src.c_str(), src.length(),
		                     mbs, src.length() * 6 + 1, NULL, NULL );
	dest = mbs;
	delete [] mbs;
}

// string¨wstring(utf-8)
void strtowstr(const std::string &src, std::wstring &dest)
{
	wchar_t *wcs = new wchar_t[src.length() + 1];
	int r;
	ZeroMemory( wcs, sizeof(wchar_t) * (src.length()+1) );
	r = MultiByteToWideChar( CP_UTF8, 0,
		                    src.c_str(), -1,
		   				    wcs, src.length() + 1 );
	dest = wcs;
	delete [] wcs;
}


// ‰pš‚ğrot47‚Å.
char *rot47(char *s)
{
	char *p = s;
	while(*p){
		if(*p >= '!' && *p <= 'O') *p = ((*p + 47) % 127);
		else if(*p >= 'P' && *p <= '~') *p = ((*p - 47) % 127); p++;
	}
	return s;
}

void rot47( std::string &str )
{
	char*buf = (char*)calloc( 1, str.length()+1 );
	strcpy( buf, str.c_str() );
	rot47( buf );
	str = buf;
	free(buf);
}


void rot47( std::wstring &str )
{
	std::string s;
	wstrtostr( str, s );
	rot47( s );
	strtowstr( s, str );
}
