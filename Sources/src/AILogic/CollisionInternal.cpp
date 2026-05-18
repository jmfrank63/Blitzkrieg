#include "stdafx.h"

#include "UnitsIterators2.h"
#include "CollisionInternal.h"
#include "PathUnit.h"
#include "UnitStates.h"
#include "Diplomacy.h"
#include "Soldier.h"
#include "Path.h"
#include "RectTiles.h"
#include "GroupLogic.h"
#include "Statistics.h"
#include "MultiplayerInfo.h"
#include "Trigonometry.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
CCollisionsCollector theColCollector;
extern CGroupLogic theGroupLogic;
extern CStatistics theStatistics;
extern CMultiplayerInfo theMPInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( ICollision );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*														CCollision														*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CCollision::CCollision( CPathUnit *_pUnit, CAIUnit *_pPushUnit, const int _nPriority )
: pUnit( _pUnit ), pPushUnit( _pPushUnit ), nPriority( _nPriority )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int GetTypeOfCollide( const SRect &unitRect, const SRect &unitSpeedRect, const SRect &unitFullSpeedRect, const SRect &unitSmallRect,
														const SRect &candRect, const SRect &candSpeedRect, const SRect &candFullSpeedRect, const SRect &candSmallRect,
														float *pfDist, const bool bIsCandInfantry )
{
	const bool bSpeedRectsInterceted = unitFullSpeedRect.IsIntersected( candFullSpeedRect );
	*pfDist = 0;
	if ( !bSpeedRectsInterceted )
		return 0;

	if ( unitSmallRect.IsIntersected( candSmallRect ) )
		return 4;

	bool bUnitRectCandSpeed = unitRect.IsIntersected( candSpeedRect );
	bool bUnitSpeedCandRect = unitSpeedRect.IsIntersected( candRect );

	if ( bUnitRectCandSpeed && !bUnitSpeedCandRect )
	{
		*pfDist = fabs( unitFullSpeedRect, candRect );
		return 1;
	}
	else if ( !bUnitRectCandSpeed && bUnitSpeedCandRect )
	{
		*pfDist = fabs( candFullSpeedRect, unitRect );
		return 2;
	}
	else if ( !bUnitRectCandSpeed && !bUnitSpeedCandRect )
	{
		SRect unitSpeedRect1;
		unitSpeedRect1.InitRect( unitSpeedRect.center, unitSpeedRect.dir, unitSpeedRect.lengthAhead * 2.50f, unitSpeedRect.lengthBack, unitSpeedRect.width );
		SRect candSpeedRect1;
		candSpeedRect1.InitRect( candSpeedRect.center, candSpeedRect.dir, candSpeedRect.lengthAhead * 2.50f, candSpeedRect.lengthBack, candSpeedRect.width );

		bUnitRectCandSpeed = unitRect.IsIntersected( candSpeedRect1 );
		bUnitSpeedCandRect = unitSpeedRect1.IsIntersected( candRect );

		if ( bUnitRectCandSpeed && bUnitSpeedCandRect )
			return 0;
		else if ( bUnitRectCandSpeed && !bUnitSpeedCandRect )
		{
			*pfDist = fabs( unitSpeedRect1, candRect );
			return 1;
		}
		else if ( !bUnitRectCandSpeed && bUnitSpeedCandRect )
		{
			*pfDist = fabs( candSpeedRect1, unitRect );
			return 2;
		}
		else
		{
			*pfDist = Min( fabs( unitSpeedRect1, candRect ), fabs( candSpeedRect1, unitRect ) );
			return 3;
		}
	}

	*pfDist = fabs( unitRect, candRect );
	return 4;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CCollision::FindCandidates()
{ 
	if ( pUnit->GetOwner()->IsColliding() && !pUnit->GetStats()->IsInfantry() &&
			 !pUnit->IsLockingTiles() && pUnit->GetCollision()->GetName() != ICollision::ECN_WAIT )
	{
		const SRect unitRect( pUnit->GetUnitRect() );
		const SRect unitSmallRect( pUnit->GetSmallRect() );

		for ( CUnitsIter<0,0> iter( 0, ANY_PARTY, pUnit->GetCenter(), 3.5f * unitRect.lengthAhead ); !iter.IsFinished(); iter.Iterate() )
		{
			CAIUnit *pCand = *iter;
			if ( IsValidObj( pCand ) )
			{
				CPathUnit *pCandPathUnit = pCand->GetPathUnit();

				if ( pCand->IsAlive() && pCand->IsColliding() &&
						 !pCandPathUnit->IsLockingTiles() && pUnit->GetID() != pCand->GetID() && 
						( pCand->GetStats()->IsInfantry() || pCand->GetID() > pUnit->GetID() || pCandPathUnit->GetCollision()->GetName() == ICollision::ECN_WAIT ) && 
						!pCand->IsInOneTrain( pUnit->GetOwner() ) )
				{
					const SRect unitSpeedRect( pUnit->GetSpeedRect( pCand->GetStats()->IsInfantry() ) );
					const SRect unitFullSpeedRect( pUnit->GetFullSpeedRect( pCand->GetStats()->IsInfantry() ) );
					
					const SRect candSpeedRect( pCandPathUnit->GetSpeedRect( false ) );
					const SRect candFullSpeedRect( pCandPathUnit->GetFullSpeedRect( false ) );
					const SRect candRect( pCand->GetUnitRect() );
					const SRect candSmallRect( pCand->GetPathUnit()->GetSmallRect() );

					float fDist;
					const int nCollideType = 
						GetTypeOfCollide( unitRect, unitSpeedRect, unitFullSpeedRect, unitSmallRect,
															candRect, candSpeedRect, candFullSpeedRect, candSmallRect, 
															&fDist, pCand->GetStats()->IsInfantry() );
					bool bHardCollision = ( nCollideType == 4 );
					bool bEnemySolider = pCand->GetStats()->IsInfantry() && theDipl.GetDiplStatus( pUnit->GetOwner()->GetPlayer(), pCand->GetPlayer() ) == EDI_ENEMY;

					if ( bEnemySolider )
					{
						if ( nCollideType == 4 && unitRect.IsIntersected( candRect ) )
						{
							theStatistics.UnitKilled( pUnit->GetOwner()->GetPlayer(), pCand->GetPlayer(), 1, pCand->GetStats()->fPrice );
							theMPInfo.UnitsKilled( pUnit->GetOwner()->GetPlayer(), pCand->GetStats()->fPrice, pCand->GetPlayer() );

							theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_DIE, false ), pCand, false );
							continue;
						}
					}

					const bool bNeedPush = pCand->GetStats()->IsInfantry() && 
																 pCandPathUnit->GetCollision()->GetPushUnit() != pUnit->GetOwner() &&
																 ( pCand->IsIdle() || pCand->GetDirVector() * pUnit->GetDirVector() > 0.8f );

					switch ( nCollideType )
					{
						case 0: break;
						case 1:
							pCand->GetCurPath()->NotifyAboutClosestThreat( pUnit->GetOwner(), fDist );
							pCandPathUnit->UpdateCollStayTime( pUnit->GetStayTime() );
							break;
						case 2:
							if ( !bNeedPush )
							{
								if ( !bEnemySolider && 
										( pCandPathUnit->GetCollision()->GetName() != ICollision::ECN_WAIT ||
											pUnit->GetCollision()->GetPushUnit() != pCand ) )
								{
									pUnit->GetCurPath()->NotifyAboutClosestThreat( pCand, fDist );
									pUnit->UpdateCollStayTime( pCandPathUnit->GetStayTime() );
								}
							}
							else
								bHardCollision = true;

							break;
						case 3:
							if ( pUnit->GetLastPushByHardCollUnit() == pCandPathUnit || pCand->GetStats()->IsInfantry() )
							{
								pCand->GetCurPath()->NotifyAboutClosestThreat( pUnit->GetOwner(), fDist );
								pCandPathUnit->UpdateCollStayTime( pUnit->GetStayTime() );
							}
							else if ( pCandPathUnit->GetLastPushByHardCollUnit() == pUnit )
							{
								if ( !bNeedPush && !bEnemySolider )
								{
									pUnit->GetCurPath()->NotifyAboutClosestThreat( pCand, fDist );
									pUnit->UpdateCollStayTime( pCandPathUnit->GetStayTime() );
								}
							}
							else
							{
								const CVec2 vDir = pUnit->GetDirVector() + pCand->GetDirVector();
								const CVec2 vToUnit = pUnit->GetCenter() - pCand->GetCenter();
								if ( vDir * vToUnit >= 0 )
								{
									pCand->GetCurPath()->NotifyAboutClosestThreat( pUnit->GetOwner(), fDist );
									pCandPathUnit->UpdateCollStayTime( pUnit->GetStayTime() );
								}
								else 
								{
									if ( !bNeedPush && !bEnemySolider )
									{
										pUnit->GetCurPath()->NotifyAboutClosestThreat( pCand, fDist );
										pUnit->UpdateCollStayTime( pCandPathUnit->GetStayTime() );
									}
								}
							}

							break;
					}

					if ( bHardCollision )
					{
						if ( !bEnemySolider && ( pCandPathUnit->GetCollision()->GetPushUnit() != pUnit->GetOwner() || pCand->GetStats()->IsInfantry() ) )
						{
							pUnit->GetCurPath()->NotifyAboutClosestThreat( pCand, fDist );
							pUnit->UpdateCollStayTime( pCandPathUnit->GetStayTime() );
						}
						if ( pUnit->GetCollision()->GetPushUnit() != pCand && !pCand->GetStats()->IsInfantry() )
						{
							pCand->GetCurPath()->NotifyAboutClosestThreat( pUnit->GetOwner(), fDist );
							pCandPathUnit->UpdateCollStayTime( pUnit->GetStayTime() );
						}
						
						if ( pCandPathUnit->GetCollision()->GetPushUnit() != pUnit->GetOwner() ||
								 pUnit->GetCollision()->GetPushUnit() != pCand && 
								 ( !pCand->GetStats()->IsInfantry() || unitRect.IsIntersected( candRect ) ) )
							theColCollector.AddCollision( pUnit, pCandPathUnit, nCollideType );
					}
					
					if ( nCollideType > 0 )
					{
						pUnit->IncNCollisions();
						pCandPathUnit->IncNCollisions();
					}
				}
			}
		}
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCollision::IsSolved()
{
	return 
		pPushUnit != 0 &&
		( !IsValidObj( pPushUnit ) || pPushUnit->GetPathUnit()->IsLockingTiles() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CFreeOfCollisions														*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFreeOfCollisions::CFreeOfCollisions( CPathUnit *pUnit, CAIUnit *pPushUnit )
: CCollision( pUnit, pPushUnit, pUnit->GetID() )
{
	if ( pUnit->GetStats()->IsInfantry() )
		nPriority <<= 12;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFreeOfCollisions::IsSolved()
{
	return 
		!pUnit->IsTurning() &&
		( pUnit->GetCurPath() == 0 || pUnit->GetCurPath()->IsFinished() || CCollision::IsSolved() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CGivingPlaceCollision												*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CGivingPlaceCollision::CGivingPlaceCollision( CPathUnit *pUnit, CAIUnit *pPushUnit, const CVec2 &_vDir, const float fDist, const int nPriority )
: CCollision( pUnit, pPushUnit, nPriority ), vDir( _vDir ), finishPoint( pUnit->GetCenter() + vDir * fDist ), timeToFinish( 0 )
{
	if ( pPushUnit->GetStats()->IsInfantry() )
		pPushUnit->ForceLockingTiles( true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPath* CGivingPlaceCollision::GetPath() const
{
	return pUnit->CreatePathByDirection( pUnit->GetCenter(), vDir, finishPoint );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGivingPlaceCollision::IsPathSolved()
{
	return ( pUnit->GetCurPath() == 0 || pUnit->GetCurPath()->IsWithFormation() || 
				   pUnit->GetCurPath()->IsFinished() || CCollision::IsSolved() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGivingPlaceCollision::IsSolved()
{
	return IsPathSolved() && ( !pUnit->GetOwner()->GetStats()->IsInfantry() || timeToFinish != 0 && timeToFinish < curTime );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGivingPlaceCollision::Segment()
{
	if ( IsPathSolved() && timeToFinish == 0 )
		timeToFinish = curTime + 1200;
	
	if ( IsSolved() && IsValidObj( pPushUnit ) )
	{
		ICollision *pPushUnitCollision = pPushUnit->GetPathUnit()->GetCollision();
		if ( pPushUnitCollision->GetName() == ICollision::ECN_WAIT && pPushUnitCollision->GetPushUnit() == pUnit->GetOwner() )
			static_cast<CWaitingCollision*>(pPushUnitCollision)->Finish();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										CGivingPlaceRotateCollision										*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CGivingPlaceRotateCollision::CGivingPlaceRotateCollision( class CPathUnit *pUnit, class CAIUnit *pPushUnit, const CVec2 &vDir, const int nPriority )
: CCollision( pUnit, pPushUnit, nPriority ), wDir( GetDirectionByVector( vDir ) ), bTurned( false )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGivingPlaceRotateCollision::Segment()
{
	if ( !bTurned )
		bTurned = pUnit->TurnToDir( wDir, true );

	if ( IsSolved() && IsValidObj( pPushUnit ) )
	{
		ICollision *pPushUnitCollision = pPushUnit->GetPathUnit()->GetCollision();
		if ( pPushUnitCollision->GetName() == ICollision::ECN_WAIT && pPushUnitCollision->GetPushUnit() == pUnit->GetOwner() )
			static_cast<CWaitingCollision*>(pPushUnitCollision)->Finish();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CWaitingCollision														*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CWaitingCollision::CWaitingCollision( CPathUnit *pUnit, CAIUnit *pPushUnit, bool _bLock )
: CCollision( pUnit, pPushUnit, pUnit->GetID() ), finishTime( curTime + 1000 + Random( 0, 1000 ) ), bLock( _bLock )
{
	if ( pUnit->GetStats()->IsInfantry() )
		nPriority <<= 12;

	if ( bLock )
		pUnit->ForceLockingTiles();
	else
	{
		pUnit->UnlockTiles( true );
		pUnit->FixUnlocking();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CWaitingCollision::~CWaitingCollision()
{
	if ( pUnit && !bLock )
		pUnit->UnfixUnlocking();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWaitingCollision::IsSolved()
{
	return curTime > finishTime || CCollision::IsSolved();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaitingCollision::Finish()
{
	finishTime = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													CStopCollision													*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStopCollision::CStopCollision( CPathUnit *pUnit )
: CCollision( pUnit, 0, pUnit->GetID() ), finishTime( curTime + 1000 + Random( 0, 2000 ) )
{
	if ( pUnit->GetStats()->IsInfantry() )
		nPriority <<= 12;
	
	pUnit->ForceLockingTiles();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStopCollision::IsSolved()
{
	return curTime > finishTime;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CCollisionsCollector												*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <int N>
bool CanGo( const CVec2 &vNewDir, CPathUnit *pUnit1, CPathUnit *pUnit2, float *pfDist, int *pnPenalty, const SGenericNumber<N> &p )
{
	SRect smallRect = pUnit1->GetSmallRect();
	SRect unitRect = pUnit1->GetUnitRect();

	if ( !pUnit1->GetOwner()->GetStats()->IsInfantry() && !pUnit1->CanRotateTo( smallRect, vNewDir, true ) )
		return false;

	smallRect.InitRect( smallRect.center, vNewDir, smallRect.lengthAhead, smallRect.lengthBack, smallRect.width );
	unitRect.InitRect( unitRect.center, vNewDir, unitRect.lengthAhead, unitRect.lengthBack, unitRect.width );
	
	SRect forceFullSpeedRect;
	CCircle forceCircle, forceThickCircle;
	if ( N )
	{
		forceFullSpeedRect = pUnit2->GetFullSpeedRect( pUnit1->GetOwner() );
		forceFullSpeedRect.InitRect( forceFullSpeedRect.center, forceFullSpeedRect.dir, forceFullSpeedRect.lengthAhead * 3.0f, forceFullSpeedRect.lengthBack, forceFullSpeedRect.width * 1.3f );
	}
	else
	{
		forceCircle = CCircle( pUnit2->GetCenter(), pUnit2->GetStats()->vAABBHalfSize.y );
		forceThickCircle = CCircle( pUnit2->GetCenter(), pUnit2->GetStats()->vAABBHalfSize.y + 10.0f );
	}

	SRect forceUnitRect = pUnit2->GetUnitRect();
	SRect resultRect( unitRect );
	if ( pUnit1->GetStats()->IsInfantry() )
	{
		resultRect.width += SConsts::TILE_SIZE / 4 ;
	}

	float fAddLength = 0;
	*pnPenalty = 0;
	bool bGoodIntersect, bBadIntersect;
	do
	{
		bBadIntersect = N ? smallRect.IsIntersected( forceUnitRect ) : smallRect.IsIntersectCircle( forceCircle );
		if ( bBadIntersect && pUnit1->GetStats()->IsInfantry() )
		{
			bBadIntersect = false;
			++(*pnPenalty);
		}

		if ( !bBadIntersect )
		{
			bGoodIntersect = N ? unitRect.IsIntersected( forceFullSpeedRect ) : unitRect.IsIntersectCircle( forceThickCircle );

			if ( bGoodIntersect )
			{
				smallRect.center += vNewDir * float( SConsts::TILE_SIZE / 2 );
				smallRect.InitRect( smallRect.center, smallRect.dir, smallRect.lengthAhead, smallRect.lengthBack, smallRect.width );

				unitRect.center += vNewDir * float( SConsts::TILE_SIZE / 2 );
				unitRect.InitRect( unitRect.center, unitRect.dir, unitRect.lengthAhead, unitRect.lengthBack, unitRect.width );

				fAddLength += float(SConsts::TILE_SIZE / 2);
			}
		}
	} while ( !bBadIntersect && bGoodIntersect );

	if ( bBadIntersect )
		return false;

	bool bUnitRectIntersected;	
	do
	{
		bBadIntersect = N ? smallRect.IsIntersected( forceUnitRect ) : smallRect.IsIntersectCircle( forceCircle );
		if ( bBadIntersect && pUnit1->GetStats()->IsInfantry() )
		{
			bBadIntersect = false;
			++(*pnPenalty);
		}
		else if ( bBadIntersect )
			return false;

		bUnitRectIntersected = N ? unitRect.IsIntersected( forceFullSpeedRect ) : unitRect.IsIntersectCircle( forceThickCircle );
		if ( bUnitRectIntersected || AICellsTiles::GetTile( unitRect.center ) == pUnit1->GetTile() )
		{
			smallRect.center += vNewDir * float( SConsts::TILE_SIZE / 2 );
			smallRect.InitRect( smallRect.center, smallRect.dir, smallRect.lengthAhead, smallRect.lengthBack, smallRect.width );

			unitRect.center += vNewDir * float( SConsts::TILE_SIZE / 2 );
			unitRect.InitRect( unitRect.center, unitRect.dir, unitRect.lengthAhead, unitRect.lengthBack, smallRect.width );

			fAddLength += float( SConsts::TILE_SIZE / 2 );
		}
	} while ( bUnitRectIntersected || AICellsTiles::GetTile( unitRect.center ) == pUnit1->GetTile() );
	
	fAddLength += int(pUnit1->GetStats()->IsInfantry()) * SConsts::TILE_SIZE;

	resultRect.InitRect( resultRect.center, vNewDir, resultRect.lengthAhead + fAddLength, resultRect.lengthBack, resultRect.width );
	if ( pUnit1->GetOwner()->IsOnLockedTiles( resultRect ) )
		return false;

	const CVec2 vResultCenter = ( resultRect.v1 + resultRect.v3 ) * 0.5f;
	const float fResultRadius = ( resultRect.lengthAhead + resultRect.lengthBack ) * 0.5 * 1.4;

	for ( CUnitsIter<0,0> iter( 0, ANY_PARTY, vResultCenter, fResultRadius ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = *iter;
		if ( IsValidObj( pUnit ) && pUnit->GetPathUnit() != pUnit1 && pUnit->GetPathUnit() != pUnit2 && !pUnit->GetStats()->IsAviation() )
		{
			const bool bCanPush = 
				pUnit->GetStats()->IsInfantry() && 
				pUnit->GetPathUnit()->GetCollision()->GetPriority() >= pUnit->GetPathUnit()->GetCollision()->GetPriority();

			if ( !bCanPush && resultRect.IsIntersected( pUnit->GetUnitRect() ) )
				return false;
		}
	}

	*pfDist = fAddLength;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const NTimer::STime GetTimeToGo( const WORD wUnitDir, const WORD wPushUnitDir, const WORD &wNewDir, const float fDist, const float fUnitSpeed, const float fUnitRotateSpeed )
{
	const WORD wDirsDiff = DirsDifference( wNewDir, wUnitDir );

	if ( fDist == 0.0f && wDirsDiff < 2000 )
		return 100000;
	
	const WORD wRotate = Min( wDirsDiff,	DirsDifference( wNewDir, wUnitDir + 32768 ) );

	NTimer::STime timeToGo = fDist / fUnitSpeed;
	if ( fUnitRotateSpeed != 0 )
		timeToGo += float(wRotate) / fUnitRotateSpeed;

	if ( wDirsDiff > 28000 || DirsDifference( wNewDir, wPushUnitDir ) < 5000 )
		timeToGo *= 3;
	if ( fDist == 0 )
		timeToGo *= 3;

	return timeToGo;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define COMPARE_TIME_TO_GO( curTime, vCurDir, fCurDist, bestTime, vBestDir, fBestDist ) \
if ( (curTime) < (bestTime ) )	\
{																\
	bestTime = curTime;						\
	vBestDir = vCurDir;						\
	fBestDist = fCurDist;					\
}

template<int N>
bool TryToPush( CPathUnit *pUnit1, CPathUnit *pUnit2, const SGenericNumber<N> &p )
{
	const CVec2 vMinAngle( NTrg::Cos( 22.5f / 180.0f * FP_PI ), NTrg::Sin( 22.5f / 180.0f * FP_PI ) );
	CVec2 vCurAngle = vMinAngle;
	int nSign = 1;

	CVec2 vDir = pUnit2->GetDirVector();
	const WORD wUnit2Dir = GetDirectionByVector( vDir );
	const WORD wUnitDir = pUnit1->GetDir();
	float fUnitSpeed = pUnit1->GetSpeedLen();
	if ( fUnitSpeed == 0.0f )
		fUnitSpeed = pUnit1->GetMaxPossibleSpeed();

	NTimer::STime bestTime = 1000000;
	CVec2 vBestDir;
	float fBestDist;

	while ( vDir * pUnit2->GetDirVector() >= -0.001f )
	{
		float fDist;
		int nPenalty;
		const WORD wDir = GetDirectionByVector( vDir );
		if ( CanGo( vDir, pUnit1, pUnit2, &fDist, &nPenalty, p ) )
		{
			NTimer::STime timeToGo = GetTimeToGo( wUnitDir, wUnit2Dir, GetDirectionByVector( vDir ), fDist, fUnitSpeed, pUnit1->GetRotateSpeed() ) + nPenalty * 10000;
			if ( DirsDifference( wDir, pUnit1->GetFrontDir() ) > DirsDifference( wDir, pUnit1->GetFrontDir() + 32768 ) )
				timeToGo *= 1.5;
			
			COMPARE_TIME_TO_GO( timeToGo, vDir, fDist, bestTime, vBestDir, fBestDist );
		}

		if ( CanGo( -vDir, pUnit1, pUnit2, &fDist, &nPenalty, p ) )
		{
			NTimer::STime timeToGo = GetTimeToGo( wUnitDir, wUnit2Dir, wDir + 32768, fDist, fUnitSpeed, pUnit1->GetRotateSpeed() ) + nPenalty * 10000;
			if ( DirsDifference( wDir + 32768, pUnit1->GetFrontDir() ) > DirsDifference( wDir + 32768, pUnit1->GetFrontDir() + 32768 ) )
				timeToGo *= 1.5;

			COMPARE_TIME_TO_GO( timeToGo, -vDir, fDist, bestTime, vBestDir, fBestDist );
		}

		vDir ^= vCurAngle;

		vCurAngle.y = fabs( vCurAngle.y );
		vCurAngle ^= vMinAngle;
		nSign *= -1;
		vCurAngle.y *= nSign;
	}

	{
		float fDist;
		int nPenalty;
		vDir = -pUnit1->GetDirVector();
		const WORD wDir = GetDirectionByVector( vDir );
		if ( CanGo( vDir, pUnit1, pUnit2, &fDist, &nPenalty, p ) )
		{
			NTimer::STime timeToGo = GetTimeToGo( wUnitDir, wUnit2Dir, GetDirectionByVector( vDir ), fDist, fUnitSpeed, pUnit1->GetRotateSpeed() ) + nPenalty * 10000;
			if ( DirsDifference( wDir, pUnit1->GetFrontDir() ) > DirsDifference( wDir, pUnit1->GetFrontDir() + 32768 ) )
				timeToGo *= 1.5;

			COMPARE_TIME_TO_GO( timeToGo, vDir, fDist, bestTime, vBestDir, fBestDist );
		}
	}

	if ( bestTime < 1000000 )
	{
		bool bEnemies = theDipl.GetDiplStatus( pUnit1->GetOwner()->GetPlayer(), pUnit2->GetOwner()->GetPlayer() ) == EDI_ENEMY;
		bool bUnit1EnemySoldier = bEnemies && pUnit1->GetStats()->IsInfantry();
		bool bUnit2EnemySoldier = bEnemies && pUnit2->GetStats()->IsInfantry();
		
		if ( !bUnit2EnemySoldier )
		{
			if ( fBestDist > 0 )
				pUnit1->SetCollision( new CGivingPlaceCollision( pUnit1, pUnit2->GetOwner(), vBestDir, fBestDist, Max( pUnit1->GetCollision()->GetPriority(), pUnit2->GetCollision()->GetPriority() ) )	);
			else
				pUnit1->SetCollision( new CGivingPlaceRotateCollision( pUnit1, pUnit2->GetOwner(), vBestDir, Max( pUnit1->GetCollision()->GetPriority(), pUnit2->GetCollision()->GetPriority() ) ) );
		}

		if ( !pUnit1->GetOwner()->GetStats()->IsInfantry() && !bUnit1EnemySoldier )
			pUnit2->SetCollision( new CWaitingCollision( pUnit2, pUnit1->GetOwner(), pUnit2->GetOwner()->GetStats()->IsInfantry() ) );

		return true;
	}
	
	return false;
}

#undef COMPARE_TIME_TO_GO
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCollisionsCollector::AddCollision( CPathUnit *pUnit1, CPathUnit *pUnit2, const int nCollideType )
{
	if ( pUnit1->GetCollision()->GetPriority() > pUnit2->GetCollision()->GetPriority() )
		collisions.push( SUnitsPair( pUnit1, pUnit2, nCollideType ) );
	else
		collisions.push( SUnitsPair( pUnit2, pUnit1, nCollideType ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCollisionsCollector::HandOutCollisions()
{
	while ( !collisions.empty() )
	{
		const SUnitsPair &unitsPair = collisions.top();
		CPathUnit *pUnit1 = unitsPair.pUnit1;
		CPathUnit *pUnit2 = unitsPair.pUnit2;

		if ( pUnit1->GetCollision()->GetName() != ICollision::ECN_STOP && 
				 pUnit2->GetCollision()->GetName() != ICollision::ECN_STOP )
		{
			bool bPush = false;
			if ( !pUnit1->GetStats()->IsTrain() )
				bPush = TryToPush( pUnit1, pUnit2, SGenericNumber<1>() );
			if ( !bPush )
			{
				if ( !pUnit2->GetStats()->IsTrain() )
					bPush = TryToPush( pUnit2, pUnit1, SGenericNumber<1>() );

				if ( !bPush )
				{
					if ( pUnit1->GetOwner()->IsTrain() && !pUnit2->GetOwner()->IsTrain() )
						pUnit1->SetCollision( new CWaitingCollision( pUnit1, pUnit2->GetOwner(), true ) );
					else if ( pUnit2->GetOwner()->IsTrain() && !pUnit1->GetOwner()->IsTrain() )
						pUnit2->SetCollision( new CWaitingCollision( pUnit2, pUnit1->GetOwner(), true ) );
					else if ( pUnit1->GetNCollisions() < pUnit2->GetNCollisions() )
						pUnit1->SetCollision( new CWaitingCollision( pUnit1, pUnit2->GetOwner(), true ) );
					else
						pUnit2->SetCollision( new CWaitingCollision( pUnit2, pUnit1->GetOwner(), true ) );
				}
				else
				{
					pUnit1->SetLastPushByHardCollUnit( pUnit2 );
					pUnit2->SetLastPushByHardCollUnit( 0 );
				}
					
			}
			else
			{
				pUnit2->SetLastPushByHardCollUnit( pUnit1 );
				pUnit1->SetLastPushByHardCollUnit( 0 );
			}
		}

		while ( !collisions.empty() && collisions.top().pUnit1.GetPtr() == pUnit1 )
			collisions.pop();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													SUnitsPair															*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool operator < ( const SUnitsPair &pair1, const SUnitsPair &pair2 )
{
	return pair1.pUnit1->GetCollision()->GetPriority() < pair2.pUnit1->GetCollision()->GetPriority();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
