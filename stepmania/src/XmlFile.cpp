#include "global.h"
#include "XmlFile.h"
#include <iostream>
#include <sstream>
#include <string>
#include "RageFile.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "DateTime.h"
#include "Foreach.h"


static const char chXMLTagOpen		= '<';
static const char chXMLTagClose	= '>';
static const char chXMLTagQuestion	= '?';	// used in checking for meta tags: "<?TAG ... ?/>"
static const char chXMLTagPre	= '/';
static const char chXMLEscape = '\\';	// for value field escape


static const XENTITY x_EntityTable[] = {
		{ '&', ("&amp;"), 5 } ,
		{ '\"', ("&quot;"), 6 } ,
		{ '\'', ("&apos;"), 6 } ,
		{ '<', ("&lt;"), 4 } ,
		{ '>', ("&gt;"), 4 } 
	};

PARSEINFO piDefault;
DISP_OPT optDefault;
XENTITYS entityDefault((XENTITY*)x_EntityTable, sizeof(x_EntityTable)/sizeof(x_EntityTable[0]) );

// skip spaces
char* tcsskip( const char* psz )
{
	while( psz && *psz == ' ' ) psz++;
		
	return (char*)psz;
}

// Name   : tcsechr
// Desc   : similar with strchr with escape process
// Param  : escape - will be escape character
char* tcsechr( const char* psz, int ch, int escape )
{
	char* pch = (char*)psz;
	char* prev_escape = NULL;
	while( pch && *pch )
	{
		if( *pch == escape && prev_escape == NULL )
			prev_escape = pch;
		else
		{
			prev_escape = NULL;
			if( *pch == ch ) return (char*)pch;
		}
		pch++;
	}
	return pch;
}

// Desc   : similar with strlen with escape process
// Param  : escape - will be escape character
int tcselen( int escape, const char *start, const char *end )
{
	int len = 0;
	if( end == NULL )
		end = (char*) sizeof(long);
	const char *prev_escape = NULL;
	while( start && *start && start<end )
	{
		if( *start == escape && prev_escape == NULL )
			prev_escape = start;
		else
		{
			prev_escape = NULL;
			len++;
		}
		++start;
	}
	return len;
}

// Desc   : similar with _tcscpy with escape process
// Param  : escape - will be escape character
void unescape( char *psz, int escape, char* srt, char* end = NULL )
{
	const char* pch = srt;
	if( end==NULL ) end = (char*)sizeof(long);
	const char* prev_escape = NULL;
	while( pch && *pch && pch<end )
	{
		if( *pch == escape && prev_escape == NULL )
			prev_escape = pch;
		else
		{
			prev_escape = NULL;
			*psz++ = *pch;
		}

		pch++;
	}

	*psz = '\0';
}

// Desc   : similar with strpbrk with escape process
// Param  : escape - will be escape character
char* tcsepbrk( const char* psz, const char* chset, int escape )
{
	char* pch = (char*)psz;
	char* prev_escape = NULL;
	while( pch && *pch )
	{
		if( *pch == escape && prev_escape == NULL )
			prev_escape = pch;
		else
		{
			prev_escape = NULL;
			if( strchr( chset, *pch ) )
				return (char*)pch;		
		}
		pch++;
	}
	return pch;
}

// Desc   : put string of (psz~end) on ps string
void SetString( char* psz, char* end, CString* ps, bool trim = false, int escape = 0 )
{
	if( trim )
	{
		while( psz && psz < end && isspace(*psz) ) psz++;
		while( (end-1) && psz < (end-1) && isspace(*(end-1)) ) end--;
	}
	int len = end - psz;
	if( len <= 0 ) return;
	if( escape )
	{
		len = tcselen( escape, psz, end );
		char* szTemp = new char[len];
		unescape( szTemp, escape, psz, end );
		*ps = szTemp;
		delete [] szTemp;
	}
	else
	{
		ps->assign( psz, len );
	}
}

XNode::~XNode()
{
	Close();
}

