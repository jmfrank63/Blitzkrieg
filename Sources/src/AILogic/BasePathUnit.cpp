#include "StdAfx.h"

#include "BasePathUnit.h"
#include "Building.h"
#include "Entrenchment.h"
#include "PathFinder.h"
#include "AIStaticMap.h"
#include "Path.h"
extern CStaticMap theStaticMap;
extern NTimer::STime curTime;
IStaticPath* IBasePathUnit::GetPathToBuilding( CBuilding *pBuilding, int *pnEntrance )
{
	float fMinLen = 1e10;
	IStaticPath *pBestPath = 0;
	*pnEntrance = 0;

	for ( int i = 0; i < pBuilding->GetNEntrancePoints(); ++i )
	{
		const CVec2 vEntr = pBuilding->GetEntrancePoint( i );

		if ( !theStaticMap.IsLocked( AICellsTiles::GetTile( vEntr ), AI_CLASS_HUMAN ) )
		{
			IStaticPath *pPath = CreateStaticPathToPoint( vEntr, VNULL2, this );
			CPtr<IStaticPath> pGarbage;

			if ( pPath && pBuilding->IsGoodPointForRunIn( pPath->GetFinishPoint(), i ) )
			{
				const float fDist = fabs2( pPath->GetFinishPoint() - GetCenter() );
				if ( fDist < fMinLen )
				{
					fMinLen = fDist;
					pGarbage = pBestPath;
					pBestPath = pPath;
					*pnEntrance = i;
				}
				else
					pGarbage = pPath;
			}
			else
				pGarbage = pPath;
		}
	}

	return pBestPath;
}
IStaticPath* IBasePathUnit::GetPathToEntrenchment( CEntrenchment *pEntrenchment )
{
	SRect rect;
	pEntrenchment->GetBoundRect( &rect );
	const CSegment rectSegment( rect.center - rect.dir * rect.lengthBack, rect.center + rect.dir * rect.lengthAhead );

	CVec2 finishPoint;
	rectSegment.GetClosestPoint( GetCenter(), &finishPoint );

	CVec2 toRectCenter( rect.center - finishPoint );
	Normalize( &toRectCenter );
	finishPoint += toRectCenter * SConsts::TILE_SIZE;

	return CreateStaticPathToPoint( finishPoint, VNULL2, this );
}
bool IBasePathUnit::TurnToUnit( const CVec2 &targCenter )
{
	return TurnToDir( GetDirectionByVector( targCenter - GetCenter() ), false ) != 0;
}
void IBasePathUnit::TurnAgainstUnit( const CVec2 &targCenter )
{
	UpdateDirection( GetCenter() - targCenter );
}
const float IBasePathUnit::GetSpeedForFollowing()
{
	return GetMaxSpeedHere( GetCenter() );
}
bool IBasePathUnit::IsOnLockedTiles( const struct SRect &rect )
{
	CTemporaryUnitRectUnlocker unlocker( GetID() );

	if ( IsRectOnLockedTiles( rect, AI_CLASS_ANY ) )
		return true;

	for ( int i = 1; i < 16; i *= 2 )
	{
		if ( ( GetAIClass() & i ) && IsRectOnLockedTiles( rect, i ) )
			return true;
	}

	return false;
}
void IBasePathUnit::CantFindPathToFormation()
{
	nextTimeToSearchPathToFormation = curTime + SConsts::PERIOD_OF_PATH_TO_FORMATION_SEARCH + Random( 0, 1000 );
}
int IBasePathUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &nextTimeToSearchPathToFormation );

	return 0;
}
