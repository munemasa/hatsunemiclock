#ifndef UDMESSAGE_H_DEF
#define UDMESSAGE_H_DEF

// ユーザー定義ウィンドウメッセージ等.
#include "winheader.h"
//#include "iTunesLib.h"

enum {
	WM_TTRAY_POPUP = WM_USER+1,
	WM_NNA_NOTIFY,		// wparamにコマンド、lparamにデータ.
	WM_USER_MAX,
};

// WM_NNA_NOTIFY用のコマンド.
#define NNA_REQUEST_CREATEWINDOW	(1)
#define NNA_CLOSED_NOTIFYWINDOW		(2)		// 通知ウィンドウが0個になったときに親ウィンドウに通知される.
#define NNA_UPDATE_RSS				(3)		// RSSが更新されたときに通知される.
#define NNA_RECONNECT				(4)		// 再接続要求.


// WM_TIMERのID
enum {
	TIMER_ID_DRAW = 1,
	TIMER_ID_REFRESH_TIPHELP,
	TIMER_ID_NICO_KEEPALIVE,
	TIMER_ID_3MIN,
	TIMER_ID_RSS_UPDATE,
};


#endif
