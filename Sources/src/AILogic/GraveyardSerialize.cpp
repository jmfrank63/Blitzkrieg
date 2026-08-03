#include "StdAfx.h"

#include "Graveyard.h"
int SKilledUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pUnit );
	saver.Add( 2, &endFogTime );
	saver.Add( 3, &endSceneTime );
	saver.Add( 4, &timeToEndDieAnimation );
	saver.Add( 5, &bSentDead );
	saver.Add( 6, &bAnimFinished );
	saver.Add( 7, &actionTime );
	saver.Add( 8, &lockedTiles );
	saver.Add( 9, &bFatality );
	saver.Add( 10, &bDisappearUpdateSent );
	saver.Add( 11, &bFogDeleted );

	return 0;
}
int CDeadUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pDieObj );
	saver.Add( 2, &dieTime );
	saver.Add( 3, &dieAction );
	saver.Add( 4, &nFatality );
	
	saver.AddTypedSuper( 5, static_cast<CLinkObject*>(this) );

	return 0;
}
int CGraveyard::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &killed );
	saver.Add( 2, &soonBeDead );
	saver.Add( 5, &bridgeDeadSoldiers );

	if ( saver.IsReading() )
	{
		std::list< CPtr<IUpdatableObj> > soldiersSet;
		saver.Add( 4, &soldiersSet );

		bridgeSoldiersSet.clear();
		bridgeSoldiersSet.insert( soldiersSet.begin(), soldiersSet.end() );
	}
	else
	{
		std::list< CPtr<IUpdatableObj> > soldiersSet( bridgeSoldiersSet.begin(), bridgeSoldiersSet.end() );

		saver.Add( 4, &soldiersSet );
	}

	return 0;
}
