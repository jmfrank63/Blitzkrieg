#include "stdafx.h"

#include "HitsStore.h"
int CHitsStore::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &hits[0] );
	saver.Add( 2, &hits[1] );
	saver.Add( 3, &curIndex );
	saver.Add( 4, &timeOfIndexBegin );

	return 0;
}
