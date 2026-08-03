#include "StdAfx.h"

#include "CommonStates.h"
#include "Guns.h"
#include "StaticObject.h"
#include "Building.h"
#include "Entrenchment.h"
#include "GroupLogic.h"
#include "Diplomacy.h"
#include "Soldier.h"
#include "Guns.h"
#include "HitsStore.h"
#include "Turret.h"
#include "Commands.h"
#include "PathFinder.h"
#include "Updater.h"
#include "AckManager.h"
#include "Formation.h"
#include "AckManager.h"
#include "Probability.h"
#include "ShootEstimator.h"
#include "Path.h"
#include "Technics.h"
#include "float.h"
#include "MPLog.h"
#include "Artillery.h"

#include "TimeCounter.h"
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
extern CHitsStore theHitsStore;
extern CGroupLogic theGroupLogic;
extern CUpdater updater;
extern CAckManager theAckManager;

extern CTimeCounter timeCounter;
IUnitState* CMechAttackUnitState::Instance( CAIUnit *pOwner, CAIUnit *pEnemy, bool bAim, const bool bSwarmAttack )
{
	return new CMechAttackUnitState( pOwner, pEnemy, bAim, bSwarmAttack );
}
CMechAttackUnitState::CMechAttackUnitState( CAIUnit *_pOwner, CAIUnit *_pEnemy, bool _bAim, const bool _bSwarmAttack )
: CFreeFireManager( _pOwner ), lastShootCheck( 0 ), lastEnemyTile( -1, -1 ), bAim( _bAim ), wLastEnemyDir( 0 ), 
	pEnemy( _pEnemy ), pUnit( _pOwner ),
	pGun( 0 ), bFinish( false ), bSwarmAttack( _bSwarmAttack ),
	bTurningToBest( false ), nBestAngle( -1 ), lastEnemyCenter( -1, -1 ),
	nEnemyParty( _pEnemy->GetParty() ), vEnemyCenter( _pEnemy->GetCenter() ), wEnemyDir( _pEnemy->GetFrontDir() )
{
	ResetTime( pEnemy );

	if ( bSwarmAttack );
	else
		pUnit->ResetGunChoosing();
}
void CMechAttackUnitState::FireToEnemy()
{
	FireNow();
	bAim = false;
	bTurningToBest = false;
}
void CMechAttackUnitState::StartAgain()
{
	if ( pGun )
	{
		if ( IsValidObj( pEnemy ) )
			pGun->TraceAim( pEnemy );
		pGun->StopFire();
	}
		
	pGun = 0;
	bTurningToBest = false;

	damageToEnemyUpdater.UnsetDamageFromEnemy( pEnemy );
}
void CMechAttackUnitState::TraceAim()
{
	if ( fabs2( pEnemy->GetCenter() - pUnit->GetCenter() ) <= 4 * sqr( pGun->GetFireRangeMax() ) )
	{
		if ( pGun->IsOnTurret() )
			pGun->GetTurret()->Lock( pGun );

		pGun->TraceAim( pEnemy );
	}
}
bool CMechAttackUnitState::TurnToBestPos()
{
	if ( pGun->IsRelaxing() && bTurningToBest && nBestAngle != -1 )
	{
		pUnit->TurnToDir( nBestAngle, false );
		return true;
	}
	else if ( pGun->IsRelaxing() && ( !bTurningToBest || bTurningToBest && nBestAngle != -1 ) )
	{
		if ( pUnit->IsMoving() )
			pUnit->StopUnit();

		if ( DirsDifference( pGun->GetGun().wDirection, 0 ) < 2 &&
				 pGun->IsOnTurret() && !pUnit->NeedDeinstall() && pGun->GetRestTimeOfRelax() > 0 && pUnit->IsIdle() )
		{
			const CVec2 vEnemyCenter = pEnemy->GetCenter();
			if ( !bTurningToBest || fabs2( lastEnemyCenter - vEnemyCenter ) >= 200.0f )
			{
				bTurningToBest = true;
				const WORD wDirToEnemy = GetDirectionByVector( pEnemy->GetCenter() - pUnit->GetCenter() );
				if ( wDirToEnemy != 0 )
				{
					const WORD wFrontDir = pUnit->GetFrontDir();
					const WORD wDirsDiff = DirsDifference( wDirToEnemy, wFrontDir );
					const int wBaseAngle = pGun->GetRestTimeOfRelax() * pUnit->GetRotateSpeed();
					const int wTurretAngle = pGun->GetRestTimeOfRelax() * pGun->GetRotateSpeed();

					const WORD wAngleOfTurn = Min( wDirsDiff, WORD( Min( wBaseAngle, wTurretAngle ) ) );

					if ( DirsDifference( wAngleOfTurn + wFrontDir, wDirToEnemy ) < DirsDifference( wFrontDir - wAngleOfTurn, wDirToEnemy ) )
						nBestAngle = WORD( wAngleOfTurn + wFrontDir );
					else
						nBestAngle = WORD( wFrontDir - wAngleOfTurn );

					if ( DirsDifference( nBestAngle, wFrontDir ) > SConsts::MIN_ROTATE_ANGLE && pUnit->CanTurnToFrontDir( nBestAngle ) )
					{
						pGun->TurnToRelativeDir( pGun->GetGlobalDir() - nBestAngle );
						pUnit->TurnToDir( nBestAngle, false );

						bAim = true;
					}
					else
						nBestAngle = -1;
				}
				else 
					nBestAngle = -1;

				lastEnemyCenter = pEnemy->GetCenter();
			}
			else if ( nBestAngle != -1 )
				pUnit->TurnToDir( nBestAngle, false );
		}
		else
		{
			bTurningToBest = true;
			nBestAngle = -1;
		}

		return true;
	}
	else
		return false;
}
void CMechAttackUnitState::AnalyzeBruteMovingPosition()
{
	bool bTurn = true;
	const bool bIdle = pUnit->IsIdle();
	const bool bEnemyVisible = pEnemy->IsVisible( pUnit->GetParty() );
	const SVector vEnemyTile = AICellsTiles::GetTile( vEnemyCenter );

	if ( bIdle || bTurningToBest || curTime - lastShootCheck >= SHOOTING_CHECK && ( !bEnemyVisible || lastEnemyTile != vEnemyTile ) ||
			 bEnemyVisible && pGun->CanShootToUnitWOMove( pEnemy ) )
	{
		TraceAim();
		
		if ( bEnemyVisible && pGun->CanShootToUnitWOMove( pEnemy ) )
		{
			if ( TurnToBestPos() )
				bTurn = false;
			else
				FireToEnemy();
		}
		else if ( pGun->TooCloseToFire( pEnemy ) )
		{
			pUnit->SendAcknowledgement( ACK_ENEMY_IS_TO_CLOSE );
			StopFire();
		}
		else if ( pUnit->IsInTankPit() )
		{
			StopFire();
		}
		else if ( bIdle || lastEnemyTile != vEnemyTile )
		{
			CPtr<IStaticPath> pStaticPath;
			if ( bEnemyVisible )
				pStaticPath = CreateStaticPathForAttack( pUnit, pEnemy, pGun->GetWeapon()->fRangeMin, pGun->GetFireRange( pEnemy->GetZ() ) );
			else
				pStaticPath = CreateStaticPathToPoint( vEnemyCenter, VNULL2, pUnit );

			bool bSent = false;
			if ( pStaticPath )
			{
				lastEnemyTile = vEnemyTile;
				bAim = true;
				bSent = pUnit->SendAlongPath( pStaticPath, VNULL2 );
			}

			if ( !bSent )
			{
				pUnit->SendAcknowledgement( ACK_CANNOT_FIND_PATH_TO_TARGET );
				StopFire();
			}
		}

		lastEnemyTile = vEnemyTile;
		lastShootCheck = curTime;
	}

	if ( bTurn )
		bTurningToBest = false;
}
bool CMechAttackUnitState::CanShootToEnemyNow() const
{
	return !pGun->IsRelaxing() && pGun->CanShootWOGunTurn( pEnemy, !bAim );
}
void CMechAttackUnitState::AnalyzeMovingPosition()
{
	bTurningToBest = false;
	const bool bIdle = pUnit->IsIdle();
	const bool bEnemyVisible = pEnemy->IsVisible( pUnit->GetParty() );
	const SVector enemyTile = AICellsTiles::GetTile( vEnemyCenter );

	if ( bEnemyVisible && curTime - lastShootCheck >= SHOOTING_CHECK && CanShootToEnemyNow() )
		FireToEnemy();
	else if ( bIdle || curTime - lastShootCheck >= SHOOTING_CHECK && ( !bEnemyVisible || lastEnemyTile != enemyTile ) )
	{
		TraceAim();
		
		if ( !bEnemyVisible || pGun->InGoToSideRange( pEnemy ) )
		{
			nBestSide = -1;
			lastEnemyTile = SVector( -1, -1 );
			
			state = ESAS_MOVING_TO_SIDE;
		}
		else if ( pGun->TooCloseToFire( pEnemy ) )
		{
			pUnit->SendAcknowledgement( ACK_ENEMY_IS_TO_CLOSE );
			StopFire();
		}
		else if ( bIdle || lastEnemyTile != enemyTile )
		{
			CPtr<IStaticPath> pStaticPath;
			if ( bEnemyVisible )
				pStaticPath = CreateStaticPathForAttack( pUnit, pEnemy, pGun->GetWeapon()->fRangeMin, 2 * pGun->GetFireRange(pEnemy->GetZ()) );
			else
				pStaticPath = CreateStaticPathToPoint( vEnemyCenter, VNULL2, pUnit );
			bool bSent = false;
			if ( pStaticPath )
			{
				lastEnemyTile = enemyTile;
				bAim = true;
				bSent = pUnit->SendAlongPath( pStaticPath, VNULL2 );
			}

			if ( !bSent )
			{
				pUnit->SendAcknowledgement( ACK_CANNOT_FIND_PATH_TO_TARGET );
				StopFire();
			}

			lastEnemyTile = pEnemy->GetTile();			
			lastShootCheck = curTime;
		}
	}
}
IStaticPath* CMechAttackUnitState::BestSidePath()
{
	IStaticPath *pBestPath = 0;
	const float fRangeMin = pGun->GetWeapon()->fRangeMin;
	const float fRangeMax = pGun->GetFireRange( pEnemy->GetZ() );

	float fMinLength = 1e10;
	float fBestProbability = -1;

	CVec2 posDir = pEnemy->GetDirVector();

	for ( int i = 0; i < 4; ++i )
	{
		if ( pGun->CanBreach( pEnemy, i ) )
		{
			if ( eAttackType == EAT_GOOD_PROB && fProb[i] >= SConsts::GOOD_ATTACK_RPOBABILITY ||
					 eAttackType == EAT_POOR_PROB && fProb[i] >= fBestProbability )
			{
				IStaticPath *pPath = 
					CreateStaticPathForSideAttack( pUnit, pEnemy, posDir, fRangeMin, fRangeMax, 8.0f * SConsts::TILE_SIZE );

				if ( pPath != 0 )
				{
					if ( eAttackType == EAT_GOOD_PROB && pPath->GetLength() < fMinLength )
					{
						CPtr<IStaticPath> pGarbage = pBestPath;
						pBestPath = pPath;
						fMinLength = pPath->GetLength();
					}
					else if ( eAttackType == EAT_POOR_PROB && ( fProb[i] > fBestProbability || fProb[i] == fBestProbability && pPath->GetLength() < fMinLength ) )
					{
						CPtr<IStaticPath> pGarbage = pBestPath;
						pBestPath = pPath;
						fBestProbability = fProb[i];
						fMinLength = pPath->GetLength();
						nBestSide = i;
					}
					else
						CPtr<IStaticPath> pGarbage = pPath;
				}
			}
		}

		std::swap( posDir.x, posDir.y );
		posDir.x *= -1;
	}

	return pBestPath;
}
void CMechAttackUnitState::AnalyzeMovingToSidePosition()
{
	bool bTurn = true;
	const bool bEnemyVisible = pEnemy->IsVisible( pUnit->GetParty() );
	const SVector enemyTile = AICellsTiles::GetTile( vEnemyCenter );

	if ( !bEnemyVisible && pUnit->IsIdle() && !pEnemy->IsRevealed() )
	{
		StopFire();
		return;
	}

	if ( bEnemyVisible && !pGun->IsRelaxing() && CanShootToEnemyNow() )
		FireToEnemy();
	else
	{
		const bool bEnemyChangedPosition = 
			bEnemyVisible &&
			( lastEnemyTile != enemyTile || DirsDifference( wEnemyDir, wLastEnemyDir ) >= ENEMY_DIR_TOLERANCE );

		if ( pUnit->IsIdle() || bTurningToBest || curTime - lastShootCheck >= SHOOTING_CHECK && bEnemyChangedPosition )
		{
			TraceAim();

			const bool bGoodSideToShoot = 
				bEnemyVisible &&
				eAttackType == EAT_GOOD_PROB && fProb[pEnemy->GetUnitRect().GetSide( pUnit->GetCenter() )] >= SConsts::GOOD_ATTACK_RPOBABILITY ||
				eAttackType == EAT_POOR_PROB && pEnemy->GetUnitRect().GetSide( pUnit->GetCenter() ) == nBestSide;

			if ( bGoodSideToShoot && pGun->CanShootToUnitWOMove( pEnemy ) )
			{
				if ( TurnToBestPos() )
					bTurn = false;
				else
				{
					FireToEnemy();
					nBestAngle = -1;
				}
			}
			else if ( pUnit->IsIdle() || bEnemyChangedPosition )
			{
				if ( !pGun->InGoToSideRange( pEnemy ) )
					state = ESAS_MOVING;
				else 
				{
					CPtr<IStaticPath> pStaticPath;
					if ( bEnemyVisible )
						pStaticPath = BestSidePath();
					else
						pStaticPath = CreateStaticPathToPoint( vEnemyCenter, VNULL2, pUnit );

					bool bSent = false;
					if ( pStaticPath )
					{
						bAim = true;
						bSent = pUnit->SendAlongPath( pStaticPath, VNULL2 );
					}
					else if ( pGun->CanBreakArmor( pEnemy ) )
					{
						state = ESAS_BRUTE_MOVING;
						bSent = true;
					}
					
					if ( !bSent )
					{
						pUnit->SendAcknowledgement( ACK_CANNOT_FIND_PATH_TO_TARGET );
						StopFire();
					}
				}
			}

			lastShootCheck = curTime;			
			lastEnemyTile = enemyTile;
			wLastEnemyDir = wEnemyDir;
		}
	}

	if ( bTurn )
		bTurningToBest = false;
}
bool CMechAttackUnitState::IsBruteMoving()
{
	const int nMinPossiblePiercing = pGun->GetMinPossiblePiercing();
	return
		!pUnit->CanMove() ||
		pEnemy->GetArmor(0) <= nMinPossiblePiercing &&
		pEnemy->GetArmor(1) <= nMinPossiblePiercing &&
		pEnemy->GetArmor(2) <= nMinPossiblePiercing &&
		pEnemy->GetArmor(3) <= nMinPossiblePiercing;
}
void CMechAttackUnitState::CalculateProbabilitites()
{
	const float x0 = pGun->GetPiercing() - pGun->GetPiercingRandom();
	const float x1 = pGun->GetPiercing() + pGun->GetPiercingRandom();

	eAttackType = EAT_POOR_PROB;
	for ( int i = 0; i < 4; ++i )
	{
		fProb[i] = CalculateProbability( x0, pEnemy->GetMinPossibleArmor( i ), x1, pEnemy->GetMaxPossibleArmor( i ) );
		NI_ASSERT_T( fProb[i] >= 0, "Wrong probability" );

		if ( fProb[i] >= SConsts::GOOD_ATTACK_RPOBABILITY )
			eAttackType = EAT_GOOD_PROB;
	}
}
void CMechAttackUnitState::StartStateWithGun( CBasicGun *_pGun )
{
	pGun = _pGun;

	damageToEnemyUpdater.UnsetDamageFromEnemy( pEnemy );
	
	pUnit->Lock( pGun );

	const bool bSPGCanShootWOMove = pUnit->GetStats()->IsSPG() && pGun->CanShootToUnitWOMove( pEnemy );

	if ( pEnemy->IsVisible( pUnit->GetParty() ) && 
			 ( pUnit->NeedDeinstall() || !pUnit->CanMove() || pGun->IsBallisticTrajectory() || bSPGCanShootWOMove ) )
		state = ESAS_SIMPLE_FIRING;
	else if ( !bSPGCanShootWOMove )
	{
		if ( IsBruteMoving() )
			state = ESAS_BRUTE_MOVING;
		else
		{
			CalculateProbabilitites();
			state = ESAS_MOVING;
		}
	}
	else
		FinishState();
}
void CMechAttackUnitState::FinishState()
{
	StopFire();
	damageToEnemyUpdater.UnsetDamageFromEnemy( pEnemy );

	pUnit->SetCommandFinished();
}
void CMechAttackUnitState::Segment()
{
	if ( bFinish )
	{
		StopFire();			
		pUnit->SetCommandFinished();
	}
	else if ( pGun == 0 )
	{
		if ( IsValidObj( pEnemy ) )
		{
			pUnit->ResetShootEstimator( pEnemy, false );
			pGun = pUnit->GetBestShootEstimatedGun();
			const EUnitAckType eReject = pUnit->GetGunsRejectReason();
			if ( !pUnit->GetCurCmd()->IsFromAI() && 
						pUnit->IsInTankPit() && 
						( eReject == ACK_ENEMY_IS_TO_CLOSE || eReject == ACK_NOT_IN_FIRE_RANGE ) )
			{
				theGroupLogic.InsertUnitCommand( SAIUnitCmd(ACTION_MOVE_LEAVE_TANK_PIT), pUnit );
				return;
			}
			if ( pGun == 0 )
			{
				if ( pEnemy->GetStats()->IsArtillery() )
				{
					if ( CFormation *pFormation = checked_cast_ptr<CArtillery*>( pEnemy )->GetCrew() )
					{
						for ( int i = 0; i < pFormation->Size(); ++i )
							theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_ATTACK_UNIT, (*pFormation)[i], bSwarmAttack ), pUnit );
					}
					else
						pUnit->SendAcknowledgement( eReject );
				}
				else
					pUnit->SendAcknowledgement( eReject );
			}
		}

		if ( pGun == 0 )
			StopFire();
		else
			StartStateWithGun( pGun );
	}
	else if ( !pGun || !pGun->IsFiring() && !pGun->CanShootToUnit( pEnemy ) )
		StartAgain();
	else if ( pGun->GetNAmmo() == 0 )
		StopFire();
	else
	{
		CFreeFireManager::Analyze( pUnit, pGun );
		damageToEnemyUpdater.SetDamageToEnemy( pUnit, pEnemy, pGun );

		if ( !pGun->IsFiring() )
		{
			if ( !IsValidObj( pEnemy ) || pEnemy.GetPtr() == pUnit || pEnemy->GetParty() != nEnemyParty )
				FinishState();

			if ( bSwarmAttack )
				pUnit->AnalyzeTargetScan( pEnemy, damageToEnemyUpdater.IsDamageUpdated(), false );
			else
			{
				CBasicGun *pNewGun = pUnit->AnalyzeGunChoose( pEnemy );
				if ( pNewGun != 0 && pNewGun != pGun )
					StartStateWithGun( pNewGun );
			}

			if ( !bFinish )
			{
				if ( pEnemy->IsVisible( pUnit->GetParty() ) || pEnemy->IsRevealed() )
				{
					vEnemyCenter = pEnemy->GetCenter();
					wEnemyDir = pEnemy->GetFrontDir();
				}
				switch ( state )
				{
					case ESAS_SIMPLE_FIRING:
						if ( !pEnemy->IsVisible( pUnit->GetParty() ) && !pEnemy->IsRevealed() )
							FinishState();
						else
							FireNow();

						break;
					case ESAS_BRUTE_MOVING:
						AnalyzeBruteMovingPosition();

						break;
					case ESAS_MOVING:
						AnalyzeMovingPosition();

						break;
					case ESAS_MOVING_TO_SIDE:
						AnalyzeMovingToSidePosition();

						break;
				}
			}
		}
	}
}
const CVec2 CMechAttackUnitState::GetPurposePoint() const
{
	if ( IsValidObj( pEnemy ) )
		return pEnemy->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
ETryStateInterruptResult CMechAttackUnitState::TryInterruptState( CAICommand *pCommand )
{
	if ( !pCommand )
	{
		StopFire();
		pUnit->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	
	const SAIUnitCmd &cmd = pCommand->ToUnitCmd();
	if ( cmd.cmdType == ACTION_COMMAND_ATTACK_UNIT && cmd.pObject.GetPtr() == pEnemy )
		return TSIR_NO_COMMAND_INCOMPATIBLE;

	if ( !pGun.IsValid() || !pGun->IsBursting() )
	{
		StopFire();
		pUnit->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	else
	{
		bFinish = true;
		return TSIR_YES_WAIT;
	}
}
void CMechAttackUnitState::FireNow()
{
	NI_ASSERT_T( pGun != 0, "Wrong gun descriptor" );

	pUnit->RegisterAsBored( ACK_BORED_ATTACK );

	pUnit->StopUnit();
	if ( pGun->IsOnTurret() )
		pGun->GetTurret()->Lock( pGun );
	pGun->StartEnemyBurst( pEnemy, bAim );
}
void CMechAttackUnitState::StopFire()
{
	if ( pGun != 0 )
	{

		if ( pUnit->IsMech() )
		{
			for ( int i = 0; i < pUnit->GetNGuns(); ++i )
				pUnit->GetGun( i )->StopFire();
		}
		else
			pGun->StopFire();

		if ( IsValidObj( pEnemy ) )
			pGun->TraceAim( pEnemy );
	}

	damageToEnemyUpdater.UnsetDamageFromEnemy( pEnemy );
	pUnit->UnRegisterAsBored( ACK_BORED_ATTACK );

	bFinish = true;
}
CCommonAttackCommonStatObjState::CCommonAttackCommonStatObjState( CAIUnit *pOwner, CStaticObject *_pObj, bool _bSwarmAttack )
: CFreeFireManager( pOwner ), pObj( _pObj ), pGun( 0 ), bAim( true ), bFinish( false ), bSwarmAttack( _bSwarmAttack ),
	nStartObjParty( theDipl.GetNParty( _pObj->GetPlayer() ) )
{
}
void CCommonAttackCommonStatObjState::StartAgain()
{
	if ( pGun.IsValid() )
	{
		pGun->StopFire();
		pGun->StopTracing();
	}

	pGun = 0;
}
void CCommonAttackCommonStatObjState::FinishState()
{
	if ( GetUnit()->IsMech() )
	{
		for ( int i = 0; i < GetUnit()->GetNGuns(); ++i )
			GetUnit()->GetGun( i )->StopFire();
	}

	if ( pGun.IsValid() )
	{
		pGun->StopFire();
		pGun->StopTracing();
	}

	GetUnit()->SetCommandFinished();
}
void CCommonAttackCommonStatObjState::AnalyzePosition()
{
	const CVec2 vUnitCenter = GetUnit()->GetCenter();
	if ( pGun->InFireRange( CVec3( pObj->GetAttackCenter( vUnitCenter ), 0.0f ) ) )
	{
		FireNow();
		bAim = false;
	}
	else if ( pGun->TooCloseToFire( CVec3( pObj->GetAttackCenter( vUnitCenter ), 0.0f ) ) )
	{
		GetUnit()->SendAcknowledgement( ACK_ENEMY_IS_TO_CLOSE );
		FinishState();
	}
	else
	{
		if ( CPtr<IStaticPath> pStaticPath = CreateStaticPathForStObjAttack( GetUnit(), pObj, pGun->GetWeapon()->fRangeMin, pGun->GetFireRange( 0 ) ) )
		{
			if ( GetUnit()->GetTile() != pStaticPath->GetFinishTile() )
				bAim = true;

			GetUnit()->SendAlongPath( pStaticPath, VNULL2 );
		}
		else
		{
			GetUnit()->SendAcknowledgement( ACK_CANNOT_FIND_PATH_TO_TARGET );
			FinishState();
		}
	}
}
void CCommonAttackCommonStatObjState::AnalyzeShootingObj()
{
	if ( GetUnit()->IsIdle() )
	{
		if ( GetUnit()->IsMech() )
			Analyze( GetUnit(), pGun );

		AnalyzePosition();
	}
}
bool CCommonAttackCommonStatObjState::AttackUnits( CStaticObject *pObj )
{
	GetUnit()->ResetShootEstimator( 0, false );

	ILoadableObject *pLoadObject;

	if ( pObj->GetObjectType() == ESOT_BUILDING )
		pLoadObject = static_cast<CBuilding*>(pObj);
	else if ( pObj->GetObjectType() == ESOT_ENTRENCHMENT )
		pLoadObject = static_cast<CEntrenchment*>(pObj);
	else
		NI_ASSERT_T( false, "Wrong object to attack units inside" );

	pLoadObject->StartIterate();
	while ( !pLoadObject->IsIterateFinished() )
	{
		GetUnit()->AddUnitToShootEstimator( pLoadObject->GetIteratedUnit() );
		pLoadObject->Iterate();
	}

	if ( CAIUnit *pTarget = GetUnit()->GetBestShootEstimatedUnit() )
	{
		theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_ATTACK_UNIT, pTarget ), GetUnit() );
		return true;
	}
	else
		return false;
}
void CCommonAttackCommonStatObjState::Segment()
{
	if ( !pGun.IsValid() )
	{
		if ( IsValidObj( pObj ) )
		{
			pGun = 0;
			EUnitAckType eRejectFireToObjReason = ACK_NONE;
			if ( pObj->GetObjectType() != ESOT_ENTRENCHMENT )
			{
				pGun = GetUnit()->ChooseGunForStatObjWOTime( pObj );
				if ( pGun == 0 )
					eRejectFireToObjReason = GetUnit()->GetGunsRejectReason();
				else if ( pGun->GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_GRENADE )
				{
					if ( !( GetUnit()->GetNGuns() > 1 && GetUnit()->GetGun(0)->GetNAmmo() == 0 ||
									fabs2( pObj->GetAttackCenter( GetUnit()->GetCenter() ) - GetUnit()->GetCenter() ) < sqr( SConsts::MAX_DISTANCE_TO_THROW_GRENADE ) &&
									pObj->GetHitPoints() < 100.0f ) )
						pGun = 0;
				}
			}
			
			if ( !GetUnit()->GetCurCmd()->IsFromAI() && 
						GetUnit()->IsInTankPit() && 
						( GetUnit()->GetGunsRejectReason() == ACK_ENEMY_IS_TO_CLOSE || GetUnit()->GetGunsRejectReason() == ACK_NOT_IN_FIRE_RANGE ) )
			{
				theGroupLogic.InsertUnitCommand( SAIUnitCmd(ACTION_MOVE_LEAVE_TANK_PIT), GetUnit() );
				return;
			}

			if ( pGun == 0 )
			{
				if ( pObj->GetObjectType() == ESOT_BUILDING || pObj->GetObjectType() == ESOT_ENTRENCHMENT )
				{
					if ( !AttackUnits( pObj ) )
					{
						if ( pObj->GetObjectType() == ESOT_ENTRENCHMENT )
							GetUnit()->SendAcknowledgement( ACK_NEGATIVE );
						else
							GetUnit()->SendAcknowledgement( GetUnit()->GetGunsRejectReason() );

						GetUnit()->SetCommandFinished();
					}
				}
				else
				{
					GetUnit()->SendAcknowledgement( eRejectFireToObjReason );
					GetUnit()->SetCommandFinished();
				}
			}
			else
			{
				GetUnit()->Lock( pGun );
				AnalyzePosition();
			}
		}
		else
			FinishState();
	}
	else if ( !pGun->IsBursting() && ( !IsValidObj( pObj ) || bFinish || theDipl.GetNParty( pObj->GetPlayer() ) != nStartObjParty ) )
		FinishState();
	else if ( !pGun->IsFiring() )
	{
		if ( !IsValidObj( pObj ) || bFinish || theDipl.GetNParty( pObj->GetPlayer() ) != nStartObjParty )
			FinishState();
		else if ( pGun->GetNAmmo() == 0 )
			StartAgain();
		else
			AnalyzeShootingObj();
	}
}
const CVec2 CCommonAttackCommonStatObjState::GetPurposePoint() const
{
	CCommonUnit *pUnit = GetUnit();
	if ( IsValidObj( pObj ) && pUnit && pUnit->IsValid() && pUnit->IsAlive() )
		return pObj->GetAttackCenter( pUnit->GetCenter() );
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CMechUnitRestState::Instance( class CAIUnit *pUnit, const CVec2 &guardPoint, const WORD wDir, const bool bFinishWnenCanMove )
{
	return new CMechUnitRestState( pUnit, guardPoint, wDir, bFinishWnenCanMove );
}
CMechUnitRestState::CMechUnitRestState( CAIUnit *_pUnit, const CVec2 &_guardPoint, const WORD _wDir, const bool _bFinishWnenCanMove )
: pUnit( _pUnit ), CCommonRestState( _guardPoint, _wDir, _pUnit ), bFinishWhenCanMove( _bFinishWnenCanMove )
{
	pUnit->StartCamouflating();
}
void CMechUnitRestState::Segment()
{
	if ( pUnit->GetStats()->type != RPG_TYPE_ART_ROCKET || theDipl.IsAIPlayer( pUnit->GetPlayer() ) )
		CCommonRestState::Segment();

	AnalyzeUnderFire( pUnit );
	
	if ( bFinishWhenCanMove && pUnit->CanMove() )
		pUnit->SetCommandFinished();
}
ETryStateInterruptResult CMechUnitRestState::TryInterruptState( class CAICommand *pCommand )
{

	pUnit->UnRegisterAsBored( ACK_BORED_IDLE );	
	
	for ( int i = 0; i < pUnit->GetNTurrets(); ++i )
	{
		pUnit->GetTurret( i )->SetCanReturn();
	}

	pUnit->SetCommandFinished();	
	return TSIR_YES_IMMIDIATELY;
}
CCommonRestState::CCommonRestState( const CVec2 &_guardPoint, const WORD _wDir, CCommonUnit *_pUnit )
: pUnit( _pUnit ), nextMove( 0 ), guardPoint( _guardPoint ), wDir( _wDir ), startMoveTime( curTime )
{
	ResetTime( pUnit );
	pUnit->ResetTargetScan();
	pUnit->StopUnit();
	
	bScanned = pUnit->IsFormation();
}
void CCommonRestState::Segment()
{
	if ( guardPoint.x == -1 )
	{
		guardPoint = pUnit->GetCenter();
		wDir = pUnit->GetFrontDir();

		if ( pUnit->GetCurCmd() == 0 )
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_GUARD, guardPoint, wDir ), pUnit, false );
		else
		{
			pUnit->GetCurCmd()->ToUnitCmd().vPos = guardPoint;
			pUnit->GetCurCmd()->ToUnitCmd().fNumber = wDir;
		}
	}
	
	bool bTargetFound = false;
	if ( !pUnit->IsFormation() )
	{
		BYTE cResult = pUnit->AnalyzeTargetScan( 0, false, false );
		if ( cResult & 2 )
			bScanned = true;
		bTargetFound = cResult & 1;
	}

	if ( !bTargetFound )
	{
		if ( bScanned && curTime >= nextMove && pUnit->CanMoveForGuard() && pUnit->IsIdle() )
		{
			const CVec2 vUnitCenter = pUnit->GetCenter();

			if ( pUnit->IsInFollowState() && fabs( vUnitCenter - pUnit->GetFollowedUnit()->GetCenter() ) >= SConsts::FOLLOW_GO_RADIUS )
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_FOLLOW_NOW, pUnit->GetFollowedUnit() ), pUnit, false );
			else if ( curTime <= startMoveTime + 10000 )
			{
				const float fDistToGuardPoint = fabs( guardPoint - vUnitCenter );
				const bool bTooFar = fDistToGuardPoint > 2.0f * (int)SConsts::TILE_SIZE;

				bool bSent = false;
				if ( bTooFar )
				{
					CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( guardPoint, VNULL2, pUnit );
					if ( pStaticPath )
					{
						const float fNewDistToGuardPoint = fabs( pStaticPath->GetFinishPoint() - guardPoint );
						if ( fNewDistToGuardPoint < fDistToGuardPoint - 2 * SConsts::TILE_SIZE &&
								 fabs2( pStaticPath->GetFinishPoint() - pUnit->GetCenter() ) >= sqr( 2.0f * SConsts::TILE_SIZE ) )
						{
							pUnit->SendAlongPath( pStaticPath, VNULL2 );
							bSent = true;
						}
					}
				}
				else if ( ( pUnit->GetRotateSpeed() > 0 || pUnit->IsFormation() ) && DirsDifference( pUnit->GetFrontDir(), wDir ) > 3000 && pUnit->CanTurnToFrontDir( wDir ) )
				{
					theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_ROTATE_TO, vUnitCenter + GetVectorByDirection( wDir ) ), pUnit, false );
					theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_GUARD, vUnitCenter, wDir ), pUnit, true );
					bSent = true;
				}

				if ( !bSent )
				{
					pUnit->RegisterAsBored( ACK_BORED_IDLE );
					pUnit->FreezeByState( true );
				}
				else
					pUnit->UnRegisterAsBored( ACK_BORED_IDLE );
			}

			nextMove = curTime + Random( 2000, 6000 );
		}
	}
}
ETryStateInterruptResult CCommonRestState::TryInterruptState( class CAICommand *pCommand )
{
	pUnit->UnRegisterAsBored( ACK_BORED_IDLE );	
	pUnit->SetCommandFinished();

	return TSIR_YES_IMMIDIATELY;
}
IUnitState* CCommonAmbushState::Instance( CCommonUnit *pUnit )
{
	return new CCommonAmbushState( pUnit );
}
CCommonAmbushState::CCommonAmbushState( CCommonUnit *_pUnit )
: pUnit( _pUnit ), pTarget( 0 ), eState( EAS_COMMON )
{
	pUnit->StopUnit();
	pUnit->SetAmbush();
	startTime = curTime;
	lastCheckTime = curTime;
	lastVisibleCheck = curTime;
	pUnit->RegisterAsBored( ACK_BORED_IDLE );

	theGroupLogic.UnitSetToAmbush( pUnit );
}
void CCommonAmbushState::CommonState()
{
	bool bAttack = false;
	if ( curTime - lastCheckTime >= AMBUSH_CHECK && ( pGun == 0 || !pGun->IsFiring() ) )
	{
		lastCheckTime = curTime;
		CAIUnit *pTargetLookingFor = 0;
		CBasicGun *pGunLookingFor = 0;
		if ( pUnit->GetBehaviour().fire == SBehaviour::EFAtWill )
			pUnit->LookForTarget( 0, false, &pTargetLookingFor, &pGunLookingFor );
		pTarget = pTargetLookingFor;
		pGun = pGunLookingFor;
		if ( pTarget != 0 )
		{
			if ( pGun != 0 )
			{
				pGun->DontShoot();
				pGun->StartEnemyBurst( pTarget, true );
			}
		}
	}
	if ( curTime - lastVisibleCheck >= VISIBLE_CHECK )
	{
		lastVisibleCheck = curTime;
		if ( pUnit->GetParty() != theDipl.GetNeutralParty() && pUnit->IsVisible( 1 - pUnit->GetParty() ) )
			bAttack = true;
	}
	
	if ( !bAttack )
		pUnit->FreezeByState( true );

	if ( bAttack )
	{

		int nSpecialGroup = pUnit->GetSpecialGroup();
		if ( nSpecialGroup == 0 && pUnit->IsInFormation() )
			nSpecialGroup = pUnit->GetFormation()->GetSpecialGroup();
		
		if (  nSpecialGroup == 0 )
		{
			if ( pTarget != 0 && pUnit->GetBehaviour().fire == SBehaviour::EFAtWill )
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_ATTACK_UNIT, pTarget, 1 ), pUnit, false );
			else
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP_THIS_ACTION ), pUnit, false );
		}
		else
		{
			std::list< CPtr<CCommonUnit> > groupUnits;
			for ( int iter = theGroupLogic.BeginGroup( nSpecialGroup ); iter != theGroupLogic.EndGroup(); iter = theGroupLogic.Next( iter ) )
				groupUnits.push_back( theGroupLogic.GetGroupUnit( iter ) );
			
			if ( pTarget != 0 && pUnit->GetBehaviour().fire == SBehaviour::EFAtWill )
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_ATTACK_UNIT, pTarget, 1 ), pUnit, false );
			else
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP_THIS_ACTION ), pUnit, false );

			for ( std::list< CPtr<CCommonUnit> >::iterator iter = groupUnits.begin(); iter != groupUnits.end(); ++iter )
			{
				CCommonUnit *pGroupUnit = *iter;
				if ( pUnit != pGroupUnit && IsValidObj( pGroupUnit ) && pGroupUnit->GetState()->GetName() == EUSN_AMBUSH )
				{
					CAIUnit *pTarget = static_cast<CCommonAmbushState*>(pGroupUnit->GetState())->GetTarget();

					if ( pTarget != 0 && pGroupUnit->GetBehaviour().fire == SBehaviour::EFAtWill )
						theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_ATTACK_UNIT, pTarget, 1 ), pGroupUnit, false );
					else
						theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP_THIS_ACTION ), pGroupUnit, false );
				}
			}

			if ( pUnit->GetSpecialGroup() )
				theGroupLogic.UnregisterSpecialGroup( pUnit->GetSpecialGroup() );
		}
	}
}
void CCommonAmbushState::FiringState()
{
	if ( pGun == 0 || !pGun->IsFiring() )
		pUnit->SetCommandFinished();
}
void CCommonAmbushState::Segment()
{
	if ( curTime - startTime >= pUnit->GetTimeToCamouflage() && eState != EAS_FIRING )
		pUnit->SetCamoulfage();

	switch ( eState )
	{
		case EAS_COMMON:
			CommonState();

			break;
		case EAS_FIRING:
			FiringState();

			break;
	}
}
ETryStateInterruptResult CCommonAmbushState::TryInterruptState( CAICommand *pCommand )
{
	if ( pCommand && pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_AMBUSH )
		return TSIR_NO_COMMAND_INCOMPATIBLE;
	
	if ( pUnit->GetParty() != theDipl.GetNeutralParty() && pUnit->IsVisible( 1 - pUnit->GetParty() ) )
		pUnit->RemoveCamouflage( ECRR_SELF_MOVE );

	pUnit->RemoveAmbush();
	theGroupLogic.DelUnitFromSpecialGroup( pUnit );

	for ( int i = 0; i < pUnit->GetNGuns(); ++i )
		pUnit->GetGun(i)->CanShoot();

	if ( pCommand != 0 && pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_ATTACK_UNIT 
				&& pTarget == pCommand->ToUnitCmd().pObject )
	{
		pCommand->ToUnitCmd().fNumber = 1;
		eState = EAS_FIRING;
	}
	else
		pUnit->SetCommandFinished();

	pUnit->UnRegisterAsBored( ACK_BORED_IDLE );

	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CCommonAmbushState::GetPurposePoint() const
{
	if ( pUnit && pUnit->IsAlive() )
		return pUnit->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
CCommonAttackUnitInBuildingState::CCommonAttackUnitInBuildingState( CAIUnit *pOwner, CSoldier *_pTarget, bool _bAim, const bool _bSwarmAttack )
: CFreeFireManager( pOwner ), pTarget( _pTarget ), bSwarmAttack( _bSwarmAttack )
{
	StartState( pOwner );
}
void CCommonAttackUnitInBuildingState::StartState( CAIUnit *pOwner )
{
	eState = EAUBS_START;
	targetCenter = pTarget->GetCenter();
	runUpToEnemy.Init( pOwner, pTarget );
	nSlot = pTarget->GetSlot();
}
void CCommonAttackUnitInBuildingState::FinishState()
{
	if ( GetUnit()->IsMech() )
	{
		for ( int i = 0; i < GetUnit()->GetNGuns(); ++i )
			GetUnit()->GetGun( i )->StopFire();
	}

	if ( pGun.IsValid() )
	{
		pGun->StopFire();
	}

	GetUnit()->UnRegisterAsBored( ACK_BORED_ATTACK );
	damageToEnemyUpdater.UnsetDamageFromEnemy( pTarget );
	runUpToEnemy.Finish();
	GetUnit()->SetCommandFinished();
}
bool CCommonAttackUnitInBuildingState::IsInTargetSector() const
{
	return IsInTheAngle(
						GetDirectionByVector( GetUnit()->GetCenter() - pTarget->GetCenter() ),
						pTarget->GetMinAngle(), pTarget->GetMaxAngle() 
					);
}
bool CCommonAttackUnitInBuildingState::FindPathToUnit()
{
	if ( CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( pTarget->GetCenter(), VNULL2, GetUnit(), true ) )
	{
		GetUnit()->SendAlongPath( pStaticPath, VNULL2 );
		return true;
	}
	else
		return false;
}
bool CCommonAttackUnitInBuildingState::FindPathToSector()
{
	const CVec2 point = 
		pTarget->GetCenter() + 
		GetVectorByDirection
		( 
			WORD( pTarget->GetMaxAngle() - pTarget->GetMinAngle() ) / 2 + pTarget->GetMinAngle()
		) * 320.0f;

	if ( CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( point, VNULL2, GetUnit() ) )
	{
		GetUnit()->SendAlongPath( pStaticPath, VNULL2 );
		return true;
	}
	else
		return false;
}
void CCommonAttackUnitInBuildingState::Segment()
{
	if ( !IsValidObj( pTarget ) || !pTarget->IsInBuilding() || nSlot == -1 )
		FinishState();
	else if ( nSlot != pTarget->GetSlot() )
	{
		CBuilding *pBuilding = pTarget->GetBuilding();
		if ( pBuilding && pBuilding->IsValid() )
		{
			CSoldier *pNewTarget = pBuilding->GetSoldierInFireplace( nSlot );
			if ( pNewTarget != 0 )
			{
				pTarget = pNewTarget;
				StartState( GetUnit() );
			}
			else
				FinishState();
		}
		else
			FinishState();
	}
	else if ( pGun == 0 || !pGun->IsFiring() )
	{
		damageToEnemyUpdater.SetDamageToEnemy( GetUnit(), pTarget, pGun );
		if ( pGun != 0 && GetUnit()->IsMech() )
			Analyze( GetUnit(), pGun );
		if ( bSwarmAttack )
			GetUnit()->AnalyzeTargetScan( pTarget, damageToEnemyUpdater.IsDamageUpdated(), false );

		if ( pGun.IsValid() && !pGun->IsFiring() )
			runUpToEnemy.Segment();

		if ( !runUpToEnemy.IsRunningToEnemy() )
		{
			switch ( eState )
			{
				case EAUBS_START:
					{
						CCommonUnit *pUnit = GetUnit();
						pUnit->ResetShootEstimator( pTarget, false );
						pGun = pUnit->GetBestShootEstimatedGun();
						if ( pGun == 0 )
							FinishState();
						else
						{
							pUnit->Lock( pGun );
							
							bAim = true;
							if ( pGun->CanShootToUnitWOMove( pTarget ) )
								eState = EAUBS_MOVING_UNIT;
							else if ( IsInTargetSector() )
							{
								if ( FindPathToUnit() )
									eState =  EAUBS_MOVING_UNIT;
								else
									FinishState();
							}
							else if ( FindPathToSector() )
								eState = EAUBS_MOVING_SECTOR;
							else
								FinishState();
						}
						if ( eState == EAUBS_START )
							pUnit->SendAcknowledgement( ACK_NEGATIVE );
					}

					break;
				case EAUBS_MOVING_SECTOR:
					if ( IsInTargetSector() )
					{
						if ( FindPathToUnit() )
							eState = EAUBS_MOVING_UNIT;
						else
							FinishState();
					}
					else if ( GetUnit()->IsIdle() )
						eState = EAUBS_START;

					break;
				case EAUBS_MOVING_UNIT:
					if ( pGun->CanShootToUnitWOMove( pTarget ) )
					{
						GetUnit()->RegisterAsBored( ACK_BORED_ATTACK );
						FireNow();
					}
					else if ( GetUnit()->IsIdle() )
						FinishState();

					break;
			}
		}
	}
}
const CVec2 CCommonAttackUnitInBuildingState::GetPurposePoint() const
{
	if ( IsValidObj( pTarget ) )
		return pTarget->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
CAIUnit* CCommonAttackUnitInBuildingState::GetTargetUnit() const 
{ 
	return GetTarget(); 
}
IUnitState* CFollowState::Instance( CCommonUnit *pUnit, CCommonUnit *pHeadUnit )
{
	return new CFollowState( pUnit, pHeadUnit );
}
CFollowState::CFollowState( CCommonUnit *_pUnit, CCommonUnit *_pHeadUnit )
: pUnit( _pUnit ), pHeadUnit( _pHeadUnit ), lastHeadUnitPos( -1.0f, -1.0f ), lastCheck( 0 )
{
}
void CFollowState::Segment()
{
	if ( !IsValidObj( pHeadUnit ) )
	{
		pUnit->UnsetDesirableSpeed();
		pUnit->SetCommandFinished();
	}
	else if ( ( pUnit->AnalyzeTargetScan( 0, false, false ) & 1 ) == 0 )
	{
		const float fDist = fabs( pUnit->GetCenter() - pHeadUnit->GetCenter() );

		if ( fDist <= SConsts::FOLLOW_STOP_RADIUS )
		{
			pUnit->UnsetDesirableSpeed();			
			pUnit->StopUnit();
			pUnit->SetCommandFinished();
		}
		else
		{
			if ( pUnit->IsIdle() || curTime - lastCheck >= CHECK_HEAD && lastHeadUnitPos != pHeadUnit->GetCenter() )
			{
				lastCheck = curTime;
				lastHeadUnitPos = pHeadUnit->GetCenter();
				if ( CPtr<IStaticPath> pPath = CreateStaticPathToPoint( pHeadUnit->GetCenter() + pUnit->GetFollowShift(), VNULL2, pUnit, true ) )
				{
					pUnit->SendAlongPath( pPath, VNULL2 );
				}
				else
				{
					pUnit->UnsetDesirableSpeed();
					pUnit->SetCommandFinished();
				}
			}

			pHeadUnit->FollowingByYou( pUnit );
			if ( fDist <= SConsts::FOLLOW_EQUALIZE_SPEED_RADIUS )
			{
				theGroupLogic.AddFollowingUnit( pUnit );
			}
		}
	}
}
ETryStateInterruptResult CFollowState::TryInterruptState( CAICommand *pCommand )
{
	pUnit->UnsetDesirableSpeed();
	pUnit->SetCommandFinished(); 
	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CFollowState::GetPurposePoint() const
{
	if ( IsValidObj( pHeadUnit ) )
		return pHeadUnit->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CCommonSwarmState::Instance( CAIUnit *pUnit, const CVec2 &point, const float fContinue )
{
	return new CCommonSwarmState( pUnit, point, fContinue );
}
CCommonSwarmState::CCommonSwarmState( CAIUnit *_pUnit, const CVec2 &_point, const float fContinue )
: pUnit( _pUnit ), state( ESSS_WAIT ), startTime( curTime ), bContinue( fContinue ),
	point( _point ), wDirToPoint( _pUnit->GetFrontDir() )
{
	pUnit->ResetTargetScan();
	pUnit->UnlockTiles();
	pUnit->FixUnlocking();
}
void CCommonSwarmState::Segment()
{
	switch ( state )
	{
		case ESSS_WAIT:
			if ( curTime - startTime >= TIME_OF_WAITING )
			{
				state = ESSS_MOVING;
				if ( !bContinue )
				{
					if ( CPtr<IStaticPath> pStaticPath = pUnit->GetCurCmd()->CreateStaticPath( pUnit ) )
						pUnit->SendAlongPath( pStaticPath, pUnit->GetGroupShift() );
				}
				else if ( CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( point, pUnit->GetGroupShift(), pUnit ) )
					pUnit->SendAlongPath( pStaticPath, pUnit->GetGroupShift() );
				else
				{
					pUnit->SendAcknowledgement( ACK_NEGATIVE );
					TryInterruptState( 0 );
				}

				point += pUnit->GetGroupShift();
				wDirToPoint = GetDirectionByVector( point - pUnit->GetCenter() );

				pUnit->UnfixUnlocking();
			}

			pUnit->AnalyzeTargetScan( 0, false, true );

			break;
		case ESSS_MOVING:
			if ( pUnit->IsIdle() )
			{
				const SUnitBaseRPGStats * pStats = pUnit->GetStats();
			
				if ( pStats->IsArmor() || pStats->IsSPG() || pStats->IsTrain() )
				{
					CTank * pTank = static_cast<CTank*>( pUnit );
					if ( pTank->IsTrackDamaged() ) 
					{
						theGroupLogic.InsertUnitCommand( SAIUnitCmd(ACTION_MOVE_WAIT_FOR_TRUCKREPAIR), pUnit );
						break;
					}
				}
				
				if ( pUnit->GetNextCommand() == 0 )
				{
					const WORD wDir = pUnit->GetFrontDir() == pUnit->GetDir() ? wDirToPoint : wDirToPoint + 32768;
					theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_GUARD, point, wDir ), pUnit, false );
				}
			
				pUnit->SetCommandFinished();
			}
			pUnit->AnalyzeTargetScan( 0, false, true );

			break;
	}
}
ETryStateInterruptResult CCommonSwarmState::TryInterruptState(class CAICommand *pCommand )
{
	if ( pUnit->GetCurCmd() != 0 )
		pUnit->GetCurCmd()->ToUnitCmd().fNumber = 1;

	pUnit->UnfixUnlocking();
	pUnit->SetCommandFinished();

	return TSIR_YES_IMMIDIATELY;
}
IUnitState* CCommonMoveToGridState::Instance( CCommonUnit *pUnit, const CVec2 &vPoint, const CVec2 &vDir )
{
	return new CCommonMoveToGridState( pUnit, vPoint, vDir );
}
CCommonMoveToGridState::CCommonMoveToGridState( CCommonUnit *_pUnit, const CVec2 &_vPoint, const CVec2 &_vDir )
: pUnit( _pUnit ), vPoint( _vPoint ), vDir( _vDir ), startMoveTime( curTime + Random( 200, 600 ) ), eState( ES_WAIT )
{
}
void CCommonMoveToGridState::Segment()
{
	switch ( eState )
	{
		case ES_WAIT:
			if ( startMoveTime < curTime )
			{
				eState = ES_MOVE;
				
				CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( vPoint, VNULL2, pUnit );
				if ( pStaticPath )
					pUnit->SendAlongPath( pStaticPath, VNULL2 );
				else
					pUnit->SetCommandFinished();
			}
			break;
		case ES_MOVE:
			if ( pUnit->IsIdle() )
			{
				SAIUnitCmd cmd( ACTION_COMMAND_GUARD, vPoint, GetDirectionByVector(vDir) );
				pUnit->PushFrontUnitCommand( new CAICommand( cmd ) );
			}

			break;
	}
}
ETryStateInterruptResult CCommonMoveToGridState::TryInterruptState( CAICommand *pCommand )
{
	pUnit->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
