#include "stdafx.h"

#include "GroupLogic.h"
#include "GroupUnit.h"
#include "CommonUnit.h"
#include "AIUnit.h"
int CGroupLogic::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &groupIds );
	saver.Add( 2, &groupUnits );
	saver.Add( 5, &followingUnits );
	
	saver.Add( 9, &lastSegmTime );
	saver.Add( 11, &registeredGroups );
	saver.Add( 12, &ambushGroups );
	saver.Add( 13, &ambushUnits );
	saver.Add( 14, &lastAmbushCheck );

	saver.Add( 17, &segmUnits );
	saver.Add( 18, &freezeUnits );
	saver.Add( 19, &firstPathUnits );
	saver.Add( 20, &secondPathUnits );
	
	if ( segmUnits.empty() )
		segmUnits.resize( 2 );
	
	return 0;
}
int CGroupUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &nGroup );
	saver.Add( 2, &nPos );
	saver.Add( 3, &nSubGroup );
	saver.Add( 4, &vShift );
	saver.Add( 5, &nSpecialGroup );
	saver.Add( 6,	&nSpecialPos );

	return 0;
}
