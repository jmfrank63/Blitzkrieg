#include "stdafx.h"

#include "Soldier.h"
#include "SoldierStates.h"
#include "InBuildingStates.h"
#include "InTransportStates.h"
#include "InEntrenchmentStates.h"
#include "Updater.h"
#include "Entrenchment.h"
#include "Building.h"
#include "Units.h"
#include "UnitsIterators2.h"
#include "Guns.h"
#include "HitsStore.h"
#include "AIWarFog.h"
#include "StaticObjects.h"
#include "Mine.h"
#include "Technics.h"
#include "UnitGuns.h"
#include "Formation.h"
#include "Shell.h"
#include "FormationStates.h"
#include "Artillery.h"
#include "Commands.h"
#include "AIStaticMap.h"
#include "WarFogTracer.h"
#include "Diplomacy.h"
#include "ShootEstimatorInternal.h"
#include "General.h"
#include "DifficultyLevel.h"
#include "Artillery.h"
#include "StaticObjectsIters.h"

// for profiling
#include "TimeCounter.h"
#include "MPLog.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CSupremeBeing theSupremeBeing;
extern CDiplomacy theDipl;
extern CUpdater updater;
extern NTimer::STime curTime;
extern CUnits units;
extern CHitsStore theHitsStore;
extern CGlobalWarFog theWarFog;
extern CStaticObjects theStatObjs;
extern CStaticMap theStaticMap;
extern CDifficultyLevel theDifficultyLevel;

