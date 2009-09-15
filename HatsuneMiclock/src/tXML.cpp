#include "tXML.h"



tXML::tXML( const char*xml, int len )
{
	m_xmldoc = xmlReadMemory( xml, len, "noname.xml", NULL, 0 );
	m_root = xmlDocGetRootElement( m_xmldoc );
}

tXML::~tXML()
{
	if( m_xmldoc ){
		xmlFreeDoc( m_xmldoc );
	}
	xmlCleanupParser();
}

char* tXML::FindAttribute( xmlNode*node, const char*attrname )
{
	xmlAttrPtr attr;
	for( attr = node->properties; attr; attr=attr->next ){
		if( attr->type==XML_ATTRIBUTE_NODE && strcmp((char*)attr->name,attrname)==0 ){
			if( attr->children && attr->children->content ){
				return (char*)attr->children->content;
			}
		}
	}
	return NULL;
}

// �w��̃m�[�h�������ŏ��̃G�������g��Ԃ�.
xmlNode* tXML::FindFirstNode( xmlNode*node, const char*nodename )
{
	xmlNode *cur = NULL;
	xmlNode *find = NULL;
	for( cur=node; cur; cur=cur->next ){
		// ���݂̃m�[�h�������Ώۂł����.
		if( cur->type==XML_ELEMENT_NODE && strcmp( (char*)cur->name, nodename )==0 ){
			return cur;
		}
		// �����łȂ���Ύq������T��.
		find = FindFirstNode( cur->children, nodename );
		// �q���̒��ɂ���΂����.
		if( find ) return find;
	}
	// �ǂ����Ă�������Ȃ��Ƃ�.
	return NULL;
}


xmlNode* tXML::FindFirstNodeFromRoot( const char*nodename )
{
	return FindFirstNode( m_root, nodename );
}

char* tXML::FindFirstTextNode( xmlNode*node, const char*nodename )
{
	xmlNode *cur = NULL;
	char *find = NULL;
	for( cur=node; cur; cur=cur->next ){
		// ���݂̃m�[�h�������Ώۂł����.
		if( cur->type==XML_TEXT_NODE ){
			if( cur->parent && strcmp( (char*)cur->parent->name, nodename )==0 ){
				return (char*)cur->content;
			}
		}
		// �����łȂ���Ύq������T��.
		find = FindFirstTextNode( cur->children, nodename );
		// �q���̒��ɂ���΂����.
		if( find ) return find;
	}
	// �ǂ����Ă�������Ȃ��Ƃ�.
	return NULL;
}

char* tXML::FindFirstTextNodeFromRoot( const char*nodename )
{
	return FindFirstTextNode( m_root, nodename );
}
