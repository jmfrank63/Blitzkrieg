#ifndef __SCRIPT_FUNCTIONS_H__
#define __SCRIPT_FUNCTIONS_H__
#pragma ONCE
#include "..\..\LuaLib\Script.h"
#include "..\..\Formats\fmtMap.h"
#include "..\AIHashFuncs.h"
interface IUpdatableObj;
interface IScenarioUnit;
class CScripts
{
	DECLARE_SERIALIZE;

	static const int TIME_TO_CHECK_SUSPENDED_REINF;

	struct SScriptInfo
	{
		DECLARE_SERIALIZE;

		public:
			NTimer::STime period;
			NTimer::STime lastUpdate;
			int nRepetitions;

			std::string szName;

			SScriptInfo() : period( 0 ), lastUpdate( 0 ), nRepetitions( -1 ), szName( "" ) {}
	};

	Script script;

	std::string szScriptFile;
	
	std::unordered_map<int, SScriptInfo> activeScripts;
	std::unordered_map<std::string, int> name2script;

	std::unordered_map<int, std::list<CPtr<IUpdatableObj> > > groups;
	struct SReinforcementObject
	{
		DECLARE_SERIALIZE;
	public:
		SMapObjectInfo mapObject;
		CGDBPtr<SHPObjectRPGStats> pStats;
		CPtr<IScenarioUnit> pScenarioUnit;

		SReinforcementObject() { }
		SReinforcementObject( const SMapObjectInfo &_mapObject, const SHPObjectRPGStats *_pStats, IScenarioUnit *_pScenarioUnit )
			: mapObject( _mapObject ), pStats( _pStats ), pScenarioUnit( _pScenarioUnit ) { }
	};
	typedef std::list<SReinforcementObject> CReinfList;
	std::unordered_map<int, CReinfList> reinforcs;
	CReinfList suspendedReinforcs;
	CReinfList::iterator reinforcsIter;
	NTimer::STime lastTimeToCheckSuspendedReinforcs;

	std::unordered_map<int, int> reservePositions;

	std::unordered_map< int, int> groupUnits;
	
	std::unordered_map<int, SScriptInfo>::iterator segmIter;

	std::unordered_map<std::string, SScriptArea> areas;

	bool bKill;

	CPtr<IConsoleBuffer> pConsole;
	bool bShowErrors;

	bool ReadScriptFile();

	int KillActiveScript( const std::string szName );

	void DelInvalidBegin( const int targetId );

	void OutScriptError( const char *pszString );

	void SetNewLinksToReinforcement( CReinfList *pReinf, std::unordered_map<int, int> *pOld2NewLinks );
	bool CanLandWithShift( const SMapObjectInfo &mapObject, IObjectsDB *pIDB, CVec2 *pvShift );
	bool CanFormationLand( const SMapObjectInfo &mapObject, IObjectsDB *pIDB, const CVec2 &vShift = VNULL2 );
	bool CanUnitLand( const SMapObjectInfo &mapObject, IObjectsDB *pIDB, const CVec2 &vShift = VNULL2 );

	void LandReinforcementWithoutLandCheck( CReinfList *pReinf, const CVec2 &vShift );
	void LandSuspendedReiforcements();

	static int ProcessCommand( struct lua_State *state, const bool bPlaceInQueue );
	
	interface ICheckObjects
	{ 
		virtual bool IsGoodObj( class CExistingObject *pObj ) const = 0; 
	};
	int GetCheckObjectsInScriptArea( const SScriptArea &area, const interface ICheckObjects &check );
	
	void SendShowReinoforcementPlacementFeedback( std::list<CVec2> *pCenters );
public:
	~CScripts();

	int GetScriptID( IUpdatableObj *pObj ) const;
	void AddObjToScriptGroup( IUpdatableObj *pObj, const int nGroup );
	void AddUnitToReinforcGroup( const SMapObjectInfo &mapObject, const int nGroup, const struct SHPObjectRPGStats *pStats, IScenarioUnit *pScenarioUnit );
	void DelInvalidUnits( const int scriptId );
	
	void Init( const SLoadMapInfo &mapInfo );
	void InitAreas( const SScriptArea scriptAreas[], const int nLen );
	void Load( const std::string &szScriptFile );

	void Segment();

	void CallScriptFunction( const char *pszCommand );

	static int Error_out( struct lua_State *state );
	
