#include "stdafx.h"

#include "PlaneStates.h"
#include "AIStaticMap.h"
#include "AIUnit.h"
#include "Building.h"
#include "GroupLogic.h"
#include "Guns.h"
#include "UnitGuns.h"
#include "Commands.h"
#include "Formation.h"
#include "Technics.h"
#include "Aviation.h"
#include "UnitsIterators2.h"
#include "UnitsIterators.h"
#include "AILogic.h"
#include "Soldier.h"
#include "UnitCreation.h"
#include "Guns.h"
#include "Shell.h"
#include "Updater.h"
#include "Turret.h"
#include "ShootEstimatorInternal.h"
#include "PlanePath.h"
#include "General.h"
#include "..\Formats\fmtMap.h"
#include "Scripts\scripts.h"
#include "Weather.h"
#include "MPLog.h"
extern CWeather theWeather;
extern CScripts *pScripts;
extern CSupremeBeing theSupremeBeing;
extern CUpdater updater;
extern CStaticMap theStaticMap;
extern CUnitCreation theUnitCreation;
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
extern CGroupLogic theGroupLogic;
void InitPathToPoint( const CVec3 &vPoint, const bool bSmooth, CAviation * pPlane )
{
	CPlanesFormation * pFormation = pPlane->GetPlanesFormation();
	if ( pFormation )
	{
		pFormation->SendAlongPath( new CPlanePath( CVec3( pFormation->GetCenter(), pPlane->GetZ() ), vPoint ) );
	}
	else
		pPlane->GetCurPath()->Init( pPlane, 
													new CPlanePath( CVec3(pPlane->GetCenter(),pPlane->GetZ()), vPoint ),
																					bSmooth
													);
}
CPtr<CPlaneStatesFactory> CPlaneStatesFactory::pFactory = 0;

