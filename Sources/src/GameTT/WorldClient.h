#ifndef __WORLDCLIENT_H__
#define __WORLDCLIENT_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Common\WorldBase.h"
#include "..\Input\Input.h"
#include "..\Input\InputHelper.h"
#include "..\AILogic\AITypes.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EWorldClientCommands
{
	WCC_FORCE_ROTATION	= 0x00400001,
	WCC_PLACE_EFFECT		= 0x00400003,
	WCC_SHOW_AI_INFO		= 0x00400004,
	WCC_SHOW_HP_INFO		= 0x00400005,
	WCC_SHOW_HAZE				= 0x00400006,
	WCC_SHOW_NOISE			= 0x00400007,
	WCC_TEST_ANIMATIONS	= 0x00400008,
	//
	WCC_SELECT_BY_TYPE	= 0x00400009,
	//
	WCC_SHOW_FIRE_RANGES_ON			= 0x0040000a,
	WCC_SHOW_FIRE_RANGES_OFF		= 0x0040000b,
	WCC_SHOW_ZERO_AREAS_ON			= 0x0040000c,
	WCC_SHOW_ZERO_AREAS_OFF			= 0x0040000d,
	WCC_SHOW_STORAGE_RANGES_ON	= 0x0040000e,
	WCC_SHOW_STORAGE_RANGES_OFF	= 0x0040000f,
	//
	WCC_ASSIGN_UNNAMED_GROUP = 0x00400010,
	//
	WCC_ASSIGN_GROUP_0	= 0x00400011,
	WCC_ASSIGN_GROUP_1	= 0x00400012,
	WCC_ASSIGN_GROUP_2	= 0x00400013,
	WCC_ASSIGN_GROUP_3	= 0x00400014,
	WCC_ASSIGN_GROUP_4	= 0x00400015,
	WCC_ASSIGN_GROUP_5	= 0x00400016,
	WCC_ASSIGN_GROUP_6	= 0x00400017,
	WCC_ASSIGN_GROUP_7	= 0x00400018,
	WCC_ASSIGN_GROUP_8	= 0x00400019,
	WCC_ASSIGN_GROUP_9	= 0x0040001a,
	//
	WCC_SELECT_GROUP_0	= 0x00400021,
	WCC_SELECT_GROUP_1	= 0x00400022,
	WCC_SELECT_GROUP_2	= 0x00400023,
	WCC_SELECT_GROUP_3	= 0x00400024,
	WCC_SELECT_GROUP_4	= 0x00400025,
	WCC_SELECT_GROUP_5	= 0x00400026,
	WCC_SELECT_GROUP_6	= 0x00400027,
	WCC_SELECT_GROUP_7	= 0x00400028,
	WCC_SELECT_GROUP_8	= 0x00400029,
	WCC_SELECT_GROUP_9	= 0x0040002a,
	//
	WCC_CENTER_GROUP_0	= 0x00400031,
	WCC_CENTER_GROUP_1	= 0x00400032,
	WCC_CENTER_GROUP_2	= 0x00400033,
	WCC_CENTER_GROUP_3	= 0x00400034,
	WCC_CENTER_GROUP_4	= 0x00400035,
	WCC_CENTER_GROUP_5	= 0x00400036,
	WCC_CENTER_GROUP_6	= 0x00400037,
	WCC_CENTER_GROUP_7	= 0x00400038,
	WCC_CENTER_GROUP_8	= 0x00400039,
	WCC_CENTER_GROUP_9	= 0x0040003a,
	//
	WCC_ASSIGN_TERRAIN_0	= 0x00400040,
	WCC_ASSIGN_TERRAIN_1	= 0x00400041,
	WCC_ASSIGN_TERRAIN_2	= 0x00400042,
	WCC_ASSIGN_TERRAIN_3	= 0x00400043,
	WCC_ASSIGN_TERRAIN_4	= 0x00400044,
	WCC_ASSIGN_TERRAIN_5	= 0x00400045,
	WCC_ASSIGN_TERRAIN_6	= 0x00400046,
	WCC_ASSIGN_TERRAIN_7	= 0x00400047,
	WCC_ASSIGN_TERRAIN_8	= 0x00400048,
	WCC_ASSIGN_TERRAIN_9	= 0x00400049,
	//
	WCC_CENTER_TERRAIN_0	= 0x00400050,
	WCC_CENTER_TERRAIN_1	= 0x00400051,
	WCC_CENTER_TERRAIN_2	= 0x00400052,
	WCC_CENTER_TERRAIN_3	= 0x00400053,
	WCC_CENTER_TERRAIN_4	= 0x00400054,
	WCC_CENTER_TERRAIN_5	= 0x00400055,
	WCC_CENTER_TERRAIN_6	= 0x00400056,
	WCC_CENTER_TERRAIN_7	= 0x00400057,
	WCC_CENTER_TERRAIN_8	= 0x00400058,
	WCC_CENTER_TERRAIN_9	= 0x00400059,
	//
	WCC_UI_BUTTON1_1		= 0x00400101,
	WCC_UI_BUTTON1_2		= 0x00400102,
	WCC_UI_BUTTON1_3		= 0x00400103,
	WCC_UI_BUTTON1_4		= 0x00400104,
	WCC_UI_BUTTON1_5		= 0x00400105,
	WCC_UI_BUTTON1_6		= 0x00400106,
	WCC_UI_BUTTON1_7		= 0x00400107,
	WCC_UI_BUTTON1_8		= 0x00400108,
	WCC_UI_BUTTON1_9		= 0x00400109,
	WCC_UI_BUTTON1_10		= 0x0040010a,
	WCC_UI_BUTTON1_11		= 0x0040010b,
	WCC_UI_BUTTON1_12		= 0x0040010c,
	//
	// to enable UI to talk with Multiplayer
	WCC_MULTIPLAYER_TO_UI_UPDATE = 0x00400213,
	//
	// Passenger squad selected/deselected
	WCC_UI_SQUAD_SEL    = 0x00400214,
	WCC_UI_SQUAD_DESEL  = 0x00400215,
	// 
	WCC_OBJECTIVES_CLOSED		= 0x00400216,
	WCC_SHOW_LAST_OBJECTIVE	= 0x00400217,
	WCC_HIDE_OBJECTVE_WINDOW = 0x00400218,
	//
	WCC_FORCE_DWORD = 0x7fffffff
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const DWORD ACTION_FLAG_FOE			= 0x00000001;
const DWORD ACTION_FLAG_FRIEND	= 0x00000002;
const DWORD ACTION_FLAG_NEUTRAL	= 0x00000004;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetPassangers( const IMOContainer *pContainer, std::vector<IMOUnit*> &passangers, const bool bCanSelectOnly = false );
void GetPassangers( const CMapObjectsList &containers, std::vector<IMOUnit*> &passangers, const bool bCanSelectOnly = false );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** selection group [0..9]
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSelectionGroup
{
	typedef std::list< CPtr<IMOUnit> > CUnitsList;
	CUnitsList units;											// units in this group
	int nVisGroupID;											// visual group ID
	//
	void Remove( SMapObject *pMO ) 
	{ 
		IMOUnit *pUnit = static_cast<IMOUnit*>( pMO );
		pUnit->AssignSelectionGroup( -1 );
		units.remove( pUnit ); 
	}
	void Add( SMapObject *pMO )
	{
		IMOUnit *pUnit = static_cast<IMOUnit*>( pMO );
		pUnit->AssignSelectionGroup( nVisGroupID );
		units.push_back( pUnit );
	}
	void Clear()
	{
		for ( CUnitsList::iterator it = units.begin(); it != units.end(); ++it )
			(*it)->AssignSelectionGroup( -1 );
		units.clear();
	}
	const bool IsEmpty() const { return units.empty(); }
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &units );
		saver.Add( 2, &nVisGroupID );
		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct STerrainSelectionPoint
{
	CVec3 vPos;
	int nID;
	//
	STerrainSelectionPoint() : vPos( VNULL3 ), nID( -1 ) {  }
	STerrainSelectionPoint( const CVec3 &_vPos, const int _nID )
		: vPos( _vPos ), nID( _nID ) {  }
	//
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &vPos );
		saver.Add( 2, &nID );
		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** class CSelector - objects selection
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSelector : public ISelector
{
	CPtr<ITransceiver> pTransceiver;			// transceiver
	//
	CMapObjectsList objects;							// currently selected units
	CMapObjectsSet objset;								// seleted objects set for a fast checking
	int nSelectionGroupID;								// AI selection group
	bool bValid;													// is current selection valid?
	CMapObjectsList addedObjects;					// objects which was added before Register() call
	//
	std::vector<SSelectionGroup> groups;	// selection groups
	//
	typedef std::list< CObj<IUIElement> > CUIElementsList;
	CUIElementsList squads;								// who-in-container interfaces
	//
	int Register();
	void UnRegister();
	// is current selection valid (up-to-date AI registered)
	bool IsValid() const { return bValid; }
	void ClearWhoInContainer();
public:
	CSelector() 
		: nSelectionGroupID( -1 ), bValid( false ), groups( 10 )
	{  
		for ( int i = 0; i < groups.size(); ++i )
			groups[i].nVisGroupID = i;
	}
	CSelector( ITransceiver *pTrans )
		: pTransceiver( pTrans ), nSelectionGroupID( -1 ), bValid( false ), groups( 10 )
	{  
		for ( int i = 0; i < groups.size(); ++i )
			groups[i].nVisGroupID = i;
	}
	// access to objects
	int size() const { return objects.size(); }
	const CPtr<SMapObject>& back() const { return objects.back(); }
	const CPtr<SMapObject>& front() const { return objects.front(); }
	const CMapObjectsList& GetObjects() const { return objects; }
	//
	//
	void SetTransceiver( ITransceiver *pTrans ) { pTransceiver = pTrans; }
	// select single map object
	bool STDCALL Select( struct SMapObject *pMO, bool bSelect, bool bSelectSuper );
	bool STDCALL IsSelected( const SMapObject *pMO ) const { return objset.find( const_cast<SMapObject*>(pMO) ) != objset.end(); }
	// done multiple selection operation
	void STDCALL DoneSelection();
	// register/unregister group
	int STDCALL GetAIGroup() { return Register(); }
	bool STDCALL IsEmpty() const { return objects.empty(); }
	void STDCALL Invalidate() { UnRegister(); bValid = false; }
	// visiting objects inside
	void STDCALL Visit( ISelectorVisitor *pVisitor ) const;
	//	
	void SendAcknowledgement( interface IAILogic *pAILogic );
	void UpdateSelection( IMOContainer *pContainer );
	//
	SSelectionGroup& GetSelectionGroup( const int nIndex ) { return groups[nIndex]; }
	// serialization
	int operator&( IStructureSaver &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** main WorldClient class
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CWorldClient : public CWorldBase
{
	DECLARE_SERIALIZE;
	//
	typedef std::pair< CPtr<IText>, CPtr<IText> > SObjectiveText;
	// actions
	typedef bool (CWorldClient::*USER_ACTION)( const SGameMessage &msg, bool bForced );
	struct SActionDesc
	{
		enum
		{
			AUTO		= 0x00000001,							// action can be initiated with respect to current pick and selection
			FORCED	= 0x00000002,							// action can be activated only from interface
			INSTANT	= 0x00000004							// action will be instantly performed in the moment of initiation
		};
		//
		USER_ACTION pfnAction;
		DWORD flags;
	};
	typedef std::unordered_map<int, SActionDesc> CActionsMap;
	CActionsMap userActions;							// user action functions
	//
	typedef std::list<STerrainSelectionPoint> CTerrainPointsList;
	CTerrainPointsList terrainPoints;			// terrain selection points
	//
	bool bShowHPs;												// show HP bars
	int nForcedAction;										// current forced action
	int nAutoAction;											// current best automatic action
	bool bActionModifierAdd;							// add this action to the queue
	bool bActionModifierForcedMove;				// force move commands serie
	bool bActionModifierForcedAttack;			// forced attack commands serie
	bool bShowMultipleMarkers;
	bool bCombineRotation;								// "move and rotate" command variables
	float fRotationStartAngle;
	bool bGotMoveCommand;
	bool bGotGridCommand;
	CVec3 vRotationalMovePos;
	float fMinRotShiftSq;
	std::list< CPtr<IVisObj> > hiddenObjects;
	bool bSetPlayerTooltip;               // set when player tooltip might be shown
	//
	bool bCheckDiplomacy;									// do we need check diplomacy during FoF operations ? (for editor mode)
	// selections...
	typedef std::vector<SSelectionGroup> CSelectionsList;
	CSelectionsList selectionGroups;			// selection groups
	CSelector selunits, selbuildings;			// selected units & buildings
	CMapObjectsList preselectedObjects;		// pre-selected objects (�.�. �������, ������� �������� ��������, �� ��� �� �������)
	CUserActions availActions;						// current available actions (of the current selection)
	CUserActions availCrossActions;				// actions, which one can do on the current point
	CUserActions lastActions;							// available actions from the last frame (to compare)
	CUserActions availPlanes;							// available airplane calls
	CUserActions selfActions;							// self actions (the same as 'userPrioritySelf', but packed to the 'CUserActions' class)
	std::vector<int> userPriorityFriendly;// user auto actions checks priority with friendly unit
	std::vector<int> userPriorityNeutral;	// user auto actions checks priority with neutral unit
	std::vector<int> userPriorityEnemy;		// user auto actions checks priority with enemy unit
	std::vector<int> userPrioritySelf;		// user self actions priority
	std::vector<int> excludeFriendly;			// exclude actions with friendly unit
	std::vector<int> excludeNeutral;			// exclude actions with neutral unit
	std::vector<int> excludeEnemy;				// exclude actions with enemy unit
	//
	CMapObjectsPtrList framePick;					// all map objects, picked in this frame under cursor
	NTimer::STime timeLastPick;						// time, pick was updated
	CVec2 vLastPickPos;										// last pick position
	//
	int	nLastAvailableActionsSet;					// last available actions set - to change 3x3 button sets
	//
	typedef std::list<SAIUnitCmd> CAIUnitCmdList;
	CAIUnitCmdList aviationPoints;				// temporal points for aviation call
	CAIUnitCmdList fencePoints;						// temporal points for fence/entrenchment
	CAIUnitCmdList buildPoints;						// temporal points for build/set mines
	CPtr<IBoldLineVisObj> pBoldLine;			// line fr fences/entrenchments

	typedef std::list<CCircle> CAviationMarkers;
	CAviationMarkers aviationCircles;			// aviation markers
	// objectives queue
	typedef std::list<SObjectiveText> CObjectivesList;
	CObjectivesList objectives;						// objective textes to display
	int nCurrObjective;										// currently shown objective
	SObjectiveText lastObjective;						// last objective, which was displayed
	bool bCanShowNextObjective;						// can I show next objective?
	// input messages
	NInput::CCommandRegistrator worldClientMsgs;
	//
	bool bAviationLocked;                 // aviation appear point
	CVec2 vAviationAppear;
	int nMultiplayer;
	// check diplomacy
	bool IsAllied( BYTE diplomacy ) const { return diplomacy == EDI_FRIEND; }
	// selection
	void ResetAIGroup( int *pnGroupID )
	{
		if ( *pnGroupID != -1 )
			pTransceiver->CommandUnregisterGroup( *pnGroupID );
		*pnGroupID = -1;
	}
	//
	STerrainSelectionPoint* GetTerrainPoint( const int nID )
	{
		for ( CTerrainPointsList::iterator it = terrainPoints.begin(); it != terrainPoints.end(); ++it )
		{
			if ( it->nID == nID ) 
				return &( *it );
		}
		return 0;
	}
	// selection groups internal functions
	SSelectionGroup& GetSelectionGroup( int nIndex ) { return selectionGroups[nIndex]; }
	void ClearSelectionGroup( int nIndex );
	void AssignSelectionGroup( int nIndex );
	void ActivateSelectionGroup( int nIndex, bool bMerge );
	void CenterSelectionGroup( int nIndex );
	virtual void RemoveFromSelectionGroup( SMapObject *pMO );
	void SelectType( const CVec2 &vPos );
	void SelectType( SMapObject *pMO, std::vector<IVisObj*> &objects );
	virtual void AddUnitToSelectionGroup( IMOUnit *pUnit, const int nSelectionGroupID );
	//
	void SetCheckDiplomacy( bool _bCheckDiplomacy ) { bCheckDiplomacy = _bCheckDiplomacy; }
	bool IsFriend( const SMapObject *pMO ) const { return pMO->diplomacy == EDI_FRIEND; }
	bool IsFoe( const SMapObject *pMO ) const { return pMO->diplomacy == EDI_ENEMY; }
	bool IsNeutral( const SMapObject *pMO ) const { return pMO->diplomacy == EDI_NEUTRAL; }
	EDiplomacyInfo GetDiplomacyRelation( const SMapObject *pMO ) const { return EDiplomacyInfo( pMO->diplomacy ); }
	//
	bool IsUnitsSelected() const { return !selunits.IsEmpty(); }
	bool IsBuildingsSelected() const { return !selbuildings.IsEmpty(); }
	bool IsSelectionEmpty() const { return selunits.IsEmpty(); }
	void Select( CMapObjectsPtrList &mapObjects, bool bMerge );
	void SelectBuilding( IVisObj *pObj, bool bAddAction );
	virtual void ResetSelection( SMapObject *pMO );
	void PickFoF( const CVec2 &vPos, EObjGameType type, CMapObjectsPtrList &friends, CMapObjectsPtrList &foes, CMapObjectsPtrList &neutrals );
	void Pick( const CVec2 &vPos, EObjGameType type, CMapObjectsPtrList &objects );
	void PickAll( const CVec2 &vPos, CMapObjectsPtrList &objects, EObjGameType upto = SGVOGT_MINE, bool bVisible = false, bool bAliveUnits = true, bool bAliveOther = true );
	virtual void UpdatePick( const CVec2 &vPos, const NTimer::STime &time, bool bForced, bool bAliveUnits = true, bool bAliveOther = true );
	const SMapObject* GetFirstPick( const EObjGameType upto, const bool bAliveUnits, bool bAliveOther ) const;
	// commands
	bool UnitAutoCommand( const CVec2 &vPos, bool bAddAction );
	bool SetForcedAction( int nAction );
	void ResetForcedAction( int nAction = -1 );
	// action mode
	void SetAutoAction( int nAction );
	void SetCursorMode( const int nAction, const bool bModifier = false );
	// feedback
	void SetStatusBar( const SMapObject *pMO );
	//
	virtual void NewObjectAdded( SMapObject *pMO );
	void UpdateWhoInContainerInterface();
	// actions
	void CalcAvailableActionsSet( CUserActions *pActions, const IMapObj *pException = 0, const IMapObj::EActionsType eActions = IMapObj::ACTIONS_BY ) const;
	bool CanDoAction( int nAction ) const;
	void CheckActions();
	//int DetermineAction( const SMapObject *pMO, bool bForced = false ) const;
	int DetermineBestAutoAction( const SMapObject *pMO );
	void ExcludeActions( CUserActions *pActions, const SMapObject *pMO ) const;
	void SetAvailablePlanes( int nPlaneType, int nTime, bool bEnable );
	//
	void RegisterAction( int nAction, DWORD flags, USER_ACTION pfnUserAction );
	SActionDesc* GetAction( int nAction )
	{
		CActionsMap::iterator pos = userActions.find( nAction );
		return pos != userActions.end() ? &( pos->second ) : 0;
	}
	const SActionDesc* GetAction( int nAction ) const
	{
		CActionsMap::const_iterator pos = userActions.find( nAction );
		return pos != userActions.end() ? &( pos->second ) : 0;
	}
	USER_ACTION GetAction( int nAction, int nType )
	{
		if ( SActionDesc *pDesc = GetAction( nAction ) )
			return pDesc->flags & nType ? pDesc->pfnAction : 0;
		return 0;
	}
	bool DoAction( int nAction, int nType, const SGameMessage &msg )
	{
		USER_ACTION pfnAction = GetAction( nAction, nType );
		return pfnAction != 0 ? (this->*pfnAction)( msg, (nType & SActionDesc::FORCED) != 0 ) : false;
	}
	bool HasAction( int nAction, int nType ) const
	{
		if ( const SActionDesc *pDesc = GetAction( nAction ) )
			return ( pDesc->flags & nType ) && pDesc->pfnAction;
		return false;
	}
	//
	const CVec2 GetPosFromMsg( const SGameMessage &msg ) const
	{
		if ( (msg.nParam & 0xc0000000) == 0 )	// do action by current cursor coordinates
			return pCursor->GetPos();
		else if ( msg.nParam & 0x40000000 )
			return CVec2( msg.nParam & 0x7fff, (msg.nParam >> 15) & 0x7fff );
		else																	// do action by particular coordinates, encoded in the low (x) and high (y) word of the message parameter
		{
			CVec2 vPos2;
			GetPos2( &vPos2, DWORD(msg.nParam) & 0x00007fff, (DWORD(msg.nParam) >> 15 ) & 0x00007fff, 0 );
			return vPos2;
		}
	}
	//
	bool DoCommandsList( CAIUnitCmdList &cmds );
	//
	void PrepareActionPosParam( SAIUnitCmd *pCmd, int nAction, const CVec2 &vPos, float fParam );
	void PrepareActionObjParam( SAIUnitCmd *pCmd, int nAction, IRefCount *pAIObj, float fParam );
	bool PerformActionObjParam( int nAction, IRefCount *pAIObj, float fParam, bool bAddAction );
	bool PerformActionObj( int nAction, IRefCount *pAIObj, bool bAddAction ) { return PerformActionObjParam(nAction, pAIObj, 0, bAddAction); }
	bool PerformActionPosParam( int nAction, const CVec2 &vPos, float fParam, bool bAddAction, bool bMarkOnTerrain = true );
	bool PerformActionPosParam( int nAction, const CVec3 &vPos, float fParam, bool bAddAction, bool bMarkOnTerrain = true );
	bool PerformActionPos( int nAction, const CVec2 &vPos, bool bAddAction ) { return PerformActionPosParam(nAction, vPos, 0, bAddAction); }
	int PerformUnitActionPosParam( int nAction, const CVec2 &vPos, float fParam, bool bAddAction );
	//
	bool PerformSelfActionPosParam( SMapObject *pMO, const int nAction, const int nParam, bool bAddAction );
	bool PerformSelfActionPos( SMapObject *pMO, const int nAction, bool bAddAction ) { return PerformSelfActionPosParam(pMO, nAction, 0, bAddAction); }
	bool DoSelfAction( SMapObject *pMO, const int nAction );
	bool DoSelfAction( const int nAction, const bool bAddAction );
	bool ActionDoSelfActionMsg( const SGameMessage &msg, bool bForced );
	//
	bool ActionMoveMsg( const SGameMessage &msg, bool bForced );
	bool ActionMoveToGridMsg( const SGameMessage &msg, bool bForced );
	bool ActionSwarmMsg( const SGameMessage &msg, bool bForced );
	bool ActionRotateMsg( const SGameMessage &msg, bool bForced );
	bool ActionAttackMsg( const SGameMessage &msg, bool bForced );
	bool ActionBoardMsg( const SGameMessage &msg, bool bForced );
	bool ActionLeaveMsg( const SGameMessage &msg, bool bForced );
	bool ActionInstallMsg( const SGameMessage &msg, bool bForced );
	bool ActionUnInstallMsg( const SGameMessage &msg, bool bForced );
	bool ActionCaptureArtilleryMsg( const SGameMessage &msg, bool bForced );
	bool ActionHookArtilleryMsg( const SGameMessage &msg, bool bForced );
	bool ActionDeployArtilleryMsg( const SGameMessage &msg, bool bForced );
	bool ActionFollowMsg( const SGameMessage &msg, bool bForced );
	bool ActionEntrenchSelfMsg( const SGameMessage &msg, bool bForced );
	bool ActionStandGroundMsg( const SGameMessage &msg, bool bForced );
	
	bool ActionPlaceMineAPMsg( const SGameMessage &msg, bool bForced );
	bool ActionPlaceMineATMsg( const SGameMessage &msg, bool bForced );
	bool ActionClearMineMsg( const SGameMessage &msg, bool bForced );
	bool ActionBuildFenceMsg( const SGameMessage &msg, bool bForced );
	bool ActionBuildEntrenchmentMsg( const SGameMessage &msg, bool bForced );
	bool ActionBuildAntiTank( const SGameMessage &msg, bool bForced );
	bool ActionBuildBridgeMsg( const SGameMessage &msg, bool bForced );
	bool ActionRepairMsg( const SGameMessage &msg, bool bForced );
	bool ActionRepairBuildingMsg( const SGameMessage &msg, bool bForced );
	bool ActionResupplyMsg( const SGameMessage &msg, bool bForced );
	bool ActionResupplyHRMsg( const SGameMessage &msg, bool bForced );
	//bool ActionBuildRUStorageMsg( const SGameMessage &msg, bool bForced );
	bool ActionFillRU( const SGameMessage &msg, bool bForced );

	bool ActionGuardMsg( const SGameMessage &msg, bool bForced );
	bool ActionAmbush( const SGameMessage &msg, bool bForced );
	bool ActionRangingMsg( const SGameMessage &msg, bool bForced );
	bool ActionSuppressMsg( const SGameMessage &msg, bool bForced );
	bool ActionLookAtBinocularsMsg( const SGameMessage &msg, bool bForced );
	bool ActionCallHQMsg( const SGameMessage &msg, bool bForced );
	bool ActionStopMsg( const SGameMessage &msg, bool bForced );
	bool ActionChangeFormation( const SGameMessage &msg, bool bForced );
	bool ActionChangeShellTypeMsg( const SGameMessage &msg, bool bForced );

	bool ActionCallForBombersMsg( const SGameMessage &msg, bool bForced );
	bool ActionCallForFightersMsg( const SGameMessage &msg, bool bForced );
	bool ActionCallForSpyMsg( const SGameMessage &msg, bool bForced );
	bool ActionCallForParadropersMsg( const SGameMessage &msg, bool bForced );
	bool ActionCallForGunplanesMsg( const SGameMessage &msg, bool bForced );
	bool ActionAviationCall();
	bool ActionAviationAddPoint( const EActionCommand cmdType, const CVec2 &vPos );

	// in multiplayer only
	bool ActionPlaceMarkerMsg( const SGameMessage &msg, bool bForced );

//	bool ActionAssignToGunMsg( const SGameMessage &msg, bool bForced );

	bool ActionDisbandSquadMsg( const SGameMessage &msg, bool bForced );
	bool ActionFormSquadMsg( const SGameMessage &msg, bool bForced );	
	bool ActionChangeFormationMsg( const SGameMessage &msg, bool bForced );
	//
	bool ActionAttack( const CVec2 &vPos, DWORD flags, bool bAddAction );
	bool ActionSendTo( const CVec2 &vPos, bool bAddAction );
	bool ActionBoard( CMapObjectsPtrList &objects, bool bAddAction );
	bool ActionBoard( SMapObject *pMO, bool bAddAction );
	bool ActionLeave( const CVec2 &vPos, bool bAddAction );
	bool ActionFollow( SMapObject *pMO, bool bAdd );
	// sounds
	void PlaySuccessSound();
	void PlayFailedSound();
	// objectives
	virtual void ReportObjectiveStateChanged( int nObjective, int nState );
	void AddAviationMarker( const CVec2 &vPos );
	void ClearAviationMarkers();
	//
	void ResetPoints();
	//
	void MarkSelectedUnits( const CVec3 &vPos, bool bFadingMark = true );
public:
	CWorldClient();
	// startup initialization
	virtual void STDCALL Init( ISingleton *pSingleton );
	// selection
	void Select( const CVec2 &vPos );
	int Select( IVisObj **objects, int nNumObjects );
	//for selection from AI script
	virtual void Select( SMapObject *pMapObj );
	void ResetSelection( IVisObj *pObj = 0 );
	virtual void ResetSelectionOverridable( IVisObj *pObj = 0 ) { ResetSelection( pObj ); };
	void PreSelect( IVisObj **objects, int nNumObjects );
	void ResetPreSelection( IVisObj *pObj = 0 );
	// action...
	virtual void STDCALL OnMouseMove( const CVec2 &vPos, interface IUIElement *pUIPickElement );
	void BeginAction( const SGameMessage &msg );
	void DoAction( const SGameMessage &msg );
	// message processing
	bool ProcessMessage( const SGameMessage &msg );
	void GetAviationCircles( interface IUIMiniMap * pMInimap, const NTimer::STime curTime );
	virtual void STDCALL Update( const NTimer::STime &currTime );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CWorldClientBridge : public IWorldClient, private CWorldClient
{
	OBJECT_NORMAL_METHODS( CWorldClientBridge );
	DECLARE_SUPER( CWorldClient );
	// serialization
	virtual int STDCALL operator&( IStructureSaver &ss ) { return CSuper::operator&( ss ); }
	// set climato-geographical zone settings
	virtual void STDCALL SetSeason( int _nSeason ) { CSuper::SetSeason( _nSeason ); }
	// startup initialization (mission begin)
	virtual void STDCALL Init( ISingleton *pSingleton ) { CSuper::Init(pSingleton); }
	// remove all from all !!!
	virtual void STDCALL Clear() { CSuper::Clear(); }
	// general world update from AI
	virtual void STDCALL Update( const NTimer::STime &currTime ) { CSuper::Update(currTime); }
	// message processing
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg ) { return CSuper::ProcessMessage( msg ); }
	virtual bool STDCALL GetMessage( SGameMessage *pMsg ) { return CSuper::GetMessage( pMsg ); }
	// mission start
	virtual void STDCALL Start() { CSuper::Start(); }
	// selection
	virtual void STDCALL Select( const CVec2 &vPos ) { CSuper::Select( vPos ); }
	virtual int STDCALL Select( IVisObj **objects, int nNumObjects ) { return CSuper::Select(objects, nNumObjects); }
	virtual void STDCALL ResetSelection( IVisObj *pObj = 0 ) { CSuper::ResetSelection(pObj); }
	virtual void STDCALL PreSelect( IVisObj **objects, int nNumObjects ) { CSuper::PreSelect(objects, nNumObjects); }
	virtual void STDCALL ResetPreSelection( IVisObj *pObj = 0 ) { CSuper::ResetPreSelection(pObj); }
	// manipulation
	virtual void STDCALL MoveObject( IVisObj *pObj, const CVec3 &vPos ) { CSuper::MoveObject(pObj, vPos); }
	// action...
	virtual void STDCALL OnMouseMove( const CVec2 &vPos, interface IUIElement *pUIPickElement ) { CSuper::OnMouseMove( vPos, pUIPickElement ); }
	virtual void STDCALL BeginAction( const SGameMessage &msg ) { CSuper::BeginAction(msg); }
	virtual void STDCALL DoAction( const SGameMessage &msg ) { CSuper::DoAction(msg); }
	virtual void STDCALL GetAviationCircles( interface IUIMiniMap * pMinimap, const NTimer::STime curTime ) { CSuper::GetAviationCircles( pMinimap, curTime ); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WORLDCLIENT_H__
