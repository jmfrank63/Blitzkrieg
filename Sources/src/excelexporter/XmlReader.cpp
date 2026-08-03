#include "StdAfx.h"
#include "XmlReader.h"
#include "io.h"

#include "../StreamIO/StreamAdaptor.h"

#include <ocidl.h>
namespace
{
	inline std::string ToString( const _bstr_t &value )
	{
		const char *pszValue = value;
		return pszValue != 0 ? pszValue : "";
	}

	inline std::string ToString( const _variant_t &value )
	{
		return ToString( _bstr_t( value ) );
	}
}

class CDataTreeXMLAutomatic
{
public:
	CDataTreeXMLAutomatic() { CoInitialize( 0 ); }
	~CDataTreeXMLAutomatic() { CoUninitialize(); }
};
static CDataTreeXMLAutomatic autoinit;

MSXML2::IXMLDOMNodePtr CXMLReader::FindRPGNode( MSXML2::IXMLDOMNodePtr startNode, const char *pszNodeName )
{
	string szFindString = pszNodeName;
	MSXML2::IXMLDOMNodeListPtr childs = startNode->childNodes;
	for ( int i=0; i<childs->length; i++ )
	{
		MSXML2::IXMLDOMNodePtr current = childs->Getitem( i );
		string szNodeName = ToString( current->nodeName );
		if ( szFindString == szNodeName )
			return current;
	}

	return 0;
}

MSXML2::IXMLDOMNodePtr CXMLWriter::FindRPGNode( MSXML2::IXMLDOMNodePtr startNode, const char *pszNodeName )
{
	string szFindString = pszNodeName;
	MSXML2::IXMLDOMNodeListPtr childs = startNode->childNodes;
	for ( int i=0; i<childs->length; i++ )
	{
		MSXML2::IXMLDOMNodePtr current = childs->Getitem( i );
		string szNodeName = ToString( current->nodeName );
		if ( szFindString == szNodeName )
			return current;
	}
	
	return 0;
}

bool CXMLReader::IsCrappedValue( const std::string &szValName, const vector<string> &crapFields, bool bIgnoreFields, bool bCompareOnlyFirstSymbols )
{
	for ( int i=0; i<crapFields.size(); i++ )
	{
		string szCur = szValName;
		{
			const char *pTemp = szCur.c_str();
			do
			{
				pTemp = strstr( pTemp, "item" );
				if ( pTemp == 0 )
					break;

				if ( szCur.size() < pTemp - szCur.c_str() + 4 + 4 )
					break;			// 4 == item, 4 == (**)
				if ( pTemp[4] != '(' || pTemp[7] != ')' )
				{
					pTemp = pTemp + 8;
					continue;
				}

				if ( pTemp[5] < '0' || pTemp[5] > '9' || pTemp[6] < '0' || pTemp[6] > '9' )
				{
					pTemp = pTemp + 8;
					continue;
				}

				szCur.erase( pTemp - szCur.c_str() + 4, 4 );
				pTemp += 4;
			} while( pTemp );
		}
		szCur = szCur.substr( 0, crapFields[i].size() );

		string szCompare = crapFields[i];
		if ( bCompareOnlyFirstSymbols )
			szCompare = szCompare.substr( 0, szCur.size() );
		if ( szCur == szCompare )
		{
			if ( bIgnoreFields )
				return true;
			else
				return false;
		}

		if ( bCompareOnlyFirstSymbols )
		{
		}
	}
	
	if ( bIgnoreFields )
		return false;
	else
		return true;
}

bool CXMLReader::ReadRPGInformationFromFile( const char *pszFileName, CXMLReadVector &result, vector<string> &crapFields, bool bIgnoreFields, const char *pszNodeName )
{
	CPtr<IDataStream> pStream = CreateFileStream( pszFileName, STREAM_ACCESS_READ );
	CStreamCOMAdaptor comstream( pStream );
	if ( !xmlDocument->load( _variant_t( static_cast<IUnknown*>( &comstream ) ) ) )
		return false;

	MSXML2::IXMLDOMNodePtr xmlCurrNode = xmlDocument;						// ??????? node
	MSXML2::IXMLDOMNodeListPtr childs = xmlCurrNode->childNodes;
	xmlCurrNode = FindRPGNode( childs->Getitem( childs->length-1 ), pszNodeName );
	if ( xmlCurrNode == 0 )
		return false;
	
	ReadInformation( xmlCurrNode, "", result, crapFields, bIgnoreFields );
	return true;
}

