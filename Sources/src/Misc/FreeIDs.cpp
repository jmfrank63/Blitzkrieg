#include "StdAfx.h"

#include "FreeIDs.h"
#include "stdafx.h"
void CFreeIds::Init( const int nElements )
{
	nexts.resize( nElements );
	preds.resize( nElements ); 
	givenIDs.clear();
	
	freePtr = 1; 
	front = 0;
}
void CFreeIds::Clear()
{
	nexts.clear();
	preds.clear();
	givenIDs.clear();

	Init();
}
void CFreeIds::AddToFreeId( int id )
{
	if ( givenIDs.find( id ) != givenIDs.end() )
	{
		NI_ASSERT_T( id, " null id" );
		NI_ASSERT_T( preds.size() >= id, "out of bounds" );
		NI_ASSERT_T( nexts.size() >= preds[id], "out of bounds" );
		nexts[preds[id]] = nexts[id];
		preds[nexts[id]] = preds[id];
		
		if ( front == id )
			front = nexts[id];
		givenIDs.erase( id );
		
		AddToFree( id );
	}
}
int CFreeIds::GetFreeId()
{
	int newPos = GetFreePos();
	
	nexts[newPos] = front;
	preds[newPos] = 0;
	
	preds[front] = newPos;
	front = newPos;

	NI_ASSERT_T( freePtr >= nexts.size() || freePtr != nexts[freePtr], "Wrong freeptr" );
	NI_ASSERT_T( givenIDs.find( newPos ) == givenIDs.end(), NStr::Format( "ID (%d) has already been given", newPos ) );
	givenIDs.insert( newPos );

	return newPos;
}
int CFreeIds::GetFreePos() 
{
	int result = freePtr;
	
	if ( nexts[freePtr] == 0 )
	{
		++freePtr;

		if ( freePtr == nexts.size() )
		{
			int nSize = nexts.size()*1.5;
			nexts.resize( nSize );
			preds.resize( nSize );
		}
	}
	else
		freePtr = nexts[freePtr];
	
	NI_ASSERT_T( freePtr != result, "Wrong freeptr" );

	return result;
}
