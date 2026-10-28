// The Remastered switch (Sources/src/Main/Remastered.h): when the player's
// option holds, and that multiplayer and replays override it. The header
// reads global vars through GetGlobalVar, which a module's StdAfx.h brings
// in; this test gives it a table of its own.
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

static std::map<std::string, std::string> vars;

static int GetGlobalVar( const char *pszName, int nDefault )
{
	std::map<std::string, std::string>::const_iterator it = vars.find( pszName );
	return it == vars.end() ? nDefault : std::atoi( it->second.c_str() );
}

#include "../../Sources/src/Main/Remastered.h"

static int failures = 0;

#define CHECK( condition ) \
	do { \
		if ( !( condition ) ) { \
			std::fprintf( stderr, "remastered check failed (line %d): %s\n", __LINE__, #condition ); \
			++failures; \
		} \
	} while ( false )

int main()
{
	// The policy: multiplayer is always the original, a replay keeps what it
	// was recorded with, otherwise the option decides.
	CHECK( !NRemastered::Resolve( false, false, false, false ) );
	CHECK( NRemastered::Resolve( true, false, false, false ) );
	CHECK( !NRemastered::Resolve( true, true, false, false ) );
	CHECK( !NRemastered::Resolve( true, true, true, true ) );
	CHECK( !NRemastered::Resolve( true, false, true, false ) );
	CHECK( NRemastered::Resolve( false, false, true, true ) );

	// The same through the global vars the game sets: nothing set is the
	// original game, which is the option's default.
	CHECK( !NRemastered::Enabled() );
	vars[NRemastered::pszOptionVar] = "1";
	CHECK( NRemastered::Enabled() );
	vars["MultiplayerGame"] = "1";
	CHECK( !NRemastered::Enabled() );
	vars.erase( "MultiplayerGame" );
	vars["History.Playing"] = "1";
	CHECK( !NRemastered::Enabled() );
	vars[NRemastered::pszReplayVar] = "1";
	vars[NRemastered::pszOptionVar] = "0";
	CHECK( NRemastered::Enabled() );
	vars.erase( "History.Playing" );
	CHECK( !NRemastered::Enabled() );

	// The simulation's rules (the fastest path): the option or a replay's
	// mode as above, but in multiplayer the host's choice, which a
	// multiplayer replay brings back with its Multiplayer vars.
	vars.clear();
	CHECK( !NRemastered::Simulation() );
	vars[NRemastered::pszOptionVar] = "1";
	CHECK( NRemastered::Simulation() );
	vars["MultiplayerGame"] = "1";
	CHECK( !NRemastered::Simulation() );
	vars[NRemastered::pszMultiplayerVar] = "1";
	vars[NRemastered::pszOptionVar] = "0";
	CHECK( NRemastered::Simulation() );
	vars["History.Playing"] = "1";
	CHECK( NRemastered::Simulation() );
	vars.erase( "MultiplayerGame" );
	CHECK( !NRemastered::Simulation() );
	vars[NRemastered::pszReplayVar] = "1";
	CHECK( NRemastered::Simulation() );
	// A replay from the command line plays as a live game but keeps its mode.
	vars.erase( "History.Playing" );
	vars[NRemastered::pszOptionVar] = "1";
	vars[NRemastered::pszReplayVar] = "0";
	vars[NRemastered::pszCommandLineReplayVar] = "1";
	CHECK( !NRemastered::Simulation() );
	CHECK( !NRemastered::ResolveSimulation( true, true, false, false, false ) );
	CHECK( NRemastered::ResolveSimulation( false, true, true, true, false ) );

	return failures == 0 ? 0 : 1;
}