void CXMLReader::ReadInformation( MSXML2::IXMLDOMNodePtr node, const string &szPrefix, CXMLReadVector &result, vector<string> &crapFields, bool bIgnoreFields )
{
	MSXML2::IXMLDOMNodeListPtr childs = node->childNodes;
	MSXML2::IXMLDOMNamedNodeMapPtr attributes = node->attributes;
	if ( attributes )
	{
		for ( int i=0; i<attributes->length; i++ )
		{
			MSXML2::IXMLDOMNodePtr current = attributes->Getitem( i );
			string szNodeName = ToString( current->nodeName );
			string szNodeValue = ToString( current->nodeTypedValue );
			
			SXMLValue val;
			val.bString = false;
			val.szName = szPrefix + szNodeName;
			val.szVal = szNodeValue;
			if ( !IsCrappedValue(val.szName, crapFields, bIgnoreFields, false) )
				result.push_back( val );
		}
	}

	int nItemIndex = 0;

	for ( int i=0; i<childs->length; i++ )
	{
		string szNewPrefix = szPrefix;
		MSXML2::IXMLDOMNodePtr current = childs->Getitem( i );

		bool bString = false;
		{
			MSXML2::IXMLDOMNodeListPtr currentChilds = current->childNodes;
			MSXML2::IXMLDOMNamedNodeMapPtr currentAttributes = current->attributes;
			
			if ( ((currentChilds == 0) || (currentChilds->length == 0)) && ((currentAttributes == 0) || (currentAttributes->length == 0)) )
			{
				string szVal = ToString( current->text );
				if ( szVal.size() > 0 )
				{
					string szNodeName = ToString( current->nodeName );
					
					SXMLValue val;
					val.bString = true;
					val.szName = szPrefix + szNodeName;
					val.szVal = szVal;
					if ( !IsCrappedValue(val.szName, crapFields, bIgnoreFields, false) )
						result.push_back( val );
					bString = true;
				}
			}
		}
		
		if ( !bString )
		{
			string szNodeName = ToString( current->nodeName );
			if ( szNodeName == "item" )
			{
				szNodeName = NStr::Format( "item(%.2d)", nItemIndex );
				nItemIndex++;
			}
			szNewPrefix += szNodeName + ';';
			if ( !IsCrappedValue(szNewPrefix, crapFields, bIgnoreFields, true) )
				ReadInformation( current, szNewPrefix, result, crapFields, bIgnoreFields );
		}
	}
}

void CXMLWriter::FindNodeAndSetAttribute( MSXML2::IXMLDOMNodePtr startNode, const string &szName, const string &szAttributeValue )
{
	int nPos = szName.find(';');
	if ( nPos != -1 )
	{
		string szCurrentFindNodeName = szName.substr( 0, nPos );
		MSXML2::IXMLDOMNodeListPtr childs = startNode->childNodes;
		int i = 0;
		int nItemIndex = 0;
		for ( ; i<childs->length; i++ )
		{
			MSXML2::IXMLDOMNodePtr current = childs->Getitem( i );
			string szNodeName = ToString( current->nodeName );
			if ( szNodeName == szCurrentFindNodeName )
				break;

			if ( szNodeName == "item" )
			{
				string szTempNodeName = NStr::Format( "item(%.2d)", nItemIndex );
				if ( szTempNodeName == szCurrentFindNodeName )
				{
					szCurrentFindNodeName = "item";
					break;
				}
				nItemIndex++;
			}
		}

		string szNextNodeName = szName.substr( nPos + 1 );
		if ( i == childs->length )
		{
			MSXML2::IXMLDOMNodePtr newNode = xmlDocument->createElement( _bstr_t( szCurrentFindNodeName.c_str() ) );
			startNode->appendChild( newNode );
			FindNodeAndSetAttribute( newNode, szNextNodeName, szAttributeValue );
		}
		else
			FindNodeAndSetAttribute( childs->Getitem( i ), szNextNodeName, szAttributeValue );
		return;
	}
	else
	{
		MSXML2::IXMLDOMElementPtr element = startNode;
		if ( szName == "#text" )
			element->text = _bstr_t( szAttributeValue.c_str() );
		else
			element->setAttribute( _bstr_t( szName.c_str() ), _variant_t( _bstr_t( szAttributeValue.c_str() ) ) );
	}
}

bool CXMLWriter::SaveRPGInformationToXML( const char *pszFileName, const CXMLValuesVector &valuesVector, const char *pszNodeName )
{
	if ( _access( pszFileName, 02 ) )
	{
		return false;
	}

	{
		CPtr<IDataStream> pStream = OpenFileStream( pszFileName, STREAM_ACCESS_READ );
		CStreamCOMAdaptor comstream( pStream );
		if ( !xmlDocument->load( _variant_t( static_cast<IUnknown*>( &comstream ) ) ) )
			return false;
	}
	
	MSXML2::IXMLDOMNodePtr xmlStartNode = xmlDocument;						// ????????? node
	MSXML2::IXMLDOMNodeListPtr childs = xmlStartNode->childNodes;
	xmlStartNode = FindRPGNode( childs->Getitem( childs->length-1 ), pszNodeName );
	if ( xmlStartNode == 0 )
		return false;
	
	for ( CXMLValuesVector::const_iterator it=valuesVector.begin(); it!=valuesVector.end(); ++it )
	{
		if ( it->second.size() > 0 )
			FindNodeAndSetAttribute( xmlStartNode, it->first, it->second );
	}

	{
		CPtr<IDataStream> pStream = OpenFileStream( pszFileName, STREAM_ACCESS_WRITE );
		CStreamCOMAdaptor comstream( pStream );
		xmlDocument->save( _variant_t( static_cast<IUnknown*>( &comstream ) ) );
	}

	return true;
}