void XNode::Close()
{
	FOREACH_Child( this, p )
		SAFE_DELETE( p );
	childs.clear();
	
	FOREACH_Attr( this, p2 )
		SAFE_DELETE( p2 );
	attrs.clear();
}
	
// attr1="value1" attr2='value2' attr3=value3 />
//                                            ^- return pointer
//========================================================
// Name   : LoadAttributes
// Desc   : loading attribute plain xml text
// Param  : pszAttrs - xml of attributes
//          pi = parser information
// Return : advanced string pointer. (error return NULL)
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
char* XNode::LoadAttributes( const char* pszAttrs, PARSEINFO *pi /*= &piDefault*/)
{
	char* xml = (char*)pszAttrs;

	while( xml && *xml )
	{
		xml = tcsskip( xml );
		if( !xml )
			continue;

		// close tag
		if( *xml == chXMLTagClose || *xml == chXMLTagPre || *xml == chXMLTagQuestion )
			// wel-formed tag
			return xml;

		// XML Attr Name
		char* pEnd = strpbrk( xml, " =" );
		if( pEnd == NULL ) 
		{
			// error
			if( !pi->erorr_occur ) 
			{
				pi->erorr_occur = true;
				pi->error_pointer = xml;
				pi->error_code = PIE_ATTR_NO_VALUE;
				pi->error_string = ssprintf( ("<%s> attribute has error "), name.c_str() );
			}
			return NULL;
		}
		
		XAttr *attr = new XAttr;

		// XML Attr Name
		SetString( xml, pEnd, &attr->name );
		
		// add new attribute
		attrs.insert( pair<CString,XAttr*>(attr->name, attr) );
		xml = pEnd;
		
		// XML Attr Value
		xml = tcsskip( xml );
		if( !xml )
			continue;

		//if( xml = strchr( xml, '=' ) )
		if( *xml == '=' )
		{
			xml = tcsskip( ++xml );
			if( !xml )
				continue;
			// if " or '
			// or none quote
			int quote = *xml;
			if( quote == '"' || quote == '\'' )
				pEnd = tcsechr( ++xml, quote, chXMLEscape );
			else
			{
				//attr= value> 
				// none quote mode
				//pEnd = tcsechr( xml, ' ', '\\' );
				pEnd = tcsepbrk( xml, (" >"), chXMLEscape );
			}

			bool trim = pi->trim_value;
			char escape = pi->escape_value;
			//SetString( xml, pEnd, &attr->value, trim, chXMLEscape );	
			SetString( xml, pEnd, &attr->value, trim, escape );
			xml = pEnd;
			// ATTRVALUE 
			if( pi->entity_value && pi->entitys )
				attr->value = pi->entitys->Ref2Entity(attr->value);

			if( quote == '"' || quote == '\'' )
				xml++;
		}
	}

	// not wel-formed tag
	return NULL;
}

