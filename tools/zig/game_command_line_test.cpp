#include "../../Sources/src/Game/GameMain.h"

#include <cstdio>

#define CHECK(condition) \
	do { \
		if ( !(condition) ) { \
			std::fprintf( stderr, "game command-line check failed: %s\\n", #condition ); \
			return 1; \
		} \
	} while ( false )

static NPlatform::Arguments Args(const char *const *argv, int argc)
{
	return NPlatform::Arguments{ argc, argv };
}

int main()
{
	{
		const char *argv[] = { "Game", "-fullscreen", "-freq144", "-mp", "-cycled", "-fps60", "-autosave30", "-x64-startup-smoke" };
		const NGame::CommandLineOptions options = NGame::ParseCommandLine( Args( argv, 8 ) );
		CHECK( options.fullscreenMode == NGame::EFullscreenMode::fullscreen );
		CHECK( options.frequency == 144 && options.multiplayer && options.cycledLaunch );
		CHECK( options.guaranteeFps == 60 && options.autoSavePeriod == 30 && options.startupSmoke );
	}
	{
		const char *argv[] = { "Game", "\"maps/My Map.xml\"", "-reference-scene", "capture.rgba", "-reference-resolution", "1280", "720", "-mod\"My Mod\"", "-movie\"Movies\"", "-password\"secret value\"" };
		const NGame::CommandLineOptions options = NGame::ParseCommandLine( Args( argv, 10 ) );
		CHECK( options.mapName == "maps/My Map.xml" );
		CHECK( options.referenceScene && options.referenceScenePath == "capture.rgba" );
		CHECK( options.referenceWidth == 1280 && options.referenceHeight == 720 );
		CHECK( options.modName == "My Mod" && options.movieDirectory == "Movies" );
		CHECK( options.passwordRequired && options.password == "secret value" );
	}
	{
		const char *argv[] = { "Game", "-connect127.0.0.1", "-host", "-namePlayer", "-roomLobby", "-datadirC:\\Data", "-unknown" };
		const NGame::CommandLineOptions options = NGame::ParseCommandLine( Args( argv, 7 ) );
		CHECK( options.connectAddress == "127.0.0.1" && options.hostPort == -1 );
		CHECK( options.playerName == "Player" && options.roomName == "Lobby" );
		CHECK( options.dataDirectory == "C:\\Data" && options.unknownArguments.size() == 1 );
	}
	{
		const char *argv[] = { "Game", nullptr, "", "   ", "\"\"", "\"  \"", "\"maps/My Map.xml\"" };
		const NGame::CommandLineOptions options = NGame::ParseCommandLine( Args( argv, 7 ) );
		CHECK( options.mapName == "maps/My Map.xml" );
		CHECK( options.unknownArguments.empty() );
	}
	{
		const char *argv[] = { "Game", u8"\"maps/世界 Map.xml\"" };
		const NGame::CommandLineOptions options = NGame::ParseCommandLine( Args( argv, 2 ) );
		CHECK( options.mapName == u8"maps/世界 Map.xml" );
	}
	{
		const char *argv[] = { "Game", "-renderer=sdl_gpu", "-fullscreen", "-datadir\"C:\\Game Data\"", "-help" };
		const NGame::CommandLineOptions options = NGame::ParseCommandLine( Args( argv, 5 ) );
		CHECK( options.fullscreenMode == NGame::EFullscreenMode::fullscreen );
		CHECK( options.dataDirectory == "C:\\Game Data" );
		CHECK( options.renderer == "sdl_gpu" && options.showHelp && !options.parseError );
		CHECK( NGame::CommandLineExitCode( options ) == 0 );
	}
	{
		const char *argv[] = { "Game", "-renderer=unsupported", "-error" };
		const NGame::CommandLineOptions options = NGame::ParseCommandLine( Args( argv, 3 ) );
		CHECK( options.parseError );
		CHECK( options.unknownArguments.size() == 1 && options.unknownArguments[0] == "-error" );
		CHECK( NGame::CommandLineExitCode( options ) == 2 );
	}
	{
		const char *argv[] = { "Game" };
		const NGame::CommandLineOptions options = NGame::ParseCommandLine( Args( argv, 1 ) );
		CHECK( options.mapName.empty() && options.saveFile.empty() );
		CHECK( options.screenWidth == 1024 && options.screenHeight == 768 );
	}
	return 0;
}
