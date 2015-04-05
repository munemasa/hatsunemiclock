#ifndef STRINGLIB_H_DEF
#define STRINGLIB_H_DEF

#include <string>
#include <vector>

std::wstring GetStringFromResource( UINT resourceid );

std::vector<std::string> split(std::string&str, std::string&delim);
std::vector<std::wstring> split(std::wstring&str, std::wstring&delim);
std::string urlencode(const char*str);
std::string urlencode(const std::string &str);

void wstrtostr(const std::wstring &src, std::string &dest);
void strtowstr(const std::string &src, std::wstring &dest);

char *rot47(char *s);
void rot47( std::string &str );
void rot47( std::wstring &str );


#endif
