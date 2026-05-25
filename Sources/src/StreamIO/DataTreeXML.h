#ifndef __DATATREEXML_H__
#define __DATATREEXML_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "MSXMLImport.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ��� ��� ���������� ���������� ������� ��� ����, �����, ����� ����� ���� ���������� com_ptr ������� � STL ����������
// �������, �������, �� ������ ������� com_ptr - ����� ������
template <class TYPE>
struct SCOMPtr
{
	TYPE data;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SNodeslList
{
	SCOMPtr<MSXML2::IXMLDOMNodeListPtr> nodes;
	int nCurrElement;
	//
	SNodeslList() : nCurrElement( -1 ) {  }
};
class CDataTreeXML : public IDataTree
{
	OBJECT_MINIMAL_METHODS( CDataTreeXML );
	//
	CPtr<IDataStream> pStream;						// stream, this table was open with
	MSXML2::IXMLDOMDocumentPtr xmlDocument;				// �������� ��������
	//
	std::list< SCOMPtr<MSXML2::IXMLDOMNodePtr> > nodes;	// ���� ����� �� �������� ���������
	std::list< SNodeslList > nodelists;					// ���� ������� ����� �� �������� ���������
	MSXML2::IXMLDOMNodePtr xmlCurrNode;						// ������� node
	//
	std::list< SCOMPtr<MSXML2::IXMLDOMElementPtr> > elements;	// ���� ��������� �� �������� ���������
	std::list< SCOMPtr<MSXML2::IXMLDOMElementPtr> > arrbases; // ���� ��������� ��������� �������� �� �������� ���������
	MSXML2::IXMLDOMElementPtr xmlCurrElement;			// ������� ������� � ������� ��������� ��� ������
	//
	IDataTree::EAccessMode eMode;
	//
	// �������� �� �������� node ������� �� �����.
	MSXML2::IXMLDOMNodePtr GetAttribute( DTChunkID idChunk )
	{
		NI_ASSERT_TF( xmlCurrNode != 0, "can't get attribute - no current node set", return 0 );
		return xmlCurrNode->attributes->getNamedItem( idChunk );
	}
	// �������� ��������� node �� �����. ��� ���� ������� �������� node, ���� single node �� ��������.
	MSXML2::IXMLDOMNodePtr GetTextNode( DTChunkID idChunk )
	{
		NI_ASSERT_TF( xmlCurrNode != 0, "can't get node - no current node set", return 0 );
		MSXML2::IXMLDOMNodePtr xmlNode = xmlCurrNode->attributes->getNamedItem( idChunk );
		if ( xmlNode == 0 )
			xmlNode = xmlCurrNode->selectSingleNode( idChunk );
		return xmlNode;
	}
public:
	CDataTreeXML( IDataTree::EAccessMode eMode );
	virtual ~CDataTreeXML();
	//
	bool Open( IDataStream *pStream, DTChunkID idBaseNode );
	// is opened in the READ mode?
	virtual bool STDCALL IsReading() const { return eMode == IDataTree::READ; }
	// start new complex chunk
	virtual int STDCALL StartChunk( DTChunkID idChunk );
	// finish complex chunk
	virtual void STDCALL FinishChunk();
	// simply data chunk: text, integer, fp
	virtual int STDCALL GetChunkSize();
	virtual bool STDCALL RawData( void *pData, int nSize );
	virtual bool STDCALL StringData( char *pData );
	virtual bool STDCALL StringData( WORD *pData );
	virtual bool STDCALL DataChunk( DTChunkID idChunk, int *pData );
	virtual bool STDCALL DataChunk( DTChunkID idChunk, double *pData );
	// array data serialization (special case)
	virtual int STDCALL CountChunks( DTChunkID idChunk );
	virtual bool STDCALL SetChunkCounter( int nCount );
	virtual int STDCALL StartContainerChunk( DTChunkID idChunk );
	virtual void STDCALL FinishContainerChunk();
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void InitCOM();
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DATATREEXML_H__