// <TAG attr1="value1" attr2='value2' attr3=value3 >
// </TAG>
// or
// <TAG />
//        ^- return pointer
//========================================================
// Name   : Load
// Desc   : load xml plain text
// Param  : pszXml - plain xml text
//          pi = parser information
// Return : advanced string pointer  (error return NULL)
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
char* XNode::Load( const char* pszXml, PARSEINFO *pi /*= &piDefault*/ )
{
	// Close it
	Close();

	char* xml = (char*)pszXml;

	xml = strchr( xml, chXMLTagOpen );
	if( xml == NULL )
		return NULL;

	// Close Tag
	if( *(xml+1) == chXMLTagPre ) // </Close
		return xml;

	// XML Node Tag Name Open
	xml++;
	char* pTagEnd = strpbrk( xml, " />" );
	SetString( xml, pTagEnd, &name );
	xml = pTagEnd;
	// Generate XML Attributte List
	xml = LoadAttributes( xml, pi );
	if( xml == NULL )
		return NULL;

	// alone tag <TAG ... /> or <?TAG ... ?>
	if( *xml == chXMLTagPre || *xml == chXMLTagQuestion )
	{
		xml++;
		if( *xml == chXMLTagClose )
		{
			// wel-formed tag
			++xml;

			// UGLY: We want to ignore all XML meta tags.  So, since the Node we 
			// just loaded is a meta tag, then Load ourself again using the rest 
			// of the file until we reach a non-meta tag.
			if( !name.empty() && name[0] == chXMLTagQuestion )
				xml = Load( xml, pi );

			return xml;
		}
		else
		{
			// error: <TAG ... / >
			if( !pi->erorr_occur ) 
			{
				pi->erorr_occur = true;
				pi->error_pointer = xml;
				pi->error_code = PIE_ALONE_NOT_CLOSED;
				pi->error_string = ("Element must be closed.");
			}
			// not wel-formed tag
			return NULL;
		}
	}
	else
	// open/close tag <TAG ..> ... </TAG>
	//                        ^- current pointer
	{
		// text value�� ������E�ֵ����Ѵ�.
		//if( this->value.empty() || this->value == ("") )
		if( XIsEmptyString( value ) )
		{
			// Text Value 
			char* pEnd = tcsechr( ++xml, chXMLTagOpen, chXMLEscape );
			if( pEnd == NULL ) 
			{
				if( !pi->erorr_occur ) 
				{
					pi->erorr_occur = true;
					pi->error_pointer = xml;
					pi->error_code = PIE_NOT_CLOSED;
					pi->error_string = ssprintf( "%s must be closed with </%s>", name.c_str(), name.c_str() );
				}
				// error cos not exist CloseTag </TAG>
				return NULL;
			}
			
			bool trim = pi->trim_value;
			char escape = pi->escape_value;
			//SetString( xml, pEnd, &value, trim, chXMLEscape );
			SetString( xml, pEnd, &value, trim, escape );

			xml = pEnd;
			// TEXTVALUE reference
			if( pi->entity_value && pi->entitys )
				value = pi->entitys->Ref2Entity(value);
		}

		// generate child nodes
		while( xml && *xml )
		{
			XNode *node = new XNode;
			
			xml = node->Load( xml,pi );
			if( !node->name.empty() )
			{
				childs.insert( pair<CString,XNode*>(node->name, node) );
			}
			else
			{
				delete node;
			}

			// open/close tag <TAG ..> ... </TAG>
			//                             ^- current pointer
			// CloseTag case
			if( xml && *xml && *(xml+1) && *xml == chXMLTagOpen && *(xml+1) == chXMLTagPre )
			{
				// </Close>
				xml+=2; // C
				
				xml = tcsskip( xml );
				if( xml == NULL )
					return NULL;

				CString closename;
				char* pEnd = strpbrk( xml, " >" );
				if( pEnd == NULL ) 
				{
					if( !pi->erorr_occur ) 
					{
						pi->erorr_occur = true;
						pi->error_pointer = xml;
						pi->error_code = PIE_NOT_CLOSED;
						pi->error_string = ssprintf( "it must be closed with </%s>", name.c_str() );
					}
					// error
					return NULL;
				}
				SetString( xml, pEnd, &closename );
				if( closename == this->name )
				{
					// wel-formed open/close
					xml = pEnd+1;
					// return '>' or ' ' after pointer
					return xml;
				}
				else
				{
					xml = pEnd+1;
					// not welformed open/close
					if( !pi->erorr_occur ) 
					{
						pi->erorr_occur = true;
						pi->error_pointer = xml;
						pi->error_code = PIE_NOT_NESTED;
						pi->error_string = ssprintf( "'<%s> ... </%s>' is not well-formed.", name.c_str(), closename.c_str() );

					}
					return NULL;
				}
			}
			else	// Alone child Tag Loaded
					// else �ؾ��ϴ���E���ƾ��ϴ���E�ǽɰ���.
			{
				
				//if( xml && this->value.empty() && *xml !=chXMLTagOpen )
				if( xml && XIsEmptyString( value ) && *xml !=chXMLTagOpen )
				{
					// Text Value 
					char* pEnd = tcsechr( xml, chXMLTagOpen, chXMLEscape );
					if( pEnd == NULL ) 
					{
						// error cos not exist CloseTag </TAG>
						if( !pi->erorr_occur )  
						{
							pi->erorr_occur = true;
							pi->error_pointer = xml;
							pi->error_code = PIE_NOT_CLOSED;
							pi->error_string = ssprintf( "it must be closed with </%s>", name.c_str() );
						}
						return NULL;
					}
					
					bool trim = pi->trim_value;
					char escape = pi->escape_value;
					//SetString( xml, pEnd, &value, trim, chXMLEscape );
					SetString( xml, pEnd, &value, trim, escape );

					xml = pEnd;
					//TEXTVALUE
					if( pi->entity_value && pi->entitys )
						value = pi->entitys->Ref2Entity(value);
				}
			}
		}
	}

	return xml;
}

