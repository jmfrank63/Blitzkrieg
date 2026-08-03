#include "StdAfx.h"

#include "PathFinder.h"
#include "PlanePath.h"
#include "ParatrooperPath.h"
#include "TrainPath.h"
#include "TankPitPath.h"
#include "PresizePath.h"
#include "StandartPath.h"
#include "StandartSmoothSoldierPath.h"
#include "StandartSmoothMechPath.h"
#include "ArtilleryPaths.h"
#include "SerializeOwner.h"
int CCommonStaticPath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &path );
	saver.Add( 2, &nLen );
	saver.Add( 3, &startTile );
	saver.Add( 4, &finishTile );
	saver.Add( 5, &finishPoint );

	return 0;
}
int CStandartPath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 2, &pPathFinder );
	saver.Add( 3, &pStPath );
	saver.Add( 4, &startPoint );
	saver.Add( 5, &finishPoint );
	saver.Add( 6, &curStPathTile );
	saver.Add( 7, &vShift );
	saver.Add( 8, &nCurTile );
	saver.Add( 9, &nCurStaticPoint );
	saver.Add( 10, &nCurPathPoint );
	saver.Add( 11, &bSmallPathTooLong );
	saver.Add( 22, &pathPoints );
	saver.Add( 23, &nBoundTileRadius );
	saver.Add( 24, &aiClass );
	saver.Add( 25, &insertedTiles );
	saver.Add( 26, &nInsertedTiles );
	saver.Add( 27, &nCurInsertedTile );
	saver.Add( 28, &lastKnownGoodTile );

	return 0;
}
int CStandartDirPath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &dir );
	saver.Add( 2, &startPoint );
	saver.Add( 3, &finishPoint );
	saver.Add( 4, &curPoint );
	saver.Add( 5, &bFinished );

	return 0;
}
int CStandartSmoothMechPath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pPath );
	saver.Add( 2, &spline );
	saver.Add( 3, &speed );
	saver.Add( 4, &bFinished );
	saver.Add( 5, &bNotified );
	saver.Add( 6, &bMinSlowed );
	saver.Add( 7, &bMaxSlowed );
	saver.Add( 8, &bStopped );
	saver.Add( 9, &bSmoothTurn );
	
	saver.Add( 10, &p0 );
	saver.Add( 11, &p1 );
	saver.Add( 12, &p2 );
	saver.Add( 13, &p3 );
	saver.Add( 14, &predPoint );
	saver.Add( 15, &nIter );
	saver.Add( 16, &fRemain );
	saver.Add( 17, &nPoints );

	saver.Add( 18, &bCanGoForward );
	saver.Add( 19, &bCanGoBackward );
	saver.Add( 20, &lastCheckToRightTurn );
	saver.Add( 21, &vLastValidatedPoint );

	return 0;
}
int CStandartSmoothSoldierPath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &pPath );
	saver.Add( 2, &pFormation );
	saver.Add( 3, &spline );
	saver.Add( 4, &speed );
	saver.Add( 5, &bFinished );
	saver.Add( 6, &bNotified );
	saver.Add( 7, &bMinSlowed);
	saver.Add( 8, &bMaxSlowed );
	saver.Add( 9, &bStopped );
	saver.Add( 10, &bWithFormation );

	saver.Add( 11, &p0 );
	saver.Add( 12, &p1 );
	saver.Add( 13, &p2 );
	saver.Add( 14, &p3 );
	saver.Add( 15, &predPoint );
	saver.Add( 16, &nIter );
	saver.Add( 17, &fRemain );
	saver.Add( 18, &nPoints );

	return 0;
}
int CStandartSmoothPathMemento::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pPath );
	saver.Add( 2, &pFormation );

	return 0;
}
int CPlanePath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &startPoint );
	saver.Add( 2, &finishPoint );
	saver.Add( 3, &fStartZ );
	saver.Add( 4, &fFinishZ );

	return 0;
}
int CPlaneSmoothPath::CPathFraction::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &bActive );
	return 0;
}
int CPlaneSmoothPath::CLinePathFraction::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CPathFraction*>( this ) );
	saver.Add( 2, &vStart );
	saver.Add( 4, &vCurPoint );
	saver.Add( 5, &fLength );
	return 0;
}
int CPlaneSmoothPath::CArcPathFraction::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CPathFraction*>( this ) );
	saver.Add( 3, &flyCircle );
	saver.Add( 4, &wFrom );
	saver.Add( 6, &nAngleSingn );
	saver.Add( 7, &fLenght );
	saver.Add( 8, &wCurAngle );
	return 0;
}
int CPlaneSmoothPath::SMemberInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &vWorldPosition );
	saver.Add( 2, &wDirection );
	saver.Add( 3, &fCurvatureRadius );
	saver.Add( 4, &vSpeed );
	saver.Add( 5, &lastMoveTime );
	return 0;
}
int CPlaneSmoothPath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pPath );
	saver.Add( 2, &fTurnR );
	saver.Add( 3, &fSpeed );
	saver.Add( 4, &fAngleSpeed );
	saver.Add( 5, &flyCircle );
	saver.Add( 6, &circePoint );
	saver.Add( 8, &angleSign );
	saver.Add( 9, &startAngle );
	saver.Add( 10, &finishAngle );
	saver.Add( 11, &dirByLine );
	saver.Add( 12, &bFinished );
	saver.Add( 13, &bByCircle );
	saver.Add( 14, &vAngleSpeed );
	saver.Add( 15, &vCurAngleSpeed );
	saver.Add( 16, &fTurnRadiusMax );
	saver.Add( 17, &fTurnRadiusMin );
	saver.Add( 19, &bGainHeight );
	saver.Add( 20, &eState );
	saver.Add( 21, &fVerTurnRatio );
	saver.Add( 22, &segmentTime );
	saver.Add( 23, &bTrackHistory );
	saver.Add( 24, &pathHistory );
	return 0;
}
int CPlaneInFormationSmoothPath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pFormation, &saver );
	SerializeOwner( 2, &pOwner, &saver );
	return 0;
}
int CParatrooperPath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 2, &vStartPoint );
	saver.Add( 3, &vCurPoint);
	saver.Add( 4, &fSpeedLen );
	saver.Add( 5, &vFinishPoint );
	saver.Add( 6, &vHorSpeed );
	saver.Add( 7, &lastPathUpdateTime );
	saver.Add( 8, &vFinishPoint2D );
	return 0;
}
int CArtilleryCrewPath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 2, &vCurPoint );
	saver.Add( 3, &vEndPoint );
	saver.Add( 4, &fSpeedLen );
	saver.Add( 5, &bSelfSpeed );
	saver.Add( 6, &bNotInitialized );
	saver.Add( 7, &vSpeed3 );	
	return 0;
}
int CArtilleryBeingTowedPath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 2, &vCurPoint );
	saver.Add( 4, &fSpeedLen );
	saver.Add( 5, &vCurPoint2D );
	saver.Add( 6, &vSpeed );

	return 0;
}
int CTankPitPath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 2, &vCurPoint );
	saver.Add( 3, &vEndPoint );
	saver.Add( 4, &fSpeedLen );

	return 0;

}
int CPresizePath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 2, &eState );
	saver.Add( 3, &vEndPoint );
	saver.Add( 4, &vEndDir );
	saver.Add( 5, &wDesiredDir );

	saver.Add( 6, &pPathStandart ); 
	saver.Add( 7, &pPathCheat );
	saver.Add( 8, &fSpeedLen );

	return 0;
}
int CTrainPath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &edges );
	saver.Add( 2, &vStartPoint );
	saver.Add( 3, &vFinishPoint );

	return 0;
}
int CTrainSmoothPath::SPathPoint::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pPoint );

	return 0;
}
int CTrainSmoothPath::SCarriagePos::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &frontWheel );
	saver.Add( 2, &backWheel );

	return 0;
}
int CTrainSmoothPath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pOwner, &saver );
	saver.Add( 2, &pTrainPath );
	saver.Add( 3, &lastUpdateTime );
	saver.Add( 4, &fSpeed );
	saver.Add( 5, &bFinished );
	saver.Add( 6, &carriages );
	saver.Add( 7, &nRecalculating );
	saver.Add( 8, &bRecalculatedPath );
	saver.Add( 9, &vRealFinishPoint );

	if ( !saver.IsReading() )
	{
		if ( pTrainPath == 0 || IsFinished() )
			iteratorShift = -1;
		else if ( !bJustLoaded )
		{
			std::list< SPathEdge >::iterator startIter = pTrainPath->GetStartEdgeIter();			
			iteratorShift = std::distance( startIter, carriages[0].frontWheel.iter );
		}

		saver.Add( 10, &iteratorShift );
	}
	else
	{
		bJustLoaded = true;
		saver.Add( 10, &iteratorShift );
	}

	return 0;
}
int SPathEdge::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pFirstPoint );
	saver.Add( 2, &pLastPoint );

	return 0;
}
int CTrainSmoothPathMemento::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pPath );
	
	return 0;
}
