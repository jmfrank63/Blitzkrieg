#ifndef __STATIC_OBJECTS_H__
#define __STATIC_OBJECTS_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Misc\AreaMap.h"
#include "..\Misc\HashFuncs.h"
#include "..\Common\Actions.h"
#include "RectTiles.h"
#include <set>
#include "PathFinder.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CExistingObject;
class CStaticObject;
class CBuildingStorage;

template<bool bOnlyContainers> class CStObjIter;
typedef std::list< CPtr<CBuildingStorage> > CStoragesList;
typedef std::unordered_set<int> CIntHash;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												  CStaticObjects													*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IObstacle;
interface IUpdatableObj;
class CStaticObjects : public IRefCount
{
private:
	OBJECT_NORMAL_METHODS( CStaticObjects );
	DECLARE_SERIALIZE;
	
	typedef CAreaMap<IObstacle, CPtr<IObstacle>, SVector, int > ObstacleAreaMap;
	typedef std::unordered_map< int, CPtr<IObstacle> > ObstacleObjectMap;

	ObstacleAreaMap obstacles;
	ObstacleObjectMap obstacleObjects;

	typedef CAreaMap<CExistingObject, CObj<CExistingObject>, SVector, int> StaticObjectsAreaMap;
	StaticObjectsAreaMap areaMap;
	StaticObjectsAreaMap containersAreaMap;
	int nObjs;

	std::list< CObj<IRefCount> > entrenchments;

	struct SSegmentObjectsSort
	{
		bool operator()( const CPtr<CStaticObject> &segmObj1, const CPtr<CStaticObject> &segmObj2 ) const;
	};

	// for iterators
	bool bIterCreated;
	StaticObjectsAreaMap& GetAreaMap() { return areaMap; }
	StaticObjectsAreaMap& GetContainersAreaMap() { return containersAreaMap; }
	void SetIterCreated( bool _bCreated ) { bIterCreated = _bCreated; }
	bool IsIterCreated() const { return bIterCreated; }
public:
		// ��� �������� ���� ���������
	interface IEnumStoragesPredicate
	{
		// ���������� ������ �������������� ���������
		virtual bool OnlyConnected() const = 0;
		// true - ���������, ��, ��� ����� ��� �������
		// ����� ���� - � ������
		virtual bool AddStorage( class CBuildingStorage * pStorage, const float fPathLenght ) = 0;
	};

	// ��� �������� ���������� � ������� RU 
	class CStoragesContainer
	{
		DECLARE_SERIALIZE;
		
		typedef std::unordered_map< CObj<CBuildingStorage>, bool, SUniqueIdHash > CStorages;
		typedef std::list< CObj<CBuildingStorage> > CStoragesList;
		
		struct CPartyInfo
		{
			DECLARE_SERIALIZE;
		public:
			CStoragesList mains;
			CStoragesList secondary;
		};
		std::vector<CPartyInfo> storageSystem;

		// ������ ����� ���������
		CStorages storages;									// for speed search storages
		
		WORD updated;
		bool bInitOnSegment;								//CRAP FOR SAVES COMPATIBILITY

		void AddStorage( class CBuildingStorage *pNewStorage, const int nPlayer );
	public:
		CStoragesContainer();
		void UpdateForParty( const int nParty );
		void Segment();
		void EnumStoragesForParty( const int nParty, IEnumStoragesPredicate * pPred );
		void Init() { Clear(); storageSystem.resize( 2 ); bInitOnSegment = false; }
		void EnumStoragesInRange( const CVec2 &vCenter, 
																					 const int nParty, 
																					 const float fMaxPathLenght,
																					 const float fMaxOffset,
  																				 class CCommonUnit * pUnitToFindPath, 
																					 IEnumStoragesPredicate * pPred );

		void AddStorage( class CBuildingStorage * pNewStorage );
		void RemoveStorage( class CBuildingStorage * pNewStorage );
		void StorageChangedDiplomacy( class CBuildingStorage *pNewStorage, const int nNewPlayer );
		void Clear();
	};

public:
	CStoragesContainer storagesContainer;
	
	// ����� ����� �� ��������������!
	typedef std::set< CPtr<CStaticObject>, SSegmentObjectsSort > CSegmObjects;
	CSegmObjects segmObjects;

	typedef std::unordered_set< CObj<CExistingObject>, SUniqueIdHash > CObjectsHashSet;
	CObjectsHashSet terraObjs;
	CObjectsHashSet deletedObjects;
	std::list< CPtr<CStaticObject> > unregisteredObjects;

	std::unordered_set<int> burningObjects;

