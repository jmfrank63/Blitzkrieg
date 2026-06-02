#include "stdafx.h"

#include "PathUnit.h"
#include "PathFinder.h"
#include "AIStaticMap.h"
#include "Units.h"
#include "UnitsIterators2.h"
#include "CollisionInternal.h"
#include "PointChecking.h"
#include "Updater.h"
#include "AIUnit.h"
#include "PlanePath.h"
#include "StandartPath.h"
#include "DifficultyLevel.h"
#include "Trigonometry.h"
#include "StaticObjectsIters.h"
#include "StaticObject.h"
#include "AIClassesID.h"
extern CPtr<IStaticPathFinder> pThePathFinder;
extern CPtr<IStaticPathFinder> pThePlanePathFinder;
extern CPtr<IStaticPathFinder> pTheTrainPathFinder;
extern CStaticMap theStaticMap;
extern CUnits units;
extern CUpdater updater;
extern NTimer::STime curTime;
extern CDifficultyLevel theDifficultyLevel;
BASIC_REGISTER_CLASS( CPathUnit );
void CPathUnit::Init( class CAIUnit *_pOwner, const CVec2 &_center, const int _z, const WORD _dir, const WORD id )
{
	pOwner = _pOwner;
	
	placement.center = _center;
	curTile = AICellsTiles::GetTile( placement.center );
	placement.dir = _dir;
	placement.z = _z;
	placement.pObj = 0;
	pPathMemento = 0;
	pCollMemento = 0;
	vSuspendedPoint.x = vSuspendedPoint.y = -1.0f;
	bIdle = true;

	dirVec = GetVectorByDirection( placement.dir );

	speed = VNULL2;
	lastKnownGoodTile = GetTile();

	bLocking = bTurning = bFoolStop = bFixUnlock = false;
	bRightDir = true;
	nCollisions = 0;
	collStayTime = 0;
	stayTime = 0;
	bOnLockedTiles = false;
	checkOnLockedTime = 0;
	
	if ( pOwner->GetStats()->IsAviation() )
	{
		pPathFinder = pThePlanePathFinder;
		pCurCollision = new CPlaneCollision();
	}
	else if ( pOwner->GetStats()->IsTrain() )
	{
		pPathFinder = pTheTrainPathFinder;
		pCurCollision = new CFreeOfCollisions( this, 0 );
	}
	else
	{
		pPathFinder = pThePathFinder;
		pCurCollision = new CFreeOfCollisions( this, 0 );
	}

	bTurnCalled = false;
	wDirAtBeginning = GetDir();
	desDir = 0;
	
	nextSecondPathSegmTime = 0;
}
void CPathUnit::UpdateDirection( const WORD newDir )
{
	const WORD wOldDir = placement.dir;
	if ( bRightDir )
		placement.dir = newDir;
	else
		placement.dir = newDir + 32768;

	dirVec = GetVectorByDirection( GetDir() );
	speed = dirVec * fabs( speed );

	if ( wOldDir != placement.dir )
	{
		UnlockTiles( true );
		
		updater.Update( ACTION_NOTIFY_PLACEMENT, pOwner );
		pOwner->WarFogChanged();

		LockTiles();
	}
}
void CPathUnit::UpdateDirectionForEditor( const CVec2 &dirVec ) 
{ 
	UpdateDirection( dirVec ); 
	ForceLockingTiles();
}
void CPathUnit::SetFrontDirWOUpdate( const WORD newDir )
{
	placement.dir = newDir;
}
void CPathUnit::UpdateDirection( const CVec2 &dirVec )
{
	NI_ASSERT_T( dirVec != VNULL2, "Wrong direction" );
	UpdateDirection( GetDirectionByVector( dirVec ) );
}
void CPathUnit::FirstSegment()
{
	if ( pOwner->CanMovePathfinding() && !IsIdle() )
	{
		pCurCollision->Segment();

		if ( pCurCollision->IsSolved() && pCollMemento != 0 )
		{
			UnlockTiles( true );
			
			if ( vSuspendedPoint.x != -1.0f )
			{
				CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vSuspendedPoint, VNULL2, GetOwner() );
				if ( pPath.IsValid() )
					SendAlongPath( pPath, VNULL2, false );
				else
					StopUnit();

				vSuspendedPoint.x = vSuspendedPoint.y = -1.0f;
			}
			else
			{
				pCurCollision = pCollMemento;
				GetSmoothPath()->Init( pPathMemento, pOwner );
			}

			pLastPushByHardCollUnit = 0;
			pPathMemento = 0;
			pCollMemento = 0;
		}

		if ( !pCurCollision->IsSolved() )
			pCurCollision->FindCandidates();

		if  ( pOwner->IsMoving() && pCurCollision->IsSolved() )
		{
			if ( vSuspendedPoint.x != -1.0f )
			{
				CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vSuspendedPoint, VNULL2, GetOwner() );
				if ( pPath.IsValid() )
					SendAlongPath( pPath, VNULL2, false );
				else
					StopUnit();

				vSuspendedPoint.x = vSuspendedPoint.y = -1.0f;
			}
			else
				StopUnit();
		}
	}
}
void CPathUnit::CheckForDestroyedObjects( const CVec2 &center ) const
{
	const CVec2 vCenter = GetCenter();
	const float fRadius = GetStats()->vAABBHalfSize.y;
	const SRect unitRect = GetUnitRect();
	const EAIClass eClass = EAIClass( GetAIClass() );
	
	for ( CStObjCircleIter<false> stObjsIter( vCenter, fRadius ); !stObjsIter.IsFinished(); stObjsIter.Iterate() )
	{
		CExistingObject *pObj = *stObjsIter;
		const EStaticObjType eType = pObj->GetObjectType();
		if ( pObj->IsAlive() && 
				 eType != ESOT_BRIDGE_SPAN && eType != ESOT_MINE && eType != ESOT_ENTR_PART && eType != ESOT_FLAG )
		{
			if ( pObj->CanUnitGoThrough( eClass ) )
			{
				SRect objRect;
				pObj->GetBoundRect( &objRect );
				if ( unitRect.IsIntersected( objRect ) )
				{
					updater.Update( ACTION_NOTIFY_SILENT_DEATH, pObj );
					pObj->Delete();
				}
			}
		}
	}
}
void CPathUnit::SecondSegment( const bool bUpdate )
{
	if ( pOwner->CanMovePathfinding() && ( !IsIdle() || stayTime == 0 ) )
	{
		bool bPathIsFinished = GetSmoothPath()->IsFinished();
		NTimer::STime timeDiff = SConsts::AI_SEGMENT_DURATION * pOwner->GetPathSegmentsPeriod();
		const WORD oldDir = GetFrontDir();

		const CVec2 center = GetCenter();
		const SVector intCenter( center );

		const CVec3 newCenter = GetSmoothPath()->GetPoint( timeDiff );
		const CVec2 newCenter2D( newCenter.x, newCenter.y );
		const SVector intNewCenter( newCenter2D );
		const SVector newTile( AICellsTiles::GetTile( intNewCenter ) );

		speed = GetSmoothPath()->GetSpeedLen() * GetDirVector();

		if ( center != newCenter2D || GetFrontDir() != oldDir )
			stayTime = 0;
		else
			stayTime += timeDiff;

		const bool bChangedIntPos = intCenter != intNewCenter;
		if ( bChangedIntPos )
		{
			theStaticMap.MemMode();
			theStaticMap.SetMode( ELM_STATIC );

			if ( theStaticMap.CanUnitGo( GetBoundTileRadius(), newTile, GetAIClass() ) )
				lastKnownGoodTile = newTile;
			theStaticMap.RestoreMode();

			if ( !GetStats()->IsInfantry() && GetZ() == 0 )
				CheckForDestroyedObjects( newCenter2D );
		}

		if ( center != newCenter2D || GetZ() != newCenter.z )
		{
			const bool bChangedVisiblePosition = bChangedIntPos || GetZ() != newCenter.z;

			units.UnitChangedPosition( static_cast<CAIUnit*>(pOwner), newCenter2D );
			placement.center = newCenter2D;
			placement.z = newCenter.z;
			curTile = newTile;

			if ( bChangedVisiblePosition )
			{
				updater.Update( ACTION_NOTIFY_PLACEMENT, pOwner );
				pOwner->WarFogChanged();
			}
		}
		
		pOwner->UnsetDesirableSpeed();

		if ( curTime - checkOnLockedTime >= 3000 + Random( 0, SAIConsts::AI_SEGMENT_DURATION * 5 ) && 
				 !pOwner->GetStats()->IsInfantry() )
		{
			bOnLockedTiles = pOwner->IsOnLockedTiles( GetUnitRect() );
			checkOnLockedTime = curTime;

			if ( !GetSmoothPath()->CanGoBackward() && !bRightDir && !bOnLockedTiles )
				bRightDir = true;
		}

		if ( !bPathIsFinished && GetSmoothPath()->IsFinished() )
			pOwner->NullSegmTime();

		nextSecondPathSegmTime = 0;
	}
	else
	{
		speed = VNULL2;
		LockTiles();

		nextSecondPathSegmTime = curTime + Random( 250, 550 );
	}

	if ( bTurning && !bTurnCalled )
		bTurning = false;

	bTurnCalled = false;

	CalculateIdle();
	if ( !IsIdle() || GetDirAtTheBeginning() != GetDir() )
		nextSecondPathSegmTime = 0;
}
void CPathUnit::LockTiles( bool bUpdate )
{
	if ( CanLockTiles() && !updater.IsPlacementUpdated( pOwner ) /*&& IsIdle() */&& !bFixUnlock )
	{
		theStaticMap.AddLockedUnitTiles( GetUnitRectForLock(), pOwner->GetID(), true, bUpdate );
		bLocking = true;
	}	
}
void CPathUnit::ForceLockingTiles( bool bUpdate )
{
	if ( CanLockTiles( true ) )
	{
		theStaticMap.AddLockedUnitTiles( GetUnitRectForLock(), pOwner->GetID(), true, bUpdate );
		bLocking = true;
	}	
}
void CPathUnit::LockTilesForEditor()
{
	if ( CanLockTiles()  )
	{
		theStaticMap.AddLockedUnitTiles( GetUnitRectForLock(), pOwner->GetID(), true );
		bLocking = true;
	}	
}
void CPathUnit::UnlockTiles( const bool bUpdate )
{
	if ( CanUnlockTiles() )
	{
		theStaticMap.RemoveLockedUnitTiles( pOwner->GetID(), bUpdate );
		bLocking = false;
	}
}
bool CPathUnit::SendAlongPath( IPath *pPath )
{
	bool bResult = false;
	
	CPtr<IPath> pUnitPath = pPath;
	if ( pOwner->CanMovePathfinding() )
	{
		pPathMemento = 0;
		pCollMemento = 0;
		stayTime = 0;
		UnlockTiles( true );

		if ( !pOwner->GetStats()->IsAviation() )
		{
			bResult = GetSmoothPath()->Init( pOwner, pPath, !pOwner->GetStats()->IsInfantry() );
			pCurCollision = new CFreeOfCollisions( this, 0 );
		}
		else
		{
			GetSmoothPath()->Init( pOwner, pPath, !pOwner->GetStats()->IsInfantry() );
			bResult = true;
		}
	}

	CalculateIdle();
	return bResult;
}
bool CPathUnit::SendAlongPath( IStaticPath *pStaticPath, const CVec2 &vShift, bool bSmoothTurn )
{
	bool bResult = true;
	CPtr<IStaticPath> pUnitPath = pStaticPath;
	if ( pOwner->CanMovePathfinding() )
	{
		pPathMemento = 0;
		pCollMemento = 0;
		stayTime = 0;
		UnlockTiles( true );

		if ( !pOwner->GetStats()->IsAviation() )
		{
			if ( pOwner->IsInFormation() && !pStaticPath )
				bResult = GetSmoothPath()->InitByFormationPath( pOwner->GetFormation(), pOwner );
			else
			{				
				if ( theStaticMap.IsPointInside( pStaticPath->GetFinishPoint() + vShift ) )
					bResult = GetSmoothPath()->Init( pOwner, new CStandartPath( pOwner->GetBoundTileRadius(), GetAIClass(), pPathFinder, pStaticPath, placement.center, pStaticPath->GetFinishPoint() + vShift, GetLastKnownGoodTile() ), !pOwner->GetStats()->IsInfantry() && bSmoothTurn );
				else
					bResult = GetSmoothPath()->Init( pOwner, new CStandartPath( pOwner->GetBoundTileRadius(), GetAIClass(), pPathFinder, pStaticPath, placement.center, pStaticPath->GetFinishPoint(), GetLastKnownGoodTile() ), !pOwner->GetStats()->IsInfantry() && bSmoothTurn );
			}

			pCurCollision = new CFreeOfCollisions( this, 0 );
		}
		else
		{
			CVec3 from( placement.center.x,placement.center.y,placement.z );

			if ( theStaticMap.IsPointInside( pStaticPath->GetFinishPoint() + vShift ) )
			{
				CVec2 to( pStaticPath->GetFinishPoint() + vShift );
				GetSmoothPath()->Init( pOwner, new CPlanePath( from, CVec3(to.x,to.y,GetZ()) ), !pOwner->GetStats()->IsInfantry() && bSmoothTurn );
			}
			else
			{
				CVec2 to( pStaticPath->GetFinishPoint() );
				GetSmoothPath()->Init( pOwner, new CPlanePath( from, CVec3(to.x,to.y,GetZ()) ), !pOwner->GetStats()->IsInfantry() && bSmoothTurn );
			}
		}
	}
	else
		bResult = false;

	CalculateIdle();

	return bResult;
}
bool CPathUnit::CanGoByDir( const CVec2 &dir, const SRect &forceRect, SRect bannedRect, CVec2 forceSpeed, int *numIntersect, float *distance, int *nBadness )
{
	SRect unitRect( GetUnitRect() );
	SRect realUnitRect( unitRect );

	const float fTime = ( ( SConsts::TILE_SIZE / 2 ) / pOwner->GetStats()->fSpeed ) * 1.1f;
	*nBadness = 0;
	int bBad = 0;
	
	while ( unitRect.IsIntersected( bannedRect ) )
	{
		unitRect.center += SConsts::TILE_SIZE / 2 * dir;
		unitRect.InitRect( unitRect.center, dir, realUnitRect.lengthAhead, realUnitRect.width );
		bannedRect.InitRect( bannedRect.center + forceSpeed * fTime / 2, bannedRect.dir, bannedRect.lengthAhead, bannedRect.lengthBack, bannedRect.width );
		if ( bBad == 2 )
			++(*nBadness);
		else
			++bBad;
	}

	
	while ( unitRect.IsIntersected( forceRect ) && !unitRect.IsIntersected( bannedRect ) )
	{
		unitRect.center += SConsts::TILE_SIZE / 2 * dir;
		unitRect.InitRect( unitRect.center, dir, realUnitRect.lengthAhead, realUnitRect.width );
		bannedRect.InitRect( bannedRect.center + forceSpeed * fTime, bannedRect.dir, bannedRect.lengthAhead, bannedRect.lengthBack, bannedRect.width );
		forceSpeed /= 1.1f;
	}

	if ( unitRect.IsIntersected( bannedRect ) )
		return false;
	else 
	{
		*distance = fabs( unitRect.center - GetCenter() );				
		SRect resultRect;
		resultRect.InitRect( GetCenter(), dir, *distance + realUnitRect.lengthAhead, realUnitRect.lengthBack, realUnitRect.width );

		if ( IsRectOnLockedTiles( resultRect, GetAIClass() ) )
			return false;

		*numIntersect = 0;

		return true;
	}
}
void CPathUnit::SetCollision( ICollision *pCollision )
{
	IPath *pNewPath = pCollision->GetPath();

	if ( pCollMemento == 0 )
	{
		pCollMemento = pCurCollision;
		pPathMemento = GetSmoothPath()->GetMemento();
	}
	
	if ( pNewPath == 0 )
		GetCurPath()->Stop();
	else
		GetSmoothPath()->Init( pOwner, pNewPath, true );

	pCurCollision = pCollision;

	CalculateIdle();
}
void CPathUnit::StopTurning()
{
	bTurning = false;
}
void CPathUnit::StopUnit()
{
	pPathMemento = 0;
	pCollMemento = 0;
	pLastPushByHardCollUnit = 0;

	if ( ISmoothPath *pSmoothPath = GetSmoothPath() )
		pSmoothPath->Init( pOwner, pPathFinder->CreatePathByDirection( GetCenter(), CVec2( 1, 1 ), GetCenter(), pOwner->GetBoundTileRadius() ), !pOwner->GetStats()->IsInfantry() );
	pCurCollision = new CFreeOfCollisions( this, 0 );

	speed = VNULL2;
	StopTurning();
	if ( GetOwner()->GetStats()->IsInfantry() )
		UnlockTiles( true );
	vSuspendedPoint.x = vSuspendedPoint.y = -1.0f;

	CalculateIdle();
}
void CPathUnit::CalculateIdle()
{
	bIdle = ( pCollMemento == 0 && GetCollision()->IsSolved() && vSuspendedPoint.x == -1.0f );
}
bool CPathUnit::IsIdle() const
{
	return bIdle;
}
const CVec2 CPathUnit::GetCenterShift() const
{
	const CVec2 realDirVec( GetFrontDirVec() );
	const CVec2 dirPerp( realDirVec.y, -realDirVec.x );

	return CVec2( realDirVec * pOwner->GetStats()->vAABBCenter.y + dirPerp * pOwner->GetStats()->vAABBCenter.x );
}
const SRect CPathUnit::GetUnitRect() const
{
	const float length = pOwner->GetStats()->vAABBHalfSize.y * SConsts::BOUND_RECT_FACTOR;
	const float width = pOwner->GetStats()->vAABBHalfSize.x * SConsts::BOUND_RECT_FACTOR;

	SRect unitRect;
	unitRect.InitRect( GetCenter() + GetCenterShift(), GetFrontDirVec(), length, width );

	return unitRect;
}
const SRect CPathUnit::GetSmallRect() const
{
	const float fCoeff = GetStats()->IsInfantry() ? 0.5 : 0.8;
	const float fLength = pOwner->GetStats()->vAABBHalfSize.y * fCoeff;
	const float fWidth = pOwner->GetStats()->vAABBHalfSize.x * fCoeff;

	SRect smallRect;
	smallRect.InitRect( GetCenter() + GetCenterShift(), GetFrontDirVec(), fLength, fWidth );

	return smallRect;
}
const SRect CPathUnit::GetUnitRectForLock() const
{
	const float length = pOwner->GetStats()->vAABBHalfSize.y * SConsts::COEFF_FOR_LOCK;
	const float width = pOwner->GetStats()->vAABBHalfSize.x * SConsts::COEFF_FOR_LOCK;

	SRect unitRect;
	unitRect.InitRect( GetCenter() + GetCenterShift(), GetFrontDirVec(), length, width );

	return unitRect;
}
const SRect CPathUnit::GetFullSpeedRect( bool bForInfantry ) const
{
	const float width = pOwner->GetStats()->vAABBHalfSize.x * SConsts::BOUND_RECT_FACTOR;// * 1.3f;
	const float lengthBack = pOwner->GetStats()->vAABBHalfSize.y * SConsts::BOUND_RECT_FACTOR;
	float lengthAhead;
	if ( !IsIdle() )
	{
		if ( bForInfantry )
			lengthAhead = 
				Max( pOwner->GetStats()->vAABBHalfSize.y * SConsts::BOUND_RECT_FACTOR + SConsts::SPEED_FACTOR * GetMaxPossibleSpeed(),
						 pOwner->GetStats()->vAABBHalfSize.y * SConsts::BOUND_RECT_FACTOR * 1.5f );
		else
			lengthAhead = 
				Max( pOwner->GetStats()->vAABBHalfSize.y * SConsts::BOUND_RECT_FACTOR + SConsts::SPEED_FACTOR * fabs( GetSpeed() ),
						 pOwner->GetStats()->vAABBHalfSize.y * SConsts::BOUND_RECT_FACTOR * 1.5f );
	}
	else
		lengthAhead = GetStats()->vAABBHalfSize.y * SConsts::BOUND_RECT_FACTOR;

	SRect speedRect;
	speedRect.InitRect( GetCenter() + GetCenterShift(), GetDirVector(), lengthAhead, lengthBack, width );

	return speedRect;
}
const SRect CPathUnit::GetSpeedRect( bool bForInfantry ) const
{
	const float width = pOwner->GetStats()->vAABBHalfSize.x * SConsts::BOUND_RECT_FACTOR;// * 1.3f;
	const float lengthBack = pOwner->GetStats()->vAABBHalfSize.y * SConsts::BOUND_RECT_FACTOR;
	float lengthAhead;
	if ( !IsIdle() )
	{
		if ( bForInfantry )
			lengthAhead = Max( SConsts::SPEED_FACTOR * GetMaxPossibleSpeed(), pOwner->GetStats()->vAABBHalfSize.y * 0.5f );
		else
			lengthAhead = Max( SConsts::SPEED_FACTOR * fabs( GetSpeed() ), pOwner->GetStats()->vAABBHalfSize.y * 0.5f );
	}
	else
		lengthAhead = 0;

	SRect speedRect;
	speedRect.InitRect( GetCenter()  + GetCenterShift() + 2 * GetDirVector() * pOwner->GetStats()->vAABBHalfSize.y, 
											GetDirVector(), lengthAhead, lengthBack, width );
	
	return speedRect;
}
bool CPathUnit::CanSetNewCoord( const CVec3 &newCenter )
{
	const SAINotifyPlacement oldPlacement( placement );
	placement.center.x = newCenter.x;
	placement.center.y = newCenter.y;
	placement.z = 0;
	curTile = AICellsTiles::GetTile( placement.center );

	SRect unitRect = GetUnitRectForLock();
	const bool bResult = 
								theStaticMap.IsPointInside( unitRect.v1 ) &&
								theStaticMap.IsPointInside( unitRect.v2 ) &&
								theStaticMap.IsPointInside( unitRect.v3 ) &&
								theStaticMap.IsPointInside( unitRect.v4 );

	placement = oldPlacement;
	curTile = AICellsTiles::GetTile( placement.center );

	return bResult;
}
bool CPathUnit::CanSetNewDir( const CVec2 &newDir )
{
	const CVec2 oldDir = dirVec;
	dirVec = newDir;
	
	SRect unitRect = GetUnitRectForLock();
	const bool bResult = 
								theStaticMap.IsPointInside( unitRect.v1 ) &&
								theStaticMap.IsPointInside( unitRect.v2 ) &&
								theStaticMap.IsPointInside( unitRect.v3 ) &&
								theStaticMap.IsPointInside( unitRect.v4 );

	dirVec = oldDir;

	return bResult;
}
void CPathUnit::SetNewCoordinates( const CVec3 &newCenter, bool bStopUnit )
{
	if ( bStopUnit )
		StopUnit();
	units.UnitChangedPosition( static_cast<CAIUnit*>(pOwner), CVec2( newCenter.x, newCenter.y ) );

	if ( IsLockingTiles() )
		theStaticMap.RemoveLockedUnitTiles( pOwner->GetID(), true );
	bLocking = false;

	placement.center.x = newCenter.x;
	placement.center.y = newCenter.y;
	placement.z = newCenter.z;
	curTile = AICellsTiles::GetTile( placement.center );
	LockTiles();

	updater.Update( ACTION_NOTIFY_PLACEMENT, pOwner );
	pOwner->WarFogChanged();
}
void CPathUnit::SetCoordWOUpdate( const CVec3 &newCenter )
{
	placement.center.x = newCenter.x;
	placement.center.y = newCenter.y;
	placement.z = newCenter.z;
	curTile = AICellsTiles::GetTile( placement.center );
	
	updater.Update( ACTION_NOTIFY_PLACEMENT, pOwner );
}
bool CPathUnit::CanLockTiles( bool bForceLocking  ) const
{
	return !bLocking && ( bForceLocking || !pOwner->GetStats()->IsInfantry() ) && !pOwner->GetStats()->IsAviation();
}
bool CPathUnit::CanUnlockTiles() const
{
	return bLocking;// && !pOwner->GetStats()->IsInfantry();
}
const CVec2& CPathUnit::GetDirVector() const 
{
	return dirVec; 
}
const WORD CPathUnit::GetDir() const
{ 
	if ( bRightDir )
		return placement.dir;
	else
		return placement.dir + 32768;
}
void CPathUnit::SetRightDir( bool _bRightDir )
{
	if ( bRightDir != _bRightDir )
	{
		bRightDir =_bRightDir;
		dirVec = -dirVec;
	}
}
void CPathUnit::ChooseDirToTurn( const WORD &newDir )
{
	if ( GetSmoothPath()->CanGoBackward() || ( pOwner->CanGoBackward() && bOnLockedTiles && !bTurning ) )
	{
		const WORD rightDir = DirsDifference( placement.dir, newDir );
		const WORD backDir =  DirsDifference( placement.dir + 32768, newDir );

		bRightDir = rightDir <= backDir;
	}
	else
		bRightDir = true;
}
bool CPathUnit::MakeTurnToDir( const WORD newDir )
{
	const WORD realNewDir = newDir + !bRightDir * 32768;
	
	const WORD clockWise = realNewDir - placement.dir;
	const WORD counterClockWise = placement.dir - realNewDir;

	const WORD bestDir = Min( clockWise, counterClockWise );

	if ( bestDir < SConsts::TURN_TOLERANCE )
	{
		UpdateDirection( newDir );
		bTurning = false;
		return true;
	}
	else
	{
		const NTimer::STime turnTime = bestDir / pOwner->GetRotateSpeed();

		if ( turnTime < SConsts::AI_SEGMENT_DURATION )
		{
			UpdateDirection( newDir );
			bTurning = false;
			return true;
		}
		else
		{
			const float dirSign = ( clockWise < counterClockWise ) ? ( 1 ) : ( -1 );
			UpdateDirection( placement.dir + (int)( dirSign * float(SConsts::AI_SEGMENT_DURATION) * pOwner->GetRotateSpeed() ) + !bRightDir * 32768 );
			return false;
		}
	}
}
bool CPathUnit::TurnToDir( const WORD &newDir, const bool bCanBackward, const bool bCanForward )
{
	bTurnCalled = true;
	if ( pOwner->GetRotateSpeed() == 0 )
	{
		UpdateDirection( newDir );
		return true;
	}
	else
	{
		if ( !bTurning || newDir != desDir )
		{
			if ( bCanBackward && bCanForward )
				ChooseDirToTurn( newDir );
			else
				bRightDir = bCanForward;

			bTurning = true;

			desDir = newDir; 
		}

		return MakeTurnToDir( newDir );
	}
}
IPath* CPathUnit::CreatePathByDirection( const CVec2 &startPoint, const CVec2 &dir, const CVec2 &finishPoint ) 
{ 
	return pPathFinder->CreatePathByDirection( startPoint, dir, finishPoint, pOwner->GetStats()->nBoundTileRadius ); 
}
const float CPathUnit::GetRotateSpeed() const 
{ 
	return 
		pOwner->GetStats()->fRotateSpeed * theDifficultyLevel.GetRotateSpeedCoeff( pOwner->GetParty() ) * pOwner->GetExpLevel().fBonusRotate;
}
const float CPathUnit::GetMaxPossibleSpeed() const 
{ 
	return pOwner->GetExpLevel().fBonusSpeed * pOwner->GetStats()->fSpeed;
}
const int CPathUnit::GetBoundTileRadius() const 
{ 
	return pOwner->GetStats()->nBoundTileRadius; 
}
const CVec2 CPathUnit::GetAABBHalfSize() const 
{ 
	return pOwner->GetStats()->vAABBHalfSize; 
}
const SUnitBaseRPGStats* CPathUnit::GetStats() const
{
	return pOwner->GetStats();
}
bool CPathUnit::CanTurnToFrontDir( const WORD wDir )
{
	return CanRotateTo( GetUnitRect(), GetVectorByDirection( wDir ), true, false );
}
void CPathUnit::GetSpeed3( CVec3 *pSpeed ) const 
{ 
	GetSmoothPath()->GetSpeed3( pSpeed );
	if ( VNULL3 == *pSpeed )
		*pSpeed = CVec3( speed, 0.0f ); 
}
void CPathUnit::GetPlacement( SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff ) const 
{ 
	CVec3 pos( placement.center.x, placement.center.y, placement.z );
	CVec3 vSpeed3;
	GetSpeed3( &vSpeed3 );
	pos -= timeDiff * vSpeed3;
	
	*pPlacement = placement;
	pPlacement->center.x = pos.x;
	pPlacement->center.y = pos.y;
	pPlacement->z = pos.z;
	
	pPlacement->fSpeed = fabs(vSpeed3);
}
BYTE CPathUnit::GetAIClass() const
{
	return GetStats()->aiClass;
}
bool CPathUnit::IsStopped() const 
{ 
	return speed == VNULL2 && GetSmoothPath()->IsFinished(); 
}
const float CPathUnit::GetSpeedLen() const 
{ 
	return GetSmoothPath()->GetSpeedLen(); 
}
int CPathUnit::GetID() const
{
	return pOwner->GetID();
}
void CPathUnit::UpdateCollStayTime( const NTimer::STime candStayTime ) 
{ 
	collStayTime = Max( collStayTime, Min( stayTime, candStayTime ) ); 
}
bool CPathUnit::CanRotateTo( SRect smallRect, const CVec2 &vNewDir, bool bWithUnits, bool bCanGoBackward )
{
	if ( pOwner->GetStats()->IsInfantry() || pOwner->GetStats()->IsAviation() )
		return true;
	if ( GetDir() != GetDirectionByVector( vNewDir ) && !CanRotate() )
		return false;

	CTemporaryUnitRectUnlocker unlocker( GetID() );
	
	const WORD wCurDir = GetDirectionByVector( smallRect.dir );
	WORD wFinalDir = GetDirectionByVector( vNewDir );
	const WORD wFinalDirBack = wFinalDir + 32768;

	if ( bCanGoBackward && DirsDifference( wCurDir, wFinalDirBack ) < DirsDifference( wCurDir, wFinalDir ) )
		wFinalDir = wFinalDirBack;

	const WORD clockWise = wFinalDir - wCurDir;
	const WORD counterClockWise = wCurDir - wFinalDir;
	const WORD bestDir = Min( clockWise, counterClockWise );

	const int dirSign = ( clockWise < counterClockWise ) ? ( 1 ) : ( -1 );

	const int nParts = 4;
	const int nAdd = dirSign * int(bestDir / nParts );
	std::vector<SRect> mediateRects( nParts );
	for ( int i = 1; i <= nParts; ++i )
	{
		const WORD wMediateDir = wCurDir + i*nAdd;// dirSign * int(bestDir / 2);
		mediateRects[i-1].InitRect( smallRect.center, GetVectorByDirection( wMediateDir ), smallRect.lengthAhead, smallRect.lengthBack, smallRect.width );
	}

	for ( int i = 0; i < nParts; ++i )
	{
		if ( pOwner->IsOnLockedTiles( mediateRects[i] ) )
			return false;
	}

	if ( bWithUnits )
	{
		for ( CUnitsIter<0,0> iter( 0, ANY_PARTY, smallRect.center, smallRect.lengthAhead ); !iter.IsFinished(); iter.Iterate() )
		{
			CAIUnit *pCandUnit = *iter;
			if ( pCandUnit->GetPathUnit() != this && !pCandUnit->GetStats()->IsAviation() )
			{
				const bool bCanPush = 
					pCandUnit->GetStats()->IsInfantry() && 
					pCandUnit->GetPathUnit()->GetCollision()->GetPriority() >= GetCollision()->GetPriority();

				bool bMediateRectsInterseted = false;
				for ( int i = 0; i < nParts && !bMediateRectsInterseted; ++i )
					bMediateRectsInterseted = bMediateRectsInterseted || mediateRects[i].IsIntersected( pCandUnit->GetUnitRect() );

				if ( !bCanPush && bMediateRectsInterseted )
					return false;
			}
		}
	}

	return true;
}
bool CPathUnit::CanMake180DegreesTurn( SRect rect )
{
	CTemporaryUnitRectUnlocker unlocker( GetID() );
	
	const CVec2 vRotateAngle( 1.0f/FP_SQRT_2, 1.0f/FP_SQRT_2 );
	for ( int i = 0; i < 4; ++i )
	{
		rect.InitRect( rect.center, rect.dir ^ vRotateAngle, rect.lengthAhead, rect.lengthBack, rect.width );
		
		if ( pOwner->IsOnLockedTiles( rect ) )
			return false;
	}

	return true;
}
bool CPathUnit::CheckToTurn( const WORD wNewDir )
{
	const SRect rect( GetUnitRectForLock() );
	const CVec2 vUnitCenter( GetCenter() );
	const WORD wUnitDir( GetFrontDir() );
	CVec2 vBestPoint( -1.0f, -1.0f );

	bool bResult = true;
	{
		CTemporaryUnitRectUnlocker unlocker( GetID() );

		if ( !CanRotateTo( rect, GetVectorByDirection( wNewDir ), true, false ) )
		{
			float fBestTime = 1000000;

			const float fMinAngle = 22.5f;
			const CVec2 vMinAngle( NTrg::Cos( fMinAngle / 180.0f * FP_PI ), NTrg::Sin( fMinAngle / 180.0f * FP_PI ) );
			CVec2 vCurDir( GetVectorByDirection( GetFrontDir() ) );
			
			for ( int i = 0; i < 360 / fMinAngle; ++i )
			{
				const WORD wCurDir( GetDirectionByVector( vCurDir ) );
				const WORD wRightDirDiff = DirsDifference( wUnitDir, wCurDir );
				const WORD wBackDirDiff = DirsDifference( wUnitDir + 32768, wCurDir );
				const WORD wDirsDiff = Min( wRightDirDiff, wBackDirDiff );
				if ( ( pOwner->CanGoBackward() || wDirsDiff == wRightDirDiff ) && CanRotateTo( rect, vCurDir, false ) )
				{
					CVec2 vPoint( vUnitCenter + vCurDir * SConsts::TILE_SIZE );
					SRect alongPathRect( rect );
					alongPathRect.InitRect( vPoint, vCurDir, rect.lengthAhead, rect.lengthBack, rect.width );

					bool bOnLockedTiles = false;
					bool bCanTurn = false;
					do
					{
						bOnLockedTiles = pOwner->IsOnLockedTiles( alongPathRect );
						bCanTurn = CanMake180DegreesTurn( alongPathRect );

						if ( !bOnLockedTiles && !bCanTurn )
						{
							vPoint += vCurDir * SConsts::TILE_SIZE;
							alongPathRect.InitRect( vPoint, vCurDir, rect.lengthAhead, rect.lengthBack, rect.width );
						}
					}
					while ( !bOnLockedTiles && !bCanTurn );

					if ( !bOnLockedTiles && bCanTurn )
					{
						const float fTime = 
							fabs( vPoint - vUnitCenter ) * GetMaxPossibleSpeed() + wDirsDiff * GetRotateSpeed();

						if ( fTime < fBestTime )
						{
							fBestTime = fTime;
							vBestPoint = vPoint;
						}
					}
				}

				vCurDir = vCurDir ^ vMinAngle;
			}

			bResult = false;
		}
		else
			bResult = true;
	}

	if ( !bResult )
	{
		if ( vBestPoint.x != -1.0f )
		{
			SetSuspendedPoint( GetSmoothPath()->GetFinishPoint() );
			const CVec2 vDir( vBestPoint - vUnitCenter );
			SendAlongPath( GetPathFinder()->CreatePathByDirection( vUnitCenter, vDir, vBestPoint, GetBoundTileRadius() ) );
		}
	}

	return bResult;
}
IStaticPath* CPathUnit::CreateBigStaticPath( const CVec2 &vStartPoint, const CVec2 &vFinishPoint, IPointChecking *pPointChecking )
{
	UnlockTiles( true );
	theStaticMap.MemMode();
	theStaticMap.SetMode( ELM_STATIC );

	pPathFinder->SetPathParameters( GetBoundTileRadius(), GetAIClass(), pPointChecking, vStartPoint, vFinishPoint, SConsts::INFINITY_PATH_LIMIT, true, GetLastKnownGoodTile() );
	pPathFinder->CalculatePathWOCycles();
	pPathFinder->SmoothPath();

	theStaticMap.RestoreMode();
	LockTiles();

	if ( pPathFinder->GetPathLength() == -1 )
		return 0;
	else if ( pPointChecking != 0 )
		return new CCommonStaticPath( *pPathFinder, CVec2( -1.0f, -1.0f ) );
	else
		return new CCommonStaticPath( *pPathFinder, vFinishPoint );
}
bool CPathUnit::IsInOneTrain( IBasePathUnit *pUnit ) const
{
	return false;
}
const SVector CPathUnit::GetLastKnownGoodTile() const
{
	return lastKnownGoodTile;
}
bool CPathUnit::CanRotate() const
{ 
	return
		pOwner->GetStats()->IsInfantry() || pOwner->GetStats()->fRotateSpeed != 0;
}
ISmoothPath* CSimplePathUnit::GetSmoothPath() const
{
	return pSmoothPath;
}
void CSimplePathUnit::InitAviationPath( const SMechUnitRPGStats* pStats )
{
	float fRatio = 1.0f;
	switch ( pStats->type )
	{
		case RPG_TYPE_AVIA_ATTACK:
			fRatio = SConsts::GUNPLANES_VERT_MANEUR_RATIO;
			break;
		case RPG_TYPE_AVIA_BOMBER:
			fRatio = SConsts::DIVEBOMBER_VERT_MANEUR_RATIO;
			break;
	}

	const float fMultiply = Random( 0.9f, 1.0f );
	pSmoothPath = new CPlaneSmoothPath( pStats->fTurnRadius * fMultiply, pStats->fTurnRadius * 2 * fMultiply,	pStats->fSpeed, fRatio );
}
void CSimplePathUnit::Init( class CAIUnit *pOwner, const CVec2 &center, const int z, const WORD dir, const WORD id )
{
	CPathUnit::Init( pOwner, center, z, dir, id );

	const SMechUnitRPGStats* pStats = static_cast<const SMechUnitRPGStats*>( GetOwner()->GetStats() );
	if ( pStats->IsAviation() )
		InitAviationPath( pStats );
	else
	{
		if ( GetStats()->IsInfantry() )
			pSmoothPath = CreateObject<ISmoothPath>( AI_STANDART_SMOOTH_SOLDIER_PATH );
		else
			pSmoothPath = CreateObject<ISmoothPath>( AI_STANDART_SMOOTH_MECH_PATH );
		
		pSmoothPath->Init( GetOwner(),
											 GetPathFinder()->CreatePathByDirection( GetCenter(), CVec2( 1, 1 ), 
											 GetCenter(),
											 GetOwner()->GetBoundTileRadius() ), 
											 !GetOwner()->GetStats()->IsInfantry() );
	}

	pSmoothPath->SetOwner( GetOwner() );
}
void CSimplePathUnit::SetCurPath( interface ISmoothPath *pNewPath )
{ 
	if ( !pDefaultPath )
		pDefaultPath = pSmoothPath;
	pSmoothPath = pNewPath;
}
void CSimplePathUnit::RestoreDefaultPath()
{ 
	if ( pDefaultPath )
		pSmoothPath = pDefaultPath;
	
	pDefaultPath = 0;
}