//========================================================
// Name   : GetXML
// Desc   : convert plain xml text from parsed xml attirbute
// Param  :
// Return : converted plain string
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
bool XAttr::GetXML( RageFileBasic &f, DISP_OPT *opt /*= &optDefault*/ )
{
	return f.Write(name + "='" + (opt->reference_value&&opt->entitys?opt->entitys->Entity2Ref(value):value) + "' ") != -1;
}

//========================================================
// Name   : GetXML
// Desc   : convert plain xml text from parsed xml node
// Param  :
// Return : converted plain string
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
bool XNode::GetXML( RageFileBasic &f, DISP_OPT *opt /*= &optDefault*/ )
{
	// tab
	if( opt && opt->newline )
	{
		if( opt && opt->newline )
			if( f.Write("\r\n") == -1 )
				return false;
		if( opt->write_tabs )
			for( int i = 0 ; i < opt->tab_base ; i++)
				if( f.Write("\t") == -1 )
					return false;
	}

	// <TAG
	if( f.Write("<" + name) == -1 )
		return false;

	// <TAG Attr1="Val1" 
	if( !attrs.empty() )
		if( f.Write(" ") == -1 )
			return false;
	FOREACH_Attr( this, p )
		if( !p->GetXML(f, opt) )
			return false;
	
	if( childs.empty() && value.empty() )
	{
		// <TAG Attr1="Val1"/> alone tag 
		if( f.Write("/>") == -1 )
			return false;
	}
	else
	{
		// <TAG Attr1="Val1"> and get child
		if( f.Write(">") == -1 )
			return false;
			
		if( opt && opt->newline && !childs.empty() )
		{
			opt->tab_base++;
		}

		FOREACH_Child( this, p )
			if( !p->GetXML( f, opt ) )
				return false;
		
		// Text Value
		if( value != ("") )
		{
			if( opt && opt->newline && !childs.empty() )
			{
				if( opt && opt->newline )
					if( f.Write("\r\n") == -1 )
						return false;
				if( opt->write_tabs )
					for( int i = 0 ; i < opt->tab_base ; i++)
						if( f.Write("\t") == -1 )
							return false;
			}
			if( f.Write((opt->reference_value&&opt->entitys?opt->entitys->Entity2Ref(value):value)) == -1 )
				return false;
		}

		// </TAG> CloseTag
		if( opt && opt->newline && !childs.empty() )
		{
			if( f.Write("\r\n") == -1 )
				return false;
			if( opt->write_tabs )
				for( int i = 0 ; i < opt->tab_base-1 ; i++)
					if( f.Write("\t") == -1 )
						return false;
		}
		if( f.Write("</" + name + ">") == -1 )
			return false;

		if( opt && opt->newline )
		{
			if( !childs.empty() )
				opt->tab_base--;
		}
	}
	return true;
}

//========================================================
// Name   : GetValue
// Desc   : 
// Param  :
// Return : 
//--------------------------------------------------------
// Coder    Date                      Desc
//========================================================
void XNode::GetValue(CString &out) const	{ out = value; }
void XNode::GetValue(int &out) const		{ out = atoi(value); }
void XNode::GetValue(float &out) const		{ out = strtof(value, NULL); }
void XNode::GetValue(bool &out) const		{ out = atoi(value) != 0; }
void XNode::GetValue(unsigned &out) const	{ out = (unsigned)atoi(value) != 0; }
void XNode::GetValue(DateTime &out) const	{ out.FromString( value ); }

