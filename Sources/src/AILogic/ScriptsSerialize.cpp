#include "StdAfx.h"

#include "Scripts/Scripts.h"
extern CScripts *pScripts;
// The globals of the script's Lua state that hold a number or a string, by name.
static void GetScriptGlobals( lua_State *pState, std::vector<std::string> *pNumberNames, std::vector<double> *pNumberValues,
	std::vector<std::string> *pStringNames, std::vector<std::string> *pStringValues )
{
	if ( pState == 0 )
		return;
	std::map<std::string, double> numbers;
	std::map<std::string, std::string> strings;
	const int nTop = lua_gettop( pState );
	lua_getglobals( pState );
	const int nGlobals = lua_gettop( pState );
	lua_pushnil( pState );
	while ( lua_next( pState, nGlobals ) != 0 )
	{
		if ( lua_type( pState, -2 ) == LUA_TSTRING )
		{
			const std::string szName( lua_tostring( pState, -2 ), lua_strlen( pState, -2 ) );
			if ( lua_type( pState, -1 ) == LUA_TNUMBER )
				numbers[szName] = lua_tonumber( pState, -1 );
			else if ( lua_type( pState, -1 ) == LUA_TSTRING )
				strings[szName] = std::string( lua_tostring( pState, -1 ), lua_strlen( pState, -1 ) );
		}
		lua_pop( pState, 1 );
	}
	lua_settop( pState, nTop );
	for ( std::map<std::string, double>::iterator it = numbers.begin(); it != numbers.end(); ++it )
	{
		pNumberNames->push_back( it->first );
		pNumberValues->push_back( it->second );
	}
	for ( std::map<std::string, std::string>::iterator it = strings.begin(); it != strings.end(); ++it )
	{
		pStringNames->push_back( it->first );
		pStringValues->push_back( it->second );
	}
}
int CScripts::SScriptInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &period );
	saver.Add( 2, &lastUpdate );
	saver.Add( 3, &szName );
	saver.Add( 4, &nRepetitions );
	saver.Add( 5, &nOrder );

	return 0;
}
int CScripts::SReinforcementObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &mapObject );
	saver.Add( 2, &pStats );
	saver.Add( 3, &pScenarioUnit );
	
	return 0;
}
int CScripts::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( saver.IsReading() )
	{
		pScripts = this;
		script.Clear();
		script.Init();
	}

	if ( saver.IsReading() )
		pConsole = GetSingleton<IConsoleBuffer>();

	saver.Add( 3, &groups );
	saver.Add( 8, &szScriptFile );

	if ( !saver.IsReading() )
	{
		// In the order Segment runs them.
		std::vector< std::pair<int, int> > order;
		for ( std::unordered_map<int, SScriptInfo>::iterator iter = activeScripts.begin(); iter != activeScripts.end(); ++iter )
			order.push_back( std::make_pair( iter->second.nOrder, iter->first ) );
		std::sort( order.begin(), order.end() );
		std::list<SScriptInfo> activeScriptsInfo;
		for ( int i = 0; i < order.size(); ++i )
			activeScriptsInfo.push_back( activeScripts[order[i].second] );

		saver.Add( 9, &activeScriptsInfo );
		saver.Add( 18, &nNextScriptOrder );
	}
	else
	{
		std::list<SScriptInfo> activeScriptsInfo;
		saver.Add( 9, &activeScriptsInfo );
		// A save from before the run order keeps the order it ran them in.
		const bool bOrdered = saver->CountChunks( 18 ) != 0;
		nNextScriptOrder = 0;
		saver.Add( 18, &nNextScriptOrder );
		if ( !bOrdered )
		{
			int nOrder = 0;
			for ( std::list<SScriptInfo>::iterator iter = activeScriptsInfo.begin(); iter != activeScriptsInfo.end(); ++iter )
				iter->nOrder = nOrder++;
			nNextScriptOrder = nOrder;
		}

		activeScripts.clear();
		name2script.clear();

		if ( ReadScriptFile() )
		{
			for ( std::list<SScriptInfo>::iterator iter = activeScriptsInfo.begin(); iter != activeScriptsInfo.end(); ++iter )
			{
				script.GetGlobal( iter->szName.c_str() );
				const int nScriptRef = script.Ref( 1 );

				name2script[iter->szName] = nScriptRef;

				activeScripts[nScriptRef].szName = iter->szName;
				activeScripts[nScriptRef].period = iter->period;
				activeScripts[nScriptRef].lastUpdate = iter->lastUpdate;
				activeScripts[nScriptRef].nRepetitions = iter->nRepetitions;
				activeScripts[nScriptRef].nOrder = iter->nOrder;
			}
		}
	}

	// The script's own number and string globals, which a mission keeps its
	// state in (vitebsk.lua counts total_tanks): a load runs the file again,
	// which set them back to their first values. Tables and functions are the
	// file's, as they always were. A save without them leaves the file's.
	std::vector<std::string> numberNames, stringNames, stringValues;
	std::vector<double> numberValues;
	if ( !saver.IsReading() )
		GetScriptGlobals( script.GetState(), &numberNames, &numberValues, &stringNames, &stringValues );
	saver.Add( 19, &numberNames );
	saver.Add( 20, &numberValues );
	saver.Add( 21, &stringNames );
	saver.Add( 22, &stringValues );
	if ( saver.IsReading() && script.GetState() != 0 )
	{
		lua_State *pState = script.GetState();
		for ( int i = 0; i < numberNames.size() && i < numberValues.size(); ++i )
		{
			lua_pushnumber( pState, numberValues[i] );
			lua_setglobal( pState, numberNames[i].c_str() );
		}
		for ( int i = 0; i < stringNames.size() && i < stringValues.size(); ++i )
		{
			lua_pushlstring( pState, stringValues[i].data(), stringValues[i].size() );
			lua_setglobal( pState, stringNames[i].c_str() );
		}
	}

	saver.Add( 10, &areas );

	saver.Add( 11, &groupUnits );
	saver.Add( 12, &reinforcs );

	saver.Add( 13, &suspendedReinforcs );
	saver.Add( 14, &lastTimeToCheckSuspendedReinforcs );
	
	// LandSuspendedReiforcements tries one suspended reinforcement a segment
	// from reinforcsIter on: where it stands, since the port (a save without it
	// starts at the first, as it always did).
	int nReinforcsPos = 0;
	if ( !saver.IsReading() )
	{
		for ( CReinfList::iterator it = suspendedReinforcs.begin(); it != reinforcsIter && it != suspendedReinforcs.end(); ++it )
			++nReinforcsPos;
	}
	saver.Add( 17, &nReinforcsPos );
	if ( saver.IsReading() )
	{
		reinforcsIter = suspendedReinforcs.begin();
		for ( int i = 0; i < nReinforcsPos && reinforcsIter != suspendedReinforcs.end(); ++i )
			++reinforcsIter;
	}

	saver.Add( 15, &reservePositions );
	saver.Add( 16, &bShowErrors );

	return 0;
}
