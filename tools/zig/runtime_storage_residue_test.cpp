#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sys/stat.h>
#include <string>

namespace
{
	int CompareAsciiNoCase( const char *left, const char *right )
	{
		while ( *left != 0 && *right != 0 )
		{
			const int foldedLeft = std::tolower( static_cast<unsigned char>( *left ) );
			const int foldedRight = std::tolower( static_cast<unsigned char>( *right ) );
			if ( foldedLeft != foldedRight ) return foldedLeft < foldedRight ? -1 : 1;
			++left;
			++right;
		}
		return *left == *right ? 0 : ( *left == 0 ? -1 : 1 );
	}

	std::string LegacyPath( std::string path )
	{
		for ( char &character : path ) if ( character == '/' ) character = '\\';
		return path;
	}

	bool Contains( const std::string &text, const char *needle )
	{
		return text.find( needle ) != std::string::npos;
	}

	std::uint32_t Checksum( const std::string &bytes )
	{
		std::uint32_t result = 2166136261U;
		for ( const unsigned char byte : bytes ) result = ( result ^ byte ) * 16777619U;
		return result;
	}
}

int main()
{
	const char *const sourceFiles[] = {
		"Sources/src/GameTT/Common.cpp",
		"Sources/src/GameTT/MainMenu.cpp",
		"Sources/src/GameTT/OptionEntryWrapper.cpp",
		"Sources/src/Main/GameDB.cpp",
		"Sources/src/Main/iMainInternal.cpp",
		"Sources/src/RandomMapGen/Resource_Functions.cpp",
		"Sources/src/RandomMapGen/Resource_Types.h",
		"Sources/src/Misc/StrProc.cpp",
		"Sources/src/AILogic/AIUnit.cpp",
		"Sources/src/AILogic/DamageToEnemyUpdater.cpp",
	};
	const char *const forbidden[] = {
		"_access",
		"_itoa",
		"_finite",
		"_stricmp",
		"_strnicmp",
		"MAX_PATH",
		"GetModuleFileName(",
		"GetCurrentDirectory",
		"CreateFile(",
		"GetFileInformationByHandle",
	};
	for ( const char *path : sourceFiles )
	{
		std::ifstream input( path );
		if ( !input ) return 1;
		const std::string text( ( std::istreambuf_iterator<char>( input )), std::istreambuf_iterator<char>() );
		for ( const char *token : forbidden ) if ( Contains( text, token ) ) return 2;
	}

	if ( CompareAsciiNoCase( "GFX.Mode", "gfx.MODE" ) != 0 || CompareAsciiNoCase( "Mission", "mission" ) != 0 || CompareAsciiNoCase( "weapon", "Ack" ) == 0 ) return 3;
	if ( LegacyPath( "data/missions\\save.sav" ) != "data\\missions\\save.sav" ) return 4;

	const char *const utf8 = "runtime-storage-residue-世界.dat";
	const std::string bytes( "portable-bytes\0v1", 16 );
	const std::uint32_t checksum = Checksum( bytes );
	if ( checksum == 0U ) return 5;
	{
		std::ofstream output( utf8, std::ios::binary );
		if ( !output ) return 6;
		output.write( bytes.data(), static_cast<std::streamsize>( bytes.size() ) );
	}
	{
		std::ifstream input( utf8, std::ios::binary );
		const std::string roundTrip( ( std::istreambuf_iterator<char>( input )), std::istreambuf_iterator<char>() );
		input.seekg( 0, std::ios::end );
		if ( roundTrip != bytes || Checksum( roundTrip ) != checksum || static_cast<std::size_t>( input.tellg() ) != bytes.size() ) return 7;
	}
	struct _stat64 status;
	if ( _stat64( utf8, &status ) != 0 || status.st_mtime == 0 ) return 8;
	std::ifstream missing( "runtime-storage-residue-missing.sav" );
	if ( missing.good() ) return 9;
	std::printf( "storage residue fixtures: bytes=%zu checksum=%08x utf8=1 mixed-separators=1 ascii-case=1 timestamp=1 missing=1\n", bytes.size(), checksum );
	std::remove( utf8 );
	return 0;
}
