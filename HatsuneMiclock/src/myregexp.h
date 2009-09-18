#ifndef MYREGEXP_H_DEF
#define MYREGEXP_H_DEF

#include "pcre.h"


class myPCRE {
	int		m_pcreflag;
	pcre	*m_re;
	const char	*m_errStr;

public:
	myPCRE( const char*utf8pattern, int flg ){
		int erroffset;
		m_pcreflag = flg;
		m_re = pcre_compile( utf8pattern, flg, &m_errStr, &erroffset, NULL );
	}
	/// ƒGƒ‰[‚ª‚ ‚Á‚½‚ç•¶š—ñ‚ğ•Ô‚·.‚È‚¯‚ê‚ÎNULL
	const char*hasError(){ return m_errStr; }

	~myPCRE(){
		free( m_re );
	}
	bool match( const char*utf8str ){
		int rc;
		int ovector[30];
		rc = pcre_exec( m_re, NULL, utf8str, strlen(utf8str), 0, PCRE_PARTIAL, ovector, 30 );
		if( rc>=0 ) return true;
		return false;
	}
};



#endif
