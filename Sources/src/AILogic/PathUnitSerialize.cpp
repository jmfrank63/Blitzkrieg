#include "StdAfx.h"

#include "PathUnit.h"
#include "SerializeOwner.h"
#include "Path.h"
#include "AIUnit.h"
#include "TrainPathUnit.h"
#include "TrainPath.h"
struct SAILegacyPlacement : public SSuspendedUpdate
{
	CVec2 center;													// (x, y)
	float z;															// height (mostly for planes)
	WORD dir;															// direction [0..65535) => [0..2pi), only for units
	DWORD dwNormal;												// нормаль
	float fSpeed;

	SAILegacyPlacement() : center( VNULL2 ), z( -1.0f ), dir( 0 ), dwNormal( 0 ), fSpeed( -100.0f ) { }
	
	virtual void Recall( IDataStream *pStream ) { }
	virtual void Pack( IDataStream *pStream ) const { }
};
int CPathUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pPathFinder );
	saver.Add( 3, &pCurCollision );

	saver.Add( 5, &speed );
	saver.Add( 6, &dirVec );
	saver.Add( 7, &bRightDir );
	saver.Add( 8, &bLocking );
	saver.Add( 9, &bTurning );
	saver.Add( 10, &desDir );
	saver.Add( 11, &bFoolStop );
	saver.Add( 13, &bFixUnlock );
	saver.Add( 14, &pPathMemento );
	saver.Add( 15, &pCollMemento );
	SerializeOwner( 16, &pOwner, &saver );
	saver.Add( 19, &nCollisions );
	saver.Add( 20, &pLastPushByHardCollUnit );
	saver.Add( 21, &stayTime );
	saver.Add( 22, &collStayTime );
	saver.Add( 23, &bOnLockedTiles );
	saver.Add( 24, &checkOnLockedTime );
	saver.Add( 25, &vSuspendedPoint );
	saver.Add( 26, &bTurnCalled );
	saver.Add( 27, &lastKnownGoodTile );

	saver.Add( 28, &placement );
	
	saver.Add( 29, &wDirAtBeginning );
	saver.Add( 30, &curTile );
	saver.Add( 31, &bIdle );
	saver.Add( 32, &nextSecondPathSegmTime );

	return 0;
}
int CSimplePathUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CPathUnit*>(this) );
	saver.Add( 2, &pSmoothPath );
	saver.Add( 3, &pDefaultPath );

	if ( saver.IsReading() )
	{
		if ( pSmoothPath.IsValid() )
			pSmoothPath->SetOwner( GetOwner() );
		if ( pDefaultPath.IsValid() )
			pDefaultPath->SetOwner( GetOwner() );
	}

	return 0;
}
int CTrainPathUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &carriages );
	saver.Add( 2, &nodesInside );
	saver.Add( 3, &intermNodes );
	saver.Add( 4, &pSmoothPath );
	saver.Add( 5, &pCurEdgePoint );
	saver.Add( 6, &vCenter );
	saver.Add( 7, &vSpeed );
	saver.Add( 8, &vDir );
	saver.Add( 9, &fMaxPossibleSpeed );
	saver.Add( 10, &fPassability );
	saver.Add( 11, &fTrainLength );
	saver.Add( 12, &bFrontDir );

	if ( saver.IsReading() && pSmoothPath.IsValid() )
		pSmoothPath->SetOwner( this );
	
	saver.AddTypedSuper( 13, static_cast<IBasePathUnit*>(this) );
	saver.Add( 14, &bCanMove );
	saver.Add( 15, &pPathToMove );

	saver.Add( 16, &damagedTrackCarriages );

	return 0;
}
int CCarriagePathUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CPathUnit*>(this) );
	saver.Add( 2, &pTrain );
	saver.Add( 3, &pFrontWheelPoint );
	saver.Add( 4, &pBackWheelPoint );
	saver.Add( 5, &edges );
	saver.Add( 6, &vOldDir );
	saver.Add(7, &vOldCenter );

	return 0;
}