extern CTimeCounter timeCounter;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													CInfantry																*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CSoldier );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::AllowLieDown( bool _bAllowLieDown )
{
	bAllowLieDown = _bAllowLieDown;
	
	if ( !bAllowLieDown )
		StandUp();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::InitGuns()
{
	SetAngles( 0, 65535 );
	pGuns = new CInfantryGuns;
	pGuns->Init( this );

	SetShootEstimator( new CSoldierShootEstimator( this ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IStatesFactory* CSoldier::GetStatesFactory() const
{
	if ( IsInBuilding() ) 
		return CInBuildingStatesFactory::Instance();
	/*else if ( IsInEntrenchment() )
		return CInEntrenchmentStatesFactory::Instance();*/
	else if ( IsInTransport() )
		return CInTransportStatesFactory::Instance();
	else
		return CSoldierStatesFactory::Instance();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBuilding* CSoldier::GetBuilding() const
{
	NI_ASSERT_T( IsInBuilding(), "Soldier isn't in a building" );
	return static_cast<CBuilding*>( pObjInside );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEntrenchment* CSoldier::GetEntrenchment() const
{
	NI_ASSERT_T( IsInEntrenchment(), "Soldier isn't in entrenchment" );
	return static_cast<CEntrenchment*>( pObjInside );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CMilitaryCar* CSoldier::GetTransportUnit() const
{
	NI_ASSERT_T( IsInTransport(), "Soldier isn't in a transport" );
	return static_cast<CMilitaryCar*>( pObjInside );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::ChangeWarFogState()
{
	IUnitState *pState = GetFormation()->GetState();

	if ( !pState || pState->GetName() != EUSN_GUN_CREW_STATE )
	{
		SFogInfo fogInfo;
		GetFogInfo( &fogInfo );
		theWarFog.ChangeUnitState( GetID(), fogInfo );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::SetInBuilding( CBuilding *pBuilding )
{
	pObjInside = pBuilding;
	eInsideType = EOIO_BUILDING;
	SetVisionAngle( 0 );

	updater.Update( ACTION_NOTIFY_ENTRANCE_STATE, this );
	ChangeWarFogState();
	
	slotInfo.nSlot = -1;
	
	pBuilding->AddInsider( this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::SetInEntrenchment( CEntrenchment *pEntrenchment )
{
	StopUnit();	
	
	pObjInside = pEntrenchment;
	eInsideType = EOIO_ENTRENCHMENT;
//	SetVisionAngle( 0 );
	updater.Update( ACTION_NOTIFY_ENTRANCE_STATE, this );

	pEntrenchment->AddInsider( this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::SetInTransport( class CMilitaryCar *pUnit )
{
	pObjInside = pUnit;
	eInsideType = EOIO_TRANSPORT;
	// �� �����
	if ( pUnit->GetStats()->IsArmor() || pUnit->GetStats()->IsSPG() )
		SetToFirePlace();
	else
	{
		SetVisionAngle( 0 );
		SetToSolidPlace();
	}

	updater.Update( ACTION_NOTIFY_ENTRANCE_STATE, this );
	ChangeWarFogState();
	SetSelectable( false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::SetFree()
{
	SetVisionAngle( SConsts::STANDART_VIS_ANGLE );
	SetSightMultiplier( 1 );
	SetAngles( 0, 65535 );

	if ( bInFirePlace )
		bInFirePlace = false;
	else if ( bInSolidPlace )
	{
		units.AddUnitToCell( this, true );
		bInSolidPlace = false;
	}

	if ( eInsideType == EOIO_TRANSPORT && GetPlayer() == theDipl.GetMyNumber() )
		SetSelectable( true );
	
	if ( eInsideType == EOIO_BUILDING || eInsideType ==  EOIO_ENTRENCHMENT || eInsideType == EOIO_TRANSPORT)
		updater.Update( ACTION_NOTIFY_ENTRANCE_STATE, this );

	eInsideType = EOIO_NONE;
// CRAP{ why?
//	pObjInside = 0;
// CRAP}

	slotInfo.nSlot = -1;
	ChangeWarFogState();

	pFormation->SetGeometryPropertiesToSoldier( this, false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::MoveToEntrenchFireplace( const CVec3 &coord, const int _nSlot )
{
	NI_ASSERT_T( IsInEntrenchment(), "Wrong unit state" );

	slotInfo.nSlot = _nSlot;

	// CRAP{ ����� �� ������
	if ( IsInEntrenchment() )
	{
	// CRAP}
		CEntrenchment *pEntrenchment = GetEntrenchment();
		
		SRect rect;
		pEntrenchment->GetBoundRect( &rect );
		UpdateDirection( -rect.dirPerp );
		
		SetToFirePlace();
	}

	SetNewCoordinates( coord );	

	updater.Update( ACTION_NOTIFY_IDLE_TRENCH, this );
	SetVisionAngle( SConsts::STANDART_VIS_ANGLE );
	ChangeWarFogState();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::GetShotInfo( SAINotifyInfantryShot *pShotInfo ) const
{
	NI_ASSERT_T( IsInTransport() == false, "Can't shoot from inside of transport unit" );

	pShotInfo->typeID = GetShootAction();

	if ( IsInBuilding() )
	{
		pShotInfo->pObj = GetBuilding();
		pShotInfo->nSlot = slotInfo.nSlot;
	}
	else
	{
		pShotInfo->pObj = const_cast<CSoldier*>(this);
		pShotInfo->nSlot = -1;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::GetThrowInfo( struct SAINotifyInfantryShot *pThrowInfo ) const
{
	NI_ASSERT_T( IsInTransport() == false, "Can't throw a grenade from inside of transport unit" );

	pThrowInfo->typeID = GetThrowAction();	

	if ( IsInBuilding() )
	{
		pThrowInfo->pObj = GetBuilding();
		pThrowInfo->nSlot = slotInfo.nSlot;
	}
	else
	{
		pThrowInfo->pObj = const_cast<CSoldier*>(this);
		pThrowInfo->nSlot = -1;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const EActionNotify CSoldier::GetAimAction() const
{
	NI_ASSERT_T( IsInTransport() == false, "Can't aim from inside of transport unit" );
	
	if ( IsInBuilding() )
		return ACTION_NOTIFY_NONE;
	else if ( IsInEntrenchment() )
		return ACTION_NOTIFY_AIM_TRENCH;
	else if ( bLying )
		return ACTION_NOTIFY_AIM_LYING;
	else
		return ACTION_NOTIFY_AIM;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const EActionNotify CSoldier::GetShootAction() const
{
	NI_ASSERT_T( IsInTransport() == false, "Can't shoot from inside of transport unit" );
	
	if ( IsInBuilding() )
		return ACTION_NOTIFY_SHOOT_BUILDING;
	else if ( IsInEntrenchment() )
		return ACTION_NOTIFY_SHOOT_TRENCH;
	else if ( IsLying() )
		return ACTION_NOTIFY_SHOOT_LYING;
	else
		return ACTION_NOTIFY_INFANTRY_SHOOT;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const EActionNotify CSoldier::GetThrowAction() const
{
	NI_ASSERT_T( IsInTransport() == false, "Can't throw a grenade from inside of transport unit" );

	if ( IsInBuilding() )
		return ACTION_NOTIFY_THROW_BUILDING;
	else if ( IsInEntrenchment() )
		return ACTION_NOTIFY_THROW_TRENCH;
	else if ( IsLying() )
		return ACTION_NOTIFY_THROW_LYING;
	else
		return ACTION_NOTIFY_THROW;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoldier::InVisCone( const CVec2 &point ) const
{
	return 
		GetVisionAngle() >= 32768 || 
		DirsDifference( GetDirectionByVector( point - GetCenter() ),GetFrontDir() ) < GetVisionAngle();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::Init( const CVec2 &center, const int z, const SUnitBaseRPGStats *_pStats, const float fHP, const WORD dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID )
{
	eInsideType = EOIO_NONE;
	pObjInside = 0;
	bInFirePlace = false;
	bInSolidPlace = false;
	fOwnSightRadius = -1.0f;

	pStats = static_cast<const SInfantryRPGStats*>( _pStats );
	lastHit = lastCheck = lastMineCheck = curTime;
	bLying = false;
	bAllowLieDown = true;
	lastDirUpdate = 0;
	bWait2Form = false;
	CAIUnit::Init( center, z, fHP, dir, player, id, eVisType, dbID );
	cFormSlot = 0;
	nextSegmTime = 0;
	timeBWSegments = 0;
	nextPathSegmTime = 0;
	nextLogicSegmTime = 0;
	wMinAngle = 0;
	wMaxAngle = 65535;
	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const EActionNotify CSoldier::GetDieAction() const
{
	if ( IsInBuilding() )
		return ACTION_NOTIFY_DIE_BUILDING;
	else if ( IsInEntrenchment() )
		return ACTION_NOTIFY_DIE_TRENCH;
	else if ( IsInTransport() )
		return ACTION_NOTIFY_DIE_TRANSPORT;
	else if ( bLying )
		return ACTION_NOTIFY_DIE_LYING;
	else
		return ACTION_NOTIFY_DIE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const EActionNotify CSoldier::GetIdleAction() const
{
	if ( IsInEntrenchment() )
		return ACTION_NOTIFY_IDLE_TRENCH;
	else// if ( IsFree() )
	{
		if ( bLying )
			return ACTION_NOTIFY_IDLE_LYING;
		else
			return ACTION_NOTIFY_IDLE;
	}
//	else
//		return ACTION_NOTIFY_IDLE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const EActionNotify CSoldier::GetMovingAction() const
{
	if ( EUSN_PARTROOP == GetState()->GetName() )
	{
		return ACTION_NOTIFY_FALLING;
	}
	else if ( bLying )
		return ACTION_NOTIFY_CRAWL;
	else
		return ACTION_NOTIFY_MOVE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoldier::IsInSolidPlace() const
{
	return bInSolidPlace;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoldier::IsInFirePlace() const
{
	return bInFirePlace;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::SetToFirePlace()
{
	bLying = false;
	if ( !IsInFirePlace() )
	{
		if ( IsInSolidPlace() )
		{
			units.AddUnitToCell( this, true );
			bInSolidPlace = false;
		}

		bInFirePlace = true;
	}

	if ( eInsideType == EOIO_NONE )
		eInsideType = EOIO_UNKNOWN;

	ChangeWarFogState();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::SetToSolidPlace()
{
	bLying = false;
	if ( !IsInSolidPlace() )
	{
		units.DelUnitFromCell( this, true );

		bInFirePlace = false;
		bInSolidPlace = true;
	}

	if ( eInsideType == EOIO_NONE )
		eInsideType = EOIO_UNKNOWN;
	
	ChangeWarFogState();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::GetEntranceStateInfo( struct SAINotifyEntranceState *pInfo ) const
{
	pInfo->pTarget = pObjInside;
	pInfo->bEnter = !IsFree();
	pInfo->pInfantry = const_cast<CSoldier*>(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CSoldier::GetCover() const
{
	if ( IsFree() )
	{
		float fCover = 1.0f;
		if ( bLying )
			fCover = SConsts::LYING_SOLDIER_COVER;

		if ( IsInFormation() )
		{
			fCover *= GetFormation()->GetCoverBonus();

			// if in gun crew and gun is entrenched, give entrench bonus
			CFormation *pFormation = GetFormation();
			if ( pFormation->GetState() && pFormation->GetState()->GetName() == EUSN_GUN_CREW_STATE )
			{
				CArtillery *pArtillery = checked_cast<CFormationGunCrewState*>(pFormation->GetState())->GetArtillery();
				if ( pArtillery->IsInTankPit() )
					fCover *= SConsts::TANKPIT_COVER;
			}
		}

		return fCover;
	}
	if ( slotInfo.nSlot == -1 || IsFree() )
		return 1.0f;
	else if ( IsInTransport() )
		return 0.0f;
	else if ( IsInBuilding() && slotInfo.nSlot >= 0 && IsValidObj( GetBuilding() ) )
		return static_cast<const SBuildingRPGStats*>(GetBuilding()->GetStats())->slots[slotInfo.nSlot].fCoverage * SConsts::BUILDING_FIREPLACE_DEFAULT_COVER;
	else if ( IsInEntrenchment() && slotInfo.nSlot >= 0 && IsValidObj( GetEntrenchment() ) )
		return static_cast<const SEntrenchmentRPGStats*>(GetEntrenchment()->GetStats())->segments[slotInfo.nSlot].fCoverage * SConsts::BUILDING_FIREPLACE_DEFAULT_COVER;
	else
		return 1.0f;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::UpdateLyingPosition()
{
	if ( bLying == true )
		updater.Update( ACTION_NOTIFY_STAYING_TO_LYING, this );
	else
		updater.Update( ACTION_NOTIFY_LYING_TO_STAYING, this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::LieDown()
{
	if ( !bLying && bAllowLieDown && IsFree() && !bLying && ( !IsValidObj( pFormation ) || pFormation->IsAllowedLieDown() ) )
	{
		bLying = true;
		UpdateLyingPosition();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::StandUp()
{
	if ( bLying && bAllowLieDown && ( !IsValidObj( pFormation ) || pFormation->IsAllowedStandUp() ) )
	{
		bLying = false;
		UpdateLyingPosition();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::Segment()
{
	if ( curTime >= nextLogicSegmTime )
		CAIUnit::Segment();

	pGuns->Segment();

	if ( IsInBuilding() && GetSlot() != -1 )
	{
		CBuilding *pBuilding = static_cast<CBuilding*>(pObjInside);
		for ( int i = 0; i < pBuilding->GetNGunsInFireSlot( GetSlot() ); ++i )
			pBuilding->GetGunInFireSlot( GetSlot(), i )->Segment();
	}

	//��� ��������� ( ����, ������ ��� ��������� )
	if (	/*pStats->type == RPG_TYPE_ENGINEER && EUSN_CLEAR_MINE == GetFormation()->GetState()->GetName() &&*/
				curTime - lastMineCheck > SConsts::ENGINEER_MINE_CHECK_PERIOD )
	{
		lastMineCheck = curTime;
		RevealNearestMines( pStats->type == RPG_TYPE_ENGINEER && EUSN_CLEAR_MINE == GetFormation()->GetState()->GetName() );
	}

	if ( curTime >= nextLogicSegmTime )
	{
		if ( timeBWSegments < 700 )
			timeBWSegments += 20;

		nextLogicSegmTime = curTime + Random( 0, timeBWSegments );
	}

	nextSegmTime = curTime + Random( 0, 250 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::FreezeSegment()
{
	if ( curTime - lastCheck >= SConsts::TIME_OF_HIT_NOTIFY + Random( 0.0f, SConsts::STAND_LIE_RANDOM_DELAY ) )
	{
		lastCheck = curTime;
		if ( theHitsStore.WasHit( GetCenter(), SConsts::RADIUS_OF_HIT_NOTIFY, CHitsStore::EHT_ANY ) )
		{
			if ( IsInFormation() )
				GetFormation()->WasHitNearUnit();

			lastHit = curTime;				
			if ( bAllowLieDown && IsInFormation() && pFormation->IsAllowedLieDown() )
				LieDown();
		}
	}

	// ������� ����������
	if ( bLying && ( !IsInFormation() || GetFormation()->IsAllowedStandUp() ) && curTime - lastHit >= SConsts::TIME_OF_LYING_UNDER_FIRE + Random( 0.0f, SConsts::STAND_LIE_RANDOM_DELAY ) )
		StandUp();

	CAIUnit::FreezeSegment();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::RevealNearestMines( const bool bIncludingAP )
{
	const int nParty = GetParty();
 	for ( CMinesIter iter( GetCenter(), SConsts::MINE_VIS_RADIUS, nParty ); !iter.IsFinished(); iter.Iterate() )
	{
		CMineStaticObject *pMine = *iter;
		if ( bIncludingAP || checked_cast<const SMineRPGStats*>( pMine->GetStats() )->type == SMineRPGStats::TECHNICS )
			pMine->SetVisible( nParty, true );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::SetFormation( CFormation *_pFormation, const BYTE _cFormSlot )
{
	pFormation = _pFormation;
	cFormSlot = _cFormSlot;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CSoldier::GetMaxPossibleSpeed() const
{
	float fSpeed;
	
	if ( bLying )
		fSpeed = CAIUnit::GetMaxPossibleSpeed() * SConsts::LYING_SPEED_FACTOR;
	else
		fSpeed = CAIUnit::GetMaxPossibleSpeed();

	// � ��������
	if ( IsFree() && IsValidObj( pFormation ) )
		fSpeed *= pFormation->GetCurSpeedBonus();

	return fSpeed;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::GetFogInfo( SFogInfo *pInfo ) const
{
	if ( !IsInSolidPlace() )
	{
		if ( IsInBuilding() )
			pInfo->pObject = GetBuilding();
		else
			pInfo->pObject = 0;

		pInfo->bPlane = false;

		pInfo->center = GetTile();
		pInfo->r = GetSightRadius() / SConsts::TILE_SIZE;
		pInfo->wUnitDir = GetFrontDir();
		pInfo->wVisionAngle = GetVisionAngle();
		pInfo->bAngleLimited = IsAngleLimited();
		pInfo->wMinAngle = GetMinAngle();
		pInfo->wMaxAngle = GetMaxAngle();
		pInfo->fSightPower = GetStats()->fSightPower;
	}
	else
		CAIUnit::GetFogInfo( pInfo );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CArtillery* CSoldier::GetArtilleryIfCrew() const
{
	if ( CFormation *pFormation = GetFormation() )
	{
		if ( IUnitState *pState = pFormation->GetState() )
		{
			if ( pState->GetName() == EUSN_GUN_CREW_STATE )
			{
				CFormationGunCrewState *pFullState = checked_cast<CFormationGunCrewState*>(pState);
				if ( CArtillery *pArtillery = pFullState->GetArtillery() )
				{
					if ( pArtillery && pArtillery->IsValid() && pArtillery->IsAlive() )
						return pArtillery;
				}
			}
		}
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CSoldier::GetNGuns() const
{
	if ( CArtillery *pArtillery = GetArtilleryIfCrew() )
		return pGuns->GetNTotalGuns() + pArtillery->GetNGuns();
	else if ( !IsInBuilding() || GetSlot() == -1 )
		return pGuns->GetNTotalGuns();
	else
		return pGuns->GetNTotalGuns() + static_cast<CBuilding*>(pObjInside)->GetNGunsInFireSlot( GetSlot() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBasicGun* CSoldier::GetGun( const int n ) const
{
	if ( n < pGuns->GetNTotalGuns() )
		return pGuns->GetGun( n );
	else if ( n >= GetNGuns() )
		return GetGuns()->GetGun( 0 );
	else if ( CArtillery *pArtillery = GetArtilleryIfCrew() )
		return pArtillery->GetGun( n - pGuns->GetNTotalGuns() );
	else
		return static_cast<CBuilding*>(pObjInside)->GetGunInFireSlot( GetSlot(), n - pGuns->GetNTotalGuns() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CSoldier::GetNAmmo( const int nCommonGun ) const
{
	if ( nCommonGun < pGuns->GetNCommonGuns() )
		return pGuns->GetNAmmo( nCommonGun );
	else
		return 1000;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::ChangeAmmo( const int nCommonGun, const int nAmmo )
{
	if ( nCommonGun < pGuns->GetNCommonGuns() )
		CAIUnit::ChangeAmmo( nCommonGun, nAmmo );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoldier::IsCommonGunFiring( const int nCommonGun ) const
{
	if ( nCommonGun < pGuns->GetNCommonGuns() )
		return CAIUnit::IsCommonGunFiring( nCommonGun );

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::Fired( const float fGunRadius, const int nGun )
{
	if ( nGun < pGuns->GetNCommonGuns() )
		CAIUnit::Fired( fGunRadius, nGun );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTurret* CSoldier::GetTurret( const int nTurret ) const 
{
	NI_ASSERT_T( nTurret == 0, "Wrong number of turret for soldier" );
	if ( !IsInBuilding() || GetSlot() == -1 )
		return 0;
	else
		return static_cast<CBuilding*>(pObjInside)->GetTurretInFireSlot( GetSlot() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CSoldier::GetNTurrets() const
{
	if ( GetTurret( 0 ) )
		return 1;
	else
		return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::GetShootAreas( SShootAreas *pShootAreas, int *pnAreas ) const
{
	std::construct( pShootAreas );
	
	if ( IsFree() || IsInBuilding() && GetTurret( 0 ) != 0 || IsInEntrenchment() && IsInFirePlace() )
		CAIUnit::GetShootAreas( pShootAreas, pnAreas );
	else
	{
		*pnAreas = 1;
		pShootAreas->areas.push_back( SShootArea() );
		SShootArea &area = pShootAreas->areas.back();
		area.eType = SShootArea::ESAT_LINE;
		area.vCenter3D = CVec3( GetCenter(), 0.0f );

		area.fMinR = 0.0f;
		area.fMaxR = GetMaxFireRange();
		
		if ( IsInSolidPlace() || IsInTransport() )
		{
			area.wStartAngle = 65535;
			area.wFinishAngle = 65535;
		}
		else
		{
			area.wStartAngle = wMinAngle;
			area.wFinishAngle = wMaxAngle;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CSoldier::GetMaxFireRange() const
{
	if ( !IsInBuilding() || GetSlot() == -1 )
		return pGuns->GetMaxFireRange( this );
	else
	{
		CTurret* pTurret = static_cast<CBuilding*>(pObjInside)->GetTurretInFireSlot( GetSlot() );
		if ( pTurret == 0 )
			return pGuns->GetMaxFireRange( this );
		else
			return Max( pGuns->GetMaxFireRange( this ), static_cast<CBuilding*>(pObjInside)->GetMaxFireRangeInSlot( GetSlot() ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CSoldier::GetSightMultiplier() const
{
	if ( !IsFree() || !IsInFormation() )
		return CAIUnit::GetSightMultiplier();
	else
		return GetFormation()->GetSightMultiplier();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CSoldier::GetSightRadius() const
{
	// ���� � ��������� �������, �� ������ �� �����
	if ( IsInBuilding() && GetBuilding()->IsAnyAttackers() || IsInEntrenchment() && GetEntrenchment()->IsAnyAttackers() )
		return 0;
	else if ( fOwnSightRadius > 0 )
		return Max( 0.0f, fOwnSightRadius + (float)floor( theStaticMap.GetTileHeight( GetTile() ) / SConsts::HEIGHT_FOR_VIS_RADIUS_INC ) * SConsts::TILE_SIZE );
	else
		return CAIUnit::GetSightRadius();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::PrepareToDelete()
{
	if ( IsAlive() )
	{
		// ������� �� ������. �������, ���� �����
		if ( IsInBuilding() )
			GetBuilding()->DelInsider( this );
		else if ( IsInEntrenchment() )
			GetEntrenchment()->DelInsider( this );
		else if ( IsInTransport() )
			GetTransportUnit()->DelPassenger( this );

		// ������� �� ��������, ���� �����
		if ( IsInFormation() )
		{
			pFormation->DelUnit( GetFormationSlot() );
			if ( GetFormation()->Size() == 0 && GetFormation()->VirtualUnitsSize() == 0 )
				pFormation->Disappear();

			if ( pMemorizedFormation )
			{
				pMemorizedFormation->DelUnit( this );
				if ( pMemorizedFormation->Size() == 0 && pMemorizedFormation->VirtualUnitsSize() == 0 )
					pMemorizedFormation->Disappear();
			}
			
			if ( pVirtualFormation )
			{
				pVirtualFormation->DelVirtualUnit( this );
				pVirtualFormation = 0;
			}
		}

		CAIUnit::PrepareToDelete();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CSoldier::GetUnitPointInFormation() const 
{ 
	return pFormation->GetUnitCoord( cFormSlot ); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoldier::IsColliding() const 
{
	const EUnitStateNames eFormationStateName = GetFormation()->GetState()->GetName();
	return
		IsFree() && ( eFormationStateName != EUSN_GUN_CREW_STATE && eFormationStateName != EUSN_PARTROOP );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::TakeDamage( const float fDamage, const SWeaponRPGStats::SShell *pShell, const int nPlayerOfShoot, CAIUnit *pShotUnit )
{
	if ( EUSN_PARTROOP != GetState()->GetName() )
	{
		CAIUnit::TakeDamage( fDamage, pShell, nPlayerOfShoot, pShotUnit );

		if ( IsAlive() && IsInBuilding() )
			GetBuilding()->InsiderDamaged( this );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoldier::IsFree() const 
{ 
	return eInsideType == EOIO_NONE || eInsideType ==	EOIO_UNKNOWN; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CSoldier::CanShootToPlanes() const 
{ 
	return pGuns->CanShootToPlanes(); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBasicGun* CSoldier::ChooseGunForStatObj( class CStaticObject *pObj, NTimer::STime *pTime ) 
{ 
	return pGuns->ChooseGunForStatObj( this, pObj, pTime ); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoldier::CalculateUnitVisibility4Party( const BYTE party ) const
{
	if ( IsInBuilding() )
	{
		return 
			( GetBuilding()->IsAnyAttackers() || IsInFirePlace() ) && 
			 GetBuilding()->IsSoldierVisible( party, GetCenter(), IsCamoulflated(), GetCamouflage() );
	}
	else if ( IsInSolidPlace() )
		return false;
	else if ( party == GetParty() )
		return true;
	else if ( IsInFormation() && GetFormation()->GetState() && GetFormation()->GetState()->GetName() == EUSN_GUN_CREW_STATE )
		return static_cast<CFormationGunCrewState*>(GetFormation()->GetState())->GetArtillery()->IsVisible( party );
	else if ( IsInFirePlace() && IsInTransport() )
		return GetTransportUnit()->IsVisible( party );
	else
		return theWarFog.IsUnitVisible( party, GetTile(), IsCamoulflated(), GetCamouflage() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::UpdateDirection( const CVec2 &newDir )
{
 UpdateDirection( GetDirectionByVector( newDir ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::UpdateDirection( const WORD newDir )
{
	if ( curTime - lastDirUpdate > 500 || 
			 DirsDifference( newDir, GetFrontDir() ) > 2 * 65536 / SConsts::NUMBER_SOLDIER_DIRS )
	{
		lastDirUpdate = curTime;
		CAIUnit::UpdateDirection( newDir );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoldier::IsNoticableByUnit( class CCommonUnit *pUnit, const float fNoticeRadius )
{
	if ( IsInSolidPlace() )
		return false;
	else if ( IsInFirePlace() && IsInBuilding() && ( IsVisible( pUnit->GetParty() ) || IsRevealed() ) )
	{
		SRect buildRect;
		GetBuilding()->GetBoundRect( &buildRect );
		if ( buildRect.IsIntersectCircle( pUnit->GetCenter(), fNoticeRadius ) )
			return true;
		else
			return false;
	}
	else
		return CAIUnit::IsNoticableByUnit( pUnit, fNoticeRadius );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoldier::ProcessAreaDamage( const CExplosion *pExpl, const int nArmorDir, const float fRadius, const float fSmallRadius )
{
	if ( !IsInBuilding() && !IsInSolidPlace() && GetZ() == pExpl->GetExplZ() && !IsSavedByCover() )
	{
		if ( !CAIUnit::ProcessAreaDamage( pExpl, nArmorDir, fRadius, fSmallRadius ) )
		{
			if ( IsFree() && !IsLying() && fabs2( GetCenter() - pExpl->GetExplCoordinates() ) <= sqr( fRadius ) )
			{
				TakeDamage( pExpl->GetRandomDamage() * SConsts::AREA_DAMAGE_COEFF, &pExpl->GetShellStats(), pExpl->GetPlayerOfShoot(), pExpl->GetWhoFire() );
				return true;
			}
		}
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CSoldier::GetDispersionBonus() const
{
	if ( IsFree() && IsValidObj( pFormation ) )
		return pFormation->GetDispersionBonus();
	else
		return theDifficultyLevel.GetDispersionCoeff( theDipl.GetNParty( GetPlayer() ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CSoldier::GetRelaxTimeBonus() const
{
	if ( IsFree() && IsValidObj( pFormation ) )
		return pFormation->GetRelaxTimeBonus();
	else
		return 1.0f;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CSoldier::GetFireRateBonus() const
{
	if ( IsFree() && IsValidObj( pFormation ) )
		return pFormation->GetFireRateBonus();
	else
		return 1.0f;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::MemCurFormation()
{
	pMemorizedFormation = pFormation;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CSoldier::GetMinArmor() const
{
/*	
	if ( IsInFormation() && GetFormation()->GetState()->GetName() == EUSN_GUN_CREW_STATE )
		return Min( CAIUnit::GetMinArmor(), static_cast<CFormationGunCrewState*>(GetFormation()->GetState())->GetArtillery()->GetMinArmor() );
	else
*/
		return CAIUnit::GetMinArmor();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CSoldier::GetMaxArmor() const
{
/*	
	if ( IsInFormation() && GetFormation()->GetState()->GetName() == EUSN_GUN_CREW_STATE )
		return Max( CAIUnit::GetMaxArmor(), static_cast<CFormationGunCrewState*>(GetFormation()->GetState())->GetArtillery()->GetMaxArmor() );
	else
*/
		return CAIUnit::GetMaxArmor();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////	
const int CSoldier::GetMinPossibleArmor( const int nSide ) const
{
/*	
	if ( nSide == RPG_FRONT && IsInFormation() && GetFormation()->GetState()->GetName() == EUSN_GUN_CREW_STATE )
		return static_cast<CFormationGunCrewState*>(GetFormation()->GetState())->GetArtillery()->GetMinPossibleArmor( nSide );
	else
*/
		return CAIUnit::GetMinPossibleArmor( nSide );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CSoldier::GetMaxPossibleArmor( const int nSide ) const
{
/*	
	if ( nSide == RPG_FRONT && IsInFormation() && GetFormation()->GetState()->GetName() == EUSN_GUN_CREW_STATE )
		return static_cast<CFormationGunCrewState*>(GetFormation()->GetState())->GetArtillery()->GetMaxPossibleArmor( nSide );
	else
*/
		return CAIUnit::GetMaxPossibleArmor( nSide );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CSoldier::GetArmor( const int nSide ) const
{
/*	
	if ( nSide == RPG_FRONT && IsInFormation() && GetFormation()->GetState()->GetName() == EUSN_GUN_CREW_STATE )
		return static_cast<CFormationGunCrewState*>(GetFormation()->GetState())->GetArtillery()->GetArmor( nSide );
	else
*/
		return CAIUnit::GetArmor( nSide );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CSoldier::GetRandomArmor( const int nSide ) const
{
/*	
	if ( nSide == RPG_FRONT && IsInFormation() && GetFormation()->GetState()->GetName() == EUSN_GUN_CREW_STATE )
		return static_cast<CFormationGunCrewState*>(GetFormation()->GetState())->GetArtillery()->GetRandomArmor( nSide );
	else
*/
		return CAIUnit::GetRandomArmor( nSide );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoldier::IsAngleLimited() const 
{ 
	return
		IsInBuilding() || ( wMinAngle > 0 || wMaxAngle < 65535 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoldier::ShouldSuspendAction( const EActionNotify &eAction ) const
{
	return 
		!theDipl.IsEditorMode() &&
		(	!IsInTransport() && 
			( eAction == ACTION_NOTIFY_DEAD_UNIT || 
			eAction == ACTION_NOTIFY_GET_DEAD_UNITS_UPDATE ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsTransitionalCommand( const EActionCommand &eCmd )
{
	return 		
		eCmd == ACTION_COMMAND_ENTER ||
		eCmd == ACTION_COMMAND_ENTER_TRANSPORT_NOW || 
		eCmd == ACTION_COMMAND_LOAD || eCmd == ACTION_COMMAND_LOAD_NOW ||
		eCmd == ACTION_COMMAND_LEAVE || eCmd == ACTION_COMMAND_LEAVE_NOW ||
		eCmd == ACTION_COMMAND_UNLOAD || eCmd == ACTION_COMMAND_UNLOAD_NOW;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoldier::CanJoinToFormation() const
{
	if ( !IsCurCmdFinished() )
	{
		bool bCan = true;
		
		if ( CAICommand *pCmd = GetCurCmd() )
			bCan = bCan && !IsTransitionalCommand( pCmd->ToUnitCmd().cmdType );
		if ( CAICommand *pCmd = GetNextCommand() )
			bCan = bCan && !IsTransitionalCommand( pCmd->ToUnitCmd().cmdType );

		return bCan;
	}
	else
		return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::SetVirtualFormation( CFormation *pFormation )
{
	pVirtualFormation = pFormation;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::MemorizeFormation() 
{ 
	pFormation = pMemorizedFormation; 
	pMemorizedFormation = 0; 

	pFormation->SetGeometryPropertiesToSoldier( this, true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::LookForTarget( CAIUnit *pCurTarget, const bool bDamageUpdated, CAIUnit **pBestTarget, CBasicGun **pGun )
{
	CAIUnit::LookForTarget( pCurTarget, bDamageUpdated, pBestTarget, pGun );

	if ( CanShoot() )
	{
		if ( GetStats()->type != RPG_TYPE_OFFICER && GetBehaviour().moving != SBehaviour::EMHoldPos )
		{
			for ( int i = 0; i < GetFormation()->Size(); ++i )
			{
				IUnitState *pState = (*GetFormation())[i]->GetState();

				if ( pState && pState->IsValid() && pState->IsAttackingState() )
				{
					NI_ASSERT_T( dynamic_cast<IUnitAttackingState*>(pState) != 0, "Unexpected unit state" );
					CAIUnit *pTarget = static_cast<IUnitAttackingState*>(pState)->GetTargetUnit();

					if ( IsValidObj( pTarget ) && theDipl.GetDiplStatus( GetPlayer(), pTarget->GetPlayer() ) == EDI_ENEMY )
					{
						if ( pTarget->IsVisible( GetParty() ) || pTarget->IsRevealed() )
							AddUnitToShootEstimator( pTarget );
					}
				}
			}
		}

		*pBestTarget = GetBestShootEstimatedUnit();
		*pGun = GetBestShootEstimatedGun();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::FirstSegment()
{
	CAIUnit::FirstSegment();
	nextPathSegmTime = curTime + Random( 500, 1000 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CSoldier::GetPathSegmentsPeriod() const
{
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoldier::FreezeByState( const bool bFreeze )
{
	if ( IsFrozenByState() != bFreeze )
	{
		if ( !bFreeze || pFormation->IsEveryUnitResting() )
			CAIUnit::FreezeByState( bFreeze );

		if ( !bFreeze )
			pFormation->FreezeByState( bFreeze );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*														CSniper																*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSniper::Init( const CVec2 &center, const int z, const SUnitBaseRPGStats *pStats, const float fHP, const WORD dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID )
{
	lastVisibilityCheck = 0;
	bVisible = false;
	bSneak = false;
	fCamouflageRemoveWhenShootProbability = 0.0f;

	CSoldier::Init( center, z, pStats, fHP, dir, player, id, eVisType, dbID );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSniper::Segment()
{
	CSoldier::Segment();

	float fAdd = SConsts::SNIPER_CAMOUFLAGE_INCREASE * SConsts::AI_SEGMENT_DURATION / 1000;
	if ( fCamouflageRemoveWhenShootProbability >= fAdd )
		fCamouflageRemoveWhenShootProbability -= fAdd;
	else
		fCamouflageRemoveWhenShootProbability = 0.0f;

	// analyze camouflage and go to camouflage if not bein seed
	if ( curTime - lastVisibilityCheck >= 1000 + Random( 0, 5 * SConsts::AI_SEGMENT_DURATION ) )
	{
		if ( IsInBuilding() && GetBuilding() )
			GetBuilding()->RemoveTransparencies();
		
		lastVisibilityCheck = curTime;
		bVisible = false;
		const float fMaxVisRadius = 30 * SConsts::TILE_SIZE;
		std::unordered_set<SVector, STilesHash> visitedTiles;

		SSniperTrace sniperTracer( this );
		const SVector curCenterTile( GetTile() );
		
		for ( CUnitsIter<0,3> iter( GetParty(), EDI_ENEMY, GetCenter(), fMaxVisRadius ); !bVisible && !iter.IsFinished(); iter.Iterate() )
		{
			CAIUnit *pUnit = *iter;
			if ( IsValidObj( pUnit ) && !pUnit->GetStats()->IsAviation() )
			{
 				const SVector unitTile = pUnit->GetTile();
				if ( visitedTiles.find( unitTile ) == visitedTiles.end() )
				{
					visitedTiles.insert( unitTile );
					
					SFogInfo fogInfo;
					pUnit->GetFogInfo( &fogInfo );

					if ( fogInfo.pObject )
						fogInfo.pObject->RemoveTransparencies();

					CWarFogTracer<SSniperTrace> tracer( curCenterTile, sniperTracer, fogInfo );

					if ( fogInfo.pObject )
						fogInfo.pObject->SetTransparencies();
				}
			}
		}

		if ( IsInBuilding() && GetBuilding() )
			GetBuilding()->SetTransparencies();
	}

	if ( !bVisible && bSneak )
		SetCamoulfage();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSniper::CalculateUnitVisibility4Party( const BYTE party ) const
{
	if ( !IsCamoulflated() )
		return CSoldier::CalculateUnitVisibility4Party( party );
	else
	{
		if ( IsInSolidPlace() )
			return false;
		else if ( party == GetParty() )
			return true;
		else
			return bVisible;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSniper::RemoveCamouflage( ECamouflageRemoveReason eReason )
{
	if ( bSneak )
	{
		switch ( eReason )
		{
		case ECRR_SELF_SHOOT:
			{
				fCamouflageRemoveWhenShootProbability += SConsts::SNIPER_CAMOUFLAGE_DECREASE_PER_SHOOT;
				float fRand = Random( 0.0f, 1.0f );
				if ( fRand < fCamouflageRemoveWhenShootProbability )
					break;
				return;
			}
			break;
		case ECRR_SELF_MOVE:
			return;

		case ECRR_GOOD_VISIBILITY:

			return;
		case ECRR_USER_COMMAND:
			break;
		}

		CAIUnit::RemoveCamouflage( eReason );
	}
	else
		CSoldier::RemoveCamouflage( eReason );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSniper::Fired( const float fGunRadius, const int nGun  )
{
	RemoveCamouflage( ECRR_SELF_SHOOT );
	CSoldier::Fired( fGunRadius, nGun );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSniper::SetSneak( const bool bSneakMode ) 
{ 
	if ( bSneak != bSneakMode )
	{
		bSneak = bSneakMode; 

		if ( bSneakMode )
		{
			LieDown();
			GetBehaviour().fire = SBehaviour::EFNoFire;
			if ( IsInFormation() )
				GetFormation()->GetBehaviour().fire = SBehaviour::EFNoFire;
			RegisterAsBored( ACK_BORED_SNIPER_SNEAK );
		}
		else
		{
			GetBehaviour().fire = SBehaviour::EFAtWill;
			GetFormation()->GetBehaviour().fire = SBehaviour::EFAtWill;
			GetFormation()->ChangeGeometry( 0 );

			UnRegisterAsBored( ACK_BORED_SNIPER_SNEAK );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
