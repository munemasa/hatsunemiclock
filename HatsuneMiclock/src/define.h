#ifndef DEFINE_H_DEF
#define DEFINE_H_DEF


#ifdef DEBUG
#define APP_TITLE		L"Hatsune Miclock Debug"
#else
#define APP_TITLE		L"Hatsune Miclock"
#endif

#define REG_SUBKEY		L"Software\\Miku39.jp\\HatsuneMiclock"
#define TIMER_3MIN		(3)	// 3���^�C�}�[�̎���(��)

#define	NICO_COMMUNITY_URL	L"http://ch.nicovideo.jp/community/"
#define NICO_CHANNEL_URL	L"http://ch.nicovideo.jp/channel/"
#define NICO_LIVE_URL		L"http://live.nicovideo.jp/watch/"

#define NICO_MAX_RECENT_PROGRAM		(10)		// �A���[�g�����̍ő吔.
#define NICONAMA_RSS_GET_INTERVAL	(10)		// minutes.

#define REG_COMMUNITY_SUBKEY	REG_SUBKEY L"\\Communities"


#endif
