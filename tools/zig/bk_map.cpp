// bk-map: bzmconvertor's map conversions and imagedefrag's minimap creator on
// every platform (Phase 8, D-16..D-19), and the Blender pipeline's obj2mod
// (bk_map_obj2mod.cpp). Maps are read and written by NMapFile, the Map
// Editor's own reader and writer, and a minimap is drawn by the engine's
// CMapInfo::CreateMiniMapImage, the call the editors and the game make; no
// format is written a second way here (D-03, D-17, D-18).
//
// The command-line rules are bk_tool.zig's (D-04), kept by hand as in
// bk_font.cpp: --help anywhere prints the usage and exits 0, bad usage prints
// it on stderr and exits 2, a failure is one "bk-map: reason" line on stderr
// and exit 1, success one line on stdout.
#include "StdAfx.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "bk_l10n.h"
#include "data_only_startup.h"
#include "tool_host.h"
#include "bk_map_obj2mod.h"
#include "../../Sources/src/MapFile/MapFile.h"
#include "../../Sources/src/Main/GameDB.h"
#include "../../Sources/src/RandomMapGen/MapInfo_Types.h"
#include "../../Sources/src/RandomMapGen/MiniMap_Types.h"

namespace fs = std::filesystem;

// The synopsis stays English; the paragraphs under it and the --lang line come from the tables.
static std::string UsageText()
{
	return std::string( "usage: bk-map to-bzm <in.xml|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]\n"
		"       bk-map to-xml <in.bzm|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]\n"
		"       bk-map minimap <map|dir> --size <px> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]\n"
		"       bk-map obj2mod <in.obj> <out.mod> [skeleton.txt] [anim.txt] [--modules <dir>]\n"
		"       bk-map validate-obj2mod <in.obj> [skeleton.txt] [anim.txt]\n" )
		+ "\n" + BK_T( "map.usage.commands" ) + "\n"
		+ "\n" + BK_T( "map.usage.directory" ) + "\n"
		+ "\n" + BK_T( "map.usage.options" ) + "\n"
		+ "\n" + BkFormat( "tool.usage.lang", { bk_l10n_valid_codes() } ) + "\n";
}

static int Usage()
{
	fputs( UsageText().c_str(), stderr );
	return 2;
}

static int Fail( const std::string &szReason )
{
	fprintf( stderr, "bk-map: %s\n", szReason.c_str() );
	return 1;
}

// A path as the tool prints it: the host's separators.
static std::string Shown( fs::path path )
{
	return path.make_preferred().string();
}

static bool HasExtension( const fs::path &path, const char *pszExtension )
{
	return NStr::CompareAsciiNoCase( path.extension().string().c_str(), pszExtension ) == 0;
}

// One map to convert or draw: where it is read from and the output path
// without an extension (minimap) or with the target one (to-bzm, to-xml).
struct SJob
{
	fs::path input;
	fs::path output;
};

