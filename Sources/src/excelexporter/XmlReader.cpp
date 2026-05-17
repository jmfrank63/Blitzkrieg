#include "StdAfx.h"
#include "XmlReader.h"
#include "io.h"

#include "..\StreamIO\StreamAdaptor.h"

#include <ocidl.h>
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CDataTreeXMLAutomatic
{
public:
	CDataTreeXMLAutomatic() { CoInitialize( 0 ); }
	~CDataTreeXMLAutomatic() { CoUninitialize(); }
};
static CDataTreeXMLAutomatic autoinit;

IXMLDOMNodePtr CXMLReader::FindRPGNode( IXMLDOMNodePtr startNode, const char *pszNodeName )
{
	string szFindString = pszNodeName;
	IXMLDOMNodeListPtr childs = startNode->GetchildNodes();
	for ( int i=0; i<childs->length; i++ )
	{
		IXMLDOMNodePtr current = childs->item[i];
		string szNodeName = current->nodeName;
		if ( szFindString == szNodeName )
			return current;
	}

	return 0;
}

IXMLDOMNodePtr CXMLWriter::FindRPGNode( IXMLDOMNodePtr startNode, const char *pszNodeName )
{
	string szFindString = pszNodeName;
	IXMLDOMNodeListPtr childs = startNode->GetchildNodes();
	for ( int i=0; i<childs->length; i++ )
	{
		IXMLDOMNodePtr current = childs->item[i];
		string szNodeName = current->nodeName;
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
			//удалим из строчки все вхождения скобочек item(**)
			int nPos = -1;
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

				//мы нашли шаблон, удалим шаблон
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
			//проверим, может быть расхождение 
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
	if ( !xmlDocument->load( static_cast<IStream*>(&comstream) ) )
		return false;

	//опускаемся по нодам, пока не найдем ноду с именем RPG
	IXMLDOMNodePtr xmlCurrNode = xmlDocument;						// текущий node
	IXMLDOMNodeListPtr childs = xmlCurrNode->GetchildNodes();
	xmlCurrNode = FindRPGNode( childs->item[childs->length-1], pszNodeName );
	if ( xmlCurrNode == 0 )
		return false;
	
	ReadInformation( xmlCurrNode, "", result, crapFields, bIgnoreFields );
	return true;
}

void CXMLReader::ReadInformation( IXMLDOMNodePtr node, const string &szPrefix, CXMLReadVector &result, vector<string> &crapFields, bool bIgnoreFields )
{
	IXMLDOMNodeListPtr childs = node->GetchildNodes();
	IXMLDOMNamedNodeMapPtr attributes = node->Getattributes();
	//сначала пишем инфу для всех атрибутов этого нода
	if ( attributes )
	{
		for ( int i=0; i<attributes->length; i++ )
		{
			IXMLDOMNodePtr current = attributes->item[i];
			string szNodeName = current->nodeName;
			_bstr_t bstr = current->nodeTypedValue;
			string szNodeValue = bstr;
			
			SXMLValue val;
			val.bString = false;
			val.szName = szPrefix + szNodeName;
			val.szVal = szNodeValue;
			if ( !IsCrappedValue(val.szName, crapFields, bIgnoreFields, false) )
				result.push_back( val );
		}
	}

/*
	for ( int i=0; i<childs->length; i++ )
	{
		IXMLDOMNodePtr current = childs->item[i];
		string szNodeName = current->nodeName;
		_bstr_t bstr = current->nodeTypedValue;
		string szNodeValue = bstr;

		CXMLValue val;
		val.first = szPrefix + szNodeName;
		val.second = szNodeValue;
		result.push_back( val );
	}
*/


	int nItemIndex = 0;

	//теперь рекурсивно вызовем функцию для всех childs
	for ( int i=0; i<childs->length; i++ )
	{
		string szNewPrefix = szPrefix;
		IXMLDOMNodePtr current = childs->item[i];

		bool bString = false;
		{
			//проверяем, вдруг current это строчка, тогда ее нужно записать
			IXMLDOMNodeListPtr childs = current->GetchildNodes();
			IXMLDOMNamedNodeMapPtr attributes = current->Getattributes();
			string szNodeName = current->nodeName;
			
			if ( ((childs == 0) || (childs->length == 0)) && ((attributes == 0) || (attributes->length == 0)) )
			{
				string szVal = current->text;
				if ( szVal.size() > 0 )
				{
					string szNodeName = current->nodeName;
					
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
			string szNodeName = current->nodeName;
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

void CXMLWriter::FindNodeAndSetAttribute( IXMLDOMNodePtr startNode, const string &szName, const string &szAttributeValue )
{
	int nPos = szName.find(';');
	if ( nPos != -1 )
	{
		string szCurrentFindNodeName = szName.substr( 0, nPos );
		//Найдем child с таким именем
		IXMLDOMNodeListPtr childs = startNode->GetchildNodes();
		int i = 0;
		int nItemIndex = 0;
		for ( ; i<childs->length; i++ )
		{
			IXMLDOMNodePtr current = childs->item[i];
			string szNodeName = current->nodeName;
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
			//создадим ноду с таким именем
			IXMLDOMNodePtr newNode = xmlDocument->createElement( szCurrentFindNodeName.c_str() );
			startNode->appendChild( newNode );
			FindNodeAndSetAttribute( newNode, szNextNodeName, szAttributeValue );
		}
		else
			FindNodeAndSetAttribute( childs->item[i], szNextNodeName, szAttributeValue );
		return;
	}
	else
	{
		//без ';' значит это имя атрибута, создадим атрибут с таким именем
		IXMLDOMElementPtr element = startNode;
		if ( szName == "#text" )
		{
			IXMLDOMCharacterDataPtr xmlText = xmlDocument->createTextNode( szAttributeValue.c_str() );
			IXMLDOMNodeListPtr childs = element->GetchildNodes();
			if ( childs->item[0] != 0 )
				element->replaceChild( xmlText, childs->item[0] );
			else
				element->appendChild( xmlText );
		}
		else
			element->setAttribute( szName.c_str(), szAttributeValue.c_str() );
		
			/*
			string szCurrentFindNodeName = szName;
			
				IXMLDOMNodeListPtr attributes = startNode->attributes;
				int i = 0;
				for ( ; i<attributes->length; i++ )
				{
				IXMLDOMNodePtr current = attributes->item[i];
				string szNodeName = current->nodeName;
				if ( szNodeName == szCurrentFindNodeName )
				break;
				}
				
					if ( i == attributes->length )
					{
					//создадим атрибут с таким именем
					startNode->setAttribute( szCurrentFindNodeName.c_str(), NStr::Format("%d", *pData) );
					startNode->set>  appendChild( newNode );
					return FindNode( newNode, szNextNodeName );
					}
					else
					return FindNode( childs->item[i], szNextNodeName );
		*/
	}
}

bool CXMLWriter::SaveRPGInformationToXML( const char *pszFileName, const CXMLValuesVector &valuesVector, const char *pszNodeName )
{
	if ( _access( pszFileName, 02 ) )
	{
//		std::cout << " !Can not open file for writing! ";
		return false;
	}

	{
		CPtr<IDataStream> pStream = OpenFileStream( pszFileName, STREAM_ACCESS_READ );
		CStreamCOMAdaptor comstream( pStream );
		if ( !xmlDocument->load( static_cast<IStream*>(&comstream) ) )
			return false;
	}
	
	//опускаемся по нодам, пока не найдем ноду с именем RPG
	IXMLDOMNodePtr xmlStartNode = xmlDocument;						// начальный node
	IXMLDOMNodeListPtr childs = xmlStartNode->GetchildNodes();
	xmlStartNode = FindRPGNode( childs->item[childs->length-1], pszNodeName );
	if ( xmlStartNode == 0 )
		return false;
	
	for ( CXMLValuesVector::const_iterator it=valuesVector.begin(); it!=valuesVector.end(); ++it )
	{
		if ( it->second.size() > 0 )
			FindNodeAndSetAttribute( xmlStartNode, it->first, it->second );
	}

	//Сохраняем файл
	{
		CPtr<IDataStream> pStream = OpenFileStream( pszFileName, STREAM_ACCESS_WRITE );
		CStreamCOMAdaptor comstream( pStream );
		xmlDocument->save( static_cast<IStream*>(&comstream) );
	}

	return true;
}
