#include "StdAfx.h"
#include "GUIUndo.h"

CSaveAllUndo::CSaveAllUndo( IUIElement *pElement )
{
	pStorage = OpenStorage( "memory", STREAM_ACCESS_READ | STREAM_ACCESS_WRITE, STORAGE_TYPE_MEM );
	NI_ASSERT( pStorage != 0 );
	
	CPtr<IDataStream> pStream = pStorage->CreateStream( "element", STREAM_ACCESS_WRITE );
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
	pElement->operator&( *pDT );
	
	pSavedElement = pElement;
}

void CSaveAllUndo::Undo()
{
	CPtr<IDataStream> pStream = pStorage->OpenStream( "element", STREAM_ACCESS_READ );
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ );
	pSavedElement->operator&( *pDT );
	pSavedElement = 0;
}
