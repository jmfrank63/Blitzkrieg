#include "StdAfx.h"

#include "DataTreeXML.h"
#include "..\StreamIO\StreamAdaptor.h"

#include <ocidl.h>
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CDataTreeXMLAutomatic
{
	bool bInitialized;
public:
	CDataTreeXMLAutomatic() 
		: bInitialized( false ) {  }
	~CDataTreeXMLAutomatic() 
	{ 
		if ( bInitialized )
			CoUninitialize(); 
	}
	//
	void Init()
	{
		if ( !bInitialized )
			CoInitialize( 0 );
		bInitialized = true;
	}
};
static CDataTreeXMLAutomatic autoinit;
void InitCOM()
{
	autoinit.Init();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDataTreeXML::CDataTreeXML( IDataTree::EAccessMode _eMode )
: xmlDocument( "Microsoft.XMLDOM" ), eMode( _eMode ) 
{ 
	xmlDocument->put_async(VARIANT_FALSE);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDataTreeXML::~CDataTreeXML()
{
	try
	{
		FinishChunk();
		if ( pStream )
		{
			CStreamCOMAdaptor comstream( pStream );
			xmlDocument->save(_variant_t(static_cast<IUnknown*>(&comstream)));
		}
	}
	catch ( ... ) 
	{
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDataTreeXML::Open( IDataStream *_pStream, DTChunkID idBaseNode )
{
	pStream = _pStream;
	if ( IsReading() )
	{
		NI_ASSERT_TF( pStream != 0, "Can't open data table from NULL stream!", return false );
		CStreamCOMAdaptor comstream( pStream );
		if ( xmlDocument->load(_variant_t(static_cast<IUnknown*>(&comstream))) )
		{
			nodes.push_back( SCOMPtr<MSXML2::IXMLDOMNodePtr>() );
			nodes.back().data = xmlDocument;
			xmlCurrNode = xmlDocument;
			return StartChunk( idBaseNode );
		}
	}
	else
	{
		MSXML2::IXMLDOMProcessingInstructionPtr pPI = xmlDocument->createProcessingInstruction(_bstr_t("xml"), _bstr_t("version=\"1.0\"" ));
		xmlDocument->appendChild( pPI );
		return StartChunk( idBaseNode );
	}
	//
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CDataTreeXML::StartChunk( DTChunkID idChunk )
{
	if ( idChunk[0] == '\0' )
		return -1;
	if ( IsReading() )
	{
		if ( xmlCurrNode != 0 )
		{
			nodes.push_back( SCOMPtr<MSXML2::IXMLDOMNodePtr>() );
			nodes.back().data = xmlCurrNode;
		}
		if ( xmlCurrNode == 0 ) 
		{
			SStorageElementStats stats;
			pStream->GetStats( &stats );
			const std::string szError = stats.pszName != 0 ? 
				NStr::Format( "Can't start chunk \"%s\" from XML file \"%s\"", idChunk, stats.pszName ) :
				NStr::Format( "Can't start chunk \"%s\" from unknown XML file", idChunk );
			NI_ASSERT_TF( xmlCurrNode != 0, szError.c_str(), return false );
		}
		xmlCurrNode = xmlCurrNode->selectSingleNode(_bstr_t(idChunk ));
		if ( xmlCurrNode == 0 )
		{
			FinishChunk();
			return 0;
		}
		else
			return 1;
	}
	else
	{
		if ( xmlCurrElement != 0 )
		{
			elements.push_back( SCOMPtr<MSXML2::IXMLDOMElementPtr>() );
			elements.back().data = xmlCurrElement;
		}
		xmlCurrElement = xmlDocument->createElement(_bstr_t(idChunk ));
		if ( elements.empty() )
			xmlDocument->appendChild( xmlCurrElement );
		else
			elements.back().data->appendChild( xmlCurrElement );
		if ( xmlCurrElement == 0 )
		{
			FinishChunk();
			return 0;
		}
		else
			return 1;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDataTreeXML::FinishChunk()
{
	if ( IsReading() )
	{
		if ( nodes.empty() )
			xmlCurrNode = 0;
		else
		{
			xmlCurrNode = nodes.back().data;
			nodes.pop_back();
		}
	}
	else
	{
		if ( elements.empty() )
			xmlCurrElement = 0;
		else
		{
			xmlCurrElement = elements.back().data;
			elements.pop_back();
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CDataTreeXML::StartContainerChunk( DTChunkID idChunk )
{
	const std::string szChunkName = idChunk[0] == '\0' ? "data" : idChunk;
	if ( IsReading() )
	{
		if ( xmlCurrNode->selectSingleNode(_bstr_t(szChunkName.c_str())) != 0 ) 
		{
			nodes.push_back( SCOMPtr<MSXML2::IXMLDOMNodePtr>() );
			nodes.back().data = xmlCurrNode;
			MSXML2::IXMLDOMNodeListPtr xmlNodesList = xmlCurrNode->selectNodes( (szChunkName + "/item").c_str() );
			nodelists.push_back( SNodeslList() );
			nodelists.back().nodes.data = xmlNodesList;
			nodelists.back().nCurrElement = -1;
			return true;
		}
		else
			return 0;
	}
	else
	{
		if ( xmlCurrElement != 0 )
		{	
			elements.push_back( SCOMPtr<MSXML2::IXMLDOMElementPtr>() );
			elements.back().data = xmlCurrElement;
		}
		MSXML2::IXMLDOMElementPtr xmlArrayBase = xmlDocument->createElement(_bstr_t(szChunkName.c_str()) );
		arrbases.push_back( SCOMPtr<MSXML2::IXMLDOMElementPtr>() );
		arrbases.back().data = xmlArrayBase;
		if ( xmlCurrElement )
			xmlCurrElement->appendChild( xmlArrayBase );
		else
			xmlDocument->appendChild( xmlArrayBase );
		return xmlArrayBase != 0;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDataTreeXML::FinishContainerChunk()
{
	FinishChunk();
	if ( IsReading() )
		nodelists.pop_back();
	else
		arrbases.pop_back();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDataTreeXML::SetChunkCounter( int nCount )
{
	if ( IsReading() )
	{
		nodelists.back().nCurrElement = nCount;
		if ( nodelists.back().nCurrElement < nodelists.back().nodes.data->length )
			xmlCurrNode = nodelists.back().nodes.data->Getitem(nodelists.back().nCurrElement);
		else
			xmlCurrNode = 0;
		return xmlCurrNode != 0;
	}
	else
	{
		xmlCurrElement = xmlDocument->createElement(_bstr_t("item" ));
		arrbases.back().data->appendChild( xmlCurrElement );
		return xmlCurrElement != 0;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CDataTreeXML::CountChunks( DTChunkID idChunk )
{
	if ( IsReading() )
	{
		const std::string szChunkName = idChunk[0] == '\0' ? "data" : idChunk;
		MSXML2::IXMLDOMNodeListPtr xmlNodesList = xmlCurrNode->selectNodes( (szChunkName + "/item").c_str() );
		return xmlNodesList->length;
	}
	else
		return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CDataTreeXML::GetChunkSize()
{
	if ( IsReading() && (xmlCurrNode != 0) )
	{
		return strlen( (const char*)xmlCurrNode->text );
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDataTreeXML::RawData( void *pData, int nSize )
{
	std::string szBuffer;
	if ( IsReading() )
	{
		int nStrSize = GetChunkSize();
		szBuffer.resize( nStrSize );
		StringData( const_cast<char*>( szBuffer.c_str() ) );
		int nCheck = 0;
		NStr::StringToBin( szBuffer.c_str(), pData, &nCheck );
		NI_ASSERT_TF( nCheck == nSize, NStr::Format("Wrong buffer size: %d != %d", nCheck, nSize), return false );
	}
	else
	{
		szBuffer.resize( nSize * 2 );
		NStr::BinToString( pData, nSize, const_cast<char*>( szBuffer.c_str() ) );
		StringData( const_cast<char*>( szBuffer.c_str() ) );
	}
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDataTreeXML::StringData( char *pData )
{
	if ( IsReading() )
	{
		if ( xmlCurrNode )
		{
			strcpy( pData, (const char*)xmlCurrNode->text );
			return true;
		}
	}
	else
	{
		if ( xmlCurrElement )
		{
			MSXML2::IXMLDOMCharacterDataPtr xmlText = xmlDocument->createTextNode(_bstr_t(pData ));
			xmlCurrElement->appendChild( xmlText);
			return true;
		}
	}
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDataTreeXML::StringData( WORD *pData )
{
	if ( IsReading() )
	{
		if ( xmlCurrNode )
		{
			wcscpy( reinterpret_cast<wchar_t*>(pData), (const wchar_t*)_bstr_t(xmlCurrNode->text) );
			return true;
		}
	}
	else
	{
		if ( xmlCurrElement )
		{
			MSXML2::IXMLDOMCharacterDataPtr xmlText = xmlDocument->createTextNode(_bstr_t(pData ));
			xmlCurrElement->appendChild( xmlText);
			return true;
		}
	}
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDataTreeXML::DataChunk( DTChunkID idChunk, int *pData )
{
	if ( IsReading() )
	{
		if ( MSXML2::IXMLDOMNodePtr pNode = GetTextNode( idChunk ) )
		{
			sscanf( (const char*)pNode->text, "%i", pData );
			return true;
		}
	}
	else
	{
		if ( xmlCurrElement )
		{
			xmlCurrElement->setAttribute( idChunk, NStr::Format("%d", *pData) );
			return true;
		}
	}
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDataTreeXML::DataChunk( DTChunkID idChunk, double *pData )
{
	if ( IsReading() )
	{
		if ( MSXML2::IXMLDOMNodePtr pNode = GetTextNode( idChunk ) )
		{
			sscanf( (const char*)pNode->text, "%lg", pData );
			return true;
		}
	}
	else
	{
		if ( xmlCurrElement )
		{
			xmlCurrElement->setAttribute( idChunk, NStr::Format("%lg", *pData) );
			return true;
		}
	}
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