// The maps of an input file or directory, in sorted order, each with its
// output: beside it, or under --out at the same place relative to the input
// directory (the legacy tool's mirror). For a minimap the output keeps no
// extension and a map with both an .xml and a .bzm is drawn once, from the
// .xml only when it is strictly newer (GameTT/MinimapCreation.cpp's rule).
static bool CollectJobs( const fs::path &input, const fs::path &out, const char *pszFrom, const char *pszTo,
	std::vector<SJob> *pJobs, std::string *pError )
{
	const bool bMinimap = pszTo == 0;
	std::error_code ec;
	std::vector<fs::path> files;
	fs::path root;
	if ( fs::is_directory( input, ec ) )
	{
		root = input;
		for ( fs::recursive_directory_iterator it( input, ec ), end; !ec && it != end; it.increment( ec ) )
		{
			if ( !it->is_regular_file( ec ) )
				continue;
			if ( bMinimap ? ( HasExtension( it->path(), ".bzm" ) || HasExtension( it->path(), ".xml" ) ) : HasExtension( it->path(), pszFrom ) )
				files.push_back( it->path() );
		}
		if ( ec )
		{
			*pError = BkFormat( "map.err.cannot_list", { Shown( input ), ec.message() } );
			return false;
		}
		if ( files.empty() )
		{
			*pError = BkFormat( "map.err.no_maps", { bMinimap ? ".bzm or .xml" : pszFrom, Shown( input ) } );
			return false;
		}
	}
	else if ( fs::is_regular_file( input, ec ) )
	{
		if ( bMinimap ? !( HasExtension( input, ".bzm" ) || HasExtension( input, ".xml" ) ) : !HasExtension( input, pszFrom ) )
		{
			*pError = BkFormat( "map.err.not_map", { Shown( input ), bMinimap ? ".bzm or .xml" : pszFrom } );
			return false;
		}
		root = input.parent_path();
		files.push_back( input );
	}
	else
	{
		*pError = BkFormat( "map.err.no_such", { Shown( input ) } );
		return false;
	}
	std::sort( files.begin(), files.end() );
	for ( size_t i = 0; i < files.size(); ++i )
	{
		fs::path base = files[i];
		base.replace_extension();
		if ( bMinimap && i + 1 < files.size() )
		{
			fs::path next = files[i + 1];
			if ( next.replace_extension() == base )
			{
				// The pair sorts .bzm before .xml.
				const bool bXmlNewer = fs::last_write_time( files[i + 1], ec ) > fs::last_write_time( files[i], ec );
				if ( !bXmlNewer )
					files.erase( files.begin() + i + 1 );
				else
					files.erase( files.begin() + i );
			}
		}
		SJob job;
		job.input = files[i];
		const fs::path relative = files[i].lexically_relative( root );
		job.output = ( out.empty() ? root : out ) / relative;
		job.output.replace_extension( bMinimap ? "" : pszTo );
		pJobs->push_back( job );
	}
	return true;
}

static const char *const MINIMAP_SUFFIXES[3] = { "_c.dds", "_l.dds", "_h.dds" };

// Every output a job list would write, so an existing one stops the run
// before anything is written.
static bool FindExisting( const std::vector<SJob> &jobs, bool bMinimap, fs::path *pExisting )
{
	std::error_code ec;
	for ( size_t i = 0; i < jobs.size(); ++i )
		for ( int n = 0; n < ( bMinimap ? 3 : 1 ); ++n )
		{
			const fs::path file = bMinimap ? fs::path( jobs[i].output.string() + MINIMAP_SUFFIXES[n] ) : jobs[i].output;
			if ( fs::exists( file, ec ) )
			{
				*pExisting = file;
				return true;
			}
		}
	return false;
}

// NMapFile's reason without the engine path it starts with, which is in
// StreamIO's backslash form; the caller names the file the host's way.
static std::string Reason( const std::string &szError )
{
	const size_t nColon = szError.rfind( ": " );
	return nColon == std::string::npos ? szError : szError.substr( nColon + 2 );
}

static bool CreateParent( const fs::path &file, std::string *pError )
{
	std::error_code ec;
	fs::create_directories( file.parent_path().empty() ? fs::path( "." ) : file.parent_path(), ec );
	if ( ec )
		*pError = BkFormat( "cxx.err.create", { Shown( file.parent_path() ), ec.message() } );
	return !ec;
}

// to-bzm and to-xml: NMapFile's read and write, which is what the Map Editor
// opens and saves with. Empty on success, the reason otherwise.
static std::string ConvertMaps( const std::vector<SJob> &jobs )
{
	for ( size_t i = 0; i < jobs.size(); ++i )
	{
		std::string szError;
		if ( !CreateParent( jobs[i].output, &szError ) )
			return szError;
		CMapInfo map;
		if ( !NMapFile::Read( NToolHost::EnginePath( fs::absolute( jobs[i].input ) ).c_str(), &map, &szError ) )
			return BkFormat( "tool.err.path_reason", { Shown( jobs[i].input ), Reason( szError ) } );
		if ( !NMapFile::Write( NToolHost::EnginePath( fs::absolute( jobs[i].output ) ).c_str(), map, &szError ) )
			return BkFormat( "tool.err.path_reason", { Shown( jobs[i].output ), Reason( szError ) } );
	}
	return std::string();
}

