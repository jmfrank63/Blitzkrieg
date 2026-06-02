#include "stdafx.h"

#include "SuspendedUpdates.h"
int CSuspendedUpdates::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	if ( saver.IsReading() )
		CommonInit();

	saver.Add( 1, &objectsByCells );
	saver.Add( 2, &tilesOfObj );
	saver.Add( 13, &recalledUpdates );
	saver.Add( 4, &visibleTiles );
	saver.Add( 5, &nMyParty );
	saver.Add( 16, &updates );
	saver.Add( 17, &diplomacyUpdates );

	return 0;
}