IStatesFactory* CPlaneStatesFactory::Instance()
{
	if ( pFactory == 0 )
		pFactory = new CPlaneStatesFactory();

	return pFactory;
}
bool CPlaneStatesFactory::CanCommandBeExecuted( class CAICommand *pCommand )
{
	const EActionCommand &cmdType = pCommand->ToUnitCmd().cmdType;
	return 
		(
			cmdType == ACTION_COMMAND_DIE			||
			cmdType == ACTION_MOVE_PLANE_BOMB_POINT ||
			cmdType == ACTION_COMMAND_DISAPPEAR ||
			cmdType == ACTION_MOVE_FIGHTER_PATROL ||
			cmdType == ACTION_MOVE_SHTURMOVIK_PATROL ||
			cmdType == ACTION_MOVE_PLANE_LEAVE ||
			cmdType == ACTION_MOVE_PLANE_SCOUT_POINT ||
			cmdType == ACTION_MOVE_DROP_PARATROOPERS ||
			cmdType == ACTION_COMMAND_PLANE_ADD_POINT ||
			cmdType == ACTION_COMMAND_PLANE_TAKEOFF_NOW ||
			cmdType == ACTION_MOVE_FLY_DEAD
		);
}
IUnitState* CPlaneStatesFactory::ProduceState( class CQueueUnit *pObj, CAICommand *pCommand )
{
	NI_ASSERT_T( dynamic_cast<CAviation*>( pObj ) != 0, "Wrong unit type" );
	CAviation *pUnit = static_cast<CAviation*>( pObj );
	
	const SAIUnitCmd &cmd = pCommand->ToUnitCmd();
	IUnitState* pResult = 0;
	
	switch ( cmd.cmdType )
	{
		case ACTION_MOVE_FLY_DEAD:
			pResult = CPlaneFlyDeadState::Instance( pUnit );
			
			break;
		case ACTION_COMMAND_DIE:
			NI_ASSERT_T( false, "Command to die in the queue" );

			break;
		case ACTION_MOVE_PLANE_SCOUT_POINT:
			pResult = CPlaneScoutState::Instance( pUnit, cmd.vPos );

			break;
		case ACTION_COMMAND_PLANE_TAKEOFF_NOW:
			{
				NI_ASSERT_T( 0 != dynamic_cast<CPlanePatrolState*>(pUnit->GetState()), NStr::Format("wrong plane state name %i takeoff now", pUnit->GetState()->GetName()) );
				CPlanePatrolState * pPatrol = static_cast<CPlanePatrolState*>(pUnit->GetState());
				pPatrol->TakeOff();
				pResult = pUnit->GetState();
			}
			break;
		case ACTION_COMMAND_PLANE_ADD_POINT:
			{
				NI_ASSERT_T( 0 != dynamic_cast<CPlanePatrolState*>(pUnit->GetState()), NStr::Format( "wrong plane state name %i add point", pUnit->GetState()->GetName() ) );
				CPlanePatrolState * pPatrol = static_cast<CPlanePatrolState*>(pUnit->GetState());
				pPatrol->AddPoint( cmd.vPos );
				pResult = pUnit->GetState();
			}

			break;
		case ACTION_MOVE_PLANE_LEAVE:
			{
				pResult = CPlaneLeaveState::Instance( pUnit, cmd.fNumber );
			}	

			break;
		case ACTION_MOVE_DROP_PARATROOPERS:
			pResult = CPlaneParaDropState::Instance( pUnit, cmd.vPos );
			
			break;
		case ACTION_MOVE_PLANE_BOMB_POINT:
			pResult = CPlaneBombState::Instance( pUnit, cmd.vPos );

			break;
		case ACTION_MOVE_SHTURMOVIK_PATROL:
			pResult = CPlaneShturmovikPatrolState::Instance( pUnit, cmd.vPos, int(cmd.fNumber) );
			break;

		case ACTION_MOVE_FIGHTER_PATROL:
			pResult = CPlaneFighterPatrolState::Instance( pUnit, cmd.vPos );
			break;
	}

	return pResult;
}
IUnitState* CPlaneStatesFactory::ProduceRestState( class CQueueUnit *pUnit )
{
	return CPlaneLeaveState::Instance( static_cast<CAviation*>( pUnit ), static_cast<CAviation*>( pUnit )->GetAviationType() );
}
IUnitState* CPlaneRestState::Instance( CAviation *_pPlane, float fHeight )
{
	return new CPlaneRestState( _pPlane, fHeight == -1.0? _pPlane->GetZ() : fHeight );
}
void CPlaneRestState::InitTriangle( CAviation *pPlane, const CVec3 &startVertex )
{
	vertices[0] = startVertex;
	
	float fTurnRadius = static_cast<const SMechUnitRPGStats*>( pPlane->GetStats() )->fTurnRadius;

	vertices[1].x = vertices[0].x + 2 * fTurnRadius;
	vertices[1].y = vertices[0].y;
	vertices[1].z = vertices[0].z ;

	vertices[2].x = vertices[0].x + fTurnRadius;
	vertices[2].y = vertices[0].y + fTurnRadius;
	vertices[2].z = vertices[0].z ;
}
CPlaneRestState::CPlaneRestState( CAviation *_pPlane, float fHeight )
: pPlane( _pPlane ), fHeight( fHeight )
{
	const CVec2 &center = pPlane->GetCenter();
	const float fR = static_cast<const SMechUnitRPGStats*>( pPlane->GetStats() )->fTurnRadius;
	const float fZ = pPlane->GetZ();

	if ( center.x < 2*fR || center.y < 2 * fR || 
			 center.x > SConsts::TILE_SIZE * ( theStaticMap.GetSizeX() - 1 ) - 4 * fR ||
			 center.y > SConsts::TILE_SIZE * ( theStaticMap.GetSizeY() - 1 ) - 4 * fR )
	{
		vertices[0].x = Min( Max( center.x, 2 * fR ), SConsts::TILE_SIZE * ( theStaticMap.GetSizeX() - 1 ) - 4 * fR );
		vertices[0].y = Min( Max( center.y, 2 * fR ), SConsts::TILE_SIZE * ( theStaticMap.GetSizeY() - 1 ) - 4 * fR );
		vertices[0].z = fHeight;

		InitTriangle( pPlane, vertices[0] );

		curVertex = 0;

		pPlane->GetCurPath()->Init( pPlane, new CPlanePath( CVec3( center.x,center.y,fZ), vertices[0] ), true );
	}
	else
	{
		InitTriangle( pPlane, CVec3( center.x,center.y,fZ) );

		curVertex = 1;		
		pPlane->GetCurPath()->Init( pPlane, new CPlanePath( CVec3( center.x,center.y,fZ), vertices[1] ), true );
	}
}
void CPlaneRestState::Segment()
{
	if ( pPlane->GetCurPath()->IsFinished() )
	{
		curVertex = ( curVertex + 1 ) % 3;
		CVec2 ce( pPlane->GetCenter() );
		pPlane->GetCurPath()->Init( pPlane, new CPlanePath( CVec3(ce.x,ce.y,pPlane->GetZ()), vertices[curVertex] ), true );
	}
	
}
ETryStateInterruptResult CPlaneRestState::TryInterruptState( class CAICommand *pCommand )
{
	pPlane->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CPlaneRestState::GetPurposePoint() const
{
	if ( pPlane && pPlane->IsValid() && pPlane->IsAlive() )
		return pPlane->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
void CPlanePatrolState::AddPoint( const CVec2 &vAddPoint )
{
	vPatrolPoints.push_back( vAddPoint );
}
	
IUnitState* CPlaneScoutState::Instance( CAviation *pPlane, const CVec2 &point )
{
	return new CPlaneScoutState( pPlane, point );
}
CPlaneScoutState::CPlaneScoutState ( CAviation *_pPlane, const CVec2 &point ) 
: CPlanePatrolState( _pPlane, point ), CPlaneDeffensiveFire( _pPlane ),
	eState( _WAIT_FOR_TAKEOFF ), fPatrolHeight( _pPlane->GetZ() ), timeOfStart( curTime )
{
}
void CPlaneScoutState::ToTakeOffState()
{
	RegisterPoints( SUCAviation::AT_SCOUT );
	eState = EPSS_GOTO_GUARDPOINT;
}
void CPlaneScoutState::Segment()
{
	if ( _WAIT_FOR_TAKEOFF != eState && theWeather.IsActive() )
		Escape( SUCAviation::AT_SCOUT );
	AnalyzeBSU();

	if ( EPSS_ESCAPE != eState && curTime - timeOfStart > SConsts::FIGHTER_PATROL_TIME  )
	{
		Escape( SUCAviation::AT_SCOUT );
		eState = EPSS_ESCAPE;
	}

	switch( eState )
	{
	case _WAIT_FOR_TAKEOFF:
		if ( pPlane->GetNextCommand() )
			pPlane->SetCommandFinished();

		break;
	case EPSS_GOTO_GUARDPOINT:
		InitPathToPoint( CVec3( GetPoint(), fPatrolHeight), true, pPlane );
		eState = EPSS_GOING_TO_GUARDPOINT;	

		break;
	case EPSS_GOING_TO_GUARDPOINT:
		if ( pPlane->GetCurPath()->IsFinished() )
			eState = EPSS_AIM_TO_NEXT_POINT;

		break;
	case EPSS_AIM_TO_NEXT_POINT:
		ToNextPoint();
		if ( 1 != GetNPoints() )
			pPlane->SendAcknowledgement( ACK_PLANE_REACH_POINT_START_ATTACK, true );
		eState = EPSS_GOTO_GUARDPOINT;
		break;
	case EPSS_ESCAPE:
		break;
	}
}
ETryStateInterruptResult CPlaneScoutState::TryInterruptState( class CAICommand *pCommand )
{
	UnRegisterPoints();
	pPlane->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
CPlaneDeffensiveFire::CPlaneDeffensiveFire( class CAviation *pPlane ) 
: timeLastBSUUpdate( 0 ), pOwner( pPlane )
{  
	pShootEstimator = new CPlaneDeffensiveFireShootEstimator( pPlane );
}
void CPlaneDeffensiveFire::AnalyzeBSU()
{
	{
		timeLastBSUUpdate = curTime;
		const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats*>(pOwner->GetStats());
		const int nGuns = pOwner->GetNGuns();
		for ( int i = 0; i < nGuns; ++i )
		{
			CBasicGun *pGun = pOwner->GetGun( i );
			CTurret *pTurret = pGun->GetTurret();
			if ( pTurret && !pGun->IsFiring() ) 
			{
				if ( !pTurret->IsLocked( pGun ) )
				{
					pShootEstimator->Reset( 0, true, 0 );
					pShootEstimator->SetGun( pGun );
					for( CPlanesIter planes; !planes.IsFinished(); planes.Iterate() )
					{
						CAviation *pEnemy = *planes;
						if ( pOwner!= pEnemy && 
								 EDI_ENEMY == theDipl.GetDiplStatus( pOwner->GetPlayer(), pEnemy->GetPlayer() ) )
						{
							pShootEstimator->AddUnit( pEnemy );
						}
					}
					if ( pShootEstimator->GetBestUnit() )
						pGun->StartEnemyBurst( pShootEstimator->GetBestUnit(), true );
				}
				else
				{
					pGun->StartEnemyBurst( pTurret->GetTracedUnit(), false );
				}
			}
		}
	}
}
CPlanePatrolState::CPlanePatrolState( CAviation *pPlane, const CVec2 &point )
	: pPlane( pPlane ), nCurPointIndex( 0 )
{
	AddPoint( point /*+ pPlane->GetGroupShift()*/ );
}
void CPlanePatrolState::InitPathByCurDir( const float fDesiredHeight )
{
	CPlanesFormation * pFormation = pPlane->GetPlanesFormation();
	if ( pFormation )
	{
	}
	else
	{
		const CVec3 vCur( pPlane->GetCenter(),pPlane->GetZ() );
		const CVec2 vDir( pPlane->GetDirVector() );
		const CVec3 vDest( vCur.x + vDir.x * 1000, vCur.y + vDir.y * 1000, fDesiredHeight );
		pPlane->GetCurPath()->Init( pPlane, new CPlanePath( vCur, vDest ), true );
	}
}
void CPlanePatrolState::RegisterPoints( const int /*SUCAviation::AIRCRAFT_TYPE*/ nPlaneType )
{
}
void CPlanePatrolState::UnRegisterPoints()
{
}
void CPlanePatrolState::Escape( const int /*SUCAviation::AIRCRAFT_TYPE*/ nAviaType )
{
	pPlane->SetCommandFinished();
	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_MOVE_PLANE_LEAVE, float(nAviaType) ), pPlane, false );
}
CAviation * CPlanePatrolState::FindBetterEnemiyPlane( CAviation * pEnemie, const float fRadius ) const
{
	CPtr<CAviation> pBetter; 
	CPlanesIter planes;
	float Dist = 0;
	const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats*>(pPlane->GetStats());
	while ( !planes.IsFinished() )
	{
		if ( pPlane != (*planes) )
		{
			float curDist = fabs2( pPlane->GetCenter() - (*planes)->GetCenter() );

			CVec2 ce((*planes)->GetCenter());
			
			float distFromPatrolpoint = fabs2( ce - GetPoint() );
			CVec2 planeToEnemie( (*planes)->GetCenter() - pPlane->GetCenter() );
			float scalarProduct = pPlane->GetDirVector() * planeToEnemie;
  		
			if ( distFromPatrolpoint < sqr( fRadius ) && //near guard point
					 theDipl.GetDiplStatus( pPlane->GetPlayer(), (*planes)->GetPlayer() ) == EDI_ENEMY &&//enemie
					 (*planes)->GetZ() <  pStats->fMaxHeight &&					// in our height range
					 ( !pBetter || Dist < curDist ) && //nearer then former or new enemie
						pStats->fMaxHeight >= (*planes)->GetZ() &&
						DirsDifference( pPlane->GetDir(), (*planes)->GetDir()) < /*30deg*/ 65535.0f/360.0f*30.0f &&
						scalarProduct > 0 
				 )
			{
				int nGuns = pPlane->GetNGuns();
				bool bCanBreak = false;
				for ( int i=0; i< nGuns && !bCanBreak; ++i )
				{
					bCanBreak = pPlane->GetGun( i )->CanBreakArmor( *planes ); //can damage
				}
				if ( bCanBreak )
				{
					Dist = curDist;
					pBetter = (*planes);
				}
			}
		}
		planes.Iterate();
	}
	if( pBetter != 0 && pBetter.GetPtr() != pEnemie )
	{
		return pBetter;
	}
	return pEnemie;
}
CAviation * CPlanePatrolState::FindNewEnemyPlane( const float fRadius ) const
{
	CPtr<CAviation> pEnemie;
	CPlanesIter planes;
	pEnemie=0;
	
	float Dist=0;
	const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats*>(pPlane->GetStats());
	while ( !planes.IsFinished() )
	{
		if ( pPlane != (*planes) )
		{
			float curDist = fabs2( pPlane->GetCenter() - (*planes)->GetCenter() );

			CVec2 ce((*planes)->GetCenter());
			float distFromPatrolpoint = fabs2(  ce - GetPoint() );
  		
			if ( distFromPatrolpoint < sqr( fRadius ) && //near guard point
					 theDipl.GetDiplStatus( pPlane->GetPlayer(), (*planes)->GetPlayer() ) == EDI_ENEMY &&//enemie
					 ( !pEnemie || Dist < curDist ) && //nearer then former or new enemie
						pStats->fMaxHeight >= (*planes)->GetZ()//can reach by height
				 )
			{
				int nGuns = pPlane->GetNGuns();
				bool bCanBreak = false;
				for ( int i = 0; i< nGuns && !bCanBreak; ++i )
				{
					bCanBreak = pPlane->GetGun( i )->CanBreakArmor( *planes ); //can damage
				}
				if ( bCanBreak )
				{
					Dist = curDist;
					pEnemie = (*planes);
				}
			}
		}
		planes.Iterate();
	}
	return pEnemie;
}
IUnitState* CPlaneBombState::Instance( CAviation *pPlane, const CVec2 &point  )
{
	return new CPlaneBombState( pPlane, point );
}
CPlaneBombState::CPlaneBombState( CAviation *_pPlane, const CVec2 &_point )
: CPlanePatrolState( _pPlane, _point ), CPlaneDeffensiveFire( _pPlane ),
	eState( _WAIT_FOR_TAKEOFF ), fInitialHeight ( _pPlane->GetZ() ),
	bDiveInProgress( false ), fFormerVerticalSpeed( 0.0f ), bExitDive( false ),
	timeOfStart( curTime ), bDive( false ), fStartAttackDist( 0.0f )
{
}
void CPlaneBombState::ToTakeOffState()
{
	RegisterPoints( SUCAviation::AT_BOMBER );
	eState = ECBS_ESTIMATE;
}
void CPlaneBombState::Segment()
{
	if ( _WAIT_FOR_TAKEOFF != eState )
	{
		if ( ECBS_START_ESACPE != eState && 
			( curTime - timeOfStart > SConsts::FIGHTER_PATROL_TIME || theWeather.IsActive() ) )
			Escape( SUCAviation::AT_BOMBER );
	}
	AnalyzeBSU();
	
	switch ( eState )
	{
	case _WAIT_FOR_TAKEOFF:
		if ( pPlane->GetNextCommand() )
			pPlane->SetCommandFinished();

		break;
	case ECBS_ESTIMATE:
		{
			const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats *>( pPlane->GetStats() );
			const float fTurnRadius = pStats->fTurnRadius;
			bDive = pStats->wDivingAngle >= SConsts::ANGLE_DIVEBOMBER_MIN_DIVE;
			if ( bDive )
			{
				const float fVertTurnRadius = fTurnRadius / SConsts::DIVEBOMBER_VERT_MANEUR_RATIO;
				const float fDiveAngle = pStats->wDivingAngle * 2.0f * PI / 65535;
				const float fBetta = ( PI - fDiveAngle ) / 2.0f;
				fStartAttackDist = (fVertTurnRadius / tan( fBetta )) + (pPlane->GetZ() / tan( fDiveAngle )) + pStats->fSpeed * 3 * SConsts::AI_SEGMENT_DURATION;


				/*const float fVertTurnRadius = fTurnRadius / SConsts::DIVEBOMBER_VERT_MANEUR_RATIO;
				const float fDiveAngle = pStats->wDivingAngle * 2.0f * PI / 65535;
				const float f1 = fabs( pPlane->GetZ() / tan( fDiveAngle ) );
				float f2 = fVertTurnRadius * tan( fDiveAngle / 2 );
				fStartAttackDist =  f1 + f2;*/
			}
			else //для стратегических бомберов
			{
				CVec3 vSpeed3;
				pPlane->GetSpeed3( &vSpeed3 );
				const CVec2 vCenter( pPlane->GetCenter() );
				const CVec3 vOffset =  CBombBallisticTraj::CalcTrajectoryFinish( CVec3(vCenter,pPlane->GetZ()), vSpeed3, VNULL2 );
				fStartAttackDist = fabs( vOffset.x - vCenter.x, vOffset.y - vCenter.y );
				const int nGuns = pPlane->GetNGuns();
				for ( int i = 0; i < nGuns; ++i )
				{
					CBasicGun *pGun = pPlane->GetGun( i );
					if ( pGun->GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_BOMB )
					{
						const SWeaponRPGStats * pStats = pGun->GetWeapon();
						fStartAttackDist += pPlane->GetSpeedLen() * 
																( 
																	pStats->fAimingTime + 
																	Min(pStats->nAmmoPerBurst,pGun->GetNAmmo()) * pGun->GetFireRate()
																) / 2.0f;

						break;
					}
				}
			}

			InitPathByCurDir( fInitialHeight );
			if ( bDive && fabs2(pPlane->GetCenter() - GetPoint()) <= sqr(fStartAttackDist) )
				eState = ECBS_GAIN_HEIGHT;
			else
				eState = ECBS_APPROACH;
		}

		break;
	case ECBS_GAIN_HEIGHT:
		if ( CPlaneSmoothPath::IsHeightOK( pPlane, fInitialHeight) )
			eState = ECBS_GAIN_DISTANCE;

		break;
	case ECBS_GAIN_DISTANCE:
		{
			const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats *>( pPlane->GetStats() );
			if ( fabs2( pPlane->GetCenter() - GetPoint() ) > sqr( fStartAttackDist + pStats->fTurnRadius * 2 ) )
			{
				InitPathToPoint( CVec3(GetPoint(),fInitialHeight), true, pPlane );
				eState = ECBS_APPROACH;
			}
		}
		
		break;
	case ECBS_APPROACH:
		{
			CVec2 vPlaneCenter;
			if ( pPlane->GetPlanesFormation() )
				vPlaneCenter = pPlane->GetPlanesFormation()->GetCenter();
			else
				vPlaneCenter = pPlane->GetCenter();

			if ( fabs2( vPlaneCenter - GetPoint() ) <= sqr( fStartAttackDist ) )
			{
				CPlanesFormation * pFormation = pPlane->GetPlanesFormation();
				if ( pFormation )
					eState = ECBS_WAIT_BOMBPOINT_REACH;
				else
				{
					
					pPlane->SendAcknowledgement( ACK_PLANE_REACH_POINT_START_ATTACK, true );
					if ( bDive )
					{
						InitPathToPoint( CVec3( GetPoint(), 0 ), false, pPlane );
						eState = ECBS_ATTACK_DIVE;
						bExitDive = false;
						fFormerVerticalSpeed = 0.0f;
					}
					else
						eState = ECBS_ATTACK_HORISONTAL;
				}
			}
		}		

		break;
	case ECBS_WAIT_BOMBPOINT_REACH:
		{
			CPlanesFormation * pFormation = pPlane->GetPlanesFormation();
			const CVec2 vBombDropPoint = pFormation->GetCenter() + 
																	 pFormation->GetDirVector() * 
																	 ( - pFormation->GetBombPointOffset() + fStartAttackDist );
			const CVec2 vDirToBombPoint = vBombDropPoint - GetPoint();

			if ( DirsDifference( GetDirectionByVector(vDirToBombPoint), pFormation->GetDir() ) < 65535 / 4  )
			{			
				eState = ECBS_ATTACK_HORISONTAL;
			}
		}
		break;

	case ECBS_ATTACK_HORISONTAL:
		{
			int nGun = pPlane->GetNGuns();
			for ( int i = 0; i < nGun; ++i )
			{
				CBasicGun *pGun = pPlane->GetGun( i );
				if ( pGun->GetNAmmo() != 0 )
				{
					if ( pGun->GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_BOMB )
					{
						if ( !pGun->IsFiring() && 0 == pGun->GetRestTimeOfRelax() )
						{
							pGun->StartPointBurst( pPlane->GetCenter(), false );
						}
					}
				}
			}
			eState = ECBS_AIM_TO_NEXT_POINT;
		}
		
		break;
	case ECBS_ATTACK_DIVE:
		{
			CVec2 vHorVerSpeed( pPlane->GetSpeedHorVer() );
			if ( !bDiveInProgress && fabs2(vHorVerSpeed.y) > fabs2(vHorVerSpeed.x) )
			{
				bDiveInProgress = true;
				updater.Update( ACTION_NOTIFY_MOVE, pPlane, pPlane->GetMovingType() );
			}
			
			if ( bDiveInProgress && vHorVerSpeed.y > 0 )
			{
				bDiveInProgress = false;
				updater.Update( ACTION_NOTIFY_MOVE, pPlane, pPlane->GetMovingType() );
			}

			CVec2 vDist = CVec2( GetPoint() ) - pPlane->GetCenter();
			if ( pPlane->GetZ() <= SConsts::PLANE_MIN_HEIGHT && !bExitDive )
			{
				bExitDive = true;
				const CVec2 vPoint( pPlane->GetCenter() + pPlane->GetDirVector() * SConsts::SHTURMOVIK_APPROACH_RADIUS );
				pPlane->GetCurPath()->Init( pPlane, 
																		new CPlanePath( CVec3(pPlane->GetCenter(), pPlane->GetZ()),
																										CVec3(vPoint,fInitialHeight) ),
																		true );
			}

			bool bDropped = false;
			const int nGun = pPlane->GetNGuns();
			for ( int i = 0; i < nGun; ++i )
			{
				CBasicGun *pGun = pPlane->GetGun( i );
				if ( pGun->GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_BOMB )
				{
					if ( fFormerVerticalSpeed < vHorVerSpeed.y ) // выход из пикирования
					{
						if ( !pGun->IsFiring() && pGun->GetRestTimeOfRelax() == 0 && pGun->GetNAmmo() != 0 )
						{
							CVec3 vSpeed3;
							pPlane->GetSpeed3( &vSpeed3 );
							const CVec3 vCurPoint3( pPlane->GetCenter(), pPlane->GetZ() );
							const CVec3 vTrajFinish( CBombBallisticTraj::CalcTrajectoryFinish( vCurPoint3, vSpeed3, VNULL2 ) );
							if ( DirsDifference( 
																		GetDirectionByVector( GetPoint().x - vTrajFinish.x, GetPoint().y - vTrajFinish.y ),
																		pPlane->GetDir()
																	) > 65535 / 4 )
							{
								pGun->StartPointBurst( pPlane->GetCenter(), false );
								bDropped = true;
							}
						}
					}
				}
				else if ( vHorVerSpeed.y < 0 )// при снижении - стрелять из пулеметов
				{
					if ( 	!pGun->IsFiring() && 0 == pGun->GetRestTimeOfRelax() )
					{
						CVec2 vShoot( pPlane->GetCenter() +
													pPlane->GetDirVector() * pPlane->GetZ() * vHorVerSpeed.x/fabs(vHorVerSpeed.y) );
						pGun->StartPointBurst( vShoot, false );
					}
				}
			}

			fFormerVerticalSpeed = vHorVerSpeed.y;
			if ( bDropped )
				eState = ECBS_AIM_TO_NEXT_POINT;
		}

		break;
	case ECBS_AIM_TO_NEXT_POINT:
		{
			bool bFiring = false;
			const int nGun = pPlane->GetNGuns();
			for ( int i = 0; i < nGun; ++i )
			{
				CBasicGun *pGun = pPlane->GetGun( i );
				if ( SWeaponRPGStats::SShell::TRAJECTORY_BOMB == pGun->GetShell().trajectory && pGun->IsFiring() )
				{
					bFiring = true;
					break;
				}
			}
			if ( !bFiring )
				eState = ECBS_AIM_TO_NEXT_POINT_2;
		}

		break;
	case ECBS_AIM_TO_NEXT_POINT_2:
		{
			bool bAmmoPresent = false;
			const int nGun = pPlane->GetNGuns();
			for ( int i = 0; i < nGun; ++i )
			{
				CBasicGun *pGun = pPlane->GetGun( i );
				if ( SWeaponRPGStats::SShell::TRAJECTORY_BOMB == pGun->GetShell().trajectory &&
							0 != pGun->GetNAmmo() )
				{
					bAmmoPresent = true;
					break;
				}
			}
			if ( bAmmoPresent )
			{
				ToNextPoint();
				InitPathByCurDir( fInitialHeight );
				eState = ECBS_GAIN_HEIGHT;
			}
			else
				eState = ECBS_START_ESACPE;
		}
		break;
	case ECBS_START_ESACPE:
		if ( bDiveInProgress )
		{
			bDiveInProgress = false;
			updater.Update( ACTION_NOTIFY_MOVE, pPlane, pPlane->GetMovingType() );
		}

		Escape( SUCAviation::AT_BOMBER );
		break;
	}
}
ETryStateInterruptResult CPlaneBombState::TryInterruptState( class CAICommand *pCommand )
{
	UnRegisterPoints();
	pPlane->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}

IUnitState* CPlaneParaDropState::Instance( CAviation *pPlane, const CVec2 &vPos )
{
	return new CPlaneParaDropState( pPlane, vPos );
}
CPlaneParaDropState::CPlaneParaDropState ( CAviation *pPlane, const CVec2 &vPos ) 
: CPlanePatrolState( pPlane, vPos ), eState( _WAIT_FOR_TAKEOFF ),
	CPlaneDeffensiveFire( pPlane ), nSquadNumber( 0 ), nDroppingSoldier( 0 ),
	vLastDrop( VNULL2 )
{
}
bool CPlaneParaDropState::CanDrop( const CVec2 & point )
{
	if ( !theStaticMap.IsTileInside( AICellsTiles::GetTile( point ) )) return true;
	theStaticMap.MemMode();
	theStaticMap.SetMode( ELM_ALL );


	SVector centerTile = AICellsTiles::GetTile( point );

	SVector landPoint;

	if ( theStaticMap.IsTileInside( centerTile ) && !theStaticMap.IsLocked( centerTile, AI_CLASS_HUMAN ) )
	{
		theStaticMap.RestoreMode();
		return true ;//found
	}
	else
	{
		for ( int i= centerTile.x-SConsts::PARADROP_SPRED; i< centerTile.x+SConsts::PARADROP_SPRED; ++i )
		{
			for ( int j = centerTile.y-SConsts::PARADROP_SPRED; j< centerTile.y+SConsts::PARADROP_SPRED; ++j )
			{
				if ( theStaticMap.IsTileInside( i, j ) && !theStaticMap.IsLocked( i, j, AI_CLASS_HUMAN ) )
				{
					theStaticMap.RestoreMode();
					return true;//found
				}
			}
		}
	}
	
	theStaticMap.RestoreMode();
	return false;
}
void CPlaneParaDropState::Segment()
{
	if ( _WAIT_FOR_TAKEOFF != eState && PPDS_DROPPING != eState && theWeather.IsActive() ) 
		Escape( SUCAviation::AT_PARADROPER );

	AnalyzeBSU();

	switch( eState )
	{
	case _WAIT_FOR_TAKEOFF:
		if ( pPlane->GetNextCommand() )
			pPlane->SetCommandFinished();

		break;
	case PPDS_ESTIMATE:
		InitPathToPoint( CVec3( GetPoint(), -1 ), true, pPlane );
		eState = PPDS_APPROACHNIG;

		break;
	case PPDS_APPROACHNIG:
		if ( pPlane->GetCurPath()->IsFinished() )
			eState = PPDS_PREPARE_TO_DROP;

		break;
	case PPDS_PREPARE_TO_DROP:
		{
			CVec3 where( pPlane->GetCenter().x, pPlane->GetCenter().y, pPlane->GetZ() );
			const int nPlaneScriptID = pScripts->GetScriptID( pPlane );
			const int nParadropScriptID  = GetGlobalVar( "ParadropSquad.ScriptID", -1 );

			if ( pPlane->GetPlayer() == theDipl.GetNeutralPlayer() )
				TryInterruptState( 0 );
			else
			{
				pSquad = theUnitCreation.CreateParatroopers( where, pPlane, nParadropScriptID );
				vLastDrop = CVec2( -1,-1 );
				eState = PPDS_DROPPING;
				nDroppingSoldier = 0;
				pPlane->SendAcknowledgement( ACK_PLANE_REACH_POINT_START_ATTACK, true );
				updater.Update( ACTION_NOTIFY_NEW_UNIT, pSquad );
			}
		}


		break;
	case PPDS_TO_NEXT_POINT:
		if ( nSquadNumber < theUnitCreation.GetNParadropers( pPlane->GetPlayer() ) )
			eState = PPDS_ESTIMATE;
		else
			Escape( SUCAviation::AT_PARADROPER );
		
		break;
	case PPDS_DROPPING:
		if ( fabs2 ( vLastDrop - pPlane->GetCenter() ) > sqr(SConsts::PLANE_PARADROP_INTERVAL) )
		{
			CPtr<CSoldier> pDropper = 0;
			for ( int i = 0; i< pSquad->Size(); ++i )
			{
				if ( (*pSquad)[i]->IsValid() && (*pSquad)[i]->IsAlive() &&  (*pSquad)[i]->IsInSolidPlace() )
				{
					pDropper = (*pSquad)[i];
					break;
				}
			}
			if ( !pDropper )
			{
				++nSquadNumber;
				eState = PPDS_TO_NEXT_POINT;
			}
			else
			{
				updater.Update( ACTION_NOTIFY_NEW_FORMATION, pDropper );

				vLastDrop = pPlane->GetCenter() ;
				const CVec2 vDropPoint = vLastDrop + 2.0f * ( nDroppingSoldier % 2 - 0.5f ) * 
					Random( SConsts::PLANE_PARADROP_INTERVAL_PERP_MIN, SConsts::PLANE_PARADROP_INTERVAL_PERP_MAX ) *
					GetVectorByDirection( pPlane->GetDir() + 65535 / 4 );
				bool bSafeLanding = CanDrop( vDropPoint );
				if ( bSafeLanding )
				{
					
					pDropper->SetFree();
					pDropper->SetNewCoordinates( CVec3( vDropPoint, pPlane->GetZ() ) );
					pDropper->GetState()->TryInterruptState( 0 );

					theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_PARACHUTE), pDropper, false );
					++nDroppingSoldier;
				}
			}
		}

		break;
	}
}
ETryStateInterruptResult CPlaneParaDropState::TryInterruptState( CAICommand *pCommand )
{
	if ( IsValidObj( pSquad ) )
	{
		std::list<CSoldier*> onboardSoldiers;
		for ( int i = 0; i < pSquad->Size(); ++i )
		{
			CSoldier *pSold = (*pSquad)[i];
			if ( pSold->IsValid() && pSold->IsAlive() && pSold->IsInSolidPlace() )
				onboardSoldiers.push_back( pSold );
		}
		for ( std::list<CSoldier*>::iterator it = onboardSoldiers.begin(); it != onboardSoldiers.end(); ++it )
			(*it)->Disappear();
	}			
	UnRegisterPoints();
	pPlane->SetCommandFinished();

	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CPlaneParaDropState::GetPurposePoint() const
{
	return GetPoint();
}
void CPlaneParaDropState::ToTakeOffState()
{
	RegisterPoints( SUCAviation::AT_PARADROPER );
	eState = PPDS_ESTIMATE;
}
IUnitState* CPlaneLeaveState::Instance( CAviation *_pPlane, const int nAviaType )
{
	return new CPlaneLeaveState( _pPlane, nAviaType );
}
CPlaneLeaveState::CPlaneLeaveState( CAviation *_pPlane, const int _nAviaType )
: pPlane( _pPlane ), eState( EPLS_STARTING ), CPlaneDeffensiveFire( _pPlane ), nAviaType( _nAviaType )
{ 
}
void CPlaneLeaveState::Segment()
{
	AnalyzeBSU();
	switch( eState )
	{
	case EPLS_STARTING:
		{
			pPlane->SendAcknowledgement( ACK_PLANE_LEAVING, true );
			const CVec3 finishPoint ( theUnitCreation.GetRandomAppearPoint( pPlane->GetPlayer(), true ),
																static_cast<const SMechUnitRPGStats*>(pPlane->GetStats())->fMaxHeight );
  		InitPathToPoint( finishPoint, true, pPlane );
			eState = EPLS_IN_ROUTE;
		}

		break;
	case EPLS_IN_ROUTE:
		if( pPlane->GetCurPath()->IsFinished() )
		{
			theUnitCreation.PlaneLandedSafely( pPlane->GetPlayer(), nAviaType );
			pPlane->SetCommandFinished();
			pPlane->Disappear();
		}

		break;
	}
}
ETryStateInterruptResult CPlaneLeaveState::TryInterruptState( class CAICommand *pCommand )
{
	pPlane->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CPlaneLeaveState::GetPurposePoint() const
{
	if ( pPlane && pPlane->IsValid() && pPlane->IsAlive() )
		return pPlane->GetCurPath()->GetFinishPoint();
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CPlaneFighterPatrolState::Instance( CAviation *_pPlane, const CVec2 &point )
{
	return new CPlaneFighterPatrolState( _pPlane, point );
}
class CAIUnit* CPlaneFighterPatrolState::GetTargetUnit() const 
{ 
	return pEnemie;
}
bool CPlaneFighterPatrolState::IsAttacksUnit() const 
{ 
	return IsValidObj( pEnemie );
}
CPlaneFighterPatrolState::CPlaneFighterPatrolState ( CAviation *_pPlane, const CVec2 &vPoint )
: CPlanePatrolState( _pPlane, vPoint ), CPlaneDeffensiveFire( _pPlane ),
	fPatrolHeight( _pPlane->GetZ() ), eState( _WAIT_FOR_TAKEOFF ), 
	timeOfStart( curTime ), timeLastCheck( curTime ), timeOfLastPathUpdate( 0 )
{ 
	const SMechUnitRPGStats *pStats = static_cast<const SMechUnitRPGStats*>( _pPlane->GetStats() );
	fPartolRadius = SConsts::PLANE_GUARD_STATE_RADIUS + pStats->fTurnRadius;
}
void CPlaneFighterPatrolState::ToTakeOffState()
{
	RegisterPoints( SUCAviation::AT_FIGHTER );
	eState = ECFS_GOTO_GUARDPOINT;
}
bool CPlaneFighterPatrolState::IsEnemyAlive( const CAviation *pEnemie ) const 
{
	return pEnemie->IsValid() && pEnemie->IsAlive() &&
		EDI_ENEMY == theDipl.GetDiplStatus( pPlane->GetPlayer(), pEnemie->GetPlayer() ) ;
}
void CPlaneFighterPatrolState::Segment()
{
	AnalyzeBSU();

	if ( _WAIT_FOR_TAKEOFF != eState )
	{
		if ( ECFS_ESCAPE != eState && ( curTime - timeOfStart > SConsts::FIGHTER_PATROL_TIME || theWeather.IsActive() ) )
			Escape( SUCAviation::AT_FIGHTER );

		if ( ECFS_ESCAPE != eState && timeLastCheck - curTime > SConsts::TIME_QUANT )
		{
			int nGuns = pPlane->GetNGuns();
			bool bAmmoRemains = false;
			for ( int i=0; i< nGuns; ++i )
			{
				CBasicGun *pGun = pPlane->GetGun( i );
				if ( pGun->GetNAmmo() != 0 )
				{
					bAmmoRemains = true;
					break;
				}
			}
			if ( !bAmmoRemains  )
				Escape( SUCAviation::AT_FIGHTER );
		}
	}

	switch ( eState )
	{
	case _WAIT_FOR_TAKEOFF:
		if ( pPlane->GetNextCommand() )
			pPlane->SetCommandFinished();

		break;
	case ECFS_GOTO_GUARDPOINT:
		InitPathToPoint( CVec3( GetPoint(), fPatrolHeight ), true, pPlane );
		eState = ECFS_GOING_TO_GUARDPOINT;	

		break;
	case ECFS_FIND_ENEMY_OR_NEXT_POINT:
		if ( pEnemie = FindNewEnemyPlane( fPartolRadius ) )
			eState = ECFS_ENGAGE_TARGET;
		else
			eState = ECFS_AIM_TO_NEXT_POINT;

		break;
	case ECFS_GOING_TO_GUARDPOINT:
  	if ( pEnemie = FindNewEnemyPlane( fPartolRadius ) )
			eState = ECFS_ENGAGE_TARGET;
		else if ( pPlane->GetCurPath()->IsFinished() )
			eState = ECFS_AIM_TO_NEXT_POINT;

		break;
	case ECFS_AIM_TO_NEXT_POINT:
		ToNextPoint();
		if ( 1 != GetNPoints() )
			pPlane->SendAcknowledgement( ACK_PLANE_REACH_POINT_START_ATTACK, true );
		eState = ECFS_GOTO_GUARDPOINT;
		
		break;
	case ECFS_ESCAPE:
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_MOVE_PLANE_LEAVE ), pPlane, false );
		
		break;
	case ECFS_ENGAGE_TARGET:
		if ( !IsEnemyAlive( pEnemie ) ||
				 	fabs2( pPlane->GetCenter() - GetPoint() ) > sqr(fPartolRadius) )
		{
			eState = ECFS_FIND_ENEMY_OR_NEXT_POINT;
		}
		else if ( pEnemie != (pEnemie = FindBetterEnemiyPlane(pEnemie, fPartolRadius)) )
		{
			TryInitPathToEnemie( true );
			eState = ECFS_ENGAGE_TARGET;
		}
		else
		{
			TryInitPathToEnemie();

			int nGun = pPlane->GetNGuns();
			for ( int i=0; i< nGun; ++i )
			{
				CBasicGun *pGun = pPlane->GetGun( i );
				if ( !pGun->IsFiring() &&
						 pGun->GetRestTimeOfRelax() == 0 &&
						 pGun->CanShootWOGunTurn( pEnemie, 1 ) )
				{
					pGun->StartPlaneBurst( pEnemie, false );
				}
			}
		}

		break;
	}
}
void CPlaneFighterPatrolState::TryInitPathToEnemie( bool isEnemieNew )
{
	if( isEnemieNew || 
			pPlane->GetCurPath()->IsFinished()||
			curTime - timeOfLastPathUpdate > SConsts::FIGHTER_PATH_UPDATE_TIME )
	{
		const float time = fabs( pEnemie->GetCenter() - pPlane->GetCenter() )/pPlane->GetStats()->fSpeed ;
		const CVec2 enemieProspectivePos( pEnemie->GetCenter() + pEnemie->GetDirVector()*pEnemie->GetStats()->fSpeed*time);
		const CVec3 en( enemieProspectivePos.x, enemieProspectivePos.y, pEnemie->GetZ() );
		TryInitPathToPoint( en, true );
	}
}
void CPlaneFighterPatrolState::TryInitPathToPoint( const CVec3 & v, bool isNewPoint )
{
	bool bByTime = curTime - timeOfLastPathUpdate > SConsts::FIGHTER_PATH_UPDATE_TIME;
	
	if( isNewPoint || bByTime || pPlane->GetCurPath()->IsFinished() )
	{
		timeOfLastPathUpdate = curTime ;
		CVec2 plCenter( pPlane->GetCenter() );
		CVec3 plCenter3( plCenter, pPlane->GetZ() );

		if( fabs2( plCenter3 - v ) > (SConsts::TILE_SIZE * SConsts::TILE_SIZE) )
			InitPathToPoint( v, false, pPlane );
	}
}
ETryStateInterruptResult CPlaneFighterPatrolState::TryInterruptState( class CAICommand *pCommand )
{
	UnRegisterPoints();
	pPlane->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
CPlaneShturmovikPatrolState::CBombEstimator::CBombEstimator( CAviation *pAttacker,
																														 const float _fDamage,
																														 const CVec2 &_vCenter,
																														 const float _fDisp,
																														 const float _fFlyTime )
: bFire( false ), nMechUnits( 0 ), nInfantry( 0 ), 
	fDamage( _fDamage ), fDisp( _fDisp ),	vCenter( _vCenter ), fFlyTime( _fFlyTime )
{
}
bool CPlaneShturmovikPatrolState::CBombEstimator::Collect( CAIUnit * pTry )
{
	if ( !pTry->IsValid() || !pTry->IsAlive() )
		return bFire;

	const CVec2 &vSpeed = pTry->GetSpeed();

	if ( fabs2(pTry->GetCenter() + pTry->GetSpeed() * fFlyTime - vCenter) < sqr(fDisp) )
	{
		if ( pTry->GetStats()->IsInfantry() )
			++nInfantry;
		else if ( !pTry->GetStats()->IsAviation() )
			++nMechUnits;
	}
	if ( nInfantry >= SConsts::MIN_INFANTRY_TO_DROP_BOMBS || nMechUnits >= SConsts::MIN_MECH_TO_DROP_BOMBS )
		bFire = true;
	return bFire;
}
void CPlaneShturmovikPatrolState::CEnemyContainer::SetEnemy( CBuilding * _pBuilding )
{
	if ( pEnemy )
		pEnemy->UpdateTakenDamagePower( -fTakenDamage );
	pEnemy = 0;
	pBuilding = _pBuilding;
}
void CPlaneShturmovikPatrolState::CEnemyContainer::SetEnemy( CAIUnit *pNewEnemy )
{
	if ( pEnemy.GetPtr() == pNewEnemy ) return;
	if ( pEnemy )
		pEnemy->UpdateTakenDamagePower( -fTakenDamage );
	if ( pNewEnemy )
	{
		fTakenDamage = pOwner->GetKillSpeed( pNewEnemy, -1 );
		pNewEnemy->UpdateTakenDamagePower( fTakenDamage );
	}
	pEnemy = pNewEnemy ;
}
bool CPlaneShturmovikPatrolState::CEnemyContainer::CanShootToTarget( class CBasicGun *pGun ) const
{
	if ( IsValidUnit() )
		return pGun->CanShootWOGunTurn( pEnemy, 1 );
	if ( IsValidBuilding() )
		return pGun->CanShootToPointWOMove( GetCenter(), GetZ() );

	return false;
}
void CPlaneShturmovikPatrolState::CEnemyContainer::StartBurst( class CBasicGun *pGun )
{
	if ( IsValidUnit() )
	{
		pGun->StartEnemyBurst( GetEnemy(), false );
	}
	if ( IsValidBuilding() )
		pGun->StartPointBurst( GetCenter(), false );
}
float CPlaneShturmovikPatrolState::CEnemyContainer::GetZ() const 
{
	if ( IsValidUnit() )
		return pEnemy->GetZ();
	if ( IsValidBuilding() )
		return 0;
	NI_ASSERT_T( false, "asked invalid target about Z" );
	return 0;
}
CVec2 CPlaneShturmovikPatrolState::CEnemyContainer::GetCenter() const
{
	if ( IsValidUnit() )
		return pEnemy->GetCenter();
	if ( IsValidBuilding() )
		return pBuilding->GetAttackCenter( pOwner->GetCenter() );
	NI_ASSERT_T( false, "asked invalid target about attack center" );
	return VNULL2;
}
CAIUnit * CPlaneShturmovikPatrolState::CEnemyContainer::GetEnemy()
{
	return IsValidUnit() ? pEnemy : 0;
}
CBuilding * CPlaneShturmovikPatrolState::CEnemyContainer::GetBuilding()
{
	return IsValidBuilding() ? pBuilding : 0;
}
bool CPlaneShturmovikPatrolState::CEnemyContainer::IsValidBuilding() const
{
	return IsValidObj( pBuilding ) &&
		EDI_ENEMY == theDipl.GetDiplStatus( pBuilding->GetPlayer(), pOwner->GetPlayer() ) ;
}
bool CPlaneShturmovikPatrolState::CEnemyContainer::IsValidUnit() const
{
	return IsValidObj( pEnemy ) &&
		EDI_ENEMY == theDipl.GetDiplStatus( pEnemy->GetPlayer(), pOwner->GetPlayer() ) ;
}
bool CPlaneShturmovikPatrolState::CEnemyContainer::IsValid() const
{
	return IsValidUnit() || IsValidBuilding();
}
IUnitState* CPlaneShturmovikPatrolState ::Instance( CAviation *_pPlane, const CVec2 &vPoint, const int /*EGunplaneCalledAs*/ _eCalledAs )
{
	return new CPlaneShturmovikPatrolState( _pPlane, vPoint, _eCalledAs );
}
CPlaneShturmovikPatrolState::CPlaneShturmovikPatrolState ( CAviation *_pPlane, const CVec2 &vPoint, const int _eCalledAs ) 
: CPlanePatrolState( _pPlane, vPoint ),
	CPlaneDeffensiveFire( _pPlane ),
	eState( _WAIT_FOR_TAKEOFF ), 
	timeOfStart( curTime ),
	timeOfLastPathUpdate( curTime ),
	pPlane( _pPlane ),
	fPatrolHeight( _pPlane->GetZ() ),
	timeLastCheck ( curTime ),
	enemie( _pPlane ),
	eCalledAs( EGunplaneCalledAs(_eCalledAs) ),
	vCurTargetPoint( -1.0f, -1.0f, -1.0f ),
	fStartAttackDist( 0.0f ), fFinishAttckDist( 0.0f ), fTurnRadius( 0.0f )
{
	const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats *>( pPlane->GetStats() );
	fTurnRadius = pStats->fTurnRadius;

	const float fVertTurnRadius = fTurnRadius / SConsts::GUNPLANES_VERT_MANEUR_RATIO;
	const float fDiveAngle = pStats->wDivingAngle * 2.0f * PI / 65535;
	const float fBetta = ( PI - fDiveAngle ) / 2.0f;
	fStartAttackDist = (fVertTurnRadius / tan( fBetta )) + (pPlane->GetZ() / tan( fDiveAngle )) + pStats->fSpeed * 3 * SConsts::AI_SEGMENT_DURATION;

	fFinishAttckDist = SConsts::PLANE_MIN_HEIGHT / tan( fDiveAngle );
	pShootEstimator = new CPlaneShturmovikShootEstimator( pPlane );
}
void CPlaneShturmovikPatrolState::ToTakeOffState()
{
	RegisterPoints( SUCAviation::AT_BATTLEPLANE );
	eState = PSPS_GOTO_GUARDPOINT;
}
void CPlaneShturmovikPatrolState::Segment()
{
	AnalyzeBSU();
	
	if ( pPlane->GetZ() < SConsts::PLANE_MIN_HEIGHT + (fPatrolHeight - SConsts::PLANE_MIN_HEIGHT) * SConsts::BOMB_START_HEIGHT &&
			 pPlane->GetSpeedHorVer().y <= 0 )
		TryDropBombs();

	if ( _WAIT_FOR_TAKEOFF != eState )
	{
		if (	PSPS_ESCAPE != eState &&
					PSPS_GAIN_HEIGHT != eState &&
					timeLastCheck - curTime > SConsts::TIME_QUANT )
		{
			const int nGuns = pPlane->GetNGuns();
			bool bAmmoRemains = false;
			for ( int i=0; i< nGuns; ++i )
			{
				CBasicGun *pGun = pPlane->GetGun( i );
				if ( pGun->GetNAmmo() != 0 )
				{
					bAmmoRemains = true;
					break;
				}
			}
			if ( !bAmmoRemains  )
			{
				eState = PSPS_ESCAPE;
			}
		}

		if (	PSPS_ESCAPE != eState &&
					PSPS_GAIN_HEIGHT != eState &&
					( curTime - timeOfStart > SConsts::FIGHTER_PATROL_TIME || theWeather.IsActive() ) )
		{
			eState = PSPS_ESCAPE;
		}
	}

	switch ( eState )
	{
	case _WAIT_FOR_TAKEOFF:
		if ( pPlane->GetNextCommand() )
			pPlane->SetCommandFinished();

		break;
	case PSPS_ESCAPE:
		Escape( eCalledAs == EGCA_GUNPLANE ? SUCAviation::AT_BATTLEPLANE : SUCAviation::AT_BOMBER );

		break;
	case PSPS_GOTO_GUARDPOINT:
		pShootEstimator->SetCurCenter( GetPoint() );
		InitPathToPoint( CVec3( GetPoint(), fPatrolHeight ), true, pPlane );

		eState = PSPS_GOING_TO_GUARDPOINT;	

		break;
	case PSPS_FIND_ENEMY_OR_NEXT_POINT:
		if ( FindNewEnemie() )
		{
			eState = PSPS_APPROACH_TARGET;
		}
		else
		{
			eState = PSPS_AIM_TO_NEXT_POINT;
		}
		
		break;
	case PSPS_GOING_TO_GUARDPOINT:
		if ( FindNewEnemie() )
		{
			eState = PSPS_APPROACH_TARGET;
		}
		else if ( pPlane->GetCurPath()->IsFinished() )
		{
			eState = PSPS_AIM_TO_NEXT_POINT;
		}

		break;
	case PSPS_AIM_TO_NEXT_POINT:
		ToNextPoint();
		if ( 1 != GetNPoints() )
			pPlane->SendAcknowledgement( ACK_PLANE_REACH_POINT_START_ATTACK, true );

		eState = PSPS_GOTO_GUARDPOINT;

		break;
	case PSPS_APPROACH_TARGET:
		if ( !enemie.IsValid() )
		{
			eState = PSPS_FIND_ENEMY_OR_NEXT_POINT;
		}
		else
		{
			const CVec2 vPoint( pPlane->GetCenter() + pPlane->GetDirVector() * SConsts::SHTURMOVIK_APPROACH_RADIUS );
			TryInitPathToPoint( CVec3( vPoint, fPatrolHeight ) );

			eState = PSPS_APPROACHING_TARGET;
		}

		break;
	case PSPS_APPROACHING_TARGET:
		if ( !enemie.IsValid() )
		{
			eState = PSPS_FIND_ENEMY_OR_NEXT_POINT;
		}
		else if ( fabs2( enemie.GetCenter() - pPlane->GetCenter() ) > sqr( fStartAttackDist + fTurnRadius ) &&
							CPlaneSmoothPath::IsHeightOK( pPlane, fPatrolHeight ) )
		{
			TryInitPathToPoint( CVec3( enemie.GetCenter(), fPatrolHeight ) );
			eState = PSPS_APPROACHING_TARGET_TOWARS_IT;
		}

		break;
	case PSPS_APPROACHING_TARGET_TOWARS_IT:
		if ( !enemie.IsValid() ||
			fabs2(enemie.GetCenter() - GetPoint()) > sqr(SConsts::PLANE_GUARD_STATE_RADIUS * 2.0f) )
		{
			eState = PSPS_FIND_ENEMY_OR_NEXT_POINT;
		}
		else 
		{
			if ( curTime - timeOfLastPathUpdate > SConsts::FIGHTER_PATH_UPDATE_TIME )
			{
				const CVec2 vEnemie( enemie.GetCenter() );
				TryInitPathToPoint( CVec3( vEnemie, fPatrolHeight ), true );
			}

			CVec2 vEnemy( enemie.GetCenter() );
			const CVec2 vDiff( vEnemy - pPlane->GetCenter() );
			const float fDiff = fabs2( vDiff );
			if (  fDiff <= sqr( fStartAttackDist ) &&	// пора начинать атаку
						fDiff	> sqr( fFinishAttckDist ) &&		// еще не пора выходить
					 CPlaneSmoothPath::IsHeightOK( pPlane, fPatrolHeight ) )
			{
				if ( 1500 >= DirsDifference( GetDirectionByVector( vDiff ), pPlane->GetDir() ) )
				{
					TryInitPathToEnemie();
					eState = PSPS_ENGAGING_TARGET;
					if ( enemie.IsValidUnit() )
						enemie.GetEnemy()->SendAcknowledgement( ACK_BEING_ATTACKED_BY_AVIATION, true );
				}
			}
			else if ( fDiff	< sqr( fStartAttackDist ) )
			{
				eState = PSPS_FIND_ENEMY_OR_NEXT_POINT;
			}
		}

		break;
	case PSPS_GAIN_HEIGHT:
		if ( CPlaneSmoothPath::IsHeightOK( pPlane, fPatrolHeight ) )
		{
			eState = PSPS_FIND_ENEMY_OR_NEXT_POINT;
		}

		break;
	case PSPS_ENGAGING_TARGET:
		{
			CAIUnit *pEn = enemie.GetEnemy();

			if ( !enemie.IsValid() )
			{
				CAIUnit * pNewEnemy = FindEnemyInFiringSector();
				if ( pNewEnemy )
					enemie.SetEnemy( pNewEnemy );
				else
					pNewEnemy = FindEnemyInPossibleDiveSector();
				if ( pNewEnemy )
					enemie.SetEnemy( pNewEnemy );
			}
		}
		

		if ( !enemie.IsValid() )
		{
			const CVec2 vPoint( pPlane->GetCenter() + pPlane->GetDirVector() * SConsts::SHTURMOVIK_APPROACH_RADIUS );
			TryInitPathToPoint( CVec3( vPoint.x, vPoint.y, fPatrolHeight ) );

			eState = PSPS_FIRE_TO_WORLD;
		}
		else 
		{
			
			if ( pPlane->GetZ() <= SConsts::PLANE_MIN_HEIGHT + CPlaneSmoothPath::CalcCriticalDistance( pPlane->GetSpeedHorVer(), SConsts::GUNPLANES_VERT_MANEUR_RATIO, fTurnRadius ) )
			{
				const CVec2 vPoint( pPlane->GetCenter() + pPlane->GetDirVector() * SConsts::SHTURMOVIK_APPROACH_RADIUS );
				TryInitPathToPoint( CVec3( vPoint.x, vPoint.y, fPatrolHeight ) );
				eState = PSPS_FIRE_TO_WORLD;
				TryBurstAllGunsToPoints();
			}
			else
			{
				TryBurstAllGuns();
				TryInitPathToEnemie();
			}
		}

		
		break;
	case PSPS_FIRE_TO_WORLD:
		if ( pPlane->GetSpeedHorVer().y < 0 )
			TryBurstAllGunsToPoints();
		else
		{
			eState = PSPS_GAIN_HEIGHT;
		}

		break;
	}
}
bool CPlaneShturmovikPatrolState::IsTargetBehind( const CVec2 &vTarget ) const
{
	const CVec2 vDist = vTarget - pPlane->GetCenter();
	return DirsDifference( GetDirectionByVector( vDist ), 
														GetDirectionByVector( pPlane->GetSpeed() ) ) > 65535/4;
}
void CPlaneShturmovikPatrolState::TryBurstAllGunsToPoints()
{
	if ( pPlane->GetSpeedHorVer().y >= 0 ) 
		return;
	
	const int nGun = pPlane->GetNGuns();
	const CVec2 vHorVerSpeed( pPlane->GetSpeedHorVer() );

	const CVec2 vShoot( pPlane->GetCenter() + pPlane->GetDirVector() * pPlane->GetZ() * vHorVerSpeed.x / fabs( vHorVerSpeed.y ) );

	const float fRange = fabs( pPlane->GetCenter() - vShoot );

	for ( int i = 0; i < nGun; ++i )
	{
		CBasicGun *pGun = pPlane->GetGun( i );
		if (	pGun->GetShell().trajectory != SWeaponRPGStats::SShell::TRAJECTORY_BOMB &&
					!pGun->IsFiring() &&
					0 == pGun->GetRestTimeOfRelax() &&
					!pGun->GetTurret() &&
					fRange <= pGun->GetFireRangeMax() )
		{
			pGun->StartPointBurst( vShoot, true );
		}
	}
}
void CPlaneShturmovikPatrolState::TryDropBombs()
{
	const int nGun = pPlane->GetNGuns();
	for ( int i=0; i< nGun; ++i )
	{
		CBasicGun *pGun = pPlane->GetGun( i );
		if ( pGun->GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_BOMB )
		{
			if ( !pGun->IsFiring() &&
					 pGun->GetRestTimeOfRelax() == 0 &&
					 pGun->GetNAmmo() != 0 )
			{
				CVec3 vSpeed3;
				pPlane->GetSpeed3( &vSpeed3 );
				
				const CVec2 vCurPoint2( pPlane->GetCenter() );
				const CVec3 vCurPoint3( vCurPoint2, pPlane->GetZ() );

				const CVec3 vTrajFinish( CBombBallisticTraj::CalcTrajectoryFinish( vCurPoint3, vSpeed3, VNULL2 ) );
				const float fDisp = pGun->GetDispersion() * pPlane->GetZ() / pGun->GetFireRangeMax();

				const float fFlyTime = 1000 * CBombBallisticTraj::GetTimeOfFly( pPlane->GetZ(), vSpeed3.z );
				const float fAddDist = pPlane->GetSpeedLen() * fFlyTime;
				
				CBombEstimator est( pPlane,
														pGun->GetDamage() + pGun->GetDamageRandom(),
														CVec2(vTrajFinish.x, vTrajFinish.y), fDisp, fFlyTime  );
				
				for ( CUnitsIter<0,1> iter( pPlane->GetParty(), EDI_ENEMY, CVec2(vTrajFinish.x, vTrajFinish.y), fDisp + fAddDist );
							!iter.IsFinished(); iter.Iterate() )
				{
					CAIUnit *pUnit = *iter;
					if ( IsValidObj( pUnit ) && est.Collect( pUnit ) ) break;
				}
				
				if ( est.NeedDrop() )
				{
					pGun->StartPointBurst( vTrajFinish, false );
				}
			}
		}
	}
}
void CPlaneShturmovikPatrolState::TryBurstAllGuns()
{
	if ( pPlane->GetSpeedHorVer().y > 0 ) 
		return;
	const int nGun = pPlane->GetNGuns();
	for ( int i = 0; i < nGun; ++i )
	{
		CBasicGun *pGun = pPlane->GetGun( i );
		if ( pGun->GetShell().trajectory != SWeaponRPGStats::SShell::TRAJECTORY_BOMB )
		{
			if ( !pGun->IsFiring() &&
					 pGun->GetRestTimeOfRelax() == 0 &&
					 enemie.CanShootToTarget( pGun ) 
				 )
			{
				
				const CVec2 vTargetDir( fabs(enemie.GetCenter() - pPlane->GetCenter()), 
													enemie.GetZ() - pPlane->GetZ() );
				const WORD diff1 = DirsDifference(  GetDirectionByVector( vTargetDir ),
																						GetDirectionByVector( pPlane->GetSpeedHorVer() ) );
				const WORD diff2 = pGun->GetWeapon()->wDeltaAngle;
				
				if ( diff1 <= diff2 )
				{
					enemie.StartBurst( pGun );
				}
				else
					pGun->StopFire();
			}
		}
	}
}
CAIUnit *CPlaneShturmovikPatrolState::FindEnemyInFiringSector()
{
	const CVec2 vCurpoint2( pPlane->GetCenter() );
	const CVec3 vCurPoint( vCurpoint2.x, vCurpoint2.y, pPlane->GetZ() );

	const int nGuns = pPlane->GetNGuns();
	float fGnutost = 0;
	float fDisp = 0;
	for ( int i = 0; i < nGuns; ++i )
	{
		CBasicGun * pGun = pPlane->GetGun( i );
		if ( pGun->GetNAmmo() != 0 )
		{
			if ( pGun->GetShell().trajectory != SWeaponRPGStats::SShell::TRAJECTORY_BOMB )
				fGnutost = Max( fGnutost, float(pGun->GetWeapon()->wDeltaAngle * 2.0f * PI / 65535) );
		}
	}

	const CVec2 vGnutost( GetVectorByDirection( fGnutost + 65535 * 3 / 4 ) );
	float firingRadius =	Max( fabs( vCurPoint - vCurTargetPoint ) * vGnutost.y, fDisp );

	pShootEstimator->Reset( enemie.GetEnemy(), true, 0 );
	for ( CUnitsIter<1,3> iter( pPlane->GetParty(), EDI_ENEMY, GetPoint(), SConsts::PLANE_GUARD_STATE_RADIUS ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit * pUnit = *iter;
		if ( pUnit->IsValid() && pUnit->IsAlive() && pUnit->IsVisible( pPlane->GetParty()) )
			pShootEstimator->AddUnit( pUnit );
	}
	return pShootEstimator->GetBestUnit();
}
CAIUnit* CPlaneShturmovikPatrolState::FindEnemyInPossibleDiveSector() 
{
	CVec2 vSpeed = pPlane->GetSpeed();
	Normalize( &vSpeed );

	const CVec2 vCenter( pPlane->GetCenter() );
	const float fMinPossibleDivePoint( pPlane->GetZ() / fPatrolHeight * fStartAttackDist );

	const int nGuns = pPlane->GetNGuns();
	float fGnutost = 0;
	float fMaxRange = 0;
	for ( int i = 0; i < nGuns; ++i )
	{
		CBasicGun *pGun = pPlane->GetGun( i );
		if ( pGun->GetNAmmo() != 0 )
		{
			if ( pGun->GetShell().trajectory != SWeaponRPGStats::SShell::TRAJECTORY_BOMB )
			{
				fGnutost = Max( fGnutost, float(pGun->GetWeapon()->wDeltaAngle * 2.0f * PI / 65535) );
				fMaxRange = Max( fMaxRange, pGun->GetFireRangeMax() );
			}
		}
	}

	const float fMaxPossibleDivePoint( fMaxRange + (pPlane->GetZ() -SConsts::PLANE_MIN_HEIGHT)/ fPatrolHeight * fStartAttackDist );
	const float fAvePossibleDivePoint( (fMaxPossibleDivePoint + fMinPossibleDivePoint)*0.5f );
	const float fRadius = fabs( fMaxPossibleDivePoint - fMinPossibleDivePoint );
	
	const CVec2 vAimCenter( vCenter + vSpeed * fAvePossibleDivePoint );

	pShootEstimator->Reset( enemie.GetEnemy(), true, 0 );
	for ( CUnitsIter<1,3> iter( pPlane->GetParty(), EDI_ENEMY, GetPoint(), SConsts::PLANE_GUARD_STATE_RADIUS ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit * pUnit = *iter;
		if ( pUnit->IsValid() && pUnit->IsAlive() && pUnit->IsVisible( pPlane->GetParty()))
			pShootEstimator->AddUnit( pUnit );
	}
	return pShootEstimator->GetBestUnit();
}
bool CPlaneShturmovikPatrolState::FindNewEnemie()
{
	pShootEstimator->Reset( 0, true, 0 );
	for ( CUnitsIter<1,3> iter( pPlane->GetParty(), EDI_ENEMY, GetPoint(), SConsts::PLANE_GUARD_STATE_RADIUS ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = *iter;
		if ( pUnit->IsValid() && pUnit->IsAlive() && pUnit->IsVisible( pPlane->GetParty()) )
			pShootEstimator->AddUnit( pUnit );
	}
		
	enemie.SetEnemy( pShootEstimator->GetBestUnit() );
	if ( !enemie.IsValid() )
	{
		pShootEstimator->CalcBestBuilding();
		enemie.SetEnemy( pShootEstimator->GetBestBuilding() );
	}

	return enemie.IsValid();
}
void CPlaneShturmovikPatrolState::TryInitPathToEnemie()
{
	CVec3 vEn( enemie.IsValid() ? CVec3( enemie.GetCenter(), enemie.GetZ() ) : vCurTargetPoint );
	const float z = pPlane->GetZ();
	vEn.z = ( z <= SConsts::PLANE_MIN_HEIGHT ? SConsts::PLANE_MIN_HEIGHT : vEn.z );
	TryInitPathToPoint( vEn );
}
void CPlaneShturmovikPatrolState::TryInitPathToPoint( const CVec3 &v, bool isNewPoint )
{
	if( isNewPoint ||
			v != vCurTargetPoint ||
			curTime - timeOfLastPathUpdate > SConsts::SHTURMOVIK_PATH_UPDATE_TIME ||
			pPlane->GetCurPath()->IsFinished() )
	{
		vCurTargetPoint = v;
		timeOfLastPathUpdate = curTime ;
		const CVec3 plCenter3( pPlane->GetCenter(), pPlane->GetZ() );
		pPlane->GetCurPath()->Init( pPlane, new CPlanePath( plCenter3, v ), false );
	}
}

ETryStateInterruptResult CPlaneShturmovikPatrolState::TryInterruptState( class CAICommand *pCommand )
{
	pShootEstimator = 0;
	UnRegisterPoints();
	pPlane->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
IUnitState* CPlaneFlyDeadState::Instance( CAviation *pPlane )
{
	return new CPlaneFlyDeadState( pPlane );
}
CPlaneFlyDeadState::CPlaneFlyDeadState ( CAviation *_pPlane )
: pPlane( _pPlane ), eState( EPDS_START_DIVE ), timeStart( curTime ), bExplodeInstantly( true ),
	fHeight( 0.0f ), bFatality( false )
{
	const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats*>( pPlane->GetStats() );
	bFatality = !pStats->szEffectFatality.empty();
	if ( bFatality )
		bExplodeInstantly = Random( 1 );
	if ( !bExplodeInstantly )
		timeStart = curTime + Random( 0, SConsts::DIVE_BEFORE_EXPLODE_TIME );
	
	pPlane->SetPlanesFormation( 0, VNULL2 );
	pPlane->InitAviationPath();
	
	deadZone.Init();
	fHeight = Max(SConsts::PLANE_MIN_HEIGHT,pPlane->GetZ() * 0.8f);
	pPlane->GetCurPath()->Init( pPlane, 
											new CPlanePath( CVec3(pPlane->GetCenter(),pPlane->GetZ()), 
											CVec3(pPlane->GetCenter() + pPlane->GetDirVector() * 1000, fHeight)),
																			true
											);
}
void CPlaneFlyDeadState::CDeadZone::Init() 
{
	fMaxX = theStaticMap.GetSizeX() * SConsts::TILE_SIZE + 3000;
	fMaxY = theStaticMap.GetSizeY() * SConsts::TILE_SIZE + 3000;
	fMinX = -3000;
	fMinY = -3000;
}
void CPlaneFlyDeadState::CDeadZone::AdjustEscapePoint( CVec2 * pPoint )
{
	const float fXMaxDiff = fabs(pPoint->x - fMaxX);
	const float fXMinDiff = fabs(pPoint->x - fMinX);

	const float fYMaxDiff = fabs(pPoint->y - fMaxY);
	const float fYMinDiff = fabs(pPoint->y - fMinY);
	
	const float fXDiff = Min( fXMaxDiff, fXMinDiff );
	const float fYDiff = Min( fYMaxDiff, fYMinDiff );
	if ( fXDiff < fYDiff )
	{
		if ( fXMaxDiff > fXMinDiff )
			pPoint->x = fMinX;
		else
			pPoint->x = fMaxX;
	}
	else
	{
		if ( fYMaxDiff > fYMinDiff )
			pPoint->y = fMinY;
		else
			pPoint->y = fMaxY;
	}
}
bool CPlaneFlyDeadState::CDeadZone::IsInZone( const CVec2 &vPoint )
{
	return vPoint.x < fMinX || vPoint.x > fMaxX || vPoint.y < fMinY || vPoint.y > fMaxY;
}
void CPlaneFlyDeadState::InitPathToNearestPoint()
{
	const WORD wDir( pPlane->GetDir() );
	const CVec2 vDirVector( pPlane->GetDirVector() );

	const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats*>( pPlane->GetStats() );

	CVec2 vPoint( pPlane->GetCenter() );
	deadZone.AdjustEscapePoint( &vPoint );
	
	pPlane->GetCurPath()->Init( pPlane, 
													new CPlanePath( CVec3(pPlane->GetCenter(),pPlane->GetZ()), 
													CVec3(vPoint,Max(SConsts::PLANE_MIN_HEIGHT,pPlane->GetZ() * 0.8f)) ),
																					true
													);

}
void CPlaneFlyDeadState::Segment()
{
	switch( eState )
	{
	case EPDS_START_DIVE:
		{
			if ( bFatality )
			{
				if ( bExplodeInstantly || timeStart < curTime )
				{
					updater.Update( ACTION_NOTIFY_DIE, pPlane, (ANIMATION_DEATH_FATALITY<<16) );
					timeStart = curTime + SConsts::DIVE_AFTER_EXPLODE_TIME;
					eState = EPDS_DIVE;
				}
			}
			else
				eState = EPDS_DIVE;
		}
		break;
	case EPDS_DIVE:
		if ( bFatality )
		{
			if ( timeStart < curTime )
			{
				pPlane->Disappear();
				break;
			}
		}
		else if ( CPlaneSmoothPath::IsHeightOK( pPlane, fHeight ) )
		{
			InitPathToNearestPoint();
			eState = EPDS_WAIT_FINISH_PATH;
		}

		break;
	case EPDS_ESTIMATE:
		eState = EPDS_WAIT_FINISH_PATH;
	
	case EPDS_WAIT_FINISH_PATH:
		if ( deadZone.IsInZone( pPlane->GetCenter()) )
		{
			pPlane->Disappear();
		}
		break;
	}
}
ETryStateInterruptResult CPlaneFlyDeadState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand || ACTION_COMMAND_DISAPPEAR == pCommand->ToUnitCmd().cmdType )
	{
		pPlane->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	return TSIR_YES_WAIT;
}
const CVec2 CPlaneFlyDeadState::GetPurposePoint() const 
{
	if ( pPlane && pPlane->IsValid() && pPlane->IsAlive() )	
		return pPlane->GetCurPath()->GetFinishPoint();
	else
		return CVec2( -1.0f, -1.0f );
}
