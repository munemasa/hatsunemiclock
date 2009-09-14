#ifndef UDMESSAGE_H_DEF
#define UDMESSAGE_H_DEF

// ユーザー定義ウィンドウメッセージ等.
#include "winheader.h"
#include "iTunesLib.h"

enum {
	WM_TTRAY_POPUP = WM_ITUNES+1,
	WM_NNA_NOTIFY,		// wparamにコマンド、lparamにデータ.
	WM_USER_MAX,
};

// WM_NNA_NOTIFY用のコマンド.
#define NNA_REQUEST_CREATEWINDOW	(1)
#define NNA_CLOSED_NOTIFYWINDOW		(2)

// WM_TIMERのID
enum {
	TIMER_ID_DRAW = 1,
	TIMER_ID_REFRESH_TIPHELP,
	TIMER_ID_NICO_KEEPALIVE,
};


#endif
