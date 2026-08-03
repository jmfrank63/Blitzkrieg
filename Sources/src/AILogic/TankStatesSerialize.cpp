#include "StdAfx.h"
#include "TechnicsStates.h"
#include "SerializeOwner.h"
int CTankPitLeaveState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &eState );
	saver.Add( 3, &timeStartLeave );
	return 0;
}
int CSoldierEntrenchSelfState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &eState );
	saver.Add( 3, &tiles );
	saver.Add( 4, &vHalfSize );
	saver.Add( 5, &pStats );
	saver.Add( 6, &nDBIndex );
	saver.Add( 7, &timeStartBuild );
	saver.Add( 8, &vTankPitCenter );
	return 0;
}