void XAttr::GetValue(CString &out) const	{ out = value; }
void XAttr::GetValue(int &out) const		{ out = atoi(value); }
void XAttr::GetValue(float &out) const		{ out = strtof(value, NULL); }
void XAttr::GetValue(bool &out) const		{ out = atoi(value) != 0; }
void XAttr::GetValue(unsigned &out) const	{ out = (unsigned)atoi(value) != 0; }
void XAttr::GetValue(DateTime &out) const	{ out.FromString( value ); }

//========================================================
// Name   : SetValue
// Desc   : 
// Param  :
// Return : 
//--------------------------------------------------------
// Coder    Date                      Desc
//========================================================
void XNode::SetValue(int v)				{ value = ssprintf("%d",v); }
void XNode::SetValue(float v)			{ value = ssprintf("%f",v); }
void XNode::SetValue(bool v)			{ value = ssprintf("%d",v); }
void XNode::SetValue(unsigned v)		{ value = ssprintf("%u",v); }
void XNode::SetValue(const DateTime &v) { value = v.GetString(); }

//========================================================
// Name   : GetAttr
// Desc   : get attribute with attribute name
// Param  :
// Return : 
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
const XAttr *XNode::GetAttr( const char* attrname ) const
{
	multimap<CString, XAttr*>::const_iterator it = attrs.find( name );
	if( it != attrs.end() )
		return it->second;
	return NULL;
}

XAttr *XNode::GetAttr( const char* attrname )
{
	multimap<CString, XAttr*>::iterator it = attrs.find( name );
	if( it != attrs.end() )
		return it->second;
	return NULL;
}

//========================================================
// Name   : GetAttrValue
// Desc   : get attribute with attribute name, return its value
// Param  :
// Return : 
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
const char*	XNode::GetAttrValue( const char* attrname )
{
	XAttr *attr = GetAttr( attrname );
	return attr ? (const char*)attr->value : NULL;
}


//========================================================
// Name   : GetChildCount
// Desc   : get child node count
// Param  :
// Return : 0 return if no child
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-12-26
//========================================================
int	XNode::GetChildCount()
{
	return childs.size();
}

//========================================================
// Name   : GetChild
// Desc   : Find child with name and return child
// Param  :
// Return : NULL return if no child.
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
XNode *XNode::GetChild( const char* name )
{
	multimap<CString, XNode*>::iterator it = childs.find( name );
	if( it != childs.end() )
		return it->second;
	return NULL;
}

const XNode *XNode::GetChild( const char* name ) const
{
	multimap<CString, XNode*>::const_iterator it = childs.find( name );
	if( it != childs.end() )
		return it->second;
	return NULL;
}

//========================================================
// Name   : GetChildValue
// Desc   : Find child with name and return child's value
// Param  :
// Return : NULL return if no child.
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
const char*	XNode::GetChildValue( const char* name )
{
	XNode *node = GetChild( name );
	return (node != NULL)? (const char*)node->value : NULL;
}

XAttr *XNode::GetChildAttr( const char* name, const char* attrname )
{
	XNode *node = GetChild(name);
	return node ? node->GetAttr(attrname) : NULL;
}

const char* XNode::GetChildAttrValue( const char* name, const char* attrname )
{
	XAttr *attr = GetChildAttr( name, attrname );
	return attr ? (const char*)attr->value : NULL;
}


//========================================================
// Name   : AppendChild
// Desc   : add node
// Param  :
// Return : 
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
XNode *XNode::AppendChild( const char* name, const char* value )		{ XNode *p = new XNode; p->name = name; p->value = value; return AppendChild( p ); }
XNode *XNode::AppendChild( const char* name, float value )				{ XNode *p = new XNode; p->name = name; p->SetValue( value ); return AppendChild( p ); }
XNode *XNode::AppendChild( const char* name, int value )				{ XNode *p = new XNode; p->name = name; p->SetValue( value ); return AppendChild( p ); }
XNode *XNode::AppendChild( const char* name, unsigned value )			{ XNode *p = new XNode; p->name = name; p->SetValue( value ); return AppendChild( p ); }
XNode *XNode::AppendChild( const char* name, const DateTime &value )	{ XNode *p = new XNode; p->name = name; p->SetValue( value ); return AppendChild( p ); }

