#include "stdafx.h"

#include "Technics.h"
#include "TankStates.h"
#include "TransportStates.h"
#include "Guns.h"
#include "UnitsIterators2.h"
#include "Commands.h"
#include "Formation.h"
#include "Updater.h"
#include "AIStaticMap.h"
#include "Soldier.h"
#include "ArtilleryStates.h"
#include "StaticObjects.h"
#include "UnitGuns.h"
#include "Turret.h"
#include "StaticObject.h"
#include "Artillery.h"
#include "Path.h"
#include "Entrenchment.h"
#include "Cheats.h"
#include "ShootEstimatorInternal.h"
#include "Diplomacy.h"
#include "Statistics.h"
#include "UnitCreation.h"
#include "GroupLogic.h"
#include "MultiplayerInfo.h"
#include "PathUnit.h"
#include "DifficultyLevel.h"

#include "..\Scene\Scene.h"
#include "..\Common\AdditionalActions.h"
#include "..\Misc\Checker.h"
// for profiling
#include "TimeCounter.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CGroupLogic theGroupLogic;
extern CUnitCreation theUnitCreation;
extern CStatistics theStatistics;
extern NTimer::STime curTime;
extern CUpdater updater;
extern CStaticObjects theStatObjs;
extern CStaticMap theStaticMap;
extern SCheats theCheats;
extern CDiplomacy theDipl;
extern CMultiplayerInfo theMPInfo;
extern CDifficultyLevel theDifficultyLevel;

