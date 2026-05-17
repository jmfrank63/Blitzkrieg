#include "stdafx.h"

#include "Building.h"
#include "Soldier.h"
#include "Updater.h"
#include "UnitStates.h"
#include "StaticObjects.h"
#include "UnitsIterators2.h"
#include "Diplomacy.h"
#include "Randomize.h"
#include "MountedGun.h"
#include "GunsInternal.h"
#include "UnitGuns.h"
#include "Turret.h"
#include "AIWarFog.h"
#include "Cheats.h"
#include "PathFinder.h"
#include "Statistics.h"
#include "Path.h"
#include "MultiplayerInfo.h"
#include "UnitStates.h"
#include "Formation.h"
#include "GroupLogic.h"
#include "Scripts\Scripts.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CUpdater updater;
extern NTimer::STime curTime;
extern CStaticObjects theStatObjs;
extern CDiplomacy theDipl;
extern CGlobalWarFog theWarFog;
extern SCheats theCheats;
extern CStatistics theStatistics;
extern CMultiplayerInfo theMPInfo;
extern CScripts *pScripts;
extern CGroupLogic theGroupLogic;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												  CBuildingStorage												*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CBuildingStorage );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBuildingStorage::CBuildingStorage( const SBuildingRPGStats *pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex, int player )
: CBuilding( pStats, center, dbID, fHP, nFrameIndex ), nPlayer( player ), timeLastBuildingRepair( 0 ), bConnected( true ) 
{ 
//	theStatObjs.UpdateStoragesForParty( theDipl.GetNParty(nPlayer), true, false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingStorage::SetHitPoints( const float fNewHP )
{
	//if ( 0 == fHP && fNewHP > 0 && SBuildingRPGStats::TYPE_TEMP_RU_STORAGE == pStats->eType )
		//theStatObjs.UpdateStoragesForParty( theDipl.GetNParty(nPlayer), true, false );
	CBuilding::SetHitPoints( fNewHP );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingStorage::AddSoldier( CSoldier *pUnit )
{
	CBuilding::AddSoldier(pUnit);
	if ( SBuildingRPGStats::TYPE_MAIN_RU_STORAGE != pStats->eType )
		ChangePlayer( pUnit->GetPlayer() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingStorage::ChangePlayer( const int _nPlayer )
{
	const CVec2 vCenter = GetCenter();
	const DWORD dwParam = int(vCenter.x) << 32 | int( vCenter.y);

	if ( EDI_ENEMY == theDipl.GetDiplStatus( _nPlayer, nPlayer ) )
	{
		if ( EDI_FRIEND == theDipl.GetDiplStatus( _nPlayer, theDipl.GetMyNumber() ) )
			updater.AddFeedBack( SAIFeedBack(EFB_YOU_CAPTURED_STORAGE,dwParam) );
		else if ( EDI_FRIEND == theDipl.GetDiplStatus( nPlayer, theDipl.GetMyNumber() ) )
			updater.AddFeedBack( SAIFeedBack(EFB_YOU_LOST_STORAGE, dwParam) );
	}
	theStatObjs.StorageChangedDiplomacy( this, _nPlayer );
	//theStatObjs.UpdateStoragesForParty( theDipl.GetNParty(nPlayer), true, false );
	//theStatObjs.UpdateStoragesForParty( theDipl.GetNParty(_nPlayer), true, false );
	nPlayer = _nPlayer;
	updater.Update( ACTION_NOTIFY_UPDATE_DIPLOMACY, this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingStorage::SetConnected( bool _bConnected )
{
	if ( bConnected != _bConnected )
	{
		bConnected = _bConnected;
		updater.Update( ACTION_NOTIFY_STORAGE_CONNECTED, this, bConnected );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const BYTE CBuildingStorage::GetPlayer() const
{
	BYTE res = CBuilding::GetPlayer();
	if ( res == theDipl.GetNeutralPlayer() )
		return nPlayer;
	return res;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingStorage::GetNewUnitInfo( SNewUnitInfo *pNewUnitInfo )
{
	CBuilding::GetNewUnitInfo( pNewUnitInfo );

	pNewUnitInfo->nPlayer = GetPlayer();
	pNewUnitInfo->eDipl = theDipl.GetDiplStatus( theDipl.GetMyNumber(), pNewUnitInfo->nPlayer );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingStorage::TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit )
{
	if ( SBuildingRPGStats::TYPE_TEMP_RU_STORAGE == pStats->eType && fHP > 0 && fHP - fDamage <= 0 )
		SetConnected( false );

	CBuilding::TakeDamage( fDamage, bFromExplosion, nPlayerOfShoot, pShotUnit );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingStorage::Segment()
{
	CBuilding::Segment();

	if ( SBuildingRPGStats::TYPE_MAIN_RU_STORAGE == pStats->eType )
	{
		const float fMaxHP = GetStats()->fMaxHP;

		if ( fHP < fMaxHP )
		{
			fHP += (curTime - timeLastBuildingRepair) * SConsts::MAIN_STORAGE_HEALING_SPEED / 1000;
			if ( fHP > fMaxHP )
				fHP = fMaxHP;
			updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );
		}
		timeLastBuildingRepair = curTime;
	}
	else //	TEMP STORAGES	can change ownership
	{
		int nNewPlayer = GetPlayer();
		// neutral storage can be taken by the first unit
		if ( theDipl.GetNParty( GetPlayer() ) == theDipl.GetNeutralParty() )
		{
			for ( CUnitsIter<0,3> iter( theDipl.GetNParty(nPlayer), ANY_PARTY, GetCenter(), SConsts::RADIUS_TO_TAKE_STORAGE_OWNERSHIP );
						!iter.IsFinished(); iter.Iterate() )
			{
				CPtr<CAIUnit> curUnit = *iter;
				const int nUnitPlayer = curUnit->GetPlayer();
				if ( curUnit->IsAlive() && !curUnit->GetStats()->IsAviation() &&
						 nUnitPlayer != nPlayer && theDipl.GetNParty( nUnitPlayer ) != theDipl.GetNeutralParty() )
				{
					nNewPlayer = nUnitPlayer;
					break;
				}
			}

			if ( nNewPlayer != GetPlayer() )
				ChangePlayer( nNewPlayer );
		}
		else
		{
			// scan for friendly units, if not present, then scan for enemy
			bool bPresent = false;

			for ( CUnitsIter<0,3> iter( theDipl.GetNParty(nPlayer), EDI_FRIEND, GetCenter(), SConsts::RADIUS_TO_TAKE_STORAGE_OWNERSHIP );
						!iter.IsFinished(); iter.Iterate() )
			{
				CPtr<CAIUnit> curUnit = *iter;
				if ( curUnit->IsAlive() && curUnit->GetPlayer() == nPlayer && !curUnit->GetStats()->IsAviation() )
				{
					bPresent = true;
					break;
				}
			}

			// if enemy present - change owner of this storage
			if ( !bPresent )
			{
				for ( CUnitsIter<0,3> iter( theDipl.GetNParty(nPlayer), EDI_ENEMY, GetCenter(), SConsts::RADIUS_TO_TAKE_STORAGE_OWNERSHIP );
							!iter.IsFinished(); iter.Iterate() )
				{
					CPtr<CAIUnit> curUnit = *iter;
					if ( curUnit->IsAlive() && curUnit->GetPlayer() != theDipl.GetNeutralPlayer() && !curUnit->GetStats()->IsAviation() )
					{
						ChangePlayer( curUnit->GetPlayer() );
						break;
					}
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildingStorage::ShouldSuspendAction( const EActionNotify &eAction ) const
{
	return !theDipl.IsEditorMode() && CBuilding::ShouldSuspendAction( eAction );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										CBuilding::SHealthySort												*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::SHealthySort::operator()( const CPtr<CSoldier> &a, const CPtr<CSoldier> &b ) 
{ 
	return a->GetHitPoints() < b->GetHitPoints(); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CBuilding::SIllSort													*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::SIllSort::operator()( const CPtr<CSoldier> &a, const CPtr<CSoldier> &b ) 
{ 
	return a->GetHitPoints() > b->GetHitPoints(); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												  CBuilding																*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CBuilding );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBuilding::CBuilding( const SBuildingRPGStats *_pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex )
: pStats( _pStats ), CGivenPassabilityStObject( center, dbID, fHP, nFrameIndex ), pLockingUnit( 0 ),
	nOveralPlaces( _pStats->nRestSlots + _pStats->nMedicalSlots + _pStats->slots.size() ),
	medical( _pStats->nMedicalSlots ), fire( _pStats->slots.size() ), rest( _pStats->nRestSlots ),
	bAlarm( false ), 
	turrets( _pStats->slots.size() ), guns( _pStats->slots.size() ),
	nextSegmTime( curTime ), lastDistibution( 0 ), nLastFreeFireSoldierChoice( 0 ),
	firePlace2Soldier( _pStats->slots.size() ),
	nLastPlayer( theDipl.GetNeutralPlayer() ), nScriptID( -1 ), bShouldEscape( false ), bEscaped( false ), timeOfDeath( 0 ),
	lastLeave( theDipl.GetNPlayers(), 0 )
{
	Init();
	
	for ( int i = 0; i < pStats->slots.size(); ++i )
	{
		guns[i] = new CMechUnitGuns();
		if ( pStats->slots[i].gun.pWeapon != 0 )
		{
			turrets[i] = new CMountedTurret( this, i );
			
			int nGuns = 0;
			guns[i]->AddGun( CMountedGunsFactory( this, static_cast_ptr<CMountedTurret*>(turrets[i]), i ), pStats->slots[i].gun.pWeapon, &nGuns, pStats->slots[i].gun.nAmmo );
		}
	}

	InitObservationPlaces();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::InitObservationPlaces()
{
	observationPlaces.SetSizes( 4, 4 );
	sides.resize( 4 );
	firePlace2Observation.resize( pStats->slots.size(), -1 );

	SRect boundRect;
	GetBoundRect( &boundRect );
	CVec2 vCenter( boundRect.center );

	std::vector<int> takenSlots( pStats->slots.size(), 0 );

	for ( int i = 0; i < 4; ++i )
	{
		int nLeftSlot = -1;
		WORD wLeftDiff = 65535;
		int nMiddleSlot = -1;
		WORD wMiddleDiff = 65535;
		int nRightSlot = -1;
		WORD wRightDiff = 65535;

		const CVec2 vRightPoint = boundRect.v[i] - vCenter; 
		const CVec2 vLeftPoint = boundRect.v[(i+1) % 4] - vCenter;
		const CVec2 vMiddlePoint = vRightPoint + vLeftPoint;
		WORD wRightDir = GetDirectionByVector( vRightPoint );
		WORD wLeftDir = GetDirectionByVector( vLeftPoint );
		WORD wMiddleDir = GetDirectionByVector( vMiddlePoint );

		for ( int j = 0; j < pStats->slots.size(); ++j )
		{
			const CVec2 vSlotDir = CVec2( pStats->slots[j].vPos.x, pStats->slots[j].vPos.y ) + GetCenter() - vCenter;
			const WORD wSlotDir = GetDirectionByVector( vSlotDir );

			const WORD wSlotSightAngle = pStats->slots[j].wAngle;
			const WORD wSlotSightDir = pStats->slots[j].wDirection;
			if ( takenSlots[j] == 0 && DirsDifference( pStats->slots[j].wDirection, wMiddleDir ) < 4000 )
			{
				if ( !IsInTheMinAngle( wSlotDir, wLeftDir, wRightDir ) )
				{
					if ( DirsDifference( wSlotDir, wLeftDir ) < DirsDifference( wSlotDir, wRightDir ) )
						wLeftDir = wSlotDir;
					else
						wRightDir = wSlotDir;
				}
				
				++sides[i].nFireSlots;
				takenSlots[j] = 1;

				const WORD wLocalLeftDiff = DirsDifference( wSlotDir, wLeftDir );
				if ( wLocalLeftDiff < wLeftDiff )
				{
					nLeftSlot = j;
					wLeftDiff = wLocalLeftDiff;
				}
			}
		}

		for ( int j = 0; j < pStats->slots.size(); ++j )
		{
			const CVec2 vSlotDir = CVec2( pStats->slots[j].vPos.x, pStats->slots[j].vPos.y ) + GetCenter() - vCenter;
			const WORD wSlotDir = GetDirectionByVector( vSlotDir );

			if ( nLeftSlot != j && DirsDifference( pStats->slots[j].wDirection, wMiddleDir ) < 4000 )
			{
				const WORD wLocalRightDiff = DirsDifference( wSlotDir, wRightDir );
				if ( wLocalRightDiff < wRightDiff )
				{
					nRightSlot = j;
					wRightDiff = wLocalRightDiff;
				}
			}
		}

		for ( int j = 0; j < pStats->slots.size(); ++j )
		{
			const CVec2 vSlotDir = CVec2( pStats->slots[j].vPos.x, pStats->slots[j].vPos.y ) + GetCenter() - vCenter;
			const WORD wSlotDir = GetDirectionByVector( vSlotDir );

			if ( nLeftSlot != j && nRightSlot != j && DirsDifference( pStats->slots[j].wDirection, wMiddleDir ) < 4000 )
			{
				const WORD wLocalMiddleDiff = DirsDifference( wSlotDir, wMiddleDir );
				if ( wLocalMiddleDiff < wMiddleDiff )
				{
					nMiddleSlot = j;
					wMiddleDiff = wLocalMiddleDiff;
				}
			}
		}

		if ( nRightSlot != -1 )
		{
			observationPlaces[i][sides[i].nObservationPoints] = nRightSlot;
			firePlace2Observation[nRightSlot] = (sides[i].nObservationPoints << 2) | i;
			++sides[i].nObservationPoints;
		}

		if ( nMiddleSlot != -1 )
		{
			observationPlaces[i][sides[i].nObservationPoints] = nMiddleSlot;
			firePlace2Observation[nMiddleSlot] = (sides[i].nObservationPoints << 2) | i;
			++sides[i].nObservationPoints;
		}

		if ( nLeftSlot != -1 )
		{
			observationPlaces[i][sides[i].nObservationPoints] = nLeftSlot;
			firePlace2Observation[nLeftSlot] = ( sides[i].nObservationPoints << 2 ) | i;
			++sides[i].nObservationPoints;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAIUnit* CBuilding::GetIteratedUnit() 
{ 
	NI_ASSERT_TF( !IsIterateFinished(), "Wrong fire unit to get", return 0 );
	return fire[nIterator];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::PopFromFire()
{
	// ���� ��� �� ���������� � ������ ����
	if ( fire.GetMaxEl()->GetSlot() != -1 )
		DelSoldierFromFirePlace( fire.GetMaxEl() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::SetFiringUnitProperties( CSoldier *pUnit, const int nSlot, const int nIndex )
{
	pUnit->SetSlotInfo( nSlot, 0, nIndex );

	pUnit->SetVisionAngle( 32768 );
	pUnit->SetAngles( pStats->slots[nSlot].wDirection - pStats->slots[nSlot].wAngle, pStats->slots[nSlot].wDirection + pStats->slots[nSlot].wAngle );
	pUnit->SetSightMultiplier( pStats->slots[nSlot].fSightMultiplier );

	pUnit->SetNewCoordinates( CVec3( GetCenter(), 0 ) + pStats->slots[nSlot].vPos );

	// ��������� owner � mounted gun
	guns[nSlot]->SetOwner( pUnit );

	pUnit->ChangeWarFogState();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const BYTE CBuilding::GetFreeFireSlot()
{
	for ( int j = 0; j < firePlace2Soldier.size(); ++j )
	{
		if ( firePlace2Soldier[j] == 0 && turrets[j] != 0 )
		{
			nLastFreeFireSoldierChoice = (nLastFreeFireSoldierChoice + 1) % pStats->slots.size();
			return j;
		}
	}
	
	int i = nLastFreeFireSoldierChoice;
	while ( i < firePlace2Soldier.size() && firePlace2Soldier[i] != 0 )
		++i;

	if ( i >= firePlace2Soldier.size() || firePlace2Soldier[i] != 0 )
	{
		 i = 0;
		 while ( i < nLastFreeFireSoldierChoice && firePlace2Soldier[i] != 0 )
			 ++i;

		NI_ASSERT_T( i < nLastFreeFireSoldierChoice, "Can't find empty fireplace" );	
	}

	nLastFreeFireSoldierChoice = (nLastFreeFireSoldierChoice + 1) % pStats->slots.size();

	return i;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::PushToFire( CSoldier *pUnit )
{
	NI_ASSERT_T( pUnit->IsInSolidPlace() || pUnit->IsInFirePlace(), "Visible unit in a building" );
	pUnit->SetToFirePlace();
	
	const int nSlot = GetFreeFireSlot();
	PushSoldierToFirePlace( pUnit, nSlot );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::PushToMedical( CSoldier *pUnit )
{
	NI_ASSERT_T( pUnit->IsInSolidPlace() || pUnit->IsInFirePlace(), "Visible unit in a building" );

	pUnit->SetSlotInfo( -1, 1, medical.Size() );
	medical.Push( pUnit );
	pUnit->SetToSolidPlace();

	pUnit->SetVisionAngle( 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::PushToRest( CSoldier *pUnit )
{
	NI_ASSERT_T( pUnit->IsInSolidPlace() || pUnit->IsInFirePlace(), "Visible unit in a building" );

	pUnit->SetSlotInfo( -1, 2, rest.Size() );
	rest.Push( pUnit );
	pUnit->SetToSolidPlace();

	pUnit->SetVisionAngle( 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CBuilding::GetEntrancePoint( const int nEntrance ) const
{
	NI_ASSERT_T( nEntrance < GetNEntrancePoints(), "Wrong number of entrance point" );
	return GetCenter() + CVec2( pStats->entrances[nEntrance].vPos.x, pStats->entrances[nEntrance].vPos.y );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CBuilding::GetEscapeHitPoints() const
{
	return GetStats()->fMaxHP * SConsts::HP_PERCENT_TO_ESCAPE_FROM_BUILDING;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::AddSoldier( CSoldier *pUnit )
{
	NI_ASSERT_T( GetNFreePlaces() != 0, "No free places in the building" );

	bool bUpdateSelectability = false;
	// ���� ��� ������ ������ ������, �� ���������������� � ���������
	if ( GetNFreePlaces() == nOveralPlaces )
	{
		nextSegmTime = curTime + SConsts::AI_SEGMENT_DURATION - 1;		
		theStatObjs.RegisterSegment( this );

		bUpdateSelectability = true;
		bShouldEscape = GetHitPoints() > GetEscapeHitPoints();
	}
	bEscaped = false;

	pUnit->SetToSolidPlace();

	const float fHP = pUnit->GetHitPoints();
	if ( fire.Size() < fire.GetReserved() )
		PushToFire( pUnit );
	else if ( medical.Size() == medical.GetReserved() )
		PushToRest( pUnit );
	else if ( fHP < pUnit->GetStats()->fMaxHP || rest.Size() == rest.GetReserved() )
		PushToMedical( pUnit );
	else
		PushToRest( pUnit );

	startOfRest = curTime;

	if ( bUpdateSelectability )
	{
		updater.Update( ACTION_NOTIFY_SELECTABLE_CHANGED, this, IsSelectable() );
		updater.Update( ACTION_NOTIFY_UPDATE_DIPLOMACY, this );

		nLastPlayer = GetPlayer();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::SeatSoldierToMedicalSlot()
{
	NI_ASSERT_T( medical.Size() < medical.GetReserved(), "Wrong call of SeatSoldierToMedicalSlot" );

	if ( !rest.IsEmpty() && rest.GetMaxEl()->GetHitPoints() < rest.GetMaxEl()->GetStats()->fMaxHP )
	{
		CSoldier *pSoldier = rest.GetMaxEl();
		rest.Pop();
		PushToMedical( pSoldier );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::SeatSoldierToFireSlot()
{
	NI_ASSERT_T( fire.Size() < fire.GetReserved(), "Wrong call of SeatSoldierToFireSlot" );

	if ( medical.IsEmpty() && rest.IsEmpty() )
		return;

	if ( medical.IsEmpty() )
	{
		CSoldier *pSoldier = rest.GetMaxEl();
		rest.Pop();
		PushToFire( pSoldier );
	}
	else if ( rest.IsEmpty() )
	{
		CSoldier *pSoldier = medical.GetMaxEl();
		medical.Pop();
		PushToFire( pSoldier );
	}
	else if ( medical.GetMaxEl()->GetHitPoints() > rest.GetMaxEl()->GetHitPoints() )
	{
		CSoldier *pSoldier = medical.GetMaxEl();
		medical.Pop();
		PushToFire( pSoldier );
		SeatSoldierToMedicalSlot();
	}
	else
	{
		CSoldier *pSoldier = rest.GetMaxEl();
		rest.Pop();
		PushToFire( pSoldier );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::DelSoldierFromFirePlace( CSoldier *pSoldier )
{
	const SStaticObjectSlotInfo slotInfo = pSoldier->GetSlotInfo();
	NI_ASSERT_T( slotInfo.nType == 0, NStr::Format( "Wrong of soldier slot type (%d)", slotInfo.nType ) );

	// CRAP{
	if ( slotInfo.nIndex < fire.Size() )
	// CRAP}
	{
		fire[slotInfo.nIndex] = 0;
		fire.Erase( slotInfo.nIndex );
	}

	// CRAP{
	if ( slotInfo.nSlot < firePlace2Soldier.size() )
	// CRAP}
	{
		firePlace2Soldier[slotInfo.nSlot] = 0;

		// � observation point
		if ( firePlace2Observation[slotInfo.nSlot] != -1 )
		{
			const int nSide = firePlace2Observation[slotInfo.nSlot] & 3;
			--sides[nSide].nSoldiersInObservationPoints;
		}
	}

	pSoldier->SetSlotInfo( -1, -1, -1 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::DelSoldierFromMedicalPlace( CSoldier *pSoldier )
{
	const SStaticObjectSlotInfo slotInfo = pSoldier->GetSlotInfo();
	NI_ASSERT_T( slotInfo.nType == 1, NStr::Format( "Wrong of soldier slot type (%d)", slotInfo.nType ) );

	medical[slotInfo.nIndex] = 0;
	medical.Erase( slotInfo.nIndex );

	pSoldier->SetSlotInfo( -1, -1, -1 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::DelSoldierFromRestPlace( CSoldier *pSoldier )
{
	const SStaticObjectSlotInfo slotInfo = pSoldier->GetSlotInfo();
	NI_ASSERT_T( slotInfo.nType == 2, NStr::Format( "Wrong of soldier slot type (%d)", slotInfo.nType ) );

	// CRAP{
	if ( slotInfo.nIndex < rest.Size() )
	// CRAP}
	{
		rest[slotInfo.nIndex] = 0;
		rest.Erase( slotInfo.nIndex );
	}

	pSoldier->SetSlotInfo( -1, -1, -1 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::DelSoldier( CSoldier *pUnit, const bool bFillEmptyFireplace )
{
	const SStaticObjectSlotInfo slotInfo = pUnit->GetSlotInfo();
	switch ( slotInfo.nType )
	{
		case 0:
			DelSoldierFromFirePlace( pUnit );
			if ( bFillEmptyFireplace )
				SeatSoldierToFireSlot();

			break;
		case 1:
			DelSoldierFromMedicalPlace( pUnit );
			if ( bFillEmptyFireplace )
				SeatSoldierToMedicalSlot();

			break;
		case 2:
			DelSoldierFromRestPlace( pUnit );

			break;
		default:
			NI_ASSERT_T( false, "Wrong slot info of unit" );
	}

	if ( fire.Size() == 0 && medical.Size() == 0 && rest.Size() == 0 )
	{
		updater.Update( ACTION_NOTIFY_UPDATE_DIPLOMACY, this );
		updater.Update( ACTION_NOTIFY_SELECTABLE_CHANGED, this, IsSelectable() );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::SoldierDamaged( CSoldier *pUnit )
{
	const SStaticObjectSlotInfo slotInfo = pUnit->GetSlotInfo();
	switch ( slotInfo.nType )
	{
		case 0:
			fire[slotInfo.nIndex] = 0;
			fire.Erase( slotInfo.nIndex );
			pUnit->SetSlotInfo( slotInfo.nSlot, 0, fire.Size() );
			fire.Push( pUnit );

			break;
		case 1:
			medical[slotInfo.nIndex] = 0;
			medical.Erase( slotInfo.nIndex );
			pUnit->SetSlotInfo( -1, 1, medical.Size() );
			medical.Push( pUnit );

			break;
		case 2:
			rest[slotInfo.nIndex] = 0;
			rest.Erase( slotInfo.nIndex );
			pUnit->SetSlotInfo( -1, 2, rest.Size() );
			rest.Push( pUnit );

			break;
		default:
			NI_ASSERT_T( false, "Wrong slot info of unit" );
	}

	Alarm();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::GoOutFromEntrance( const int nEntrance, class CSoldier *pUnit )
{
	NI_ASSERT_T( nEntrance < pStats->entrances.size(), "Wrong number of entrance" );
	DelInsider( pUnit );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::SwapFireMed()
{
	CPtr<CSoldier> pFireSoldier = fire.GetMaxEl();
	PopFromFire();
	CPtr<CSoldier> pMedicalSoldier = medical.GetMaxEl();
	medical.Pop();
	PushToFire( pMedicalSoldier );
	PushToMedical( pFireSoldier );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::SwapRestMed()
{
	CPtr<CSoldier> pMedicalSoldier = medical.GetMaxEl();
	medical.Pop();
	CPtr<CSoldier> pRestSoldier = rest.GetMaxEl();
	rest.Pop();

	PushToMedical( pRestSoldier ); 
	PushToRest( pMedicalSoldier );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::IsIllInRest()
{
	return !rest.IsEmpty() && rest.GetMaxEl()->GetHitPoints() < rest.GetMaxEl()->GetStats()->fMaxHP;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::IsIllInFire()
{
	return !fire.IsEmpty() && fire.GetMaxEl()->GetHitPoints() < fire.GetMaxEl()->GetStats()->fMaxHP;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::DistributeAll()
{
	// �������� ���, ��� ��������� � medical � �������� � fire ��� rest	
	while ( !medical.IsEmpty() && medical.GetMaxEl()->GetHitPoints() == medical.GetMaxEl()->GetStats()->fMaxHP &&	( IsIllInFire() || IsIllInRest() ) )
	{
		const bool bIllInFire = IsIllInFire();
		const bool bIllInRest = IsIllInRest();

		if ( bIllInFire && bIllInRest )
		{
			if ( fire.GetMaxEl()->GetHitPoints() < rest.GetMaxEl()->GetHitPoints() )
				SwapFireMed();
			else
				SwapRestMed();
		}
		else if ( bIllInFire )
			SwapFireMed();
		else
			SwapRestMed();
	}

	// ������� ���������� �� medical places � fireplaces
	while ( !medical.IsEmpty() && medical.GetMaxEl()->GetHitPoints() == medical.GetMaxEl()->GetStats()->fMaxHP &&	fire.Size() != fire.GetReserved() )
	{
		CSoldier *pSoldier = medical.GetMaxEl();
		medical.Pop();
		PushToFire( pSoldier );
	}

	// ������� ���������� �� medical places � restplaces
	while ( !medical.IsEmpty() && medical.GetMaxEl()->GetHitPoints() == medical.GetMaxEl()->GetStats()->fMaxHP &&	rest.Size() != rest.GetReserved() )
	{
		CSoldier *pSoldier = medical.GetMaxEl();
		medical.Pop();
		PushToRest( pSoldier );
	}

	// ������� ���, ���� ���������, ��������
	while ( medical.Size() != medical.GetReserved() && ( IsIllInRest() || IsIllInFire() ) )
	{
		if ( !fire.IsEmpty() && !rest.IsEmpty() )
		{
			if ( fire.GetMaxEl()->GetHitPoints() < rest.GetMaxEl()->GetHitPoints() )
			{
				CSoldier *pSoldier = fire.GetMaxEl();
				DelSoldierFromFirePlace( pSoldier );
				PushToMedical( pSoldier );
			}
			else
			{
				CSoldier *pSoldier = rest.GetMaxEl();
				rest.Pop();
				PushToMedical( pSoldier );
			}
		}
		else if ( !rest.IsEmpty() )
		{
			CSoldier *pSoldier = rest.GetMaxEl();
			rest.Pop();
			PushToMedical( pSoldier );
		}
		else
		{
			CSoldier *pSoldier = fire.GetMaxEl();
			DelSoldierFromFirePlace( pSoldier );
			PushToMedical( pSoldier );
		}
	}

	SetSoldiersToObservationPoints();
	CentreSoldiersInObservationPoints();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::CentreSoldiersInObservationPoints()
{
	for ( int i = 0; i < fire.Size(); ++i )
	{
		CSoldier *pSoldier = fire[i];
		if ( IsSoldierInObservationPoint( pSoldier ) )
		{
			int nSide = firePlace2Observation[pSoldier->GetSlot()] & 3;
			if ( sides[nSide].nSoldiersInObservationPoints == 1 && GetMiddleObservationPoint( nSide ) != pSoldier->GetSlot() )
			{
				DelSoldierFromFirePlace( pSoldier );
				PushSoldierToFirePlace( pSoldier, GetMiddleObservationPoint( nSide ) );
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::TryToPushRestSoldierToObservation( CSoldier *pRestingSoldier )
{
	// �� ��������
	for ( int j = 0; j < 4; ++j )
	{
		// �� ������ ���������� � ��������
		for ( int k = 0; k < sides[j].nObservationPoints; ++k )
		{
			if ( CSoldier *pSoldierInPoint = firePlace2Soldier[observationPlaces[j][k]] )
			{
				if ( IsBetterChangeObservationSoldier( pRestingSoldier, pSoldierInPoint ) )
				{
					SStaticObjectSlotInfo slotInfo = pRestingSoldier->GetSlotInfo();
					NI_ASSERT_T( slotInfo.nType == 2, "Wrong slot info of resting soldier" );

					rest[slotInfo.nIndex] = 0;
					rest.Erase( slotInfo.nIndex );

					DelSoldierFromFirePlace( pSoldierInPoint );
					PushToRest( pSoldierInPoint );
					PushSoldierToFirePlace( pRestingSoldier, observationPlaces[j][k] );

					return true;
				}
			}
		}
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::TryToPushFireSoldierToObservation( CSoldier *pFiringSoldier )
{
	// �� ��������
	for ( int j = 0; j < 4; ++j )
	{
		// �� ������ ���������� � ��������
		for ( int k = 0; k < sides[j].nObservationPoints; ++k )
		{
			if ( CSoldier *pSoldierInPoint = firePlace2Soldier[observationPlaces[j][k]] )
			{
				if ( IsBetterChangeObservationSoldier( pFiringSoldier, pSoldierInPoint ) )
				{
					const int nFiringSoldierSlot = pFiringSoldier->GetSlot();
					DelSoldierFromFirePlace( pFiringSoldier );
					DelSoldierFromFirePlace( pSoldierInPoint );

					PushSoldierToFirePlace( pFiringSoldier, observationPlaces[j][k] );
					PushSoldierToFirePlace( pSoldierInPoint, nFiringSoldierSlot );

					return true;
				}
			}
		}
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::PushSoldierToFirePlace( CSoldier *pUnit, const int nFirePlace )
{
	pUnit->SetToFirePlace();
	
	pUnit->SetSlotInfo( -1, 0, fire.Size() );
	const int nIndex = fire.Push( pUnit );
	SetFiringUnitProperties( pUnit, nFirePlace, nIndex );

	firePlace2Soldier[nFirePlace] = pUnit;

	// ��� - observation point
	if ( firePlace2Observation[nFirePlace] != -1 )
	{
		// ��������� ���������� ������ � observation point �� �������
		const int nSide = firePlace2Observation[nFirePlace] & 3;
		++sides[nSide].nSoldiersInObservationPoints;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::PushSoldierToObservationPoint( CSoldier *pSoldier, const int nSide )
{
	if ( sides[nSide].nObservationPoints != 0  )
	{
		// ��� ����� �� �����
		if ( sides[nSide].nSoldiersInObservationPoints == 0 )
		{
			const int nFirePlace = GetMiddleObservationPoint( nSide );
			PushSoldierToFirePlace( pSoldier, nFirePlace );
		}
		// ����� ������ ����
		else if ( sides[nSide].nSoldiersInObservationPoints == 1 && sides[nSide].nObservationPoints > 1 )
		{
			int nLeftPoint, nRightPoint;
			GetSidesObservationPoints( nSide, &nLeftPoint, &nRightPoint );

			CSoldier *pMiddleSoldier = GetSoldierOnSide( nSide );
			DelSoldierFromFirePlace( pMiddleSoldier );

			PushSoldierToFirePlace( pSoldier, nLeftPoint );
			PushSoldierToFirePlace( pMiddleSoldier, nRightPoint );
		}
		else
			NI_ASSERT_T( false, "Can't push soldier to observation point" );
	}
	else
		NI_ASSERT_T( false, "Can't push soldier to observation point" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::IsSoldierInObservationPoint( CSoldier *pSoldier ) const
{
	if ( !pSoldier->IsInFirePlace() )
		return false;
	else
	{
		const int nFireSlot = pSoldier->GetSlot();
		return firePlace2Observation[nFireSlot] != -1;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::SetSoldiersToObservationPoints()
{
	for ( int i = 0; i < rest.Size();	++i )
	{
		CSoldier *pRestingSoldier = rest[i];		
		const int nSide = ChooseSideToSetSoldier( pRestingSoldier );
		// �� ���� ������ ���������� ���� �������
		if ( nSide == -1 || fire.Size() == fire.GetReserved() )
		{
			if ( TryToPushRestSoldierToObservation( pRestingSoldier ) )
				return;
		}
		else
		{
			rest[i] = 0;
			rest.Erase( i );
			PushSoldierToObservationPoint( pRestingSoldier, nSide );
			return;
		}
	}

	// ��������� ������ �� � ������ ����������
	for ( int i = 0; i < fire.Size(); ++i )
	{
		CSoldier *pSoldier = fire[i];
		if ( !IsSoldierInObservationPoint( pSoldier ) )
		{
			const int nSide = ChooseSideToSetSoldier( pSoldier );
			if ( nSide == -1 )
			{
				if ( TryToPushFireSoldierToObservation( pSoldier ) )
					return;
			}
			else
			{
				DelSoldierFromFirePlace( pSoldier );
				PushSoldierToObservationPoint( pSoldier, nSide );
				return;
			}
		}
	}

	// ��������� ������ � ������ ����������
	int nIndexToTry = -1;
	int nSoldierInTrySide = -1;
	for ( int i = 0; i < fire.Size(); ++i )
	{
		if ( IsSoldierInObservationPoint( fire[i] ) )
		{
			const int nSide = firePlace2Observation[fire[i]->GetSlot()] & 3;
			if ( nIndexToTry == -1 || nSoldierInTrySide < sides[nSide].nSoldiersInObservationPoints )
			{
				nIndexToTry = i;
				nSoldierInTrySide = sides[nSide].nSoldiersInObservationPoints;
			}
		}
	}

	if ( nIndexToTry != -1 )
	{
		CSoldier *pSoldier = fire[nIndexToTry];

		const int nSide = ChooseSideToSetSoldier( pSoldier );
		if ( nSide == -1 )
			TryToPushFireSoldierToObservation( pSoldier );
		else
		{
			DelSoldierFromFirePlace( pSoldier );
			PushSoldierToObservationPoint( pSoldier, nSide );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::DistributeNonFires()
{
	// �������� ���, ��� ��������� � medical � �������� � rest
	while ( !medical.IsEmpty() && medical.GetMaxEl()->GetHitPoints() == medical.GetMaxEl()->GetStats()->fMaxHP && IsIllInRest() )
	{
		CPtr<CSoldier> pMedicalSoldier = medical.GetMaxEl();
		medical.Pop();
		CPtr<CSoldier> pRestSoldier = rest.GetMaxEl();
		rest.Pop();

		PushToMedical( pRestSoldier ); 
		PushToRest( pMedicalSoldier );
	}

	// ������� ���, ��� ���������
	while ( rest.Size() < rest.GetReserved() && !medical.IsEmpty() && medical.GetMaxEl()->GetHitPoints() == medical.GetMaxEl()->GetStats()->fMaxHP )
	{
		CSoldier *pSoldier = medical.GetMaxEl();
		medical.Pop();
		PushToRest( pSoldier );
	}

	// ������� ������� �� rest ��������
	while ( IsIllInRest() && medical.Size() < medical.GetReserved() )
	{
		CSoldier *pSoldier = rest.GetMaxEl();
		rest.Pop();
		PushToMedical( pSoldier );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::DistributeFiringSoldiers()
{
	// �� ��� fireslots ������
	if ( fire.Size() < fire.GetReserved() )
	{
		for ( int i = 0; i < fire.Size(); ++i )
		{
			// �� ��������
			if ( fire[i]->GetState()->GetName() == EUSN_REST_IN_BUILDING )
			{
				CSoldier *pSoldier = fire[i];

				bool bShouldWatch = true;
				for ( int j = 0; j < 4; ++j )
					bShouldWatch = bShouldWatch && ( sides[j].nSoldiersInObservationPoints == 0 );

				if ( bShouldWatch )
				{
					const int nSide = ChooseSideToSetSoldier( pSoldier );
					NI_ASSERT_T( nSide != -1, "Wrong side chosen" );

					DelSoldierFromFirePlace( pSoldier );
					PushSoldierToObservationPoint( pSoldier, nSide );
				}
				else
				{
					nLastFreeFireSoldierChoice += Random( 0, pStats->slots.size() );
					const int nNewFireSlot = GetFreeFireSlot();

					DelSoldierFromFirePlace( pSoldier );
					PushSoldierToFirePlace( pSoldier, nNewFireSlot );
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::ExchangeSoldiersToTurrets()
{
	for ( int i = 0; i < turrets.size(); ++i )
	{
		if ( turrets[i] != 0 && firePlace2Soldier[i] == 0 )
		{
			int j = 0;
			while ( j < firePlace2Soldier.size() && ( firePlace2Soldier[j] == 0 || turrets[j] == 0 ) )
				++j;

			if ( j < firePlace2Soldier.size() )
			{
				CPtr<CSoldier> pSoldier = firePlace2Soldier[j];
				NI_ASSERT_T( j == pSoldier->GetSlotInfo().nSlot, "Wrong soldier slot info" );
				DelSoldierFromFirePlace( pSoldier );
				PushSoldierToFirePlace( pSoldier, i );
			}
			else
				return;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::Segment()
{
	nextSegmTime = curTime + Random( 2 * SConsts::AI_SEGMENT_DURATION - 1, 10 * SConsts::AI_SEGMENT_DURATION );

	if ( !IsUnitsInside() )
	{
		if ( SBuildingRPGStats::TYPE_MAIN_RU_STORAGE != pStats->eType && SBuildingRPGStats::TYPE_TEMP_RU_STORAGE != pStats->eType )
			theStatObjs.UnregisterSegment( this );

		bShouldEscape = false;
		bEscaped = false;
		timeOfDeath = 0;
	}
	// �� ������ ������
	else if ( timeOfDeath != 0 && timeOfDeath + 2000 < curTime )
	{
		KillAllInsiders();
		timeOfDeath = 0;
	}
	else if ( !CStormableObject::Segment() )
	{
		// ��������
		const float fInc = SConsts::AI_SEGMENT_DURATION * SConsts::CURE_SPEED_IN_BUILDING;
		for ( int i = 0; i < medical.Size(); ++i )
		{
			const float fWantedInc = Min( fInc, medical[i]->GetStats()->fMaxHP - medical[i]->GetHitPoints() );
			medical[i]->IncreaseHitPoints( fWantedInc );
		}

		// ���������� alarms
		if ( bAlarm )
		{
			while ( !fire.Size() == fire.GetReserved() && ( !medical.IsEmpty() || !rest.IsEmpty() ) )
				SeatSoldierToFireSlot();

			bAlarm = false;
			startOfRest = curTime;
		}
		else
		{
			for ( int i = 0; i < fire.Size(); ++i )
			{
				if ( fire[i]->GetState()->GetName() != EUSN_REST_IN_BUILDING )
				{
					startOfRest = curTime;
					break;
				}
			}
		}

		if ( curTime - lastDistibution >= 4000 || curTime - startOfRest < SConsts::TIME_OF_BUILDING_ALARM && curTime - lastDistibution >= 3500 )
		{
			lastDistibution = curTime;			

			// �������
			if ( curTime - startOfRest < SConsts::TIME_OF_BUILDING_ALARM )
				DistributeFiringSoldiers();

			// ���� ����� ��������
			if ( curTime - startOfRest >= SConsts::TIME_OF_BUILDING_ALARM )
				DistributeAll();
			else
				// ������ �� ���������� ����� ��������
				DistributeNonFires();
		}

		// ������� ���� ���������� � fireplaces
		while ( !rest.IsEmpty() && fire.Size() < fire.GetReserved() )
		{
			CSoldier *pSoldier = rest.GetMaxEl();
			rest.Pop();
			PushToFire( pSoldier );
		}

		if ( pLockingUnit && ( !pLockingUnit->IsValid() && !pLockingUnit->IsAlive() ) )
			Unlock( pLockingUnit );
	}

	ExchangeSoldiersToTurrets();

	// �������� � turrets
	for ( int i = 0; i < fire.Size(); ++i )
	{
		if ( IsValidObj( fire[i] ) )
		{
			if ( fire[i]->GetSlot() >= 0 && fire[i]->GetSlot() < turrets.size() && turrets[fire[i]->GetSlot()] != 0 )
				turrets[fire[i]->GetSlot()]->Segment();
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::KillAllInsiders()
{
	std::list< CPtr<CSoldier> > dead;

	for ( int i = 0; i < medical.Size(); ++i )
		dead.push_back( medical[i] );

	for ( int i = 0; i < rest.Size(); ++i )
		dead.push_back( rest[i] );

	for ( int i = 0; i < fire.Size(); ++i )
		dead.push_back( fire[i] );

	for ( std::list< CPtr<CSoldier> >::iterator iter = dead.begin(); iter != dead.end(); ++iter )
		(*iter)->Die( false, 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::Die( const float fDamage )
{
	if ( !bEscaped )
		KillAllInsiders();

	timeOfDeath = curTime;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::SetHitPoints( const float fNewHP ) 
{ 
	if ( fNewHP != fHP )
	{
/*		
		if ( fHP <= 0.0f )
		{
			if ( nScriptID != -1 )
			{
				pScripts->DelInvalidUnits( nScriptID );
				pScripts->AddObjToScriptGroup( this, nScriptID );
			}
		}
*/
		
		fHP = Min( fNewHP, GetStats()->fMaxHP );
		updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );

		if ( fHP > GetEscapeHitPoints() )
			bShouldEscape = true;

		if ( fHP > 0.0f )
			timeOfDeath = 0.0f;
	}	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::DriveOut( CSoldier *pSoldier, std::unordered_set<int> *pFormations )
{
	CFormation *pFormation = pSoldier->GetFormation();
	const int nFormationID = pSoldier->GetFormation()->GetID();
	if ( pFormations->find( nFormationID ) == pFormations->end() )
	{
		pFormations->insert( nFormationID );
		const int nEntrancePoint = Random( 0, GetNEntrancePoints() - 1 );
		const CVec2 vEntrancePoint( GetEntrancePoint( nEntrancePoint ) );
		CVec2 vDirFromCenter( vEntrancePoint - GetCenter() );
		Normalize( &vDirFromCenter );
		const CVec2 vPointsToGo( vEntrancePoint + vDirFromCenter * SAIConsts::TILE_SIZE * 3.0f );

		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_LEAVE, vPointsToGo ), pFormation, false );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit )
{
	if ( fHP > 0 )
	{
		Alarm();

		if ( bFromExplosion )
		{
			fHP = Max( fHP - fDamage, 0.0f );
			if ( theCheats.GetFirstShoot( theDipl.GetNParty( nPlayerOfShoot ) ) == 1 )
				fHP = 0.0f;
			
			bEscaped = GetHitPoints() <= GetEscapeHitPoints() && bShouldEscape;
			if ( bEscaped )
			{
				std::unordered_set<int> formations;
				for ( int i = 0; i < fire.Size(); ++i )
					DriveOut( fire[i], &formations );
				for ( int i = 0; i < medical.Size(); ++i )
					DriveOut( medical[i], &formations );
				for ( int i = 0; i < rest.Size(); ++i )
					DriveOut( rest[i], &formations );
			}
			
			// ��� �����
			if ( GetHitPoints() == 0.0f )
			{
				// ��������� �� �������
				if ( SBuildingRPGStats::TYPE_MAIN_RU_STORAGE != pStats->eType &&
						 SBuildingRPGStats::TYPE_TEMP_RU_STORAGE != pStats->eType )
				{
					theStatistics.ObjectDestroyed( nPlayerOfShoot );

					if ( !bEscaped )
					{
						for ( int i = 0; i < fire.Size(); ++i )
						{
							if ( pShotUnit )
								pShotUnit->EnemyKilled( fire[i] );

							theStatistics.UnitKilled( nPlayerOfShoot, fire[i]->GetPlayer(), 1, fire[i]->GetStats()->fPrice );
							theMPInfo.UnitsKilled( nPlayerOfShoot, fire[i]->GetStats()->fPrice, fire[i]->GetPlayer() );
						}
						for ( int i = 0; i < medical.Size(); ++i )
						{
							if ( pShotUnit )
								pShotUnit->EnemyKilled( medical[i] );

							theStatistics.UnitKilled( nPlayerOfShoot, medical[i]->GetPlayer(), 1, medical[i]->GetStats()->fPrice );
							theMPInfo.UnitsKilled( nPlayerOfShoot, medical[i]->GetStats()->fPrice, medical[i]->GetPlayer() );
						}
						for ( int i = 0; i < rest.Size(); ++i )
						{
							if ( pShotUnit )
								pShotUnit->EnemyKilled( rest[i] );

							theStatistics.UnitKilled( nPlayerOfShoot, rest[i]->GetPlayer(), 1, rest[i]->GetStats()->fPrice );
							theMPInfo.UnitsKilled( nPlayerOfShoot, rest[i]->GetStats()->fPrice, rest[i]->GetPlayer() );
						}
					}
				}

				Die( fDamage );
			}
			else
			{
				if ( !bEscaped )
				{
					const float fProbability = fDamage / pStats->fMaxHP;

					std::list< CPtr<CSoldier> > dead;								
					for ( int i = 0; i < medical.Size(); ++i )
					{
						// �� �����
						if ( Random( 0.0f, 1.0f ) < fProbability )
							dead.push_back( medical[i] );
					}

					for ( int i = 0; i < rest.Size(); ++i )
					{
						// �� �����
						if ( Random( 0.0f, 1.0f ) < fProbability )
							dead.push_back( rest[i] );
					}
						
					for ( int i = 0; i < fire.Size(); ++i )
					{
						// �� �����
						if ( Random( 0.0f, 1.0f ) < fProbability )
							dead.push_back( fire[i] );
					}

					for ( std::list< CPtr<CSoldier> >::iterator iter = dead.begin(); iter != dead.end(); ++iter )
					{
						CSoldier *pSoldier = *iter;

						if ( pShotUnit )
							pShotUnit->EnemyKilled( pSoldier );
						
						theStatistics.UnitKilled( nPlayerOfShoot, dead.front()->GetPlayer(), 1, pSoldier->GetStats()->fPrice );
						theMPInfo.UnitsKilled( nPlayerOfShoot, pSoldier->GetStats()->fPrice, pSoldier->GetPlayer() );

						pSoldier->Die( false, 0 );
					}
				}

				WasHit();
			}

			updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::Alarm()
{
	bAlarm = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CBuilding::GetNFreePlaces() const 
{ 
	if ( GetHitPoints() > 0 )
		return nOveralPlaces - medical.Size() - fire.Size() - rest.Size();

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::IsGoodPointForRunIn( const SVector &point, const int nEntrance, const float fMinDist ) const
{
	return fabs2( point.ToCVec2() - GetEntrancePoint( nEntrance ) ) <= sqr( float( SConsts::TILE_SIZE ) + fMinDist );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CBuilding::GetNDefenders() const
{
	return medical.Size() + rest.Size() + fire.Size();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSoldier* CBuilding::GetUnit( const int n ) const
{
	NI_ASSERT_T( n < GetNDefenders(), "Wrong number of unit to get from defenders of a building" );

	if ( n < fire.Size() )
		return fire[n];
	else if ( n < fire.Size() + rest.Size() )
		return rest[n - fire.Size()];
	return medical[n - fire.Size() - rest.Size()];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const BYTE CBuilding::GetPlayer() const
{
	if ( !fire.IsEmpty() )
		return fire[0]->GetPlayer();
	else if ( !rest.IsEmpty() )
		return rest[0]->GetPlayer();
	else if ( !medical.IsEmpty() )
		return medical[0]->GetPlayer();

	return theDipl.GetNeutralPlayer();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::Lock( CCommonUnit *pUnit )
{
	pLockingUnit = pUnit;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::IsLocked( const int nPlayer ) const
{
	return pLockingUnit != 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::Unlock( CCommonUnit *pUnit )
{
	pLockingUnit = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CBuilding::GetNGunsInFireSlot( const int nSlot )
{
	NI_ASSERT_T( nSlot < guns.size(), NStr::Format( "Wrong number of slot (%d)", nSlot ) );

	return guns[nSlot]->GetNTotalGuns();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBasicGun* CBuilding::GetGunInFireSlot( const int nSlot, const int nGun )
{
	NI_ASSERT_T( nSlot < guns.size(), NStr::Format( "Wrong number of slot (%d)", nSlot ) );

	if ( nGun >= guns[nSlot]->GetNTotalGuns() )
		return 0;

	return guns[nSlot]->GetGun(nGun);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTurret* CBuilding::GetTurretInFireSlot( const int nSlot )
{
	NI_ASSERT_T( nSlot < guns.size(), NStr::Format( "Wrong number of slot (%d)", nSlot ) );
	return turrets[nSlot];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CBuilding::GetMaxFireRangeInSlot( const int nSlot ) const
{
	NI_ASSERT_T( nSlot < guns.size(), NStr::Format( "Wrong number of slot (%d)", nSlot ) );
	return guns[nSlot]->GetMaxFireRange( 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::IsSoldierVisible( const int nParty, const CVec2 &center, bool bCamouflated, const float fCamouflage ) const
{
	if ( nParty == theDipl.GetNParty( GetPlayer() ) )
		return true;

	for ( int i = 0; i < theDipl.GetNPlayers(); ++i )
	{
		if ( theDipl.GetDiplStatusForParties( nParty, theDipl.GetNParty( i ) ) == EDI_FRIEND && 
				 GetNFriendlyAttackers( i ) > 0 )
			return true;
	}

	const CVec2 vOrigin( pStats->GetOrigin( GetFrameIndex() ) );
	const int nDownX = GetDownX();
	const int nDownY = GetDownY();

	const CArray2D<BYTE> &passability = pStats->GetPassability( GetFrameIndex() );
	const int nSizeX = passability.GetSizeX();
	const int nSizeY = passability.GetSizeY();

	const int nYShift = ( center.y - nDownY ) / float(SConsts::TILE_SIZE);
	const int nXShift = ( center.x - nDownX ) / float(SConsts::TILE_SIZE);

	const int nYDown = ( nYShift <= nSizeY / 2 ) ? 0 : nSizeY / 2;
	const int nXDown = ( nXShift <= nSizeX / 2 ) ? 0 : nSizeX / 2;
	const int nYUp	 = ( nYShift <= nSizeY / 2 ) ? nSizeY / 2 + 1 : nSizeY;
	const int nXUp   = ( nXShift <= nSizeX / 2 ) ? nSizeX / 2 + 1 : nSizeX;

	for ( int y = nYDown; y < nYUp; ++y )
	{
		for ( int x = nXDown; x < nXUp; ++x )
		{
			if ( passability[y][x] == 1 ) 
			{
				const SVector tile( AICellsTiles::GetTile( nDownX + SConsts::TILE_SIZE*x, nDownY + SConsts::TILE_SIZE*y ) );
				if ( theWarFog.IsUnitVisible( nParty, tile, bCamouflated, fCamouflage ) )
					return true;
			}
		}
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::ChooseEntrance( class CCommonUnit *pUnit, const CVec2 &vPoint, int *pnEntrance ) const
{
	int nMinDistance = 1000000;
	*pnEntrance = -1;

	for ( int i = 0; i < GetNEntrancePoints(); ++i )
	{
		CPtr<IStaticPath> pPath = CreateStaticPathToPoint( GetEntrancePoint( i ), vPoint, VNULL2, pUnit, true );
		if ( pPath != 0 && ( *pnEntrance == -1 || pPath->GetLength() < nMinDistance )/* && fabs2( ( pPath->GetFinishPoint() - vPoint ) ) <= sqr( 2 * SConsts::GOOD_LAND_DIST )*/ )
		{
			nMinDistance = pPath->GetLength();
			*pnEntrance = i;
		}
	}

	if ( *pnEntrance == -1 && GetNEntrancePoints() > 0 )
		*pnEntrance = 0;

	return ( *pnEntrance != -1 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::IsSelectable() const
{
	return theDipl.GetNParty( GetPlayer() ) == theDipl.GetMyParty();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::CanUnitGoThrough( const EAIClass &eClass ) const
{
	return ( pStats->dwAIClasses & eClass ) == 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::CanRotateSoldier( CSoldier *pSoldier ) const
{
/*	
	// ��������
	if ( pSoldier->GetState() && IsRestState( pSoldier->GetState()->GetName() ) )
	{
		// ������ � fireplace
		// ��� �� � fireplace, �� �� ����� �������, ��� �� �������, ��� �������, �� ��� ��������� 
		if ( pSoldier->IsInFirePlace() || 
				 pSoldier->IsInSolidPlace() && 
				 ( bAlarm || pSoldier->GetSoliderPlaceParameter() != 1 || pSoldier->GetHitPoints() == pSoldier->GetStats()->fMaxHP ) )
			return true;
	}
*/
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::ExchangeUnitToFireplace( CSoldier *pSoldier, int nFirePlace )
{
	CSoldier *pDeletedSoldier = GetSoldierInFireplace( nFirePlace );
	if ( pDeletedSoldier )
		DelSoldier( pDeletedSoldier, false );

	DelSoldier( pSoldier, false );

	PushSoldierToFirePlace( pSoldier, nFirePlace );

	if ( pDeletedSoldier )
		AddSoldier( pDeletedSoldier );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CBuilding::GetNFirePlaces() const
{
	return pStats->slots.size();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoldier* CBuilding::GetSoldierInFireplace( const int nFireplace) const
{
	return firePlace2Soldier[nFireplace];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::SSwapAction::operator()( CPtr<CSoldier> pSoldier1, CPtr<CSoldier> pSoldier2, const int nSoldier1Index, const int nSoldier2Index )
{
	if ( pSoldier1 )
		pSoldier1->SetSlotIndex( nSoldier2Index );
	if ( pSoldier2 )
		pSoldier2->SetSlotIndex( nSoldier1Index );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::IsBetterChangeObservationSoldier( CSoldier *pSoldier, CSoldier *pSoldierInPoint )
{
	return pSoldier->GetSightRadius() > pSoldierInPoint->GetSightRadius();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoldier* CBuilding::GetSoldierOnSide( const int nSide )
{
	NI_ASSERT_T( nSide < 4, NStr::Format( "Wrong side passed (%d)", nSide ) );
	for ( int i = 0; i < sides[nSide].nObservationPoints; ++i )
	{
		if ( firePlace2Soldier[observationPlaces[nSide][i]] != 0 )
			return firePlace2Soldier[observationPlaces[nSide][i]];
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CBuilding::GetMiddleObservationPoint( const int nSide ) const
{
	NI_ASSERT_T( nSide < 4, NStr::Format( "Wrong side passed (%d)", nSide ) );

	if ( sides[nSide].nObservationPoints == 0 )
		return -1;
	else if ( sides[nSide].nObservationPoints < 3 )
		return observationPlaces[nSide][0];
	else
		return observationPlaces[nSide][1];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::GetSidesObservationPoints( const int nSide, int *pnLeftPoint, int *pnRightPoint ) const
{
	NI_ASSERT_T( nSide < 4, NStr::Format( "Wrong side passed (%d)", nSide ) );

	if ( sides[nSide].nObservationPoints == 0 )
		*pnLeftPoint = *pnRightPoint = -1;
	else if ( sides[nSide].nObservationPoints == 1 )
		*pnLeftPoint = *pnRightPoint = observationPlaces[nSide][0];
	else if ( sides[nSide].nObservationPoints == 2 )
	{
		*pnRightPoint = observationPlaces[nSide][0];
		*pnLeftPoint = observationPlaces[nSide][1];
	}
	else if ( sides[nSide].nObservationPoints == 3 )
	{
		*pnRightPoint = observationPlaces[nSide][0];
		*pnLeftPoint = observationPlaces[nSide][2];
	}
	else
		NI_ASSERT_T( false, NStr::Format( "Wrong number of observation points (%d)", sides[nSide].nObservationPoints ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CBuilding::ChooseSideToSetSoldier( CSoldier *pSoldier ) const
{
	int nSoldierSide = -1;
	if ( pSoldier->IsInFirePlace() )
	{
		const int nSoldierSlot = pSoldier->GetSlot();
		if ( firePlace2Observation[nSoldierSlot] != -1 )
			nSoldierSide = firePlace2Observation[nSoldierSlot] & 3;
	}
	
	int nBestSide = -1;
	int nBestSideSoldiers = -1;
	for ( int i = 0; i < 4; ++i )
	{
		int nSoldiersInObservationPoints = sides[i].nSoldiersInObservationPoints;
		if ( i == nSoldierSide )
			--nSoldiersInObservationPoints;
			
		if ( sides[i].nObservationPoints > 0 && nSoldiersInObservationPoints < 2 && nSoldiersInObservationPoints < sides[i].nObservationPoints )
		{
			if ( nBestSide == -1 )
			{
				nBestSide = i;
				nBestSideSoldiers = nSoldiersInObservationPoints;
			}
			else if ( nSoldiersInObservationPoints < nBestSideSoldiers )
			{
				nBestSide = i;
				nBestSideSoldiers = nSoldiersInObservationPoints;
			}
			else if ( nSoldiersInObservationPoints == nBestSideSoldiers && Random( 0.0f, 1.0f ) < 0.5f )
			{
				nBestSide = i;
				nBestSideSoldiers = nSoldiersInObservationPoints;
			}
		}
	}

	return nBestSide;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CBuilding::IsVisibleForDiplomacyUpdate()
{ 
	// this is storage
	if ( SBuildingRPGStats::TYPE_TEMP_RU_STORAGE == pStats->eType )
		return true;
	// �� ��������/��������
	if ( theDipl.GetNParty( nLastPlayer ) == theDipl.GetMyParty() ||
				theDipl.GetNParty( GetPlayer() ) == theDipl.GetMyParty())
		return true;
	// ���� �������
	else if ( GetPlayer() != theDipl.GetNeutralPlayer() )
		return IsAnyInsiderVisible();
	// ���� �������
	else
		return IsVisible( theDipl.GetMyParty() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::SetLastLeaveTime( const int nPlayer )
{
	lastLeave[nPlayer] = curTime;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
