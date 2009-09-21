#ifndef TXML_H_DEF
#define TXML_H_DEF

#include "winheader.h"
#include "libxml/tree.h"
#include "libxml/parser.h"


class tXML {
	// ÉÅÉÇÉäÉäÅ[ÉNÇÕëÂè‰ïv.
	xmlDocPtr		m_xmldoc;
	xmlNodePtr		m_root;

public:
	tXML( const char*xml, int len, bool html=false );
	~tXML();

	static xmlNode* FindFirstNode( xmlNode*node, const char*nodename );
	static char* FindFirstTextNode( xmlNode*node, const char*nodename );
	static char* FindAttribute( xmlNode*node, const char*attrname );
	
	inline xmlNodePtr getRootNode(){ return m_root; }

	char* FindFirstTextNodeFromRoot( const char*nodename );
	xmlNode* FindFirstNodeFromRoot( const char*nodename );
};

#endif
