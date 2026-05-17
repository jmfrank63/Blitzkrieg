#include "stdafx.h"

#include "StaticObjects.h"
#include "StaticObjectsIters.h"
#include "StaticObject.h"
#include "Building.h"
#include "Entrenchment.h"
#include "Updater.h"
#include "UnitStates.h"
#include "Bridge.h"
#include "Mine.h"
#include "AIWarFog.h"
#include "UnitsIterators2.h"
#include "Diplomacy.h"
#include "AIUnit.h"
#include "Fence.h"
#include "pathfinder.h"
#include "AIStaticMap.h"
#include "AICellsTiles.h"
#include "Path.h"
#include "SmokeScreen.h"
#include "Obstacle.h"
#include "ObstacleINternal.h"
#include "Flag.h"
#include "Graveyard.h"
#include "Cheats.h"

#include "..\Misc\BitData.h"
#include "..\Scene\Scene.h"

// for profiling
#include "MPLog.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticObjects theStatObjs;
extern CStaticMap theStaticMap;
extern CUpdater updater;
extern CGlobalWarFog theWarFog;
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
extern CPtr<IStaticPathFinder> pThePathFinder;
extern CGraveyard theGraveyard;
extern SCheats theCheats;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*									CStaticObjects::SSegmentObjectsSort							*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStaticObjects::SSegmentObjectsSort::operator()( const CPtr<CStaticObject> &segmObj1, const CPtr<CStaticObject> &segmObj2 ) const
{
	bool res = 
					segmObj1->GetNextSegmentTime() < segmObj2->GetNextSegmentTime() ||
				 segmObj1->GetNextSegmentTime() == segmObj2->GetNextSegmentTime() &&
				 segmObj1->GetUniqueId() < segmObj2->GetUniqueId();
	return res;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*									CStaticObjects::CStoragesContainer							*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticObjects::CStoragesContainer::CStoragesContainer()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::CStoragesContainer::StorageChangedDiplomacy( class CBuildingStorage *pNewStorage, const int nNewPlayer )
{
	RemoveStorage( pNewStorage );
	AddStorage( pNewStorage, nNewPlayer );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::CStoragesContainer::AddStorage( class CBuildingStorage *pNewStorage )
{
	AddStorage( pNewStorage, pNewStorage->GetPlayer() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::CStoragesContainer::AddStorage( class CBuildingStorage *pNewStorage, const int nPlayer )
{
	const int nParty = theDipl.GetNParty( nPlayer );
	if ( nParty >= 2 ) 
		return;
	updated |= (1 << nParty);
	storages[pNewStorage] = true;
		
	const SBuildingRPGStats * pStats = static_cast<const SBuildingRPGStats*>( pNewStorage->GetStats() );
	NI_ASSERT_T( SBuildingRPGStats::TYPE_MAIN_RU_STORAGE == pStats->eType || SBuildingRPGStats::TYPE_TEMP_RU_STORAGE == pStats->eType, "wrong object in stores list");

	if ( pStats->eType ==  SBuildingRPGStats::TYPE_MAIN_RU_STORAGE )
		storageSystem[nParty].mains.push_back( pNewStorage );
	else
		storageSystem[nParty].secondary.push_back( pNewStorage );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::CStoragesContainer::RemoveStorage( class CBuildingStorage *pNewStorage )
{
	if ( storages.find( pNewStorage ) != storages.end() ) // this static object is storage
	{
		const int nParty = theDipl.GetNParty(pNewStorage->GetPlayer());
		if ( nParty >= 2 ) 
			return;
		updated |= (1 << nParty );

		storages.erase( pNewStorage );
		const SBuildingRPGStats * pStats = static_cast<const SBuildingRPGStats*>( pNewStorage->GetStats() );
		NI_ASSERT_T( SBuildingRPGStats::TYPE_MAIN_RU_STORAGE == pStats->eType || SBuildingRPGStats::TYPE_TEMP_RU_STORAGE == pStats->eType, "wrong object in stores list");

		if ( pStats->eType ==  SBuildingRPGStats::TYPE_MAIN_RU_STORAGE )
			storageSystem[nParty].mains.remove( pNewStorage );
		else
			storageSystem[nParty].secondary.remove( pNewStorage );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::CStoragesContainer::Clear()
{
	storages.clear();
	updated = (1<<0) | (1<<1);
	storageSystem.clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::CStoragesContainer::EnumStoragesForParty( const int nParty, CStaticObjects::IEnumStoragesPredicate * pPred )
{
	for ( CStorages::iterator i = storages.begin(); i != storages.end(); ++i )
	{
		CBuildingStorage *pStor = i->first;
		if ( theDipl.GetNParty(pStor->GetPlayer()) == nParty &&
				 (!pPred->OnlyConnected() || pStor->IsConnected()) &&
				 pStor->IsAlive() )
		{
			pPred->AddStorage( pStor, 0 );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::CStoragesContainer::EnumStoragesInRange( const CVec2 &vCenter, 
																					 const int nParty, 
																					 const float fMaxPathLenght,
																					 const float fMaxOffset,
  																				 CCommonUnit *pUnitToFindPath, 
																					 CStaticObjects::IEnumStoragesPredicate *pPred )
{
	if ( nParty >= 2 ) 
		return;
	
	CPartyInfo &possibleConnected = storageSystem[nParty];
	
	bool bFoundMains = false;
	
	for ( CStoragesList::iterator it = possibleConnected.mains.begin(); it != possibleConnected.mains.end(); ++it )
	{
		CBuildingStorage *pStor = *it;
		bFoundMains = true;
		const CVec2 vStorageCenter( pStor->GetAttackCenter( pUnitToFindPath->GetCenter() ) );
		CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vStorageCenter, VNULL2, pUnitToFindPath, true );
		if ( pPath &&
					fabs2( vStorageCenter - pPath->GetFinishPoint() ) <= sqr( fMaxOffset ) &&
					pPred->AddStorage( pStor, pPath->GetLength() ) )
		{
			return;
		}
	}
	
	if ( bFoundMains )
	{
		for ( CStoragesList::iterator it = possibleConnected.secondary.begin(); it != possibleConnected.secondary.end(); ++it )
		{
			CBuildingStorage *pStor = *it;
			bFoundMains = true;
			const CVec2 vStorageCenter( pStor->GetAttackCenter( pUnitToFindPath->GetCenter() ) );
			CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vStorageCenter, VNULL2, pUnitToFindPath, true );
			if ( pPath &&
						fabs2( vStorageCenter - pPath->GetFinishPoint() ) <= sqr( fMaxOffset ) &&
						pPred->AddStorage( pStor, pPath->GetLength() ) )
			{
				return;
			}
		}
	}

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::CStoragesContainer::Segment()
{
	//CRAP{ FOR SAVES COMPATIBLITY
	if ( bInitOnSegment )
	{
		for ( CStorages::iterator it = storages.begin(); it != storages.end(); ++it )
			AddStorage( it->first );
		bInitOnSegment = false;
	}
	//CARP}
	if ( updated )
	{
		for ( int i = 0; i < 2; ++i )
		{
			if ( updated & (1<<i) )
			{
				// find party's main storage
				bool bFound = false;
				CPartyInfo &info = storageSystem[i];
				for ( CStoragesList::iterator it = info.mains.begin(); it != info.mains.end(); ++it )
				{
					if ( (*it)->IsAlive() )
					{
						bFound = true;
						break;
					}
				}
				for ( CStoragesList::iterator it = info.secondary.begin(); it != info.secondary.end(); ++it )
					(*it)->SetConnected( bFound );
				updated &= ~( 1<<i );
			}
		}
		updated = 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												  CStaticObjects													*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::StorageChangedDiplomacy( class CBuildingStorage *pNewStorage, const int nNewPlayer )
{
	storagesContainer.StorageChangedDiplomacy( pNewStorage, nNewPlayer );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::RecalcPassabilityForPlayer( CArray2D<BYTE> *array, const int nParty )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::Init( const int nMapTileSizeX, const int nMapTileSizeY )
{
	areaMap.SetSizes( nMapTileSizeX / SConsts::STATIC_OBJ_CELL, nMapTileSizeY / SConsts::STATIC_OBJ_CELL );
	containersAreaMap.SetSizes( nMapTileSizeX / SConsts::STATIC_CONTAINER_OBJ_CELL, nMapTileSizeY / SConsts::STATIC_CONTAINER_OBJ_CELL );
	obstacles.SetSizes( nMapTileSizeX / SConsts::STATIC_OBJ_CELL, nMapTileSizeY / SConsts::STATIC_OBJ_CELL );
	nObjs = 0;
	segmObjects.clear();
	
	storagesContainer.Init();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::EnumObstaclesInRange(  const CVec2 &vCenter,
																						const float fR,
																						interface IObstacleEnumerator *f )
{
	const int nMinX = Max( 0, int( ( vCenter.x - fR ) / ( SConsts::TILE_SIZE * SConsts::STATIC_OBJ_CELL ) ) );
	const int nMaxX = Min( obstacles.GetSizeX() - 1, int( ( vCenter.x + fR ) / float( SConsts::TILE_SIZE * SConsts::STATIC_OBJ_CELL ) ) );
	const int nMinY = Max( 0, int( ( vCenter.y - fR ) / ( SConsts::TILE_SIZE * SConsts::STATIC_OBJ_CELL ) ) );

	const int nMaxY = Min( obstacles.GetSizeY() - 1, int( ( vCenter.y + fR ) / float( SConsts::TILE_SIZE * SConsts::STATIC_OBJ_CELL ) ) );
	const float fR2 = sqr( fR );

	for ( int x = nMinX; x <= nMaxX; ++x )
	{
		for ( int y = nMinY; y <= nMaxY; ++y )
		{
			for ( ObstacleAreaMap::CDataList::iterator it = obstacles(x,y).begin(); it != obstacles(x,y).end(); ++it )
			{

				if ( (*it)->IsAlive() && fabs2( vCenter - (*it)->GetCenter()) < fR2 )
					f->AddObstacle( *it );
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::EnumStoragesForParty( const int nParty, CStaticObjects::IEnumStoragesPredicate * pPred )
{
	storagesContainer.EnumStoragesForParty( nParty, pPred );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::EnumStoragesInRange( const CVec2 &vCenter, 
																					 const int nParty, 
																					 const float fMaxPathLenght,
																					 const float fMaxOffset,
  																				 class CCommonUnit * pUnitToFindPath, 
																					 CStaticObjects::IEnumStoragesPredicate * pPred )
{
	storagesContainer.EnumStoragesInRange( vCenter, nParty, fMaxPathLenght, fMaxOffset, pUnitToFindPath, pPred );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::DeleteInternalObjectInfo( CExistingObject *pObj )
{
	deletedObjects.insert( pObj );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::DeleteInternalObjectInfoForEditor( CExistingObject *pObj )
{
	UnregisterSegment( pObj );

	if ( pObj->GetObjectType() != ESOT_TERRA )
		RemoveFromAreaMap( pObj );
	else
		terraObjs.erase( pObj );
	CBuildingStorage * pStor = static_cast<CBuildingStorage*>( pObj );
	storagesContainer.RemoveStorage( pStor );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::DeleteInternalEntrenchmentInfo( CEntrenchment *pEntrench )
{
	IRefCount *pObj = pEntrench;
	std::list< CObj<IRefCount> > ::iterator iter = entrenchments.begin();
	while ( iter != entrenchments.end() && (*iter).GetPtr() != pObj )
		++iter;

	NI_ASSERT_T( iter != entrenchments.end(), "Wrong object to delete" );

	entrenchments.erase( iter );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticObject* CStaticObjects::AddNewFenceObject( const SFenceRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const int nFrameIndex, const int nDiplomacy, bool bInitialization, bool IsEditor )
{
	CFence *pFence = new CFence( pStats, center, dbID, pStats->fMaxHP * fHPFactor, nFrameIndex, nDiplomacy, IsEditor );
	pFence->Init();
	
	AddStaticObject( pFence, false, bInitialization );

	return pFence;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticObject* CStaticObjects::AddNewSmokeScreen( const CVec2 &vCenter, const float fR, const int nTransparency, const int nTime )
{
	CSmokeScreen *pObj = new CSmokeScreen( vCenter, fR, nTransparency, nTime );
	pObj->Mem2UniqueIdObjs();
	pObj->Init();

	pObj->SetTransparencies();
	
	AddToAreaMap( pObj );

	theWarFog.ReclaculateFogAfterAddObject( pObj );

	return pObj;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CExistingObject* CStaticObjects::AddNewTankPit( const SMechUnitRPGStats *pStats, const CVec2& center, const WORD dir, const int nFrameIndex, const int dbID, const class CVec2 &vHalfSize, const CTilesSet &tilesToLock, class CAIUnit *pOwner, bool bInitialization )
{
	CEntrenchmentTankPit *pObj = new CEntrenchmentTankPit( pStats, center, dir, nFrameIndex, dbID, vHalfSize, tilesToLock, pOwner );
	pObj->Mem2UniqueIdObjs();
	pObj->LockTiles( bInitialization );
	AddToAreaMap( pObj );

	/*if ( pOwner->IsVisibleByPlayer() )
	updater.Update( ACTION_NOTIFY_NEW_ST_OBJ, pObj );*/
	return pObj;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::AddStaticObject( class CCommonStaticObject* pObj, bool bAlreadyLocked, bool bInitialization )
{
	pObj->Mem2UniqueIdObjs();
	if ( !bAlreadyLocked )
		pObj->LockTiles( bInitialization );
	pObj->SetTransparencies();
	AddToAreaMap( pObj );

	updater.Update( ACTION_NOTIFY_NEW_ST_OBJ, pObj );

	if ( ESOT_FENCE == pObj->GetObjectType() && pObj->GetPlayer() != theDipl.GetNeutralPlayer() )
	{
		CPtr<CObstacleStaticObject> pObstacle = new CObstacleStaticObject( pObj );
		AddObstacle( pObstacle );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticObject* CStaticObjects::AddNewStaticObject( const SObjectBaseRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const int nFrameIndex, bool bInitialization )
{
	CCommonStaticObject *pObj = new CSimpleStaticObject( pStats, center, dbID, pStats->fMaxHP * fHPFactor, nFrameIndex, ESOT_COMMON );
	pObj->Mem2UniqueIdObjs();
	pObj->Init();

	AddStaticObject( pObj, false, bInitialization );
	
	return pObj;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticObject* CStaticObjects::AddNewFlag( const SStaticObjectRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const int nFrameIndex, int player, bool bInitialization )
{
	CFlag *pFlag = new CFlag( pStats, center, dbID, pStats->fMaxHP * fHPFactor, nFrameIndex, player, ESOT_FLAG );
	pFlag->Mem2UniqueIdObjs();
	pFlag->Init();

	AddStaticObject( pFlag, false, bInitialization );

	return pFlag;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticObject* CStaticObjects::AddNewTerraObj( const SObjectBaseRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const int nFrameIndex, bool bInitialization )
{
	CCommonStaticObject *pObj = new CSimpleStaticObject( pStats, center, dbID, pStats->fMaxHP * fHPFactor, nFrameIndex, ESOT_TERRA );
	pObj->Mem2UniqueIdObjs();
	pObj->Init();
	
	pObj->LockTiles( bInitialization );
	pObj->SetTransparencies();
	terraObjs.insert( pObj );

	updater.Update( ACTION_NOTIFY_NEW_ST_OBJ, pObj );

	return pObj;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticObject* CStaticObjects::AddNewTerraMeshObj( const SObjectBaseRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const WORD wDir, const int nFrameIndex, bool bInitialization )
{
	CCommonStaticObject *pObj = new CTerraMeshStaticObject( pStats, center, wDir, dbID, pStats->fMaxHP * fHPFactor, nFrameIndex, ESOT_TERRA );
	pObj->Mem2UniqueIdObjs();
	pObj->Init();
	
	pObj->LockTiles( bInitialization );
	pObj->SetTransparencies();
	terraObjs.insert( pObj );

	updater.Update( ACTION_NOTIFY_NEW_ST_OBJ, pObj );

	return pObj;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::AddStorage( CBuildingStorage *pObj, bool bInitialization )
{
	pObj->LockTiles( bInitialization );
	pObj->SetTransparencies();

	AddToAreaMap( pObj );
	storagesContainer.AddStorage( pObj );
	//storagesContainer2.AddStorage( pObj );

	updater.Update( ACTION_NOTIFY_NEW_ST_OBJ, pObj );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticObject* CStaticObjects::AddNewStorage( const SBuildingRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const int nFrameIndex, int player, bool bInitialization )
{
	CBuildingStorage *pObj = new CBuildingStorage( pStats, center, dbID, pStats->fMaxHP * fHPFactor, nFrameIndex, player );
	pObj->Mem2UniqueIdObjs();
	pObj->Init();
//	if ( SBuildingRPGStats::TYPE_MAIN_RU_STORAGE == pStats->eType )
	RegisterSegment( pObj );
	
	AddStorage( pObj, bInitialization );
		
	return pObj;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticObject* CStaticObjects::AddNewBuilding( const SBuildingRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const int nFrameIndex, bool bInitialization )
{
	CBuildingSimple *pObj = new CBuildingSimple( pStats, center, dbID, pStats->fMaxHP * fHPFactor, nFrameIndex );
	pObj->Mem2UniqueIdObjs();
	pObj->Init();
	
	pObj->LockTiles( bInitialization );
	pObj->SetTransparencies();

	AddToAreaMap( pObj );

	updater.Update( ACTION_NOTIFY_NEW_ST_OBJ, pObj );

	return pObj;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticObject* CStaticObjects::AddNewBridgeSpan( const SBridgeRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const WORD dir, const int nFrameIndex, bool bInitialization )
{
	CBridgeSpan *pObj = new CBridgeSpan( pStats, center, dbID, pStats->fMaxHP * fHPFactor, nFrameIndex );
	pObj->Mem2UniqueIdObjs();
	pObj->Init();
	
	pObj->LockTiles( bInitialization );
	pObj->SetTransparencies();
	AddToAreaMap( pObj );

	updater.Update( ACTION_NOTIFY_NEW_BRIDGE_SPAN, pObj );

	return pObj;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::RemoveObstacle( interface IObstacle *pObstacle )
{
	obstacleObjects.erase( pObstacle->GetObject()->GetUniqueId() );
	obstacles.RemoveFromPosition( pObstacle, AICellsTiles::GetTile(pObstacle->GetCenter()) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::AddObstacle( interface IObstacle *pObstacle )
{
	obstacleObjects.insert( std::pair< int, CPtr<IObstacle> >(pObstacle->GetObject()->GetUniqueId(), pObstacle ) );
	obstacles.AddToPosition( pObstacle, AICellsTiles::GetTile(pObstacle->GetCenter()) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::AddEntrencmentPart(  class CEntrenchmentPart *pObj, bool bLockedAlready, bool bInitialization )
{
	pObj->Mem2UniqueIdObjs();
	if ( !bLockedAlready )
		pObj->LockTiles( bInitialization );
	pObj->SetTransparencies();

	AddToAreaMap( pObj );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticObject* CStaticObjects::AddNewEntrencmentPart( const SEntrenchmentRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const WORD dir, const int nFrameIndex, bool bInitialization )
{
	CEntrenchmentPart *pObj = new CEntrenchmentPart( pStats, center, dir, nFrameIndex, dbID, pStats->fMaxHP * fHPFactor );
	pObj->Mem2UniqueIdObjs();
	pObj->Init();

	AddEntrencmentPart( pObj, false, bInitialization );

	return pObj;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticObject* CStaticObjects::AddNewEntrencment( IRefCount** segments, const int nLen, class CFullEntrenchment *pFullEntrenchment, bool bInitialization )
{
	CEntrenchment *pEntrench = new CEntrenchment( segments, nLen, pFullEntrenchment );
	pEntrench->Mem2UniqueIdObjs();

	entrenchments.push_back( pEntrench );

	return pEntrench;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticObject* CStaticObjects::AddNewMine( const SMineRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const int nFrameIndex, const int player, bool bInitialization )
{
	CMineStaticObject *pObj = new CMineStaticObject( pStats, center, dbID, pStats->fMaxHP * fHPFactor, nFrameIndex, player );
	pObj->Mem2UniqueIdObjs();
	pObj->Init();

	pObj->LockTiles( bInitialization );
	pObj->SetTransparencies();

	pObj->ClearVisibleStatus();

	AddToAreaMap( pObj );
	
	// ������ ������ ���� ����
	if ( theDipl.GetDiplStatus( theDipl.GetMyNumber(), player ) == EDI_FRIEND || theCheats.IsHistoryPlaying() )
		pObj->RegisterInWorld();

	return pObj;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::AddObjectToAreaMapTile( CExistingObject *pObj, const SVector &tile )
{
	if ( theStaticMap.IsTileInside( tile ) )
	{
		areaMap.AddToPosition( pObj, tile );
		if ( pObj->IsContainer() )
			containersAreaMap.AddToPosition( pObj, tile );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::AddToAreaMap( CExistingObject *pObj )
{
	CTilesSet tiles;
	pObj->GetCoveredTiles( &tiles );
	
	// ����� �� �������� ����� update
	if ( tiles.empty() )
		AddObjectToAreaMapTile( pObj, AICellsTiles::GetTile( pObj->GetCenter() ) );
	else
	{
		for ( CTilesSet::iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
			AddObjectToAreaMapTile( pObj, *iter );
	}

	++nObjs;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::RemoveObjectFromAreaMapTile( CExistingObject *pObj, const SVector &tile )
{
	if ( theStaticMap.IsTileInside( tile ) )
	{
		areaMap.RemoveFromPosition( pObj, tile );
		if ( pObj->IsContainer() )
			containersAreaMap.RemoveFromPosition( pObj, tile );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::RemoveFromAreaMap( CExistingObject *pObj )
{
	CTilesSet tiles;
	pObj->GetCoveredTiles( &tiles );

	if ( tiles.empty() )
		RemoveObjectFromAreaMapTile( pObj, AICellsTiles::GetTile( pObj->GetCenter() ) );
	else
	{
		for ( CTilesSet::iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
			RemoveObjectFromAreaMapTile( pObj, *iter );
	}

	--nObjs;
	NI_ASSERT_T( nObjs >= 0, "Wrong number of static objects" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::RegisterSegment( class CStaticObject *pObj )
{
	segmObjects.insert( pObj );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::UnregisterSegment( class CStaticObject *pObj )
{
	unregisteredObjects.push_back( pObj );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::Segment()
{
	std::set< CPtr<CStaticObject>, SSegmentObjectsSort >::iterator iter = segmObjects.begin();
	std::list< CPtr<CStaticObject> > changedObjs;
	while ( iter != segmObjects.end() && curTime >= (*iter)->GetNextSegmentTime() )
	{
		changedObjs.push_back( *iter );
		++iter;
	}

	for ( std::list< CPtr<CStaticObject> >::iterator iter = changedObjs.begin(); iter != changedObjs.end(); ++iter )
	{
		CPtr<CStaticObject> pObject = *iter;
		
		segmObjects.erase( pObject.GetPtr() );

		pObject->Segment();
		segmObjects.insert( pObject.GetPtr() );
	}

	for ( std::list< CPtr<CStaticObject> >::iterator iter = unregisteredObjects.begin(); iter != unregisteredObjects.end(); ++iter )
		segmObjects.erase( *iter );
	unregisteredObjects.clear();

	for ( CObjectsHashSet::iterator iter = deletedObjects.begin(); iter != deletedObjects.end(); ++iter )
	{
		CExistingObject *pObj = *iter;
		UnregisterSegment( pObj );

		// ����������� ����� ��� ��������� ������
		// CRAP{ �� �������������, ���� ��������� ����. ������ ��������� ���������
		theWarFog.ReclaculateFogAfterRemoveObject( pObj );
		// CRAP}

		updater.Update( ACTION_NOTIFY_DELETED_ST_OBJ, pObj );

		storagesContainer.RemoveStorage( static_cast<CBuildingStorage*>(pObj) );
		//storagesContainer2.RemoveStorage( static_cast<CBuildingStorage*>(pObj) );

		if ( obstacleObjects.end() != obstacleObjects.find( pObj->GetUniqueId() ) )
			RemoveObstacle( obstacleObjects[pObj->GetUniqueId()] );

		pObj->UnlockTiles();
		pObj->RemoveTransparencies();
		if ( pObj->GetObjectType() != ESOT_TERRA )
			RemoveFromAreaMap( pObj );
		else
			terraObjs.erase( pObj );
	}

	deletedObjects.clear();

	// ������� �������
	std::list<int> burningList;
	for ( std::unordered_set<int>::const_iterator iter = burningObjects.begin(); iter != burningObjects.end(); ++iter )
		burningList.push_back( *iter );

	for ( std::list<int>::iterator iter = burningList.begin(); iter != burningList.end(); ++iter )
	{
		CExistingObject *pObj = GetObjectByUniqueIdSafe<CExistingObject>( *iter );
		if ( !pObj || !pObj->IsValid() || !pObj->IsAlive() )
			burningObjects.erase( *iter );
		else
			pObj->BurnSegment();
	}

  storagesContainer.Segment();
	//storagesContainer2.Segment();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::StartBurning( CExistingObject *pObj )
{
	burningObjects.insert( pObj->GetUniqueId() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::EndBurning( CExistingObject *pObj )
{
	burningObjects.erase( pObj->GetUniqueId() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticObjects::UpdateAllObjectsPos()
{
	for ( CObjectsHashSet::iterator iter = terraObjs.begin(); iter != terraObjs.end(); ++iter )
	{
		CExistingObject *pObj = *iter;
		pObj->SetNewPlacement( pObj->GetCenter(), pObj->GetDir() );
	}

	for ( CStObjGlobalIter<false> iter; !iter.IsFinished(); iter.Iterate() )
	{
		CExistingObject *pObj = *iter;
		pObj->SetNewPlacement( pObj->GetCenter(), pObj->GetDir() );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