//========================================================
// Name   : AppendChild
// Desc   : add node
// Param  :
// Return : 
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
XNode *XNode::AppendChild( XNode *node )
{
	childs.insert( pair<CString,XNode*>(node->name,node) );
	return node;
}

//========================================================
// Name   : RemoveChild
// Desc   : detach node and delete object
// Param  :
// Return : 
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
bool XNode::RemoveChild( XNode *node )
{
	FOREACHMM( CString, XNode*, childs, p )
	{
		if( p->second == node )
		{
			SAFE_DELETE( p->second );
			childs.erase( p );
			return true;
		}
	}
	return false;
}


//========================================================
// Name   : AppendAttr
// Desc   : add attribute
// Param  :
// Return : 
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
XAttr *XNode::AppendAttr( XAttr *attr )
{
	attrs.insert( pair<CString,XAttr*>(attr->name,attr) );
	return attr;
}

//========================================================
// Name   : RemoveAttr
// Desc   : detach attribute and delete object
// Param  :
// Return : 
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
bool XNode::RemoveAttr( XAttr *attr )
{
	FOREACHMM( CString, XAttr*, attrs, p )
	{
		if( p->second == attr )
		{
			SAFE_DELETE( p->second );
			attrs.erase( p );
			return true;
		}
	}
	return false;
}

//========================================================
// Name   : CreateNode
// Desc   : Create node object and return it
// Param  :
// Return : 
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
XNode *XNode::CreateNode( const char* name /*= NULL*/, const char* value /*= NULL*/ )
{
	XNode *node = new XNode;
	node->name = name;
	node->value = value;
	return node;
}

//========================================================
// Name   : CreateAttr
// Desc   : create Attribute object and return it
// Param  :
// Return : 
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
XAttr *XNode::CreateAttr( const char* name /*= NULL*/, const char* value /*= NULL*/ )
{
	XAttr *attr = new XAttr;
	attr->name = name;
	attr->value = value;
	return attr;
}

//========================================================
// Name   : AppendAttr
// Desc   : add attribute
// Param  :
// Return : 
//--------------------------------------------------------
// Coder    Date                      Desc
// bro      2002-10-29
//========================================================
XAttr *XNode::AppendAttr( const char* name /*= NULL*/, const char* value /*= NULL*/ )
{
	return AppendAttr( CreateAttr( name, value ) );
}

XAttr *XNode::AppendAttr( const char* name, float value ){ return AppendAttr(name,ssprintf("%f",value)); }
XAttr *XNode::AppendAttr( const char* name, int value )	{ return AppendAttr(name,ssprintf("%d",value)); }
XAttr *XNode::AppendAttr( const char* name, unsigned value )	{ return AppendAttr(name,ssprintf("%u",value)); }


XENTITYS::XENTITYS( XENTITY *entities, int count )
{
	for( int i = 0; i < count; i++)
		push_back( entities[i] );
}

XENTITY *XENTITYS::GetEntity( int entity )
{
	for( unsigned i = 0 ; i < size(); i ++ )
	{
		if( at(i).entity == entity )
			return (XENTITY *) (&at(i));
	}
	return NULL;
}

XENTITY *XENTITYS::GetEntity( char* entity )
{
	for( unsigned i = 0 ; i < size(); i ++ )
	{
		char* ref = (char*)at(i).ref;
		char* ps = entity;
		while( ref && *ref )
			if( *ref++ != *ps++ )
				break;
		if( ref && !*ref )	// found!
			return (XENTITY *) (&at(i));
	}
	return NULL;
}

