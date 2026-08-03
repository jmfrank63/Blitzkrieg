#include "StdAfx.h"

#include "BasePathUnit.h"
#include "StandartPath.h"
#include "StandartSmoothMechPath.h"
#include "AIStaticMap.h"
extern CStaticMap theStaticMap;
extern NTimer::STime curTime;
BASIC_REGISTER_CLASS( CStandartSmoothMechPath );
CStandartSmoothMechPath::CStandartSmoothMechPath()
: speed( 0 ),	bNotified( false ), pPath( 0 ), nPoints( 0 ), bStopped( false ), bMinSlowed( false ), bMaxSlowed( false ), 
	bSmoothTurn( false ), bCanGoForward( true ), bCanGoBackward( true ), pUnit( 0 ), vLastValidatedPoint( VNULL2 )
{
}
void CStandartSmoothMechPath::SetOwner( IBasePathUnit *_pUnit )
{
	pUnit = _pUnit;
}
IBasePathUnit* CStandartSmoothMechPath::GetOwner() const
{
	return pUnit;
}
void CStandartSmoothMechPath::AddSmoothTurn()
{
	CVec2 vDir = spline.GetDX();
	Normalize( &vDir );
	if ( pUnit->GetDirVector() * vDir > pUnit->GetSmoothTurnThreshold() )
	{
		const float dist = fabs( p3 - p0 );
		const float fProj = vDir * pUnit->GetDirVector();
		const float coeff = ( dist / 2.0f ) / fProj;

		const CVec2 point = p0 + pUnit->GetDirVector() * coeff / 1.5f;

		const SVector tile = AICellsTiles::GetTile( point );
		CBres bres;
		bres.InitPoint( AICellsTiles::GetTile( p0 ), tile );

		while ( bres.GetDirection() != tile )
		{
			if ( !theStaticMap.CanUnitGo( pUnit->GetBoundTileRadius(), bres.GetDirection(), pUnit->GetAIClass() ) )
				break;

			bres.MakePointStep();
		}

		if ( bres.GetDirection() == tile && theStaticMap.CanUnitGo( pUnit->GetBoundTileRadius(), bres.GetDirection(), pUnit->GetAIClass() ) )
		{
			std::list<SVector> insertedTiles;

			bres.Init( tile, AICellsTiles::GetTile( p3 ) );
			while ( bres.GetDirection() != tile )
			{
				if ( !theStaticMap.CanUnitGo( pUnit->GetBoundTileRadius(), bres.GetDirection(), pUnit->GetAIClass() ) )
					break;

				insertedTiles.push_back( bres.GetDirection() );
				bres.MakePointStep();
			}
			
			if ( bres.GetDirection() == tile && theStaticMap.CanUnitGo( pUnit->GetBoundTileRadius(), bres.GetDirection(), pUnit->GetAIClass() ) )
			{
				pPath->InsertTiles( insertedTiles );

				p3 = point;
				spline.Init( p0, p1, p2, p3 );
			}
		}
	}
}
bool CStandartSmoothMechPath::CheckTurn( const WORD wNewDir )
{
	bool bCanBackward = pPath->CanGoBackward( pUnit );
	const WORD wUnitDir = pUnit->GetDir();
	const WORD wRightDirsDiff = DirsDifference( wUnitDir, wNewDir );
	const WORD wBackDirsDiff = DirsDifference( wUnitDir + 32768, wNewDir );
	if ( wRightDirsDiff < SConsts::DIR_DIFF_TO_SMOOTH_TURNING || bCanBackward && wBackDirsDiff < SConsts::DIR_DIFF_TO_SMOOTH_TURNING )
		return true;
	else
	{
		const CVec2 vNewDir = GetVectorByDirection( wNewDir );
		int nResult = -1;
		
		SRect unitRect = pUnit->GetUnitRectForLock();
		CTemporaryUnitRectUnlocker unlocker( pUnit->GetID() );

		bool bCanRotateForward = pUnit->CanRotateTo( unitRect, vNewDir, false, false );
		bool bCanRotateBackward = pUnit->CanRotateTo( unitRect, -vNewDir, false, false );

		if ( bCanRotateForward && bCanRotateBackward )
		{
			if ( pUnit->IsDangerousDirExist() )
			{
				const WORD wDangerousDir = pUnit->GetDangerousDir();

				bCanRotateForward =
					DirsDifference( wNewDir, wDangerousDir ) < DirsDifference( wNewDir + 32768, wDangerousDir );
				bCanRotateBackward = !bCanRotateForward;
			}
		}

		if ( nResult == -1 && bCanRotateForward && bCanRotateBackward )
			nResult = 1;
		if ( nResult == -1 && !bCanBackward && bCanRotateForward )
			nResult = 1;
		if ( nResult == -1 && bCanBackward && wBackDirsDiff < wRightDirsDiff && bCanRotateBackward )
			nResult = 1;

		if ( nResult == -1 && bCanRotateForward )
		{
			bCanGoBackward = false;
			nResult = 1;
		}

		if ( nResult == -1 && bCanRotateBackward )
		{
			bCanGoForward = false;
			nResult = 1;
		}

		if ( nResult == -1 && pUnit->CheckToTurn( wNewDir ) )
			nResult = 1;

		return ( nResult == 1 );
	}
}
bool CStandartSmoothMechPath::Init( IBasePathUnit *_pUnit, IPath *_pPath, bool _bSmoothTurn, bool bCheckTurn )
{
	NI_ASSERT_T( _pPath != 0, "Smooth path is trying to be initialized by NULL static path" );

	pUnit = _pUnit;
	bSmoothTurn = _bSmoothTurn;
	bCanGoForward = bCanGoBackward = true;
	lastCheckToRightTurn = 0;

	const WORD wOldPathDirection = pUnit->GetDir();
	
	pPath = _pPath;
	if ( pUnit->GetTile() == AICellsTiles::GetTile( pPath->GetFinishPoint() ) )
	{
		pPath = 0;
		bFinished = true;
		return false;
	}
	else
	{
		if ( !pPath->CanGoBackward( pUnit ) )
			pUnit->ForceGoByRightDir();

		predPoint = p0 = p1 = p2 = p3 = pPath->GetStartPoint();

		const int nInitSplineResult = InitSpline();

		if ( bCheckTurn && pPath->ShouldCheckTurn() )
		{

			CheckTurn( GetDirectionByVector( spline.GetDX() ) );
		}

		if ( pPath && bSmoothTurn && nInitSplineResult == SConsts::SPLINE_STEP )
			AddSmoothTurn();

		fRemain = 0;
		bFinished = false;

		if ( !pPath || pPath->IsFinished() && spline.GetPoint() == pPath->GetFinishPoint() )
		{
			bFinished = true;
			pPath = 0;
			return false;
		}
		nPoints = 0;

		spline.Iterate();
		bStopped = false;
	}
	
	if ( DirsDifference( GetDirectionByVector( spline.GetDX() ), wOldPathDirection ) > 20000 )
		speed = 0.0f;

	return true;
}
void CStandartSmoothMechPath::GetNextTiles( std::list<SVector> *pTiles )
{
	CBSpline::SForwardIter iter;
	spline.StartForwardIterating( &iter );

	while ( iter.t != -1 )
	{
		const SVector tile = AICellsTiles::GetTile( iter.x );
		if ( pTiles->empty() || tile != pTiles->back() )
			pTiles->push_back( tile );

		spline.IterateForward( &iter );
	}
}
CVec2 CStandartSmoothMechPath::GetShift( const int nToShift ) const
{
	NI_ASSERT_T( pPath != 0, "Wrong call of GetShift" );
	return pPath->PeekPoint( nToShift );
}
bool CStandartSmoothMechPath::ValidateCurPath( const CVec2 &center, const CVec2 &newPoint )
{
	if ( fabs2( center - vLastValidatedPoint ) >= sqr( (float)SAIConsts::TILE_SIZE / 2.0f ) )
/*
		&&
			 ( center.x / ( 2.0f * SConsts::TILE_SIZE ) != newPoint.x / ( 2.0f * SConsts::TILE_SIZE ) ||
				 center.y / ( 2.0f * SConsts::TILE_SIZE ) != newPoint.y / ( 2.0f * SConsts::TILE_SIZE ) )
		 )
*/
	{
		vLastValidatedPoint = center;
		SRect unitRect( pUnit->GetUnitRectForLock() );
		unitRect.InitRect( center, unitRect.dir, unitRect.lengthAhead + SConsts::TILE_SIZE * 0.85f, 0, unitRect.width * 0.9f );

		if ( IsRectOnLockedTiles( unitRect, pUnit->GetAIClass() ) )
		{
			const SVector unitTileCenter = AICellsTiles::GetTile( center );
			const int nBoundTileRadius = pUnit->GetBoundTileRadius();

			CTilesSet tiles;
			GetTilesCoveredByRect( unitRect, &tiles );
			CTilesSet::iterator iter = tiles.begin();
			while ( iter != tiles.end() )
			{
				const SVector &tile = *iter;
				if ( abs( tile.x - unitTileCenter.x ) <= nBoundTileRadius || 
						 abs( tile.y - unitTileCenter.y ) <= nBoundTileRadius )
					iter = tiles.erase( iter );
				else
				{
					const CVec2 vTileCenter = AICellsTiles::GetPointByTile( tile );
					CVec2 vDiff = vTileCenter - center;
					Normalize( &vDiff );
					if ( vDiff * pUnit->GetDirVector() > 1/sqrt(2.0f) || vDiff * pUnit->GetDirVector() < -1/sqrt(2.0f) )
						iter = tiles.erase( iter );
					else
						++iter;
				}
			}

			theStaticMap.UpdateMaxesByTiles( tiles, AI_CLASS_ANY, true );
			
			pPath->Recalculate( newPoint, pUnit->GetLastKnownGoodTile() );
			Init( pUnit, pPath, bSmoothTurn, false );

			theStaticMap.UpdateMaxesByTiles( tiles, AI_CLASS_ANY, false );
		}
	}

	return true;
}
bool CStandartSmoothMechPath::IsFinished() const
{ 
	return bFinished || !pPath;
}
void CStandartSmoothMechPath::Stop()
{
	bStopped = true;
	speed = 0;
	pUnit->StopTurning();
}
int CStandartSmoothMechPath::InitSpline()
{
	p0 = p1; p1 = p2; p2 = p3;

	int inc = 0;
	if ( theStaticMap.IsBroken(
				 pUnit->GetBoundTileRadius(),
				 AICellsTiles::GetTile( predPoint ), 
				 AICellsTiles::GetTile( pPath->PeekPoint( 0 ) ), 
				 AICellsTiles::GetTile( pPath->PeekPoint( 1 ) ),
				 pUnit->GetAIClass() )
			|| 
			 theStaticMap.IsBroken(
				 pUnit->GetBoundTileRadius(),
				 AICellsTiles::GetTile( pPath->PeekPoint( 0 ) ), 
				 AICellsTiles::GetTile( pPath->PeekPoint( 1 ) ), 
				 AICellsTiles::GetTile( pPath->PeekPoint( 2 ) ),
				 pUnit->GetAIClass() )
		 )
	{
		predPoint = p3;
		p3 = pPath->PeekPoint( 1 );
		pPath->Shift( 1 );
		++nPoints;
	}
	else
	{
		while ( !pPath->IsFinished() && inc < SConsts::SPLINE_STEP &&
						!theStaticMap.IsBroken( 
								pUnit->GetBoundTileRadius(),
								AICellsTiles::GetTile( pPath->PeekPoint( 0 ) ), 
								AICellsTiles::GetTile( pPath->PeekPoint( 1 ) ), 
								AICellsTiles::GetTile( pPath->PeekPoint( 2 ) ),
								pUnit->GetAIClass() )
					)
		{
			++inc;
			++nPoints;
			predPoint = pPath->PeekPoint( 1 );
			pPath->Shift( 1 );
		}

		p3 = pPath->PeekPoint( 1 );
		pPath->Shift( 1 );
	}

	nIter = 0;	
	spline.Init( p0, p1, p2, p3 );
	
	return inc;
}
const CVec2 CStandartSmoothMechPath::GetPointWithoutFormation( NTimer::STime timeDiff )
{
	if ( !pPath || bStopped || bFinished || p0 == p1 && p1 == p2 && p2 == p3 )
	{
		speed = 0;
		if ( !bStopped && !bFinished )
		{
			Stop();
			bFinished = true;
			pPath = 0;
		}

		return pUnit->GetCenter();
	}

	if ( !bCanGoForward && curTime - lastCheckToRightTurn >= 3000 + Random( 0, 1000 ) )
	{
		lastCheckToRightTurn = curTime;
		
		const WORD wFrontDir = pUnit->GetFrontDir();
		bool bCheck = true;
		if ( pUnit->IsDangerousDirExist() )
		{
			const WORD wDangerousDir = pUnit->GetDangerousDir();
			bCheck = DirsDifference( wFrontDir, wDangerousDir ) > DirsDifference( wFrontDir + 32768, wDangerousDir );
		}

		if ( bCheck )
		{
			const CVec2 vFrontDir = GetVectorByDirection( wFrontDir );
			SRect unitRect = pUnit->GetUnitRectForLock();
			if ( pUnit->CanRotateTo( unitRect, -vFrontDir, false, false ) )
			{
				bCanGoForward = true;
				if ( !pPath->CanGoBackward( pUnit ) )
					pUnit->ForceGoByRightDir();
			}
		}
	}
	
	if ( spline.GetDX() != VNULL2 )
	{
		const WORD wDirsDiff = DirsDifference( pUnit->GetDir(), GetDirectionByVector( spline.GetDX() ) );
		if ( wDirsDiff != 0 && pUnit->IsTurning() || wDirsDiff > SConsts::DIR_DIFF_TO_SMOOTH_TURNING )
		{
			if ( !pUnit->TurnToDir( GetDirectionByVector( spline.GetDX() ), CanGoBackward(), CanGoForward() ) )
			{
				speed = 0;
				const CVec2 vResult = pUnit->GetCenter();
				ValidateCurPath( vResult, vResult );
				return vResult;
			}
		}
	}

	fRemain = speed * timeDiff;

	while ( !bFinished && fabs( spline.GetPoint() - pUnit->GetCenter() ) < fRemain ) 
	{
		while ( fabs( spline.GetPoint() - pUnit->GetCenter() ) < fRemain && nIter < CBSpline::N_OF_ITERATONS )
		{
			spline.Iterate();
			++nIter;
		}

		if ( pPath->IsFinished() && mDistance( spline.GetPoint(), pPath->GetFinishPoint() ) < 2 )
		{
			bFinished = true;
			Stop();
			pPath = 0;
			speed = fabs( spline.GetPoint() - pUnit->GetCenter() ) / timeDiff;
			return spline.GetPoint();
		}
		else		
		{
			if ( fabs( spline.GetPoint() - pUnit->GetCenter() ) < fRemain && nIter >= CBSpline::N_OF_ITERATONS )
				InitSpline();
		}
	}

	if ( !bFinished )
	{
		float fSplineSpeed = pUnit->GetMaxPossibleSpeed();
		if ( spline.GetReverseR() >= 0.01 )
		{
			const float R = 1 / spline.GetReverseR() * 0.01;
			fSplineSpeed = pUnit->GetMaxPossibleSpeed() * R * sqrt(R);
		}

		if ( fSplineSpeed < speed )
			speed = fSplineSpeed;

		if ( pPath->IsFinished() && mDistance( spline.GetPoint(), pPath->GetFinishPoint() ) < 2 )
		{
			bFinished = true;
			Stop();
			pPath = 0;
			speed = fabs( spline.GetPoint() - pUnit->GetCenter() ) / timeDiff;
			return spline.GetPoint();
		}
	}

	if ( spline.GetDX() != VNULL2 )
	{
		const WORD wDirsDiff = DirsDifference( pUnit->GetDir(), GetDirectionByVector( spline.GetDX() ) );
		if ( wDirsDiff != 0 && pUnit->IsTurning() || wDirsDiff	> SConsts::DIR_DIFF_TO_SMOOTH_TURNING )
		{
			if ( !pUnit->TurnToDir( GetDirectionByVector( spline.GetDX() ) ) )
			{
				speed = 0;
				const CVec2 vResult = pUnit->GetCenter();
				ValidateCurPath( vResult, vResult );
				return vResult;
			}
		}
		else
			pUnit->UpdateDirection( spline.GetDX() );
	}
	
	CVec2 result;
	if ( fabs( spline.GetPoint() - pUnit->GetCenter() ) < fRemain )
		result = spline.GetPoint();
	else
		result = pUnit->GetCenter() + Norm( spline.GetPoint() - pUnit->GetCenter() ) * fRemain;

	float fDist = fabs2( result - pPath->GetFinishPoint() );
	if ( fDist <= sqr( 4.0f * pUnit->GetAABBHalfSize().y ) )
	{
		SRect rect = pUnit->GetUnitRectForLock();
		if ( pUnit->GetDir() == pUnit->GetFrontDir() )
			rect.InitRect( result, rect.dir, rect.lengthAhead * 1.5f, 0, rect.width );
		else
			rect.InitRect( result, rect.dir, 0, rect.lengthBack * 1.5f, rect.width );
			
		if ( pUnit->IsOnLockedTiles( rect ) )
		{
			bFinished = true;
			Stop();
			pPath = 0;
			speed = fabs( result - pUnit->GetCenter() ) / timeDiff;
			return result;
		}
	}

	speed = fabs( result - pUnit->GetCenter() ) / timeDiff;
	
	if ( !bFinished )
		ValidateCurPath( pUnit->GetCenter(), result );
		
	return result;
}
const CVec3 CStandartSmoothMechPath::GetPoint( NTimer::STime timeDiff )
{
	const bool bFirstCall = ( speed == 0.0f );
	
	CVec2 vResult;
	if ( !bNotified )
	{
		const float minSpeed = speed - pUnit->GetMaxPossibleSpeed() / 40;
		const float maxSpeed = speed + pUnit->GetMaxPossibleSpeed() / 40;
		float maxSpeedHere = pUnit->GetMaxSpeedHere( pUnit->GetCenter() );		

		if ( maxSpeed < maxSpeedHere )
			speed = maxSpeed;
		else if ( minSpeed > maxSpeedHere )
			speed = minSpeed;
		else
			speed = maxSpeedHere;

		vResult = GetPointWithoutFormation( timeDiff );
	}
	else if ( bMaxSlowed && speed == 0.0f )
	{
		vResult = pUnit->GetCenter();
		vLastValidatedPoint = VNULL2;
	}
	else
		vResult = GetPointWithoutFormation( timeDiff );

	bNotified = bMinSlowed = bMaxSlowed = false;
	return CVec3( vResult, pUnit->GetZ() );
}
void CStandartSmoothMechPath::NotifyAboutClosestThreat( IBasePathUnit *pUnit, const float fDist )
{
	if ( pUnit->CanMovePathfinding() )
	{
		const float maxSpeedHere = pUnit->GetMaxSpeedHere( pUnit->GetCenter() );
		const float maxPossibleSpeed = pUnit->GetMaxPossibleSpeed();
		if ( fDist >= 3.0f * SConsts::TILE_SIZE )
		{
			if ( speed >= 2 * maxSpeedHere / 3 && !bMaxSlowed && !bMinSlowed )
			{
				speed = speed - maxPossibleSpeed / 40;
				bMinSlowed = true;
			}
			else
			{
				if ( !bMinSlowed && !bMaxSlowed )
					speed = speed + maxPossibleSpeed / 40;
			}
		}
		else if ( fDist >= SConsts::TILE_SIZE )
		{
			if ( speed >= maxSpeedHere / 2 && !bMaxSlowed )
			{
				if ( bMinSlowed )
					speed = speed - maxPossibleSpeed / 40;
				else
					speed = speed - maxPossibleSpeed / 20;

				bMaxSlowed = true;
			}
			else
			{
				if ( !bMinSlowed && !bMaxSlowed )
					speed = speed + maxPossibleSpeed / 20;
			}
		}
		else
		{
			speed = 0;
			bMaxSlowed = true;
		}
	}

	bNotified = true;
}
void CStandartSmoothMechPath::SlowDown()
{
	if ( !bMinSlowed && !bMaxSlowed )
	{
		bNotified = true;
		speed = Max( pUnit->GetMaxSpeedHere( pUnit->GetCenter() ) / 3.0f, speed - pUnit->GetMaxPossibleSpeed() / 60 );
	}
}
bool CStandartSmoothMechPath::Init( IMemento *pMemento, IBasePathUnit *_pUnit )
{
	NI_ASSERT_T( dynamic_cast<CStandartSmoothPathMemento*>(pMemento) != 0, "Wrong memento passed" );
	
	pUnit = _pUnit;
	CStandartSmoothPathMemento* pMem = static_cast<CStandartSmoothPathMemento*>(pMemento);
	pPath = pMem->pPath;
	bCanGoForward = bCanGoBackward = true;
	lastCheckToRightTurn = 0;

	if ( pPath != 0 )
	{
		pPath->RecoverState( pUnit->GetCenter(), pUnit->GetLastKnownGoodTile() );
		return Init( pUnit, pPath, bSmoothTurn );
	}
	else
		return false;
}
IMemento* CStandartSmoothMechPath::GetMemento() const
{
	CStandartSmoothPathMemento *pMemento = new CStandartSmoothPathMemento();
	pMemento->pPath = pPath;
	pMemento->pFormation = 0;

	return pMemento;
}
bool CStandartSmoothMechPath::CanGoBackward() const
{
	if ( pPath != 0 )
	{
		if ( bCanGoBackward && !bCanGoForward )
			return true;
		else
			return pPath->CanGoBackward( pUnit );
	}
	else
		return false;
}