	//
	void AddToAreaMap( CExistingObject *pObj );
	void AddObjectToAreaMapTile( CExistingObject *pObj, const SVector &tile );
	void RemoveFromAreaMap( CExistingObject *pObj );
	void RemoveObjectFromAreaMapTile( CExistingObject *pObj, const SVector &tile );
public:
	CStaticObjects()
		: areaMap( SConsts::STATIC_OBJ_CELL ), containersAreaMap( SConsts::STATIC_CONTAINER_OBJ_CELL ),
			obstacles( SConsts::STATIC_OBJ_CELL ), bIterCreated( false ) { }
	void Init( const int nMapTileSizeX, const int nMapTileSizeY );
	void Clear() 
	{ 
		//storagesContainer2.Clear(); 
		DestroyContents(); 
	}

	// for editor
	void RecalcPassabilityForPlayer( CArray2D<BYTE> *array, const int nParty );

	void AddObstacle( interface IObstacle *pObstacle );
	void RemoveObstacle( interface IObstacle *pObstacle );
	
	class CStaticObject* AddNewFenceObject( const SFenceRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const int nFrameIndex, const int nDiplomacy, bool bInitialization = false, bool IsEditor = false );
	void AddStaticObject( class CCommonStaticObject* pObj, bool bAlreadyLocked, bool bInitialization =false ) ;
	class CStaticObject* AddNewStaticObject( const SObjectBaseRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const int nFrameIndex, bool bInitialization = false );
	class CStaticObject* AddNewTerraObj( const SObjectBaseRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const int nFrameIndex, bool bInitialization = false );
	class CStaticObject* AddNewTerraMeshObj( const SObjectBaseRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const WORD wDir, const int nFrameIndex, bool bInitialization = false );
	class CStaticObject* AddNewBuilding( const SBuildingRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const int nFrameIndex, bool bInitialization = false );
	class CStaticObject* AddNewStorage( const SBuildingRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const int nFrameIndex, int player, bool bInitialization = false );
	class CStaticObject* AddNewFlag( const SStaticObjectRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const int nFrameIndex, int player, bool bInitialization = false );
	void AddStorage( class CBuildingStorage *pObj, bool bInitialization = false );
	class CStaticObject* AddNewEntrencmentPart( const SEntrenchmentRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const WORD dir, const int nFrameIndex, bool bInitialization = false );
	void AddEntrencmentPart(  class CEntrenchmentPart *pObj, bool bLockedAlready, bool bInitialization=false );
	class CStaticObject* AddNewEntrencment( IRefCount** segments, const int nLen, class CFullEntrenchment *pFullEntrenchment, bool bInitialization = false );
	class CStaticObject* AddNewBridgeSpan( const SBridgeRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const WORD ir, const int nFrameIndex, bool bInitialization = false );
	class CStaticObject* AddNewSmokeScreen( const CVec2 &vCenter, const float fR, const int nTransparency, const int nTime );

	class CStaticObject* AddNewMine( const SMineRPGStats *pStats, const float fHPFactor, const int dbID, const CVec2 &center, const int nFrameIndex, const int player, bool bInitialization = false );
	class CExistingObject* AddNewTankPit( const SMechUnitRPGStats *pStats, const CVec2& center, const WORD dir, const int nFrameIndex, const int dbID, const class CVec2 &vHalfSize, const CTilesSet &tilesToLock, class CAIUnit *pOwner, bool bInitialization = false );	
	void GetNewStaticObjects( struct SNewUnitInfo **pObjects, int *pnLen );
	void GetDeletedStaticObjects( IRefCount ***pObjects, int *pnLen );

	void RegisterSegment( class CStaticObject *pObj );
	void UnregisterSegment( class CStaticObject *pObj );

	//void UpdateRUStorageAreas( const EActionNotify eAction, const int nDipl );

	void Segment();

	// for RU storages 
	//void UpdateStoragesForParty( const int nParty, const bool bNewStorage, const bool bIncreasePassibility );
	//void UpdateAllPartiesStorages( const bool bNewStorage, const bool bIncreasePassibility );

	// ���������� ������ ������ ���������� ���������
	void DeleteInternalObjectInfo( class CExistingObject *pObj );
	void DeleteInternalObjectInfoForEditor( class CExistingObject *pObj );
	void DeleteInternalEntrenchmentInfo( class CEntrenchment *pEntrench );
	
	void StartBurning( class CExistingObject *pObj );
	void EndBurning( class CExistingObject *pObj );

	//bool IsPointUnderSupply( const int nPlayer, const CVec2 &vCenter ) const;
	void StorageChangedDiplomacy( class CBuildingStorage *pNewStorage, const int nNewPlayer );
	
	// ��� ���������
	void UpdateAllObjectsPos();

	void EnumObstaclesInRange( const CVec2 &vCenter, const float fRadius, interface IObstacleEnumerator *f );
	void EnumStoragesForParty( const int nParty, interface IEnumStoragesPredicate *pPred );
	void EnumStoragesInRange( const CVec2 &vCenter, const int nParty, const float fMaxPathLength, const float fMaxOffset,
														class CCommonUnit *pUnitToFindPath, interface IEnumStoragesPredicate *pPred );


	friend class CStObjIter<false>;
	friend class CStObjIter<true>;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __STATIC_OBJECTS_H__
