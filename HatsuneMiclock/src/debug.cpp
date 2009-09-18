#include "winheader.h"
#include "misc.h"

static tCriticalSection crit;

int dprintf(WCHAR*format,...)
{
	va_list list;
	va_start( list, format );
	int r=0;

#ifdef DEBUG
	WCHAR buf[8192];
	r = wvsprintf(buf, format, list);
	OutputDebugString(buf);
#endif

	va_end( list ); 
	return r;
}
