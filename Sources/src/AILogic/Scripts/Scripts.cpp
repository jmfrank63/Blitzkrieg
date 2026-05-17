#include "stdafx.h"

#include "scripts.h"

#include "..\AILogicInternal.h"
#include "..\Updater.h"
#include "..\StaticObjects.h"
#include "..\Building.h"
#include "..\Entrenchment.h"
#include "..\Diplomacy.h"
#include "..\Units.h"
#include "..\UnitsIterators2.h"
#include "..\UnitsIterators.h"
#include "..\Cheats.h"
#include "..\UnitCreation.h"
#include "..\Formation.h"
#include "..\Soldier.h"
#include "..\GroupLogic.h"
#include "..\Statistics.h"
#include "..\General.h"
#include "..\AIStaticMap.h"
#include "..\UnitStates.h"
#include "..\UnitGuns.h"
#include "..\Weather.h"
#include "..\MPLog.h"
#include "..\StaticObjectsIters.h"

#include "..\..\LuaLib\Script.h"

#include "..\..\Scene\Scene.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CWeather theWeather;
extern CSupremeBeing theSupremeBeing;
extern CGroupLogic theGroupLogic;
extern CUnitCreation theUnitCreation;
extern CAILogic *pAILogic;
extern CUnits units;
extern CUpdater updater;
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
extern SCheats theCheats;
extern CStatistics theStatistics;
extern CStaticMap theStaticMap;
extern CStaticObjects theStatObjs;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CScripts* pScripts;
const int CScripts::TIME_TO_CHECK_SUSPENDED_REINF = 200;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define CHECK_ERROR( bCond, message, nReturn )																												\
if ( !(bCond) )																																												\
{																																																			\
	pScripts->OutScriptError( ( std::string("Script error. ") + std::string(message) ).c_str() );				\
	return ( nReturn );																																									\
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CScripts::~CScripts()
{
	for ( std::unordered_map<int, SScriptInfo>::iterator iter = activeScripts.begin(); iter != activeScripts.end(); ++iter )
		script.Unref( iter->first );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetScriptID( IUpdatableObj *pObj ) const
{
	if ( groupUnits.find( pObj->GetUniqueId() ) != groupUnits.end() )
		return groupUnits.find( pObj->GetUniqueId() )->second;

	return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScripts::AddObjToScriptGroup( IUpdatableObj *pObj, const int nGroup )
{
	if ( nGroup != -1 )
	{
		groups[nGroup].push_back( pObj );
		groupUnits[pObj->GetUniqueId()] = nGroup;
		pObj->SetScriptID( nGroup );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScripts::AddUnitToReinforcGroup( const SMapObjectInfo &mapObject, const int nGroup, const SHPObjectRPGStats *pStats, IScenarioUnit *pScenarioUnit )
{
	NI_ASSERT_T( nGroup != -1, "Wrong number of reinforcement group" );
	reinforcs[nGroup].push_back( SReinforcementObject( mapObject, pStats, pScenarioUnit ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScripts::Init( const SLoadMapInfo &mapInfo )
{
	pScripts = this;
	pConsole = GetSingleton<IConsoleBuffer>();

	activeScripts.clear();
	groups.clear();
	groupUnits.clear();
	reinforcs.clear();
	reservePositions.clear();
	lastTimeToCheckSuspendedReinforcs = 0;
	reinforcsIter = suspendedReinforcs.begin();

	for ( SLoadMapInfo::TReservePositionsList::const_iterator iter = mapInfo.reservePositionsList.begin(); iter != mapInfo.reservePositionsList.end(); ++iter )
	{
		reservePositions[iter->nArtilleryLinkID] = iter->nTruckLinkID;
		reservePositions[iter->nTruckLinkID] = iter->nArtilleryLinkID;
	}
	
	bShowErrors = GetGlobalVar( "ShowScriptErrors", 0 ) != 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScripts::InitAreas( const SScriptArea scriptAreas[], const int nLen )
{
	for ( int i = 0; i < nLen; ++i )
		areas[scriptAreas[i].szName] = scriptAreas[i];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScripts::ReadScriptFile()
{
	// register global script functions
	script.Register( pRegList );
	// read and execute script
	if ( szScriptFile != "" )
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( (szScriptFile + ".lua").c_str(), STREAM_ACCESS_READ );
		if ( pStream )
		{
			int nSize = pStream->GetSize();
			// +10 �� ������ ������
			std::vector<char> buffer( nSize + 10 );
			pStream->Read( &(buffer[0]), nSize );

			return !( script.DoBuffer( &(buffer[0]), nSize, "Script" ) );
		}
		else
			return false;
	}
	else
		return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScripts::Load( const std::string &_szScriptFile )
{
	szScriptFile = _szScriptFile;
	if ( ReadScriptFile() )
	{
		script.GetGlobal( "Init" );
		script.Call( 0, 0 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::KillActiveScript( const std::string szName )
{
	CHECK_ERROR( name2script.find( szName ) != name2script.end(), NStr::Format( "Attempt to kill non-active script %s", szName.c_str() ), 0 );
	
	const int nRef = name2script[szName];
	
	std::unordered_map<int, SScriptInfo>::iterator killIter = activeScripts.find( nRef );
	NI_ASSERT_T(  killIter != activeScripts.end(), "Wrong script reference to kill" );

	if ( segmIter == killIter )
		++segmIter;

	script.Unref( killIter->first );
	name2script.erase( szName );
	activeScripts.erase( killIter );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScripts::CanUnitLand( const SMapObjectInfo &mapObject, IObjectsDB *pIDB, const CVec2 &vShift )
{
	CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( mapObject.szName.c_str() );
	const SUnitBaseRPGStats* pStats = dynamic_cast<const SUnitBaseRPGStats*>(pIDB->GetRPGStats( pDesc ));
	NI_ASSERT_T( pStats != 0, NStr::Format( "Object %s isn't a unit", mapObject.szName.c_str() ) );

	const CVec2 vCenter( mapObject.vPos.x + vShift.x, mapObject.vPos.y + vShift.y );
	if ( theStaticMap.CanUnitGo( pStats->nBoundTileRadius, AICellsTiles::GetTile( vCenter), pStats->aiClass ) )
	{
		const float length = pStats->vAABBHalfSize.y * SConsts::BOUND_RECT_FACTOR;
		const float width = pStats->vAABBHalfSize.x * SConsts::BOUND_RECT_FACTOR;

		const CVec2 realDirVec( GetVectorByDirection( mapObject.nDir ) );
		const CVec2 dirPerp( realDirVec.y, -realDirVec.x );
		const CVec2 vShift( realDirVec * pStats->vAABBCenter.y + dirPerp * pStats->vAABBCenter.x );

		SRect unitRect;
		unitRect.InitRect( vCenter + vShift, realDirVec, length, width );

		for ( CUnitsIter<0,1> iter( 0, ANY_PARTY, vCenter, 2 * length ); !iter.IsFinished(); iter.Iterate() )
		{
			CAIUnit *pUnit = *iter;
			if ( ( !pStats->IsInfantry() || !pUnit->GetStats()->IsInfantry() ) && unitRect.IsIntersected( pUnit->GetUnitRect() ) )
				return false;
		}

		return true;
	}
	else
		return false;

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScripts::CanFormationLand( const SMapObjectInfo &mapObject, IObjectsDB *pIDB, const CVec2 &vShift )
{
	CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( mapObject.szName.c_str() );
	const SSquadRPGStats* pStats = dynamic_cast<const SSquadRPGStats*>(pIDB->GetRPGStats( pDesc ));
	NI_ASSERT_T( pStats != 0, NStr::Format( "Object %s isn't a formation", mapObject.szName.c_str() ) );

	const CVec2 vCenter( mapObject.vPos.x + vShift.x, mapObject.vPos.y + vShift.y );

	std::list<CVec2> centers;
	theUnitCreation.GetCentersOfAllFormationUnits( pStats, vCenter, mapObject.nDir, mapObject.nFrameIndex, -1, &centers );

	SMapObjectInfo soldierMapObject;
	soldierMapObject.szName = pStats->formations[mapObject.nFrameIndex].order[0].szSoldier;
	for ( std::list<CVec2>::iterator iter = centers.begin(); iter != centers.end(); ++iter )
	{
		soldierMapObject.vPos = *iter;
		if ( !CanUnitLand( soldierMapObject, pIDB ) )
			return false;
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScripts::CanLandWithShift( const SMapObjectInfo &mapObject, IObjectsDB *pIDB, CVec2 *pvShift )
{
	CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( mapObject.szName.c_str() );
	const SUnitBaseRPGStats *pStats = dynamic_cast<const SUnitBaseRPGStats*>(pIDB->GetRPGStats( pDesc ));
	if ( pStats != 0 )
	{
		*pvShift = VNULL2;
		if ( CanUnitLand( mapObject, pIDB, *pvShift ) )
			return true;

		if ( !pStats->IsTrain() )
		{
			for ( int x = -4; x <= 4; ++x )
			{
				for ( int y = -4; y <= 4; ++y )
				{
					pvShift->x = x * 2 * SConsts::TILE_SIZE;
					pvShift->y = y * 2 * SConsts::TILE_SIZE;

					if ( CanUnitLand( mapObject, pIDB, *pvShift ) )
						return true;
				}
			}
		}
	}
	else if ( dynamic_cast<const SSquadRPGStats*>(pIDB->GetRPGStats( pDesc )) != 0 )
	{
		*pvShift = VNULL2;
		if ( CanFormationLand( mapObject, pIDB, *pvShift ) )
			return true;

		for ( int x = -4; x <= 4; ++x )
		{
			for ( int y = -4; y <= 4; ++y )
			{
				pvShift->x = x * 2 * SConsts::TILE_SIZE;
				pvShift->y = y * 2 * SConsts::TILE_SIZE;

				if ( CanFormationLand( mapObject, pIDB, *pvShift ) )
					return true;
			}
		}
	}
	else
		return true;

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsUnit( const SMapObjectInfo &mapObject, IObjectsDB *pIDB )
{
	CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( mapObject.szName.c_str() );
	return ( dynamic_cast<const SUnitBaseRPGStats*>(pIDB->GetRPGStats( pDesc )) != 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScripts::LandSuspendedReiforcements()
{
	if ( curTime >= lastTimeToCheckSuspendedReinforcs + TIME_TO_CHECK_SUSPENDED_REINF )
	{
		lastTimeToCheckSuspendedReinforcs = curTime;
		CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
		if ( reinforcsIter == suspendedReinforcs.end() )
			reinforcsIter = suspendedReinforcs.begin();

		int cnt = 0;
		while ( reinforcsIter != suspendedReinforcs.end() && cnt < 1 )
		{
			if ( reinforcsIter->mapObject.link.nLinkWith == 0 )
			{
				++cnt;
				CVec2 vShift( VNULL2 );
				if ( CanLandWithShift( reinforcsIter->mapObject, pIDB, &vShift ) )
				{
					std::unordered_set<int> candidates;
					const int nLink = reinforcsIter->mapObject.link.nLinkID;
					candidates.insert( nLink );
					CReinfList candObjects;

					bool bAdded = false;
					bool bCanLand = true;
					do
					{
						bAdded = false;
						
						CReinfList::iterator candreinforcsIter = suspendedReinforcs.begin();
						while ( candreinforcsIter != suspendedReinforcs.end() )
						{
							const int nLinkID = candreinforcsIter->mapObject.link.nLinkID;
							const int nLinkWith = candreinforcsIter->mapObject.link.nLinkWith;
							bool bReserve = reservePositions.find( nLinkID ) != reservePositions.end();

							if ( candidates.find( nLinkID ) == candidates.end() &&
									 ( nLinkWith != 0 && candidates.find( nLinkWith ) != candidates.end() ||
										 bReserve && candidates.find( reservePositions[nLinkID] ) != candidates.end() )
									)
							{
								if ( false )
//								if ( !CanLand( candreinforcsIter->mapObject, pIDB ) )
								{
									bCanLand = false;
									// �� ������ �� ������� ������������, �.�. ����� ������ �������, ������� ����-������ ���������
									suspendedReinforcs.splice( suspendedReinforcs.begin(), candObjects );
									break;
								}
								else
								{
									candidates.insert( nLinkID );
									CReinfList::iterator oldIter = candreinforcsIter++;
									// �� ������ �� ������� ������������, �.�. ����� ������ �������, ������� ����-������ ���������
									candObjects.splice( candObjects.begin(), suspendedReinforcs, oldIter );
									bAdded = true;
								}
							}
							else
								++candreinforcsIter;
						}
					} while ( bAdded && bCanLand );

					if ( bCanLand )
					{
						candObjects.push_back( *reinforcsIter );
						LandReinforcementWithoutLandCheck( &candObjects, vShift );
						reinforcsIter = suspendedReinforcs.erase( reinforcsIter );
					}
					else
						++reinforcsIter;
				}
				else
					++reinforcsIter;
			}
			else
				++reinforcsIter;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScripts::Segment()
{
	// retrieve all script calls
	{
		bool bOldShowScriptErrors = bShowErrors;
		bShowErrors = true;
		while ( const char* pszCommand = pConsole->ReadASCII(CONSOLE_STREAM_SCRIPT) )
		{
			if ( theCheats.IsPasswordOk() || strncmp( pszCommand, "Password(", 9 ) == 0 )
				CallScriptFunction( pszCommand );
		}
		bShowErrors = bOldShowScriptErrors;
	}
	//
	segmIter = activeScripts.begin();
	while ( segmIter != activeScripts.end() )
	{
		if ( curTime - segmIter->second.lastUpdate >= segmIter->second.period )
		{
			segmIter->second.lastUpdate = curTime;

			int &nRepetitions = segmIter->second.nRepetitions;
			bKill = ( nRepetitions == 1 );
			if ( nRepetitions != -1 )
				--nRepetitions;

			const std::string name = segmIter->second.szName;
			
			const int nRef = segmIter->first;
			// ��� ����������� �������� ��������
			++segmIter;

			script.GetRef( nRef );
			
			script.Call( 0, 0 );

			if ( bKill && name2script.find( name ) != name2script.end() )
				KillActiveScript( name );
		}
		else
			++segmIter;
	}

	LandSuspendedReiforcements();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScripts::CallScriptFunction( const char *pszCommand )
{
	const int oldtop = script.GetTop();
	const std::string szFunction( pszCommand );

	script.DoString( ("return " + szFunction).c_str() );

	const int nReturns = script.GetTop();
	std::string szAnswer("");

	for ( int i = 1; i <= nReturns; ++i )
	{
		Script::Object obj = script.GetObject( i );

		if ( obj.IsNumber() )
			szAnswer += std::string( NStr::Format( "%d", int(obj) ) );
		else if ( const char *pszAnswer = obj.GetString() )
			szAnswer += pszAnswer;

		if ( i < nReturns )
			szAnswer += ", ";
	}

  script.SetTop(oldtop);

	if ( !szAnswer.empty() )
			pConsole->WriteASCII( CONSOLE_STREAM_CONSOLE, szAnswer.c_str(), 0xff00ff00 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScripts::DelInvalidBegin( const int targetId )
{
	if ( groups.find( targetId ) != groups.end() )
	{
		while ( !groups[targetId].empty() )
		{
			IUpdatableObj *pObj = groups[targetId].back();
			if ( pObj == 0 || !pObj->IsValid() || dynamic_cast<CStaticObject*>(pObj) == 0 && !pObj->IsAlive() )
				groups[targetId].pop_back();
			else
				break;
		}

		int nDeleted;
		for( std::unordered_map< int, int>::iterator it = groupUnits.begin(); it != groupUnits.end(); ++it )
		{
			const int nUniqueId = it->first;
			CLinkObject *pObj = GetObjectByUniqueIdSafe<CLinkObject>( nUniqueId );
			if ( !pObj || !pObj->IsValid() || dynamic_cast<CStaticObject*>(pObj) == 0 && !pObj->IsAlive() )
				nDeleted = nUniqueId;
			else
				break;
		}

		groupUnits.erase( nDeleted );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScripts::DelInvalidUnits( const int scriptId )
{
	if ( groups.find( scriptId ) != groups.end() )
	{
		std::list<CPtr<IUpdatableObj> >::iterator iter = groups[scriptId].begin();
		while ( iter != groups[scriptId].end() )
		{
			IUpdatableObj *pObj = *iter;
			if ( pObj == 0 || !pObj->IsValid() || dynamic_cast<CStaticObject*>(pObj) == 0 && !pObj->IsAlive() )
				iter = groups[scriptId].erase( iter );
			else
				++iter;
		}

		std::list<int> deleted;
		for( std::unordered_map< int, int>::iterator it = groupUnits.begin(); it != groupUnits.end(); ++it )
		{
			const int nUniqueId = it->first;
			CLinkObject *pObj = GetObjectByUniqueIdSafe<CLinkObject>( nUniqueId );
			if ( !pObj || !pObj->IsValid() || dynamic_cast<CStaticObject*>(pObj) == 0 && !pObj->IsAlive() )
				deleted.push_back( nUniqueId );
		}

		while( !deleted.empty() )
		{
			groupUnits.erase( deleted.front() );
			deleted.pop_front();
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetCheckObjectsInScriptArea( const SScriptArea &area, const interface ICheckObjects &check )
{
	float fR = area.eType == SScriptArea::EAT_CIRCLE ? area.fR : Max( area.vAABBHalfSize.x, area.vAABBHalfSize.y );	
	int nResult = 0;
	for ( CStObjCircleIter<false> iter( area.center, fR ); !iter.IsFinished(); iter.Iterate() )
	{
		CExistingObject *pObj = *iter;
		if ( pObj->IsValid() && pObj->IsAlive() && check.IsGoodObj( pObj ) )
			++nResult;
	}

	return nResult;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScripts::OutScriptError( const char *pszString )
{
	if ( bShowErrors )
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, pszString, 0xffff0000, true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Script::SRegFunction CScripts::pRegList[] =
{
	{ "_ERRORMESSAGE",												CScripts::Error_out },

	{	"RunScript",														CScripts::RunScript },
	{ "Suicide",															CScripts::Suicide },
	{ "KillScript",														CScripts::KillScript },

	{ "GetNUnitsInCircle",										CScripts::GetNUnitsInCircle },
	{ "LandReinforcement",										CScripts::LandReinforcement },
	{	"Win",																	CScripts::Win							 },
	{ "Loose",																CScripts::Loose						 },
	{ "Draw",																	CScripts::Draw						 },
	{ "GiveCommand",													CScripts::GiveCommand			 },
	{ "Cmd",																	CScripts::GiveCommand			 },
	{ "GiveQCommand",													CScripts::GiveQCommand		 },
	{ "QCmd",																	CScripts::GiveQCommand		 },
	{	"GetNUnitsInScriptGroup",						 	  CScripts::GetNUnitsInScriptGroup },
	{	"ShowActiveScripts",										CScripts::ShowActiveScripts },
	{	"GetNUnitsInArea",											CScripts::GetNUnitsInArea },
	{ "GetNScriptUnitsInArea",								CScripts::GetNScriptUnitsInArea },
	{ "ChangeWarFog",													CScripts::ChangeWarFog },
	{ "ChangePlayer",													CScripts::ChangePlayer },
	{ "God",																	CScripts::God },
	{ "SetIGlobalVar",												CScripts::SetIGlobalVar },
	{ "SetFGlobalVar",												CScripts::SetFGlobalVar },
	{ "SetSGlobalVar",												CScripts::SetSGlobalVar },
	{ "GetIGlobalVar",												CScripts::GetIGlobalVar },
	{ "GetFGlobalVar",												CScripts::GetFGlobalVar },
	{ "GetSGlobalVar",												CScripts::GetSGlobalVar },
	{ "EnableAviation",												CScripts::EnableAviation },
	{ "DiableAviation",												CScripts::DisableAviation },
	{ "DisableAviation",											CScripts::DisableAviation },

	{ "GetObjectHPs",													CScripts::GetObjectHPs },
	{ "GetNUnitsInParty",											CScripts::GetNUnitsInParty },
	{ "GetNUnitsInPartyUF",										CScripts::GetNUnitsInPartyUF },
	{ "GetNUnitsInPlayerUF",									CScripts::GetNUnitsInPlayerUF },
	{ "ChangeFormation",											CScripts::ChangeFormation },

	{ "Trace",																CScripts::Trace },
	{ "DisplayTrace",													CScripts::DisplayTrace },
	{ "ObjectiveChanged",											CScripts::ObjectiveChanged },
	{ "GetNAmmo",															CScripts::GetNAmmo },

	{ "GetPartyOfUnits",											CScripts::GetPartyOfUnits },
	{ "ReserveAviationForTimes",							CScripts::ReserveAviationForTimes },
	{ "DamageObject",													CScripts::DamageObject },
	{ "CallAssert",														CScripts::CallAssert },
	{ "GetUnitState",													CScripts::GetUnitState },
	{ "GetSquadInfo",													CScripts::GetSquadInfo },
	{ "IsFollowing",													CScripts::IsFollowing },
	{ "GetFrontDir",													CScripts::GetFrontDir },
	{ "IsWarehouseConnected",									CScripts::IsWarehouseConnected },
	{ "IsUnitUnderSupply",										CScripts::IsUnitUnderSupply },
	{ "GetUnitMorale",												CScripts::GetUnitMorale },
	{ "GetActiveShellType",										CScripts::GetActiveShellType },
	{ "AskClient",														CScripts::AskClient },
	{ "RandomFloat",													CScripts::RandomFloat },
	{ "RandomInt",														CScripts::RandomInt },
	{ "ChangeSelection",											CScripts::ChangeSelection },
	{ "ReturnScriptIDs",											CScripts::ReturnScriptIDs },
	{ "GetPlayersMask",												CScripts::GetPlayersMask },
	{ "GetObjCoord",													CScripts::GetObjCoord },
	{ "GetScriptAreaParams",									CScripts::GetScriptAreaParams },
	{ "IsPlayerPresent",											CScripts::IsPlayerPresent },
	{ "SwitchWeather",											  CScripts::SwitchWeather },
	{ "SwitchWeatherAutomatic",								CScripts::SwitchWeatherAutomatic },

	{ "GetNUnitsInSide",											CScripts::GetNUnitsInSide },
	{ "AddIronMan",														CScripts::AddIronMan },
	{ "SetDifficultyLevel",										CScripts::SetDifficultyLevel },
	{ "SetCheatDifficultyLevel",							CScripts::SetCheatDifficultyLevel },
	{ "DeleteReinforcement",									CScripts::DeleteReinforcement },
	{ "ViewZone",															CScripts::ViewZone },
	{ "IsStandGround",												CScripts::IsStandGround },
	{ "IsEntrenched",													CScripts::IsEntrenched },

	{ "GetNAPFencesInScriptArea",								CScripts::GetNAPFencesInScriptArea },
	{ "GetNAntitankInScriptArea",								CScripts::GetNAntitankInScriptArea },
	{ "GetNFencesInScriptArea",									CScripts::GetNFencesInScriptArea },
	{ "GetNTrenchesInScriptArea",								CScripts::GetNTrenchesInScriptArea },
	{ "GetNMinesInScriptArea",									CScripts::GetNMinesInScriptArea },
	{ "GetAviationState",												CScripts::GetAviationState },
	{ "Password",																CScripts::Password },
	{ "SetGameSpeed",														CScripts::SetGameSpeed },
	{ "GetNUnitsOfType",												CScripts::GetNUnitsOfType },
	{	"GetMapSize",															CScripts::GetMapSize },

	{ 0, 0 } // End
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetAviationState( struct lua_State *state )
{
	Script script(state);
	CHECK_ERROR( script.IsNumber( 1 ), "EnableAviation: the first parameter is not a player number", 0 );
	const int nPlayer = script.GetObject( 1 );
	script.PushNumber( theUnitCreation.GetLastCalledAviation( nPlayer ) );
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::AddIronMan( struct lua_State *state )
{
	Script script(state);
	const int nScriptID = script.GetObject( 1 );
	theSupremeBeing.AddIronman( nScriptID );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::Error_out( struct lua_State *state )
{
	Script script(state);
	Script::Object obj = script.GetObject(script.GetTop());
	pScripts->OutScriptError( NStr::Format("Script error: %s", obj.GetString() ) );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::EnableAviation( struct lua_State *state )
{
// params: <party of player : -1 - anyplayer> <type of aviation: -1 - any aviation>; returns: none;
	Script script(state);

	CHECK_ERROR( script.IsNumber( 1 ), "EnableAviation: the first parameter is not a player number", 0 );
	CHECK_ERROR( script.IsNumber( 2 ), "EnableAviation: the second parameter is not an aviation type", 0 );

	const int nPlayer = script.GetObject( 1 );
	const int nAvia = script.GetObject( 2 );
	theUnitCreation.EnableAviationScript( nPlayer, nAvia );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::DisableAviation( struct lua_State *state )
{
	Script script(state);

	CHECK_ERROR( script.IsNumber( 1 ), "DiableAviation: the first parameter is not a player number", 0 );
	CHECK_ERROR( script.IsNumber( 2 ), "DiableAviation: the second parameter is not an aviation type", 0 );

	const int nPlayer = script.GetObject( 1 );
	const int nAvia = script.GetObject( 2 );
	theUnitCreation.DisableAviationScript( nPlayer, nAvia );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::RunScript( struct lua_State *state ) 
{
	Script script( state );

	CHECK_ERROR( script.IsString( 1 ), "RunScript : the first parameter is not a name of function", 0 );
	CHECK_ERROR( script.IsNumber( 2 ), "RunScript : the second parameter is not a periodicity", 0 );

	const std::string szName = script.GetObject( 1 );
	const int nPeriod = script.GetObject( 2 );
	int nRepetitions = -1;
	if ( script.GetTop() == 3 )
	{
		CHECK_ERROR( script.IsNumber( 3 ), "RunScript : the third parameter is not a number", 0 );
		nRepetitions = script.GetObject( 3 );
		script.Pop();
	}
	script.Pop();
	script.Pop();

	script.GetGlobal( szName.c_str() );
	const int nScriptRef = script.Ref( 1 );

	pScripts->name2script[szName] = nScriptRef;
	
	pScripts->activeScripts[nScriptRef].lastUpdate = GetAIGetSegmTime( pAILogic->GetGameSegment() );
	pScripts->activeScripts[nScriptRef].period = nPeriod;
	pScripts->activeScripts[nScriptRef].szName = szName;
	pScripts->activeScripts[nScriptRef].nRepetitions = nRepetitions;

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::Suicide( struct lua_State *state )
{
	pScripts->bKill = true;

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::KillScript( struct lua_State *state )
{
	Script script( state );
	CHECK_ERROR( script.IsString( 1 ), "Kill script: the first parameter is not a number", 0 );

	pScripts->KillActiveScript( std::string( script.GetObject( 1 ) ) );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNUnitsInCircle( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsNumber( 1 ), "GetNUnitsInCircle : the first parameter is not a number", 1 );
	CHECK_ERROR( script.IsNumber( 2 ), "GetNUnitsInCircle : the second parameter is not a number", 1 );
	CHECK_ERROR( script.IsNumber( 3 ), "GetNUnitsInCircle : the third parameter is not a number", 1 );
	CHECK_ERROR( script.IsNumber( 4 ), "GetNUnitsInCircle : the fourth parameter is not a number", 1 );

	const float fR = script.GetObject( -1 );
	const CVec2 center( script.GetObject( -3 ), script.GetObject( -2 ) );
	const int nPlayer = script.GetObject( -4 );

	int cnt = 0;

	for ( CUnitsIter<0,2> iter( theDipl.GetNParty( nPlayer ), EDI_FRIEND, center, fR ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = *iter;
		if ( pUnit->IsValid() && pUnit->IsAlive() &&
				 pUnit->GetPlayer() == nPlayer && !pUnit->GetStats()->IsAviation() )
		{
			if ( fabs2( (*iter)->GetCenter() - center ) <= fR * fR )
				++cnt;
		}
	}

	script.PushNumber( cnt );
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNUnitsInScriptGroup( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsNumber( 1 ), "GetNUnitsInScriptGroup: the first parameter is not a number", 1 );
	
	const int nScriptID = script.GetObject( 1 );

	int nNumber = 0;	
	if ( pScripts->groups.find( nScriptID ) != pScripts->groups.end() )
	{
		pScripts->DelInvalidUnits( nScriptID );

		if ( script.IsNumber( 2 ) )
		{
			const int nPlayer	= script.GetObject( 2 );
			for ( std::list<CPtr<IUpdatableObj> >::iterator it = pScripts->groups[nScriptID].begin();
					it != pScripts->groups[nScriptID].end(); ++it )
			{
				if ( (*it)->GetPlayer() == nPlayer )
					++nNumber;
			}
		}
		else
		{
			for ( std::list<CPtr<IUpdatableObj> >::iterator it = pScripts->groups[nScriptID].begin();
						it != pScripts->groups[nScriptID].end(); ++it )
			{
				if ( (*it)->IsAlive() )
					++nNumber;
			}
		}
	}

	script.PushNumber( nNumber );
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScripts::SetNewLinksToReinforcement( CReinfList *pReinf, std::unordered_map<int, int> *pOld2NewLinks )
{
	// set new links (not intersected with existing)
	std::list<int> freeLinks;
	CLinkObject::GetFreeLinks( &freeLinks, pReinf->size() );
	for ( CReinfList::iterator iter = pReinf->begin(); iter != pReinf->end(); ++iter )
	{
		(*pOld2NewLinks)[iter->mapObject.link.nLinkID] = freeLinks.front();
		iter->mapObject.link.nLinkID = freeLinks.front();
		freeLinks.pop_front();
	}

	for ( CReinfList::iterator iter = pReinf->begin(); iter != pReinf->end(); ++iter )
	{
		const int nLinkWith = iter->mapObject.link.nLinkWith;
		if ( pOld2NewLinks->find( nLinkWith ) != pOld2NewLinks->end() )
			iter->mapObject.link.nLinkWith = (*pOld2NewLinks)[nLinkWith];
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScripts::LandReinforcementWithoutLandCheck( CReinfList *pReinf, const CVec2 &vShift )
{
	std::unordered_map<int, int> old2NewLinks;
	SetNewLinksToReinforcement( pReinf, &old2NewLinks );

	std::list<CCommonUnit*> pUnits;

	// land reinforcement
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();

	LinkInfo linksInfo;
	CReinfList transports;
	// ��������� ��� �� ���������
	for ( CReinfList::iterator iter = pReinf->begin(); iter != pReinf->end(); ++iter )
	{
		const IGDBObject *pObject = NGDB::GetRPGStats<IGDBObject>( iter->mapObject.szName.c_str() );
		const SUnitBaseRPGStats *pStats = dynamic_cast<const SUnitBaseRPGStats*>( pObject );
		if ( !pStats || !pStats->IsTransport() )
		{
			iter->mapObject.vPos += CVec3( vShift, 0.0f );
			IRefCount *pUnit = pAILogic->AddObject( iter->mapObject, pIDB, &linksInfo, false, false, iter->pStats );
			
			if ( pUnit )
			{
				if ( CCommonUnit *pUnit1 = dynamic_cast<CCommonUnit*>(pUnit) )
				{
					pUnit1->SetScenarioUnit( iter->pScenarioUnit );
					pUnits.push_back( pUnit1 );
				}
				if ( CAIUnit *pAIUnit = dynamic_cast<CAIUnit*>(pUnit) )
					pAIUnit->SetScenarioStats();
			}
		}
		else
			transports.push_back( *iter );
	}

	// ��������� ��� ���������
	for ( CReinfList::iterator iter = transports.begin(); iter != transports.end(); ++iter )
	{
		iter->mapObject.vPos += CVec3( vShift, 0.0f );
		IRefCount *pUnit = pAILogic->AddObject( iter->mapObject, pIDB, &linksInfo, false, false, iter->pStats );
		
		if ( pUnit )
		{
			if ( CCommonUnit *pUnit1 = dynamic_cast<CCommonUnit*>(pUnit) )
			{
				pUnit1->SetScenarioUnit( iter->pScenarioUnit );
				pUnits.push_back( pUnit1 );
			}
			if ( CAIUnit *pAIUnit = dynamic_cast<CAIUnit*>(pUnit) )
				pAIUnit->SetScenarioStats();

		}
	}

	pAILogic->InitLinks( linksInfo );
	pAILogic->InitReservePositions( old2NewLinks );
	pAILogic->InitStartCommands( linksInfo, old2NewLinks );

	theSupremeBeing.GiveNewUnitsToGenerals( pUnits );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScripts::SendShowReinoforcementPlacementFeedback( std::list<CVec2> *pCenters )
{
	CVec2 vMostLeftCenter = pCenters->front();

	for ( std::list<CVec2>::iterator iter = pCenters->begin(); iter != pCenters->end(); ++iter )
	{
		if ( vMostLeftCenter.x > iter->x )
			vMostLeftCenter = *iter;
	}

	int nUnits = 0;
	CVec2 vGroupCenter( VNULL2 );

	std::list<CVec2>::iterator iter = pCenters->begin();
	while ( iter != pCenters->end() )
	{
		if ( fabs2( vMostLeftCenter - (*iter) ) < sqr( SConsts::REINFORCEMENT_GROUP_DISTANCE ) )
		{
			vGroupCenter += *iter;
			++nUnits;

			iter = pCenters->erase( iter );
		}
		else
			++iter;
	}

	vGroupCenter /= nUnits;
	
	updater.AddFeedBack( SAIFeedBack( EFB_REINFORCEMENT_CENTER, MAKELONG( vGroupCenter.x, vGroupCenter.y ) ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::LandReinforcement( struct lua_State *state )
{
	Script script( state );
	
	CHECK_ERROR( script.IsNumber( 1 ), "LandReinforcement: the first parameter is not a number", 0 );

	const int nRein = script.GetObject( 1 );
	CHECK_ERROR( pScripts->reinforcs.find( nRein ) != pScripts->reinforcs.end(), NStr::Format( "Wrong number of reinforcement, %d", nRein ), 0 );

	WORD playersOfReinforcement = 0;
	bool bSendAck = false;
	std::list<CVec2> centers;
	for ( CReinfList::iterator iter = pScripts->reinforcs[nRein].begin(); iter != pScripts->reinforcs[nRein].end(); ++iter )
	{
		const int nPlayer = iter->mapObject.nPlayer;
		if ( theDipl.IsPlayerExist( nPlayer ) )
		{
			pScripts->suspendedReinforcs.push_front( *iter );

			//
			if ( nPlayer == theDipl.GetMyNumber() )
				bSendAck = true;

			if ( theCheats.IsHistoryPlaying() || nPlayer == theDipl.GetMyNumber() )
				centers.push_back( CVec2( iter->mapObject.vPos.x, iter->mapObject.vPos.y ) );

			playersOfReinforcement = playersOfReinforcement | ( 1 << nPlayer );
		}
	}

	if ( bSendAck )
	{
		updater.AddFeedBack( SAIFeedBack( EFB_REINFORCEMENT_ARRIVED ) );
		while ( !centers.empty() )
			pScripts->SendShowReinoforcementPlacementFeedback( &centers );
	}

	for ( int i = 0; i < theDipl.GetNPlayers(); ++i )
	{
		if ( playersOfReinforcement & 1 )
		{
			NI_ASSERT_T( theDipl.IsPlayerExist( i ), "Wrong number of player" );
			theStatistics.ReinforcementUsed( i );
		}

		playersOfReinforcement >>= 1;
	}
	
	// ���������� �������� ��� ������������
	if ( pScripts->lastTimeToCheckSuspendedReinforcs != curTime )
	{
		pScripts->lastTimeToCheckSuspendedReinforcs = 0;
		pScripts->reinforcsIter = pScripts->suspendedReinforcs.begin();
		pScripts->LandSuspendedReiforcements();
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::ReserveAviationForTimes( struct lua_State *pState )
{
	Script script( pState );
	CHECK_ERROR( script.IsNumber( 1 ), "ReserveAviationForTimes: first parameter isn't party", 0 );

	const int nParty = script.GetObject( 1 );
	std::vector<NTimer::STime> times;

	for ( int i = 2; script.IsNumber( i ); ++i )
		times.push_back( script.GetObject( i ) );
	CHECK_ERROR( !times.empty(), "ReserveAviationForTimes: NO times", 0 );

	theSupremeBeing.ReserveAviationForTimes( nParty, times );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::Draw( struct lua_State *state )
{
	Script script( state );
	updater.AddFeedBack( SAIFeedBack( EFB_DRAW ) );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::Win( struct lua_State *state )
{
	Script script( state );
	
	CHECK_ERROR( script.IsNumber( 1 ), "Win: the first parameter is not a number", 0 );
	const int nParty = script.GetObject( 1 );

	if ( nParty == theDipl.GetMyParty() )
		updater.AddFeedBack( SAIFeedBack( EFB_WIN ) );
	else
		updater.AddFeedBack( SAIFeedBack( EFB_LOOSE ) );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::Loose( struct lua_State *state )
{
	updater.AddFeedBack( SAIFeedBack( EFB_LOOSE ) );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::ProcessCommand( struct lua_State *state, const bool bPlaceInQueue )
{
	Script script( state );

	CHECK_ERROR( script.IsNumber( 1 ), "GiveCommand : the first parameter is not a command", 0 );
	CHECK_ERROR( script.IsNumber( 2 ), "GiveCommand : the second parameter is not a script id", 0 );
	
	const int scriptId = script.GetObject( 2 );
	int nGroup = -1;
	bool bAviationCallCommand = false;
	std::list< std::pair<CCommonUnit*, int> > oldUnitsGroups;
	// ������� � ���������
	if ( scriptId != -1 && pScripts->groups.find( scriptId ) != pScripts->groups.end() )
	{
		//CHECK_ERROR( pScripts->groups.find( scriptId ) != pScripts->groups.end(), NStr::Format( "GiveCommand: wrong script id, %d", scriptId ), 0 );

		pScripts->DelInvalidUnits( scriptId );
		// group registration
		IRefCount **pObjects = GetTempBuffer<IRefCount*>( pScripts->groups[scriptId].size() );
		int nLen = 0;
		for ( std::list<CPtr<IUpdatableObj> >::iterator iter = pScripts->groups[scriptId].begin(); iter != pScripts->groups[scriptId].end(); ++iter )
		{
			CHECK_ERROR( dynamic_cast_ptr<CCommonUnit*>(*iter) != 0, "Can't give command to non-unit", 0 );
			CCommonUnit *pUnit = static_cast_ptr<CCommonUnit*>(*iter);
			pObjects[nLen++] = pUnit;

			oldUnitsGroups.push_back( std::pair<CCommonUnit*, int>( pUnit, pUnit->GetNGroup() ) );
		}
		
		nGroup = pAILogic->GenerateGroupNumber();
		pAILogic->RegisterGroup( pObjects, nLen, nGroup );
	}
	// ������������ �������
	SAIUnitCmd command;
	command.cmdType = EActionCommand( int( script.GetObject( 1 ) ) );
	bool bValid = true;
	int nPlayer =0;
	switch ( command.cmdType )
	{
		case ACTION_COMMAND_MOVE_TO:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_MOVE_TO command : the third parameter is not an X coordinate", 0 );
			CHECK_ERROR( script.IsNumber( 4 ), "Give ACTION_COMMAND_MOVE_TO command : the fourth parameter is not an Y coordinate", 0 );

			command.vPos.x = script.GetObject( 3 );
			command.vPos.y = script.GetObject( 4 );
		
			break;
		case ACTION_COMMAND_ATTACK_UNIT:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_ATTACK_UNIT command : the third parameter is not a script id", 0 );

			{
				const int targetId = script.GetObject( 3 );
				pScripts->DelInvalidBegin( targetId );

				if ( !pScripts->groups[targetId].empty() )
					command.pObject = pScripts->groups[targetId].back();
				else
					bValid = false;
			}

			break;
		case ACTION_COMMAND_ATTACK_OBJECT:
			CHECK_ERROR( false, "Script command ACTION_COMMAND_ATTACK_OBJECT hasn't been implemented yet", 0 );

			break;
		case ACTION_COMMAND_SWARM_TO:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_SWARM_TO command : the third parameter is not an X coordinate", 0 );
			CHECK_ERROR( script.IsNumber( 4 ), "Give ACTION_COMMAND_SWARM_TO command : the fourth parameter is not an Y coordinate", 0 );

			command.vPos.x = script.GetObject( 3 );
			command.vPos.y = script.GetObject( 4 );

			break;
		case ACTION_COMMAND_LOAD:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_LOAD command : the third parameter is not a script id", 0 );

			{
				const int targetId = script.GetObject( 3 );
				pScripts->DelInvalidBegin( targetId );

				if ( !pScripts->groups[targetId].empty() )
					command.pObject = pScripts->groups[targetId].back();
				else
					bValid = false;
			}

			break;
		case ACTION_COMMAND_UNLOAD:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_UNLOAD command : the third parameter is not an X coordinate", 0 );
			CHECK_ERROR( script.IsNumber( 4 ), "Give ACTION_COMMAND_UNLOAD command : the fourth parameter is not an Y coordinate", 0 );

			command.vPos.x = script.GetObject( 3 );
			command.vPos.y = script.GetObject( 4 );

			break;
		case ACTION_COMMAND_ENTER:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_ENTER command : the third parameter is not a script id", 0 );

			{
				const int targetId = script.GetObject( 3 );
				pScripts->DelInvalidBegin( targetId );

				if ( !pScripts->groups[targetId].empty() )
				{
					command.pObject = pScripts->groups[targetId].back();

					if ( dynamic_cast<CBuilding*>(command.pObject.GetPtr()) )
						command.fNumber = 0;
					else if ( dynamic_cast<CEntrenchmentPart*>(command.pObject.GetPtr()) )
						command.fNumber = 2;
					else
						CHECK_ERROR( false, "Give ACTION_COMMAND_ENTER command : the object to enter to is not buliding or entrenchment", 0 );

				}
				else
					bValid = false;
			}

			break;
		case ACTION_COMMAND_LEAVE:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_LEAVE command : the third parameter is not an X coordinate", 0 );
			CHECK_ERROR( script.IsNumber( 4 ), "Give ACTION_COMMAND_LEAVE �ommand : the fourth parameter is not an Y coordinate", 0 );

			command.vPos.x = script.GetObject( 3 );
			command.vPos.y = script.GetObject( 4 );

			break;
		case ACTION_COMMAND_ROTATE_TO:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_ROTATE_TO command : the third parameter is not an X coordinate", 0 );
			CHECK_ERROR( script.IsNumber( 4 ), "Give ACTION_COMMAND_ROTATE_TO command : the fourth parameter is not an Y coordinate", 0 );

			command.vPos.x = script.GetObject( 3 );
			command.vPos.y = script.GetObject( 4 );

			break;
		case ACTION_COMMAND_ROTATE_TO_DIR:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_ROTATE_TO_DIR command : the third parameter is not an X coordinate", 0 );
			CHECK_ERROR( script.IsNumber( 4 ), "Give ACTION_COMMAND_ROTATE_TO_DIR command : the fourth parameter is not an Y coordinate", 0 );

			command.vPos.x = script.GetObject( 3 );
			command.vPos.y = script.GetObject( 4 );

			break;
		case ACTION_COMMAND_STOP:

			break;
		case ACTION_COMMAND_DIE:

			break;
		case ACTION_COMMAND_CALL_FIGHTERS:
		case ACTION_COMMAND_PARADROP:
		case ACTION_COMMAND_CALL_BOMBERS:
		case ACTION_COMMAND_CALL_SHTURMOVIKS:
		case ACTION_COMMAND_CALL_SCOUT:
			{
				bAviationCallCommand = true;
				
				CHECK_ERROR( script.IsNumber( 4 ), "Give ACTION_COMMAND_CALL_(plane) command : the 4 parameter is not an X coordinate", 0 );
				CHECK_ERROR( script.IsNumber( 5 ), "Give ACTION_COMMAND_CALL_(plane) command : the 5 parameter is not an Y coordinate", 0 );
				CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_CALL_(plane) command : the 3 parameter is not a number of player", 0 );

				nPlayer = script.GetObject( 3 );
				CHECK_ERROR( nPlayer < theDipl.GetNPlayers(), "Give ACTION_COMMAND_CALL_(plane) command : the 6 parameter is too large", 0 );

				nGroup = pAILogic->GenerateGroupNumber();

				command.vPos.x = script.GetObject( 4 );
				command.vPos.y = script.GetObject( 5 );
				command.fNumber = 1;
				// �������� ������� Call(plane)
				pAILogic->UnitCommand( &command, nGroup, nPlayer );
				
				// ���� ���������
				if ( theGroupLogic.BeginGroup( nGroup ) != theGroupLogic.EndGroup() )
				{
					for ( int i = 6; ; i += 2 )
					{
						if ( script.IsNumber( i ) && script.IsNumber( i + 1 ) )
						{
							SAIUnitCmd cmd;
							cmd.cmdType = ACTION_COMMAND_PLANE_ADD_POINT;
							cmd.vPos.x = script.GetObject( i );
							cmd.vPos.y = script.GetObject( i + 1 );
							cmd.fNumber = 0;
							pAILogic->GroupCommand( &cmd, nGroup, true );
						}
						else
							break;
					}
					// send takeoff command
					{
						SAIUnitCmd cmd;
						cmd.cmdType = ACTION_COMMAND_PLANE_TAKEOFF_NOW;
						pAILogic->GroupCommand( &cmd, nGroup, true );
					}
				}					
				if ( scriptId != -1 ) // ��������� ����� ��� ����� � ������� �� � ������
				{
					for ( int i = theGroupLogic.BeginGroup( nGroup ); 
									i != theGroupLogic.EndGroup(); i = theGroupLogic.Next( i ) )
					{
						pScripts->AddObjToScriptGroup( theGroupLogic.GetGroupUnit( i ), scriptId );
					}
				}

				return 0;
			}
			
			break;
		case ACTION_COMMAND_TAKE_ARTILLERY:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_TAKE_ARTILLERY command : the third parameter is not a script id", 0 );
			{
				const int targetId = script.GetObject( 3 );
				pScripts->DelInvalidBegin( targetId );

				if ( !pScripts->groups[targetId].empty() )
					command.pObject = pScripts->groups[targetId].back();
				else
					bValid = false;
			}

			break;
		case ACTION_COMMAND_LOAD_NOW:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_LOAD_NOW command : the third parameter is not a script id", 0 );

			{
				const int targetId = script.GetObject( 3 );
				pScripts->DelInvalidBegin( targetId );

				if ( !pScripts->groups[targetId].empty() )
					command.pObject = pScripts->groups[targetId].back();
				else
					bValid = false;
			}

			break;
		case ACTION_COMMAND_UNLOAD_NOW:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_UNLOAD_NOW command : the third parameter is not an X coordinate", 0 );
			CHECK_ERROR( script.IsNumber( 4 ), "Give ACTION_COMMAND_UNLOAD_NOW command : the fourth parameter is not an Y coordinate", 0 );

			command.vPos.x = script.GetObject( 3 );
			command.vPos.y = script.GetObject( 4 );

			break;
		case ACTION_COMMAND_IDLE_BUILDING:
			CHECK_ERROR( script.IsNumber( 3 ), "ACTION_COMMAND_IDLE_BUILDING command : the third parameter is not a script id", 0 );

			{
				const int targetId = script.GetObject( 3 );
				pScripts->DelInvalidBegin( targetId );

				if ( !pScripts->groups[targetId].empty() )
					command.pObject = pScripts->groups[targetId].back();
				else
					bValid = false;
			}

			break;
		case ACTION_COMMAND_IDLE_TRENCH:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_IDLE_TRENCH command : the third parameter is not a script id", 0 );

			{
				const int targetId = script.GetObject( 3 );
				pScripts->DelInvalidBegin( targetId );

				if ( !pScripts->groups[targetId].empty() )
					command.pObject = pScripts->groups[targetId].back();
				else
					bValid = false;
			}

			break;
		case ACTION_COMMAND_LEAVE_NOW:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_LEAVE_NOW command : the third parameter is not an X coordinate", 0 );
			CHECK_ERROR( script.IsNumber( 4 ), "Give ACTION_COMMAND_LEAVE_NOW command : the fourth parameter is not an Y coordinate", 0 );

			command.vPos.x = script.GetObject( 3 );
			command.vPos.y = script.GetObject( 4 );

			break;
		case ACTION_COMMAND_DISAPPEAR:

			break;
		case ACTION_COMMAND_PLACEMINE:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_PLACEMINE command : the third parameter is not an X coordinate", 0 );
			CHECK_ERROR( script.IsNumber( 4 ), "Give ACTION_COMMAND_PLACEMINE command : the fourth parameter is not an Y coordinate", 0 );
				
			command.vPos.x = script.GetObject( 3 );
			command.vPos.y = script.GetObject( 4 );

			break;
		case ACTION_COMMAND_PARADE:

			break;
		case ACTION_COMMAND_AMBUSH:

			break;
		case ACTION_COMMAND_ART_BOMBARDMENT:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_ART_BOMBARDMENT command : the third parameter is not an X coordinate", 0 );
			CHECK_ERROR( script.IsNumber( 4 ), "Give ACTION_COMMAND_ART_BOMBARDMENT command : the fourth parameter is not an Y coordinate", 0 );

			command.vPos.x = script.GetObject( 3 );
			command.vPos.y = script.GetObject( 4 );

			break;
		case ACTION_COMMAND_RANGE_AREA:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_ART_BOMBARDMENT command : the third parameter is not an X coordinate", 0 );
			CHECK_ERROR( script.IsNumber( 4 ), "Give ACTION_COMMAND_ART_BOMBARDMENT command : the fourth parameter is not an Y coordinate", 0 );

			command.vPos.x = script.GetObject( 3 );
			command.vPos.y = script.GetObject( 4 );

			break;
		case ACTION_COMMAND_INSTALL:

			break;
		case ACTION_COMMAND_UNINSTALL:

			break;
		case ACTION_COMMAND_USE_SPYGLASS:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_USE_SPYGLASS command : the third parameter is not an X coordinate", 0 );
			CHECK_ERROR( script.IsNumber( 4 ), "Give ACTION_COMMAND_USE_SPYGLASS command : the fourth parameter is not an Y coordinate", 0 );

			command.vPos.x = script.GetObject( 3 );
			command.vPos.y = script.GetObject( 4 );

			break;
		case ACTION_COMMAND_DISBAND_FORMATION:

			break;
		case ACTION_COMMAND_FORM_FORMATION:

			break;
		case ACTION_COMMAND_FOLLOW:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_FOLLOW command : the third parameter is not a script id", 0 );

			{
				const int targetId = script.GetObject( 3 );
				pScripts->DelInvalidBegin( targetId );

				if ( !pScripts->groups[targetId].empty() )
					command.pObject = pScripts->groups[targetId].back();
				else
					bValid = false;
			}

			break;
		case ACTION_COMMAND_STAND_GROUND:

			break;
		case ACTION_COMMAND_DEPLOY_ARTILLERY:
			CHECK_ERROR( script.IsNumber( 3 ), "Give ACTION_COMMAND_DEPLOY_ARTILLERY command : the third parameter is not an X coordinate", 0 );
			CHECK_ERROR( script.IsNumber( 4 ), "Give ACTION_COMMAND_DEPLOY_ARTILLERY command : the fourth parameter is not an Y coordinate", 0 );

			command.vPos.x = script.GetObject( 3 );
			command.vPos.y = script.GetObject( 4 );
			command.fNumber = 0;

			break;
		default:
			CHECK_ERROR( false, NStr::Format( "Unknown command %d", command.cmdType ), 0 );
	}

	if ( bValid )
	{
		if ( nGroup != -1 )
		{
			pAILogic->GroupCommand( &command, nGroup, bPlaceInQueue );
			pAILogic->UnregisterGroup( nGroup );
		}
		else
		{
			CHECK_ERROR( bAviationCallCommand, NStr::Format( "Script group %d not found", scriptId ), 0 );
			
			nGroup = pAILogic->GenerateGroupNumber();
			pAILogic->UnitCommand( &command, nGroup, nPlayer );
			if ( scriptId != -1 ) // ��������� ����� ��� ����� � �������� �� � ������
			{
				for ( int i = theGroupLogic.BeginGroup( nGroup ); i != theGroupLogic.EndGroup(); i = theGroupLogic.Next( i ) )
					pScripts->AddObjToScriptGroup( theGroupLogic.GetGroupUnit( i ), scriptId );
			}
		}
	}

	for ( std::list< std::pair<CCommonUnit*, int> >::iterator iter = oldUnitsGroups.begin(); iter != oldUnitsGroups.end(); ++iter )
		theGroupLogic.AddUnitToGroup( iter->first, iter->second );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GiveCommand( struct lua_State *state )
{
	return ProcessCommand( state, false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GiveQCommand( struct lua_State *state )
{
	return ProcessCommand( state, true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::ShowActiveScripts( struct lua_State *state )
{
	for ( std::unordered_map<std::string, int>::iterator iter = pScripts->name2script.begin(); iter != pScripts->name2script.end(); ++iter )
		pScripts->pConsole->WriteASCII( CONSOLE_STREAM_CONSOLE, iter->first.c_str(), 0xff00ff00 );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNUnitsInArea( struct lua_State *state )
{
	Script script( state );
	
	CHECK_ERROR( script.IsNumber( 1 ), "GetNUnitsInArea: the first parameter is not a number", 1 );
	CHECK_ERROR( script.IsString( 2 ), "GetNUnitsInArea: the second parameter is not a string", 1 );

	const int nPlayer = script.GetObject( 1 );
	const std::string szName = script.GetObject( 2 );

	CHECK_ERROR( pScripts->areas.find( szName ) != pScripts->areas.end(), NStr::Format( "GetNUnitsInArea: wrong script area name (%s)", szName.c_str() ), 1 );

	const SScriptArea &area = pScripts->areas[szName];
	int nResult = 0;
	if ( area.eType == SScriptArea::EAT_CIRCLE ) 
	{
		for ( CUnitsIter<0,2> iter( theDipl.GetNParty( nPlayer ), EDI_FRIEND, area.center, area.fR ); !iter.IsFinished(); iter.Iterate() )
		{
			CAIUnit *pUnit = *iter;
			if ( pUnit->IsValid() && pUnit->IsAlive() &&
					 pUnit->GetPlayer() == nPlayer && !pUnit->GetStats()->IsAviation() && 
					 pUnit->GetUnitRect().IsIntersectCircle( area.center, area.fR ) )
				++nResult;
		}
	}
	else
	{
		SRect areaRect;
		areaRect.InitRect( area.center, CVec2( 1, 0 ), area.vAABBHalfSize.x, area.vAABBHalfSize.y );
		for ( CUnitsIter<0,1> iter( theDipl.GetNParty( nPlayer ), EDI_FRIEND, area.center, Max( area.vAABBHalfSize.x, area.vAABBHalfSize.y ) ); !iter.IsFinished(); iter.Iterate() )
		{
			CAIUnit *pUnit = *iter;
			if ( pUnit->IsValid() && pUnit->IsAlive() &&
				   pUnit->GetPlayer() == nPlayer && !pUnit->GetStats()->IsAviation() && 
					 pUnit->GetUnitRect().IsIntersected( areaRect ) )
				++nResult;
		}
	}

	script.PushNumber( nResult );	
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNScriptUnitsInArea( struct lua_State *state )
{
	Script script( state );
	
	CHECK_ERROR( script.IsNumber( 1 ), "GetNScriptUnitsInArea: the first parameter is not a number", 1 );
	CHECK_ERROR( script.IsString( 2 ), "GetNScriptUnitsInArea: the second parameter is not a string", 1 );

	const int nScriptGroup = script.GetObject( 1 );
	const std::string szName = script.GetObject( 2 );

	CHECK_ERROR( pScripts->areas.find( szName ) != pScripts->areas.end(), NStr::Format( "GetNScriptUnitsInArea: wrong script area name (%s)", szName.c_str() ), 1 );
	CHECK_ERROR( nScriptGroup >= 0, NStr::Format( "GetNScriptUnitsInArea: wrong number of script group (%d)", nScriptGroup ), 1 );

	pScripts->DelInvalidUnits( nScriptGroup );
	if ( pScripts->groups.find( nScriptGroup ) == pScripts->groups.end() || pScripts->groups[nScriptGroup].empty() )
		script.PushNumber( 0 );
	else
	{
		const SScriptArea &area = pScripts->areas[szName];
		int nResult = 0;
		if ( area.eType == SScriptArea::EAT_CIRCLE ) 
		{
			// CRAP{ ����� ���������� static objects � �����, � ���������� �� dynamic_cast
			for ( std::list< CPtr<IUpdatableObj> >::iterator iter = pScripts->groups[nScriptGroup].begin(); iter != pScripts->groups[nScriptGroup].end(); ++iter )
			{
				if ( CFormation *pFormation = dynamic_cast_ptr<CFormation*>( *iter ) )
				{
					for ( int i = 0; i < pFormation->Size(); ++i )
					{
						if ( (*pFormation)[i]->GetUnitRect().IsIntersectCircle( area.center, area.fR ) )
						{
							++nResult;
							break;
						}
					}
				}
				else if ( CAIUnit *pUnit = dynamic_cast_ptr<CAIUnit*>( *iter ) )
				{
					if ( pUnit->GetUnitRect().IsIntersectCircle( area.center, area.fR ) )
						++nResult;
				}
			}
			// }CRAP
		}
		else
		{
			SRect areaRect;
			areaRect.InitRect( area.center, CVec2( 1, 0 ), area.vAABBHalfSize.x, area.vAABBHalfSize.y );
			// CRAP{ ����� ���������� static objects � �����, � ���������� �� dynamic_cast
			for ( std::list<CPtr<IUpdatableObj> >::iterator iter = pScripts->groups[nScriptGroup].begin(); iter != pScripts->groups[nScriptGroup].end(); ++iter )
			{
				if ( CFormation *pFormation = dynamic_cast_ptr<CFormation*>( *iter ) )
				{
					for ( int i = 0; i < pFormation->Size(); ++i )
					{
						if ( (*pFormation)[i]->GetUnitRect().IsIntersected( areaRect ) )
						{
							++nResult;
							break;
						}
					}
				}
				else if ( CAIUnit *pUnit = dynamic_cast_ptr<CAIUnit*>( *iter ) )
				{
					if ( pUnit->GetUnitRect().IsIntersected( areaRect ) )
						++nResult;
				}
			}
			// }CRAP
		}

		script.PushNumber( nResult );	
	}
		
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::ChangeWarFog( struct lua_State *state )
{
	Script script( state );
	
	CHECK_ERROR( script.IsNumber( 1 ), "ChangeWarFog: the first parameter is not a number", 0 );

	const int nParty = script.GetObject( 1 );
	CHECK_ERROR( nParty < 3, NStr::Format( "ChangeWarFog: wrong number of party (%d)", nParty ), 0 );

	theCheats.SetNPartyForWarFog( nParty, false );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::ChangePlayer( struct lua_State *state )
{
	Script script( state );
	
	CHECK_ERROR( script.IsNumber( 1 ), "ChangePlayer: the first parameter is not a number", 0 );
	CHECK_ERROR( script.IsNumber( 2 ), "ChangePlayer: the second parameter is not a number", 0 );

	const int scriptID = script.GetObject( 1 );
	const int nPlayer = script.GetObject( 2 );

	CHECK_ERROR( pScripts->groups.find( scriptID ) != pScripts->groups.end(), NStr::Format( "ChangePlayer: wrong number of script group (%d)", scriptID ), 0 );
	CHECK_ERROR( nPlayer < theDipl.GetNPlayers(), NStr::Format( "ChangeParty: wrong number of party (%d)", nPlayer ), 0 );

	pScripts->DelInvalidUnits( scriptID );

	for ( std::list< CPtr<IUpdatableObj> >::iterator iter = pScripts->groups[scriptID].begin(); iter != pScripts->groups[scriptID].end(); ++iter )
	{
		CHECK_ERROR( dynamic_cast_ptr<CCommonUnit*>(*iter) != 0, "Request to change player of non-unit", 0 );
		static_cast_ptr<CCommonUnit*>(*iter)->ChangePlayer( nPlayer );
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::God( struct lua_State *state )
{
	Script script( state );
	
	CHECK_ERROR( script.IsNumber( 1 ), "God: the first parameter is not a number", 0 );
	CHECK_ERROR( script.IsNumber( 2 ), "God: the second parameter is not a number", 0 );

	const int nPlayer = script.GetObject( 1 );
	const int nMode = script.GetObject( 2 );

	CHECK_ERROR( nPlayer >= 0 && nPlayer < theDipl.GetNPlayers(), NStr::Format( "God: wrong nubmer of party (%d), total number of parties (%d)", nPlayer, theDipl.GetNPlayers() ), 0 );
	CHECK_ERROR( nMode >= 0 && nMode <= 5, NStr::Format( "God: wrong nubmer of mode (%d), total number of modes (%d)", nMode, 5 ), 0 );

	// nMode = 0 - ����� god mode ���������
	// nMode = 1 - �������������
	// nMode = 2 - ������������� � ����������� �������
	// nMode = 3 - ����������� �������
	// nMode = 4 - ����� ������ �������������
	// nMode = 5 - ����� ������ ����������� �������

	switch ( nMode )
	{
		case 0: theCheats.SetImmortals(nPlayer, 0 ); theCheats.SetFirstShoot( nPlayer, 0 ); break;
		case 1: theCheats.SetImmortals( nPlayer, 1 ); break;
		case 2: theCheats.SetImmortals( nPlayer, 1 ); theCheats.SetFirstShoot( nPlayer, 1 ); break;
		case 3: theCheats.SetFirstShoot( nPlayer, 1 ); break;
		case 4: theCheats.SetImmortals( nPlayer, 0 ); break;
		case 5: theCheats.SetFirstShoot( nPlayer, 0 ); break;
	}
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::SetIGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	CHECK_ERROR( script.IsString( 1 ), "SetIGlobalVar: the first parameter is not a string", 0 );
	CHECK_ERROR( script.IsNumber( 2 ), "SetIGlobalVar: the second parameter is not a number", 0 );

	SetGlobalVar( script.GetObject( 1 ), int(script.GetObject( 2 )) );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::SetFGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	CHECK_ERROR( script.IsString( 1 ), "SetFGlobalVar: the first parameter is not a string", 0 );
	CHECK_ERROR( script.IsNumber( 2 ), "SetFGlobalVar: the second parameter is not a number", 0 );

	SetGlobalVar( script.GetObject( 1 ), float(script.GetObject( 2 )) );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::SetSGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	CHECK_ERROR( script.IsString( 1 ), "SetSGlobalVar: the first parameter is not a string", 0 );
	CHECK_ERROR( script.IsString( 2 ), "SetSGlobalVar: the second parameter is not a number", 0 );

	SetGlobalVar( script.GetObject( 1 ), (const char *)(script.GetObject( 2 )) );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetIGlobalVar( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsString( 1 ), "GetIGlobalVar: the first parameter is not a string", 1 );
	CHECK_ERROR( script.IsNumber( 2 ), "GetIGlobalVar: the second parameter is not a number", 1 );

	script.PushNumber( GetGlobalVar( script.GetObject(1), int(script.GetObject(2)) ) );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetFGlobalVar( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsString( 1 ), "GetFGlobalVar: the first parameter is not a string", 1 );
	CHECK_ERROR( script.IsString( 2 ), "GetFGlobalVar: the second parameter is not a number", 1 );

	script.PushNumber( GetGlobalVar( script.GetObject(1), float(script.GetObject(2)) ) );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetSGlobalVar( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsString( 1 ), "GetSGlobalVar: the first parameter is not a string", 1 );
	CHECK_ERROR( script.IsString( 2 ), "GetSGlobalVar: the second parameter is not a string", 1 );

	script.PushString( GetGlobalVar( script.GetObject(1), (const char*)(script.GetObject(2)) ) );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetObjectHPs( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsNumber( 1 ), "GetObjectHPs: the first parameter is not a number", 1 );

	const int scriptId = script.GetObject( 1 );

	CHECK_ERROR( pScripts->groups.find( scriptId ) != pScripts->groups.end(), NStr::Format( "GetObjectHPs: wrong script id, %d", scriptId ), 0 );
	pScripts->DelInvalidUnits( scriptId );

	CHECK_ERROR( !pScripts->groups[scriptId].empty(), "GetObjectHPs: empty script group", 1 );
	CHECK_ERROR( dynamic_cast_ptr<CStaticObject*>(pScripts->groups[scriptId].front()) != 0, "GetObjectHPs: not static object in script group", 1 );

	script.PushNumber( static_cast_ptr<CStaticObject*>(pScripts->groups[scriptId].front())->GetHitPoints() );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNUnitsInParty( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsNumber( 1 ), "GetNUnitsInParty: the first parameter is not a number", 1 );
	const int nParty = script.GetObject( 1 );
	CHECK_ERROR( nParty < 3, NStr::Format( "GetNUnitsInParty: wrong number of party (%d)", nParty ), 1 );

	int cnt = 0;
	for ( CGlobalIter iter( nParty, EDI_FRIEND ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = *iter;
		if ( pUnit->IsValid() && pUnit->IsAlive() )
		{
			IUnitState *pState = pUnit->GetStats()->IsInfantry() ? pUnit->GetFormation()->GetState() : pUnit->GetState();
			if ( !pState || pState->GetName() != EUSN_GUN_CREW_STATE )
				++cnt;
		}
	}

	script.PushNumber( cnt );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNUnitsInPartyUF( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsNumber( 1 ), "GetNUnitsInParty: the first parameter is not a number", 1 );
	const int nParty = script.GetObject( 1 );
	CHECK_ERROR( nParty < 3, NStr::Format( "GetNUnitsInParty: wrong number of party (%d)", nParty ), 1 );

	int cnt = 0;
	for ( CGlobalIter iter( nParty, EDI_FRIEND ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = *iter;
		if ( pUnit->IsValid() && pUnit->IsAlive() && ( !pUnit->GetStats()->IsInfantry() || pUnit == (*pUnit->GetFormation())[0] ) )
		{
			IUnitState *pState = pUnit->GetStats()->IsInfantry() ? pUnit->GetFormation()->GetState() : pUnit->GetState();
			if ( !pState || pState->GetName() != EUSN_GUN_CREW_STATE )
				++cnt;
		}
	}

	script.PushNumber( cnt );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNUnitsInPlayerUF( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsNumber( 1 ), "GetNUnitsInParty: the first parameter is not a number", 1 );
	const int nPlayer = script.GetObject( 1 );

	int cnt = 0;
	for ( CGlobalIter iter( theDipl.GetNParty( nPlayer ), EDI_FRIEND ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = *iter;
		if ( pUnit->IsValid() && pUnit->IsAlive() && pUnit->GetPlayer() == nPlayer &&
				 ( !pUnit->GetStats()->IsInfantry() || pUnit == (*pUnit->GetFormation())[0] ) )
		{
			IUnitState *pState = pUnit->GetStats()->IsInfantry() ? pUnit->GetFormation()->GetState() : pUnit->GetState();
			if ( !pState || pState->GetName() != EUSN_GUN_CREW_STATE )
				++cnt;
		}
	}

	script.PushNumber( cnt );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GetTraceFormatResult( Script *pScript, std::string *pResult )
{
	const char *pStr = pScript->GetObject( 1 );
	
	switch ( pScript->GetTop() )
	{
		case 1:	*pResult = pStr;
			break;
		case 2: *pResult = NStr::Format( pStr, float(pScript->GetObject( 2 )) );
			break;
		case 3: *pResult = NStr::Format( pStr, float(pScript->GetObject( 2 )), float(pScript->GetObject( 3 )) );
			break;
		case 4: *pResult = NStr::Format( pStr, float(pScript->GetObject( 2 )), float(pScript->GetObject( 3 )), float(pScript->GetObject( 4 )) );
			break;
		case 5: *pResult = NStr::Format( pStr, float(pScript->GetObject( 2 )), float(pScript->GetObject( 3 )), float(pScript->GetObject( 4 )), 
																	float(pScript->GetObject( 5 )) );
			break;
		case 6: *pResult = NStr::Format( pStr, float(pScript->GetObject( 2 )), float(pScript->GetObject( 3 )), float(pScript->GetObject( 4 )), 
																	float(pScript->GetObject( 5 )), float(pScript->GetObject( 6 )) );
			break;
		case 7: *pResult = NStr::Format( pStr, float(pScript->GetObject( 2 )), float(pScript->GetObject( 3 )), float(pScript->GetObject( 4 )), 
																	float(pScript->GetObject( 5 )), float(pScript->GetObject( 6 )), float(pScript->GetObject( 7 )) );
			break;
		case 8: *pResult = NStr::Format( pStr, float(pScript->GetObject( 2 )), float(pScript->GetObject( 3 )), float(pScript->GetObject( 4 )), 
																	float(pScript->GetObject( 5 )), float(pScript->GetObject( 6 )), float(pScript->GetObject( 7 )), float(pScript->GetObject( 8 )) );
			break;
		case 9: *pResult = NStr::Format( pStr, float(pScript->GetObject( 2 )), float(pScript->GetObject( 3 )), float(pScript->GetObject( 4 )), 
																	float(pScript->GetObject( 5 )), float(pScript->GetObject( 6 )), float(pScript->GetObject( 7 )), float(pScript->GetObject( 8 )), float(pScript->GetObject( 9 )) );
			break;
		case 10: *pResult = NStr::Format( pStr, float(pScript->GetObject( 2 )), float(pScript->GetObject( 3 )), float(pScript->GetObject( 4 )), 
																	float(pScript->GetObject( 5 )), float(pScript->GetObject( 6 )), float(pScript->GetObject( 7 )), float(pScript->GetObject( 8 )), float(pScript->GetObject( 9 )), float(pScript->GetObject( 10 )) );
			break;
		default:
			break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::Trace( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsString( 1 ), "Trace: the first parameter is not a number", 0 );
	for ( int i = 2; i <= script.GetTop(); ++i )
		CHECK_ERROR( script.IsNumber( i ), NStr::Format( "Trace: the %d parameter is not a number", i ), 0 );

	std::string result;	
	GetTraceFormatResult( &script, &result );

	pScripts->pConsole->WriteASCII( CONSOLE_STREAM_CONSOLE, result.c_str(), 0xff00ff00 );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::DisplayTrace( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsString( 1 ), "DisplayTrace: the first parameter is not a number", 0 );
	for ( int i = 2; i <= script.GetTop(); ++i )
		CHECK_ERROR( script.IsNumber( i ), NStr::Format( "DisplayTrace: the %d parameter is not a number", i ), 0 );

	std::string result;	
	GetTraceFormatResult( &script, &result );

	const std::string szSeasonName = GetGlobalVar( "World.Season" );
	const DWORD dwColor = GetGlobalVar( ("Scene.Colors." + szSeasonName + ".Text.Chat.Color").c_str(), int(0xffffffff) );
	GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, result.c_str(), dwColor, false );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::ChangeFormation( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsNumber( 1 ), "ChangeFormationOrder: first parameter isn't a number", 0 );
	CHECK_ERROR( script.IsNumber( 2 ), "ChangeFormationOrder: second parameter isn't a number", 0 );

	const int nScriptID = script.GetObject( 1 );
	const int nGeometry = script.GetObject( 2 );

	CHECK_ERROR( pScripts->groups.find( nScriptID ) != pScripts->groups.end(), NStr::Format( "ChangeFormationOrder: wrong script id (%d)", nScriptID ), 0 );
	pScripts->DelInvalidUnits( nScriptID );

	for ( std::list<CPtr<IUpdatableObj> >::iterator iter = pScripts->groups[nScriptID].begin(); iter != pScripts->groups[nScriptID].end(); ++iter )
	{
		if ( CFormation *pFormation = dynamic_cast_ptr<CFormation*>(*iter) )
		{
			if ( nGeometry < pFormation->GetNGeometries() )
				pFormation->ChangeGeometry( nGeometry );
		}
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::ObjectiveChanged( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsNumber( 1 ), "ObjectiveChanged: first parameter isn't a number", 0 );
	CHECK_ERROR( script.IsNumber( 2 ), "ObjectiveChanged: second parameter isn't a number", 0 );

	const int nObj = script.GetObject( 1 );
	const int nValue = script.GetObject( 2 );

	CHECK_ERROR( nObj >= 0 && nObj < 255, NStr::Format( "ObjectiveChanged: wrong number of objective (%d)", nObj ), 0 );
	CHECK_ERROR( nValue >= 0 && nValue < 255, NStr::Format( "ObjectiveChanged: wrong value of objective (%d)", nValue ), 0 );

	updater.AddFeedBack( SAIFeedBack( EFB_OBJECTIVE_CHANGED, ( nObj << 8 ) | nValue ) );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNAmmo( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsNumber( 1 ), "GetNAmmo: first paramter isn't a nubmer", 2 );

	const int nScriptID = script.GetObject( 1 );
	CHECK_ERROR( pScripts->groups.find( nScriptID ) != pScripts->groups.end(), NStr::Format( "GetNAmmo: wrong script id (%d)", nScriptID ), 2 );
	pScripts->DelInvalidBegin( nScriptID );

	if ( pScripts->groups[nScriptID].empty() )
	{
		script.PushNumber( 0 );
		script.PushNumber( 0 );
	}
	else
	{
		IUpdatableObj *pObj = *(pScripts->groups[nScriptID].begin());
		CHECK_ERROR( dynamic_cast<CAIUnit*>(pObj) != 0, NStr::Format( "GetNAmmo: first object in script group (%d) is not a unit", nScriptID ), 2 );

		CAIUnit *pUnit = dynamic_cast<CAIUnit*>(pObj);

		int nMainAmmo = 0;
		int nSecondaryAmmo = 0;
		for ( int i = 0; i < pUnit->GetNCommonGuns(); ++i )
		{
			if ( pUnit->GetCommonGunStats( i ).bPrimary )
				nMainAmmo += pUnit->GetNAmmo( i );
			else
				nSecondaryAmmo += pUnit->GetNAmmo( i );
		}

		script.PushNumber( nMainAmmo );
		script.PushNumber( nSecondaryAmmo );
	}

	return 2;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetPartyOfUnits( struct lua_State *state )
{
	Script script( state );

	CHECK_ERROR( script.IsNumber( 1 ), "GetPartyOfUnits: first parameter isn't a number", 1 );

	const int nScriptID = script.GetObject( 1 );
	CHECK_ERROR( pScripts->groups.find( nScriptID ) != pScripts->groups.end(), NStr::Format( "GetPartyOfUnits: wrong script id (%d)", nScriptID ), 1 );

	IUpdatableObj *pObj = *(pScripts->groups[nScriptID].begin());
	CHECK_ERROR( dynamic_cast<CCommonUnit*>(pObj) != 0, NStr::Format( "GetPartyOfUnits: first object in script group (%d) is not a unit", nScriptID ), 1 );

	CCommonUnit *pUnit = dynamic_cast<CCommonUnit*>(pObj);
	script.PushNumber( pUnit->GetParty() );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::DamageObject( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "DamageObject: first parameter isn't a number", 0 );
	CHECK_ERROR( script.IsNumber( 2 ), "DamageObject: second parameter isn't a number", 0 );

	const int nScriptID = script.GetObject( 1 );
	CHECK_ERROR( pScripts->groups.find( nScriptID ) != pScripts->groups.end(), NStr::Format( "DamageObject: wrong script id (%d)", nScriptID ), 0 );

	pScripts->DelInvalidUnits( nScriptID );

	const float fDamage = script.GetObject( 2 );

	for ( std::list<CPtr<IUpdatableObj> >::iterator iter = pScripts->groups[nScriptID].begin(); iter != pScripts->groups[nScriptID].end(); ++iter )
	{
		IUpdatableObj *pUpdatableObj = *iter;
		if ( CStaticObject *pObj = dynamic_cast<CStaticObject*>(pUpdatableObj) )
		{
			if ( fDamage == 0.0f )
				pObj->TakeDamage( pObj->GetHitPoints() * 2, true, theDipl.GetNeutralPlayer(), 0 );
			else if ( fDamage < 0.0f )
				pObj->SetHitPoints( pObj->GetHitPoints() - fDamage );
			else
				pObj->TakeDamage( fDamage, true, theDipl.GetNeutralPlayer(), 0 );
		}
		else if ( CAIUnit *pUnit = dynamic_cast<CAIUnit*>(pUpdatableObj) )
		{
			if ( fDamage == 0.0f )
				pUnit->TakeDamage( pUnit->GetHitPoints() * 2, 0, theDipl.GetNeutralPlayer(), 0 );
			else if ( fDamage < 0.0f )
				pUnit->IncreaseHitPoints( -fDamage );
			else
				pUnit->TakeDamage( fDamage, 0, theDipl.GetNeutralPlayer(), 0 );
		}
		else if ( CFormation *pFormation = dynamic_cast<CFormation*>(pUpdatableObj) )
		{
			std::list<CSoldier*> soldiers;
			for ( int i = 0; i < pFormation->Size(); ++i )
				soldiers.push_back( (*pFormation)[i] );
			for ( std::list<CSoldier*>::iterator iter = soldiers.begin(); iter != soldiers.end(); ++iter )
			{
				CSoldier *pSoldier = *iter;

				if ( fDamage == 0.0f )
					pSoldier->TakeDamage( pSoldier->GetHitPoints() * 2, 0, theDipl.GetNeutralPlayer(), 0 );
				else if ( fDamage < 0.0f )
					pSoldier->IncreaseHitPoints( -fDamage );
				else
					pSoldier->TakeDamage( fDamage, 0, theDipl.GetNeutralPlayer(), 0 );
			}
		}
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::CallAssert( struct lua_State *pState )
{
	NI_ASSERT_T( false, "You are welcome!" );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetUnitState( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "GetUnitState: first parameter isn't a number", 1 );
	const int nScriptID = script.GetObject( 1 );

	pScripts->DelInvalidUnits( nScriptID );
	
	if ( pScripts->groups.find( nScriptID ) == pScripts->groups.end() || pScripts->groups[nScriptID].empty() )
		script.PushNumber( -1 );
	else
	{
		IUpdatableObj *pObj = pScripts->groups[nScriptID].front();
		CHECK_ERROR( dynamic_cast<CQueueUnit*>(pObj) != 0, "GetUnitState: scriptID object isn't a unit", 1 );
		CQueueUnit *pUnit = dynamic_cast<CQueueUnit*>(pObj);

		IUnitState *pState = pUnit->GetState();

		if ( pState == 0 || !pState->IsValid() )
			script.PushNumber( 0 );
		else
			script.PushNumber( pState->GetName() );
	}

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetSquadInfo( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "GetSquadInfo: first parameters isn't a number", 1 );
	const int nScriptID = script.GetObject( 1 );

	pScripts->DelInvalidUnits( nScriptID );
	
	if ( pScripts->groups.find( nScriptID ) == pScripts->groups.end() || pScripts->groups[nScriptID].empty() )
		script.PushNumber( -3 );
	else
	{
		IUpdatableObj *pObj = pScripts->groups[nScriptID].front();
		CFormation *pFormation = dynamic_cast<CFormation*>( pObj );
		if ( pFormation == 0 )
			script.PushNumber( -2 );
		else if ( pFormation->IsDisabled() || pFormation->Size() == 1 && (*pFormation)[0]->GetMemFormation() != 0 )
			script.PushNumber( -1 );
		else
			script.PushNumber( pFormation->GetCurGeometry() );
	}

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::IsFollowing( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "IsFollowing: first parameters isn't a number", 1 );
	const int nScriptID = script.GetObject( 1 );

	pScripts->DelInvalidUnits( nScriptID );

	if ( pScripts->groups.find( nScriptID ) == pScripts->groups.end() || pScripts->groups[nScriptID].empty() )
		script.PushNumber( -1 );
	else
	{
		IUpdatableObj *pObj = pScripts->groups[nScriptID].front();
		CCommonUnit *pUnit = dynamic_cast<CCommonUnit*>(pObj);
		if ( pUnit == 0 )
			script.PushNumber( -1 );
		else if ( pUnit->IsInFollowState() )
			script.PushNumber( 1 );
		else
			script.PushNumber( 0 );
	}

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetFrontDir( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "IsFollowing: first parameters isn't a number", 1 );
	const int nScriptID = script.GetObject( 1 );

	pScripts->DelInvalidUnits( nScriptID );

	if ( pScripts->groups.find( nScriptID ) == pScripts->groups.end() || pScripts->groups[nScriptID].empty() )
		script.PushNumber( -1 );
	else
	{
		IUpdatableObj *pObj = pScripts->groups[nScriptID].front();
		IBasePathUnit *pUnit = dynamic_cast<IBasePathUnit*>( pObj );
		if ( pUnit == 0 )
			script.PushNumber( -1 );
		else
			script.PushNumber( pUnit->GetFrontDir() );
	}

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::IsWarehouseConnected( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "IsWarehouseConnected: first parameters isn't a number", 1 );
	const int nScriptID = script.GetObject( 1 );

	pScripts->DelInvalidUnits( nScriptID );
	CHECK_ERROR( pScripts->groups.find( nScriptID ) != pScripts->groups.end(), NStr::Format( "IsWarehouseConnected: object with scriptID (%d) doesn't exist", nScriptID ), 1 );
	CHECK_ERROR( !pScripts->groups[nScriptID].empty(), NStr::Format( "IsWarehouseConnected: object with scriptID (%d) doesn't exist", nScriptID ), 1 );

	IUpdatableObj *pObj = pScripts->groups[nScriptID].front();
	CBuildingStorage *pStorage = dynamic_cast<CBuildingStorage*>( pObj );

	CHECK_ERROR( pStorage != 0, NStr::Format( "IsWarehouseConnected: object with scriptID (%d) isn't a storage", nScriptID ), 1 );

	script.PushNumber( int( pStorage->IsConnected() ) );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::IsUnitUnderSupply( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "IsUnitUnderSupply: first parameters isn't a number", 1 );
	const int nScriptID = script.GetObject( 1 );

	pScripts->DelInvalidUnits( nScriptID );
	CHECK_ERROR( pScripts->groups.find( nScriptID ) != pScripts->groups.end(), NStr::Format( "IsUnitUnderSupply: object with scriptID (%d) doesn't exist", nScriptID ), 1 );
	CHECK_ERROR( !pScripts->groups[nScriptID].empty(), NStr::Format( "IsUnitUnderSupply: object with scriptID (%d) doesn't exist", nScriptID ), 1 );

	IUpdatableObj *pObj = pScripts->groups[nScriptID].front();
	CCommonUnit *pUnit = dynamic_cast<CCommonUnit*>( pObj );

	CHECK_ERROR( pUnit != 0, NStr::Format( "IsUnitUnderSupply: object with scriptID (%d) isn't a unit", nScriptID ), 1 );

	//script.PushNumber( (int)theStatObjs.IsPointUnderSupply( pUnit->GetPlayer(), pUnit->GetCenter() ) );
	script.PushNumber( 1 );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetUnitMorale( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "GetUnitMorale: first parameters isn't a number", 1 );
	const int nScriptID = script.GetObject( 1 );

	pScripts->DelInvalidUnits( nScriptID );
	CHECK_ERROR( pScripts->groups.find( nScriptID ) != pScripts->groups.end(), NStr::Format( "GetUnitMorale: object with scriptID (%d) doesn't exist", nScriptID ), 1 );
	CHECK_ERROR( !pScripts->groups[nScriptID].empty(), NStr::Format( "GetUnitMorale: object with scriptID (%d) doesn't exist", nScriptID ), 1 );

	IUpdatableObj *pObj = pScripts->groups[nScriptID].front();
	CAIUnit *pUnit = dynamic_cast<CAIUnit*>( pObj );

	CHECK_ERROR( pUnit != 0, NStr::Format( "GetUnitMorale: object with scriptID (%d) isn't a unit", nScriptID ), 1 );

	script.PushNumber( pUnit->GetMorale() );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetActiveShellType( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "GetActiveShellType: first parameter isn't a number", 1 );
	const int nScriptID = script.GetObject( 1 );

	pScripts->DelInvalidUnits( nScriptID );
	CHECK_ERROR( pScripts->groups.find( nScriptID ) != pScripts->groups.end(), NStr::Format( "GetActiveShellType: object with scriptID (%d) doesn't exist", nScriptID ), 1 );
	CHECK_ERROR( !pScripts->groups[nScriptID].empty(), NStr::Format( "GetActiveShellType: object with scriptID (%d) doesn't exist", nScriptID ), 1 );

	IUpdatableObj *pObj = pScripts->groups[nScriptID].front();
	CAIUnit *pUnit = dynamic_cast<CAIUnit*>( pObj );

	CHECK_ERROR( pUnit != 0, NStr::Format( "GetActiveShellType: object with scriptID (%d) isn't a unit", nScriptID ), 1 );

	script.PushNumber( pUnit->GetGuns()->GetActiveShellType() );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::AskClient( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsString( 1 ), "AskClient: first parameter isn't a string", 0 );
	std::string szRequest = script.GetObject( 1 );

	GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_WORLD, szRequest.c_str() );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::RandomFloat( struct lua_State *pState )
{
	Script script( pState );

	script.PushNumber( Random( 0.0f, 1.0f ) );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::RandomInt( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "RandomInt: the first parameter isn't a number", 1 );
	const int n = script.GetObject( 1 );
	CHECK_ERROR( n >= 1, NStr::Format( "RandomInt: upper parameter (%d) is too small", n ), 1 );

	if ( n == 1 )
		script.PushNumber( 0 );
	else
		script.PushNumber( Random( 0, n - 1 ) );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::ChangeSelection( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "ChangeSelection: first parameter isn't a number", 0 );
	const int nScriptID = script.GetObject( 1 );

	CHECK_ERROR( script.IsNumber( 2 ), "ChangeSelection: second parameters isn't a number", 0 );
	const int nParam = script.GetObject( 2 );

	pScripts->DelInvalidUnits( nScriptID );

	if ( pScripts->groups.find( nScriptID ) != pScripts->groups.end() )
	{
		for ( std::list<CPtr<IUpdatableObj> >::iterator iter = pScripts->groups[nScriptID].begin(); iter != pScripts->groups[nScriptID].end(); ++iter )
			updater.Update( ACTION_NOTIFY_CHANGE_SELECTION, *iter, nParam );
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::ReturnScriptIDs( struct lua_State *pState )
{
	Script script( pState );

	const int nReturns = script.GetTop();
	std::unordered_set<int> selectedUnits;
	for ( int i = 1; i <= nReturns; ++i )
	{
		NI_ASSERT_T( script.IsNumber( i ), "ReturnScriptIDs: %d parameter isn't a number" );
		
		const int nPtr = script.GetObject( i );
		IRefCount *pObj = reinterpret_cast<IRefCount*>( nPtr );

		NI_ASSERT_T( dynamic_cast<IUpdatableObj*>(pObj) != 0, "Unknown object passed" );
		IUpdatableObj *pUpdatableObject = dynamic_cast<IUpdatableObj*>(pObj);

		const int nScriptID = pScripts->GetScriptID( pUpdatableObject );
		if ( nScriptID != -1 )
			selectedUnits.insert( nScriptID );
	}

	while ( !selectedUnits.empty() )
	{
		const int nScriptID = *(selectedUnits.begin());
		selectedUnits.erase( nScriptID );

		pScripts->CallScriptFunction( NStr::Format( "GetSelectedUnitsFeedBack( %d )", nScriptID ) );
	}
						
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetPlayersMask( struct lua_State *pState )
{
	Script script( pState );

	DWORD wMask = 0;
	for ( int i = 0; i < theDipl.GetNPlayers(); ++i )
	{
		if ( theDipl.IsPlayerExist( i ) )
			wMask |= ( 1UL << i );
	}

	script.PushNumber( wMask );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetObjCoord( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "GetObjCoord: first parameter isn't a number", 2 );
	const int nScriptID = script.GetObject( 1 );

	pScripts->DelInvalidUnits( nScriptID );

	CVec2 vCenter( -1.0f, -1.0f );
	if ( pScripts->groups.find( nScriptID ) != pScripts->groups.end() && !pScripts->groups[nScriptID].empty() )
	{
		IUpdatableObj *pObj = pScripts->groups[nScriptID].front();
		if ( CStaticObject *pStaticObj = dynamic_cast<CStaticObject*>( pObj ) )
			vCenter = pStaticObj->GetCenter();
		else if ( IBasePathUnit *pPathUnit = dynamic_cast<IBasePathUnit*>( pObj ) )
			vCenter = pPathUnit->GetCenter();
	}

	script.PushNumber( vCenter.x );
	script.PushNumber( vCenter.y );

	return 2;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetScriptAreaParams( struct lua_State *pState )
{
	Script script( pState );

	const std::string szName = script.GetObject( 1 );

	CHECK_ERROR( pScripts->areas.find( szName ) != pScripts->areas.end(), NStr::Format( "GetScriptAreaParams: wrong script area name (%s)", szName.c_str() ), 4 );
	const SScriptArea &area = pScripts->areas[szName];

	script.PushNumber( area.center.x );
	script.PushNumber( area.center.y );

	if ( area.eType == SScriptArea::EAT_CIRCLE )
	{
		script.PushNumber( area.fR );
		script.PushNumber( area.fR );
	}
	else
	{
		script.PushNumber( area.vAABBHalfSize.x );
		script.PushNumber( area.vAABBHalfSize.y );
	}

	return 4;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::IsPlayerPresent( struct lua_State *pState )
{
	Script script( pState );

	const int nPlayer = script.GetObject( 1 );
	script.PushNumber( (int)theDipl.IsPlayerExist( nPlayer ) );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::SwitchWeather( struct lua_State *pState )
{
	Script script( pState );
	const bool bOn = ( script.GetObject( 1 ) == 1 );
	theWeather.Switch( bOn );
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::SwitchWeatherAutomatic( struct lua_State *pState )
{
	Script script( pState );
	const bool bOn = ( script.GetObject( 1 ) == 1 );
	theWeather.SwitchAutomatic( bOn );
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNUnitsInSide( struct lua_State *pState )
{
	Script script( pState );

	const int nParty = script.GetObject( 1 );
	CHECK_ERROR( nParty < 3, NStr::Format( "GetNUnitsInSide: wrong number of side (%d)", nParty ), 1 );

	script.PushNumber( units.Size( nParty ) );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::SetDifficultyLevel( struct lua_State *state )
{
	Script script( state );

	const int nLevel = script.GetObject( 1 );
	CHECK_ERROR( nLevel >= 0 && nLevel < 3, NStr::Format( "SetDifficultyLevel: lever (%d) not in range [0..2]", nLevel ), 0 );
	
	pAILogic->SetDifficultyLevel( nLevel );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::SetCheatDifficultyLevel( struct lua_State *state )
{
	Script script( state );

	const int nLevel = script.GetObject( 1 );
	CHECK_ERROR( nLevel >= 0 && nLevel < 3, NStr::Format( "SetCheatDifficultyLevel: lever (%d) not in range [0..2]", nLevel ), 0 );
	
	pAILogic->SetCheatDifficultyLevel( nLevel );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::DeleteReinforcement( struct lua_State *pState )
{
	Script script( pState );

	const int nScriptID = script.GetObject( 1 );

	CHECK_ERROR( pScripts->groups.find( nScriptID ) != pScripts->groups.end(), NStr::Format( "DeleteReinforcement: wrong number of script group, %d", nScriptID ), 1 );
	pScripts->DelInvalidUnits( nScriptID );

	for ( std::list<CPtr<IUpdatableObj> >::iterator it = pScripts->groups[nScriptID].begin();
				it != pScripts->groups[nScriptID].end(); ++it )
	{
		if ( CCommonUnit *pUnit = dynamic_cast_ptr<CCommonUnit*>(*it) )
			pUnit->Disappear();
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::ViewZone( struct lua_State *pState )
{
	if ( !theDipl.IsNetGame() )
	{
		Script script( pState );

		CHECK_ERROR( script.IsString( 1 ), "ViewZone: the first parameter is not a string", 0 );
		CHECK_ERROR( script.IsNumber( 2 ), "ViewZone: the second parameter is not a number", 0 );

		const std::string szName = script.GetObject( 1 );
		const int nShow = script.GetObject( 2 );

		CHECK_ERROR( pScripts->areas.find( szName ) != pScripts->areas.end(), NStr::Format( "ViewZone: wrong script area name (%s)", szName.c_str() ), 1 );

		const SScriptArea &area = pScripts->areas[szName];
		CHECK_ERROR( area.eType == SScriptArea::EAT_CIRCLE, NStr::Format( "ViewZone: wrong type of area %s", szName.c_str() ), 0 );

		theWarFog.ToggleOpen4ScriptAreaTiles( area, nShow == 1 );
	}
	else
	{
		CHECK_ERROR( false, "ViewZone: can't perform in multiplayer game", 0 );
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::IsStandGround( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "IsStandGround: first parameter isn't a number", 1 );
	const int nScriptID = script.GetObject( 1 );

	pScripts->DelInvalidUnits( nScriptID );

	int nAnswer = 0;
	if ( pScripts->groups.find( nScriptID ) != pScripts->groups.end() && !pScripts->groups[nScriptID].empty() )
	{
		if ( CCommonUnit *pUnit = dynamic_cast_ptr<CCommonUnit*>( pScripts->groups[nScriptID].front() ) )
			nAnswer = pUnit->GetBehaviour().moving == SBehaviour::EMHoldPos;
	}
	script.PushNumber( nAnswer );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::IsEntrenched( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "IsEntrenched: first parameter isn't a number", 2 );
	const int nScriptID = script.GetObject( 1 );

	pScripts->DelInvalidUnits( nScriptID );
	
	int nAnswer = 0;
	if ( pScripts->groups.find( nScriptID ) != pScripts->groups.end() && !pScripts->groups[nScriptID].empty() )
	{
		if ( CAIUnit *pUnit = dynamic_cast_ptr<CAIUnit*>( pScripts->groups[nScriptID].front() ) )
			nAnswer = pUnit->IsInTankPit();
	}
	script.PushNumber( nAnswer );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNMinesInScriptArea( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsString( 1 ), "GetNMinesInScriptArea: the second parameter is not a string", 1 );
	const std::string szName = script.GetObject( 1 );
	CHECK_ERROR( pScripts->areas.find( szName ) != pScripts->areas.end(), NStr::Format( "GetNMinesInScriptArea: wrong script area name (%s)", szName.c_str() ), 1 );

	const SScriptArea &area = pScripts->areas[szName];

	class CMinesCheck : public ICheckObjects
	{
		public:
			virtual bool IsGoodObj( CExistingObject *pObj ) const { return pObj->GetObjectType() == ESOT_MINE; }
	};

	CMinesCheck minesCheck;
	script.PushNumber( pScripts->GetCheckObjectsInScriptArea( area, minesCheck ) );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNTrenchesInScriptArea( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsString( 1 ), "GetNTrenchesInScriptArea: the second parameter is not a string", 1 );
	const std::string szName = script.GetObject( 1 );
	CHECK_ERROR( pScripts->areas.find( szName ) != pScripts->areas.end(), NStr::Format( "GetNTrenchesInScriptArea: wrong script area name (%s)", szName.c_str() ), 1 );

	const SScriptArea &area = pScripts->areas[szName];

	class CTrenchesCheck : public ICheckObjects
	{
		public:
			virtual bool IsGoodObj( CExistingObject *pObj ) const { return pObj->GetObjectType() == ESOT_ENTR_PART; }
	};

	CTrenchesCheck trenchesCheck;
	script.PushNumber( pScripts->GetCheckObjectsInScriptArea( area, trenchesCheck ) );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNFencesInScriptArea( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsString( 1 ), "GetNFencesInScriptArea: the second parameter is not a string", 1 );
	const std::string szName = script.GetObject( 1 );
	CHECK_ERROR( pScripts->areas.find( szName ) != pScripts->areas.end(), NStr::Format( "GetNFencesInScriptArea: wrong script area name (%s)", szName.c_str() ), 1 );

	const SScriptArea &area = pScripts->areas[szName];
	
	class CFencesCheck : public ICheckObjects
	{
		public:
			virtual bool IsGoodObj( CExistingObject *pObj ) const { return pObj->GetObjectType() == ESOT_FENCE; }
	};

	CFencesCheck fencesCheck;
	script.PushNumber( pScripts->GetCheckObjectsInScriptArea( area, fencesCheck ) );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNAntitankInScriptArea( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsString( 1 ), "GetNAntitankInScriptArea: the second parameter is not a string", 1 );
	const std::string szName = script.GetObject( 1 );
	CHECK_ERROR( pScripts->areas.find( szName ) != pScripts->areas.end(), NStr::Format( "GetNAntitankInScriptArea: wrong script area name (%s)", szName.c_str() ), 1 );

	const SScriptArea &area = pScripts->areas[szName];

	class CAntiTankCheck : public ICheckObjects
	{
		public:
			virtual bool IsGoodObj( CExistingObject *pObj ) const { return theUnitCreation.IsAntiTank( pObj->GetStats() ); }
	};
	
	CAntiTankCheck antitankCheck;
	script.PushNumber( pScripts->GetCheckObjectsInScriptArea( area, antitankCheck ) );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNAPFencesInScriptArea( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsString( 1 ), "GetNAPFencesInScriptArea: the second parameter is not a string", 1 );
	const std::string szName = script.GetObject( 1 );
	CHECK_ERROR( pScripts->areas.find( szName ) != pScripts->areas.end(), NStr::Format( "GetNAPFencesInScriptArea: wrong script area name (%s)", szName.c_str() ), 1 );

	const SScriptArea &area = pScripts->areas[szName];

	class CAntiTankCheck : public ICheckObjects
	{
		public:
			virtual bool IsGoodObj( CExistingObject *pObj ) const { return theUnitCreation.IsAPFence( pObj->GetStats() ); }
	};
	
	CAntiTankCheck antitankCheck;
	script.PushNumber( pScripts->GetCheckObjectsInScriptArea( area, antitankCheck ) );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::Password( struct lua_State *pState )
{
	Script script( pState );

	if ( script.IsString( 1 ) )
		theCheats.CheckPassword( std::string(script.GetObject( 1 )) );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::SetGameSpeed( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsNumber( 1 ), "SetGameSpeed: first parameters isn't a number", 0 );
	const int nSpeed = script.GetObject( 1 );

	GetSingleton<IGameTimer>()->SetSpeed( nSpeed );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetNUnitsOfType( struct lua_State *pState )
{
	Script script( pState );

	CHECK_ERROR( script.IsString( 1 ), "GetNUnitsOfType: first parameters isn't a string", 1 );
	CHECK_ERROR( script.IsNumber( 2 ), "GetNUnitsOfType: second parameters isn't a number", 1 );

	const std::string szType = script.GetObject( 1 );
	const int nParty = script.GetObject( 2 );

	CHECK_ERROR( nParty >= 0 && nParty < 3, NStr::Format( "GetNUnitsOfType: wrong number of party (%d)", nParty ), 1 );

	CPtr<IRPGStatsAutomagic> pAutoMagic = CreateObject<IRPGStatsAutomagic>( MAIN_AUTOMAGIC );
	const int nType = pAutoMagic->ToInt( szType.c_str() );

	CHECK_ERROR( nType != -1, NStr::Format( "GetNUnitsOfType: type %s not found", szType.c_str() ), 1 );
	script.PushNumber( units.GetNUnitsOfType( nParty, nType ) );

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::GetMapSize( struct lua_State *pState )
{
	Script script( pState );

	script.PushNumber( theStaticMap.GetSizeX() * SConsts::TILE_SIZE );
	script.PushNumber( theStaticMap.GetSizeY() * SConsts::TILE_SIZE );

	return 2;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#undef CHECK_ERROR
