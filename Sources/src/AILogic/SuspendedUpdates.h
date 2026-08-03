#ifndef __SUSPENDED_UPDATES_H__
#define __SUSPENDED_UPDATES_H__
#pragma ONCE
#include "../Misc/AreaMap.h"
#include "UpdatableObject.h"
#include "AIHashFuncs.h"
class CSuspendedUpdates
{
	DECLARE_SERIALIZE;

	typedef CAreaMap<IUpdatableObj, CPtr<IUpdatableObj>, SVector, int> CObjectsByCells;
	CObjectsByCells objectsByCells;
	std::unordered_map< CObj<IUpdatableObj>, std::vector< CPtr<IDataStream> >, SUpdatableObjectObjHash > updates;
	typedef std::unordered_map< int, std::unordered_set<SVector, STilesHash > > CTilesMap;
	CTilesMap tilesOfObj;
	
	typedef std::unordered_map< CObj<IUpdatableObj>, CPtr<IDataStream>, SUpdatableObjectObjHash > CDiplomacyUpdatesType;
	CDiplomacyUpdatesType diplomacyUpdates;

	struct SRecalledUpdate
	{
		virtual int STDCALL operator&( IStructureSaver &ss ) { CSaverAccessor saver = &ss; saver.Add( 1, &pObj ); saver.Add( 3, &pUpdateInfo ); return 0; }
	public:	
		CObj<IUpdatableObj> pObj;
		CPtr<IDataStream> pUpdateInfo;

		SRecalledUpdate() { }
		SRecalledUpdate( IUpdatableObj *_pObj, IDataStream *_pUpdateInfo ) 
			: pObj( _pObj ), pUpdateInfo( _pUpdateInfo ) { }

		void Recall( SSuspendedUpdate *pRecallTo );
	};

	typedef std::list<SRecalledUpdate> CRecalledUpdatesType;
	std::vector<CRecalledUpdatesType> recalledUpdates;

	std::unordered_set< SVector, STilesHash > visibleTiles;

	int nMyParty;

	void DeleteObjectInfo( IUpdatableObj *pObj );
	void DeleteUpdate( IUpdatableObj *pObj, const EActionNotify &eAction );
	void CommonInit();
	void SuspendUpdate( const EActionNotify &eAction, IUpdatableObj * pObj, const  SSuspendedUpdate &update );
	void UpdateVisibleTiles( const std::unordered_set< SVector, STilesHash > &tilesSet, std::unordered_set<SVector, STilesHash> *pCoverTiles );
public:
	CSuspendedUpdates();

	void Init( const int nStaticMapSizeX, const int nStaticMapSizeY );
	void Clear();

	void AddComplexObjectUpdate( const EActionNotify &eAction, IUpdatableObj * pObj, const  SSuspendedUpdate &update );
	
	void TileBecameVisible( const SVector &tile, const int nParty );

	bool CheckToSuspend( const EActionNotify &eAction, IUpdatableObj *pObj, const SSuspendedUpdate &update );

	bool DoesExistSuspendedUpdate( IUpdatableObj *pObj, const EActionNotify &eAction );

	bool IsRecalledEmpty( const EActionNotify &eAction ) const;
	const int GetNRecalled( const EActionNotify &eAction ) const;

	void GetRecalled( const EActionNotify &eAction, SSuspendedUpdate *pUpdate );
	void DeleteUpdates( IUpdatableObj *pObj );

	void Segment();
};
#endif // __SUSPENDED_UPDATES_H__