extern CTimeCounter timeCounter;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CMilitaryCar																*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CMilitaryCar );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMilitaryCar::Init( const CVec2 &center, const int z, const SUnitBaseRPGStats *_pStats, const float fHP, const WORD dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID )
{
	pStats = static_cast<const SMechUnitRPGStats*>( _pStats );
	fDispersionBonus = 1.0f;
	
	CAIUnit::Init( center, z, fHP, dir, player, id, eVisType, dbID );
	timeLastHeal = Random( GetBehUpdateDuration() );
	lastResupplyMorale = Random( GetBehUpdateDuration() / 2 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMilitaryCar::InitGuns()
{
	if ( pStats->platforms.size() > 1 )
	{
		const int nTurrets = pStats->platforms.size() - 1;
		turrets.resize( nTurrets );

		for ( int i = 0; i < nTurrets; ++i )
		{
			const SMechUnitRPGStats::SPlatform &platform = pStats->platforms[i+1];
			turrets[i] = new CUnitTurret( 
																		this, platform.nModelPart, platform.dwGunCarriageParts, 
																		platform.wHorizontalRotationSpeed, platform.wVerticalRotationSpeed,
																		platform.constraint.wMax, platform.constraintVertical.wMax
																	);
		}
	}

	pGuns = new CMechUnitGuns;
	pGuns->Init( this );

	SetShootEstimator( new CTankShootEstimator( this ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CMilitaryCar::GetEntrancePoint() const
{
	const CVec2 vFrontDir = GetVectorByDirection( GetFrontDir() );
	const CVec2 vTurn( vFrontDir.y, -vFrontDir.x );
	const CVec2 vEntrNow( (pStats->vEntrancePoint) ^ vTurn );

	CVec2 vResult( GetCenter() + vEntrNow );

	const SVector tile = AICellsTiles::GetTile( vResult );
	if ( theStaticMap.IsLocked( tile, AI_CLASS_HUMAN ) )
	{
		CVec2 vDir( vEntrNow );
		Normalize( &vDir );

		vResult += vDir * SConsts::TILE_SIZE / 2;

		if ( theStaticMap.IsLocked( tile, AI_CLASS_HUMAN ) )
			vResult += vDir * SConsts::TILE_SIZE / 2;
	}

	return vResult;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMilitaryCar::AddPassenger( CSoldier *pUnit )
{
	if ( pass.empty() )
	{
		for ( int i = 0; i < GetNTurrets(); ++i )
			GetTurret( i )->SetRotateTurretState( false );
	}
	
	pass.push_back( pUnit );
	updater.Update( ACTION_NOTIFY_ENTRANCE_STATE, pUnit );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMilitaryCar::PrepareToDelete()
{
	// всех сидящих внутри - убить.
	while ( GetNPassengers() )
		GetPassenger( 0 )->Die( false, 0 );

	CAIUnit::PrepareToDelete();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMilitaryCar::SendNTotalKilledUnits( const int nPlayerOfShoot )
{
	for ( std::list<CPtr<CSoldier> >::const_iterator iter = pass.begin(); iter != pass.end(); ++iter )
	{
		CAIUnit *pUnit = *iter;
		
		theStatistics.UnitKilled( nPlayerOfShoot, pUnit->GetPlayer(), 1, pUnit->GetStats()->fPrice );
		theMPInfo.UnitsKilled( nPlayerOfShoot, pUnit->GetStats()->fPrice, pUnit->GetPlayer() );
	}

	theStatistics.UnitKilled( nPlayerOfShoot, GetPlayer(), 1, GetStats()->fPrice );
	theMPInfo.UnitsKilled( nPlayerOfShoot, GetStats()->fPrice, GetPlayer() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CMilitaryCar::GetPassengerCoordinates( const int n )
{
	const int nPass = GetNPassengers();
	// солдат на стороне
	const int nSoldiersOnSide = ( n > nPass / 2 ) ? ( nPass / 2 + nPass % 2 ) : ( nPass / 2 );
	// знак, отвечающий за сторону, на которой сидит солдат (поворот frontDirVec - влево или вправо)
	float fSideSign = ( n > nPass / 2 ) ? -1.0f : 1.0f;

	const int nSoldierIndex = ( n <= nPass / 2 ) ? n : n - nPass / 2;

	const float fSideHalfLen = GetStats()->vAABBHalfSize.y / 2.0f;
	const CVec2 vFrontDirVec = GetVectorByDirection( GetFrontDir() );

	const CVec2 vShift = 
			( 2 * fSideHalfLen * (float)nSoldierIndex / float( nSoldiersOnSide + 1 ) - fSideHalfLen ) * vFrontDirVec +
			 fSideSign * 3.0f / 4.0f * CVec2( -vFrontDirVec.y, vFrontDirVec.x );

	return GetCenter() + vShift;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMilitaryCar::Segment()
{
	CAIUnit::Segment();

	pGuns->Segment();
	for ( int i = 0; i < GetNTurrets(); ++i )
		GetTurret( i )->Segment();

	if ( pLockingUnit != 0 && ( !pLockingUnit->IsValid() || !pLockingUnit->IsAlive() ) )
		pLockingUnit = 0;
	
	// CRAR{ соптимизировать!, не посылать, если координаты не изменились
	int i = 0;
	for ( std::list<CPtr<CSoldier> >::iterator iter = pass.begin(); iter != pass.end(); ++iter, ++i )
	{
		CSoldier *pSoldier = *iter;
		const CVec3 vCenter( GetPassengerCoordinates( i ), 0 );

		if ( pSoldier->IsInSolidPlace() )
			pSoldier->SetCoordWOUpdate( vCenter );
		else
			pSoldier->SetNewCoordinates( vCenter );
		
		NI_ASSERT_T( pSoldier->IsInTransport(), "Wrong state of the intransport soldier" );

		updater.Update( ACTION_NOTIFY_PLACEMENT, pSoldier );
	}
	// CRAP}
	
	//медицинские грузовички лечат пехоту в радиусе
	if ( curTime - timeLastHeal > GetBehUpdateDuration() )
	{
		if ( CanCommandBeExecutedByStats( ACTION_COMMAND_HEAL_INFANTRY ) )
		{
			for ( CUnitsIter<0,2> iter( GetParty(), EDI_FRIEND, GetCenter(), SConsts::MED_TRUCK_HEAL_RADIUS );
						!iter.IsFinished(); iter.Iterate() )
			{
				CAIUnit *pUnit = *iter;
				if ( pUnit->GetStats()->IsInfantry() )
				{
					pUnit->IncreaseHitPoints( SConsts::MED_TRUCK_HEAL_PER_UPDATEDURATION );
				}
			}
		}
		timeLastHeal = curTime;
	}

	if ( curTime > lastResupplyMorale + 2000 )
	{
		if ( IsAlive() && CanCommandBeExecutedByStats( ACTION_COMMAND_RESUPPLY_MORALE ) )
		{
			// heal morale of friendly units in range
			CUnitsIter<0,2> iter( GetParty(), EDI_FRIEND, GetCenter(), SConsts::RESUPPLY_RADIUS_MORALE );
			CPtr<CAIUnit> curUnit = 0;
			while ( !iter.IsFinished() )
			{
				curUnit = *iter;
				if ( IsValidObj( curUnit ) )
					curUnit->SetMoraleSupport();

				iter.Iterate();
			}
		}
		lastResupplyMorale = curTime ;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoldier* CMilitaryCar::GetPassenger( const int n )
{
	NI_ASSERT_T( n < pass.size(), "Wrong number of passenger" );

	std::list< CPtr<CSoldier> >::iterator pos = pass.begin();
	std::advance( pos, n );
	return *pos;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMilitaryCar::ClearAllPassengers()
{
	pass.clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMilitaryCar::DelPassenger( const int n )
{
	CheckRange( pass, n );
	std::list< CPtr<CSoldier> >::iterator pos = pass.begin();
	std::advance( pos, n );
	pass.erase( pos );
	//
	if ( pass.empty() )
	{
		for ( int i = 0; i < GetNTurrets(); ++i )
			GetTurret( i )->SetRotateTurretState( true );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMilitaryCar::DelPassenger( CSoldier *pSoldier )
{
	NI_ASSERT_T( std::find( pass.begin(), pass.end(), pSoldier ) != pass.end(), "Intransport soldier not found" );
	pass.remove( pSoldier );

	//
	if ( pass.empty() )
	{
		for ( int i = 0; i < GetNTurrets(); ++i )
			GetTurret( i )->SetRotateTurretState( true );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CMilitaryCar::GetDistanceToLandPoint() const
{
	return GetStats()->vAABBHalfSize.y + SConsts::GOOD_LAND_DIST;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CMilitaryCar::GetMaxFireRange() const
{
	return pGuns->GetMaxFireRange( this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMilitaryCar::Lock( CFormation *_pLockingUnit ) 
{ 
	NI_ASSERT_T( pLockingUnit == 0, "Transport is already locked" ); 
	pLockingUnit = _pLockingUnit; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMilitaryCar::GetNGuns() const { return pGuns->GetNTotalGuns(); }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBasicGun* CMilitaryCar::GetGun( const int n ) const { return pGuns->GetGun( n ); }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBasicGun* CMilitaryCar::ChooseGunForStatObj( class CStaticObject *pObj, NTimer::STime *pTime ) 
{ 
	return pGuns->ChooseGunForStatObj( this, pObj, pTime ); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CMilitaryCar::CanShootToPlanes() const 
{ 
	return pGuns->CanShootToPlanes(); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBasicGun* CMilitaryCar::GetFirstArtilleryGun() const
{
	return pGuns->GetFirstArtilleryGun(); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMilitaryCar::GetRangeArea( SShootAreas *pRangeArea ) const
{
	*pRangeArea = SShootAreas();	
	if ( GetState()->GetName() == EUSN_RANGING )
	{
		CCircle rangeCircle;
		static_cast<const CArtilleryRangeAreaState*>(GetState())->GetRangeCircle( &rangeCircle );
		pRangeArea->areas.push_back( SShootArea() );

		SShootArea &area = pRangeArea->areas.back();
		area.eType = SShootArea::ESAT_RANGE_AREA;
		area.fMinR = 0.0f;
		area.fMaxR = rangeCircle.r;
		area.vCenter3D = CVec3( rangeCircle.center, 0.0f );
		area.wStartAngle = 65535;
		area.wFinishAngle = 65535;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec3 CMilitaryCar::GetHookPoint3D() const
{
	const CVec3 vTraNormale = DWORDToVec3( GetNormale() );
	const CVec2 vTraDir = GetVectorByDirection( GetFrontDir() );
	CVec3 vTraDir3D;
	vTraDir3D.x = vTraDir.x;
	vTraDir3D.y = vTraDir.y;
	vTraDir3D.z = ( -vTraDir3D.x * vTraNormale.x - vTraDir3D.y * vTraNormale.y ) / vTraNormale.z;
	Normalize( &vTraDir3D );

	const CVec2 vTraCenter( GetCenter() );
	CVec3 vTraCenter3D( vTraCenter, theStaticMap.GetVisZ( vTraCenter.x, vTraCenter.y ) );

	return vTraCenter3D + vTraDir3D * static_cast<const SMechUnitRPGStats*>( GetStats() )->vTowPoint.y;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CMilitaryCar::GetHookPoint() const
{
	const CVec3 vHookPoint3D( GetHookPoint3D() );
	return CVec2( vHookPoint3D.x, vHookPoint3D.y );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMilitaryCar::LookForTarget( CAIUnit *pCurTarget, const bool bDamageUpdated, CAIUnit **pBestTarget, CBasicGun **pGun )
{
	CAIUnit::LookForTarget( pCurTarget, bDamageUpdated, pBestTarget, pGun );

	if ( GetFirstArtilleryGun() != 0 )
	{
		// в радиусе цели нет
		if ( *pBestTarget == 0 && 
				 ( pCurTarget == 0 || pCurTarget->GetStats()->IsInfantry() ) && theDipl.IsAIPlayer( GetPlayer() ) )
		{
			LookForFarTarget( pCurTarget, bDamageUpdated, pBestTarget, pGun );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CMilitaryCar::GetDispersionBonus() const
{ 
	return fDispersionBonus * theDifficultyLevel.GetDispersionCoeff( theDipl.GetNParty( GetPlayer() ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															CTank																*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CTank );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTank::Init( const CVec2 &center, const int z, const SUnitBaseRPGStats *pStats, const float fHP, const WORD dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID )
{
	bTrackDamaged = false;

	wDangerousDir = 0;
	bDangerousDirSet = false;
	bDangerousDirSetInertia = false;
	nextTimeOfDangerousDirScan = 0;
	lastTimeOfDangerousDirChanged = 0;

	wDangerousDirUnderFire = 0;
	fDangerousDamageUnderFire = -1.0f;

	CMilitaryCar::Init( center, z, pStats, fHP, dir, player, id, eVisType, dbID );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IStatesFactory* CTank::GetStatesFactory() const
{ 
	return CTankStatesFactory::Instance(); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTank::TakeDamage( const float fDamage, const SWeaponRPGStats::SShell *pShell, const int nPlayerOfShoot, CAIUnit *pShotUnit )
{
	CMilitaryCar::TakeDamage( fDamage, pShell, nPlayerOfShoot, pShotUnit );

	// обработать specials
	if ( IsAlive() && theCheats.GetImmortals( theDipl.GetNParty( nPlayerOfShoot ) ) != 1 )
	{
		// отрывает гусеницу, but not in the "easy" level
		if ( theDifficultyLevel.GetLevel() != 0 &&
				 pShell && ( pShell->specials.GetData( 0 ) || Random( 0.0f, 1.0f ) < pShell->fBrokeTrackProbability ) )
		{
			StopUnit();
			updater.Update( ACTION_NOTIFY_BREAK_TRACK, this );
			bTrackDamaged = true;
			TrackDamagedState( true );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTank::RepairTrack() 
{
	if ( bTrackDamaged )
	{
		bTrackDamaged = false;
		TrackDamagedState( false );
		updater.Update( ACTION_NOTIFY_REPAIR_TRACK, this );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTank::CanTurnToFrontDir( const WORD wDir )
{ 
	return !bTrackDamaged && CAIUnit::CanTurnToFrontDir( wDir ) && !IsInTankPit();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTank::CanMove() const
{
	return CMilitaryCar::CanMove() && !bTrackDamaged && !IsInTankPit();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTank::CanMovePathfinding() const
{
	return CMilitaryCar::CanMovePathfinding() && !bTrackDamaged && !IsInTankPit();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTank::CanRotate() const
{ 
	return GetStats()->fSpeed != 0 && GetPathUnit()->CanMove() && GetPathUnit()->CanRotate() && !bTrackDamaged && !IsInTankPit();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTank::ScanForDangerousDir()
{
	if ( nextTimeOfDangerousDirScan < curTime )
	{
		const float fR = 1.3f * Max( GetSightRadius(), Min( GetMaxFireRange(), SConsts::MAX_FIRE_RANGE_TO_SHOOT_BY_LINE ) );
		const CVec2 vCenter( GetCenter() );
		const int nParty = GetParty();

		float fDangerousDamage = fDangerousDamageUnderFire;
		bool bNewDangerousDirSet = fDangerousDamageUnderFire > 0.0f;
		if ( bNewDangerousDirSet )
			wDangerousDir = wDangerousDirUnderFire;

		fDangerousDamageUnderFire = -1.0f;

		for ( CUnitsIter<1,3> iter( nParty, EDI_ENEMY, vCenter, fR ); !iter.IsFinished(); iter.Iterate() )
		{
			CAIUnit *pUnit = *iter;
			if ( IsValidObj(pUnit) && ( pUnit->IsVisible( nParty ) || pUnit->IsRevealed() ) && pUnit->GetZ() <= 0.0f )
			{
				const float fDamage = pUnit->GetMaxDamage( this );
				if ( fDamage > fDangerousDamage )
				{
					fDangerousDamage = fDamage;
					wDangerousDir = GetDirectionByVector( pUnit->GetCenter() - vCenter );
					bNewDangerousDirSet = true;
				}
			}
		}

		// changed
		if ( bDangerousDirSet != bNewDangerousDirSet )
		{
			lastTimeOfDangerousDirChanged = curTime;

			if ( bNewDangerousDirSet || lastTimeOfDangerousDirChanged + 3000 < curTime )
				bDangerousDirSetInertia = bNewDangerousDirSet;
		}
		else if ( bDangerousDirSetInertia != bDangerousDirSet && lastTimeOfDangerousDirChanged + 3000 < curTime )
			bDangerousDirSetInertia = bDangerousDirSet;

		bDangerousDirSet = bNewDangerousDirSet;
		nextTimeOfDangerousDirScan = curTime + 1000 + Random( 0, 2000 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTank::Grazed( CAIUnit *pUnit )
{
	if ( IsValidObj( pUnit ) && !pUnit->GetStats()->IsAviation() )
	{
		const float fDamage = pUnit->GetMaxDamage( this );
		if ( fDamage > fDangerousDamageUnderFire )
		{
			fDangerousDamageUnderFire = fDamage;
			wDangerousDirUnderFire = GetDirectionByVector( pUnit->GetCenter() - GetCenter() );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTank::Segment()
{
	CMilitaryCar::Segment();
	ScanForDangerousDir();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTank::CanMoveAfterUserCommand() const
{
	return !IsTrackDamaged() && CAIUnit::CanMoveAfterUserCommand();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*														CAITransportUnit											*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CAITransportUnit );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITransportUnit::Init( const CVec2 &center, const int z, const SUnitBaseRPGStats *_pStats, const float fHP, const WORD dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID )
{
	CMilitaryCar::Init( center, z, _pStats, fHP, dir, player, id, eVisType, dbID)	;
	fResursUnits = SConsts::TRANSPORT_RU_CAPACITY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITransportUnit::SetResursUnitsLeft( float _fResursUnits ) 
{ 
	if ( fResursUnits != _fResursUnits )
	{
		fResursUnits = _fResursUnits; 
		updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITransportUnit::Segment()
{
	CMilitaryCar::Segment();

	if ( pTowedArtillery && !IsTowing() ) // убили буксоируемую пушку
	{
		updater.Update( ACTION_NOTIFY_STATE_CHANGED, this, ECS_UNHOOK_CANNON );
		pTowedArtillery = 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITransportUnit::DecResursUnitsLeft( float dRU ) 
{
	theStatistics.ResourceUsed( GetPlayer(), dRU );
	SetResursUnitsLeft( fResursUnits - dRU );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IStatesFactory* CAITransportUnit::GetStatesFactory() const 
{ 
	return CTransportStatesFactory::Instance(); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAITransportUnit::HasTowedArtilleryCrew() const 
{ 
	return IsValidObj( pTowedArtilleryCrew ); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITransportUnit::SetTowedArtilleryCrew( class CFormation *pFormation ) 
{ 
	pTowedArtilleryCrew = pFormation; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormation * CAITransportUnit::GetTowedArtilleryCrew() 
{ 
	return pTowedArtilleryCrew; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITransportUnit::SetMustTow( class CAIUnit *_pUnit ) 
{ 
	pMustTow = _pUnit; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAITransportUnit::IsMustTow() const 
{ 
	return IsValidObj( pMustTow );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAITransportUnit::CanCommandBeExecuted( CAICommand *pCommand )
{
	return	CMilitaryCar::CanCommandBeExecuted( pCommand ) &&
					
		( !IsValidObj( pTowedArtillery ) ||
			pCommand->ToUnitCmd().cmdType != ACTION_COMMAND_TAKE_ARTILLERY || 
			pCommand->ToUnitCmd().cmdType != ACTION_COMMAND_UNLOAD ) ;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAITransportUnit::IsTowing() const 
{ 
	return IsValidObj( pTowedArtillery );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITransportUnit::Die( const bool fromExplosion, const float fDamage )
{
	CAIUnit::Die( fromExplosion, fDamage );
	for ( CExternLoaders::iterator it = externLoaders.begin(); it != externLoaders.end(); )
	{
		FreeLoaders( *it, this );
		it = externLoaders.erase( it );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITransportUnit::GetRPGStats( SAINotifyRPGStats *pStats )
{
	CMilitaryCar::GetRPGStats( pStats );
	if ( GetNCommonGuns() == 0 )
		pStats->nMainAmmo = fResursUnits / SConsts::TRANSPORT_RU_CAPACITY * 1000.0f;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITransportUnit::SetTowedArtillery( CArtillery * _pTowedArtillery) 
{ 
	pTowedArtillery = _pTowedArtillery; 
	updater.Update( ACTION_NOTIFY_STATE_CHANGED, this, GetUnitState() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CAITransportUnit::GetUnitState() const
{
	if ( pTowedArtillery )
		return ECS_HOOK_CANNON;
	else
		return ECS_UNHOOK_CANNON;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITransportUnit::FreeLoaders( CFormation *pLoaderSquad, CAITransportUnit * pTransport ) 
{
	//kill loaders in transport
	if ( pLoaderSquad && pLoaderSquad->IsValid() && pLoaderSquad->IsAlive() )
	{
		std::list<CSoldier*> soldiersInside;
		const int nSize = pLoaderSquad->Size();
		for ( int i = 0; i < nSize; ++i )
		{
			CSoldier * pSold = (*pLoaderSquad)[i];
			if ( pSold->IsInSolidPlace() )
				soldiersInside.push_back( pSold );
		}
		while ( !soldiersInside.empty() )
		{
			(*soldiersInside.begin())->Die( false, 0.0f );
			soldiersInside.pop_front();
		}
	}

	// free others
	if ( pLoaderSquad && pLoaderSquad->IsValid() && pLoaderSquad->IsAlive() )
	{

		pLoaderSquad->SetResupplyable( true );
		theUnitCreation.SendFormationToWorld( pLoaderSquad );
		pLoaderSquad->SetSelectable( pLoaderSquad->GetPlayer() == theDipl.GetMyNumber() );
		theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_MOVE_TO, pLoaderSquad->GetCenter()), pLoaderSquad, false );
	}
	if ( pTransport && pTransport->IsValid() && pTransport->IsAlive() )
		pTransport->Unlock();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITransportUnit::PrepareLoaders( CFormation *pLoaderSquad, CAITransportUnit * pTransport ) 
{
	NI_ASSERT_T( pTransport->IsValid() && pTransport->IsAlive(), " not valid transport passed" );
	const CVec3 vEntrancePoint( pTransport->GetEntrancePoint(), 0 );
	pLoaderSquad->SetResupplyable( false );
	pLoaderSquad->SetNewCoordinates( vEntrancePoint, false );
	updater.Update( ACTION_NOTIFY_NEW_FORMATION, (*pLoaderSquad)[0] );

	pLoaderSquad->SetFree();
	for ( int i = 0; i < pLoaderSquad->Size(); ++ i )
	{
		CPtr<CSoldier> pLandUnit = (*pLoaderSquad)[i];
		if ( pLandUnit->IsInSolidPlace() )
			pLandUnit->SetCoordWOUpdate( vEntrancePoint );
		else
			pLandUnit->SetNewCoordinates( vEntrancePoint, false );

		pLandUnit->SetFree();
		updater.Update( ACTION_NOTIFY_NEW_UNIT, pLandUnit );
		updater.Update( ACTION_NOTIFY_CHANGE_VISIBILITY, pLandUnit, pLandUnit->IsVisibleByPlayer() );
	}				
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITransportUnit::AddExternLoaders( CFormation *pLoaders )
{ 
	if ( pLoaders && pLoaders->IsValid() && pLoaders->IsAlive() )
		externLoaders.push_back( pLoaders );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAITransportUnit::CanHookUnit( CAIUnit *pUnitToHook ) const
{
	if ( CanCommandBeExecutedByStats( ACTION_COMMAND_TAKE_ARTILLERY ) )
	{
		const float fWeight = pUnitToHook->GetStats()->fWeight;
		const float fTowForce = static_cast<const SMechUnitRPGStats*>(GetStats())->fTowingForce; 

		return fWeight <= fTowForce;
	}
	else
		return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CAITransportUnit::GetNUnitToTakeArtillery( bool bPlaceInQueue, CAIUnit *pUnitToTake )
{
	float fMinDist = 0.0f;
	CCommonUnit *pBestUnit = 0;
	
	const int nGroup = GetNGroup();
	for ( int i = theGroupLogic.BeginGroup( nGroup ); i != theGroupLogic.EndGroup(); i = theGroupLogic.Next( i ) )
	{
		CCommonUnit *pUnit = theGroupLogic.GetGroupUnit( i );
		const bool bDoItNow = !bPlaceInQueue || pUnit->IsEmptyCmdQueue() ||
												  pUnit->GetNextCommand()->ToUnitCmd().cmdType == ACTION_COMMAND_STOP;
													
		if ( pUnit->CanHookUnit( pUnitToTake ) && ( !bDoItNow || !pUnit->IsTowing() ) )
		{
			if ( ( !pUnit->GetState() ||  pUnit->GetState()->GetName() != EUSN_HOOK_ARTILLERY ) &&
					 ( pUnit->IsEmptyCmdQueue() || pUnit->GetLastCommand()->ToUnitCmd().cmdType != ACTION_COMMAND_TAKE_ARTILLERY ) )
			{
				const float fDist = fabs2( pUnit->GetCenter() - pUnitToTake->GetCenter() );
				if ( pBestUnit == 0 || fMinDist > fDist )
				{
					fMinDist = fDist;
					pBestUnit = pUnit;
				}
			}
		}
	}

	if ( pBestUnit == 0 )
		return -2;
	else
		return pBestUnit->GetUniqueId();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITransportUnit::UnitCommand( CAICommand *pCommand, bool bPlaceInQueue, bool bOnlyThisUnitCommand )
{
	if ( !bOnlyThisUnitCommand && pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_TAKE_ARTILLERY )
	{
		if ( pCommand->ToUnitCmd().pObject == 0 ) return;

		CAIUnit *pArtillery = dynamic_cast_ptr<CAIUnit*>(pCommand->ToUnitCmd().pObject);
		if ( pArtillery )
		{
			int nUnitToTakeArtillery = pCommand->GetFlag();
			if ( nUnitToTakeArtillery == -1 )
			{
				nUnitToTakeArtillery = GetNUnitToTakeArtillery( bPlaceInQueue, pArtillery );
				pCommand->SetFlag( nUnitToTakeArtillery );
			}

			// can't hook artillery
			if ( nUnitToTakeArtillery == -2 )
			{
				if ( !CanHookUnit( pArtillery ) & !bPlaceInQueue )
					SendAcknowledgement( pCommand, ACK_NO_TOO_HEAVY_ARTILLERY_FOR_TRANSPORT );
			}

			if ( nUnitToTakeArtillery == GetUniqueId() )
				CMilitaryCar::UnitCommand( pCommand, bPlaceInQueue, bOnlyThisUnitCommand );
			else if ( !bPlaceInQueue )
				UnitCommand( new CAICommand( SAIUnitCmd( ACTION_COMMAND_STOP ) ), false, bOnlyThisUnitCommand );
		}
	}
	else
		CMilitaryCar::UnitCommand( pCommand, bPlaceInQueue, bOnlyThisUnitCommand );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
