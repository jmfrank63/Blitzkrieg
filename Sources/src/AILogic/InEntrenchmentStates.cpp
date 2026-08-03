#include "StdAfx.h"

#include "InEntrenchmentStates.h"
#include "Commands.h"
#include "Soldier.h"
#include "Entrenchment.h"
#include "Updater.h"
#include "UnitStates.h"
#include "Guns.h"
#include "CommonStates.h"
#include "GroupLogic.h"
extern CUpdater updater;
extern NTimer::STime curTime;
extern CGroupLogic theGroupLogic;
CPtr<CInEntrenchmentStatesFactory> CInEntrenchmentStatesFactory::pFactory = 0;

IStatesFactory* CInEntrenchmentStatesFactory::Instance()
{
	if ( pFactory == 0 )
		pFactory = new CInEntrenchmentStatesFactory();

	return pFactory;
}
bool CInEntrenchmentStatesFactory::CanCommandBeExecuted( CAICommand *pCommand )
{
	const EActionCommand &cmdType = pCommand->ToUnitCmd().cmdType;
	return 
		( cmdType == ACTION_COMMAND_DIE						||
			cmdType == ACTION_COMMAND_ATTACK_UNIT		||
			cmdType == ACTION_COMMAND_IDLE_TRENCH		||
			cmdType == ACTION_COMMAND_AMBUSH				||
			cmdType == ACTION_COMMAND_SWARM_ATTACK_UNIT ||
			cmdType == ACTION_COMMAND_DISAPPEAR );
}
IUnitState* CInEntrenchmentStatesFactory::ProduceState( class CQueueUnit *pUnit, CAICommand *pCommand )
{
	NI_ASSERT_T( dynamic_cast<CSoldier*>( pUnit ) != 0, "Wrong unit type" );
	CSoldier *pSoldier = static_cast<CSoldier*>( pUnit );
	
	const SAIUnitCmd &cmd = pCommand->ToUnitCmd();
	IUnitState* pResult = 0;
	bool bSwarmAttack = false;
	
	switch ( cmd.cmdType )
	{
		case ACTION_COMMAND_DIE:
			NI_ASSERT_T( false, "Command to die in the queue" );

			break;
		case ACTION_COMMAND_SWARM_ATTACK_UNIT:
			bSwarmAttack = true;
		case ACTION_COMMAND_ATTACK_UNIT:
			{
				CONVERT_OBJECT_PTR( CAIUnit, pUnit, cmd.pObject, "Wrong unit to attack" );
				pResult = CSoldierAttackInEtrenchState::Instance( pSoldier, pUnit, bSwarmAttack );
			}

			break;
		case ACTION_COMMAND_IDLE_TRENCH:
			pResult = CSoldierRestInEntrenchmentState::Instance( pSoldier, 0 );

			break;
		case ACTION_COMMAND_AMBUSH:
			pResult = CCommonAmbushState::Instance( pSoldier );

			break;
		default:
			NI_ASSERT_T( false, "Wrong command" );
	}

	return pResult;
}
IUnitState* CInEntrenchmentStatesFactory::ProduceRestState( class CQueueUnit *pUnit )
{
	NI_ASSERT_T( dynamic_cast<CSoldier*>( pUnit ) != 0, "Wrong unit type" );
	return CSoldierRestInEntrenchmentState::Instance( static_cast<CSoldier*>( pUnit ), 0 );
}
IUnitState* CSoldierRestInEntrenchmentState::Instance( CSoldier *pSoldier, CEntrenchment *pEntrenchment )
{
	CSoldierRestInEntrenchmentState *pRest = new CSoldierRestInEntrenchmentState( pSoldier );
	pRest->SetUnitTo( pEntrenchment );

	return pRest;
}
CSoldierRestInEntrenchmentState::CSoldierRestInEntrenchmentState( CSoldier *_pSoldier )
: pSoldier( _pSoldier )
{
	startTime = curTime;
	pSoldier->StartCamouflating();
}
void CSoldierRestInEntrenchmentState::SetUnitTo( CEntrenchment *pEntrenchment )
{
	if ( pEntrenchment != 0 && !pSoldier->IsInEntrenchment())
		pSoldier->SetInEntrenchment( pEntrenchment );		
	else
	{
		updater.Update( ACTION_NOTIFY_IDLE_TRENCH, pSoldier );
	}
}
void CSoldierRestInEntrenchmentState::Segment()
{
	if ( pSoldier->IsInFirePlace() )
		pSoldier->AnalyzeTargetScan( 0, false, false );

	if ( pSoldier->IsInFollowState() && fabs( pSoldier->GetCenter() - pSoldier->GetFollowedUnit()->GetCenter() ) >= SConsts::FOLLOW_GO_RADIUS )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_FOLLOW_NOW, pSoldier->GetFollowedUnit() ), pSoldier, false );
	else
		pSoldier->FreezeByState( true );
}
ETryStateInterruptResult CSoldierRestInEntrenchmentState::TryInterruptState( class CAICommand *pCommand )
{
	pSoldier->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CSoldierRestInEntrenchmentState::GetPurposePoint() const
{
	if ( pSoldier && pSoldier->IsValid() && pSoldier->IsAlive() )	
		return pSoldier->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CSoldierAttackInEtrenchState::Instance( CSoldier *pSoldier, CAIUnit *pEnemy, const bool bSwarmAttack )
{
	return new CSoldierAttackInEtrenchState( pSoldier, pEnemy, bSwarmAttack );
}
CSoldierAttackInEtrenchState::CSoldierAttackInEtrenchState( CSoldier *_pSoldier, CAIUnit *_pEnemy, const bool _bSwarmAttack )
: pSoldier( _pSoldier ), pEnemy( _pEnemy ), bFinish( false ), bAim( true ), bSwarmAttack( _bSwarmAttack ),
	nEnemyParty( _pEnemy->GetParty() )
{ 
	if ( !pEnemy->IsAlive() )
		pSoldier->SendAcknowledgement( ACK_INVALID_TARGET, true );
}
void CSoldierAttackInEtrenchState::AnalyzeCurrentState()
{
	if ( pGun->InFireRange( pEnemy ) )
	{
		if ( pGun->CanShootToUnit( pEnemy ) )
		{
			pSoldier->RegisterAsBored( ACK_BORED_ATTACK );
			pGun->StartEnemyBurst( pEnemy, bAim );
			bAim = false;
		}
		else
		{
			pSoldier->SendAcknowledgement( pGun->GetRejectReason() );
			pGun->StopFire();
			pSoldier->UnRegisterAsBored( ACK_BORED_ATTACK );
			damageToEnemyUpdater.UnsetDamageFromEnemy( pEnemy );
			pSoldier->SetCommandFinished();
		}
	}
	else
	{
		pSoldier->SendAcknowledgement( ACK_NOT_IN_FIRE_RANGE );
		pGun->StopFire();
		pSoldier->UnRegisterAsBored( ACK_BORED_ATTACK );
		damageToEnemyUpdater.UnsetDamageFromEnemy( pEnemy );
		pSoldier->SetCommandFinished();
	}
}
void CSoldierAttackInEtrenchState::FinishState()
{
	pSoldier->UnRegisterAsBored( ACK_BORED_ATTACK );
	damageToEnemyUpdater.UnsetDamageFromEnemy( pEnemy );
	pSoldier->SetCommandFinished();
}
void CSoldierAttackInEtrenchState::Segment()
{
	if ( bFinish || !IsValidObj( pEnemy ) )
		FinishState();
	else if ( !pSoldier->GetEntrenchment()->IsAnyAttackers() )
	{
		if ( pGun == 0 )
		{
			pSoldier->ResetShootEstimator( pEnemy, false );
			pGun = pSoldier->GetBestShootEstimatedGun();

			if ( pGun == 0 )
			{
				pSoldier->SendAcknowledgement( pSoldier->GetGunsRejectReason() );				
				FinishState();
			}
		}
		else if ( !pGun->IsFiring() )
		{
			damageToEnemyUpdater.SetDamageToEnemy( pSoldier, pEnemy, pGun );
			if ( !IsValidObj( pEnemy ) ||
					 !pEnemy->IsVisible( pSoldier->GetParty() ) || pEnemy.GetPtr() == pSoldier || bFinish ||
					 pEnemy->GetParty() != nEnemyParty )
			{
				if ( IsValidObj( pEnemy ) && !pEnemy->IsVisible( pSoldier->GetParty() ) )
					pSoldier->SendAcknowledgement( ACK_DONT_SEE_THE_ENEMY );
				
				pGun->StopFire();
				damageToEnemyUpdater.UnsetDamageFromEnemy( pEnemy );
				pSoldier->UnRegisterAsBored( ACK_BORED_ATTACK );
				pSoldier->SetCommandFinished();
			}
			else
				AnalyzeCurrentState();
		}
	}
}
ETryStateInterruptResult CSoldierAttackInEtrenchState::TryInterruptState( class CAICommand *pCommand )
{ 
	if ( !pCommand )
	{
		damageToEnemyUpdater.UnsetDamageFromEnemy( pEnemy );
		pSoldier->UnRegisterAsBored( ACK_BORED_ATTACK );
		pSoldier->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}

	const SAIUnitCmd &cmd = pCommand->ToUnitCmd();
	if ( cmd.cmdType != ACTION_COMMAND_ATTACK_UNIT || cmd.pObject.GetPtr() != pEnemy )
	{
		if ( pGun.IsValid() )
			pGun->StopFire();

		bFinish = true;
		return TSIR_YES_WAIT;
	}
	else
		return TSIR_NO_COMMAND_INCOMPATIBLE;
}
const CVec2 CSoldierAttackInEtrenchState::GetPurposePoint() const
{
	if ( IsValidObj( pEnemy ) )
		return pEnemy->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
CAIUnit* CSoldierAttackInEtrenchState::GetTargetUnit() const
{
	return pEnemy;
}
