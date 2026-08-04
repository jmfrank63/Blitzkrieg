#include "StdAfx.h"

#include "PresizePath.h"
#include "BasePathUnit.h"
#include "TankPitPath.h"
#include "StandartPath.h"
#include "StandartSmoothSoldierPath.h"
#include "StandartSmoothMechPath.h"
CPresizePath::CPresizePath( IBasePathUnit *_pUnit, const class CVec2 &vEndPoint, const class CVec2 &vEndDir )
:	vEndPoint( vEndPoint ), vEndDir( vEndDir ), eState( EPPS_APPROACH_BY_STANDART ), fSpeedLen( 0.0f ),
	pUnit( _pUnit )
{
	wDesiredDir = GetDirectionByVector(vEndDir);

	if ( pUnit->GetRotateSpeed() == 0.0f )
		pPathStandart = new CStandartSmoothSoldierPath();
	else
		pPathStandart = new CStandartSmoothMechPath();

	pPathStandart->SetOwner( pUnit );
}
bool CPresizePath::Init( IBasePathUnit *_pUnit, IPath *pPath, bool bSmoothTurn, bool bCheckTurn )
{
	pUnit =_pUnit;
	CPtr<IPath> p = pPath;
	pPathStandart->Init( pUnit, pPath, bSmoothTurn, bCheckTurn );
	eState = EPPS_APPROACH_BY_STANDART;

	return true;
}
bool CPresizePath::InitByFormationPath( CFormation *pFormation, IBasePathUnit *_pUnit )
{
	pUnit = _pUnit;
	if ( eState == EPPS_APPROACH_BY_STANDART)
		pPathStandart->InitByFormationPath( pFormation, pUnit );

	return true;
}
bool CPresizePath::Init( IMemento *pMemento, IBasePathUnit *_pUnit ) 
{
	CPtr<IMemento> p = pMemento;
	pUnit = _pUnit;
	if ( eState == EPPS_APPROACH_BY_STANDART && pMemento )
		pPathStandart->Init( pMemento, pUnit );

	return true;
}
bool CPresizePath::IsFinished() const
{
	return EPPS_FINISHED == eState;
}
const CVec3 CPresizePath::GetPoint( NTimer::STime timeDiff )
{
	if ( pUnit && !pPathStandart->GetOwner() )
		pPathStandart->SetOwner( pUnit );

	switch ( eState )
	{
		case EPPS_WAIT_FOR_INIT:
			break;
		case EPPS_APPROACH_BY_STANDART:
			if ( !pPathStandart->IsFinished() )
				return pPathStandart->GetPoint( timeDiff );
			else
				eState = EPPS_TURN_TO_DESIRED_POINT;

			break;
		case EPPS_TURN_TO_DESIRED_POINT:
			{
				WORD dir = GetDirectionByVector( vEndPoint - pUnit->GetCenter() );
				dir = !pUnit->GetRightDir() ? dir : dir+65535/2;
				if ( pUnit->TurnToDir( dir ) )
				{
					pPathCheat = new CTankPitPath( pUnit, pUnit->GetCenter(), vEndPoint );
					eState = EPPS_APPROACH_DESIRED_POINT;
				}
			}
			break;
		case EPPS_APPROACH_DESIRED_POINT:
			if ( !pPathCheat->IsFinished() )
				return pPathCheat->GetPoint( timeDiff );
			else
				eState = EPPS_TURN_TO_DESIRED_DIR;

			break;
		case EPPS_TURN_TO_DESIRED_DIR:
			if ( pUnit->TurnToDir( wDesiredDir, true ) )
				eState = EPPS_FINISHED;

			break;
	}

	return CVec3( pUnit->GetCenter(), pUnit->GetZ() );
}
void CPresizePath::Stop()
{
	if ( eState == EPPS_APPROACH_BY_STANDART)
		pPathStandart->Stop();
}
float& CPresizePath::GetSpeedLen()
{ 
	if ( eState == EPPS_APPROACH_BY_STANDART)
		return pPathStandart->GetSpeedLen(); 
	if ( eState == EPPS_APPROACH_DESIRED_POINT)
		return pPathCheat->GetSpeedLen();
	return fSpeedLen;
}
void CPresizePath::NotifyAboutClosestThreat( IBasePathUnit *pCollUnit, const float fDist ) 
{
	if ( eState == EPPS_APPROACH_BY_STANDART)
		pPathStandart->NotifyAboutClosestThreat( pCollUnit, fDist ) ;
}
void CPresizePath::SlowDown()
{
	if ( eState == EPPS_APPROACH_BY_STANDART )
		pPathStandart->SlowDown();
}
bool CPresizePath::CanGoBackward() const 
{ 
	if ( eState == EPPS_APPROACH_BY_STANDART)
		return pPathStandart->CanGoBackward();
	return false;
}
bool CPresizePath::CanGoForward() const
{
	if ( eState == EPPS_APPROACH_BY_STANDART)
		return pPathStandart->CanGoForward();
	return false;
}
void CPresizePath::GetNextTiles( std::list<SVector> *pTiles ) 
{
	if ( eState == EPPS_APPROACH_BY_STANDART)
		pPathStandart->GetNextTiles( pTiles ) ;
}
CVec2 CPresizePath::GetShift( const int nToShift ) const 
{ 
	if ( eState == EPPS_APPROACH_BY_STANDART)
		return pPathStandart->GetShift( nToShift );
	return VNULL2;
}
IMemento* CPresizePath::GetMemento() const 
{
	if ( eState == EPPS_APPROACH_BY_STANDART)
		return pPathStandart->GetMemento();
	return 0;
}
void CPresizePath::SetOwner( IBasePathUnit *_pUnit )
{ 
	pUnit = _pUnit;
	
	if ( pPathStandart.IsValid() )
		pPathStandart->SetOwner( pUnit );
	if ( pPathCheat.IsValid() )
		pPathCheat->SetOwner( pUnit );
}
