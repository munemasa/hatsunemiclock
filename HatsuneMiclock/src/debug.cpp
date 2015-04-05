#include "winheader.h"
#include "debug.h"


int dprintf(WCHAR*format,...)
{
    va_list ap;
    va_start(ap, format);
	WCHAR buf[4096];
    int size = wvsprintf(buf, format, ap);
    va_end(ap);
	OutputDebugString(buf);
	return 0;
}
