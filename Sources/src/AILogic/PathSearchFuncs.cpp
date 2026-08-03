#include "StdAfx.h"

#include "PathFinder.h"
#include "BasePathUnit.h"
#include "Soldier.h"
#include "StaticObject.h"
#include "AIStaticMap.h"
#include "PointChecking.h"
#include "Path.h"
extern CStaticMap theStaticMap;
IStaticPath* CreateStaticPathToPoint( const CVec2 &finishPoint, const CVec2 &vShift, IBasePathUnit *pUnit, const bool bCanGoOutOfRadius )
{
	if ( !bCanGoOutOfRadius && !pUnit->CanGoToPoint( finishPoint ) )
		return 0;
	else
	{
		CVec2 startPoint( pUnit->GetCenter() + vShift );
		if ( !theStaticMap.IsPointInside( startPoint ) )
			startPoint = pUnit->GetCenter();
		
		return 
			pUnit->CreateBigStaticPath( startPoint, finishPoint, 0 );
	}
}
IStaticPath* CreateStaticPathToPoint( const CVec2 &_startPoint, const CVec2 &finishPoint, const CVec2 &vShift, IBasePathUnit *pUnit, const bool bCanGoOutOfRadius )
{
	if ( !bCanGoOutOfRadius && !pUnit->CanGoToPoint( finishPoint ) )
		return 0;
	else
	{
		CVec2 startPoint( _startPoint + vShift );
		if ( !theStaticMap.IsPointInside( startPoint ) )
			startPoint = _startPoint;

		return 
			pUnit->CreateBigStaticPath( startPoint, finishPoint, 0 );
	}
}
IStaticPath* CreateStaticPathForAttack( IBasePathUnit *pUnit, CAIUnit *pTarget, const float fRangeMin, const float fRangeMax, const float fRandomCant )
{
	if ( !( pTarget->GetStats()->IsInfantry() && 
				  static_cast<CSoldier*>(pTarget)->IsInBuilding() && ( static_cast<CSoldier*>(pTarget)->GetMinAngle() !=0 || static_cast<CSoldier*>(pTarget)->GetMaxAngle() != 65535 ) ) )
	{
		CPtr<IPointChecking> pPointChecking = new CAttackPointChecking( fRangeMin, Max( 0.0f, fRangeMax - 4.0f * SConsts::TILE_SIZE ), pTarget->GetTile() );

		CVec2 vRandomCant( VNULL2 );
		if ( fRandomCant != 0.0f )
		{
			const float fRandomDist = Random( 0.0f, fRandomCant );
			CVec2 vDirFromEnemy = pUnit->GetCenter() - pTarget->GetCenter();
			Normalize( &vDirFromEnemy );
			const CVec2 vPerpDir( -vDirFromEnemy.y, vDirFromEnemy.x );

			vRandomCant = vPerpDir * fRandomDist;
			if ( Random( 0.0f, 1.0f ) < 0.5f )
				vRandomCant = -vRandomCant;
		}
		
		const CVec2 vFinishPoint = pTarget->GetCenter() + vRandomCant;
		IStaticPath *pPath = pUnit->CreateBigStaticPath( pUnit->GetCenter(), vFinishPoint, pPointChecking );

		if ( pPath != 0 )
		{
			if ( !pUnit->CanGoToPoint( pPath->GetFinishPoint() ) )
			{
				CPtr<IStaticPath> pGarbage = pPath;
				return 0;
			}
			else if ( fabs2( pPath->GetFinishPoint() - pTarget->GetCenter() ) > sqr( fRangeMax ) )
			{
				CPtr<IStaticPath> pGarbage = pPath;
				return 0;
			}
			else 
			{
				if ( fRandomCant != 0.0f )
				{
					Normalize( &vRandomCant );
					pPath->MoveFinishPointBy( vRandomCant * ( SConsts::TILE_SIZE / 2 - 1 ) );
				}

				return pPath;
			}
		}
		else
			return 0;
	}
	else
	{
		CSoldier *pSoldier = static_cast<CSoldier*>( pTarget );
		CVec2 vAttackDir( GetVectorByDirection( ( pSoldier->GetMaxAngle() + pSoldier->GetMinAngle() ) / 2 ) );
		const CVec2 vDirFromEnemy( pSoldier->GetCenter() - pUnit->GetCenter() );
		const float fProj = vAttackDir * vDirFromEnemy;
		const float fMinDist = Min( fProj, fRangeMax * 3.0f / 4.0f );

		return 
			CreateStaticPathForSideAttack( 
				pUnit, pTarget, vAttackDir, 
				fRangeMin, fRangeMax, fMinDist, ( pSoldier->GetMaxAngle() - pSoldier->GetMinAngle() ) / 2 );
	}
}
IStaticPath* CreateStaticPathForSideAttack( IBasePathUnit *pUnit, CAIUnit *pTarget, const CVec2 &attackDir, const float fRangeMin, const float fRangeMax, const float fDistToPoint, const WORD wHalfAngle )
{
	const WORD wAttackDir = GetDirectionByVector( attackDir );
	
	CPtr<IPointChecking> pPointChecking = new CAttackSideChecking( fRangeMin, fRangeMax, pTarget->GetTile(), wAttackDir, wHalfAngle );
	IStaticPath *pPath = pUnit->CreateBigStaticPath( pUnit->GetCenter(), pTarget->GetCenter() + attackDir * fDistToPoint, pPointChecking );

	if ( pPath != 0 )
	{
		CVec2 finishDir = pPath->GetFinishPoint() - pTarget->GetCenter();
		const WORD wFinishDir = GetDirectionByVector( finishDir );
		if ( DirsDifference( wFinishDir, wAttackDir ) > wHalfAngle || fabs2( pPath->GetFinishPoint() - pTarget->GetCenter() ) > sqr( fRangeMax ) )
		{
			CPtr<IStaticPath> pGarbage = pPath;			
			return 0;
		}
	}
	
	if ( pPath && !pUnit->CanGoToPoint( pPath->GetFinishPoint() ) )
	{
		CPtr<IStaticPath> pGarbage = pPath;		
		return 0;
	}
	else
		return pPath;
}
IStaticPath* CreatePathWithChecking( IBasePathUnit *pUnit, const SVector &vTargetTile, IPointChecking *pPointChecking )
{
	CPtr<IPointChecking> pGarbage = pPointChecking;

	IStaticPath *pPath = pUnit->CreateBigStaticPath( pUnit->GetCenter(), AICellsTiles::GetPointByTile( vTargetTile ), pPointChecking );
	if ( pPath != 0 && ( !pUnit->CanGoToPoint( pPath->GetFinishPoint() ) || !pPointChecking->IsGoodTile( pPath->GetFinishTile() ) ) )
	{
		CPtr<IStaticPath> pGarbage = pPath;		
		return 0;
	}

	return pPath;
}
IStaticPath* CreateStaticPathForStObjAttack( IBasePathUnit *pUnit, CStaticObject *pObj, const float fRangeMin, const float fRangeMax )
{
	const CVec2 vUnitCenter( pUnit->GetCenter() );	
	
	CPtr<IPointChecking> pPointChecking = new CAttackStObjectChecking( fRangeMin, fRangeMax, pObj, vUnitCenter );
	IStaticPath *pPath = pUnit->CreateBigStaticPath( vUnitCenter, pObj->GetAttackCenter( vUnitCenter ), pPointChecking );

	if ( pPath != 0 && !pUnit->CanGoToPoint( pPath->GetFinishPoint() ) )
	{
		CPtr<IStaticPath> pGarbage = pPath;
		return 0;
	}

	return pPath;
}
bool CanUnitApproachToUnitByPath( const CAIUnit *Moving, const IStaticPath *Path, const CAIUnit *Standing )
{
	if( !Path ) 
		return false;

	SRect rMoving = Moving->GetUnitRect();
	CVec2 vShift = Path->GetFinishPoint() - Moving->GetCenter();
	SRect rMovingFinal;
	rMovingFinal.InitRect( rMoving.v1+vShift, rMoving.v2+vShift, rMoving.v3+vShift, rMoving.v4+vShift);
	
	SRect rStanding = Standing->GetUnitRect();
	rMovingFinal.Compress( 1.2f );
	rStanding.Compress( 1.2f );
	
	return 
		rMovingFinal.IsIntersected( rStanding );
}
bool CanUnitApproachToPointByPath( const CAIUnit *Moving, const IStaticPath *Path, const CVec2 &point )
{
	if( !Path ) 
		return false;

	SRect rMoving = Moving->GetUnitRect();
	CVec2 vShift = Path->GetFinishPoint() - Moving->GetCenter();
	SRect rMovingFinal;
	rMovingFinal.InitRect( rMoving.v1+vShift, rMoving.v2+vShift, rMoving.v3+vShift, rMoving.v4+vShift);

	rMovingFinal.center = Path->GetFinishPoint();

	rMovingFinal.Compress( 1.2f );
	
	return 
		rMovingFinal.IsPointInside( point );
}
bool CanUnitApproachToObjectByPath( const CAIUnit *Moving, const IStaticPath *Path, const CStaticObject *standing )
{
	if( !Path ) 
		return false;

	SRect rMoving = Moving->GetUnitRect();
	CVec2 vShift = Path->GetFinishPoint() - Moving->GetCenter();
	SRect rMovingFinal;
	rMovingFinal.InitRect( rMoving.v1+vShift, rMoving.v2+vShift, rMoving.v3+vShift, rMoving.v4+vShift);

	SRect rStanding;
	standing->GetBoundRect( &rStanding );

	rMovingFinal.Compress( 1.2f );
	rStanding.Compress( 1.2f );
	
	return 
		rMovingFinal.IsIntersected( rStanding );
}
bool IsUnitNearObject( const CAIUnit *pUnit, const CStaticObject *pObj )
{
	SRect r1 = pUnit->GetUnitRect();
	SRect r2;
	pObj->GetBoundRect( &r2 );

	r1.Compress( 1.2f );
	r2.Compress( 1.2f );
	
	return r1.IsIntersected( r2 );
}
bool IsUnitNearUnit( const CAIUnit *pUnit1, const CAIUnit *pUnit2 )
{
	SRect r1 = pUnit1->GetUnitRect();
	SRect r2 = pUnit2->GetUnitRect();
	
	r1.Compress( 1.2f );
	r2.Compress( 1.2f );
	
	return r1.IsIntersected( r2 );
}
bool IsUnitNearPoint( const CAIUnit *pUnit, const CVec2 &point, const int add )
{
	SRect r1 = pUnit->GetUnitRect();
	r1.InitRect( r1.center, r1.dir, r1.width + add, r1.lengthAhead + add);

	return r1.IsPointInside( point );
}
bool IsPointNearPoint( const CVec2 &point1, const CVec2 &point2 )
{
	return fabs2(point1-point2) < sqr(static_cast<int>(SConsts::TILE_SIZE) );
}