	static int RunScript( struct lua_State *state ); 
	static int Suicide( struct lua_State *state );
	static int KillScript( struct lua_State *state );
	static int GetNUnitsInCircle( struct lua_State *state );
	static int GetNUnitsInArea( struct lua_State *state );
	static int GetNScriptUnitsInArea( struct lua_State *state );
	static int GetNUnitsInScriptGroup( struct lua_State *state );
	static int LandReinforcement( struct lua_State *state );
	static int Win( struct lua_State *state );
	
	static int Draw( struct lua_State *state );
	static int Loose( struct lua_State *state );
	static int GiveCommand( struct lua_State *state );

	static int GiveQCommand( struct lua_State *state );

	static int ShowActiveScripts( struct lua_State *state );
	
	static int ChangeWarFog( struct lua_State *state );

	static int EnableAviation( struct lua_State *state );

	static int DisableAviation( struct lua_State *state );

	static int ChangePlayer( struct lua_State *state );
	
	static int God( struct lua_State *state );

	static int SetIGlobalVar( struct lua_State *state );
	static int SetFGlobalVar( struct lua_State *state );
	static int SetSGlobalVar( struct lua_State *state );
	
	static int GetIGlobalVar( struct lua_State *state );
	static int GetFGlobalVar( struct lua_State *state );
	static int GetSGlobalVar( struct lua_State *state );
	
	static int GetObjectHPs( struct lua_State *state );

	static int GetNUnitsInParty( struct lua_State *state );
	static int GetNUnitsInPartyUF( struct lua_State *pState );
	static int GetNUnitsInPlayerUF( struct lua_State *pState );
	
	static int ChangeFormation( struct lua_State *state );
	
	static int Trace( struct lua_State *state );
	static int DisplayTrace( struct lua_State *state );
	
	static int ObjectiveChanged( struct lua_State *state );
	
	static int GetNAmmo( struct lua_State *state );
	
	static int GetPartyOfUnits( struct lua_State *state );
	
	static int ReserveAviationForTimes( struct lua_State *pState );
	
	static int DamageObject( struct lua_State *pState );

	static int GetUnitState( struct lua_State *pState );
	
	static int GetSquadInfo( struct lua_State *pState );
	
	static int IsFollowing( struct lua_State *pState );
	
	static int GetFrontDir( struct lua_State *pState );
	
	static int IsWarehouseConnected( struct lua_State *pState );

	static int IsUnitUnderSupply( struct lua_State *pState );
	static int GetUnitMorale( struct lua_State *pState );

	static int GetActiveShellType( struct lua_State *pState );

	static int AskClient( struct lua_State *pState );

	static int RandomFloat( struct lua_State *pState );
	static int RandomInt( struct lua_State *pState );

	static int ChangeSelection( struct lua_State *pState );
	
	static int GetPlayersMask( struct lua_State *pState );
	static int IsPlayerPresent( struct lua_State *pState );
	
	static int GetObjCoord( struct lua_State *pState );

	static int GetScriptAreaParams( struct lua_State *pState );
	
	static int SwitchWeather( struct lua_State *pState );
	static int SwitchWeatherAutomatic( struct lua_State *pState );

	static int GetNUnitsInSide( struct lua_State *pState );
	
	static int AddIronMan( struct lua_State *state );

	static int SetDifficultyLevel( struct lua_State *state );
	static int SetCheatDifficultyLevel( struct lua_State *state );

	static int DeleteReinforcement( struct lua_State *pState );
	
	static int ViewZone( struct lua_State *pState );
	
	static int IsStandGround( struct lua_State *pState );
	static int IsEntrenched( struct lua_State *pState );

	static int GetNAPFencesInScriptArea( struct lua_State *pState );
	static int GetNAntitankInScriptArea( struct lua_State *pState );
	static int GetNFencesInScriptArea( struct lua_State *pState );
	static int GetNTrenchesInScriptArea( struct lua_State *pState );
	static int GetNMinesInScriptArea( struct lua_State *pState );

	static int GetAviationState( struct lua_State *state );
	
	static int Password( struct lua_State *pState );
	static int ReturnScriptIDs( struct lua_State *pState );
	
	static int SetGameSpeed( struct lua_State *pState );

	static int GetNUnitsOfType( struct lua_State *pState );
	static int GetMapSize( struct lua_State *pState );
	
	static int CallAssert( struct lua_State *pState );
		
	static Script::SRegFunction pRegList[];
};
#endif // __SCRIPT_FUNCTIONS_H__
