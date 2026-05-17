#ifndef __WORLDBASE_H__
#define __WORLDBASE_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\GFX\GFX.h"
#include "..\Scene\Scene.h"
#include "..\Input\Input.h"
#include "..\Common\World.h"
#include "..\Common\MapObject.h"
#include "..\AILogic\AILogic.h"
#include "..\Main\Transceiver.h"
#include "..\GameTT\AckManager.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CComplexObjects
{
	typedef std::unordered_map<IRefCount*, CMapObjectsList, SDefaultPtrHash> CAIVisMap;
	typedef std::unordered_map<SMapObject*, CPtr<IRefCount>, SDefaultPtrHash> CVisAIMap;
	CAIVisMap aivis;
	CVisAIMap visai;
public:
	void Clear()
	{
		aivis.clear();
		visai.clear();
	}
	//
	void AddSegment( IRefCount *pAI, SMapObject *pMO )
	{
		aivis[pAI].push_back( pMO );
		visai[pMO] = pAI;
	}
	//
	void RemoveSegment( SMapObject *pMO )
	{
		CVisAIMap::iterator pos = visai.find( pMO );
		if ( pos == visai.end() )
			return;
		IRefCount *pAI = pos->second;
		CMapObjectsList &mos = aivis[pAI];
		mos.remove( pMO );
		if ( mos.empty() )
			aivis.erase( pAI );
		visai.erase( pMO );
	}
	//
	void RemoveObject( IRefCount *pAI )
	{
		CMapObjectsList &mos = aivis[pAI];
		for ( CMapObjectsList::iterator it = mos.begin(); it != mos.end(); ++it )
			visai.erase( (*it) );
		aivis.erase( pAI );
	}
	//
	IRefCount* GetAIObj( SMapObject *pObj )
	{
		CVisAIMap::iterator pos = visai.find( pObj );
		return pos == visai.end() ? 0 : pos->second;
	}
	//
	CMapObjectsList* GetAllSegmentsByAI( IRefCount *pAIObj )
	{
		CAIVisMap::iterator pos = aivis.find( pAIObj );
		return pos == aivis.end() ? 0 : &( pos->second );
	}
	//
	CMapObjectsList* GetAllSegmentsByMO( SMapObject *pMO )
	{
		CVisAIMap::iterator pos = visai.find( pMO );
		return pos == visai.end() ? 0 : GetAllSegmentsByAI( pos->second );
	}
	//
	int operator&( IStructureSaver &ss );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CWorldObjectsIterator
{
	CMapObjectsMap::iterator it;
public:
	CWorldObjectsIterator() {  }
	CWorldObjectsIterator( const CMapObjectsMap::iterator &_it ) : it( _it ) {  }
	CWorldObjectsIterator( const CWorldObjectsIterator &_it ) : it( _it.it ) {  }
	//
	const CWorldObjectsIterator& operator=( const CWorldObjectsIterator &_it ) { it = _it.it; return *this; }
	bool operator==( const CWorldObjectsIterator &_it ) const { return it == _it.it; }
	bool operator!=( const CWorldObjectsIterator &_it ) const { return it != _it.it; }
	//
	operator const SMapObject*() const { return it->second; }
	const SMapObject* operator->() const { return it->second; }
	//
	void operator++() { ++it; }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CWorldBase
{
	DECLARE_SERIALIZE;
	struct SObjectSounds
	{
		WORD wSoundID;
		WORD wLoopedSoundID;
		SObjectSounds() : wSoundID( 0 ), wLoopedSoundID( 0 ) {  }
	};
	typedef std::unordered_map< CPtr<SMapObject>, SObjectSounds, SPtrHash > ObjectsSounds;
	ObjectsSounds objectsSounds;						// � ��������� �������� ���� �����, ������� ����� ������
	//
	typedef std::unordered_map< CPtr<IRefCount>, CPtr<IRefCount>, SDefaultPtrHash> CLinksMap;
	
	// update check values
	CVec3 vLastAnchor;										// ��������� ������� ������
	NTimer::STime warFogLastTime;					// ��������� ����� ���������� ������ �����
	// world object maps
	CMapObjectsMap aiobjects;							// ��� ������� ���� � AI ������� ��� �� �����
	CMapObjectsMap visobjects;						// ������������ Vis obj <=> map object ��� ��������� ���������� ��� ��������� ��������
	CMapObjectsList delayedRemoveAIObjes;	// list of objects for delayed remove from AI
	// bridges
	CBridgeSpanObjectsMap aispans;				// ��� ������ ������ � AI ������� ��� �� �����
	CBridgeSpanObjectsMap visspans;				// ������������ Vis obj <=> bridge span object
	//
	CComplexObjects entrenchments;				// ���������� ������ � ������ �������
	CLinksMap inContainer;								// units in containers. unit => container
	// delayed lists
	std::list< CPtr<IMOUnit> > updatable;	// units, which require update
	//
	bool bEnableAIInfo;										// enable to show (and retrieve from AI) info for the current screen
	//std::unordered_map<int, bool> showicons;
	// CRAP{ ��� ��������������� �������� ������� � �������� �������
	bool bForceRotation;
	// CRAP}
	// AI updates
	typedef void (CWorldBase::*AI_UPDATE)( const NTimer::STime &currTime );
	std::vector<AI_UPDATE> fnAIUpdates;
	//
	int nSeason;													// ����� (���� ����/����, ������ ����/����, ������)
	DWORD dwFlashFireColor;								// flash color diring firing
	DWORD dwFlashExpColor;								// flash color diring exploding
	// messages ������
	std::list<SGameMessage> messages;
protected:
	typedef CWorldObjectsIterator iterator;
	// shortcuts
	CPtr<IScene> pScene;									// shortcut to scene
	CPtr<IClientAckManager> pAckManager;	// -~- to client's acknowledgements manager
	CPtr<ICursor> pCursor;								// -~- to cursor
	CPtr<IInput> pInput;									// -~- to input
	CPtr<IAILogic> pAILogic;							// -~- to AI logic
	CPtr<ITransceiver> pTransceiver;			// -~- to transceiver
	CPtr<ICamera> pCamera;								// -~- to camera
	CPtr<IGFX> pGFX;											// -~- to grafix engine
	CPtr<ISFX> pSFX;											// -~- to sound engine
	CPtr<ISoundManager> pSMan;						// -~- to sound manager
	CPtr<IObjectsDB> pGDB;								// -~- to game database
	CPtr<IVisObjBuilder> pVOB;						// -~- to vis obj builder
	CPtr<IGameTimer> pTimer;							// -~- to game timer
	bool bAADetectedFlag;                 // to choose correct message to send about AA start
	void GetVisibilityRectBounds( const CTRect<float> &rcScreen, CVec2 *pvLT, CVec2 *pvRT, CVec2 *pvLB, CVec2 *pvRB );
	void GetAllObjectsByMatch( std::list<SMapObject*> &mapobjects, const SGDBObjectDesc *pDesc, const bool bSelectableOnly );
private:
	void AIUpdatePassability( const CVec2 &vLT, const CVec2 &vLB, const CVec2 &vRB, const CVec2 &vRT );
	void ChangeObjectHP( SMapObject *pMO, float fHP );
	// updates from AI
	void AIUpdateWarFog( const NTimer::STime &currTime );
	void AIUpdateNewObjects( const NTimer::STime &currTime );
	void AIUpdateRemoveObjects( const NTimer::STime &currTime );
	void AIUpdateNewUnits( const NTimer::STime &currTime );
	void AIUpdateNewProjectiles( const NTimer::STime &currTime );
	void AIUpdatePlacements( const NTimer::STime &currTime );
	void AIUpdateRPGParams( const NTimer::STime &currTime );
	void AIUpdateDiplomacy( const NTimer::STime &currTime );
	void AIUpdateActions( const NTimer::STime &currTime );
	void AIUpdateAiming( const NTimer::STime &currTime );
	void AIUpdateShots( const NTimer::STime &currTime );
	void AIUpdateDeadUnits( const NTimer::STime &currTime );
	void AIUpdateDeadProjectiles( const NTimer::STime &currTime );
	void AIUpdateHits( const NTimer::STime &currTime );
	void AIUpdateFeedbacks( const NTimer::STime &currTime );
	void AIUpdateAcknowledgemets( const NTimer::STime &currTime );
	void AIUpdateEntrances( const NTimer::STime &currTime );
	void AIUpdateEntrenchments( const NTimer::STime &currTime );
	void AIUpdateSquads( const NTimer::STime &currTime );
	void AIUpdateBridges( const NTimer::STime &currTime );
	void AIUpdateRevealCircles( const NTimer::STime &currTime );
	void AIUpdateAreas( const NTimer::STime &currTime );
	void AIUpdateCombatSituationInfo( const NTimer::STime &currTime );
	//
	void AIUpdateWarFogLocal();
protected:
	// iteration.
	// NOTE{ ��� ���������� ��� ���������. ��� ������������ ������ (!) ������� ������� � �������� ��.
	iterator begin() { return CWorldObjectsIterator( aiobjects.begin() ); }
	iterator end() { return CWorldObjectsIterator( aiobjects.end() ); }
	// NOTE}
	//
	virtual void ResetSelection( SMapObject *pMO ) = 0;
	virtual void RemoveFromSelectionGroup( SMapObject *pMO ) {  }
	virtual void UpdatePick( const CVec2 &vPos, const NTimer::STime &time, bool bForced ) {  }
	virtual void AddUnitToSelectionGroup( IMOUnit *pUnit, const int nSelectionGroupID ) {  }
	//
	void GetPos3( CVec3 *pPos, const CVec2 &pos ) const { pScene->GetPos3( pPos, pos ); }
	void GetPos3( CVec3 *pPos, float x, float y ) const { pScene->GetPos3( pPos, CVec2(x, y) ); }
	void GetPos2( CVec2 *pPos, const CVec3 &pos ) const { pScene->GetPos2( pPos, pos ); }
	void GetPos2( CVec2 *pPos, float x, float y, float z ) const { pScene->GetPos2( pPos, CVec3(x, y, z) ); }
	//
	// map objects section
	//
	// create map object
	SMapObject* CreateMapObject( IRefCount *pAIObj, int nDBID, int nFrameIndex, const float fNewHP );
	// create and add map object to internal structures. NOTE: don't add to the scene, return added object
	SMapObject* AddToWorld( IRefCount *pAIObj, int nDBID, int nFrameIndex = -1, const float fNewHP = 1 );
	bool AddToWorld( SMapObject *pMO );
	// add map object to scene
	void AddToScene( SMapObject *pMO, bool bOutbound = false, EObjGameType eGameType = SGVOGT_UNKNOWN );
	// remove map object from internal structures. NOTE: after this function any pointer to this SMapObject becomes invalid!!!
	void RemoveFromWorld( SMapObject *pMO );
	// remove map object from scene
	void RemoveFromScene( SMapObject *pMO );
	// completelly remove object
	void RemoveMapObj( SMapObject *pMO );
	void RemoveAIObj( SMapObject *pMO, bool bDelayed );
	//
	// spans section
	//
	// create bridge span
	SBridgeSpanObject* CreateSpanObject( int nDBID, int nFrameIndex, float fNewHP );
	// create and add bridge span to internal structures. NOTE: don't add to the scene, return added object
	SBridgeSpanObject* AddSpanToWorld( IRefCount *pAIObj, int nDBID, int nFrameIndex, float fNewHP );
	// add bridge span to scene
	void AddToScene( SBridgeSpanObject *pSpan );
	// remove bridge span from internal structures.
	void RemoveFromWorld( SBridgeSpanObject *pSpan );
	// remove bridge span from scene
	void RemoveFromScene( SBridgeSpanObject *pSpan );
	// completelly remove object
	void RemoveSpanObj( SBridgeSpanObject *pSpan ) { RemoveFromScene( pSpan ); RemoveFromWorld( pSpan ); }
	//
	// pick functions
	//
	SBridgeSpanObject* PickSpan( const CVec2 &vPos );
	//
	//
	//
	void ChangeModel( SMapObject *pMO, const char *pszModel, const NTimer::STime &time = -1 );
	//
	//
	bool ToggleAIInfo();
	bool ToggleSceneDepthComplexity() { return pScene->ToggleShow( SCENE_SHOW_DEPTH_COMPLEXITY ); }
	bool ToggleBoundingBoxes() { return pScene->ToggleShow( SCENE_SHOW_BBS ); }
	// CRAP{ ��� ��������������� �������� �������
	bool ToggleRotation() { bForceRotation = !bForceRotation; return bForceRotation; }
	// CRAP}
	//
	virtual void NewObjectAdded( SMapObject *pMO ) {  }
	void ShowIcons( int nID, bool bShow );
	//
	IRefCount* GetAIEntrenchment( SMapObject *pMO ) { return entrenchments.GetAIObj( pMO ); }
	SMapObject* GetContainer( SMapObject *pMO )
	{
		CLinksMap::iterator pos = inContainer.find( pMO->pAIObj );
		return pos == inContainer.end() ? 0 : FindByAI( pos->second );
	}
	void RemoveFromContainer( SMapObject *pMO )
	{
		inContainer.erase( pMO->pAIObj );
	}
	// squads
	void AddToSquad( IRefCount *pAISquad, IMOUnit *pUnit );
	//
	void AddMessage( const SGameMessage &msg ) { messages.push_back( msg ); }
	//
	void SetIcon( SMapObject *pMO, int nType );
	void RemoveIcon( SMapObject *pMO, int nType );
	//
	void AddUpdatableUnit( IMOUnit *pUnit ) { updatable.push_back( pUnit ); }
	virtual void ReportObjectiveStateChanged( int nObjective, int nState ) {  }
	void ReportReinforcementArrived();
	//
	void UpdateAllUnits();
public:
	CWorldBase();
	virtual ~CWorldBase();
	// mission start
	void Start();
	// set climato-geographical zone settings
	virtual void STDCALL SetSeason( int _nSeason );
	virtual int	 STDCALL GetSeason( ) { return nSeason; }
	const char* GetSeasonName() const;

	// startup initialization
	virtual void STDCALL Init( ISingleton *pSingleton );
	// remove all from all !!!
	virtual void STDCALL Clear();
	// general world update from AI
	virtual void STDCALL Update( const NTimer::STime &currTime );
	// manipulation
	void MoveObject( IVisObj *pObj, const CVec3 &vPos );
	//
	bool GetMessage( SGameMessage *pMsg )
	{
		if ( messages.empty() )
			return false;
		*pMsg = messages.front();
		messages.pop_front();
		return true;
	}
	// find map object by AI
	SMapObject* FindByAI( IRefCount *pObj )
	{
		CMapObjectsMap::iterator pos = aiobjects.find( pObj );
		//NI_ASSERT_SLOW_TF( pos != aiobjects.end(), NStr::Format("Can't find map object by AI (0x%x)", pObj), return 0 );
		return pos != aiobjects.end() ? pos->second : 0;
	}
	// find map object by Vis
	SMapObject* FindByVis( IVisObj *pObj )
	{
		CMapObjectsMap::iterator pos = visobjects.find( pObj );
		//NI_ASSERT_SLOW_TF( pos != visobjects.end(), NStr::Format("Can't find map object by Vis (0x%x)", pObj), return 0 );
		return pos != visobjects.end() ? pos->second : 0;
	}
	//
	bool IsExistByVis( IVisObj *pObj ) const { return visobjects.find( pObj ) != visobjects.end(); }
	bool IsExistByAI( IRefCount *pObj ) const { return aiobjects.find( pObj ) != aiobjects.end(); }
	bool IsInScene( SMapObject *pMO ) const	{ return pMO && IsExistByVis( pMO->pVisObj.GetPtr() ); }
	bool IsInWorld( SMapObject *pMO ) const	{ return pMO && IsExistByAI( pMO->pAIObj.GetPtr() ); }
	// find bridge span by AI
	SBridgeSpanObject* FindSpanByAI( IRefCount *pObj )
	{
		CBridgeSpanObjectsMap::iterator pos = aispans.find( pObj );
		//NI_ASSERT_SLOW_TF( pos != aiobjects.end(), NStr::Format("Can't find map object by AI (0x%x)", pObj), return 0 );
		return pos != aispans.end() ? pos->second : 0;
	}
	// find map object by Vis
	SBridgeSpanObject* FindSpanByVis( IVisObj *pObj )
	{
		CBridgeSpanObjectsMap::iterator pos = visspans.find( pObj );
		return pos != visspans.end() ? pos->second : 0;
	}
	//for selection form AI script
	virtual void Select( SMapObject *pMapObj ) {  }
	virtual void ResetSelectionOverridable( IVisObj *pObj = 0 ) {  }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WORLDBASE_H__
