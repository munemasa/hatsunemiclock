#ifndef UDMESSAGE_H_DEF
#define UDMESSAGE_H_DEF

// ���[�U�[��`�E�B���h�E���b�Z�[�W��.
#include "winheader.h"
#include "iTunesLib.h"

enum {
	WM_TTRAY_POPUP = WM_ITUNES+1,
	WM_NNA_NOTIFY,		// wparam�ɃR�}���h�Alparam�Ƀf�[�^.
	WM_USER_MAX,
};

// WM_NNA_NOTIFY�p�̃R�}���h.
#define NNA_REQUEST_CREATEWINDOW	(1)
#define NNA_CLOSED_NOTIFYWINDOW		(2)

// WM_TIMER��ID
enum {
	TIMER_ID_DRAW = 1,
	TIMER_ID_REFRESH_TIPHELP,
	TIMER_ID_NICO_KEEPALIVE,
};


#endif
