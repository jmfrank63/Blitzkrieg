#include "stdafx.h"
#include <new>

#include "Updater.h"
#include "UpdatableObject.h"
#include "Entrenchment.h"
#include "AIStaticMap.h"
#include "Diplomacy.h"
#include "Soldier.h"
#include "Formation.h"
#include "SuspendedUpdates.h"
#include "AILogicInternal.h"
extern CSuspendedUpdates theSuspendedUpdates;
static const int TEMP_BUFFER_INDEX_SHOOT_AREAS = 1;
static int g_nConstructedShootAreasInTempBuffer = 0;
static SShootAreas* GetShootAreasTempBuffer( const int nCount )
{
	SShootAreas *pBuffer = GetTempBufferN<SShootAreas>( nCount, TEMP_BUFFER_INDEX_SHOOT_AREAS );
	for ( int i = 0; i < g_nConstructedShootAreasInTempBuffer; ++i )
		pBuffer[i].~SShootAreas();
	g_nConstructedShootAreasInTempBuffer = 0;
	return pBuffer;
}
extern NTimer::STime curTime;
extern CStaticMap theStaticMap;
extern CDiplomacy theDipl;
CUpdater updater;
extern CAILogic *pAILogic;
const int CUpdater::nUpdateTypes = 200;
CUpdater::CUpdater()
{
	simpleUpdates.clear(); simpleUpdates.resize( nUpdateTypes );
	complexUpdates.clear(); complexUpdates.resize( nUpdateTypes );
	feedBacks.clear();
	unitAnimation.clear();
	updatedPlacements.clear();
	bPlacementsUpdated = false;
	nShootAreasGroup = -1;

	bDestroying = false;
	bGameFinishUpdateSend = false;
}
CUpdater::~CUpdater()
{
	bDestroying = true;
}
void CUpdater::DestroyContents()
{
	CUpdater::~CUpdater();
	new(this) CUpdater();
}
void CUpdater::Init()
{
	pGameTimer = GetSingleton<IGameTimer>();
	pGameSegment = GetSingleton<IGameTimer>()->GetGameSegmentTimer();

	bPlacementsUpdated = false;
	nShootAreasGroup = -1;
	bGameFinishUpdateSend = false;
}
void CUpdater::AddUpdate( const EActionNotify updateType, IUpdatableObj *pObj, const int nParam )
{
	if ( updateType & 1 )
	{
		const SSimpleUpdate update( pObj, nParam );
		simpleUpdates[updateType >> 4][pObj->GetUniqueId()] = update;
	}
	else
		complexUpdates[updateType >> 4][pObj->GetUniqueId()] = pObj;
}
void CUpdater::Update( const EActionNotify updateType, IUpdatableObj *pObj, const int nParam )
{
	if ( !bDestroying )
	{
		NI_ASSERT_T( ( updateType & 1 ) || nParam == -1, "Complex update with nParam" );

		if ( updateType == ACTION_NOTIFY_NEW_ST_OBJ || updateType == ACTION_NOTIFY_DELETED_ST_OBJ )
		{
			if ( complexUpdates[updateType >> 4].find( pObj->GetUniqueId() ) == complexUpdates[updateType >> 4].end() )
			{
				const EActionNotify eReverseUpdate = (updateType == ACTION_NOTIFY_NEW_ST_OBJ) ? ACTION_NOTIFY_DELETED_ST_OBJ : ACTION_NOTIFY_NEW_ST_OBJ;
				if ( complexUpdates[eReverseUpdate >> 4].find( pObj->GetUniqueId() ) != complexUpdates[eReverseUpdate >> 4].end() )
				{
					complexUpdates[eReverseUpdate >> 4].erase( pObj->GetUniqueId() );
					return;
				}
			}
		}

		const int nAnimation = GetAnimationFromAction( updateType );
		const bool bAnimation = ( (updateType & 1) == 1 && nAnimation != -1 );

		if ( nAnimation != -1 )
			pObj->AnimationSet( nAnimation );

		if ( updateType == ACTION_NOTIFY_MECH_SHOOT )
		{
			SAINotifyMechShot shot;
			pObj->GetMechShotInfo( &shot, curTime );
			const int nAnimation = GetAnimationFromAction( shot.typeID );
			if ( nAnimation != -1 )
				checked_cast<IUpdatableObj*>(shot.pObj)->AnimationSet( nAnimation );
		}
		else if ( updateType == ACTION_NOTIFY_INFANTRY_SHOOT )
		{
			SAINotifyInfantryShot shot;
			pObj->GetInfantryShotInfo( &shot, curTime );
			const int nAnimation = GetAnimationFromAction( shot.typeID );
			if ( nAnimation != -1 )
			{
				CSoldier *pSoldier = checked_cast<CSoldier*>(shot.pObj);
				if ( pSoldier )
				{
					pSoldier->AnimationSet( nAnimation );
					if ( !pSoldier->IsInSolidPlace() )
						unitAnimation[pSoldier->GetUniqueId()] = shot.typeID;
				}
			}
		}

		if ( bAnimation )
		{
			if ( pObj->IsAlive() || IsDeathAnimation( GetAnimationFromAction( updateType ) ) )
				unitAnimation[pObj->GetUniqueId()] = updateType;
		}

		if ( updateType != ACTION_NOTIFY_NONE && ( bAnimation && DoWeNeedAction( updateType ) || !bAnimation ) )
			AddUpdate( updateType, pObj, nParam );

		if ( updateType != ACTION_NOTIFY_NONE )
			garbage.insert( CComplexUpdatesSet::value_type( pObj->GetUniqueId(), pObj ) );

		if ( updateType == ACTION_NOTIFY_PLACEMENT )
			updatedPlacements.insert( CComplexUpdatesSet::value_type( pObj->GetUniqueId(), pObj ) );
	}
}
void CUpdater::EndUpdates()
{
	garbage.clear();
}
void CUpdater::DelUpdate( const EActionNotify updateType, IUpdatableObj *pObj )
{
	if ( updateType & 1 )
		simpleUpdates[updateType >> 4].erase( pObj->GetUniqueId() );
	else
		complexUpdates[updateType >> 4].erase( pObj->GetUniqueId() );
}
void CUpdater::DelActionUpdates( IUpdatableObj *pObj )
{
	for ( int i = 0; i < simpleUpdates.size(); ++i )
	{
		if ( !IsDyingAction( EActionNotify( ( i << 4 ) | 1 ) ) )
			DelUpdate( EActionNotify( ( i << 4 ) | 1 ), pObj );
	}
}
void CUpdater::ClearAllUpdates( const EActionNotify updateType )
{
	if ( updateType & 1 )
	{
		for ( CSimpleUpdatesSet::iterator iter = simpleUpdates[updateType >> 4].begin(); iter != simpleUpdates[updateType >> 4].end(); ++iter )
			garbage.insert( CComplexUpdatesSet::value_type( iter->first, iter->second.pObj ) );
	}
	else
	{
		for ( CComplexUpdatesSet::iterator iter = complexUpdates[updateType >> 4].begin(); iter != complexUpdates[updateType >> 4].end(); ++iter )
			garbage.insert( *iter );
	}

	if ( updateType == ACTION_NOTIFY_PLACEMENT )
	{
		if ( bPlacementsUpdated )
		{
			complexUpdates[ACTION_NOTIFY_PLACEMENT >> 4].clear();
			bPlacementsUpdated = false;
		}
	}
	else
	{
		if ( updateType & 1 )
			simpleUpdates[updateType >> 4].clear();
		else
			complexUpdates[updateType >> 4].clear();
	}
}
void CUpdater::ClearAllUpdates()
{
	for ( int i = 0; i < simpleUpdates.size(); ++i )
		simpleUpdates[i].clear();
	for ( int i = 0; i < complexUpdates.size(); ++i )
		complexUpdates[i].clear();

	unitAnimation.clear();
}
template<class T>
void AddRecalled( const EActionNotify &eAction, T *pBuffer, int *pnLen )
{
	while ( !theSuspendedUpdates.IsRecalledEmpty( eAction ) )
	{
		::new ( &pBuffer[(*pnLen)] ) T();
		theSuspendedUpdates.GetRecalled( eAction, &pBuffer[(*pnLen)] );

		++(*pnLen);
	}
}
template<>
void AddRecalled( const EActionNotify &eAction, SNewUnitInfo *pObjects, int *pnLen )
{
	while ( !theSuspendedUpdates.IsRecalledEmpty( eAction ) )
	{
		::new ( &pObjects[(*pnLen)] ) SNewUnitInfo();
		theSuspendedUpdates.GetRecalled( eAction, &pObjects[(*pnLen)] );

		if ( pObjects[(*pnLen)].nFrameIndex != -2 )
			pObjects[(*pnLen)].dbID = checked_cast<IUpdatableObj*>(pObjects[(*pnLen)].pObj)->GetDBID();

		++(*pnLen);
	}
}
void CUpdater::UpdateActions( SAINotifyAction **pActionsBuffer, int *pnLen )
{
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		const NTimer::STime time = GetAIGetSegmTime( pGameSegment );
		const NTimer::STime curTime = pGameSegment->Get();

		int nTotalSize = 0;
		for ( int i = 0; i < simpleUpdates.size(); ++i )
		{
			nTotalSize += simpleUpdates[i].size();
			nTotalSize += theSuspendedUpdates.GetNRecalled( EActionNotify( (i<<4)|1 ) );
		}

		nTotalSize += 
			complexUpdates[ACTION_NOTIFY_DEAD_UNIT >> 4].size() + 
			theSuspendedUpdates.GetNRecalled( ACTION_NOTIFY_DEAD_UNIT ) +
			unitAnimation.size();

		*pnLen = 0;
		*pActionsBuffer = GetTempBuffer<SAINotifyAction>( nTotalSize );

		for ( int i = 0; i < simpleUpdates.size(); ++i )
		{
			EActionNotify eAction = EActionNotify( ( i << 4 ) | 1 );

			AddRecalled( eAction, *pActionsBuffer, pnLen );
			for ( CSimpleUpdatesSet::iterator iter = simpleUpdates[i].begin(); iter != simpleUpdates[i].end(); ++iter )
			{
				::new ( &(*pActionsBuffer)[(*pnLen)] ) SAINotifyAction();

				SSimpleUpdate &update = iter->second;
	
				(*pActionsBuffer)[*pnLen].time = curTime;
				(*pActionsBuffer)[*pnLen].typeID = eAction;
				(*pActionsBuffer)[*pnLen].pObj = update.pObj;

				if ( eAction == ACTION_NOTIFY_SERVED_ARTILLERY || eAction == ACTION_NOTIFY_SELECT_CHECKED ||
						 eAction == ACTION_SET_SELECTION_GROUP )
				{
					if ( update.nParam == -1 )
						(*pActionsBuffer)[*pnLen].nParam = 0;
					else
						(*pActionsBuffer)[*pnLen].nParam = reinterpret_cast<int>( CLinkObject::GetObjectByUniqueIdSafe( update.nParam ) );
				}
				(*pActionsBuffer)[*pnLen].nParam = update.nParam;
				
				if ( !theSuspendedUpdates.CheckToSuspend( eAction, update.pObj, (*pActionsBuffer)[(*pnLen)] ) )
					++(*pnLen);
			}

			ClearAllUpdates( eAction );
		}

		for ( CAnimationSet::iterator iter = unitAnimation.begin(); iter != unitAnimation.end(); ++iter )
		{
			(*pActionsBuffer)[*pnLen].pObj = GetObjectByUniqueIdSafe<CLinkObject>( iter->first );

			(*pActionsBuffer)[*pnLen].time = curTime;
			(*pActionsBuffer)[*pnLen].typeID = ACTION_NOTIFY_ANIMATION_CHANGED;
			(*pActionsBuffer)[*pnLen].nParam = ( iter->second ) << 16;

			if ( (*pActionsBuffer)[*pnLen].pObj != 0 )
				++(*pnLen);
		}
		unitAnimation.clear();

		AddRecalled( ACTION_NOTIFY_DEAD_UNIT, *pActionsBuffer, pnLen );
		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_DEAD_UNIT >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_DEAD_UNIT >> 4].end(); ++iter )
		{
			::new ( &(*pActionsBuffer)[(*pnLen)] ) SAINotifyAction();

			iter->second->GetDyingInfo( &(*pActionsBuffer)[(*pnLen)] );
			IUpdatableObj *pUnit = checked_cast<IUpdatableObj*>( (*pActionsBuffer)[(*pnLen)].pObj );
			
			if ( !theSuspendedUpdates.CheckToSuspend( ACTION_NOTIFY_DEAD_UNIT, pUnit, (*pActionsBuffer)[(*pnLen)] ) )
				++(*pnLen);
		}

		ClearAllUpdates( ACTION_NOTIFY_DEAD_UNIT );
	}
}
void CUpdater::UpdatePlacements( SAINotifyPlacement **pObjPosBuffer, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		const NTimer::STime timeDiff = GetAIGetSegmTime( pGameSegment ) - pGameTimer->GetGameTime();
		
		*pnLen = 0;
		*pObjPosBuffer = GetTempBuffer<SAINotifyPlacement>( complexUpdates[ACTION_NOTIFY_PLACEMENT >> 4].size() );
		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_PLACEMENT >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_PLACEMENT >> 4].end(); ++iter )
		{
			IUpdatableObj *pObj = iter->second;
			if ( pObj->IsValid() )
			{
				pObj->GetPlacement( &(*pObjPosBuffer)[*pnLen], timeDiff );
				
				CVec3 pos( (*pObjPosBuffer)[*pnLen].center, (*pObjPosBuffer)[*pnLen].z );
				(*pObjPosBuffer)[*pnLen].z += pObj->GetTerrainHeight( pos.x, pos.y, timeDiff );
				(*pObjPosBuffer)[*pnLen].pObj = pObj;

				const SVector tile( AICellsTiles::GetTile( pos.x, pos.y ) );
				if ( theStaticMap.IsTileInside( tile ) )
					(*pObjPosBuffer)[*pnLen].cSoil = theStaticMap.GetSoilType( tile );
				else
					(*pObjPosBuffer)[*pnLen].cSoil = 0;

				++(*pnLen);
			}
		}
		
		bPlacementsUpdated = true;
	}
}
void CUpdater::UpdateRPGParams( SAINotifyRPGStats **pUnitRPGBuffer, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		const int nSize = 
			complexUpdates[ACTION_NOTIFY_RPG_CHANGED >> 4].size() + 
			theSuspendedUpdates.GetNRecalled( ACTION_NOTIFY_RPG_CHANGED );

		*pUnitRPGBuffer = GetTempBuffer<SAINotifyRPGStats>( nSize );

		AddRecalled( ACTION_NOTIFY_RPG_CHANGED, *pUnitRPGBuffer, pnLen );	
		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_RPG_CHANGED >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_RPG_CHANGED >> 4].end(); ++iter )
		{
			IUpdatableObj *pObj = iter->second;
			if ( pObj->IsValid() )
			{
				::new ( &(*pUnitRPGBuffer)[(*pnLen)] ) SAINotifyRPGStats();
				pObj->GetRPGStats( &(*pUnitRPGBuffer)[(*pnLen)] );

				if ( !theSuspendedUpdates.CheckToSuspend( ACTION_NOTIFY_RPG_CHANGED, pObj, (*pUnitRPGBuffer)[(*pnLen)] ) )
					++(*pnLen);
			}
		}

		ClearAllUpdates( ACTION_NOTIFY_RPG_CHANGED );
	}
}
void CUpdater::UpdateHits( SAINotifyHitInfo **pHits, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		*pHits = GetTempBuffer<SAINotifyHitInfo>( complexUpdates[ACTION_NOTIFY_HIT >> 4].size() );

		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_HIT >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_HIT >> 4].end(); ++iter )
		{
			iter->second->GetHitInfo( &(*pHits)[(*pnLen)] );
			(*pHits)[(*pnLen)].explCoord.z += theStaticMap.GetVisZ( (*pHits)[(*pnLen)].explCoord.x, (*pHits)[(*pnLen)].explCoord.y );

			(*pnLen)++;
		}

		ClearAllUpdates( ACTION_NOTIFY_HIT );
	}
}
void CUpdater::UpdateStObjPlacements( SAINotifyPlacement **pObjPosBuffer, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		*pObjPosBuffer= GetTempBuffer<SAINotifyPlacement>( complexUpdates[ACTION_NOTIFY_ST_OBJ_PLACEMENT >> 4].size() );

		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_ST_OBJ_PLACEMENT >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_ST_OBJ_PLACEMENT >> 4].end(); ++iter )
		{
			iter->second->GetPlacement( &(*pObjPosBuffer)[(*pnLen)], 0 );

			CVec2 vPos = (*pObjPosBuffer)[(*pnLen)].center;
			(*pObjPosBuffer)[*pnLen].z += iter->second->GetTerrainHeight( vPos.x, vPos.y, 0 );

			++(*pnLen);
		}

		ClearAllUpdates( ACTION_NOTIFY_ST_OBJ_PLACEMENT );
	}
}
void CUpdater::GetNewProjectiles( struct SAINotifyNewProjectile **pProjectiles, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		*pProjectiles = GetTempBuffer<SAINotifyNewProjectile>( complexUpdates[ACTION_NOTIFY_NEW_PROJECTILE >> 4].size() );
			
		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_NEW_PROJECTILE >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_NEW_PROJECTILE >> 4].end(); ++iter )
			iter->second->GetProjectileInfo( &(*pProjectiles)[(*pnLen)++] );

		ClearAllUpdates( ACTION_NOTIFY_NEW_PROJECTILE );
	}
}
void CUpdater::GetDeadProjectiles( IRefCount ***pProjectilesBuf, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		*pProjectilesBuf = GetTempBuffer<IRefCount*>( complexUpdates[ACTION_NOTIFY_DEAD_PROJECTILE >> 4].size() );	

		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_DEAD_PROJECTILE >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_DEAD_PROJECTILE >> 4].end(); ++iter )
		{
			IUpdatableObj *pObj = iter->second;

			(*pProjectilesBuf)[(*pnLen)++] = pObj;
			DelUpdate( ACTION_NOTIFY_PLACEMENT, pObj );
		}

		ClearAllUpdates( ACTION_NOTIFY_DEAD_PROJECTILE );
	}
}
void CUpdater::GetNewUnits( SNewUnitInfo **pNewUnitBuffer, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		*pNewUnitBuffer = GetTempBuffer<SNewUnitInfo>( complexUpdates[ACTION_NOTIFY_NEW_UNIT >> 4].size() );	
		
		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_NEW_UNIT >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_NEW_UNIT >> 4].end(); ++iter )
		{
			IUpdatableObj *pObj = iter->second;
			if ( pObj->IsValid() && pObj->IsAlive() )
			{
				pObj->GetNewUnitInfo( &(*pNewUnitBuffer)[*pnLen] );
				(*pNewUnitBuffer)[*pnLen].z += pObj->GetTerrainHeight( (*pNewUnitBuffer)[*pnLen].center.x, (*pNewUnitBuffer)[*pnLen].center.y, 0 );

				(*pnLen)++;
			}
		}

		ClearAllUpdates( ACTION_NOTIFY_NEW_UNIT );
	}
}
void CUpdater::GetDisappearedUnits( IRefCount ***pUnitsBuffer, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		*pUnitsBuffer = GetTempBuffer<IRefCount*>( complexUpdates[ACTION_NOTIFY_DISSAPEAR_UNIT >> 4].size() );

		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_DISSAPEAR_UNIT >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_DISSAPEAR_UNIT >> 4].end(); ++iter )
		{
			IUpdatableObj *pObj = iter->second->GetDieObject();
			(*pUnitsBuffer)[(*pnLen)++] = pObj;

			const bool bShouldPlaceDeathCrater = 
				theSuspendedUpdates.DoesExistSuspendedUpdate( pObj, ACTION_NOTIFY_DEAD_UNIT ) && 
				iter->second->ShouldSuspendAction( ACTION_NOTIFY_NEW_ST_OBJ );

			theSuspendedUpdates.DeleteUpdates( pObj );

			if ( bShouldPlaceDeathCrater )
			{
				CTilesSet tiles;
				iter->second->GetTilesForVisibility( &tiles );

				SAINotifyAction dyingInfo;
				iter->second->GetDyingInfo( &dyingInfo );

				if ( (dyingInfo.nParam & 0x80000000) != 0 && !theStaticMap.IsBridge( tiles.front() ) )
				{
					SNewUnitInfo deathCraterUpdate;
					deathCraterUpdate.center = AICellsTiles::GetPointByTile( tiles.front() );
					deathCraterUpdate.z = theStaticMap.GetVisZ( deathCraterUpdate.center.x, deathCraterUpdate.center.y );
					deathCraterUpdate.dbID = dyingInfo.nParam;
					deathCraterUpdate.nFrameIndex = -2;
					deathCraterUpdate.nPlayer = theDipl.GetNeutralPlayer();

					theSuspendedUpdates.CheckToSuspend( ACTION_NOTIFY_NEW_ST_OBJ, iter->second, deathCraterUpdate );
				}
			}
		}

		ClearAllUpdates( ACTION_NOTIFY_DISSAPEAR_UNIT );
	}
}
void CUpdater::GetNewStaticObjects( struct SNewUnitInfo **pObjects, int *pnLen )
{
	const int nSize = 
		complexUpdates[ACTION_NOTIFY_NEW_ST_OBJ >> 4].size() +
		theSuspendedUpdates.GetNRecalled( ACTION_NOTIFY_NEW_ST_OBJ );

	*pObjects = GetTempBuffer<SNewUnitInfo>( nSize );	
	*pnLen = 0;	

	AddRecalled( ACTION_NOTIFY_NEW_ST_OBJ, *pObjects, pnLen );
	for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_NEW_ST_OBJ >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_NEW_ST_OBJ >> 4].end(); ++iter )
	{
		::new ( &(*pObjects)[(*pnLen)] ) SNewUnitInfo();		
		
		IUpdatableObj *pObj = iter->second;
		pObj->GetNewUnitInfo( &(*pObjects)[*pnLen] );
		(*pObjects)[*pnLen].z += pObj->GetTerrainHeight( (*pObjects)[*pnLen].center.x, (*pObjects)[*pnLen].center.y, 0 );

		if ( !theSuspendedUpdates.CheckToSuspend( ACTION_NOTIFY_NEW_ST_OBJ, pObj, (*pObjects)[*pnLen] ) )
			++(*pnLen);
	}

	ClearAllUpdates( ACTION_NOTIFY_NEW_ST_OBJ );
}
void CUpdater::GetDeletedStaticObjects( IRefCount ***pObjBuffer, int *pnLen )
{
	*pObjBuffer = GetTempBuffer<IRefCount*>( complexUpdates[ACTION_NOTIFY_DELETED_ST_OBJ >> 4].size() );
	*pnLen = 0;

	for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_DELETED_ST_OBJ >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_DELETED_ST_OBJ >> 4].end(); ++iter )
		(*pObjBuffer)[(*pnLen)++] = iter->second;

	ClearAllUpdates( ACTION_NOTIFY_DELETED_ST_OBJ );
}
void CUpdater::UpdateTurretTurn( SAINotifyTurretTurn **pTurretsBuffer, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		*pTurretsBuffer = GetTempBuffer<SAINotifyTurretTurn>( complexUpdates[ACTION_NOTIFY_TURRET_HOR_TURN >> 4].size() );

		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_TURRET_HOR_TURN >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_TURRET_HOR_TURN >> 4].end(); ++iter )
		{
			IUpdatableObj *pObj = iter->second;
			if ( pObj->IsValid() && pObj->IsAlive() )
				pObj->GetHorTurretTurnInfo( &(*pTurretsBuffer)[(*pnLen)++] );
		}
		ClearAllUpdates( ACTION_NOTIFY_TURRET_HOR_TURN );

		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_TURRET_VERT_TURN	>> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_TURRET_VERT_TURN >> 4].end(); ++iter )
		{
			IUpdatableObj *pObj = iter->second;
			if ( pObj->IsValid() && pObj->IsAlive() )
				pObj->GetVerTurretTurnInfo( &(*pTurretsBuffer)[(*pnLen)++] );
		}
		ClearAllUpdates( ACTION_NOTIFY_TURRET_VERT_TURN );
	}
}
void CUpdater::UpdateShots( SAINotifyMechShot **pShots, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		const NTimer::STime time = GetAIGetSegmTime( pGameSegment );	
		
		*pShots = GetTempBuffer<SAINotifyMechShot>( complexUpdates[ACTION_NOTIFY_MECH_SHOOT >> 4].size() );

		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_MECH_SHOOT >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_MECH_SHOOT >> 4].end(); ++iter )
		{
			IUpdatableObj *pObj = iter->second;
			if ( pObj->IsValid() )
			{
				pObj->GetMechShotInfo( &( (*pShots)[(*pnLen)] ), time );
				++(*pnLen );
			}
		}

		ClearAllUpdates( ACTION_NOTIFY_MECH_SHOOT );
	}
}
void CUpdater::UpdateShots( SAINotifyInfantryShot **pShots, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		const NTimer::STime time = GetAIGetSegmTime( pGameSegment );	
		
		*pShots = GetTempBuffer<SAINotifyInfantryShot>( complexUpdates[ACTION_NOTIFY_INFANTRY_SHOOT >> 4].size() );

		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_INFANTRY_SHOOT >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_INFANTRY_SHOOT >> 4].end(); ++iter )
		{
			IUpdatableObj *pObj = iter->second;
			if ( pObj->IsValid() )
			{
				pObj->GetInfantryShotInfo( &( (*pShots)[(*pnLen)] ), time );
				++(*pnLen );
			}
		}

		ClearAllUpdates( ACTION_NOTIFY_INFANTRY_SHOOT );
	}
}
void CUpdater::UpdateEntranceStates( SAINotifyEntranceState **pUnits, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		*pUnits = GetTempBuffer<SAINotifyEntranceState>( complexUpdates[ACTION_NOTIFY_ENTRANCE_STATE >> 4].size() );

		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_ENTRANCE_STATE >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_ENTRANCE_STATE >> 4].end(); ++iter )
		{
			IUpdatableObj *pObj = iter->second;
			if ( pObj->IsValid() )
			{
				pObj->GetEntranceStateInfo( &( (*pUnits)[(*pnLen)] ) );
				++(*pnLen);
			}
		}
		
		ClearAllUpdates( ACTION_NOTIFY_ENTRANCE_STATE );
	}
}
void CUpdater::GetEntrenchments( SSegment2Trench **pEntrenchemnts, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		*pEntrenchemnts = GetTempBuffer<SSegment2Trench>( complexUpdates[ACTION_NOTIFY_NEW_ENTRENCHMENT >> 4].size() );

		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_NEW_ENTRENCHMENT >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_NEW_ENTRENCHMENT >> 4].end(); ++iter )
		{
			IUpdatableObj *pObj = iter->second;
			if ( pObj->IsValid() )
			{
				NI_ASSERT_T( dynamic_cast<CEntrenchmentPart*>( pObj ) != 0, "Wrong object's type" );
				CEntrenchmentPart *pPart = static_cast<CEntrenchmentPart*>( pObj );

				(*pEntrenchemnts)[*pnLen].pSegment = pPart;
				(*pEntrenchemnts)[*pnLen].pEntrenchment = pPart->GetOwner();

				++(*pnLen);
			}
		}
		
		ClearAllUpdates( ACTION_NOTIFY_NEW_ENTRENCHMENT );
	}
}
void CUpdater::GetFormations( struct SSoldier2Formation **pFormations, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		*pFormations = GetTempBuffer<SSoldier2Formation>( complexUpdates[ACTION_NOTIFY_NEW_FORMATION >> 4].size() );

		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_NEW_FORMATION >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_NEW_FORMATION >> 4].end(); ++iter )
		{
			IUpdatableObj *pObj = iter->second;
			if ( pObj->IsValid() )
			{
				NI_ASSERT_T( dynamic_cast<CSoldier*>( pObj ) != 0, "Wrong unit's type" );
				CSoldier *pSoldier = static_cast<CSoldier*>( pObj );

				(*pFormations)[*pnLen].pSoldier = pSoldier;
				(*pFormations)[*pnLen].pFormation = pSoldier->GetFormation();

				++(*pnLen);
			}
		}
		
		ClearAllUpdates( ACTION_NOTIFY_NEW_FORMATION );
	}
}
void CUpdater::GetNewBridgeSpans( SNewUnitInfo **pObjects, int *pnLen )
{
	*pObjects = GetTempBuffer<SNewUnitInfo>( complexUpdates[ACTION_NOTIFY_NEW_BRIDGE_SPAN >> 4].size() );
	*pnLen = 0;	

	for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_NEW_BRIDGE_SPAN >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_NEW_BRIDGE_SPAN >> 4].end(); ++iter )
		iter->second->GetNewUnitInfo( &(*pObjects)[(*pnLen)++] );

	ClearAllUpdates( ACTION_NOTIFY_NEW_BRIDGE_SPAN );
}
void CUpdater::GetRevealCircles( CCircle **pCircleBuffer, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		*pCircleBuffer = GetTempBuffer<CCircle>( complexUpdates[ACTION_NOTIFY_REVEAL_ARTILLERY >> 4].size() );

		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_REVEAL_ARTILLERY >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_REVEAL_ARTILLERY >> 4].end(); ++iter )
			iter->second->GetRevealCircle( &(*pCircleBuffer)[(*pnLen)++] );

		ClearAllUpdates( ACTION_NOTIFY_REVEAL_ARTILLERY );
	}
}
void CUpdater::UpdateDiplomacies( SAINotifyDiplomacy **pDiplomaciesBuffer, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		const int nSize = 
			complexUpdates[ACTION_NOTIFY_UPDATE_DIPLOMACY >> 4].size() +
			theSuspendedUpdates.GetNRecalled( ACTION_NOTIFY_UPDATE_DIPLOMACY );

		*pDiplomaciesBuffer = GetTempBuffer<SAINotifyDiplomacy>( nSize );

		AddRecalled( ACTION_NOTIFY_UPDATE_DIPLOMACY, *pDiplomaciesBuffer, pnLen );
		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_UPDATE_DIPLOMACY >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_UPDATE_DIPLOMACY >> 4].end(); ++iter )
		{
			::new ( &(*pDiplomaciesBuffer)[(*pnLen)] ) SAINotifyDiplomacy();

			IUpdatableObj *pObj = iter->second;
			(*pDiplomaciesBuffer)[(*pnLen)].pObj = pObj;

			const int nPlayer = pObj->GetPlayer();
			const bool bNeutral = nPlayer == theDipl.GetNeutralPlayer() || nPlayer >= theDipl.GetNPlayers();

			(*pDiplomaciesBuffer)[(*pnLen)].eDiplomacy = bNeutral ? EDI_NEUTRAL : theDipl.GetDiplStatus( theDipl.GetMyNumber(), nPlayer );
			(*pDiplomaciesBuffer)[(*pnLen)].nPlayer = nPlayer;

			if ( !theSuspendedUpdates.CheckToSuspend( ACTION_NOTIFY_UPDATE_DIPLOMACY, pObj, (*pDiplomaciesBuffer)[(*pnLen)] ) )
				++(*pnLen);
		}

		ClearAllUpdates( ACTION_NOTIFY_UPDATE_DIPLOMACY );
	}
}
void CUpdater::UpdateShootAreas( SShootAreas **pShootAreas, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		*pShootAreas = GetShootAreasTempBuffer( complexUpdates[ACTION_NOTIFY_SHOOT_AREA >> 4].size() * 15 );

		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_SHOOT_AREA >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_SHOOT_AREA >> 4].end(); ++iter )
		{
			IUpdatableObj *pObj = iter->second;
			if ( IsValidObj( pObj ) )
			{
				int nAreas;
				SShootAreas *pAreaSlot = &(*pShootAreas)[(*pnLen)];
				new ( pAreaSlot ) SShootAreas();
				pObj->GetShootAreas( pAreaSlot, &nAreas );

				bool bGoodAreas = false;
				for ( int i = 0; i < nAreas; ++i )
				{
					for ( std::list<SShootArea>::iterator areaIter = (*pShootAreas)[(*pnLen) + i].areas.begin(); areaIter != (*pShootAreas)[(*pnLen) + i].areas.end(); ++areaIter )
					{
						const float fX = areaIter->vCenter3D.x;
						const float fY = areaIter->vCenter3D.y;
						areaIter->vCenter3D.z = pObj->GetTerrainHeight( fX, fY, 0 );
					}
				}

				if ( nAreas > 0 )
				{
					(*pnLen) += nAreas;
					g_nConstructedShootAreasInTempBuffer = *pnLen;
				}
				else
				{
					pAreaSlot->~SShootAreas();
				}
			}
		}
	}
}
void CUpdater::UpdateRangeAreas( SShootAreas **pRangeAreas, int *pnLen )
{
	*pnLen = 0;
	if ( !theDipl.IsNetGame() || pAILogic->IsNetGameStarted() )
	{
		*pRangeAreas = GetShootAreasTempBuffer( complexUpdates[ACTION_NOTIFY_RANGE_AREA >> 4].size() );

		for ( CComplexUpdatesSet::iterator iter = complexUpdates[ACTION_NOTIFY_RANGE_AREA >> 4].begin(); iter != complexUpdates[ACTION_NOTIFY_RANGE_AREA >> 4].end(); ++iter )
		{
			IUpdatableObj *pObj = iter->second;
			bool bGoodAreas = false;
			if ( pObj->IsValid() )
			{
				SShootAreas *pAreaSlot = &(*pRangeAreas)[(*pnLen)];
				new ( pAreaSlot ) SShootAreas();
				pObj->GetRangeArea( pAreaSlot );

				for ( std::list<SShootArea>::iterator areaIter = (*pRangeAreas)[(*pnLen)].areas.begin(); areaIter != (*pRangeAreas)[(*pnLen)].areas.end(); ++areaIter )
				{
					const float fX = areaIter->vCenter3D.x;
					const float fY = areaIter->vCenter3D.y;
					areaIter->vCenter3D.z = pObj->GetTerrainHeight( fX, fY, 0 );

					bGoodAreas = bGoodAreas || areaIter->fMaxR != 0;
				}
				if ( bGoodAreas )
				{
					++(*pnLen);
					g_nConstructedShootAreasInTempBuffer = *pnLen;
				}
				else
				{
					pAreaSlot->~SShootAreas();
				}
			}
		}
	}
}
void CUpdater::AddFeedBack( const SAIFeedBack &feedBack )
{
	const bool bFinishGame = 
		feedBack.feedBackType == EFB_WIN || feedBack.feedBackType == EFB_DRAW || feedBack.feedBackType == EFB_LOOSE;

	if ( !bFinishGame || !bGameFinishUpdateSend )
	{
		if ( !bGameFinishUpdateSend )
			bGameFinishUpdateSend = bFinishGame;

		feedBacks.push_back( feedBack );
	}
}
void CUpdater::UpdateFeedBacks( SAIFeedBack **pFeedBacksBuffer, int *pnLen )
{
	*pFeedBacksBuffer = GetTempBuffer<SAIFeedBack>( feedBacks.size() );
	*pnLen = 0;

	for ( std::list<SAIFeedBack>::iterator iter = feedBacks.begin(); iter != feedBacks.end(); ++iter )
	{
		(*pFeedBacksBuffer)[*pnLen] = *iter;
		++(*pnLen);
	}
	
	feedBacks.clear();
}
void CUpdater::UpdateAreasGroup( const int nGroup ) 
{ 
 	nShootAreasGroup = nGroup; 
	ClearAllUpdates( ACTION_NOTIFY_SHOOT_AREA );
	ClearAllUpdates( ACTION_NOTIFY_RANGE_AREA );
}
bool CUpdater::IsPlacementUpdated( IUpdatableObj *pObj ) const
{
	return updatedPlacements.find( pObj->GetUniqueId() ) != updatedPlacements.end();
}
void CUpdater::ClearPlacementsUpdates()
{
	updatedPlacements.clear();
}
void CUpdater::Add2Garbage( IUpdatableObj *pObj )
{ 
	garbage.insert( CComplexUpdatesSet::value_type( pObj->GetUniqueId(), pObj ) ); 
}
