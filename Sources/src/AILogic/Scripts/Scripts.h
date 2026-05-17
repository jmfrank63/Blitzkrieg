#ifndef __SCRIPT_FUNCTIONS_H__
#define __SCRIPT_FUNCTIONS_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\..\LuaLib\Script.h"
#include "..\..\Formats\fmtMap.h"
#include "..\AIHashFuncs.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IUpdatableObj;
interface IScenarioUnit;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	
	// �������� �������
	std::unordered_map<int, SScriptInfo> activeScripts;
	// �� ����� ������� - �� ���
	std::unordered_map<std::string, int> name2script;

	// ����� ������ - �����
	std::unordered_map<int, std::list<CPtr<IUpdatableObj> > > groups;
	// ����� reinforcement - reinforcement object
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
	// ���������� (������ ���������) ������������
	CReinfList suspendedReinforcs;
	CReinfList::iterator reinforcsIter;
	NTimer::STime lastTimeToCheckSuspendedReinforcs;

	std::unordered_map<int, int> reservePositions;

	// ���� - ����� ���������� ������
	std::unordered_map< int, int> groupUnits;
	
	// ��� ��������
	std::unordered_map<int, SScriptInfo>::iterator segmIter;

	std::unordered_map<std::string, SScriptArea> areas;

	bool bKill;

	CPtr<IConsoleBuffer> pConsole;
	bool bShowErrors;

	//
	bool ReadScriptFile();

	int KillActiveScript( const std::string szName );

	// ������� ��� ���������� ����� � ������ ������ ������
	void DelInvalidBegin( const int targetId );

	// ������� ��������� �� ������
	void OutScriptError( const char *pszString );

	// ���������� ����� ����� ������������
	void SetNewLinksToReinforcement( CReinfList *pReinf, std::unordered_map<int, int> *pOld2NewLinks );
	//
	bool CanLandWithShift( const SMapObjectInfo &mapObject, IObjectsDB *pIDB, CVec2 *pvShift );
	bool CanFormationLand( const SMapObjectInfo &mapObject, IObjectsDB *pIDB, const CVec2 &vShift = VNULL2 );
	bool CanUnitLand( const SMapObjectInfo &mapObject, IObjectsDB *pIDB, const CVec2 &vShift = VNULL2 );

	void LandReinforcementWithoutLandCheck( CReinfList *pReinf, const CVec2 &vShift );
	void LandSuspendedReiforcements();

	//
	static int ProcessCommand( struct lua_State *state, const bool bPlaceInQueue );
	
	//	
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
	// ������� ��� ���������� ����� � ������, 
	void DelInvalidUnits( const int scriptId );
	
	void Init( const SLoadMapInfo &mapInfo );
	void InitAreas( const SScriptArea scriptAreas[], const int nLen );
	void Load( const std::string &szScriptFile );

	void Segment();

	void CallScriptFunction( const char *pszCommand );

	//
	// script functions
	//
	static int Error_out( struct lua_State *state );
	
	//
	// params: <"name of script"> <periodicity> [<number of repetitions>]; returns: none;
	static int RunScript( struct lua_State *state ); 
	// params: none; returns: none;
	static int Suicide( struct lua_State *state );
	// params: <"name of script">; returns: nonw;
	static int KillScript( struct lua_State *state );
	//
	// params: <number of player> <x coord of the circle> <y coord> <radius>; returns: number of units;
	static int GetNUnitsInCircle( struct lua_State *state );
	//
	// params: <number of player> <"name of script area">; returns: number of units;
	static int GetNUnitsInArea( struct lua_State *state );
	//
	// params: <number of script group> <"name of script area">; returns: number of units;
	static int GetNScriptUnitsInArea( struct lua_State *state );
	//
	// params: <number of script group>; returns: number of units;
	static int GetNUnitsInScriptGroup( struct lua_State *state );
	//	
	// params: <number of reinforcement>; returns: none;
	static int LandReinforcement( struct lua_State *state );
	//
	// params: <number of winner party>; returns: none;
	static int Win( struct lua_State *state );
	
	// no params
	static int Draw( struct lua_State *state );
	//
	// params: none; returns: none;
	static int Loose( struct lua_State *state );
	//
	// params: <command> <script group id> <necessary command parameters>; returns: none;
	static int GiveCommand( struct lua_State *state );

	// params: <command> <script group id> <necessary command parameters>; returns: none;
	static int GiveQCommand( struct lua_State *state );

	// params: none; returns: none;
	static int ShowActiveScripts( struct lua_State *state );
	
	// params: <party of warfog>; returns: none;
	static int ChangeWarFog( struct lua_State *state );

	// params: <party of player : -1 - anyplayer> <type of aviation: -1 - any aviation>; returns: none;
	static int EnableAviation( struct lua_State *state );

	// params: <party of player : -1 - anyplayer> <type of aviation: -1 - any aviation>; returns: none;
	//double function because of compatibility.
	static int DisableAviation( struct lua_State *state );

	// params: <number of script group> <number of new player>; returns: none;
	static int ChangePlayer( struct lua_State *state );
	
	// params: <number of player> <number of mode>; returns: none;
	// nMode = 0 - ����� god mode ���������
	// nMode = 1 - �������������
	// nMode = 2 - ������������� � �������� � ������� ����
	// nMode = 3 - �������� � ������� ����
	// nMode = 4 - ����� ������ �������������
	// nMode = 5 - ����� ������ �������� � ������� ����
	static int God( struct lua_State *state );

	// params: <name of global var> <integer value of global var>; returns: none;
	static int SetIGlobalVar( struct lua_State *state );
	// params: <name of global var> <float value of global var>; returns: none;
	static int SetFGlobalVar( struct lua_State *state );
	// params: <name of global var> <string value of global var>; returns: none;
	static int SetSGlobalVar( struct lua_State *state );
	
	// params: <name of global var> <default interger value of global var>; returns: <integer value of global var>;
	static int GetIGlobalVar( struct lua_State *state );
	// params: <name of global var> <default float value of global var>; returns: <float value of global var>;
	static int GetFGlobalVar( struct lua_State *state );
	// params: <name of global var> <default string value of global var>; returns: <string value of global var>;
	static int GetSGlobalVar( struct lua_State *state );
	
	// params: <object's script id>; returns: <float value of hps>
	static int GetObjectHPs( struct lua_State *state );

	// params: <number of party>; returns: <number of units in party>
	static int GetNUnitsInParty( struct lua_State *state );
	// �������� ��������� �� ���� ����
	static int GetNUnitsInPartyUF( struct lua_State *pState );
	// �������� ��������� �� ���� ����
	static int GetNUnitsInPlayerUF( struct lua_State *pState );
	
	// params: <script id of squad> <number of new formation>; returns: none;
	static int ChangeFormation( struct lua_State *state );
	
	// params: <format string> <float parameter>...<float parameter>; returns: <none>
	// out trace info to the console
	static int Trace( struct lua_State *state );
	// params: <format string> <float parameter>...<float parameter>; returns: <none>
	// out trace info on display
	static int DisplayTrace( struct lua_State *state );
	
	// params: <number of objective> <new value of objective>; returns: none;
	static int ObjectiveChanged( struct lua_State *state );
	
	// params: <script id of unit>; returns: <number of primary ammo> <number of secondary ammo>
	static int GetNAmmo( struct lua_State *state );
	
	// params: <script id>; returns: <party>
	static int GetPartyOfUnits( struct lua_State *state );
	
	static int ReserveAviationForTimes( struct lua_State *pState );
	
	// params: <script id> <damage value>; returns: none;
	// ���� damage == 0, �� ������ ������������
	// ���� damage < 0, �� ������ �������
	static int DamageObject( struct lua_State *pState );

	// params: <script id>; returns: <id of unit state>
	// returns 0, if state is unknown or not set
	// returns -1, if unit doesn't exist
	static int GetUnitState( struct lua_State *pState );
	
	// params: <script id>; returns: <squad info>
	// returns -3, is the object doesn't exist,
	// returns -2, if it isn't a squad
	// returns -1, if it is a disbanded squad,
	// returns number of squad order, if it's a squad
	static int GetSquadInfo( struct lua_State *pState );
	
	// params: <script id>; returns: <follow info>
	// returns -1, is the object doesn't exist or not a unit
	// returns 0, if isn't following
	// returns 1, if is following
	static int IsFollowing( struct lua_State *pState );
	
	// params: <script id>; returns: <directions in range [0, 65535]>
	// returns -1 if unit doesn't exist or the object isn't a unit
	static int GetFrontDir( struct lua_State *pState );
	
	// params: <script id>; returns: <1, if connected, 0 - otherwise>
	static int IsWarehouseConnected( struct lua_State *pState );

	// params: <script id>; returns: <1, if under supply, 0 - otherwise>
	static int IsUnitUnderSupply( struct lua_State *pState );
	// params: <script id>; returns: <amount of morale>
	static int GetUnitMorale( struct lua_State *pState );

	// params: <script id>; returns: <shell type>
	static int GetActiveShellType( struct lua_State *pState );

	// params: <request string>; returns: none;
	static int AskClient( struct lua_State *pState );

	// params: none; returns: float random in [0, 1]
	static int RandomFloat( struct lua_State *pState );
	// params: n; returns: int random in [0, n-1]
	static int RandomInt( struct lua_State *pState );

	// params: <script id> <nParam>; returns: none;
	// nParam == 1 - select, nParam == 0 - deselect
	static int ChangeSelection( struct lua_State *pState );
	
	// params: <none>; returns: <mask of existing players>
	static int GetPlayersMask( struct lua_State *pState );
	// params: <player>; returns: 0 or 1
	static int IsPlayerPresent( struct lua_State *pState );
	
	// params: <script id>; returns: x y
	// if script group doesn't exist, return ( -1, -1 )
	static int GetObjCoord( struct lua_State *pState );

	// params: <name of script area>; returns: x y halflength halfwidth
	static int GetScriptAreaParams( struct lua_State *pState );
	
	// params: <bool>; returns: none
	static int SwitchWeather( struct lua_State *pState );
	// params: <bool>; returns: none
	static int SwitchWeatherAutomatic( struct lua_State *pState );

	// params: <diplomatic side>; returns: number of units, belonging to the side
	static int GetNUnitsInSide( struct lua_State *pState );
	
	// params: <script id>; returns: none
	static int AddIronMan( struct lua_State *state );

	// params: <difficulty level>; returns: none
	static int SetDifficultyLevel( struct lua_State *state );
	static int SetCheatDifficultyLevel( struct lua_State *state );

	// params: <script id of reinforcement to delete>; returns: none
	static int DeleteReinforcement( struct lua_State *pState );
	
	// params: <"name of script area"> <1 - open, 0 - close>; returns: none
	static int ViewZone( struct lua_State *pState );
	
	// params: <script id of unit>; returns: 0 or 1
	static int IsStandGround( struct lua_State *pState );
	// params: <script id of unit>; returns: 0 or 1
	static int IsEntrenched( struct lua_State *pState );

	// params: <script area name>; return: number of antiperson fences
	static int GetNAPFencesInScriptArea( struct lua_State *pState );
	// params: <script area name>; return: number of antitanks
	static int GetNAntitankInScriptArea( struct lua_State *pState );
	// params: <script area name>; return: number of fences
	static int GetNFencesInScriptArea( struct lua_State *pState );
	// params: <script area name>; return: number of trenches
	static int GetNTrenchesInScriptArea( struct lua_State *pState );
	// params: <script area name>; return: number of mines
	static int GetNMinesInScriptArea( struct lua_State *pState );

	// params player's number. returns type of last aviation called or -1 if no aviation was called
	static int GetAviationState( struct lua_State *state );
	
	static int Password( struct lua_State *pState );
	//
	// for internal usage
	static int ReturnScriptIDs( struct lua_State *pState );
	
	//
	static int SetGameSpeed( struct lua_State *pState );

	// params: <name of unit type> <number of party>; return: number of units
	// �������� ��������!
	static int GetNUnitsOfType( struct lua_State *pState );
	// params: none; returns: <xsize ysize> in AI points
	static int GetMapSize( struct lua_State *pState );
	
	//
	// for debug
	// params: none; return: none;
	static int CallAssert( struct lua_State *pState );
		
	//
	static Script::SRegFunction pRegList[];
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SCRIPT_FUNCTIONS_H__
