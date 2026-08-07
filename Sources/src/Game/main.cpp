#include "GameMain.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#ifndef BLITZKRIEG_COMMAND_LINE_TEST
#if !defined(_WIN32)
int main(int argc, char **argv)
{
	return GameMain( NPlatform::Arguments{ argc, argv } );
}
#endif
#endif

namespace
{
std::string TrimQuotes(std::string value)
{
	while ( value.size() >= 2 && value.front() == '"' && value.back() == '"' )
		value = value.substr( 1, value.size() - 2 );
	return value;
}

std::string Lower(std::string value)
{
	std::transform( value.begin(), value.end(), value.begin(), []( unsigned char c ) {
		return static_cast<char>( std::tolower( c ) );
	} );
	return value;
}

bool HasSuffix(const std::string &value, const char *suffix)
{
	const std::string lowered = Lower( value );
	const std::string wanted = suffix;
	return lowered.size() >= wanted.size() &&
		lowered.compare( lowered.size() - wanted.size(), wanted.size(), wanted ) == 0;
}

std::string AttachedValue(const std::string &argument, std::size_t prefixLength)
{
	return TrimQuotes( argument.substr( prefixLength ) );
}
}

namespace NGame
{
CommandLineOptions ParseCommandLine(const NPlatform::Arguments &arguments)
{
	CommandLineOptions result;
	for ( int index = 1; index < arguments.argc; ++index )
	{
		const std::string raw = arguments.argv[index] ? arguments.argv[index] : "";
		const std::string unquoted = TrimQuotes( raw );
		const std::string argument = Lower( unquoted );
		if ( argument.empty() ) continue;

		if ( HasSuffix( argument, ".xml" ) || HasSuffix( argument, ".bzm" ) )
		{
			if ( result.mapName.empty() ) result.mapName = unquoted;
		}
		else if ( HasSuffix( argument, ".sav" ) ) result.saveFile = unquoted;
		else if ( argument.rfind( "-freq", 0 ) == 0 ) result.frequency = std::atoi( argument.c_str() + 5 );
		else if ( argument == "-mp" ) result.multiplayer = true;
		else if ( argument == "-x64-startup-smoke" || argument == "-startup-smoke" ) result.startupSmoke = true;
		else if ( argument.rfind( "-reference-scene", 0 ) == 0 )
		{
			result.referenceScene = true;
			result.startupSmoke = true;
			result.referenceScenePath = TrimQuotes( raw.substr( 16 ) );
			if ( result.referenceScenePath.empty() && index + 1 < arguments.argc )
				result.referenceScenePath = TrimQuotes( arguments.argv[++index] ? arguments.argv[index] : "" );
		}
		else if ( argument == "-reference-resolution" && index + 2 < arguments.argc )
		{
			result.referenceWidth = std::atoi( arguments.argv[++index] );
			result.referenceHeight = std::atoi( arguments.argv[++index] );
		}
		else if ( argument.rfind( "-mod", 0 ) == 0 ) result.modName = AttachedValue( raw, 4 );
		else if ( argument == "-windowed" ) result.fullscreenMode = EFullscreenMode::windowed;
		else if ( argument == "-fullscreen" ) result.fullscreenMode = EFullscreenMode::fullscreen;
		else if ( argument.rfind( "-autosave", 0 ) == 0 ) result.autoSavePeriod = std::atoi( argument.c_str() + 9 );
		else if ( argument == "-cycled" ) result.cycledLaunch = true;
		else if ( argument.rfind( "-fps", 0 ) == 0 ) result.guaranteeFps = std::atoi( argument.c_str() + 4 );
		else if ( argument.rfind( "-movie", 0 ) == 0 ) result.movieDirectory = AttachedValue( raw, 6 );
		else if ( argument.rfind( "-showscripterr", 0 ) == 0 ) result.showScriptErrors = true;
		else if ( argument.rfind( "-sh", 0 ) == 0 ) result.saveHistoryFile = AttachedValue( raw, 3 );
		else if ( argument.rfind( "-lhclient", 0 ) == 0 ) result.loadHistoryFile = AttachedValue( raw, 9 );
		else if ( argument.rfind( "-lh", 0 ) == 0 ) result.loadHistoryFile = AttachedValue( raw, 3 );
		else if ( argument.rfind( "-datadir", 0 ) == 0 ) result.dataDirectory = AttachedValue( raw, 8 );
		else if ( argument.rfind( "-connect", 0 ) == 0 ) result.connectAddress = AttachedValue( raw, 8 );
		else if ( argument.rfind( "-host", 0 ) == 0 )
		{
			const std::string value = AttachedValue( raw, 5 );
			result.hostPort = value.empty() ? -1 : std::atoi( value.c_str() );
		}
		else if ( argument.rfind( "-password", 0 ) == 0 )
		{
			result.passwordRequired = true;
			result.password = AttachedValue( raw, 9 );
		}
		else if ( argument.rfind( "-name", 0 ) == 0 ) result.playerName = AttachedValue( raw, 5 );
		else if ( argument.rfind( "-room", 0 ) == 0 ) result.roomName = AttachedValue( raw, 5 );
		else if ( argument.rfind( "-cheats", 0 ) == 0 ) result.cheats = true;
		else if ( argument.rfind( "-numsaves", 0 ) == 0 ) result.oneSave = true;
		else if ( argument == "-help" || argument == "--help" || argument == "/?" ) result.showHelp = true;
		else if ( argument.rfind( "-renderer=", 0 ) == 0 )
		{
			result.renderer = argument.substr( 10 );
			if ( result.renderer != "sdl_gpu" && result.renderer != "legacy" ) result.parseError = true;
		}
		else if ( argument.rfind( "-renderer", 0 ) == 0 )
		{
			result.renderer = argument.substr( 9 );
			if ( result.renderer != "sdl_gpu" && result.renderer != "legacy" ) result.parseError = true;
		}
		else if ( !argument.empty() && argument.front() == '-' )
		{
			result.unknownArguments.push_back( raw );
			result.parseError = true;
		}
	}
	return result;
}

int CommandLineExitCode( const CommandLineOptions &options )
{
	if ( options.showHelp ) return 0;
	if ( options.parseError ) return 2;
	return -1;
}
}
