#include "stdafx.h"

#include "GeneralArtillery.h"
#include "AIUnit.h"
#include "UnitStates.h"
#include "Guns.h"
#include "GroupLogic.h"

#include "Updater.h"
#include "AntiArtillery.h"
#include "Artillery.h"
#include "GeneralConsts.h"
#include "Diplomacy.h"
#include "Shell.h"
#include "Technics.h"
#include "GeneralInternal.h"
extern NTimer::STime curTime;
extern CGroupLogic theGroupLogic;
extern CUpdater updater;
extern CDiplomacy theDipl;
bool HasTruck( CAIUnit *pUnit )
{
	CAIUnit *pTruck = pUnit->GetTruck();
	return
		pTruck && pTruck->IsValid() && pTruck->IsAlive() && 
		theDipl.GetDiplStatusForParties( pUnit->GetParty(), pTruck->GetParty() ) != EDI_ENEMY;
}
bool CanUnitBombardRegion( CAIUnit *pUnit, const CVec2 &vRegionCenter )
{
	CVec2 vBattlePos;
	if ( ( HasTruck( pUnit ) || pUnit->CanMove() ) && pUnit->DoesReservePosExist() )
		vBattlePos = pUnit->GetBattlePos();
	else
		vBattlePos = pUnit->GetCenter();

	const float fDistToRegion2 = fabs2( vRegionCenter - vBattlePos );
	const float fFireRange2 = sqr( pUnit->GetFirstArtilleryGun()->GetFireRange( 0 ) );

	return fFireRange2 > fDistToRegion2;
}
bool IsArtilleryFree( CAIUnit *pUnit )
{
	return !pUnit->GetState() || 
				 ( pUnit->GetState()->GetName() == EUSN_REST || pUnit->GetState()->GetName() == EUSN_AMBUSH );
}
bool IsArtilleryBeingTowed( CAIUnit *pUnit )
{
	return pUnit->GetState() && pUnit->GetState()->GetName() == EUSN_BEING_TOWED;
}
BASIC_REGISTER_CLASS( CGeneralArtillery );
CGeneralArtillery::CGeneralArtillery( const int _nParty, CGeneral *_pGeneralOwner )
: nParty( _nParty ), pGeneralOwner( _pGeneralOwner )
{
}
void CGeneralArtillery::Segment()
{
	for ( CUnitsList::iterator it = trucks.begin(); it != trucks.end(); )
	{
		if ( IsValidObj( *it ) )
		{
			if ( !checked_cast_ptr<CAITransportUnit*>( *it )->IsMustTow() )
			{
				pGeneralOwner->Give( *it );
				it = trucks.erase( it );
			}
			else
				++it;
		}
		else 
			++it;
	}

	for ( CUnitsList::iterator iter = freeUnits.begin(); iter != freeUnits.end(); )
	{
		CAIUnit *pUnit = *iter;
		const bool bValidUnit = pUnit && pUnit->IsValid() && pUnit->IsAlive();
		if ( !bValidUnit || theDipl.GetDiplStatusForParties( pUnit->GetParty(), nParty ) == EDI_ENEMY )
		{
			if ( bValidUnit )
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), pUnit, false );

			iter = freeUnits.erase( iter );
		}
		else
			++iter;
	}

	std::list<CGeneralArtilleryTask>::iterator iter = tasks.begin();
	while ( iter != tasks.end() )
	{
		if ( iter->IsTaskFinished() )
			iter = tasks.erase( iter );
		else
			(iter++)->Segment();
	}
}
bool CGeneralArtillery::CanBombardRegion( const CVec2 &vRegionCenter )
{
	for ( CUnitsList::iterator iter = freeUnits.begin(); iter != freeUnits.end(); ++iter )
	{
		CAIUnit *pUnit = *iter;
		CVec2 vDir = vRegionCenter - pUnit->GetCenter();
		Normalize( &vDir );

		const CVec2 vNewCenter = vRegionCenter + vDir * 10 * SConsts::TILE_SIZE;
		if ( CanUnitBombardRegion( pUnit, vNewCenter ) )
			return true;
	}

	return false;
}
int CGeneralArtillery::RequestForSupport( const CVec2 &vCenter, const float fRadius, bool bIsAntiArtilleryFight, const int nCellNumber )
{
	std::list<CAIUnit*> bombardmentUnits;
	int nMaxUnits = Min( (int)freeUnits.size(), (int)Random( 4, 8 ) );
	int cnt = 0;
	for ( CUnitsList::iterator iter = freeUnits.begin(); iter != freeUnits.end() && cnt < nMaxUnits; )
	{
		CAIUnit *pUnit = *iter;
		if ( pUnit->GetParty() == nParty && CanUnitBombardRegion( pUnit, vCenter ) && ( IsArtilleryFree( pUnit ) || IsArtilleryBeingTowed( pUnit ) ) )
		{
			bombardmentUnits.push_back( pUnit );
			iter = freeUnits.erase( iter );
			++cnt;
		}
		else
			++iter;
	}

	if ( bombardmentUnits.empty() )
		return 0;
	else
	{
		tasks.push_back( CGeneralArtilleryTask( this, bombardmentUnits, bIsAntiArtilleryFight, vCenter, fRadius, nCellNumber ) );
	}

	return 0;
}
void CGeneralArtillery::CancelRequest( int nRequestID, enum EForceType eType )
{
}
void CGeneralArtillery::SetEnemyContainer( IEnemyContainer *pEnemyContainer )
{
}
bool CGeneralArtillery::EnumEnemy( CAIUnit *pEnemy )
{
	return false;
}
void CGeneralArtillery::TakeTruck( CAIUnit *pUnit )
{
	trucks.push_back( pUnit );
}
void CGeneralArtillery::TakeArtillery( CAIUnit *pUnit )
{
	freeUnits.push_back( pUnit );
}
void CGeneralArtillery::SetCellInUse( const int nResistanceCell, bool bInUse )
{
	pGeneralOwner->SetCellInUse( nResistanceCell, bInUse );
}
CGeneralArtilleryGoToPosition::CGeneralArtilleryGoToPosition( CAIUnit *_pUnit, const CVec2 &_vPos, bool _bToReservePosition )
: pUnit( _pUnit ), vPos( _vPos ), bToReservePosition( _bToReservePosition ), eState( EBS_START ), bFinished( false ), startTime( curTime )
{
	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), pUnit, false );
}
void CGeneralArtilleryGoToPosition::StartState()
{
	if ( !pUnit->DoesReservePosExist() )
	{
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), pUnit, false );
		bFinished = true;
	}
	else if ( !HasTruck( pUnit ) )
	{
		eState = EBS_MOVING;
		if ( pUnit->CanCommandBeExecutedByStats( ACTION_COMMAND_SWARM_TO ) )
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_SWARM_TO, vPos ), pUnit, false );
		else
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_MOVE_TO, vPos ), pUnit, false );
	}
	else
	{
		eState = EBS_WAIT_FOR_TRUCK;
		startTime = curTime;
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_TAKE_ARTILLERY, pUnit ), pUnit->GetTruck(), false );
	}
}
void CGeneralArtilleryGoToPosition::WaitForTruck()
{
	if ( !HasTruck( pUnit ) )
		eState = EBS_START;
	else if ( IsArtilleryFree( pUnit->GetTruck() ) && pUnit->GetState() && IsArtilleryBeingTowed( pUnit ) )
	{
		eState = EBS_MOVING_WITH_TRUCK;
		if ( DoesGoToReservePosition() )
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_MOVE_TO, vPos ), pUnit->GetTruck(), false );
		else
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_DEPLOY_ARTILLERY, vPos ), pUnit->GetTruck(), false );
	}
	else if ( startTime + 2000 < curTime && 
						pUnit->GetTruck()->GetState() && IsRestState( pUnit->GetTruck()->GetState()->GetName() ) )
		bFinished = true;
}
void CGeneralArtilleryGoToPosition::MovingWithTruck()
{
	if ( !HasTruck( pUnit ) )
		eState = EBS_START;
	else if ( IsArtilleryFree( pUnit ) || IsArtilleryFree( pUnit->GetTruck() ) )
	{
		if ( DoesGoToReservePosition() || !pUnit->NeedDeinstall() ) 
			bFinished = true;
		else
		{
			eState = EBS_FINISHING;
			timeOfFinish = curTime;
		}
	}
}
void CGeneralArtilleryGoToPosition::Finishing()
{
	if ( curTime - timeOfFinish >= 1500 )
	{
		NI_ASSERT_T( dynamic_cast<CArtillery*>(pUnit) != 0, "Wrong unit" );
		CArtillery *pArtillery = static_cast<CArtillery*>(pUnit);

		if ( !pArtillery->IsInInstallAction() )
			bFinished = true;
	}
}
void CGeneralArtilleryGoToPosition::Segment()
{
	if ( !bFinished )
	{
		switch ( eState )
		{
			case EBS_START:
				StartState();

				break;
			case EBS_MOVING:
				if ( IsArtilleryFree( pUnit ) )
					bFinished = true;

				break;
			case EBS_WAIT_FOR_TRUCK:
				WaitForTruck();

				break;
			case EBS_MOVING_WITH_TRUCK:
				MovingWithTruck();

				break;
			case EBS_FINISHING:
				Finishing();

				break;
			default:
				NI_ASSERT_T( false, "Wrong state" );

		}
	}
}
CGeneralArtilleryTask::SBombardmentUnitState::SBombardmentUnitState( CAIUnit *_pUnit )
: pUnit( _pUnit )
{
	vReservePosition = pUnit->GetCenter();
}
CGeneralArtilleryTask::CGeneralArtilleryTask( CGeneralArtillery *_pOwner, std::list<CAIUnit*> &givenUnits, bool bAntiArtilleryFight, const CVec2 &vCenter, const float fRadius, const int _nCellNumber )
: pOwner( _pOwner ), bIsAntiArtilleryFight( bAntiArtilleryFight ),
	bBombardmentFinished( false ), eState( EBS_START ), vBombardmentCenter( vCenter ),
	fBombardmentRadius( fRadius ), timeToSendAntiArtilleryAck( 0 ),
	nCellNumber( _nCellNumber ), nParty( _pOwner->GetParty() )
{
	for ( std::list<CAIUnit*>::iterator iter = givenUnits.begin(); iter != givenUnits.end(); ++iter )
	{
		bombardmentUnits.push_back( SBombardmentUnitState( *iter ) );
	}

	pOwner->SetCellInUse( nCellNumber, true );
}
void CGeneralArtilleryTask::SetBombardmentFinished()
{
	bBombardmentFinished = true;
	pOwner->SetCellInUse( nCellNumber, false );
}
void CGeneralArtilleryTask::CheckEscapingUnits()
{
	for ( std::list<SBombardmentUnitState>::iterator iter = bombardmentUnits.begin(); iter != bombardmentUnits.end(); )
	{
		if ( iter->pGoToPosition )
		{
			iter->pGoToPosition->Segment();
			if ( iter->pGoToPosition->IsFinished() )
			{
				pOwner->TakeArtillery( iter->pUnit );
				iter = bombardmentUnits.erase( iter );
			}
			else
				++iter;
		}
		else
			++iter;
	}
}
void CGeneralArtilleryTask::StartBombardment()
{
	const int nUnits = bombardmentUnits.size();
	const float fShift = ( nUnits == 0 ) ? 0.0f : fBombardmentRadius / 2.0f;
	WORD wDir = 0;

	for ( std::list<SBombardmentUnitState>::iterator iter = bombardmentUnits.begin(); iter != bombardmentUnits.end(); ++iter, wDir += 65536.0f / float(nUnits) )
	{
		const CVec2 vCenter = vBombardmentCenter + GetVectorByDirection( wDir ) * fShift;
		CAIUnit *pUnit = iter->pUnit;

		if ( CanUnitBombardRegion( pUnit, vCenter ) )
			iter->vAttackPos = vCenter;
		else
			iter->vAttackPos = vBombardmentCenter;

		CVec2 vBattlePos;
		if ( ( HasTruck( pUnit ) || pUnit->CanMove() ) && pUnit->DoesReservePosExist() )
			vBattlePos = pUnit->GetBattlePos();
		else
			vBattlePos = pUnit->GetCenter();

		iter->pGoToPosition = new CGeneralArtilleryGoToPosition( pUnit, vBattlePos, false );
	}

	eState = EBS_GOING_TO_BATTLE;
}
void CGeneralArtilleryTask::GoingToBattle()
{
	bool bFinished = true;
	for ( std::list<SBombardmentUnitState>::iterator iter = bombardmentUnits.begin(); iter != bombardmentUnits.end(); ++iter )
	{
		if ( iter->pGoToPosition )
		{
			iter->pGoToPosition->Segment();
			if ( iter->pGoToPosition->IsFinished() )
			{
				CAIUnit *pUnit = iter->pUnit;

				if ( IsArtilleryBeingTowed( pUnit ) )
					iter->pGoToPosition = new CGeneralArtilleryGoToPosition( pUnit, iter->vReservePosition, true );
				else
				{
					const CVec2 vCenter = pUnit->GetCenter();
					const float fDistToAttackPos2 = fabs2( vCenter - iter->vAttackPos );
					const float fFireRange2 = sqr( pUnit->GetFirstArtilleryGun()->GetFireRange( 0 ) );

					if ( fFireRange2 <= fDistToAttackPos2 )
						iter->pGoToPosition = new CGeneralArtilleryGoToPosition( pUnit, iter->vReservePosition, true );
					else
						iter->pGoToPosition = 0;
				}
			}
		}

		if ( iter->pGoToPosition && !iter->pGoToPosition->DoesGoToReservePosition() )
			bFinished = false;
	}

	if ( bFinished )
	{
		eState = EBS_ROTATING;
		startRotatingTime = curTime;

		for ( std::list<SBombardmentUnitState>::iterator iter = bombardmentUnits.begin(); iter != bombardmentUnits.end(); ++iter )
		{
			if ( iter->pGoToPosition == 0 )
			{
				CAIUnit *pUnit = iter->pUnit;
				if ( HasTruck( pUnit ) )
				{
					CAIUnit *pTruck = pUnit->GetTruck();
					theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_RESUPPLY, pTruck->GetCenter() + pTruck->GetDirVector() * 2.0f * pTruck->GetStats()->vAABBHalfSize.y ), pTruck, false );
				}
			}
		}
	}
}
void CGeneralArtilleryTask::Rotating()
{
	CheckEscapingUnits();

	if ( startRotatingTime + 1000 < curTime )
	{	
		if ( startRotatingTime != 0 )
		{
			startRotatingTime = 0;

			for ( std::list<SBombardmentUnitState>::iterator iter = bombardmentUnits.begin(); iter != bombardmentUnits.end(); ++iter )
			{
				if ( !iter->pGoToPosition )
				{
					CAIUnit *pUnit = iter->pUnit;
					if ( !pUnit->GetFirstArtilleryGun()->CanShootToPointWOMove( iter->vAttackPos, 0 ) )
							theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_ROTATE_TO, iter->vAttackPos ), pUnit, false );
				}
			}
		}
		else
		{
			for ( std::list<SBombardmentUnitState>::iterator iter = bombardmentUnits.begin(); iter != bombardmentUnits.end(); ++iter )
			{
				if ( !iter->pGoToPosition )
				{
					CAIUnit *pUnit = iter->pUnit;
					if ( pUnit->GetState() && pUnit->GetState()->GetName() == EUSN_TURN_TO_POINT )
						return;
					if ( !pUnit->IsInstalled() )
						return;
				}
			}

			eState = EBS_FIRING;
			NTimer::STime maxTimeToShoot = 0;
			CalculateTimeToSendAntiArtilleryAck();
			for ( std::list<SBombardmentUnitState>::iterator iter = bombardmentUnits.begin(); iter != bombardmentUnits.end(); ++iter )
			{
				if ( !iter->pGoToPosition )
				{
					CAIUnit *pUnit = iter->pUnit;
					
					theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_ART_BOMBARDMENT, iter->vAttackPos ), pUnit, false );

					CBasicGun *pGun = pUnit->GetFirstArtilleryGun();
					const NTimer::STime timeToShoot = pGun->GetAimTime( false ) + pGun->GetRelaxTime( false );

					if ( timeToShoot > maxTimeToShoot )
						maxTimeToShoot = timeToShoot;
				}
			}

			timeToFinishBombardment = curTime + maxTimeToShoot * SGeneralConsts::SHOOTS_OF_ARTILLERY_FIRE;
		}
	}
}
void CGeneralArtilleryTask::Firing()
{
	CheckEscapingUnits();	
	
	if ( curTime >= timeToFinishBombardment )
	{
		eState = EBS_ESCAPING;
		
		for ( std::list<SBombardmentUnitState>::iterator iter = bombardmentUnits.begin(); iter != bombardmentUnits.end(); ++iter )
		{
			if ( !iter->pGoToPosition )
				iter->pGoToPosition = new CGeneralArtilleryGoToPosition( iter->pUnit, iter->vReservePosition, true );
		}
	}
}
void CGeneralArtilleryTask::Escaping()
{
	bool bFinished = true;
	for ( std::list<SBombardmentUnitState>::iterator iter = bombardmentUnits.begin(); iter != bombardmentUnits.end(); )
	{
		iter->pGoToPosition->Segment();
		if ( iter->pGoToPosition->IsFinished() )
		{
			pOwner->TakeArtillery( iter->pUnit );
			iter = bombardmentUnits.erase( iter );
		}
		else
		{
			bFinished = false;
			++iter;
		}
	}
	
	if ( bFinished )
		SetBombardmentFinished();
}
void CGeneralArtilleryTask::CalculateTimeToSendAntiArtilleryAck()
{
	vAntiArtilleryAckCenter = VNULL2;
	int cnt = 0;
	NTimer::STime maxTimeToWait = 0;
	for ( std::list<SBombardmentUnitState>::iterator iter = bombardmentUnits.begin(); iter != bombardmentUnits.end(); ++iter )
	{
		if ( !iter->pGoToPosition )
		{
			++cnt;
			vAntiArtilleryAckCenter += iter->vAttackPos;

			CAIUnit *pUnit = iter->pUnit;
			if ( CBasicGun *pGun = pUnit->GetFirstArtilleryGun() )
			{
				IBallisticTraj *pTraj = pGun->CreateTraj( iter->vAttackPos );
				const NTimer::STime localTimeToWait = 
						pTraj->GetExplTime() - pTraj->GetStartTime() + 
						2 * ( pGun->GetAimTime( false ) + pGun->GetRelaxTime( false ) );

				if ( localTimeToWait > maxTimeToWait )
					maxTimeToWait = localTimeToWait;
			}
		}
	}

	vAntiArtilleryAckCenter /= float( cnt );
	if ( maxTimeToWait != 0 )
		timeToSendAntiArtilleryAck = curTime + maxTimeToWait;
	else
		timeToSendAntiArtilleryAck = 0;
}
void CGeneralArtilleryTask::Segment()
{
	for ( std::list<SBombardmentUnitState>::iterator iter = bombardmentUnits.begin(); iter != bombardmentUnits.end(); )
	{
		CPtr<CAIUnit> pUnit = iter->pUnit;

		bool bWrongParty = false;
		if ( IsValidObj( pUnit ) )
			bWrongParty = !IsArtilleryBeingTowed( pUnit ) && pUnit->GetParty() != nParty;

		if ( !IsValidObj( pUnit ) || bWrongParty )
		{
			if ( bWrongParty )
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), pUnit, false );

			iter = bombardmentUnits.erase( iter );
		}
		else
			++iter;
	}

	if ( bombardmentUnits.empty() )
		SetBombardmentFinished();
	else
	{
		switch ( eState )
		{
			case EBS_START:
				StartBombardment();

				break;
			case EBS_GOING_TO_BATTLE:
				GoingToBattle();

				break;
			case EBS_ROTATING:
				Rotating();

				break;
			case EBS_FIRING:
				{
					Firing();

					if ( bIsAntiArtilleryFight && timeToSendAntiArtilleryAck != 0 && timeToSendAntiArtilleryAck <= curTime )
					{
						NI_ASSERT_T( vAntiArtilleryAckCenter.x >= 0.0f && vAntiArtilleryAckCenter.y >= 0.0f, NStr::Format( "Wrong vAntiArtilleryAckCenter (%g,%g)", vAntiArtilleryAckCenter.x, vAntiArtilleryAckCenter.y ) );
						updater.AddFeedBack( SAIFeedBack( EFB_ENEMY_STARTED_ANTIARTILLERY, MAKELONG( vAntiArtilleryAckCenter.x, vAntiArtilleryAckCenter.y ) ) );
						timeToSendAntiArtilleryAck = 0;
					}
				}

				break;
			case EBS_ESCAPING:
				Escaping();

				break;
			default:
				NI_ASSERT_T( false, NStr::Format( "Wrong bombardment state (%d)", int(eState) ) );
		}
	}
}