// minimap: imagedefrag's loop, with the editor bridge's preparation of the
// map (UnpackFrameIndices, as BkEditorCreateMiniMapImage and the game do).
// The engine writes the three DDS levels itself, by full path.
static std::string DrawMinimaps( const std::vector<SJob> &jobs, int nSize, const fs::path &data )
{
	CPtr<IObjectsDB> pGDB = CreateObjectsDB();
	RegisterSingleton( IObjectsDB::tidTypeID, pGDB );
	GetSLS()->SetGDB( pGDB );
	if ( !pGDB->LoadDB() )
		return BkFormat( "map.err.db_load", { Shown( data ) } );
	for ( size_t i = 0; i < jobs.size(); ++i )
	{
		std::string szError;
		if ( !CreateParent( jobs[i].output, &szError ) )
			return szError;
		CMapInfo map;
		if ( !NMapFile::Read( NToolHost::EnginePath( fs::absolute( jobs[i].input ) ).c_str(), &map, &szError ) )
			return BkFormat( "tool.err.path_reason", { Shown( jobs[i].input ), Reason( szError ) } );
		map.UnpackFrameIndices();
		CRMImageCreateParameterList parameters;
		parameters.push_back( SRMImageCreateParameter( NToolHost::EnginePath( fs::absolute( jobs[i].output ) ), CTPoint<int>( nSize, nSize ), true ) );
		if ( !map.CreateMiniMapImage( parameters ) )
			return BkFormat( "map.err.minimap_failed", { Shown( jobs[i].input ), Shown( data ) } );
		std::error_code ec;
		for ( int n = 0; n < 3; ++n )
			if ( !fs::exists( jobs[i].output.string() + MINIMAP_SUFFIXES[n], ec ) )
				return BkFormat( "map.err.not_written", { Shown( jobs[i].output.string() + MINIMAP_SUFFIXES[n] ) } );
	}
	return std::string();
}

static bool ParseSize( const char *pszText, int *pValue )
{
	char *pszEnd = 0;
	const long nValue = strtol( pszText, &pszEnd, 10 );
	if ( pszEnd == pszText || *pszEnd != 0 || nValue < 1 || nValue > 4096 )
		return false;
	*pValue = int( nValue );
	return true;
}

// The engine calls of one command, guarded: a stream or saver that throws is
// one line of failure, not a crash.
template <class TFunction>
static std::string Guard( TFunction function )
{
	try
	{
		return function();
	}
	catch ( ... )
	{
		return BK_T( "cxx.err.threw" );
	}
}

