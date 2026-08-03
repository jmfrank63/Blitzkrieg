#include "StdAfx.h"

#include "AILogicInternal.h"
#include "Soldier.h"
#include "StatesFactory.h"
#include "Updater.h"
#include "GroupLogic.h"
#include "Building.h"
#include "Commands.h"
#include "Diplomacy.h"
#include "Units.h"
#include "UnitsIterators2.h"
#include "UnitsIterators.h"
#include "AIWarFog.h"
#include "AntiArtilleryManager.h"
#include "Cheats.h"
#include "PathUnit.h"
#include "AntiArtillery.h"
#include "Turret.h"
#include "UnitGuns.h"
#include "Shell.h"
#include "AIStaticMap.h"
#include "AckManager.h"
#include "Probability.h"
#include "Artillery.h"
#include "Behaviour.h"
#include "Aviation.h"
#include "StaticObjects.h"
#include "StaticObject.h"
#include "CombatEstimator.h"
#include "Formation.h"
#include "Statistics.h"
#include "StaticObjects.h"
#include "float.h"
#include "General.h"
#include "ShootEstimatorInternal.h"
#include "AIUnitInfoForGeneral.h"
#include "GeneralConsts.h"
#include "ScanLimiter.h"
#include "MultiplayerInfo.h"
#include "FormationStates.h"
#include "UnitStates.h"
#include "MPLog.h"
#include "DifficultyLevel.h"
#include "Graveyard.h"
#include "StaticObjectsIters.h"
#include "TrainPathUnit.h"

