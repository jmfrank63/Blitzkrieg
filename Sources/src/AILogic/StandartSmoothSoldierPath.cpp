#include "StdAfx.h"

#include "BasePathUnit.h"
#include "StandartPath.h"
#include "StandartSmoothSoldierPath.h"
#include "AIStaticMap.h"
#include "Formation.h"
#include "PathFinder.h"
extern CStaticMap theStaticMap;
extern NTimer::STime curTime;
BASIC_REGISTER_CLASS( CStandartSmoothSoldierPath );
CStandartSmoothSoldierPath::CStandartSmoothSoldierPath()
: speed( 0 ),	bNotified( false ), pPath( 0 ), nPoints( 0 ), bStopped( false ), 
	bMinSlowed( false ), bMaxSlowed( false ),
	pUnit( 0 )
{
}
void CStandartSmoothSoldierPath::SetOwner( IBasePathUnit *_pUnit )
{
	pUnit = _pUnit;
}
IBasePathUnit* CStandartSmoothSoldierPath::GetOwner() const
{
	return pUnit;
}
bool CStandartSmoothSoldierPath::Init( IBasePathUnit *_pUnit, IPath *_pPath, bool _bSmoothTurn, bool bCheckTurn )
{
	NI_ASSERT_T( _pPath != 0, "Smooth path is trying to be initialized by NULL static path" );

	pUnit = _pUnit;
	
	bWithFormation = false;
	pPath = _pPath;
	if ( pUnit->GetTile() == AICellsTiles::GetTile( pPath->GetFinishPoint() ) )
	{
		pPath = 0;
		bFinished = true;
		return false;
	}
	else
	{
		predPoint = p0 = p1 = p2 = p3 = pPath->GetStartPoint();

		const int nInitSplineResult = InitSpline();

		fRemain = 0;
		bFinished = false;

		if ( pPath->IsFinished() && spline.GetPoint() == pPath->GetFinishPoint() )
		{
			bFinished = true;
			pPath = 0;
			return false;
		}
		nPoints = 0;

		spline.Iterate();
		bStopped = false;
	}
	
	return true;
}
bool CStandartSmoothSoldierPath::InitByFormationPath( CFormation *_pFormation, interface IBasePathUnit *_pUnit )
{
	pUnit = _pUnit;
	pFormation = _pFormation;
	pPath = 0;
	bFinished = false;

	bStopped = false;

	return true;
}
void CStandartSmoothSoldierPath::GetNextTiles( std::list<SVector> *pTiles )
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
CVec2 CStandartSmoothSoldierPath::GetShift( const int nToShift ) const
{
	NI_ASSERT_T( pPath != 0, "Wrong call of GetShift" );
	return pPath->PeekPoint( nToShift );
}
bool CStandartSmoothSoldierPath::ValidateCurPath( const CVec2 &center, const CVec2 &newPoint )
{
	if ( center.x / ( SConsts::TILE_SIZE ) != newPoint.x / ( SConsts::TILE_SIZE ) ||
			 center.y / ( SConsts::TILE_SIZE ) != newPoint.y / ( SConsts::TILE_SIZE ) )
	{
		CBSpline::SForwardIter iter;
		spline.StartForwardIterating( &iter );

		CVec2 predPoint( iter.x );
		spline.IterateForward( &iter ); 

		while ( iter.t != -1 && fabs2( predPoint - iter.x ) <= sqr( (float)SConsts::TILE_SIZE ) / 4 )
			spline.IterateForward( &iter );

		int i = 1;
		bool bBad = false;
		while ( iter.t != -1 )
		{
			if ( fabs2( predPoint - iter.x ) >= sqr( (float)SConsts::TILE_SIZE ) / 4 )
			{
				if ( !theStaticMap.CanUnitGoToPoint( pUnit->GetBoundTileRadius(), iter.x, pUnit->GetAIClass() ) )
				{
					if ( !bBad )
						bBad = true;
					else
					{
						const float fDist2 = fabs2( center - pPath->GetFinishPoint() );
						if ( fDist2 <= sqr( 3 * pUnit->GetAABBHalfSize().y ) )
						{
							if ( pFormation == 0 )
							{
								bFinished = true;
								Stop();
							}
							pPath = 0;
						}
						else
						{
							pPath->Recalculate( newPoint, pUnit->GetLastKnownGoodTile() );
							Init( pUnit, pPath, false, false );
						}

						return false;
					}
				}
				else
					bBad = false;

				predPoint = iter.x;
			}
			
			++i;
			spline.IterateForward( &iter );
		}
	}

	return true;
}
bool CStandartSmoothSoldierPath::IsFinished() const
{ 
	return
		( bFinished ||
			!pPath && !IsValidObj( pFormation ) ||
			!pPath && IsValidObj( pFormation ) && pFormation->GetCurPath()->IsFinished() );
}
void CStandartSmoothSoldierPath::Stop()
{
	bStopped = true;
	speed = 0;
}
int CStandartSmoothSoldierPath::InitSpline()
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
const CVec2 CStandartSmoothSoldierPath::GetPointWithoutFormation( NTimer::STime timeDiff )
{
	const CVec2 &vUnitCenter = pUnit->GetCenter();
	if ( !pPath || bStopped || bFinished || p0 == p1 && p1 == p2 && p2 == p3 )
	{
		speed = 0;
		if ( !bStopped && !bFinished )
		{
			Stop();
			bFinished = true;
			pPath = 0;
		}

		return vUnitCenter;
	}

	if ( spline.GetDX() != VNULL2 )
		pUnit->UpdateDirection( spline.GetDX() );

	fRemain = speed * timeDiff;

	while ( !bFinished && fabs( spline.GetPoint() - vUnitCenter ) < fRemain ) 
	{
		while ( fabs( spline.GetPoint() - vUnitCenter ) < fRemain && nIter < CBSpline::N_OF_ITERATONS )
		{
			spline.Iterate();
			++nIter;
		}

		if ( pPath->IsFinished() && mDistance( spline.GetPoint(), pPath->GetFinishPoint() ) < 2 )
		{
			pPath = 0;
			speed = fabs( spline.GetPoint() - vUnitCenter ) / timeDiff;
			if ( pFormation == 0 )
			{
				bFinished = true;
				Stop();
			}

			return spline.GetPoint();
		}
		else		
		{
			if ( fabs( spline.GetPoint() - vUnitCenter ) < fRemain && nIter >= CBSpline::N_OF_ITERATONS )
				InitSpline();
		}
	}

	if ( !bFinished )
	{
		speed = Min( speed, pUnit->GetMaxPossibleSpeed() );

		if ( pPath->IsFinished() && mDistance( spline.GetPoint(), pPath->GetFinishPoint() ) < 2 )
		{
			pPath = 0;
			speed = fabs( spline.GetPoint() - vUnitCenter ) / timeDiff;
			if ( pFormation == 0 )
			{
				bFinished = true;
				Stop();
			}

			return spline.GetPoint();
		}
	}

	if ( spline.GetDX() != VNULL2 )
		pUnit->UpdateDirection( spline.GetDX() );
	
	const CVec2 result = 
		fabs( spline.GetPoint() - vUnitCenter ) < fRemain  ?
		spline.GetPoint() :
		vUnitCenter + Norm( spline.GetPoint() - vUnitCenter ) * fRemain;

	speed = fabs( result - vUnitCenter ) / timeDiff;
	
	if ( !bFinished )
		ValidateCurPath( vUnitCenter, result );

	return result;
}
void CStandartSmoothSoldierPath::ValidateCurPathWithFormation( const CVec2 &newCenter )
{
	if ( !theStaticMap.CanUnitGoToPoint( pUnit->GetBoundTileRadius(), newCenter, pUnit->GetAIClass() ) )
	{
		const bool bDrive = DriveToFormation( pUnit->GetCenter(), false );
		if ( !bDrive && pFormation->IsStopped() )
		{
			bFinished = true;
			pFormation = 0;
			pUnit->UpdateDirection( pUnit->GetFormation()->GetUnitDir( pUnit->GetFormationSlot() ) );
			Stop();
		}

		return;
	}

	const SVector vNewCenterTile = AICellsTiles::GetTile( newCenter );
	CVec2 vCenterAhead = newCenter + pUnit->GetDirVector() * SConsts::TILE_SIZE / 2;
	if ( AICellsTiles::GetTile( vCenterAhead ) == vNewCenterTile )
		vCenterAhead += pUnit->GetDirVector() * SConsts::TILE_SIZE / 2;

	if ( !theStaticMap.CanUnitGoToPoint( pUnit->GetBoundTileRadius(), vCenterAhead, pUnit->GetAIClass() ) )
	{
		const bool bDrive = DriveToFormation( newCenter, false );
		if ( !bDrive && pFormation->IsStopped() )
		{
			bFinished = true;
			pFormation = 0;
			pUnit->UpdateDirection( pUnit->GetFormation()->GetUnitDir( pUnit->GetFormationSlot() ) );
			Stop();
		}

		return;
	}
}
void CStandartSmoothSoldierPath::CutDriveToFormationPath( CCommonStaticPath *pPath )
{
	if ( pPath->GetLength() >= 4 )
		pPath->MoveFinishTileTo( pPath->GetLength() / 2 );
}
bool CStandartSmoothSoldierPath::CanGoToFormationPos( const CVec2 &newCenter, const CVec2 &vDesPos, const CVec2 &vFormPos )
{
	if ( theStaticMap.CanUnitGoToPoint( pUnit->GetBoundTileRadius(), vDesPos, pUnit->GetAIClass() ) )
	{
		IStaticPath* _pPath = CreateStaticPathToPoint( newCenter, vDesPos, VNULL2, pUnit );
		CPtr<CCommonStaticPath> pPath;
		if ( _pPath != 0 )
		{
			NI_ASSERT_T( dynamic_cast<CCommonStaticPath*>(_pPath) != 0, "Wrong path created" );
			pPath = static_cast<CCommonStaticPath*>(_pPath);
		}

		if ( pPath != 0 )
		{
			CPtr<IStaticPath> pCheckPath = CreateStaticPathToPoint( pPath->GetFinishPoint(), vFormPos, VNULL2, pUnit );

			if ( pCheckPath != 0 && 
					( pCheckPath->GetLength() * SConsts::TILE_SIZE <= 2.0f * fabs( pFormation->GetUnitShift( pUnit->GetFormationSlot() ) )
					   /*pCheckPath->GetLength() <= 2.0 * pPath->GetLength() / 3.0f */)
				 )
			{
				CutDriveToFormationPath( pPath );
				Init( pUnit, new CStandartPath( pUnit->GetBoundTileRadius(), pUnit->GetAIClass(), pUnit->GetPathFinder(), pPath, newCenter, pPath->GetFinishPoint(), pUnit->GetLastKnownGoodTile() ), false );
				return true;
			}
		}
	}

	return false;
}
bool CStandartSmoothSoldierPath::DriveToFormation( const CVec2 &newCenter, const bool bAnyPoint )
{
	if ( bAnyPoint )
	{
		const CVec2 &vDesPos = pFormation->GetUnitCoord( pUnit->GetFormationSlot() );
		if ( CanGoToFormationPos( newCenter, vDesPos, pFormation->GetCenter() ) )
			return true;
	}

	if ( !pFormation->IsStopped() )
	{
		const CVec2 &vFarFormationPos = pFormation->GetFarFormationPos();
		const CVec2 &vUnitShift = pFormation->GetUnitShift( pUnit->GetFormationSlot() );
		if ( !CanGoToFormationPos( newCenter, vFarFormationPos + vUnitShift, vFarFormationPos ) )
		{
			IStaticPath *_pPath = CreateStaticPathToPoint( newCenter, vFarFormationPos, VNULL2, pUnit );
			CPtr<CCommonStaticPath> pPath;
			if ( _pPath != 0 )
			{
				NI_ASSERT_T( dynamic_cast<CCommonStaticPath*>(_pPath) != 0, "Wrong path created" );
				pPath = static_cast<CCommonStaticPath*>(_pPath);
			}

			if ( pPath != 0 )
			{
				CutDriveToFormationPath( pPath );				
				Init( pUnit, new CStandartPath( pUnit->GetBoundTileRadius(), pUnit->GetAIClass(), pUnit->GetPathFinder(), pPath, newCenter, pPath->GetFinishPoint(), pUnit->GetLastKnownGoodTile() ), false );

				return true;
			}
			else 
				return false;
		}

		return true;
	}
	else
		return false;
}
const CVec2 CStandartSmoothSoldierPath::GetPointWithFormation( NTimer::STime timeDiff, const bool bFirstCall )
{
	if ( pUnit->GetTimeToNextSearchPathToFormation() > curTime )
	{
		speed = 0;
		return pUnit->GetCenter();
	}

	if ( pFormation->IsStopped() )
	{
		Stop();
		if ( pFormation->GetCurPath()->IsFinished() )
			bFinished = true;

		pFormation = 0;		
		speed = 0;
		
		return pUnit->GetCenter();
	}
	
	if ( bStopped || bFinished )
	{
		speed = 0;
		return pUnit->GetCenter();
	}

	CVec2 result = pUnit->GetUnitPointInFormation();
  const float lineShift = pFormation->GetUnitLineShift( pUnit->GetFormationSlot() );
	if ( lineShift > 0 )
		result += pFormation->GetDirVector() * lineShift;

	const float fDist = fabs( result - pUnit->GetCenter() );
	const float fDiff = fDist - speed * timeDiff;

	if ( fDiff > 0 )
		result = pUnit->GetCenter() + ( result - pUnit->GetCenter() ) * ( speed * timeDiff / fDist );

	if ( !theStaticMap.IsPointInside( result ) )
	{
		DriveToFormation( result, false );
		speed = 0;
		return pUnit->GetCenter();
	}
	
	speed = fabs( result - pUnit->GetCenter() ) / timeDiff;

	if ( curTime - pUnit->GetTimeToNextSearchPathToFormation() > 1000 )
		pFormation->NotifyDiff( -lineShift );

	if ( AICellsTiles::GetTile( result ) != AICellsTiles::GetTile( pUnit->GetCenter() ) || bFirstCall )
	{
		const bool bCanGoToPoint = theStaticMap.CanUnitGoToPoint( pUnit->GetBoundTileRadius(), result, pUnit->GetAIClass() );
		if ( lineShift <= 0 && fDiff > 6 * SConsts::TILE_SIZE && bCanGoToPoint )
		{
			if ( DriveToFormation( result, true ) )
				return result;
			else
			{
				speed = 0;
				pUnit->CantFindPathToFormation();
				return pUnit->GetCenter();
			}
		}
		else
		{
			ValidateCurPathWithFormation( result );
			if ( !bCanGoToPoint )
			{
				speed = 0;
				pUnit->CantFindPathToFormation();
				return pUnit->GetCenter();
			}
		}

		if ( pFormation != 0 )
		{
			CPtr<IStaticPath> pCheckPath = CreateStaticPathToPoint( pFormation->GetCenter(), VNULL2, pUnit, true );
			const float fR = fabs( pFormation->GetCenter() - pUnit->GetCenter() );
			if ( fR < SConsts::TILE_SIZE && pCheckPath->GetLength() > 3 ||
					 fR >= SConsts::TILE_SIZE && pCheckPath->GetLength() * SConsts::TILE_SIZE > 3.0f * fR )
				DriveToFormation( result, false );
		}
	}

	if ( pFormation != 0 && fabs2( pUnit->GetCenter() - result ) < sqr( pFormation->GetMaxSpeedHere( pFormation->GetCenter() ) * timeDiff / 1.5f ) )
	{
		speed = 0;
		result = pUnit->GetCenter();
	}
	else if ( result != pUnit->GetCenter() && !bFinished )
		pUnit->UpdateDirection( result - pUnit->GetCenter() );

	return result;
}
const CVec3 CStandartSmoothSoldierPath::GetPoint( NTimer::STime timeDiff )
{
	const bool bFirstCall = speed == 0.0f;
	
	if ( !bNotified )
	{
		if ( pUnit->GetRotateSpeed() == 0 )
			speed = pUnit->GetMaxSpeedHere( pUnit->GetCenter() );
		else
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
		}
	}
	else if ( bMaxSlowed && speed == 0.0f )
	{
		bNotified = bMinSlowed = bMaxSlowed = false;		
		return CVec3( pUnit->GetCenter(), pUnit->GetZ() );
	}
	bNotified = bMinSlowed = bMaxSlowed = false;		

	if ( pPath || !pUnit->IsInFormation() || !IsValidObj( pFormation ) )
	{
		bWithFormation = false;
		const CVec2 vResult = GetPointWithoutFormation( timeDiff );
		return CVec3( vResult, pUnit->GetZ() );
	}
	else
	{
		if ( !bWithFormation )
			bStopped = bFinished = false;

		bWithFormation = true;
		const CVec2 vResult = GetPointWithFormation( timeDiff, bFirstCall );
		return CVec3( vResult, pUnit->GetZ() );
	}
}
void CStandartSmoothSoldierPath::NotifyAboutClosestThreat( IBasePathUnit *pUnit, const float fDist )
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
void CStandartSmoothSoldierPath::SlowDown()
{
	if ( !bMinSlowed && !bMaxSlowed )
	{
		bNotified = true;
		speed = Max( pUnit->GetMaxSpeedHere( pUnit->GetCenter() ) / 3.0f, speed - pUnit->GetMaxPossibleSpeed() / 60 );
	}
}
bool CStandartSmoothSoldierPath::Init( IMemento *pMemento, IBasePathUnit *_pUnit )
{
	NI_ASSERT_T( dynamic_cast<CStandartSmoothPathMemento*>(pMemento) != 0, "Wrong memento passed" );
	
	pUnit = _pUnit;
	CStandartSmoothPathMemento* pMem = static_cast<CStandartSmoothPathMemento*>(pMemento);
	pPath = pMem->pPath;

	if ( pPath != 0 || pFormation != 0 )
	{
		pFormation = pMem->pFormation;

		if ( pPath != 0 )
		{
			pPath->RecoverState( pUnit->GetCenter(), pUnit->GetLastKnownGoodTile() );
			return Init( pUnit, pPath, false );
		}
		else
		{
			NI_ASSERT_T( pFormation != 0, "Wrong smoothpath state" );
			InitByFormationPath( pFormation, pUnit );

			return false;
		}

		return true;
	}
	else
		return false;
}
IMemento* CStandartSmoothSoldierPath::GetMemento() const
{
	CStandartSmoothPathMemento *pMemento = new CStandartSmoothPathMemento();
	pMemento->pPath = pPath;
	pMemento->pFormation = pFormation;

	return pMemento;
}