int main( int argc, char *argv[] )
{
	NToolHost::BinaryStdio();
	const int nLanguage = NToolHost::StartLanguage( &argc, &argv, "bk-map", UsageText );
	if ( nLanguage >= 0 )
		return nLanguage;
	for ( int i = 1; i < argc; ++i )
		if ( strcmp( argv[i], "--help" ) == 0 || strcmp( argv[i], "-h" ) == 0 )
		{
			fputs( UsageText().c_str(), stdout );
			return 0;
		}
	if ( argc < 2 )
		return Usage();
	const std::string szCommand = argv[1];
	const bool bToBzm = szCommand == "to-bzm", bToXml = szCommand == "to-xml", bMinimap = szCommand == "minimap";
	const bool bObjToMod = szCommand == "obj2mod", bValidate = szCommand == "validate-obj2mod";
	if ( !bToBzm && !bToXml && !bMinimap && !bObjToMod && !bValidate )
		return Usage();

	std::vector<std::string> positional;
	std::string szOut, szData = "Data", szModules;
	bool bForce = false;
	int nSize = 0;
	for ( int i = 2; i < argc; ++i )
	{
		const std::string szArg = argv[i];
		const bool bHasValue = i + 1 < argc;
		const bool bMapCommand = bToBzm || bToXml || bMinimap;
		if ( szArg == "--force" && bMapCommand ) bForce = true;
		else if ( szArg.compare( 0, 2, "--" ) != 0 ) positional.push_back( szArg );
		else if ( !bHasValue ) return Usage();
		else if ( szArg == "--out" && bMapCommand ) szOut = argv[++i];
		else if ( szArg == "--modules" && !bValidate ) szModules = argv[++i];
		else if ( szArg == "--data" && bMapCommand ) szData = argv[++i];
		else if ( szArg == "--size" && bMinimap ) { if ( !ParseSize( argv[++i], &nSize ) ) return Usage(); }
		else return Usage();
	}
	const fs::path moduleRoot = NToolHost::ModuleRoot( szModules );

	if ( bObjToMod || bValidate )
	{
		const size_t nFirstSidecar = bObjToMod ? 2 : 1;
		if ( positional.size() < nFirstSidecar || positional.size() > nFirstSidecar + 2 )
			return Usage();
		std::vector<std::string> paths;
		for ( size_t i = 0; i < positional.size(); ++i )
			paths.push_back( Shown( NToolHost::InputPath( positional[i] ) ) );
		paths.resize( nFirstSidecar + 2 );
		// The legacy tool needed the .mod's folder to be there; this makes it.
		std::string szError;
		if ( bObjToMod && !CreateParent( fs::path( paths[1] ), &szError ) )
			return Fail( szError );
		// The startup reports its own reason on stderr before this one.
		if ( bObjToMod && !NDataOnly::StartStreamIO( moduleRoot.string().c_str() ) )
			return Fail( BkFormat( "cxx.err.libs", { Shown( moduleRoot ) } ) );
		const bool bDone = NObjToMod::Run( paths[0], bObjToMod ? paths[1] : std::string(), paths[nFirstSidecar], paths[nFirstSidecar + 1], bValidate );
		if ( bObjToMod )
			NDataOnly::Stop();
		return bDone ? 0 : 1;
	}

	if ( positional.size() != 1 || ( bMinimap && nSize == 0 ) )
		return Usage();
	const fs::path input = NToolHost::InputPath( positional[0] );
	const fs::path out = szOut.empty() ? fs::path() : NToolHost::InputPath( szOut );
	std::vector<SJob> jobs;
	std::string szError;
	if ( !CollectJobs( input, out, bToBzm ? ".xml" : ".bzm", bMinimap ? 0 : ( bToBzm ? ".bzm" : ".xml" ), &jobs, &szError ) )
		return Fail( szError );
	fs::path existing;
	if ( !bForce && FindExisting( jobs, bMinimap, &existing ) )
		return Fail( BkFormat( "cxx.err.exists", { Shown( existing ) } ) );

	const fs::path data = NToolHost::InputPath( szData );
	std::error_code ec;
	if ( bMinimap && !fs::is_regular_file( data / "objects.xml", ec ) )
		return Fail( BkFormat( "map.err.no_objects", { Shown( data ) } ) );
	if ( !fs::is_directory( data, ec ) )
		return Fail( BkFormat( "map.err.no_data", { Shown( data ) } ) );
	if ( !NDataOnly::Start( moduleRoot.string().c_str(), NToolHost::EnginePath( fs::absolute( data ) ).c_str() ) ||
	     ( bMinimap && !NDataOnly::StartImage( moduleRoot.string().c_str() ) ) )
		return Fail( BkFormat( "cxx.err.libs", { Shown( moduleRoot ) } ) );
	const std::string szResult = Guard( [&]() { return bMinimap ? DrawMinimaps( jobs, nSize, data ) : ConvertMaps( jobs ); } );
	NDataOnly::Stop();
	if ( !szResult.empty() )
		return Fail( szResult );

	const bool bSingle = jobs.size() == 1 && !fs::is_directory( input, ec );
	if ( bMinimap && bSingle )
		printf( "wrote %s_c.dds, _l.dds and _h.dds, %dx%d\n", Shown( jobs[0].output ).c_str(), nSize, nSize );
	else if ( bMinimap )
		printf( "wrote %d minimaps (_c.dds, _l.dds and _h.dds), %dx%d, under %s\n", int( jobs.size() ), nSize, nSize, Shown( out.empty() ? input : out ).c_str() );
	else if ( bSingle )
		printf( "wrote %s\n", Shown( jobs[0].output ).c_str() );
	else
		printf( "wrote %d %s maps under %s\n", int( jobs.size() ), bToBzm ? ".bzm" : ".xml", Shown( out.empty() ? input : out ).c_str() );
	return 0;
}
