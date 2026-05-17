#include "StdAfx.h"

#include "DataTableXML.h"
#include "DataTreeXML.h"

#include "..\StreamIO\StreamAdaptor.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDataTableXML::CDataTableXML()
: xmlDocument( "Microsoft.XMLDOM" ), bModified( false )
{
	xmlDocument->async = false; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDataTableXML::~CDataTableXML()
{
	try
	{
		if ( bModified && pStream )
		{
			CStreamCOMAdaptor comstream( pStream );
			xmlDocument->save( static_cast<IStream*>(&comstream) );
		}
	}
	catch ( ... )
	{
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDataTableXML::Open( IDataStream *_pStream, const char *pszBaseNode )
{
	try
	{
		pStream = _pStream;
		CStreamCOMAdaptor comstream( pStream );
		int nPos = pStream->GetPos();
		if ( xmlDocument->load( static_cast<IStream*>(&comstream) ) )
		{
			xmlRootNode = xmlDocument->selectSingleNode( pszBaseNode );
			pStream->Seek( nPos, STREAM_SEEK_SET );
		}
		else 
		{ 
			MSXML2::IXMLDOMProcessingInstructionPtr pPI = xmlDocument->createProcessingInstruction( "xml", "version=\"1.0\"" );
			xmlDocument->appendChild( pPI );
			MSXML2::IXMLDOMElementPtr pElement = xmlDocument->createElement( pszBaseNode );
			xmlDocument->appendChild( pElement );
		}
		return true;
	}
	catch ( ... ) 
	{
	}
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** 
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MSXML2::IXMLDOMNodePtr CDataTableXML::GetNode( const std::string &szName )
{
	const int nPos = szName.rfind( '/' );
	const std::string szRestName = szName.substr( nPos + 1 );
	if ( MSXML2::IXMLDOMNodePtr xmlCurrNode = xmlRootNode->selectSingleNode(szName.substr(0, nPos).c_str()) )
	{
		if ( MSXML2::IXMLDOMNodePtr xmlNode = xmlCurrNode->attributes->getNamedItem(szRestName.c_str()) ) 
			return xmlNode;
		else
			return xmlCurrNode->selectSingleNode( szRestName.c_str() );
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline int AddToBuffer( const std::string &szName, char* &pszBuffer, const int nBufferSize, const int nTotalSize )
{
	NI_ASSERT_TF( nTotalSize + szName.size() + 2 < nBufferSize, "Buffer too small to add name", return 0 );
	strcpy( pszBuffer, szName.c_str() );
	pszBuffer += szName.size();
	*pszBuffer++ = '\0';
	return szName.size() + 1;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CDataTableXML::GetRowNames( char *pszBuffer, int nBufferSize )
{
	try
	{
		int nTotalSize = 0;
		MSXML2::IXMLDOMNodeListPtr pNodes = xmlRootNode->childNodes;
		for ( int i=0; i<pNodes->length; ++i )
		{
			std::string szName = pNodes->item[i]->nodeName;
			nTotalSize += AddToBuffer( szName, pszBuffer, nBufferSize, nTotalSize );
		}
		*pszBuffer++ = '\0';
		//
		return nTotalSize + 1;
	}
	catch ( ... ) 
	{
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CDataTableXML::GetEntryNames( const char *pszRow, char *pszBuffer, int nBufferSize )
{
	try
	{
		int nTotalSize = 0;
		MSXML2::IXMLDOMNodePtr pNode = xmlRootNode->selectSingleNode( pszRow );
		// attributes
		for ( int i=0; i<pNode->attributes->length; ++i )
		{
			const std::string szName = pNode->attributes->item[i]->nodeName;
			nTotalSize += AddToBuffer( szName, pszBuffer, nBufferSize, nTotalSize );
		}
		// named nodes
		MSXML2::IXMLDOMNodeListPtr pNodes = pNode->childNodes;
		for ( int i=0; i<pNodes->length; ++i )
		{
			std::string szName = pNodes->item[i]->nodeName;
			if ( (szName == "#comment") || (szName == "#text") ) 
				continue;
			else
			{
				std::string szRowName = std::string(pszRow) + "/" + szName;
				std::replace_if( szRowName.begin(), szRowName.end(), [](char c){ return c == '.'; }, '/' );
				//
				char buffer[65536];
				const int nSize = GetEntryNames( szRowName.c_str(), buffer, 65536 );
				if ( nSize > 1 )
				{
					const char *pos = buffer;
					while ( (*pos != 0) && (pos - buffer <= nSize) )
					{
						std::string szNewName = szName + "/" + pos;
						std::replace_if( szNewName.begin(), szNewName.end(), [](char c){ return c == '/'; }, '.' );
						nTotalSize += AddToBuffer( szNewName, pszBuffer, nBufferSize, nTotalSize );
						pos = std::find( pos, (const char*)(buffer) + nSize, '\0' ) + 1;
					}
				}
				else
				{
					nTotalSize += AddToBuffer( szName, pszBuffer, nBufferSize, nTotalSize );
				}
			}
		}
		*pszBuffer++ = '\0';
		//
		return nTotalSize + 1;
	}
	catch ( ... ) 
	{
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// get
int CDataTableXML::GetInt( const char *pszRow, const char *pszEntry, int defval )
{
	const std::string szName = MakeName( pszRow, pszEntry );
	if ( MSXML2::IXMLDOMNodePtr pNode = GetNode(szName) )
		return atoi( pNode->text );
	else
		return defval;
}
double CDataTableXML::GetDouble( const char *pszRow, const char *pszEntry, double defval )
{
	const std::string szName = MakeName( pszRow, pszEntry );
	if ( MSXML2::IXMLDOMNodePtr pNode = GetNode(szName) )
		return atof( pNode->text );
	else
		return defval;
}
const char* CDataTableXML::GetString( const char *pszRow, const char *pszEntry, const char *defval, char *pszBuffer, int nBufferSize )
{
	const std::string szName = MakeName( pszRow, pszEntry );
	if ( MSXML2::IXMLDOMNodePtr pNode = GetNode(szName) )
	{
		std::string szString = pNode->text;
		NI_ASSERT_TF( nBufferSize >= szString.size(), "Buffer too small to fill all string", return 0 );
		strcpy( pszBuffer, szString.c_str() );
	}
	else
		strcpy( pszBuffer, defval );
	return pszBuffer;
}
int CDataTableXML::GetRawData( const char *pszRow, const char *pszEntry, void *pBuffer, int nBufferSize )
{
	const std::string szName = MakeName( pszRow, pszEntry );
	if ( MSXML2::IXMLDOMNodePtr pNode = GetNode(szName) )
	{
		std::string szBuffer = pNode->text;
		NI_ASSERT_TF( nBufferSize >= szBuffer.size(), "Buffer too small to fill all string", return 0 );
		int nCheck = szBuffer.size() / 2;
		NI_ASSERT_TF( nCheck >= nBufferSize, NStr::Format("Wrong buffer size: %d >= %d", nCheck, nBufferSize), return 0 );
		NStr::StringToBin( szBuffer.c_str(), pBuffer, &nCheck );
		return nCheck;
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set
void CDataTableXML::SetInt( const char *pszRow, const char *pszEntry, int val )
{
	NI_ASSERT_T( false, "Still not implemented" );
	SetValue( pszRow, pszEntry, long(val) );
	SetModified();
}
void CDataTableXML::SetDouble( const char *pszRow, const char *pszEntry, double val )
{
	NI_ASSERT_T( false, "Still not implemented" );
	SetValue( pszRow, pszEntry, val );
	SetModified();
}
void CDataTableXML::SetString( const char *pszRow, const char *pszEntry, const char *val )
{	
	NI_ASSERT_T( false, "Still not implemented" );
	if ( MSXML2::IXMLDOMNodePtr pNode = xmlRootNode->selectSingleNode( pszRow ) )
	{
		MSXML2::IXMLDOMCharacterDataPtr xmlText = xmlDocument->createTextNode( val );
		pNode->appendChild( xmlText );
	}
	else
	{
		MSXML2::IXMLDOMElementPtr pElement = xmlDocument->createElement( pszRow );
		xmlRootNode->appendChild( pElement );
		MSXML2::IXMLDOMCharacterDataPtr xmlText = xmlDocument->createTextNode( val );
		pElement->appendChild( xmlText );
	}
	SetModified();
}
void CDataTableXML::SetRawData( const char *pszRow, const char *pszEntry, const void *pBuffer, int nBufferSize )
{
	NI_ASSERT_T( false, "Still not implemented" );
	std::string szString;
	szString.resize( nBufferSize * 2 );
	NStr::BinToString( pBuffer, nBufferSize, const_cast<char*>(szString.c_str()) );
	SetString( pszRow, pszEntry, szString.c_str() );
	SetModified();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