#include "..\Main\ScenarioTracker.h"
#include "..\Main\ScenarioTrackerTypes.h"
#include "..\Scene\Scene.h"
extern CSupremeBeing theSupremeBeing;
extern CStaticObjects theStatObjs;
extern CCombatEstimator theCombatEstimator;
extern CAckManager theAckManager;
extern CAILogic *pAILogic;
extern CUpdater updater;
extern CGroupLogic theGroupLogic;
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
extern CUnits units;
extern CGlobalWarFog theWarFog;
extern CAntiArtilleryManager theAAManager;
extern SCheats theCheats;
extern CStaticMap theStaticMap;
extern CStatistics theStatistics;
extern CScanLimiter theScanLimiter;
extern CMultiplayerInfo theMPInfo;
extern CDifficultyLevel theDifficultyLevel;
extern CGraveyard theGraveyard;
BASIC_REGISTER_CLASS( CAIUnit );
void CAIUnit::SendNTotalKilledUnits( const int nPlayerOfShoot )
{
	theStatistics.UnitKilled( nPlayerOfShoot, GetPlayer(), 1, GetStats()->fPrice );
	theMPInfo.UnitsKilled( nPlayerOfShoot, GetStats()->fPrice, GetPlayer() );
}
const SAINotifyHitInfo::EHitType CAIUnit::ProcessExactHit( const SRect &combatRect, const CVec2 &explCoord, const int nRandPiercing, const int nRandArmor ) const
{
	if ( combatRect.IsPointInside( explCoord ) )
	{
		if ( nRandPiercing >= nRandArmor && !IsSavedByCover() )
			return SAINotifyHitInfo::EHT_HIT;
		else
			return SAINotifyHitInfo::EHT_REFLECT;
	}
	else
		return SAINotifyHitInfo::EHT_MISS;
}
const int CAIUnit::GetRandArmorByDir( const int nArmorDir, const WORD wAttackDir, const SRect &unitRect )
{
	switch ( nArmorDir )
	{
		case 0:	return GetRandomArmor( unitRect.GetSide( wAttackDir ) );
		case 1: return GetRandomArmor( RPG_BOTTOM );
		case 2: return GetRandomArmor( RPG_TOP );
		default: NI_ASSERT_TF( false, "Wrong armor dir", 0 );
	}

	return 0;
}
bool CAIUnit::ProcessCumulativeExpl( CExplosion *pExpl, const int nArmorDir, const bool bFromExpl )
{
	if ( !IsInSolidPlace() )
	{
		SRect unitRect = GetUnitRect();
		const CVec2 vExplCoord( pExpl->GetExplCoordinates() );

		if ( fabs( GetZ() - pExpl->GetExplZ() ) <= GetStats()->vAABBHalfSize.y && unitRect.IsPointInside( vExplCoord ) )
		{
			const int nRandArmor = GetRandArmorByDir( nArmorDir, pExpl->GetAttackDir(), unitRect );
			if ( nArmorDir != 1 )
				unitRect.Compress( GetRemissiveCoeff() ); 

			const SAINotifyHitInfo::EHitType eHitType = ProcessExactHit( unitRect, vExplCoord, pExpl->GetRandomPiercing(), nRandArmor );

			if ( eHitType == SAINotifyHitInfo::EHT_HIT || theCheats.GetFirstShoot( pExpl->GetPartyOfShoot() ) == 1 )
				TakeDamage( pExpl->GetRandomDamage(), &pExpl->GetShellStats(), pExpl->GetPlayerOfShoot(), pExpl->GetWhoFire() );

			if ( !GetStats()->IsInfantry() )
				pExpl->AddHitToSend( new CHitInfo( pExpl, this, eHitType, CVec3( vExplCoord, pExpl->GetExplZ() ) ) );
			else
			{
				CSoldier *pSoldier = static_cast<CSoldier*>(this);
				if ( !pSoldier->IsInBuilding() )
					pExpl->AddHitToSend( new CHitInfo( pExpl, this, eHitType, CVec3( vExplCoord, pExpl->GetExplZ() ) ) );
			}

			return true;
		}
	}

	return false;
}
bool CAIUnit::ProcessAreaDamage( const class CExplosion *pExpl, const int nArmorDir, const float fRadius, const float fSmallRadius )
{
	if ( !IsInSolidPlace() && fabs( GetZ() - pExpl->GetExplZ() ) < GetStats()->vAABBHalfSize.y && !IsSavedByCover() )
	{
		SRect unitRect = GetUnitRect();
		if ( nArmorDir != 1 )
			unitRect.Compress( GetRemissiveCoeff() );

		if ( unitRect.IsIntersectCircle( pExpl->GetExplCoordinates(), fSmallRadius ) && GetRandArmorByDir( nArmorDir, pExpl->GetAttackDir(), unitRect ) <= SConsts::ARMOR_FOR_AREA_DAMAGE )
		{
			TakeDamage( pExpl->GetRandomDamage() * SConsts::AREA_DAMAGE_COEFF, &pExpl->GetShellStats(), pExpl->GetPlayerOfShoot(), pExpl->GetWhoFire() );
			return true;
		}
	}

	return false;
}
bool CAIUnit::ProcessBurstExpl( CExplosion *pExpl, const int nArmorDir, const float fRadius, const float fSmallRadius )
{
	if ( !ProcessCumulativeExpl( pExpl, nArmorDir, true ) )
	{
		ProcessAreaDamage( pExpl, nArmorDir, fRadius, fSmallRadius );
		return false;
	}
	return true;
}
void CAIUnit::IncreaseHitPoints( const float fInc ) 
{ 
	const float fMaxHP = GetStats()->fMaxHP;
	
	if ( fHitPoints != fMaxHP )
	{
		const float fFormerHP = fHitPoints ;
		fHitPoints = Min( fMaxHP, fHitPoints + fInc );
		updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );
		if ( fFormerHP / fMaxHP <= SConsts::LOW_HP_PERCENTAGE && fHitPoints / fMaxHP > SConsts::LOW_HP_PERCENTAGE )
			UnRegisterAsBored( ACK_BORED_LOW_HIT_POINTS );
	}
}
void CAIUnit::TakeEditorDamage( const float fDamage )
{
	if ( fDamage < 0 )
		IncreaseHitPoints( fabs( fDamage ) );
	else
	{
		updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );

		if ( ( fHitPoints -= fDamage ) <= 0 )
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_DIE, false ), static_cast<CAIUnit*>( this ), false );
	}
}
int nCntGuns = 0;
float fRange = 0;
void CAIUnit::Init( const CVec2 &center, const int z, const float fHP, const WORD dir, const BYTE _player, const WORD _id, EObjVisType eVisType, const int dbID )
{
	creationTime = curTime;
	bUnitUnderSupply = true;
	id = _id;
	player = _player;
	fHitPoints = fHP;
	fMorale = 1.0f;
	fTakenDamagePower = 0.0f;
	nGrenades = 0;
	bFreeEnemySearch = false;

	bAlive = true;
	bVisibleByPlayer = false;
	targetScanRandom = 0;
	wWisibility = 0;

	timeLastmoraleUpdate = 0;
	nVisIndexInUnits = 0;

	SetScenarioStats();

	bRevealed = false;
	bQueredToReveal = false;
	nextRevealCheck = 0;
	vPlaceOfReveal = VNULL2;

	pPathUnit = GetStats()->IsTrain() ?
							CreateObject<CPathUnit>( AI_CARRIAGE_PATH_UNIT ) : CreateObject<CPathUnit>( AI_SIMPLE_PATH_UNIT );
	pPathUnit->Init( this, center, z, dir, id );
	
	pAnimUnit = GetStats()->IsInfantry() ?
							CreateObject<IAnimUnit>( AI_ANIM_UNIT_SOLDIER ) : CreateObject<IAnimUnit>( AI_ANIM_UNIT_MECH );
	pAnimUnit->Init( this );

	CCommonUnit::Init( dbID );

	InitGuns();

	fCamouflage = 1.0f;
	wVisionAngle = SConsts::STANDART_VIS_ANGLE;
	fSightMultiplier = 1.0f;
	bHasMoraleSupport = true;

	camouflateTime = 0;
	lastAckTime = 0;

	updater.Update( GetIdleAction(), this );
	updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );
	updater.Update( ACTION_NOTIFY_STORAGE_CONNECTED, this, bUnitUnderSupply );
	pUnitInfoForGeneral = new CAIUnitInfoForGeneral( this );

	lastTimeOfVis = 0;

	theGroupLogic.RegisterSegments( this, pAILogic->IsFirstTime(), true );
	theGroupLogic.RegisterPathSegments( this, pAILogic->IsFirstTime() );

	bAlwaysVisible = ( GetStats()->szParentName == "CoastBattery_Todt" );
	visible4Party.resize( 3 );

	if ( CBasicGun *pGun = GetFirstArtilleryGun() )
	{
		++nCntGuns;
		fRange += pGun->GetFireRange( 0 );

		GetSingleton<IScene>()->GetStatSystem()->UpdateEntry( "Artillery guns:", NStr::Format( "%d %g", nCntGuns, fRange / (float)nCntGuns ) );
	}
}
void CAIUnit::SetScenarioStats()
{
	pExpLevels = checked_cast<const SAIExpLevel*>( GetSingleton<IObjectsDB>()->GetExpLevels( GetStats()->type ) );
	if ( IScenarioUnit *pScenarioUnit = GetScenarioUnit() )
	{
		fExperience = pScenarioUnit->GetValue( STUT_EXP );
		nLevel = pExpLevels->levels.size() - 1;
		while ( nLevel > 0 && fExperience < pExpLevels->levels[nLevel].nExp )
			--nLevel;
	}
	else
	{
		fExperience = 0;
		nLevel = 0;
	}
}
void CAIUnit::GetNewUnitInfo( SNewUnitInfo *pNewUnitInfo )
{
	GetPlacement( pNewUnitInfo, 0 );
	
	pNewUnitInfo->dbID = GetDBID();
	pNewUnitInfo->eDipl = theDipl.GetDiplStatus( theDipl.GetMyNumber(), GetPlayer() );
	pNewUnitInfo->nFrameIndex = GetScenarioUnit() ? GetScenarioUnit()->GetScenarioID() : -1;
	pNewUnitInfo->fHitPoints = GetHitPoints();
	pNewUnitInfo->fMorale = GetMorale();
	pNewUnitInfo->fResize = 1.0f;
	pNewUnitInfo->nPlayer = GetPlayer();
}
void CAIUnit::GetPlacement( SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff )
{ 
	pPathUnit->GetPlacement( pPlacement, timeDiff );
	pPlacement->dwNormal = GetNormale( pPlacement->center );
	pPlacement->pObj = this;
}
void CAIUnit::PrepareToDelete()
{
	if ( bAlive )
	{
		bAlive = false;

		StopUnit();

		theStaticMap.RemoveTemporaryUnlockingByUnit( GetID() );

		if ( pAntiArtillery != 0 )
			theAAManager.RemoveAA( pAntiArtillery );

		theGroupLogic.DelUnitFromGroup( this );
		theGroupLogic.DelUnitFromSpecialGroup( this );
		DelCmdQueue( GetID() );
		
		pAntiArtillery = 0;

		updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );
		theAckManager.UnitDead( this );
		theCombatEstimator.DelUnit( this );
		pUnitInfoForGeneral->Die();

		GetState()->TryInterruptState( 0 );
		SetOffTankPit();

		theGroupLogic.UnregisterSegments( this );
	}
}
void CAIUnit::Disappear()
{
	PrepareToDelete();
	
	updater.DelUpdate( ACTION_NOTIFY_PLACEMENT, this );
	updater.DelActionUpdates( this );

	CDeadUnit *pDeadUnit = new CDeadUnit( this, 0, ACTION_NOTIFY_NONE, GetDBID(), false );
	updater.Update( ACTION_NOTIFY_DISSAPEAR_UNIT, pDeadUnit );

	UnlockTiles();

	theGraveyard.PushToKilled( SKilledUnit( 0, 0, 0, curTime + SConsts::DEAD_SEE_TIME ), this );
	units.DeleteUnitFromMap( this );
}
void CAIUnit::DieTrain( const float fDamage )
{
	if ( bAlive )
	{
		bAlive = false;

		if ( GetStats()->type == RPG_TYPE_TRAIN_LOCOMOTIVE )
			StopUnit();
		GetState()->TryInterruptState( 0 );
		checked_cast<CCarriagePathUnit*>( GetPathUnit() )->UnitDead();
		
		if ( pAntiArtillery != 0 )
			theAAManager.RemoveAA( pAntiArtillery );

		theGroupLogic.DelUnitFromGroup( this );
		theGroupLogic.DelUnitFromSpecialGroup( this );
		DelCmdQueue( GetID() );
		
		pAntiArtillery = 0;
		CalcVisibility();

		updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );

		const int nFatality = ChooseFatality( fDamage );

		updater.Update( ACTION_NOTIFY_DEAD_UNIT, new CDeadUnit( this, curTime, GetDieAction(), nFatality, false ) );

		theAckManager.UnitDead( this );
		theCombatEstimator.DelUnit( this );
		
		ForceLockingTiles();
		theWarFog.DeleteUnit( GetID() );
	}
}
void CAIUnit::Die( const bool fromExplosion, const float fDamage )
{
	if ( bAlive )
	{
		if ( GetStats()->IsTrain() )
			DieTrain( fDamage );
		else
		{
			if ( !theCheats.IsHistoryPlaying() && theDipl.GetMyParty() == GetParty() )
			{
				if ( GetScenarioUnit() )
					updater.AddFeedBack( SAIFeedBack( EFB_SCENARIO_UNIT_DEAD, MAKELONG( GetCenter().x, GetCenter().y ) ) );
				if ( GetStats()->type == RPG_TYPE_SNIPER && GetPlayer() == theDipl.GetMyNumber() )
					updater.AddFeedBack( SAIFeedBack( EFB_SNIPER_DEAD, MAKELONG( GetCenter().x, GetCenter().y ) ) );
			}
			
			PrepareToDelete();

			theGraveyard.AddToSoonBeDead( this, fDamage );

			if ( fromExplosion && GetStats()->IsInfantry() && IsFree() )
				timeToDeath = curTime + Random( SConsts::AI_SEGMENT_DURATION ) * 20;
			else
				timeToDeath = curTime;
		}

		theStatistics.UnitDead( this );
	}
}
const bool CAIUnit::IsVisible( const BYTE cParty ) const
{
	return visible4Party[cParty];
}
bool CAIUnit::CalculateUnitVisibility4Party( const BYTE party ) const
{
	if ( bAlwaysVisible )
		return true;
	else if ( theDipl.GetNeutralParty() == party )
		return false;
	else
	{
		const BYTE cParty = theDipl.GetNParty( player );
		return 
			party == cParty || theStaticMap.IsTileInside( GetTile() ) && theWarFog.IsUnitVisible( party, GetTile(), IsCamoulflated(), GetCamouflage() );
	}
}
const DWORD CAIUnit::GetNormale( const CVec2 &vCenter ) const
{
	return theStaticMap.GetNormal( vCenter.x, vCenter.y );
}
const DWORD CAIUnit::GetNormale() const
{
	return CAIUnit::GetNormale( GetCenter() );
}
void CAIUnit::GetTilesForVisibility( CTilesSet *pTiles ) const 
{ 
	pTiles->clear();
	const SVector tile = GetTile();
	if ( theStaticMap.IsTileInside( tile ) )
		pTiles->push_back( tile );
}
bool CAIUnit::ShouldSuspendAction( const EActionNotify &eAction ) const
{
	return 
		!theDipl.IsEditorMode() &&
		( eAction == ACTION_NOTIFY_DEAD_UNIT || 
			eAction == ACTION_NOTIFY_GET_DEAD_UNITS_UPDATE ||
			eAction == ACTION_NOTIFY_NEW_ST_OBJ );
}
void CAIUnit::CheckForReveal()
{
	if ( !theDipl.IsNetGame() && nextRevealCheck <= curTime )
	{
		nextRevealCheck = curTime + 1000 + Random( 0, 1000 );
		if ( bQueredToReveal )
		{
			bQueredToReveal = false;
			if ( !bRevealed )
			{
				bRevealed = Random( 0.0f, 1.0f ) < SConsts::REVEAL_INFO[GetStats()->type].fRevealByQuery;
				if ( bRevealed )
					timeOfReveal = curTime;
			}

			vPlaceOfReveal = GetCenter();
		}
		else if ( bRevealed )
		{
			const int nType = GetStats()->type;
			bRevealed =
				timeOfReveal + SConsts::REVEAL_INFO[nType].nTimeOfReveal >= curTime &&
				fabs2( GetCenter() - vPlaceOfReveal ) < sqr(SConsts::REVEAL_INFO[nType].fForgetRevealDistance) && 
				Random( 0.0f, 1.0f ) < 1 - SConsts::REVEAL_INFO[nType].fRevealByMovingOff;
		}
	}
}
void CAIUnit::Segment()
{
	if ( bAlive )
		CCommonUnit::Segment();

	if ( pAntiArtillery )
	{
		pAntiArtillery->Segment( IsVisible( theDipl.GetMyParty() ) );
		if ( theDipl.GetNeutralPlayer() != GetPlayer() )
		{
			const int nEnemyParty = 1 - GetParty();
			pUnitInfoForGeneral->UpdateAntiArtFire( 
				pAntiArtillery->GetLastHeardTime( nEnemyParty ), 
				pAntiArtillery->GetRevealCircle( nEnemyParty ).center 
			);
		}
	}

	CalcVisibility();
	CheckForReveal();
	units.UpdateUnitVis4Enemy( this );
}
void CAIUnit::FreezeSegment()
{
	if ( !theDipl.IsNetGame() )
	{
		if ( GetPlayer() != theDipl.GetMyNumber() && !GetStats()->IsAviation() )
		{
			if ( bVisibleByPlayer && GetSpeed() != VNULL2 )
				theCombatEstimator.AddUnit( this );
			else
				theCombatEstimator.DelUnit( this );
		}

		pUnitInfoForGeneral->Segment();
	}

	if ( !IsValidObj( pTankPit ) )
		pTankPit = 0;

	if ( creationTime + 1000 < curTime )
		CCommonUnit::FreezeSegment();

	if ( GetState() && ( IsRestState( GetState()->GetName() ) || GetState()->GetName() == EUSN_USE_SPYGLASS ) )
		AnalyzeCamouflage();

	CalcVisibility();
	CheckForReveal();

	units.UpdateUnitVis4Enemy( this );
}
const bool CAIUnit::IsVisibleByPlayer()
{
	return bVisibleByPlayer;
}
void CAIUnit::CalcVisibility()
{
	if ( curTime >= creationTime + SConsts::AI_SEGMENT_DURATION * SConsts::SHOW_ALL_TIME_COEFF )
	{
		const bool bVisibility = 
						IsVisible( theDipl.GetMyParty() ) || IsVisible( theCheats.GetNPartyForWarFog() ) ||
						theCheats.IsHistoryPlaying() && !IsInSolidPlace()	|| theWarFog.IsOpenBySriptArea( GetTile() );

		if ( bVisibility != bVisibleByPlayer )
		{
			if ( bVisibility || lastTimeOfVis + SConsts::RESIDUAL_VISIBILITY_TIME < curTime )
			{
				bVisibleByPlayer = bVisibility;

				updater.Update( ACTION_NOTIFY_CHANGE_VISIBILITY, this, IsVisibleByPlayer() );

				if ( IsInTankPit() )
					UpdateTankPitVisibility();
				if ( GetStats()->IsArtillery() )
					(static_cast<CArtillery*>(this))->UpdateAmmoBoxVisibility();
			}
		}

		for ( int i = 0; i < theDipl.GetNPlayers(); ++i )
		{
			const bool bVisibility = IsVisible( theDipl.GetNParty( i ) );
			if ( bool( wWisibility & (1<<i) ) != bVisibility )
			{
				theSupremeBeing.SetUnitVisible( this, i, bVisibility );
				wWisibility ^= ( 1<<i );

				if ( theDipl.GetNeutralPlayer() != GetPlayer() && !theDipl.IsAIPlayer( GetPlayer() ) && theDipl.IsAIPlayer( i ) )
					pUnitInfoForGeneral->UpdateVisibility( bVisibility );
				
				if ( bVisibility && GetParty() == theDipl.GetNeutralParty() )
					UpdateUnitsRequestsForResupply();
			}
		}

		const int nUnitParty = GetParty();
		for ( int i = 0; i < 3; ++i )
			visible4Party[i] = CalculateUnitVisibility4Party( i );
	}
}
void CAIUnit::GetRPGStats( SAINotifyRPGStats *pStats ) 
{ 
	pStats->fHitPoints = GetHitPoints();
	pStats->pObj = this;
	pStats->fMorale = (fMorale - SConsts::MORALE_MIN_VALUE) / ( 1 - SConsts::MORALE_MIN_VALUE );

	pStats->nMainAmmo = pStats->nSecondaryAmmo = 0;
	for ( int i = 0; i < GetNCommonGuns(); ++i )
	{
		if ( GetCommonGunStats( i ).bPrimary )
			pStats->nMainAmmo += GetNAmmo( i );
		else
			pStats->nSecondaryAmmo += GetNAmmo( i );
	}
	
	pStats->time = curTime;
}
bool CAIUnit::CanCommandBeExecuted( CAICommand *pCommand )
{
	return GetStats()->HasCommand( int( pCommand->ToUnitCmd().cmdType ) );
}
bool CAIUnit::CanCommandBeExecutedByStats( CAICommand *pCommand )
{
	return CanCommandBeExecutedByStats( pCommand->ToUnitCmd().cmdType);
}
bool CAIUnit::CanCommandBeExecutedByStats( int nCmd ) const
{
	return GetStats()->HasCommand( nCmd );
}
const float CAIUnit::GetSightRadius() const
{
	return 
		Max( 0.0f,
				( GetStats()->fSight * GetSightMultiplier() * GetExpLevel().fBonusSight 	+ 
					(float)floor( theStaticMap.GetTileHeight( GetTile() ) / SConsts::HEIGHT_FOR_VIS_RADIUS_INC ) ) * SConsts::TILE_SIZE
 			 );
}
void CAIUnit::TakeDamage( const float fDamage, const SWeaponRPGStats::SShell *pShell, const int nPlayerOfShoot, CAIUnit *pShotUnit )
{
	if ( fHitPoints > 0 )
	{
		updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );

		const float fFormerHP = fHitPoints;
		const int nPartyOfShoot = theDipl.GetNParty( nPlayerOfShoot );
		if ( ( fHitPoints -= fDamage * ( 1 - theCheats.GetImmortals( GetPlayer() ) ) ) <= 0 || theCheats.GetFirstShoot( nPartyOfShoot ) == 1 )
		{
			if ( pShotUnit && pShotUnit->IsAlive() )
				pShotUnit->EnemyKilled( this );
			SendNTotalKilledUnits( nPlayerOfShoot );
			
			bool bDisappear = false;
			if ( !GetStats()->IsAviation() && theStaticMap.IsBridge( GetTile() ) )
			{
				theStaticMap.MemMode();
				theStaticMap.SetMode( ELM_STATIC );
				bDisappear = theStaticMap.IsLocked( GetTile(), AI_CLASS_ANY );

				if ( bDisappear )
				  theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_DISAPPEAR ), this, false );
				if ( GetStats()->IsInfantry() )
					theGraveyard.AddBridgeKilledSoldier( GetTile(), this );

				theStaticMap.RestoreMode();
			}

			if ( !bDisappear )
			{
				const bool bFromExpl = ( pShell != 0 && pShell->fArea2 != 0 );
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_DIE, fDamage, bFromExpl ), static_cast<CAIUnit*>( this ), false );
			}
		}
		else // юнит не умер
		{
			const SUnitBaseRPGStats *pStats = GetStats();
			if ( !pStats->IsAviation() )
			{
				const float fMaxHP = pStats->fMaxHP;
				if ( fFormerHP / fMaxHP > SConsts::LOW_HP_PERCENTAGE && fHitPoints / fMaxHP <= SConsts::LOW_HP_PERCENTAGE )
					RegisterAsBored( ACK_BORED_LOW_HIT_POINTS );

				if ( fFormerHP == fMaxHP && fHitPoints != fMaxHP )
					theSupremeBeing.UnitAskedForResupply( this, pStats->IsInfantry() ? ERT_MEDICINE : ERT_REPAIR, true );
			}
		}
	}
}
void CAIUnit::Fired( const float fGunRadius, const int nGun )
{
	if ( pAntiArtillery != 0 && fGunRadius != 0.0f )
		pAntiArtillery->Fired( fGunRadius, GetCenter() );

	if ( !GetStats()->IsAviation() )
	{
		const int nAmmo = GetNAmmo( nGun );
		const int nMaxAmmo = GetCommonGunStats( nGun ).nAmmo;
		if ( nAmmo < nMaxAmmo / 3 && ( nAmmo + 1 ) >= nMaxAmmo / 3 )
			theSupremeBeing.UnitAskedForResupply( this, ERT_RESUPPLY, true );
	}
}
void CAIUnit::UpdateUnitsRequestsForResupply()
{
	if ( !theDipl.IsNetGame() )
	{
		if ( !IsAlive() || !IsValid() ) return;
		if ( theDipl.GetNParty( GetPlayer() ) == theDipl.GetNeutralParty() )
		{
			if ( GetStats()->IsArtillery() )
				theSupremeBeing.UnitAskedForResupply( this, ERT_HUMAN_RESUPPLY, true );
		}
		else
		{
			const SUnitBaseRPGStats * pStats = GetStats();
			if ( !pStats->IsAviation() )
			{
				const int nCommonGuns = GetNCommonGuns();
				for ( int i = 0; i < nCommonGuns; ++i )
				{
					const int nAmmo = GetNAmmo( i );
					const int nMaxAmmo = GetCommonGunStats( i ).nAmmo;
					if ( nAmmo < nMaxAmmo / 3 )
					{
						theSupremeBeing.UnitAskedForResupply( this, ERT_RESUPPLY, true );
						break;
					}
				}
			}
			
			const float fMaxHP = pStats->fMaxHP;
			if ( GetHitPoints() != fMaxHP )
			{
				if ( pStats->IsInfantry() )
					theSupremeBeing.UnitAskedForResupply( this, ERT_MEDICINE, true );
				else if ( !pStats->IsAviation() )
					theSupremeBeing.UnitAskedForResupply( this, ERT_REPAIR, true );
			}
		}
	}
}
void CAIUnit::ChangePlayer( const BYTE cPlayer )
{
	if ( GetParty() != theDipl.GetNParty( cPlayer ) && !theDipl.IsNetGame() )
	{
		theSupremeBeing.UnitChangedParty( this, theDipl.GetNParty( cPlayer ) );
	}
	
	units.ChangePlayer( this, cPlayer );
	updater.Update( ACTION_NOTIFY_UPDATE_DIPLOMACY, this );
	
	theWarFog.ChangeUnitParty( GetID(), theDipl.GetNParty( cPlayer ) );
	const int nGroup = GetNGroup();
	if ( nGroup > 0 )
	{
		theGroupLogic.DelUnitFromGroup( this );
		if ( theGroupLogic.BeginGroup( nGroup ) == theGroupLogic.EndGroup() )
			theGroupLogic.UnregisterGroup( nGroup );
	}

	SetSelectable( GetPlayer() == theDipl.GetMyNumber() );

	UpdateUnitsRequestsForResupply();

	updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );

	const int nShellType = GetGuns()->GetActiveShellType();
	updater.Update( ACTION_NOTIFY_SHELLTYPE_CHANGED, this, nShellType );

	for ( int i = 0; i < GetNGuns(); ++i )
	{
		if ( GetGun(i)->GetOwner() == this )
			GetGun( i )->SetOwner( this );
	}
	
	if ( pAntiArtillery )
		pAntiArtillery->SetParty( GetParty() );
	
	if ( GetFormation() )
		GetFormation()->ChangePlayer( cPlayer );
}
void CAIUnit::SetPlayerForEditor( const int nPlayer )
{
	player = nPlayer;
}
float CAIUnit::GetRemissiveCoeff() const
{
	return GetStats()->fSmallAABBCoeff * theDifficultyLevel.GetSmallAABBCoeff( GetParty() );
}
const NTimer::STime CAIUnit::GetTimeToCamouflage() const
{
	if ( GetStats()->type == RPG_TYPE_SNIPER )
		return SConsts::TIME_BEFORE_SNIPER_CAMOUFLAGE;
	return SConsts::TIME_BEFORE_CAMOUFLAGE;
}
void CAIUnit::ChangeAmmo( const int nCommonGun, const int nAmmo ) 
{ 
	CUnitGuns * pGuns = GetGuns();
	const int nFormerAmmo = pGuns->GetNAmmo( nCommonGun );
	pGuns->ChangeAmmo( nCommonGun, nAmmo ); 
	updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );
}
void CAIUnit::CreateAntiArtillery( const float fMaxRevealRadius )
{
	pAntiArtillery = new CAntiArtillery( this );
	pAntiArtillery->Init( fMaxRevealRadius, GetParty() );
}
const float CAIUnit::GetKillSpeed( CAIUnit *pEnemy, const DWORD dwGuns ) const
{
	float fSpeed = 0;
	for ( int i = 0; i < sizeof(dwGuns) * 8 && i < GetNGuns(); ++i )
	{
		if ( (dwGuns & i<<i) )
			fSpeed += GetKillSpeed( pEnemy, GetGun( i ) );
	}

	return fSpeed;
}
const float CAIUnit::GetKillSpeed( const SHPObjectRPGStats *pStats, const CVec2 &vCenter, const DWORD dwGuns ) const
{
	float fSpeed = 0;
	for ( int i = 0; i < sizeof(dwGuns) * 8 && i < GetNGuns(); ++i )
	{
		if ( (dwGuns & i<<i) )
			fSpeed += GetKillSpeed( pStats, vCenter, GetGun( i ) );
	}

	return fSpeed;
}
const float CAIUnit::GetKillSpeed( const SHPObjectRPGStats *pStats, const CVec2 &vCenter, CBasicGun *pGun ) const
{
	float fPiercingProbability = 0.0f;

	static const float weights[4] = { 1.2f, 0.6f, 0.3f, 0.6f };
	
	const int nPiercing = pGun->GetPiercing();
	const int nPiercingRandom = pGun->GetPiercingRandom();
	const int nMinPiercing = nPiercing - nPiercingRandom;
	const int nMaxPiercing = nPiercing + nPiercingRandom;

	for ( int i = 0; i < 4; ++i )
	{
		if ( pGun->CanBreach( pStats, i ) )
		{
			const int nMinArmor = pStats->GetMinPossibleArmor( i );
			const int nMaxArmor = pStats->GetMaxPossibleArmor( i );

			fPiercingProbability += CalculateProbability( nMinPiercing, nMinArmor, nMaxPiercing, nMaxArmor ) * weights[i];
		}
	}

	fPiercingProbability /= ( weights[0] + weights[1] + weights[2] + weights[3] );

	if ( fPiercingProbability == 0.0f )
		return 0;
	else
	{
		const float fMaxHP = pStats->fMaxHP;

		const int nAmmoPerBurst = pGun->GetWeapon()->nAmmoPerBurst;
		const int nBursts = ( fMaxHP / fPiercingProbability ) / ( pGun->GetDamage() * nAmmoPerBurst ) + 1;

		const float fTimeToKill =
			pGun->GetAimTime( false ) + nBursts * ( pGun->GetRelaxTime( false ) + pGun->GetFireRate() * nAmmoPerBurst );

		NI_ASSERT_T( _finite( nBursts ) != 0, "Wrong nBursts (infinity)" );
		NI_ASSERT_T( _finite( fTimeToKill ) != 0, "Wrong fTimeToKill (infinity)" );
		NI_ASSERT_T( fTimeToKill != 0, "Wrong fTimeToKill (0)" );

		return fMaxHP / fTimeToKill;
	}
}
const float CAIUnit::GetKillSpeed( CAIUnit *pEnemy, CBasicGun *pGun ) const
{
	float fPiercingProbability = 0.0f;

	static const float weights[4] = { 1.2f, 0.6f, 0.3f, 0.6f };

	const int nPiercing = pGun->GetPiercing();
	const int nPiercingRandom = pGun->GetPiercingRandom();
	const int nMinPiercing = nPiercing - nPiercingRandom;
	const int nMaxPiercing = nPiercing + nPiercingRandom;

	for ( int i = 0; i < 4; ++i )
	{
		if ( pGun->CanBreach( pEnemy, i ) )
		{
			const int nMinArmor = pEnemy->GetMinPossibleArmor( i );
			const int nMaxArmor = pEnemy->GetMaxPossibleArmor( i );

			fPiercingProbability += CalculateProbability( nMinPiercing, nMinArmor, nMaxPiercing, nMaxArmor ) * weights[i];
		}
	}

	fPiercingProbability /= ( weights[0] + weights[1] + weights[2] + weights[3] );

	if ( fPiercingProbability == 0.0f )
		return 0;
	else
	{
		float fTimeToGo = 0;

		const float fMaxHP = pEnemy->GetStats()->fMaxHP;
		const float fCover = IsValidObj( pEnemy ) ? pEnemy->GetCover() : 1.0f;

		const int nAmmoPerBurst = pGun->GetWeapon()->nAmmoPerBurst;
		const int nBursts = ( fMaxHP / ( fPiercingProbability * fCover ) ) / ( pGun->GetDamage() * nAmmoPerBurst ) + 1;

		const float fTimeToKill = 
			pGun->GetAimTime( false ) + nBursts * ( pGun->GetRelaxTime( false ) + pGun->GetFireRate() * nAmmoPerBurst );

		NI_ASSERT_T( _finite( nBursts ) != 0, "Wrong nBursts (infinity)" );
		NI_ASSERT_T( _finite( fTimeToKill ) != 0, "Wrong fTimeToKill (infinity)" );
		NI_ASSERT_T( fTimeToKill != 0, "Wrong fTimeToKill (0)" );

		return fMaxHP / ( fTimeToKill + fTimeToGo );
	}
}
const float CAIUnit::GetKillSpeed( CAIUnit *pEnemy ) const
{
	int i = 0;
	while ( i < GetNGuns() && !GetGun(i)->CanShootToUnit( pEnemy ) )
		++i;

	if ( i >= GetNGuns() )
		return 0;

	return GetKillSpeed( pEnemy, GetGun( i ) );
}
void CAIUnit::UpdateTakenDamagePower( const float fUpdate ) 
{ 
	if ( fTakenDamagePower + fUpdate >= -0.0001f )
	{
		fTakenDamagePower += fUpdate; 
		if ( fTakenDamagePower < 0 && fTakenDamagePower >= -0.0001f )
			fTakenDamagePower = 0.0f;

		NI_ASSERT_T( fTakenDamagePower >= 0, "Wrong taken damage power" );
	}
}
void CAIUnit::ResetTargetScan()
{
	GetLastBehTime() = 0;
	targetScanRandom = Random( 0, 1000 );
}
void CAIUnit::ResetGunChoosing()
{
	GetLastBehTime() = curTime;
	targetScanRandom = Random( 0, 1000 );
}
CBasicGun* CAIUnit::AnalyzeGunChoose( CAIUnit *pEnemy )
{
	if ( curTime - GetLastBehTime() >= GetBehUpdateDuration() + targetScanRandom )
	{
		GetLastBehTime() = curTime;
		targetScanRandom = Random( 0, 1000 );

		ResetShootEstimator( pEnemy, false );
		return GetBestShootEstimatedGun();
	}
	else
		return 0;
}
IObstacle* CAIUnit::LookForObstacle()
{
	if ( CanShoot() && theSupremeBeing.MustShootToObstacles( GetPlayer() ) )
	{
		CShootEstimatorForObstacles estimator( this );
		theStatObjs.EnumObstaclesInRange( GetCenter(), GetSightRadius(), &estimator );
		
		return estimator.GetBest();
	}

	return 0;
}
const float CAIUnit::GetTargetScanRadius()
{
	if ( theDipl.IsAIPlayer( GetPlayer() ) && GetFirstArtilleryGun() != 0 && !DoesReservePosExist() )
		return GetFirstArtilleryGun()->GetFireRange( 0 );
	else if ( GetStats()->type == RPG_TYPE_OFFICER )
		return GetGun(0)->GetFireRange( 0 ) * SConsts::OFFICER_COEFFICIENT_FOR_SCAN;
	else if ( GetStats()->IsArtillery() )
		return Min( GetMaxFireRange(), SConsts::MAX_FIRE_RANGE_TO_SHOOT_BY_LINE );
	else
	{
		const float fCallForHelpRadius =
			theDipl.IsAIPlayer( GetPlayer() ) ? SConsts::AI_CALL_FOR_HELP_RADIUS : SConsts::CALL_FOR_HELP_RADIUS;

		return Max( fCallForHelpRadius, GetSightRadius() );
	}
}
void CAIUnit::LookForTarget( CAIUnit *pCurTarget, const bool bDamageUpdated, CAIUnit **pBestTarget, CBasicGun **pGun )
{
	if ( CanShoot() )
	{
		ResetShootEstimator( pCurTarget, bDamageUpdated );
		
		const float r = GetTargetScanRadius();
		const CVec2 vCenter( GetCenter() );

		for ( CUnitsIter<1,3> iter( GetParty(), EDI_ENEMY, vCenter, r );
				  theScanLimiter.CanScan() && !iter.IsFinished(); iter.Iterate() )
		{
			CAIUnit *pTarget = *iter;
			if ( IsValidObj( pTarget ) && pTarget->IsNoticableByUnit( this, r ) )
			{
				if ( pTarget->GetStats()->IsArtillery() )
				{
					CArtillery *pArt = checked_cast<CArtillery*>( pTarget ) ;
					if ( pArt->HasServeCrew() )
					{
						CFormation *pCrew = pArt->GetCrew();
						for ( int i=0; i < pCrew->Size(); ++i )
						{
							CSoldier *pSoldier = (*pCrew)[i];
							if ( pSoldier->IsNoticableByUnit( this, r ) )
								AddUnitToShootEstimator( pSoldier );
						}
					}
				}

				AddUnitToShootEstimator( pTarget );
			}
		}

		for ( CStObjCircleIter<true> iter( GetCenter(), r ); 
					theScanLimiter.CanScan() && !iter.IsFinished(); iter.Iterate() )
		{
			CExistingObject *pTarget = *iter;

			for ( int i = 0; i < pTarget->GetNDefenders(); ++i )
			{
				if ( theDipl.GetDiplStatus( GetPlayer(), pTarget->GetPlayer() ) == EDI_ENEMY && pTarget->GetUnit( i )->IsNoticableByUnit( this, r ) )
					AddUnitToShootEstimator( pTarget->GetUnit( i ) );
			}
		}

		*pBestTarget = GetBestShootEstimatedUnit();
		*pGun = GetBestShootEstimatedGun();

		if ( GetBestShootEstimatedUnit() == 0 && CanShootToPlanes() )
		{
			CShootEstimatorLighAA estimatorAA;
			estimatorAA.Init( this );
	
			for ( CPlanesIter iter; !iter.IsFinished(); iter.Iterate() )
			{
				if ( theDipl.GetDiplStatus( GetPlayer(), (*iter)->GetPlayer() ) == EDI_ENEMY )
					estimatorAA.AddUnit( *iter );
			}

			*pBestTarget = estimatorAA.GetBestUnit();
			*pGun = 0;
		}
	}
	else
	{
		*pBestTarget = 0;
		*pGun = 0;
	}
}
void CAIUnit::LookForFarTarget( CAIUnit *pCurTarget, const bool bDamageUpdated, CAIUnit **pBestTarget, CBasicGun **pGun )
{
	CAIUnit *pChosenTarget = *pBestTarget;
	CBasicGun *pChosenGun = *pGun;
	
	SetCircularAttack( true );
	bFreeEnemySearch = true;

	CAIUnit::LookForTarget( pCurTarget, bDamageUpdated, pBestTarget, pGun );
	if ( (*pBestTarget) != 0 )
	{
		const float fDamagePower = (*pBestTarget)->GetTakenDamagePower();

		if ( fDamagePower != 0 )
		{
			const float fKillEnemyTime = (*pBestTarget)->GetHitPoints() / fDamagePower;
			if ( fKillEnemyTime < 2000 && ( (*pBestTarget)->GetStats()->IsInfantry() || (*pBestTarget)->GetStats()->IsTransport() ) )
			{
				*pBestTarget = pChosenTarget;
				*pGun = pChosenGun;
			}
		}
	}
	else
	{
		*pBestTarget = pChosenTarget;
		*pGun = pChosenGun;
	}

	bFreeEnemySearch = false;
	SetCircularAttack( false );
}
void CAIUnit::SetCircularAttack( const bool bCanAttack )
{
	for ( int i = 0; i < GetNGuns(); ++i )
		GetGun( i )->SetCircularAttack( bCanAttack );
}
bool CAIUnit::IsTimeToAnalyzeTargetScan() const
{
	return 
		curTime - GetLastBehTime() >= GetBehUpdateDuration() + targetScanRandom;
}
BYTE CAIUnit::AnalyzeTargetScan( CAIUnit *pCurTarget, const bool bDamageUpdated, const bool bScanForObstacles, IRefCount *pCheckBuilding )
{
	if ( IsTimeToAnalyzeTargetScan() && theScanLimiter.CanScan() )
	{
		GetLastBehTime() = curTime;
		if ( GetStats()->IsInfantry() )
		{
			if ( GetState() && GetState()->IsAttackingState() )
				targetScanRandom = Random( 0, 3000 );
			else
				targetScanRandom = Random( 0, 2000 );
		}
		else
			targetScanRandom = Random( 0, 1000 );
		
		const SBehaviour &beh = GetBehaviour();
		CAICommand *pCommand = GetCurCmd();

		if ( beh.fire == SBehaviour::EFAtWill )
		{
			CAIUnit *pTarget = 0;
			CBasicGun *pGun = 0;

			LookForTarget( pCurTarget, bDamageUpdated, &pTarget, &pGun );

			if ( pTarget != 0 && pCheckBuilding != 0 && pTarget->GetStats()->IsInfantry() &&
					 static_cast<CSoldier*>(pTarget)->IsInBuilding() && static_cast<CSoldier*>(pTarget)->GetBuilding() == pCheckBuilding )
				pTarget = 0;

			if ( pTarget != 0 && ( pCurTarget == 0 || pTarget != pCurTarget ) )
			{
				if ( GetStats()->type == RPG_TYPE_ART_ROCKET )
					theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_ART_BOMBARDMENT, pTarget->GetCenter() ), this );
				else
					theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_SWARM_ATTACK_UNIT, pTarget ), this );

				return 3;
			}
			else if ( !pTarget && bScanForObstacles )
			{
				IObstacle *pObstacle = LookForObstacle();
				if ( pObstacle )
				{
					pObstacle->IssueUnitAttackCommand( this );
					return 3;
				}
			}
		}

		return 2;
	}
	else
		return 0;
}
void CAIUnit::SetAmbush()
{
	updater.Update( ACTION_NOTIFY_SET_AMBUSH, this );
}
void CAIUnit::RemoveAmbush()
{
	updater.Update( ACTION_NOTIFY_REMOVE_AMBUSH, this );
}
void CAIUnit::GetFogInfo( SFogInfo *pInfo ) const
{
	pInfo->pObject = 0;
	pInfo->center = GetTile();
	pInfo->bPlane = GetStats()->IsAviation();
	pInfo->fSightPower = GetStats()->fSightPower;

	if ( IsInSolidPlace() )
	{
		pInfo->values = 0;
		pInfo->r = 0;
		pInfo->wVisionAngle = 0;
	}
	else 
	{
		pInfo->r = GetSightRadius() / SConsts::TILE_SIZE;
		pInfo->wUnitDir = GetFrontDir();
		pInfo->wVisionAngle = GetVisionAngle();

		pInfo->bAngleLimited = false;
		pInfo->wMinAngle = 0;
		pInfo->wMaxAngle = 0;
	}
}
void CAIUnit::WarFogChanged()
{
	if ( IsAlive() )
	{
		bool bShouldUpdateWarfog = true;
		if ( CFormation *pFormation = GetFormation() )
		{
			IUnitState *pState = pFormation->GetState();
			bShouldUpdateWarfog = !pState || pState->GetName() != EUSN_GUN_CREW_STATE;
		}

		if ( bShouldUpdateWarfog )
		{
			SFogInfo fogInfo;
			GetFogInfo( &fogInfo );

			if ( fogInfo.bAngleLimited || fogInfo.wVisionAngle < 32768 )
				theWarFog.ChangeUnitState( id, fogInfo );
			else
				theWarFog.ChangeUnitCoord( id, fogInfo );
		}
	}
}
const int CAIUnit::ChooseFatality( const float fDamage )
{
	const SUnitBaseRPGStats *pStats = GetStats();

	if ( Random( 0.0f, 1.0f ) < SConsts::FATALITY_PROBABILITY ||
			 fDamage / GetStats()->fMaxHP > SConsts::DAMAGE_FOR_MASSIVE_DAMAGE_FATALITY && 
			 Random( 0.0f, 0.1f ) < SConsts::MASSIVE_DAMAGE_FATALITY_PROBABILITY )
	{
		if ( pStats->animdescs.size() > ANIMATION_DEATH_FATALITY && !pStats->animdescs[ANIMATION_DEATH_FATALITY].empty() && !pStats->aabb_as.empty() )
		{
			const int nFatality = Random( pStats->animdescs[ANIMATION_DEATH_FATALITY].size() );

			const int nRect = pStats->animdescs[ANIMATION_DEATH_FATALITY][nFatality].nAABB_A;
			NI_ASSERT_SLOW_T( nRect != -1, NStr::Format("Wrong fatality %d AABB for unit \"%s\"", nFatality, pStats->szParentName.c_str()) );
			SRect fatalityRect;

			const CVec2 vFrontDir = GetVectorByDirection( GetFrontDir() );
			const CVec2 vRectTurn( vFrontDir.y, -vFrontDir.x );
			
			fatalityRect.InitRect( GetCenter() + ( (pStats->aabb_as[nRect].vCenter) ^ vRectTurn ), vFrontDir, 
														 pStats->aabb_as[nRect].vHalfSize.y, pStats->aabb_as[nRect].vHalfSize.x );

			UnlockTiles();
			bool bFree = IsMapFullyFree( fatalityRect, this );
			LockTiles();

			if ( bFree )
				return nFatality;
		}
	}

	if ( pStats->animdescs[ANIMATION_DEATH].empty() )
		return -1;
	else
		return -1 * int( Random( pStats->animdescs[ANIMATION_DEATH].size() ) + 1 );
}
void CAIUnit::SetMoraleSupport()
{
	bHasMoraleSupport = true;
}
void CAIUnit::SetMorale( float _fMorale )
{ 
	_fMorale = _fMorale <= SConsts::MORALE_MIN_VALUE ? SConsts::MORALE_MIN_VALUE : _fMorale ;
	if( fMorale != _fMorale )
	{
		if ( SConsts::MORALE_MIN_VALUE != fMorale && SConsts::MORALE_MIN_VALUE == _fMorale )
			RegisterAsBored( ACK_BORED_MINIMUM_MORALE );
		else if ( SConsts::MORALE_MIN_VALUE == fMorale && SConsts::MORALE_MIN_VALUE != _fMorale )
			UnRegisterAsBored( ACK_BORED_MINIMUM_MORALE );
		
		if ( _fMorale == SConsts::MORALE_MIN_VALUE )
			theSupremeBeing.UnitAskedForResupply( this, ERT_MORALE, true );
		if ( _fMorale == 1.0f )
			theSupremeBeing.UnitAskedForResupply( this, ERT_MORALE, false );

		fMorale = _fMorale; 
		updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );
	}
}
void CAIUnit::InitializeShootArea( SShootArea *pArea, CBasicGun *pGun, const float fRangeMin, const float fRangeMax ) const
{
	pArea->vCenter3D = CVec3( GetCenter(), 0.0f );
	pArea->fMinR = fRangeMin;
	pArea->fMaxR = fRangeMax;
	
	if ( NeedDeinstall() || !CanMove() && !CanRotate() )
	{
		const WORD wConstraint = 
			pGun ?
			pGun->GetTurret() ? pGun->GetTurret()->GetHorTurnConstraint() : pGun->GetWeapon()->wDeltaAngle :
			65535;

		if ( wConstraint >= 32767 )
		{
			pArea->wStartAngle = 65535;
			pArea->wFinishAngle = 65535;
		}
		else
		{
			const WORD wFrontDir = GetFrontDir();
			pArea->wStartAngle = wFrontDir - wConstraint;
			pArea->wFinishAngle = wFrontDir + wConstraint;
		}
	}
}
void CAIUnit::GetShootAreas( SShootAreas *pShootAreas, int *pnAreas ) const
{
	*pShootAreas = SShootAreas();
	*pnAreas = 1;

	if ( GetFirstArtilleryGun() != 0 )
	{
		pShootAreas->areas.push_back( SShootArea() );
		pShootAreas->areas.back().eType = SShootArea::ESAT_BALLISTIC;
	
		CBasicGun *pGun = GetFirstArtilleryGun();
		InitializeShootArea( &(pShootAreas->areas.back()), pGun, pGun->GetWeapon()->fRangeMin, pGun->GetFireRangeMax() );
	}
	{
		pShootAreas->areas.push_back( SShootArea() );
		pShootAreas->areas.back().eType = SShootArea::ESAT_LINE;

		float fMaxFireRange = -1.0f;
		float fMinFireRange = 1000000.0f;
		CBasicGun *pGun = 0;
		for ( int i = 0; i < GetNGuns(); ++i )
		{
			if ( GetGun( i )->GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE ||
					 GetGun( i )->GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_GRENADE
					)
			{
				CBasicGun *pGunI = GetGun( i );
				const float fLocalFireRange = pGunI->GetFireRange( 0.0f );
				if ( fLocalFireRange > fMaxFireRange )
					fMaxFireRange = fLocalFireRange;

				const float fLocalMinFireRange = pGunI->GetWeapon()->fRangeMin;
				if ( fLocalMinFireRange < fMinFireRange )
					fMinFireRange = fLocalMinFireRange;

				if ( !pGun || !pGun->IsOnTurret() && pGunI->IsOnTurret() )
					pGun = pGunI;
				else if ( pGun && !pGun->IsOnTurret() && !pGunI->IsOnTurret() )
				{
					if ( pGun->GetWeapon()->wDeltaAngle < pGunI->GetWeapon()->wDeltaAngle )
						pGun = pGunI;
				}
				else if ( pGun && pGun->IsOnTurret() && pGunI->IsOnTurret() )
				{
					if ( pGun->GetHorTurnConstraint() < pGunI->GetHorTurnConstraint() )
						pGun = pGunI;
				}
			}
		}
		if ( fMaxFireRange != -1.0f )
			InitializeShootArea( &(pShootAreas->areas.back()), pGun, fMinFireRange, fMaxFireRange );
	}

	if ( GetStats()->type == RPG_TYPE_ART_AAGUN )
	{
		pShootAreas->areas.push_back( SShootArea() );

		pShootAreas->areas.back().eType = SShootArea::ESAT_AA;
		InitializeShootArea( &(pShootAreas->areas.back()), 0, 0.0f, GetMaxFireRange() );
	}
}
bool CAIUnit::IsInTankPit() const
{
	return IsValidObj( pTankPit );
}
void CAIUnit::SetInTankPit( CExistingObject *_pTankPit )
{ 
	pTankPit = _pTankPit;
	if ( IsInTankPit() )
		UpdateTankPitVisibility();
}
void CAIUnit::UpdateTankPitVisibility()
{
	if ( IsVisibleByPlayer() )
		updater.Update( ACTION_NOTIFY_NEW_ST_OBJ, GetTankPit() );
	else
		updater.Update(	ACTION_NOTIFY_DELETED_ST_OBJ, GetTankPit() );
}
void CAIUnit::SetOffTankPit()
{
	if ( IsValidObj( pTankPit ) )
		theStatObjs.DeleteInternalObjectInfo( pTankPit );

	pTankPit = 0;
}
void CAIUnit::SetCamoulfage()
{
	if ( GetStats()->fCamouflage != 0 )
	{
		fCamouflage = 1.0f - GetStats()->fCamouflage;
		updater.Update( ACTION_NOTIFY_SET_CAMOUFLAGE, this );
	}
}
void CAIUnit::RemoveCamouflage( ECamouflageRemoveReason eReason )
{
	if ( GetStats()->fCamouflage != 0 )
	{
		fCamouflage = 1.0f;
		updater.Update( ACTION_NOTIFY_REMOVE_CAMOUFLAGE, this );
	}
}
void CAIUnit::StartCamouflating()
{
	camouflateTime = curTime;
}
void CAIUnit::InitAviationPath()
{
	const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats*>( GetStats() );
	pPathUnit->InitAviationPath( pStats );
}
void CAIUnit::AnalyzeCamouflage()
{
	if ( GetStats()->fCamouflage != 0 )
	{
		if ( !IsIdle() )
		{
			camouflateTime = curTime;
			if ( IsCamoulflated() )
				RemoveCamouflage( ECRR_SELF_MOVE );
		}
		else
		{
			const BYTE cParty = theDipl.GetNParty( player );
			if ( cParty != theDipl.GetNeutralParty() && IsVisible( 1 - cParty ) ||
					 cParty == theDipl.GetNeutralParty() && ( IsVisible( 0 ) || IsVisible( 1 ) ) )
			{
				camouflateTime = curTime;
				if ( IsCamoulflated() )
					RemoveCamouflage( ECRR_GOOD_VISIBILITY );
			}
			else if ( !IsCamoulflated() && curTime - camouflateTime >= GetTimeToCamouflage() )
				SetCamoulfage();
		}
	}
}
void CAIUnit::EnemyKilled( CAIUnit *pEnemy )
{
	if ( pEnemy && pEnemy->IsValid() &&
			 theDipl.GetDiplStatusForParties( GetParty(), pEnemy->GetParty() ) == EDI_ENEMY )
	{
		if ( pEnemy->IsVisible( GetParty() ) )
		{
			const EUnitRPGType eType = pEnemy->GetStats()->type;
			
			if ( ::IsInfantry( eType ) )
				SendAcknowledgement( ACK_KILLED_ENEMY_INFANTRY, true );
			else if ( ::IsAviation( eType ) )
				SendAcknowledgement( ACK_KILLED_ENEMY_AVIATION, true );
			else if ( ::IsArmor( eType ) || ::IsSPG( eType ) || ::IsTrain( eType ) )
				SendAcknowledgement( ACK_KILLED_ENEMY_TANK, true );
			else
				SendAcknowledgement( ACK_KILLED_ENEMY, true );
		}
		
		theStatistics.IncreasePlayerExperience( GetPlayer(), pEnemy->GetPriceMax() );

		float fPrice = pEnemy->GetStats()->fPrice;
		if ( pEnemy->IsInFormation() )
		{
			CFormation* pFormation = pEnemy->GetFormation();
			if ( pFormation->Size() == 1 && (*pFormation)[0] == pEnemy && pFormation->GetState()->GetName() == EUSN_GUN_CREW_STATE )
			{
				CAIUnit *pArtillery = checked_cast<CFormationGunCrewState*>( pFormation->GetState() )->GetArtillery();
				theStatistics.IncreasePlayerExperience( GetPlayer(), pFormation->GetPriceMax() );
				fPrice += pArtillery->GetStats()->fPrice;
			}
		}

		if ( GetScenarioUnit() != 0 )
		{
			if ( nLevel < pExpLevels->levels.size() - 1 )
			{
				fExperience += fPrice;
				if ( nLevel == pExpLevels->levels.size() - 2 )
					fExperience = Min( fExperience, float( pExpLevels->levels[nLevel+1].nExp ) );

				GetScenarioUnit()->SetValue( STUT_EXP, fExperience );

				if ( fExperience >= pExpLevels->levels[nLevel+1].nExp )
				{
					++nLevel;
					GetScenarioUnit()->SetValue( STUT_LEVEL, nLevel );
					theStatistics.UnitLeveledUp( this );

					SFogInfo fogInfo;
					GetFogInfo( &fogInfo );
					theWarFog.ChangeUnitState( id, fogInfo );
					
					updater.Update( ACTION_NOTIFY_LEVELUP, this, nLevel );
				}
			}
		}
	}
}
bool CAIUnit::IsNoticableByUnit( CCommonUnit *pUnit, const float fNoticeRadius )
{
	const bool bRadiusOk = fabs2( pUnit->GetCenter() - GetCenter() ) <= sqr( fNoticeRadius );

	return 
		bRadiusOk && ( IsVisible( pUnit->GetParty() ) || IsRevealed() && pUnit->CanMove() && !pUnit->NeedDeinstall() );
}
void CAIUnit::UpdateArea( const EActionNotify eAction ) 
{
	updater.Update( eAction, this );
}
void CAIUnit::SendAcknowledgement( CAICommand *pCommand, EUnitAckType ack, bool bForce )
{
	const SUnitBaseRPGStats *pStats = GetStats();
	if ( pStats && GetPlayer() == theDipl.GetMyNumber() && ( bForce || pCommand && pCommand->IsValid() && !pCommand->IsFromAI() ) )
	{
		bool bSend = false;
		int nSet = 0;
		if ( pStats->IsInfantry() && GetStats()->type == RPG_TYPE_ENGINEER )
		{
			IUnitState *pCurrentState = static_cast<CSoldier*>(this)->GetFormation()->GetState();
			if ( pCurrentState && pCurrentState->GetName() == EUSN_GUN_CREW_STATE )
			{
				CArtillery* pArtillery = static_cast<CFormationGunCrewState*>(pCurrentState)->GetArtillery();
				bSend = true;
				if ( pArtillery->GetStats()->type == RPG_TYPE_ART_HEAVY_GUN || pArtillery->GetStats()->type == RPG_TYPE_ART_HOWITZER )
					nSet = 1;
			}
		}
		else if ( pStats->IsTrain() && ( ack == ACK_CANNOT_MOVE_NEED_TO_BE_TOWED_TO_MOVE || ack == ACK_CANNOT_MOVE_TRACK_DAMAGED ) )
		{
			bSend = true;
			ack = ACK_NEGATIVE; //ACK_CANNOT_FIND_PATH_TO_TARGET;
		}
		else if ( ack == ACK_CANNOT_MOVE_NEED_TO_BE_TOWED_TO_MOVE )
			bSend = ( pStats->IsArtillery() || pStats->IsInfantry() );
		else
			bSend = true;

		if ( bSend )
			theAckManager.AddAcknowledgment( ack, this, nSet );
	}
}
void CAIUnit::SendAcknowledgement( EUnitAckType ack, bool bForce )
{
	SendAcknowledgement( GetCurCmd(), ack, bForce );
}
EUnitAckType CAIUnit::GetGunsRejectReason() const
{
	EUnitAckType eRejectReason = GetGuns()->GetRejectReason();
	return ( eRejectReason == ACK_NONE ) ? ACK_NEGATIVE : eRejectReason;
}
bool CAIUnit::DoesExistRejectGunsReason( const EUnitAckType &ackType ) const
{
	return GetGuns()->DoesExistRejectReason( ackType );
}
const int CAIUnit::GetMinArmor() const
{
	return GetStats()->nMinArmor;
}
const int CAIUnit::GetMaxArmor() const
{
	return GetStats()->nMaxArmor;
}
const int CAIUnit::GetMinPossibleArmor( const int nSide ) const
{
	return GetStats()->GetMinPossibleArmor( nSide );
}
const int CAIUnit::GetMaxPossibleArmor( const int nSide ) const
{
	return GetStats()->GetMaxPossibleArmor( nSide );
}
const int CAIUnit::GetArmor( const int nSide ) const
{
	return GetStats()->GetArmor( nSide );
}
const int CAIUnit::GetRandomArmor( const int nSide ) const
{
	return GetStats()->GetRandomArmor( nSide );
}
const float CAIUnit::GetCover() const 
{ 
	return IsInTankPit() ? SConsts::TANKPIT_COVER : 1.0f;
}
bool CAIUnit::IsSavedByCover() const
{ 
	return 
		Random( 0.0f, 1.0f ) >= GetCover();
}
const int CAIUnit::GetNCommonGuns() const { return GetGuns()->GetNCommonGuns(); }
const SBaseGunRPGStats& CAIUnit::GetCommonGunStats( const int nCommonGun ) const { return GetGuns()->GetCommonGunStats( nCommonGun ); }
int CAIUnit::GetNAmmo( const int nCommonGun ) const { return GetGuns()->GetNAmmo( nCommonGun ); }
bool CAIUnit::IsCommonGunFiring( const int nCommonGun ) const { return GetGuns()->IsCommonGunFiring( nCommonGun ); }
void CAIUnit::SetRightDir( bool bRightDir ) { pPathUnit->SetRightDir( bRightDir ); }
void CAIUnit::GetSpeed3( CVec3 *pSpeed ) const { pPathUnit->GetSpeed3( pSpeed ); }
const CVec2& CAIUnit::GetCenter() const { return pPathUnit->GetCenter(); }
const SVector CAIUnit::GetTile() const { return pPathUnit->GetTile(); }
const float CAIUnit::GetRotateSpeed() const { return pPathUnit->GetRotateSpeed(); }
const float CAIUnit::GetMaxPossibleSpeed() const { return pPathUnit->GetMaxPossibleSpeed(); }
const float CAIUnit::GetPassability() const { return GetStats()->fPassability; }
const CVec2& CAIUnit::GetSpeed() const { return pPathUnit->GetSpeed(); }
float CAIUnit::GetSpeedLen() const { return pPathUnit->GetSpeedLen(); }
const int CAIUnit::GetBoundTileRadius() const { return pPathUnit->GetBoundTileRadius(); }
const WORD CAIUnit::GetDir() const { return pPathUnit->GetDir(); }
const WORD CAIUnit::GetFrontDir() const { return pPathUnit->GetFrontDir(); }
const CVec2& CAIUnit::GetDirVector() const { return pPathUnit->GetDirVector(); }
const CVec2 CAIUnit::GetAABBHalfSize() const { return pPathUnit->GetAABBHalfSize(); }
void CAIUnit::SetCoordWOUpdate( const CVec3 &newCenter ) { pPathUnit->SetCoordWOUpdate( newCenter ); }
void CAIUnit::SetNewCoordinates( const CVec3 &newCenter, bool bStopUnit ) 
{ 
	pPathUnit->SetNewCoordinates( newCenter, bStopUnit );
}
void CAIUnit::SetNewCoordinatesForEditor( const CVec3 &newCenter ) { pPathUnit->SetNewCoordinatesForEditor( newCenter ); }
bool CAIUnit::CanSetNewCoord( const CVec3 &newCenter )	const { return pPathUnit->CanSetNewCoord( newCenter ); }
const SRect CAIUnit::GetUnitRectForLock() const { return pPathUnit->GetUnitRectForLock(); }
bool CAIUnit::TurnToDir( const WORD &newDir, const bool bCanBackward, const bool bForward ) 
{ 
	const bool bResult = pPathUnit->TurnToDir( newDir, bCanBackward, bForward ); 
	if ( GetVisionAngle() != 32768 )
		WarFogChanged();

	return bResult;
}
void CAIUnit::UpdateDirection( const CVec2 &newDir )
{ 
	pPathUnit->UpdateDirection( newDir ); 

	if ( GetVisionAngle() != 32768 )
		WarFogChanged();
}
void CAIUnit::UpdateDirectionForEditor( const CVec2 &newDir ) { pPathUnit->UpdateDirectionForEditor( newDir ); }
void CAIUnit::UpdateDirection( const WORD newDir )
{ 
	pPathUnit->UpdateDirection( newDir );

	if ( GetVisionAngle() != 32768 )
		WarFogChanged();
}
bool CAIUnit::CanSetNewDir( const CVec2 &newDir ) const { return pPathUnit->CanSetNewDir( newDir ); }
bool CAIUnit::IsIdle() const { return pPathUnit->IsIdle(); }
bool CAIUnit::IsTurning() const { return pPathUnit->IsTurning(); }
void CAIUnit::StopUnit() { pPathUnit->StopUnit(); }
void CAIUnit::StopTurning() { pPathUnit->StopTurning(); }
IStaticPathFinder* CAIUnit::GetPathFinder() const { return pPathUnit->GetPathFinder(); }
ISmoothPath* CAIUnit::GetCurPath() const { return pPathUnit->GetCurPath(); }
void CAIUnit::SetCurPath( interface ISmoothPath * newPath ){ pPathUnit->SetCurPath(newPath); }
void CAIUnit::RestoreDefaultPath(){pPathUnit->RestoreDefaultPath();}
const SVector CAIUnit::GetLastKnownGoodTile() const { return pPathUnit->GetLastKnownGoodTile(); }
bool CAIUnit::SendAlongPath( IStaticPath *pStaticPath, const CVec2 &vShift, bool bSmoothTurn ) 
{ 
	if ( IsInTankPit() )
	{
		theGroupLogic.InsertUnitCommand( SAIUnitCmd(ACTION_MOVE_LEAVE_TANK_PIT), this );
		return true;
	}
	else
		return pPathUnit->SendAlongPath( pStaticPath, vShift, bSmoothTurn );
}
bool CAIUnit::SendAlongPath( IPath *pPath ) 
{ 
	if ( IsInTankPit() )
	{
		theGroupLogic.InsertUnitCommand( SAIUnitCmd(ACTION_MOVE_LEAVE_TANK_PIT), this );
		return true;
	}
	else
		return pPathUnit->SendAlongPath( pPath );
}
void CAIUnit::LockTiles( bool bUpdate ) { pPathUnit->LockTiles( bUpdate ); }
void CAIUnit::ForceLockingTiles( bool bUpdate ) { pPathUnit->ForceLockingTiles( bUpdate ); }
void CAIUnit::LockTilesForEditor() { pPathUnit->LockTilesForEditor(); }
void CAIUnit::UnlockTiles(  const bool bUpdate ) { pPathUnit->UnlockTiles( bUpdate ); }
const float CAIUnit::GetZ() const { return pPathUnit->GetZ(); }
const WORD CAIUnit::GetDirAtTheBeginning() const { return pPathUnit->GetDirAtTheBeginning(); }	
const SRect CAIUnit::GetUnitRect() const { return pPathUnit->GetUnitRect(); }
void CAIUnit::FirstSegment() 
{ 
	pPathUnit->FirstSegment(); 
}
void CAIUnit::SecondSegment( const bool bUpdate ) { pPathUnit->SecondSegment( bUpdate ); }
void CAIUnit::FixUnlocking() { pPathUnit->FixUnlocking(); }
void CAIUnit::UnfixUnlocking() { pPathUnit->UnfixUnlocking(); }
bool CAIUnit::CanTurnToFrontDir( const WORD wDir ) { return pPathUnit->CanTurnToFrontDir( wDir ); }
float CAIUnit::GetSmoothTurnThreshold() const { return 0.3f; }
void CAIUnit::NullCollisions() { pPathUnit->NullCollisions(); }
void CAIUnit::ForceGoByRightDir() { pPathUnit->ForceGoByRightDir(); }
bool CAIUnit::IsLockingTiles() const { return pPathUnit->IsLockingTiles(); }
bool CAIUnit::CanRotateTo( SRect smallRect, const CVec2 &vNewDir, bool bWithUnits, bool bCanGoBackward ) const { return pPathUnit->CanRotateTo( smallRect, vNewDir, bWithUnits, bCanGoBackward ); }
bool CAIUnit::CheckToTurn( const WORD wNewDir ) { return pPathUnit->CheckToTurn( wNewDir ); }
bool CAIUnit::HasSuspendedPoint() const { return pPathUnit->HasSuspendedPoint(); }
IStaticPath* CAIUnit::CreateBigStaticPath( const CVec2 &vStartPoint, const CVec2 &vFinishPoint, interface IPointChecking *pPointChecking ) { return pPathUnit->CreateBigStaticPath( vStartPoint, vFinishPoint, pPointChecking ); }
void CAIUnit::UnRegisterAsBored( const enum EUnitAckType eBoredType )
{
	theAckManager.UnRegisterAsBored( eBoredType, this );
}
void CAIUnit::RegisterAsBored( const enum EUnitAckType eBoredType )
{
	theAckManager.RegisterAsBored( eBoredType, this );
}
bool CAIUnit::IsInOneTrain( IBasePathUnit *pUnit ) const
{
	return pPathUnit->IsInOneTrain( pUnit );
}
bool CAIUnit::IsTrain() const
{
	return GetStats()->IsTrain();
}
const float CAIUnit::GetTimeToForget() const
{
	return SGeneralConsts::TIME_TO_FORGET_UNIT;
}
CAIUnitInfoForGeneral* CAIUnit::GetUnitInfoForGeneral() const
{
	return pUnitInfoForGeneral;
}
void CAIUnit::SetLastVisibleTime( const NTimer::STime time )
{
	pUnitInfoForGeneral->SetLastVisibleTime( time );
}
bool CAIUnit::CanMove() const
{ 
	return
		GetBehaviour().moving != SBehaviour::EMHoldPos &&
		GetStats()->fSpeed != 0 && pPathUnit->CanMove();
}
bool CAIUnit::CanMovePathfinding() const
{
	return
		GetStats()->fSpeed != 0 && pPathUnit->CanMove();
}
bool CAIUnit::CanRotate() const
{
	return pPathUnit->CanRotate();
}
float CAIUnit::GetPriceMax() const
{
	return GetStats()->fPrice;
}
const SAIExpLevel::SLevel& CAIUnit::GetExpLevel() const
{
	return pExpLevels->levels[nLevel];
}
void CAIUnit::UnitCommand( CAICommand *pCommand, bool bPlaceInQueue, bool bOnlyThisUnitCommand )
{
	if ( !bOnlyThisUnitCommand )
	{
		GetBehaviour().moving = SBehaviour::EMRoaming;
		if ( GetStats()->IsInfantry() )
			UnlockTiles();
	}

	CCommonUnit::UnitCommand( pCommand, bPlaceInQueue, bOnlyThisUnitCommand );
}
void CAIUnit::Lock( const CBasicGun *pGun )
{
	if ( !GetStats()->IsTrain() )
		CCommonUnit::Lock( pGun );
}
void CAIUnit::Unlock( const CBasicGun *pGun )
{
	if ( !GetStats()->IsTrain() )
		CCommonUnit::Unlock( pGun );
}
bool CAIUnit::IsLocked( const CBasicGun *pGun ) const
{
	if ( GetStats()->IsTrain() )
		return true;
	else
		return CCommonUnit::IsLocked( pGun );
}
void CAIUnit::SetActiveShellType( const enum SWeaponRPGStats::SShell::EDamageType eShellType )
{
	if ( GetGuns()->SetActiveShellType( eShellType ) )
		updater.Update( ACTION_NOTIFY_SHELLTYPE_CHANGED, this, static_cast<int>(eShellType) );
}
void CAIUnit::AnimationSet( int nAnimation )
{
	pAnimUnit->AnimationSet( nAnimation );
}
void CAIUnit::AnimationSegment()
{
	pAnimUnit->Segment();
}
void CAIUnit::Moved()
{
	pAnimUnit->Moved();
}
void CAIUnit::Stopped()
{
	pAnimUnit->Stopped();
}
void CAIUnit::StopCurAnimation()
{
	pAnimUnit->StopCurAnimation();
}
void CAIUnit::TrackDamagedState( const bool bTrackDamaged )
{
	pPathUnit->TrackDamagedState( bTrackDamaged );
}
bool CAIUnit::IsColliding() const
{ 
	return true;
}
void CAIUnit::WantedToReveal( CAIUnit *pWhoRevealed )
{
	if ( !theDipl.IsNetGame() &&
			 theDipl.GetMyNumber() == GetPlayer() && 
			 theDipl.GetDiplStatus( pWhoRevealed->GetPlayer(), GetPlayer() ) == EDI_ENEMY )
		bQueredToReveal = true;
}
bool CAIUnit::IsRevealed() const
{
	return bRevealed;
}
bool CAIUnit::IsInfantry() const
{
	return GetStats()->IsInfantry();
}
const NTimer::STime CAIUnit::GetNextSecondPathSegmTime() const
{
	return pPathUnit->GetNextSecondPathSegmTime();
}
bool CAIUnit::CanMoveAfterUserCommand() const
{
	return GetStats()->HasCommand( ACTION_COMMAND_MOVE_TO ) && 
				 GetStats()->fSpeed != 0 && pPathUnit->CanMove();
}
