#ifndef __REMASTERED_H__
#define __REMASTERED_H__
#pragma ONCE
// The Remastered gameplay option (Options > Settings > Gameplay, off by
// default): off is the 2003 game's behaviour, on adds what the original did
// not have. Every feature behind it asks NRemastered::Enabled() and nothing
// else, so the rules for when it holds live here once
// (docs/superpowers/specs/2026-10-25-remastered-option-design.md).
//
// The header reads global vars through GetGlobalVar, which each module's
// StdAfx.h brings in (StreamIO/Globals.h); a test may provide its own.
namespace NRemastered
{
	// Set by the option's SetRemastered action (StreamIOZig/legacy_bridge.cpp)
	// to 1 or 0 whenever the option changes and once at startup.
	const char * const pszOptionVar = "Options.GamePlay.Remastered";
	// What a replay recorded (CCommandsHistory): a replay plays back with the
	// mode it was recorded with, not the viewer's.
	const char * const pszReplayVar = "History.Remastered";

	// Multiplayer is always the original game: every player's AI runs the same
	// commands in lockstep, so a rule that changes what an order does must be
	// the same on every machine, and the original rules are the ones every
	// client has. A replay keeps the mode it was recorded with; otherwise the
	// player's option decides.
	inline bool Resolve( const bool bOption, const bool bMultiplayer, const bool bReplay, const bool bReplayRemastered )
	{
		if ( bMultiplayer )
			return false;
		if ( bReplay )
			return bReplayRemastered;
		return bOption;
	}
	inline bool Enabled()
	{
		return Resolve( GetGlobalVar( pszOptionVar, 0 ) != 0,
		                GetGlobalVar( "MultiplayerGame", 0 ) != 0,
		                GetGlobalVar( "History.Playing", 0 ) != 0,
		                GetGlobalVar( pszReplayVar, 0 ) != 0 );
	}

	// The host's option in a multiplayer game: the game settings carry it to
	// every player (SGameInfo), and a multiplayer replay brings it back with
	// the other Multiplayer.* vars; a game or replay without it has none.
	const char * const pszMultiplayerVar = "Multiplayer.Remastered";
	// Remastered rules the simulation itself plays by, decided once as a
	// mission starts and kept in its saves (the fastest path,
	// docs/superpowers/specs/2026-10-28-fastest-path-design.md). Unlike
	// Enabled() they hold in multiplayer too: every machine computes the
	// same paths from the same orders, so all a lockstep game needs is the
	// one choice, the host's.
	inline bool ResolveSimulation( const bool bOption, const bool bMultiplayer, const bool bHost, const bool bReplay, const bool bReplayRemastered )
	{
		if ( bMultiplayer )
			return bHost;
		return Resolve( bOption, false, bReplay, bReplayRemastered );
	}
	inline bool Simulation()
	{
		return ResolveSimulation( GetGlobalVar( pszOptionVar, 0 ) != 0,
		                          GetGlobalVar( "MultiplayerGame", 0 ) != 0,
		                          GetGlobalVar( pszMultiplayerVar, 0 ) != 0,
		                          GetGlobalVar( "History.Playing", 0 ) != 0,
		                          GetGlobalVar( pszReplayVar, 0 ) != 0 );
	}
};
#endif // __REMASTERED_H__
