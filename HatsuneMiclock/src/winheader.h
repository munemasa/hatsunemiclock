#ifndef WINHEADER_H_DEF
#define WINHEADER_H_DEF

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <mmsystem.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX( x_, y_ )		((x_)>(y_)?(x_):(y_))
#define MIN( x_, y_ )		((x_)<(y_)?(x_):(y_))

#define SAFE_DELETE( x_ )	if(x_){ delete (x_); (x_) = NULL; }
#define SAFE_RELEASE( x_ )  if(x_){ (x_)->Release(); (x_) = NULL; }
#define SAFE_FREE( x_ )		if(x_){ free(x_); (x_) = NULL; }

typedef unsigned __int64 uint64_t;
typedef __int64 int64_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned char uint8_t;
typedef char int8_t;

#endif
