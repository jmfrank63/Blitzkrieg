#include "GameMain.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
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

// Accepts a lowercased, already-trimmed -mode value: "auto", or "WxH" /
// "WxHxBPP" with positive width/height (BPP defaults to 32, matching the
// GFX.Mode option's own format). Returns false - leaving normalized
// untouched - for anything else, including an empty value.
//
// %n (and a two-pass WxHxBPP-then-WxH attempt) requires the whole value to
// be consumed: plain "%dx%dx%d" alone would happily accept "800x600zzzz"
// (sscanf just stops at the first unmatched character once >=2 fields are
// filled), silently dropping the trailing garbage instead of rejecting it.
// This tightening is deliberately local to this CLI-validation path; the
// looser "%dx%dx%d" convention used elsewhere for reading the GFX.Mode
// config option is untouched.
bool NormalizeModeValue(const std::string &lowered, std::string &normalized)
{
	if ( lowered == "auto" )
	{
		normalized = "Auto";
		return true;
	}
	int width = 0, height = 0, bpp = 0, consumed = 0;
	if ( std::sscanf( lowered.c_str(), "%dx%dx%d%n", &width, &height, &bpp, &consumed ) == 3
		&& consumed == (int)lowered.size() && width > 0 && height > 0 )
	{
		char buffer[32];
		std::snprintf( buffer, sizeof buffer, "%dx%dx%d", width, height, bpp );
		normalized = buffer;
		return true;
	}
	width = 0; height = 0; consumed = 0;
	if ( std::sscanf( lowered.c_str(), "%dx%d%n", &width, &height, &consumed ) == 2
		&& consumed == (int)lowered.size() && width > 0 && height > 0 )
	{
		char buffer[32];
		std::snprintf( buffer, sizeof buffer, "%dx%dx32", width, height );
		normalized = buffer;
		return true;
	}
	return false;
}

// Shared by -help and by a parse error, so both point at the same list of
// recognized flags. Deliberately leaves out internal/testing-only flags
// (-reference-scene, -startup-smoke, -lh*, ...).
const char *const kUsageText =
"Usage: Game [options] [map.xml | save.sav]\n"
"\n"
"Options:\n"
"  -windowed                     run in a window\n"
"  -fullscreen                   run fullscreen\n"
"  -mode=WxH[xBPP] | -mode=auto  set the resolution, e.g. -mode=1024x768x32\n"
"  -monitorN                     pick a display by 1-based ordinal, e.g. -monitor2\n"
"  -monitor=\"name\"               pick a display by name\n"
"  -profile=Name                 play with this player profile\n"
"  -mp                           multiplayer\n"
"  -mod<dir>                     load a mod from <dir>\n"
"\n"
"Example: Game -windowed -mode=1024x768x32\n";
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
		else if ( argument == "-mode" || argument.rfind( "-mode=", 0 ) == 0 )
		{
			// Exact "-mode" or "-mode=..." only - NOT a bare prefix match.
			// "-mode" is also a prefix of "-mod<dir>" (e.g. -modExpansion
			// lowercases to "-modexpansion"), so a loose rfind("-mode",0)==0
			// here would swallow that mod argument, fail to parse "xpansion"
			// as a resolution, and hard-exit a perfectly valid -mod. Rejected
			// here rather than left for GameMain's later re-parse: this is
			// the one path RunGame always takes before any engine/window
			// init (see CommandLineExitCode below), so it is the only
			// reliable place to bail out clean.
			std::string value = AttachedValue( raw, 5 );
			if ( !value.empty() && value.front() == '=' ) value = TrimQuotes( value.substr( 1 ) );
			std::string normalized;
			if ( NormalizeModeValue( Lower( value ), normalized ) )
				result.mode = normalized;
			else
			{
				result.parseError = true;
				result.modeInvalid = true;
				result.modeError = value;
			}
		}
		else if ( argument.rfind( "-mod", 0 ) == 0 ) result.modName = AttachedValue( raw, 4 );
		else if ( argument == "-windowed" ) result.fullscreenMode = EFullscreenMode::windowed;
		else if ( argument == "-fullscreen" ) result.fullscreenMode = EFullscreenMode::fullscreen;
		else if ( argument.rfind( "-monitor", 0 ) == 0 )
		{
			std::string value = AttachedValue( raw, 8 );
			if ( !value.empty() && value.front() == '=' ) value = TrimQuotes( value.substr( 1 ) );
			result.monitor = value;
		}
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

void ReportCommandLine( const CommandLineOptions &options )
{
	if ( options.showHelp )
	{
		std::fputs( kUsageText, stdout );
		return;
	}
	if ( !options.parseError ) return;
	if ( options.modeInvalid && options.modeError.empty() )
		std::fprintf( stderr, "Error: -mode requires a value (expected WxH, WxHxBPP, or auto)\n" );
	else if ( options.modeInvalid )
		std::fprintf( stderr, "Error: invalid -mode value \"%s\" (expected WxH, WxHxBPP, or auto)\n", options.modeError.c_str() );
	else if ( !options.unknownArguments.empty() )
		std::fprintf( stderr, "Error: unrecognized argument \"%s\"\n", options.unknownArguments.front().c_str() );
	else
		std::fprintf( stderr, "Error: invalid command line\n" );
	std::fputs( kUsageText, stderr );
}
}
