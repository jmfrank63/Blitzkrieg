#include "stdafx.h"

#include "CollisionInternal.h"
#include "SerializeOwner.h"
int SUnitsPair::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &pUnit1 );
	saver.Add( 2, &pUnit2 );
	saver.Add( 3, &nCollideType );

	return 0;
}
int CCollisionsCollector::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &collisions );

	return 0;
}
int CCollision::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &pPushUnit );
	saver.Add( 3, &nPriority );

	return 0;
}
int CFreeOfCollisions::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CCollision*>(this) ); 
	
	return 0;
}
int CGivingPlaceCollision::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CCollision*>(this) ); 
	saver.Add( 2, &vDir );
	saver.Add( 3, &finishPoint );
	saver.Add( 4, &timeToFinish );

	return 0;
}
int CGivingPlaceRotateCollision::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CCollision*>(this) );
	saver.Add( 2, &wDir );
	saver.Add( 3, &bTurned );

	return 0;
}
int CWaitingCollision::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CCollision*>(this) );
	saver.Add( 2, &finishTime );
	saver.Add( 3, &bLock );

	return 0;
}
int CStopCollision::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CCollision*>(this) );
	saver.Add( 2, &finishTime );

	return 0;
}
int CPlaneCollision::operator&( IStructureSaver &ss )
{
	return 0;
}