int XENTITYS::GetEntityCount( const char* str )
{
	int nCount = 0;
	char* ps = (char*)str;
	while( ps && *ps )
		if( GetEntity( *ps++ ) ) nCount ++;
	return nCount;
}

int XENTITYS::Ref2Entity( const char* estr, char* str, int strlen )
{
	char* pes = (char*)estr;
	char* ps = str;
	char* ps_end = ps+strlen;
	while( pes && *pes && ps < ps_end )
	{
		XENTITY *ent = GetEntity( pes );
		if( ent )
		{
			// copy entity meanning char
			*ps = ent->entity;
			pes += ent->ref_len;
		}
		else
			*ps = *pes++;	// default character copy
		ps++;
	}
	*ps = '\0';
	
	// total copied characters
	return ps-str;	
}

int XENTITYS::Entity2Ref( const char* str, char* estr, int estrlen )
{
	char* ps = (char*)str;
	char* pes = (char*)estr;
	char* pes_end = pes+estrlen;
	while( ps && *ps && pes < pes_end )
	{
		XENTITY *ent = GetEntity( *ps );
		if( ent )
		{
			// copy entity string
			char* ref = (char*)ent->ref;
			while( ref && *ref )
				*pes++ = *ref++;
		}
		else
			*pes++ = *ps;	// default character copy
		ps++;
	}
	*pes = '\0';
	
	// total copied characters
	return pes-estr;
}

CString XENTITYS::Ref2Entity( const char* estr )
{
	CString es;
	if( estr )
	{
		int len = strlen(estr);
//		char* esbuf = es.GetBufferSetLength( len+1 );
		char* szTemp = new char[len+1];
		if( szTemp )
			Ref2Entity( estr, szTemp, len );
		es = szTemp;
		delete [] szTemp;
	}
	return es;
}

CString XENTITYS::Entity2Ref( const char* str )
{
	CString s;
	if( str )
	{
		int nEntityCount = GetEntityCount(str);
		if( nEntityCount == 0 )
			return CString(str);
		int len = strlen(str) + nEntityCount*10 ;
		//char* sbuf = s.GetBufferSetLength( len+1 );
		char* szTemp = new char[len+1];
		if( szTemp )
			Entity2Ref( str, szTemp, len );
		s = szTemp;
		delete [] szTemp;
	}
	return s;
}

CString XRef2Entity( const char* estr )
{
	return entityDefault.Ref2Entity( estr );
}

CString XEntity2Ref( const char* str )
{
	return entityDefault.Entity2Ref( str );
}

bool XNode::LoadFromFile( const CString &sFile, PARSEINFO *pi )
{
	RageFile f;
	if( !f.Open(sFile, RageFile::READ) )
	{
		LOG->Warn("Couldn't open %s for reading: %s", sFile.c_str(), f.GetError().c_str() );
		return false;
	}
	CString s;
	if( f.Read( s ) == -1 )
	{
		if( pi )
		{
			pi->erorr_occur = true;
			pi->error_pointer = NULL;
			pi->error_code = PIE_READ_ERROR;
			pi->error_string = f.GetError();
		}
		
		return false;
	}
	this->Load( s, pi );
	return true;
}

bool XNode::SaveToFile( RageFileBasic &f, DISP_OPT *opt )
{
	f.PutLine( "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" );
	if( !opt->stylesheet.empty() )
		f.PutLine( "<?xml-stylesheet type=\"text/xsl\" href=\"" + opt->stylesheet + "\"?>" );
	if( !this->GetXML(f, opt) )
		return false;
	if( f.Flush() == -1 )
		return false;
	return true;
}

bool XNode::SaveToFile( const CString &sFile, DISP_OPT *opt )
{
	RageFile f;
	if( !f.Open(sFile, RageFile::WRITE) )
	{
		LOG->Warn("Couldn't open %s for writing: %s", sFile.c_str(), f.GetError().c_str() );
		return false;
	}

	return SaveToFile( f, opt );
}

bool XIsEmptyString( const char* str )
{
	CString s(str);
	TrimLeft( s );
	TrimRight( s );

	return ( s.empty() || s == "" );
}
