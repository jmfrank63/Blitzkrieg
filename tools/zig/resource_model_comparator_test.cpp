// test-resource-model-comparator: the D-11 comparator (Sources/src/ResourceModel/
// comparator.*) proved on the shipped game data, then the golden comparison.
//
//   1. Every shipped stats file the Resource Editor's kinds export is copied
//      into the scratch folder and compared with itself through the engine's
//      own reader: objects.xml's objects (the GameDB::GetRPGStats switch, sound
//      excepted: no sub-editor exports sounds), the weapons, medals, missions,
//      chapters, campaigns, effects, particles, tilesets, crossets, 3D roads
//      and rivers. Each must be EQUAL with every node read: a real MFC export
//      has no unknown field.
//   2. Planted changes to a copy must fail with a precise message: a float one
//      ulp off, a dropped field, an extra field the reader does not read, a
//      byte of an _h.dds and a DXT header.
//   3. The DXT gate: listed shipped _c.dds re-encoded by NDxt are within
//      dxt-tolerance.json; planted colour and alpha regions, lost DXT1
//      punch-through, a truncated file, a changed uncompressed DDS and a
//      missing or malformed tolerance fail precisely.
//   4. The S06 exporters (wpn, mcp, trc, scp): each fixture project is
//      exported through FindExporter and its stats file read back by the
//      engine: no unknown field, and field-equal (CompareStats) with the
//      struct derived by hand from the fixture's values and written by the
//      engine's own writer. Their refusals and warnings name what is missing.
//   5. The golden comparison: each fixture's golden/ folder against the port's
//      export. Goldens come from MFC editor.exe on win-home
//      (tools/zig/win-home/export-goldens.ps1). An extension without them is
//      "pending: golden missing", never PASS.
//
// The log, one line per check, is <scratch>/resource_model/comparator.log.
//
// argv: <installation> <scratch> <repo Data> <fixture root>
#include "StdAfx.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
#include "../../Sources/src/ResourceModel/comparator.h"
#include "../../Sources/src/ResourceModel/xml.h"
#include "../../Sources/src/ResourceModel/dxt_gate.h"
#include "../../Sources/src/ResourceModel/exporter.h"
#include "../../Sources/src/ResourceModel/project.h"
#include "../../Sources/src/Main/RPGStats.h"
#include "../../Sources/src/Platform/DynamicLibrary.h"
#include "../../Sources/src/Platform/Paths.h"
#include "../../Sources/src/Image/Image.h"

namespace fs = std::filesystem;
using namespace NResourceModel;

void RunUiScreenTests( const fs::path &data, const fs::path &scratchRoot, bool ( *Check )( bool, const std::string & ) );

static int g_nFailures = 0;
static std::string g_szLog;

static void Log( const std::string &szLine )
{
	std::printf( "%s\n", szLine.c_str() );
	g_szLog += szLine + "\n";
}

static bool Check( bool bCondition, const std::string &szWhat )
{
	Log( std::string( bCondition ? "PASS " : "FAIL " ) + szWhat );
	if ( !bCondition )
		++g_nFailures;
	return bCondition;
}

static bool ReadBytes( const fs::path &path, std::string *pBytes )
{
	std::ifstream file( path, std::ios::binary );
	if ( !file )
		return false;
	pBytes->assign( std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() );
	return true;
}

static bool WriteBytes( const fs::path &path, const std::string &bytes )
{
	std::error_code error;
	fs::create_directories( path.parent_path(), error );
	std::ofstream file( path, std::ios::binary | std::ios::trunc );
	file.write( bytes.data(), static_cast<std::streamsize>( bytes.size() ) );
	return file.good();
}

static bool SameNoCase( const std::string &a, const std::string &b )
{
	return a.size() == b.size() && std::equal( a.begin(), a.end(), b.begin(),
	       []( char x, char y ) { return tolower( ( unsigned char )x ) == tolower( ( unsigned char )y ); } );
}

// A storage name (backslashes, any case) as the disk under root spells it;
// empty when a component is not there.
static fs::path FindNoCase( const fs::path &root, const std::string &szName )
{
	fs::path path = root;
	std::string::size_type nStart = 0;
	while ( nStart < szName.size() )
	{
		std::string::size_type nEnd = szName.find_first_of( "\\/", nStart );
		if ( nEnd == std::string::npos )
			nEnd = szName.size();
		const std::string szPart = szName.substr( nStart, nEnd - nStart );
		nStart = nEnd + 1;
		if ( szPart.empty() )
			continue;
		std::error_code error;
		if ( fs::exists( path / szPart, error ) )
		{
			path /= szPart;
			continue;
		}
		bool bFound = false;
		for ( fs::directory_iterator it( path, error ), end; !error && it != end; it.increment( error ) )
			if ( SameNoCase( it->path().filename().string(), szPart ) )
			{
				path = it->path();
				bFound = true;
				break;
			}
		if ( !bFound )
			return fs::path();
	}
	return path;
}

struct SShipped
{
	EExportKind kind;
	fs::path file;   // relative to Data
};

// Every 1.xml (or every *.xml) under Data/<dir>, any depth.
static void AddTree( const fs::path &data, const char *pszDir, bool bOnlyOne, EExportKind kind, std::vector<SShipped> *pFiles )
{
	const fs::path root = FindNoCase( data, pszDir );
	if ( root.empty() )
	{
		Check( false, std::string( "shipped Data has " ) + pszDir );
		return;
	}
	std::vector<fs::path> found;
	std::error_code error;
	for ( fs::recursive_directory_iterator it( root, error ), end; !error && it != end; it.increment( error ) )
	{
		if ( !it->is_regular_file() )
			continue;
		const std::string szName = it->path().filename().string();
		if ( bOnlyOne ? SameNoCase( szName, "1.xml" ) : SameNoCase( it->path().extension().string(), ".xml" ) )
			found.push_back( fs::relative( it->path(), data ) );
	}
	std::sort( found.begin(), found.end() );
	for ( const fs::path &file : found )
		pFiles->push_back( { kind, file } );
}

static std::string Text( const NResourceXml::Node &item, const char *pszName )
{
	const NResourceXml::Node *pChild = NResourceXml::FindChild( item, pszName );
	if ( !pChild )
		return std::string();
	std::string szText = pChild->text;
	for ( const NResourceXml::Node &child : pChild->children )
		if ( child.kind == NResourceXml::Node::Text )
			szText += child.text;
	return szText;
}

// objects.xml through CObjectsDB::GetRPGStats's switch: which struct, and the
// object's <path>\1.xml.
static void AddObjects( const fs::path &data, std::vector<SShipped> *pFiles )
{
	std::string bytes;
	NResourceXml::Document doc;
	std::string szError;
	if ( !Check( ReadBytes( data / "objects.xml", &bytes ) && NResourceXml::Parse( bytes, doc, szError ), "objects.xml reads " + szError ) )
		return;
	const NResourceXml::Node *pObjects = NResourceXml::FindChild( doc.root, "Objects" );
	if ( !Check( pObjects != 0, "objects.xml has <Objects>" ) )
		return;
	int nMissing = 0;
	for ( const NResourceXml::Node &item : pObjects->children )
	{
		if ( item.kind != NResourceXml::Node::Element )
			continue;
		const std::string szGame = Text( item, "game_type" ), szVis = Text( item, "type" );
		EExportKind kind = EExportKind::OBJECT;
		if ( szGame == "sound" )
			continue;
		else if ( szGame == "unit" )
			kind = szVis == "sprite" ? EExportKind::INFANTRY : EExportKind::MECH_UNIT;
		else if ( szGame == "tank_pit" )
			kind = EExportKind::MECH_UNIT;
		else if ( szGame == "building" || szGame == "fortification" )
			kind = EExportKind::BUILDING;
		else if ( szGame == "fence" )
			kind = EExportKind::FENCE;
		else if ( szGame == "entrenchment" )
			kind = EExportKind::ENTRENCHMENT;
		else if ( szGame == "bridge" )
			kind = EExportKind::BRIDGE;
		else if ( szGame == "mine" )
			kind = EExportKind::MINE;
		else if ( szGame == "squad" )
			kind = EExportKind::SQUAD;
		const fs::path file = FindNoCase( data, Text( item, "path" ) + "\\1.xml" );
		if ( file.empty() )
		{
			// GetRPGStats tolerates an object without a stats file
			// (dessau.bzm's Logs08); there is nothing to compare.
			++nMissing;
			continue;
		}
		pFiles->push_back( { kind, fs::relative( file, data ) } );
	}
	Log( "objects.xml: " + std::to_string( nMissing ) + " objects have no stats file in Data" );
}

static void ShippedSelfCompare( const fs::path &data, const fs::path &scratch, std::map<EExportKind, fs::path> *pSamples )
{
	std::vector<SShipped> files;
	AddObjects( data, &files );
	// GetAddStats( WEAPON ): weapons\<name>.xml.
	AddTree( data, "Weapons", false, EExportKind::WEAPON, &files );
	AddTree( data, "Medals", true, EExportKind::MEDAL, &files );
	AddTree( data, "Scenarios\\ScenarioMissions", true, EExportKind::MISSION, &files );
	AddTree( data, "Scenarios\\TemplateMissions", true, EExportKind::MISSION, &files );
	AddTree( data, "Scenarios\\Chapters", true, EExportKind::CHAPTER, &files );
	// Campaigns are Scenarios\Campaigns\<side>\<side>.xml, not 1.xml.
	AddTree( data, "Scenarios\\Campaigns", false, EExportKind::CAMPAIGN, &files );
	AddTree( data, "Effects\\Effects", false, EExportKind::EFFECT, &files );
	AddTree( data, "Effects\\Particles", false, EExportKind::PARTICLE, &files );
	const fs::path sets = FindNoCase( data, "Terrain\\sets" );
	std::error_code error;
	for ( fs::directory_iterator it( sets, error ), end; !error && it != end; it.increment( error ) )
	{
		if ( !it->is_directory() )
			continue;
		const std::string szSet = "Terrain\\sets\\" + it->path().filename().string();
		const fs::path tileset = FindNoCase( data, szSet + "\\tileset.xml" ), crosset = FindNoCase( data, szSet + "\\crosset.xml" );
		if ( !tileset.empty() )
			files.push_back( { EExportKind::TILESET, fs::relative( tileset, data ) } );
		if ( !crosset.empty() )
			files.push_back( { EExportKind::CROSSET, fs::relative( crosset, data ) } );
		if ( !FindNoCase( data, szSet + "\\Roads3D" ).empty() )
			AddTree( data, ( szSet + "\\Roads3D" ).c_str(), false, EExportKind::VSO, &files );
		if ( !FindNoCase( data, szSet + "\\Rivers" ).empty() )
			AddTree( data, ( szSet + "\\Rivers" ).c_str(), false, EExportKind::VSO, &files );
	}

	std::map<EExportKind, int> counted, failed, fields, stale;
	int nShown = 0;
	for ( const SShipped &shipped : files )
	{
		// Read from a copy: shipped Data is read-only for every tier.
		const fs::path copy = scratch / "shipped" / shipped.file;
		std::string bytes;
		if ( !ReadBytes( data / shipped.file, &bytes ) || !WriteBytes( copy, bytes ) )
		{
			Check( false, "copy " + shipped.file.generic_string() );
			continue;
		}
		const SCompareResult result = CompareStats( shipped.kind, copy.string(), copy.string() );
		++counted[shipped.kind];
		fields[shipped.kind] += result.nFieldsCompared;
		stale[shipped.kind] += result.nStale;
		if ( result.status != ECompareStatus::EQUAL || result.nFieldsCompared == 0 )
		{
			++failed[shipped.kind];
			if ( nShown++ < 40 )
			{
				Log( std::string( "  " ) + CompareStatusName( result.status ) + " " + shipped.file.generic_string() + " fields=" + std::to_string( result.nFieldsCompared ) );
				for ( size_t i = 0; i < result.messages.size() && i < 6; ++i )
					Log( "    " + result.messages[i] );
			}
		}
		else if ( pSamples->find( shipped.kind ) == pSamples->end() )
			( *pSamples )[shipped.kind] = copy;
	}
	for ( EExportKind kind : AllExportKinds() )
	{
		const SExportKindInfo &info = GetExportKindInfo( kind );
		Check( counted[kind] > 0 && failed[kind] == 0,
		       std::string( "shipped " ) + info.pszName + " via " + info.pszReader + ": " + std::to_string( counted[kind] ) + " files equal to themselves, " +
		       std::to_string( failed[kind] ) + " not, " + std::to_string( fields[kind] ) + " fields, " + std::to_string( stale[kind] ) + " stale nodes" );
	}
}

static bool HasMessage( const SCompareResult &result, const std::string &szNeedle )
{
	for ( const std::string &szMessage : result.messages )
		if ( szMessage.find( szNeedle ) != std::string::npos )
			return true;
	return false;
}

static std::string Messages( const SCompareResult &result )
{
	std::string szAll;
	for ( size_t i = 0; i < result.messages.size() && i < 3; ++i )
		szAll += " | " + result.messages[i];
	return szAll;
}

// Replaces the first Name="value" attribute of the element <szElement ...>.
static bool EditAttribute( std::string *pXml, const std::string &szElement, const std::string &szName, const std::string *pNewValue, std::string *pOldValue )
{
	const std::string::size_type nElement = pXml->find( "<" + szElement + " " );
	if ( nElement == std::string::npos )
		return false;
	const std::string::size_type nEnd = pXml->find( '>', nElement );
	const std::string::size_type nAttr = pXml->find( " " + szName + "=\"", nElement );
	if ( nAttr == std::string::npos || nAttr > nEnd )
		return false;
	const std::string::size_type nValue = nAttr + szName.size() + 3;
	const std::string::size_type nClose = pXml->find( '"', nValue );
	*pOldValue = pXml->substr( nValue, nClose - nValue );
	if ( pNewValue )
		pXml->replace( nValue, nClose - nValue, *pNewValue );
	else
		pXml->erase( nAttr, nClose + 1 - nAttr );
	return true;
}

static void PlantedChanges( const fs::path &scratch, const std::map<EExportKind, fs::path> &samples )
{
	const auto weapon = samples.find( EExportKind::WEAPON );
	if ( !Check( weapon != samples.end(), "a shipped weapon to plant changes in" ) )
		return;
	std::string original;
	ReadBytes( weapon->second, &original );
	const fs::path planted = scratch / "planted" / "weapon.xml";

	// A float one ulp off: RPG/Dispersion is a float field of SWeaponRPGStats.
	{
		std::string xml = original, szOld;
		EditAttribute( &xml, "RPG", "Dispersion", 0, &szOld );
		const float fNext = std::nextafter( static_cast<float>( std::atof( szOld.c_str() ) ), 1e30f );
		char buffer[64];
		std::snprintf( buffer, sizeof( buffer ), "%.9g", fNext );
		const std::string szNew = buffer;
		xml = original;
		Check( EditAttribute( &xml, "RPG", "Dispersion", &szNew, &szOld ) && WriteBytes( planted, xml ), "plant Dispersion " + szOld + " -> " + szNew );
		const SCompareResult result = CompareStats( EExportKind::WEAPON, planted.string(), weapon->second.string() );
		Check( result.status == ECompareStatus::DIFFERENT && HasMessage( result, "field RPG/Dispersion: port " ) && HasMessage( result, "float 0x" ),
		       std::string( "one-ulp float change fails: " ) + CompareStatusName( result.status ) + Messages( result ) );
	}
	// A dropped field: RPG/RangeMax removed; the struct keeps its default.
	{
		std::string xml = original, szOld;
		Check( EditAttribute( &xml, "RPG", "RangeMax", 0, &szOld ) && WriteBytes( planted, xml ), "plant: drop RPG/RangeMax" );
		const SCompareResult result = CompareStats( EExportKind::WEAPON, planted.string(), weapon->second.string() );
		Check( result.status == ECompareStatus::DIFFERENT && HasMessage( result, "dropped field RPG/RangeMax" ),
		       std::string( "dropped field fails: " ) + CompareStatusName( result.status ) + Messages( result ) );
	}
	// An extra field no reader reads, on either side.
	{
		std::string xml = original;
		const std::string::size_type nAt = xml.find( "<KeyName>" );
		xml.insert( nAt, "<SomethingTheEngineDoesNotRead>1</SomethingTheEngineDoesNotRead>" );
		Check( nAt != std::string::npos && WriteBytes( planted, xml ), "plant: extra RPG/SomethingTheEngineDoesNotRead" );
		const SCompareResult port = CompareStats( EExportKind::WEAPON, planted.string(), weapon->second.string() );
		Check( port.status == ECompareStatus::UNKNOWN_FIELD && HasMessage( port, "UNKNOWN FIELD port RPG/SomethingTheEngineDoesNotRead" ),
		       std::string( "extra unknown field in the port export fails: " ) + CompareStatusName( port.status ) + Messages( port ) );
		const SCompareResult golden = CompareStats( EExportKind::WEAPON, weapon->second.string(), planted.string() );
		Check( golden.status == ECompareStatus::UNKNOWN_FIELD && HasMessage( golden, "UNKNOWN FIELD golden RPG/SomethingTheEngineDoesNotRead" ),
		       std::string( "extra unknown field in the golden fails: " ) + CompareStatusName( golden.status ) + Messages( golden ) );
	}
	// A stale node (comparator.cpp kStaleFields) on one side only: the
	// particle's old KeyData/Position, taken out of or put into a shipped copy.
	const auto particle = samples.find( EExportKind::PARTICLE );
	if ( Check( particle != samples.end(), "a shipped particle to plant a stale node in" ) )
	{
		std::string xml;
		ReadBytes( particle->second, &xml );
		const std::string::size_type nAt = xml.find( "<Position " );
		const bool bHad = nAt != std::string::npos;
		if ( bHad )
			xml.erase( nAt, xml.find( "/>", nAt ) + 2 - nAt );
		else
			xml.insert( xml.find( "<TextureName>" ), "<Position x=\"0\" y=\"0\" z=\"0\"/>" );
		const fs::path stale = scratch / "planted" / "particle.xml";
		WriteBytes( stale, xml );
		const SCompareResult result = CompareStats( EExportKind::PARTICLE, stale.string(), particle->second.string() );
		Check( result.status == ECompareStatus::DIFFERENT &&
		           HasMessage( result, bHad ? "stale field KeyData/Position: in the golden, not in the port export" : "stale field KeyData/Position: in the port export, not in the golden" ),
		       std::string( "a stale node on one side only fails: " ) + CompareStatusName( result.status ) + Messages( result ) );
	}
	// A wrong kind. GetGameStats<SMedalStats> opens a weapon's <RPG> without
	// complaint and reads none of it, so every weapon field is unknown; the
	// effect reader's <effect> base is not there at all.
	{
		const SCompareResult medal = CompareStats( EExportKind::MEDAL, weapon->second.string(), weapon->second.string() );
		Check( medal.status == ECompareStatus::UNKNOWN_FIELD && HasMessage( medal, "UNKNOWN FIELD port RPG/" ),
		       std::string( "a weapon read as a medal fails with unknown fields: " ) + CompareStatusName( medal.status ) + Messages( medal ) );
		const SCompareResult effect = CompareStats( EExportKind::EFFECT, weapon->second.string(), weapon->second.string() );
		Check( effect.status == ECompareStatus::UNREADABLE && HasMessage( effect, "opens <effect>" ),
		       std::string( "a weapon read as an effect is UNREADABLE: " ) + CompareStatusName( effect.status ) + Messages( effect ) );
	}
}

static fs::path FirstFile( const fs::path &data, const std::string &szSuffix )
{
	std::vector<fs::path> found;
	std::error_code error;
	for ( fs::recursive_directory_iterator it( FindNoCase( data, "Units" ), error ), end; !error && it != end; it.increment( error ) )
	{
		std::string szName = it->path().filename().string();
		std::transform( szName.begin(), szName.end(), szName.begin(), []( unsigned char c ) { return static_cast<char>( tolower( c ) ); } );
		if ( it->is_regular_file() && szName.size() > szSuffix.size() && szName.compare( szName.size() - szSuffix.size(), szSuffix.size(), szSuffix ) == 0 )
			found.push_back( it->path() );
	}
	std::sort( found.begin(), found.end() );
	return found.empty() ? fs::path() : found.front();
}

static void BytesAndDxt( const fs::path &data, const fs::path &scratch )
{
	std::string bytes;
	const fs::path height = FirstFile( data, "_h.dds" );
	if ( Check( !height.empty() && ReadBytes( height, &bytes ) && WriteBytes( scratch / "bytes" / "a_h.dds", bytes ), "a shipped _h.dds: " + height.generic_string() ) )
	{
		const fs::path copy = scratch / "bytes" / "a_h.dds";
		Check( CompareBytes( copy.string(), copy.string() ).status == ECompareStatus::EQUAL, "_h.dds equal to itself" );
		bytes[bytes.size() / 2] ^= 1;
		WriteBytes( scratch / "bytes" / "b_h.dds", bytes );
		const SCompareResult result = CompareBytes( ( scratch / "bytes" / "b_h.dds" ).string(), copy.string() );
		Check( result.status == ECompareStatus::DIFFERENT && HasMessage( result, "offset " + std::to_string( bytes.size() / 2 ) ),
		       std::string( "_h.dds with one bit flipped fails at its offset: " ) + CompareStatusName( result.status ) + Messages( result ) );
	}
	const fs::path colour = FirstFile( data, "_c.dds" );
	if ( Check( !colour.empty() && ReadBytes( colour, &bytes ) && bytes.size() > 160 && WriteBytes( scratch / "bytes" / "a_c.dds", bytes ), "a shipped _c.dds: " + colour.generic_string() ) )
	{
		const fs::path copy = scratch / "bytes" / "a_c.dds";
		Check( CompareDxt( copy.string(), copy.string(), SDxtTolerance() ).status == ECompareStatus::EQUAL, "_c.dds equal to itself, even with no tolerance loaded" );
		std::string header = bytes;
		header[28] = static_cast<char>( header[28] + 1 );
		WriteBytes( scratch / "bytes" / "mips_c.dds", header );
		const SCompareResult mips = CompareDxt( ( scratch / "bytes" / "mips_c.dds" ).string(), copy.string(), SDxtTolerance() );
		Check( mips.status == ECompareStatus::DIFFERENT && HasMessage( mips, "DDS mip count" ), std::string( "_c.dds with another mip count fails: " ) + CompareStatusName( mips.status ) + Messages( mips ) );
	}
}

// The DXT gate (S03 T07): dxt-tolerance.json, measured on shipped textures by
// `zig build measure-dxt-tolerance`, bounds the decoded deltas of a port
// _c.dds against its golden. A listed texture re-encoded by NDxt must pass it
// (the gate is the largest per-file measurement), and planted colour and alpha
// changes, a missing or malformed tolerance and a truncated file must fail.
static void DxtGate( const fs::path &data, const fs::path &scratch, const fs::path &fixtures )
{
	const fs::path dir = scratch / "dxt";
	SDxtTolerance tolerance;
	std::string szError;
	if ( !Check( LoadDxtTolerance( ( fixtures / "dxt-tolerance.json" ).string(), &tolerance, &szError ) && tolerance.Find( "DXT1" ) &&
	             tolerance.Find( "DXT3" ) && tolerance.Find( "DXT5" ),
	             "dxt-tolerance.json loads with DXT1/DXT3/DXT5 gates " + szError ) )
		return;
	for ( const auto &bad : { std::make_pair( std::string( "{ \"schema_version\": 1, \"formats\": {} }" ), std::string( "schema_version" ) ),
	                          std::make_pair( std::string( "{ \"schema_version\": 2, \"gate\": { \"DXT5\": { \"colour_max_delta\": 1, \"colour_p99\": 1, \"alpha_max_delta\": 1 } } }" ),
	                                          std::string( "alpha_p99" ) ),
	                          std::make_pair( std::string( "{ \"schema_version\": 2, \"gate\": { } }" ), std::string( "empty" ) ) } )
	{
		WriteBytes( dir / "bad-tolerance.json", bad.first );
		SDxtTolerance rejected;
		Check( !LoadDxtTolerance( ( dir / "bad-tolerance.json" ).string(), &rejected, &szError ) && !rejected.bLoaded && szError.find( bad.second ) != std::string::npos,
		       "a malformed tolerance is refused naming " + bad.second + ": " + szError );
	}
	SDxtTolerance missing;
	Check( !LoadDxtTolerance( ( dir / "missing.json" ).string(), &missing, &szError ) && szError.find( "cannot open" ) != std::string::npos,
	       "a missing tolerance file is refused: " + szError );

	// Listed textures, one per format, alpha in each: DXT1 punch-through, DXT3 explicit, DXT5 interpolated.
	static const char *const kListed[] = { "Terrain\\sets\\1\\tileset_c.dds", "Scenarios\\Chapters\\Allies\\Ardennes\\map_c.dds", "Units\\Humans\\Allies\\Bren\\1_c.dds" };
	for ( const char *pszListed : kListed )
	{
		szError.clear();
		const fs::path original = FindNoCase( data, pszListed );
		std::string bytes, reencoded;
		SDdsImage image;
		if ( !Check( !original.empty() && ReadBytes( original, &bytes ) && DecodeDds( bytes, &image, &szError ), std::string( "a listed _c.dds decodes: " ) + pszListed + " " + szError ) )
			continue;
		const std::string szName = image.szFourCC;
		const fs::path golden = dir / ( szName + "-golden_c.dds" );
		WriteBytes( golden, bytes );
		Check( EncodeDds( bytes, image, &reencoded, &szError ) && reencoded.size() == bytes.size() && WriteBytes( dir / ( szName + "-reencoded_c.dds" ), reencoded ),
		       szName + " re-encodes by NDxt to the same size " + szError );
		const SCompareResult within = CompareDxt( ( dir / ( szName + "-reencoded_c.dds" ) ).string(), golden.string(), tolerance );
		Check( within.status == ECompareStatus::EQUAL && HasMessage( within, "within the " + szName + " gate" ),
		       szName + " re-encoded by NDxt is within its gate: " + CompareStatusName( within.status ) + Messages( within ) );
		const SCompareResult unloaded = CompareDxt( ( dir / ( szName + "-reencoded_c.dds" ) ).string(), golden.string(), SDxtTolerance() );
		Check( reencoded == bytes || unloaded.status == ECompareStatus::UNREADABLE,
		       szName + " pixels that differ with no tolerance loaded are UNREADABLE, never EQUAL: " + CompareStatusName( unloaded.status ) );

		// A 16x16 region pushed to the far end of each channel: colour deltas of at least 128.
		SDdsImage planted = image;
		SDdsMip &mip = planted.mips[0];
		const bool bAlpha = szName != "DXT1";
		for ( int y = 0; y < std::min( 16, mip.nHeight ); ++y )
			for ( int x = 0; x < std::min( 16, mip.nWidth ); ++x )
			{
				unsigned &nPixel = mip.pixels[static_cast<size_t>( y ) * mip.nWidth + x];
				unsigned nFlipped = 0;
				for ( int nShift = 0; nShift < 32; nShift += 8 )
				{
					const unsigned nChannel = ( nPixel >> nShift ) & 0xff;
					const bool bFlip = nShift < 24 || bAlpha;
					nFlipped |= ( bFlip ? ( nChannel < 128 ? 255u : 0u ) : nChannel ) << nShift;
				}
				nPixel = nFlipped;
			}
		std::string szPlanted;
		EncodeDds( bytes, planted, &szPlanted, &szError );
		WriteBytes( dir / ( szName + "-planted_c.dds" ), szPlanted );
		const SCompareResult far = CompareDxt( ( dir / ( szName + "-planted_c.dds" ) ).string(), golden.string(), tolerance );
		Check( far.status == ECompareStatus::DIFFERENT && HasMessage( far, szName + " colour max delta" ) && HasMessage( far, "largest delta" ) &&
		           ( !bAlpha || HasMessage( far, szName + " alpha max delta" ) ),
		       szName + " with a planted 16x16 region fails the gate: " + CompareStatusName( far.status ) + Messages( far ) );

		const std::string truncated = bytes.substr( 0, 128 + ( bytes.size() - 128 ) / 2 );
		WriteBytes( dir / ( szName + "-truncated_c.dds" ), truncated );
		const SCompareResult cut = CompareDxt( ( dir / ( szName + "-truncated_c.dds" ) ).string(), golden.string(), tolerance );
		Check( cut.status == ECompareStatus::UNREADABLE && HasMessage( cut, "mip 0 needs" ), szName + " cut short is UNREADABLE: " + CompareStatusName( cut.status ) + Messages( cut ) );
	}

	// DXT1 punch-through survives NDxt: an alpha-0 pixel that turns opaque is an alpha difference.
	{
		const fs::path original = FindNoCase( data, "Terrain\\sets\\1\\tileset_c.dds" );
		std::string bytes, szOpaque;
		SDdsImage image;
		if ( ReadBytes( original, &bytes ) && DecodeDds( bytes, &image, &szError ) )
		{
			int nCleared = 0;
			for ( unsigned &nPixel : image.mips[0].pixels )
				if ( ( nPixel >> 24 ) == 0 )
				{
					nPixel |= 0xff000000u;
					++nCleared;
				}
			EncodeDds( bytes, image, &szOpaque, &szError );
			WriteBytes( dir / "DXT1-opaque_c.dds", szOpaque );
			const SCompareResult opaque = CompareDxt( ( dir / "DXT1-opaque_c.dds" ).string(), ( dir / "DXT1-golden_c.dds" ).string(), tolerance );
			Check( nCleared > 0 && opaque.status == ECompareStatus::DIFFERENT && HasMessage( opaque, "DXT1 alpha max delta 255 exceeds the gate 0" ),
			       "a DXT1 tileset that lost its punch-through fails the alpha gate: " + std::to_string( nCleared ) + " pixels, " + CompareStatusName( opaque.status ) + Messages( opaque ) );
		}
	}

	// An uncompressed DDS has no pixel tolerance: a changed byte is a difference.
	const fs::path rgb = FindNoCase( data, "UI\\container_c.dds" );
	std::string bytes;
	if ( Check( !rgb.empty() && ReadBytes( rgb, &bytes ) && bytes.size() > 200, "an uncompressed shipped _c.dds: " + rgb.generic_string() ) )
	{
		WriteBytes( dir / "rgb-golden_c.dds", bytes );
		bytes[bytes.size() - 1] ^= 1;
		WriteBytes( dir / "rgb-port_c.dds", bytes );
		const SCompareResult result = CompareDxt( ( dir / "rgb-port_c.dds" ).string(), ( dir / "rgb-golden_c.dds" ).string(), tolerance );
		Check( result.status == ECompareStatus::DIFFERENT && HasMessage( result, "uncompressed" ), std::string( "an uncompressed DDS with one byte changed fails: " ) + CompareStatusName( result.status ) + Messages( result ) );
	}
}

// The S06 exporters over their fixtures. The expected structs are the
// fixture projects' values put through the MFC frames' SaveRPGStats by hand
// (WeaponFrm.cpp, MineFrm.cpp, TrenchFrm.cpp, SquadFrm.cpp), written by the
// engine's own writer, so the comparison is the game's reader on both sides.

template <class TStats>
static bool WriteExpected( const fs::path &file, TStats &stats )
{
	std::error_code error;
	fs::create_directories( file.parent_path(), error );
	CPtr<IDataStorage> pStorage = CreateStorage( ( file.parent_path().string() + "/" ).c_str(), STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
	CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->CreateStream( file.filename().string().c_str(), STREAM_ACCESS_WRITE ) : 0;
	if ( pStream == 0 )
		return false;
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
	if ( pDT == 0 )
		return false;
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &stats );
	return true;
}

// A copy of a fixture folder at <scratch>/<ext>/<ext>/, so the project's
// folder name, which names the exported file, is the extension.
static fs::path CopyFixture( const fs::path &fixtures, const fs::path &scratch, const std::string &szExt )
{
	const fs::path dir = scratch / szExt / szExt;
	std::error_code error;
	fs::remove_all( scratch / szExt, error );
	fs::create_directories( dir, error );
	// Recursive: the fence fixture's pictures sit in its Fences directory.
	fs::copy( fixtures / szExt, dir, fs::copy_options::recursive | fs::copy_options::overwrite_existing, error );
	return dir / ( "project." + szExt );
}

static bool LoadProject( const fs::path &file, Project *pProject )
{
	std::string szBytes, szError;
	return ReadBytes( file, &szBytes ) && Load( szBytes, *pProject, szError );
}

// The objects database a squad export asks, as a fixture: the one infantry
// unit the scp fixture names, under its path and key in Data/objects.xml.
static bool FixtureUnitKey( const std::string &szPath, std::string &szKey )
{
	if ( szPath != "units\\humans\\ussr\\mosin" )
		return false;
	szKey = "USSR_Mosin";
	return true;
}

// An uncompressed 24-bit bottom-up TGA of a gradient with edges, so the DXT
// blocks have something to approximate; nWidth x nHeight, any size.
static std::string MakeTga( int nWidth, int nHeight )
{
	std::string tga( 18, '\0' );
	tga[2] = 2;
	tga[12] = char( nWidth & 0xff ); tga[13] = char( nWidth >> 8 );
	tga[14] = char( nHeight & 0xff ); tga[15] = char( nHeight >> 8 );
	tga[16] = 24;
	for ( int y = 0; y < nHeight; ++y )
		for ( int x = 0; x < nWidth; ++x )
		{
			const bool bEdge = ( ( x / 4 ) + ( y / 4 ) ) % 2 == 0;
			tga += char( bEdge ? 200 : 40 + y * 3 );          // B
			tga += char( x * 255 / std::max( nWidth - 1, 1 ) ); // G
			tga += char( bEdge ? 30 : 220 - x * 2 );          // R
		}
	return tga;
}

// An uncompressed 24-bit TGA as the ARGB mip the DXT gate compares.
static bool ReadTga( const fs::path &file, SDdsMip *pMip )
{
	std::string tga;
	if ( !ReadBytes( file, &tga ) || tga.size() < 18 || tga[2] != 2 || tga[16] != 24 )
		return false;
	pMip->nWidth = (unsigned char)tga[12] | ( (unsigned char)tga[13] << 8 );
	pMip->nHeight = (unsigned char)tga[14] | ( (unsigned char)tga[15] << 8 );
	if ( tga.size() < 18 + size_t( pMip->nWidth ) * pMip->nHeight * 3 )
		return false;
	pMip->pixels.assign( size_t( pMip->nWidth ) * pMip->nHeight, 0 );
	for ( int y = 0; y < pMip->nHeight; ++y )
		for ( int x = 0; x < pMip->nWidth; ++x )
		{
			const unsigned char *p = (const unsigned char *)tga.data() + 18 + ( size_t( y ) * pMip->nWidth + x ) * 3;
			pMip->pixels[ size_t( pMip->nHeight - 1 - y ) * pMip->nWidth + x ] = 0xff000000u | ( p[2] << 16 ) | ( p[1] << 8 ) | p[0];
		}
	return true;
}

static void PlantTgas( const fs::path &dir, std::initializer_list<const char *> names, int nWidth = 16, int nHeight = 16 )
{
	for ( const char *pszName : names )
		WriteBytes( dir / pszName, MakeTga( nWidth, nHeight ) );
}

static int CountFiles( const fs::path &dir, const std::string &szExtension )
{
	int nCount = 0;
	std::error_code error;
	for ( fs::recursive_directory_iterator it( dir, error ), end; !error && it != end; it.increment( error ) )
		if ( it->is_regular_file() && it->path().extension() == szExtension )
			++nCount;
	return nCount;
}

// The exported DXT file decoded with NDxt against the source TGA, within the
// measured gate of its format; the measured deltas are logged beside it.
static void CheckDds( const std::string &szWhat, const fs::path &dds, const fs::path &tga, const std::string &szFourCC, const SDxtTolerance &tolerance )
{
	std::string szBytes, szError;
	SDdsImage image;
	SDdsMip source;
	if ( !Check( ReadBytes( dds, &szBytes ) && DecodeDds( szBytes, &image, &szError ) && image.szFourCC == szFourCC && !image.mips.empty(),
	             szWhat + ": " + dds.filename().string() + " is a " + szFourCC + " DDS " + szError ) )
		return;
	if ( !Check( ReadTga( tga, &source ), szWhat + ": the source TGA reads" ) )
		return;
	const SDdsMip &mip = image.mips[0];
	if ( !Check( mip.nWidth == source.nWidth && mip.nHeight == source.nHeight, szWhat + ": the DDS keeps the picture's size, " + std::to_string( mip.nWidth ) + "x" + std::to_string( mip.nHeight ) ) )
		return;
	SDxtDelta delta;
	delta.Add( 0, mip, source );
	const SDxtStats measured = delta.Stats();
	const SDxtStats *pGate = tolerance.Find( szFourCC );
	// The gate's p99 was measured on pictures already DXT-quantised, which an
	// encoder reproduces almost exactly; a TGA is not, so its own p99 is
	// logged and only the max is held to the gate.
	Check( pGate != nullptr && measured.nColourMax <= pGate->nColourMax && measured.nAlphaMax <= pGate->nAlphaMax,
	       szWhat + ": " + szFourCC + " within the dxt-tolerance max gate (" + ( pGate ? std::to_string( pGate->nColourMax ) + "/" + std::to_string( pGate->nAlphaMax ) : std::string( "none" ) ) + "): colour max " + std::to_string( measured.nColourMax ) + " p99 " + std::to_string( measured.nColourP99 ) +
	       ", alpha max " + std::to_string( measured.nAlphaMax ) + " p99 " + std::to_string( measured.nAlphaP99 ) );
}

struct SExportRun
{
	bool bExported = false;
	SExportOutcome outcome;
	fs::path data;
};

static SExportRun RunExporter( const std::string &szExt, const fs::path &project, const fs::path &data, SExportContext context )
{
	SExportRun run;
	run.data = data;
	std::error_code error;
	fs::remove_all( data, error );
	fs::create_directories( data, error );
	Project loaded;
	const FExporter pfnExporter = FindExporter( szExt );
	if ( !Check( pfnExporter != nullptr, "export " + szExt + ": FindExporter has a production exporter" ) )
		return run;
	if ( !Check( LoadProject( project, &loaded ), "export " + szExt + ": the fixture loads" ) )
		return run;
	context.szProjectPath = project.string();
	context.szStagingRoot = data.string();
	run.bExported = pfnExporter( loaded, context, run.outcome );
	return run;
}

// The exported file read by the engine: readable, every node read, none of
// an older layout, then field-equal with the expected struct.
template <class TStats>
static void CheckExported( EExportKind kind, const std::string &szExt, const SExportRun &run, const std::string &szFile, TStats &expected, const fs::path &scratch )
{
	const fs::path file = run.data / szFile;
	if ( !Check( run.bExported && fs::is_regular_file( file ), "export " + szExt + ": writes " + szFile + " " + run.outcome.szError ) )
		return;
	const SExportRead read = ReadExport( kind, file.string() );
	Check( read.bReadable && read.unknown.empty() && read.stale.empty(),
	       "export " + szExt + ": " + GetExportKindInfo( kind ).pszReader + " reads every node of the export (" + std::to_string( read.unknown.size() ) +
	       " unknown, " + std::to_string( read.stale.size() ) + " stale) " + read.szError );
	const fs::path golden = scratch / szExt / "expected.xml";
	if ( !Check( WriteExpected( golden, expected ), "export " + szExt + ": the engine writes the hand-derived expectation" ) )
		return;
	const SCompareResult result = CompareStats( kind, file.string(), golden.string() );
	Check( result.status == ECompareStatus::EQUAL && result.nFieldsCompared > 0,
	       "export " + szExt + ": field-equal with the fixture's values as MFC's SaveRPGStats fills them (" + std::to_string( result.nFieldsCompared ) +
	       " fields): " + CompareStatusName( result.status ) + Messages( result ) );
}

static void Exporters( const fs::path &fixtures, const fs::path &data, const fs::path &scratchRoot )
{
	const fs::path scratch = scratchRoot / "export";
	SExportContext context;
	context.findUnitKey = &FixtureUnitKey;
	SDxtTolerance tolerance;
	{
		std::string szError;
		Check( LoadDxtTolerance( ( fixtures / "dxt-tolerance.json" ).string(), &tolerance, &szError ), "export: the DXT gate loads " + szError );
	}

	// Weapon: WeaponFrm.cpp FillRPGStats over the one shell of the fixture.
	{
		const fs::path project = CopyFixture( fixtures, scratch, "wpn" );
		const SExportRun run = RunExporter( "wpn", project, scratch / "wpn" / "data", context );
		SWeaponRPGStats expected;
		expected.szKeyName = "Unknown Weapon";
		expected.wDeltaAngle = 10;
		expected.nAmmoPerBurst = 1;
		expected.fDispersion = 1.0f;
		expected.fRangeMin = 1.0f;
		expected.fRangeMax = 100.0f;
		expected.nCeiling = 100;
		expected.fAimingTime = 100.0f;
		expected.fRevealRadius = 10.0f;
		SWeaponRPGStats::SShell &shell = expected.shells[0];
		shell.trajectory = SWeaponRPGStats::SShell::TRAJECTORY_LINE;
		shell.nPiercing = 0;
		shell.nPiercingRandom = 0;
		shell.fDamagePower = 5.0f;
		shell.nDamageRandom = 2;
		shell.fArea = 1.0f;
		shell.fArea2 = 2.0f;
		shell.fSpeed = 10.0f;
		shell.fDetonationPower = 0.0f;
		shell.fFireRate = 1.0f;
		shell.fRelaxTime = 100.0f;
		shell.eDamageType = SWeaponRPGStats::SShell::DAMAGE_HEALTH;
		shell.fTraceProbability = 10.0f / 100.0f;   // "Trace probability (%)"
		shell.fTraceSpeedCoeff = 1.0f;
		shell.fBrokeTrackProbability = 0.01f;
		shell.specials.RemoveData( 0 );
		shell.flashFire.nPower = 100;
		shell.flashFire.nDuration = 1000;
		shell.flashExplosion.nPower = 100;
		shell.flashExplosion.nDuration = 1000;
		CheckExported( EExportKind::WEAPON, "wpn", run, "weapons/wpn.xml", expected, scratch );

		// Stats only writes the same stats: a weapon has no graphics.
		SExportContext statsOnly = context;
		statsOnly.bStatsOnly = true;
		const SExportRun runStats = RunExporter( "wpn", project, scratch / "wpn" / "data-stats", statsOnly );
		std::string szFull, szStats;
		Check( runStats.bExported && ReadBytes( run.data / "weapons/wpn.xml", &szFull ) && ReadBytes( runStats.data / "weapons/wpn.xml", &szStats ) &&
		       szFull == szStats && runStats.outcome.nWritten == 1, "export wpn: stats only writes the same weapons/wpn.xml" );

		// own_data/export_file_name, as MFC's Export stored it, names the file.
		std::string szXml;
		ReadBytes( project, &szXml );
		const std::string::size_type nOpen = szXml.find( '>', szXml.find( "<Weapon_Composer_Project" ) );
		szXml.insert( nOpen + 1, "<own_data><export_file_name>custom\\named.xml</export_file_name></own_data>" );
		const fs::path named = project.parent_path() / "named.wpn";
		WriteBytes( named, szXml );
		const SExportRun runNamed = RunExporter( "wpn", named, scratch / "wpn" / "data-named", context );
		Check( runNamed.bExported && fs::is_regular_file( runNamed.data / "weapons/custom/named.xml" ),
		       "export wpn: own_data/export_file_name puts the file at weapons\\custom\\named.xml " + runNamed.outcome.szError );
	}

	// Mine: MineFrm.cpp FillRPGStats; the weapon prop names both fields.
	{
		const fs::path project = CopyFixture( fixtures, scratch, "mcp" );
		PlantTgas( project.parent_path(), { "1.tga", "1s.tga" } );
		const SExportRun run = RunExporter( "mcp", project, scratch / "mcp" / "data", context );
		SMineRPGStats expected;
		expected.szKeyName = "";
		expected.fWeight = 10.0f;
		expected.szFlagModel = "1";
		expected.szWeapon = "";
		CheckExported( EExportKind::MINE, "mcp", run, "objects/simpleobjects/common/summer/mine/mcp/1.xml", expected, scratch );
		Check( run.outcome.szObjectName == "objects\\simpleobjects\\common\\summer\\mine\\mcp\\1",
		       "export mcp: names the composed sprite \"1\" beside the stats: " + run.outcome.szObjectName );

		// ComposeSingleObject: the sprite and the shadow, each as _c (DXT5),
		// _l and _h with its animation file.
		const fs::path mineDir = run.data / "objects/simpleobjects/common/summer/mine/mcp";
		for ( const char *pszName : { "1_c.dds", "1_l.dds", "1_h.dds", "1.san", "1s_c.dds", "1s_l.dds", "1s_h.dds", "1s.san" } )
			Check( fs::is_regular_file( mineDir / pszName ), std::string( "export mcp: composes " ) + pszName );
		CheckDds( "export mcp", mineDir / "1_c.dds", project.parent_path() / "1.tga", "DXT5", tolerance );

		SExportContext statsOnly = context;
		statsOnly.bStatsOnly = true;
		const SExportRun runStats = RunExporter( "mcp", project, scratch / "mcp" / "data-stats", statsOnly );
		Check( runStats.bExported && CountFiles( runStats.data, ".dds" ) == 0 && CountFiles( runStats.data, ".san" ) == 0 && CountFiles( runStats.data, ".xml" ) == 1,
		       "export mcp: stats only writes the stats and no image " + runStats.outcome.szError );

		// Negative cases: nothing may be left promoted by a failed compose.
		fs::remove( project.parent_path() / "1s.tga" );
		const SExportRun runNoShadow = RunExporter( "mcp", project, scratch / "mcp" / "data-noshadow", context );
		Check( !runNoShadow.bExported && runNoShadow.outcome.szError.find( "1s.tga" ) != std::string::npos && CountFiles( runNoShadow.data, ".dds" ) == 0,
		       "export mcp: a missing 1s.tga fails naming it and writes no picture: " + runNoShadow.outcome.szError );
		PlantTgas( project.parent_path(), { "1s.tga" } );
		std::string szTga;
		ReadBytes( project.parent_path() / "1.tga", &szTga );
		WriteBytes( project.parent_path() / "1.tga", szTga.substr( 0, 18 + 100 ) );
		const SExportRun runTruncated = RunExporter( "mcp", project, scratch / "mcp" / "data-truncated", context );
		Check( !runTruncated.bExported && runTruncated.outcome.szError.find( "1.tga" ) != std::string::npos && CountFiles( runTruncated.data, ".dds" ) == 0,
		       "export mcp: a truncated 1.tga is rejected naming it: " + runTruncated.outcome.szError );
		PlantTgas( project.parent_path(), { "1.tga", "1s.tga" }, 20, 12 );
		const SExportRun runOdd = RunExporter( "mcp", project, scratch / "mcp" / "data-odd", context );
		Log( "export mcp: a 20x12 picture exports " + std::string( runOdd.bExported ? "and writes " : "and fails: " + runOdd.outcome.szError ) +
		     std::to_string( CountFiles( runOdd.data, ".dds" ) ) + " DDS" );
		Check( runOdd.bExported && CountFiles( runOdd.data, ".dds" ) == 6, "export mcp: a picture that is not a power of two is composed as MFC did: " + runOdd.outcome.szError );
	}

	// Trench: TrenchFrm.cpp SaveRPGStats. The fixture's one segment has no
	// model file: MFC's "Cannot copy file" box, and no segment.
	{
		const fs::path project = CopyFixture( fixtures, scratch, "trc" );
		PlantTgas( project.parent_path(), { "1.tga", "1w.tga", "1a.tga" } );
		const SExportRun run = RunExporter( "trc", project, scratch / "trc" / "data", context );
		SEntrenchmentRPGStats expected;
		expected.szKeyName = "Unknown Trench";
		expected.fMaxHP = 100.0f;
		for ( int i = 0; i < 6; ++i )
		{
			expected.defences[i].nArmorMin = 300;
			expected.defences[i].nArmorMax = 300;
			expected.defences[i].fSilhouette = 1.0f;
		}
		CheckExported( EExportKind::ENTRENCHMENT, "trc", run, "units/technics/common/entrenchment/trc/1.xml", expected, scratch );
		Check( run.outcome.warnings.size() == 1 && run.outcome.warnings[0].find( "Cannot copy file" ) != std::string::npos,
		       "export trc: the empty segment source is MFC's \"Cannot copy file\" warning" );
		Check( run.outcome.szObjectName == "units\\technics\\common\\entrenchment\\trc\\1", "export trc: names the sprite \"1\" beside the stats: " + run.outcome.szObjectName );
		const fs::path trenchDir = run.data / "units/technics/common/entrenchment/trc";
		for ( const char *pszName : { "1", "1w", "1a" } )
		{
			for ( const char *pszSuffix : { "_c.dds", "_l.dds", "_h.dds" } )
				Check( fs::is_regular_file( trenchDir / ( std::string( pszName ) + pszSuffix ) ), std::string( "export trc: converts " ) + pszName + pszSuffix );
			CheckDds( std::string( "export trc " ) + pszName, trenchDir / ( std::string( pszName ) + "_c.dds" ), project.parent_path() / ( std::string( pszName ) + ".tga" ), "DXT5", tolerance );
		}

		SExportContext statsOnly = context;
		statsOnly.bStatsOnly = true;
		const SExportRun runStats = RunExporter( "trc", project, scratch / "trc" / "data-stats", statsOnly );
		Check( runStats.bExported && CountFiles( runStats.data, ".dds" ) == 0 && CountFiles( runStats.data, ".xml" ) == 1,
		       "export trc: stats only writes the stats and no image " + runStats.outcome.szError );

		fs::remove( project.parent_path() / "1w.tga" );
		const SExportRun runNoWater = RunExporter( "trc", project, scratch / "trc" / "data-nowater", context );
		bool bWarned = false;
		for ( const std::string &szWarning : runNoWater.outcome.warnings )
			bWarned = bWarned || szWarning.find( "1w.tga" ) != std::string::npos;
		Check( runNoWater.bExported && bWarned && !fs::exists( runNoWater.data / "units/technics/common/entrenchment/trc/1w_c.dds" ) && fs::exists( runNoWater.data / "units/technics/common/entrenchment/trc/1a_c.dds" ),
		       "export trc: a missing 1w.tga is a warning naming it, the other pictures still convert" );

		// A segment model the export can open: its box from chunk 4 of the
		// shipped .mod and its fire places from the context's mesh reader.
		std::string szXml;
		ReadBytes( project, &szXml );
		const std::string szEmpty = "<default_name>Source file</default_name>";
		const std::string::size_type nSource = szXml.find( "<string_value/>", szXml.find( szEmpty ) );
		if ( Check( nSource != std::string::npos, "export trc: the fixture has a segment source" ) )
		{
			szXml.replace( nSource, std::string( "<string_value/>" ).size(), "<string_value>models\\5.mod</string_value>" );
			const fs::path withModel = project.parent_path() / "model.trc";
			WriteBytes( withModel, szXml );
			std::string szMod;
			ReadBytes( FindNoCase( data, "Units\\Technics\\Common\\Entrenchment\\5.mod" ), &szMod );
			WriteBytes( project.parent_path() / "models" / "5.mod", szMod );
			PlantTgas( project.parent_path() / "models", { "1.tga", "1w.tga", "1a.tga" } );

			const SExportRun runNoMesh = RunExporter( "trc", withModel, scratch / "trc" / "data-nomesh", context );
			Check( !runNoMesh.bExported && runNoMesh.outcome.szError.find( "5.mod" ) != std::string::npos &&
			       runNoMesh.outcome.szError.find( "mesh builder" ) != std::string::npos,
			       "export trc: without a mesh reader the export fails, naming the model: " + runNoMesh.outcome.szError );

			SExportContext withMesh = context;
			withMesh.meshFirePlaces = []( const std::string &szModFile, std::vector<std::pair<float, float>> &firePlaces, std::string & )
			{
				firePlaces.push_back( std::make_pair( 1.5f, -2.5f ) );
				return szModFile.find( "5.mod" ) != std::string::npos;
			};
			const SExportRun runMesh = RunExporter( "trc", withModel, scratch / "trc" / "data-mesh", withMesh );
			SEntrenchmentRPGStats read;
			bool bRead = false;
			if ( runMesh.bExported )
			{
				const fs::path exported = runMesh.data / "units/technics/common/entrenchment/trc";
				CPtr<IDataStorage> pStorage = OpenStorage( ( exported.string() + "/" ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
				CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->OpenStream( "1.xml", STREAM_ACCESS_READ ) : 0;
				CPtr<IDataTree> pDT = pStream != 0 ? CreateDataTreeSaver( pStream, IDataTree::READ, "base" ) : 0;
				if ( pDT != 0 )
				{
					CTreeAccessor tree = pDT;
					tree.Add( "RPG", &read );
					bRead = true;
				}
			}
			{
				std::string szCopy;
				Check( ReadBytes( runMesh.data / "units/technics/common/entrenchment/trc/5.mod", &szCopy ) && szCopy == szMod,
				       "export trc: the segment model is copied beside the stats, byte-identical" );
				Check( fs::is_regular_file( runMesh.data / "units/technics/common/entrenchment/trc/1_c.dds" ),
				       "export trc: 1/1w/1a come from the first model's folder" );
			}
			Check( bRead && read.segments.size() == 1 && read.segments[0].szModel == "5" && read.lines.size() == 1 && read.lines[0] == 0 &&
			       read.segments[0].eType == SEntrenchmentRPGStats::EST_LINE && read.segments[0].fCoverage == 0.2f &&
			       read.segments[0].vAABBHalfSize.x > 1.0f && read.segments[0].fireplaces.size() == 1 && read.segments[0].fireplaces[0].x == 1.5f,
			       "export trc: a line segment with model 5, coverage 0.2, the .mod's box and the reader's fire place " + runMesh.outcome.szError );
		}
	}

	// Squad: SquadFrm.cpp SaveRPGStats. The member is a path, resolved by
	// the objects database; the soldier is placed relative to the formation's
	// zero point moved by the cross icon's half size on screen.
	{
		const fs::path project = CopyFixture( fixtures, scratch, "scp" );
		const SExportRun run = RunExporter( "scp", project, scratch / "scp" / "data", context );
		Check( run.bExported && !run.outcome.warnings.empty() && run.outcome.warnings[0].find( "icon.tga" ) != std::string::npos &&
		       !fs::exists( run.data / "squads/scp/icon.tga" ),
		       "export scp: a missing icon is a warning naming it and the stats are still written" );
		std::string szIcon;
		ReadBytes( project.parent_path() / "sprite-1frame.tga", &szIcon );
		WriteBytes( project.parent_path() / "icon.tga", szIcon );
		const SExportRun runIcon = RunExporter( "scp", project, scratch / "scp" / "data-icon", context );
		std::string szCopy;
		Check( runIcon.bExported && ReadBytes( runIcon.data / "squads/scp/icon.tga", &szCopy ) && szCopy == szIcon && runIcon.outcome.warnings.empty(),
		       "export scp: the icon is copied beside the stats, byte-identical" );
		SExportContext statsOnly = context;
		statsOnly.bStatsOnly = true;
		const SExportRun runStats = RunExporter( "scp", project, scratch / "scp" / "data-stats", statsOnly );
		Check( runStats.bExported && !fs::exists( runStats.data / "squads/scp/icon.tga" ), "export scp: stats only copies no icon" );

		// The shift on screen (15.4, 15.4) in the world, under the squad
		// frame's camera (SetDefaultCamera: yaw 45, pitch -(90+30)) and one
		// world unit per pixel: x + y = 15.4 / cos45, x - y = 15.4 / (cos45 sin30).
		const float fCos45 = std::cos( ToRadian( 45.0f ) );
		const float fSin30 = std::sin( ToRadian( 30.0f ) );
		const float fSum = 15.4f / fCos45, fDiff = 15.4f / ( fCos45 * fSin30 );
		const float fShiftX = ( fSum + fDiff ) / 2, fShiftY = ( fSum - fDiff ) / 2;
		// The same shift through the engine's own view matrix: back on screen
		// it is 15.4 pixels right and 15.4 down (the viewport flips y).
		{
			SHMatrix view;
			CreateViewMatrixRH( &view, VNULL3, CQuat( ToRadian( 45.0f ), V3_AXIS_Z ) * CQuat( -ToRadian( 90.0f + 30.0f ), V3_AXIS_X ) );
			CVec3 vCamera;
			view.RotateHVector( &vCamera, CVec3( fShiftX, fShiftY, 0 ) );
			Check( std::fabs( vCamera.x - 15.4f ) < 1e-3f && std::fabs( -vCamera.y - 15.4f ) < 1e-3f,
			       "export scp: the zero point's shift is (15.4, 15.4) pixels on the squad frame's screen: (" + std::to_string( vCamera.x ) + ", " +
			       std::to_string( -vCamera.y ) + ")" );
		}
		SSquadRPGStats expected;
		expected.szIcon = "icon.tga";
		expected.type = SSquadRPGStats::RIFLEMANS;
		expected.memberNames.push_back( "USSR_Mosin" );
		SSquadRPGStats::SFormation form;
		form.type = SSquadRPGStats::SFormation::DEFAULT;
		form.changesByEvent.resize( 1 );
		form.changesByEvent[0] = -1;
		form.cLieFlag = 0;
		form.fSpeedBonus = form.fDispersionBonus = form.fFireRateBonus = form.fRelaxTimeBonus = form.fCoverBonus = 1.0f;
		SSquadRPGStats::SFormation::SEntry entry;
		entry.szSoldier = "USSR_Mosin";
		const CVec3 vRealZero( 724.077f + fShiftX, 362.039f + fShiftY, 0 );
		entry.vPos.x = 723.42f - vRealZero.x;
		entry.vPos.y = 361.71f - vRealZero.y;
		entry.fDir = ToDegree( 0.5f );
		form.order.push_back( entry );
		expected.formations.push_back( form );
		CheckExported( EExportKind::SQUAD, "scp", run, "squads/scp/1.xml", expected, scratch );

		SExportContext noLookup = context;
		noLookup.findUnitKey = nullptr;
		const SExportRun runNoLookup = RunExporter( "scp", project, scratch / "scp" / "data-nolookup", noLookup );
		Check( !runNoLookup.bExported && runNoLookup.outcome.szError.find( "USSR\\Mosin" ) != std::string::npos,
		       "export scp: without an objects database the member cannot be resolved, and the error names it: " + runNoLookup.outcome.szError );
		SExportContext unknown = context;
		unknown.findUnitKey = []( const std::string &, std::string & ) { return false; };
		const SExportRun runUnknown = RunExporter( "scp", project, scratch / "scp" / "data-unknown", unknown );
		Check( !runUnknown.bExported && runUnknown.outcome.szError.find( "Can't find stats for \"units\\humans\\ussr\\mosin\"" ) != std::string::npos,
		       "export scp: an unknown member is MFC's \"Can't find stats\": " + runUnknown.outcome.szError );
	}

	// Infantry: AnimationFrm.cpp FillRPGStats over the fixture's 24
	// animations (frame time 125, action frame 0, speed 1). No frame file exists, so
	// the compose finds no valid animation: a warning, as MFC's message box,
	// and the stats are written all the same.
	{
		const fs::path project = CopyFixture( fixtures, scratch, "unt" );
		const SExportRun run = RunExporter( "unt", project, scratch / "unt" / "data", context );
		SInfantryRPGStats expected;
		expected.szKeyName = "Unknown unit";
		expected.type = RPG_TYPE_SOLDIER;
		expected.fMaxHP = 100.0f;
		expected.nMinArmor = expected.nMaxArmor = 4;
		expected.fSight = 20.0f;
		expected.fCamouflage = 1.0f;
		expected.fSpeed = 2.0f;
		expected.fPassability = 100.0f;
		expected.bCanAttackUp = true;
		expected.bCanAttackDown = true;
		expected.fPrice = 1.0f;
		expected.fSightPower = 1.0f;
		expected.szAcksNames.resize( 2 );
		expected.availCommands.Clear();
		expected.availExposures.Clear();
		expected.fRotateSpeed = 0.0f;
		expected.nPriority = 0;
		expected.nUninstallRotate = 0;
		expected.nUninstallTransport = 0;
		// An empty grenade collapses to one gun: the generic weapon of the Weapon item.
		expected.guns.resize( 1 );
		expected.guns[0].szWeapon = "generic";
		expected.guns[0].nAmmo = 100;
		expected.guns[0].fReloadCost = 100.0f;
		expected.fRunSpeed = 1.0f;
		expected.fCrawlSpeed = 1.0f;
		// The //CRAP +1: one slot more than the 24 animations. Only Idle has a frame item, "frame-1":
		// 125 ms for it, in its animtimes slot and in its description's length.
		expected.animtimes.assign( 25, 0 );
		expected.animtimes[ANIMATION_IDLE] = 125;
		expected.animdescs.resize( ANIMATION_LAST_ANIMATION );
		static const int kTypes[24] =
		{
			ANIMATION_MOVE, ANIMATION_CRAWL, ANIMATION_SHOOT, ANIMATION_SHOOT_DOWN, ANIMATION_SHOOT_TRENCH, ANIMATION_AIMING, ANIMATION_AIMING_DOWN,
			ANIMATION_AIMING_TRENCH, ANIMATION_THROW, ANIMATION_THROW_DOWN, ANIMATION_THROW_TRENCH, ANIMATION_DEATH, ANIMATION_DEATH_DOWN,
			ANIMATION_PRISONING, ANIMATION_IDLE, ANIMATION_IDLE_DOWN, ANIMATION_IDLE2, ANIMATION_LIE, ANIMATION_STAND, ANIMATION_USE_DOWN,
			ANIMATION_USE, ANIMATION_POINTING, ANIMATION_BINOCULARS, ANIMATION_RADIO,
		};
		for ( int i = 0; i < 24; ++i )
		{
			SUnitBaseRPGStats::SAnimDesc desc;
			desc.nIndex = i;
			desc.nAction = 0;
			desc.nLength = 0;
			desc.nAABB_A = -1;
			desc.nAABB_D = -1;
			if ( kTypes[i] == ANIMATION_IDLE )
				desc.nLength = 125;
			expected.animdescs[kTypes[i]].push_back( desc );
		}
		CheckExported( EExportKind::INFANTRY, "unt", run, "units/humans/unt/1.xml", expected, scratch );
		bool bNoAnimations = false;
		for ( const std::string &szWarning : run.outcome.warnings )
			bNoAnimations = bNoAnimations || szWarning.find( "no valid animations" ) != std::string::npos;
		Check( bNoAnimations && CountFiles( run.data, ".san" ) == 0, "export unt: no frames is MFC's \"no valid animations\" warning and no .san" );

		SExportContext statsOnly = context;
		statsOnly.bStatsOnly = true;
		const SExportRun runStats = RunExporter( "unt", project, scratch / "unt" / "data-stats", statsOnly );
		std::string szFull, szStats;
		Check( runStats.bExported && ReadBytes( run.data / "units/humans/unt/1.xml", &szFull ) && ReadBytes( runStats.data / "units/humans/unt/1.xml", &szStats ) && szFull == szStats,
		       "export unt: stats only writes the same 1.xml " + runStats.outcome.szError );
	}

	Check( FindExporter( "mip" ) != nullptr, "export: the mission exporter is registered (S14 T04)" );
	Check( FindExporter( "mdc" ) != nullptr, "export: the medal exporter is registered (S14 T01)" );
}

static bool EndsWith( const std::string &sz, const std::string &szSuffix )
{
	return sz.size() >= szSuffix.size() && SameNoCase( sz.substr( sz.size() - szSuffix.size() ), szSuffix );
}

// One golden file against the port's file of the same relative path, by the
// kind of file: stats through the engine's reader, .san and _h.dds as bytes,
// _c.dds within the DXT tolerance, _l.dds (uncompressed, no gate) and
// everything else (name.txt, icons, ...) byte for byte.
static SCompareResult CompareGoldenFile( EExportKind kind, const std::string &szRelative, const fs::path &port, const fs::path &golden, const SDxtTolerance &tolerance )
{
	if ( EndsWith( szRelative, ".xml" ) )
		return CompareGolden( kind, port.string(), golden.string() );
	if ( EndsWith( szRelative, "_c.dds" ) || EndsWith( szRelative, "_l.dds" ) )
		return CompareDxt( port.string(), golden.string(), tolerance );
	return CompareBytes( port.string(), golden.string() );
}

// What a kind's golden comparison accepts beyond CompareGolden's explained
// stats differences: files whose difference is explained for the whole file,
// each with its reason (by the end of the file name), and a DXT gate of its
// own. Both are listed in the log and the spec; the default accepts nothing.
struct SGoldenRules
{
	std::vector<std::pair<std::string, std::string>> pendingSuffixes;   // file name suffix, reason; reported pending, not accepted
	const SDxtTolerance *pTolerance = nullptr;
	const char *szToleranceReason = "";
};

struct SGoldenResult
{
	int nFiles = 0;
	std::vector<std::string> accepted;   // "<file>: <difference> [<reason>]", proven from MFC's source and the golden bytes
	std::vector<std::string> pending;    // "<file>: <difference> [<reason>]", cause not proven, waits for a regenerated golden
	std::vector<std::string> failures;   // "<file>: <reason>"
	std::vector<std::string> details;    // every difference line of every failed file, "<file>: <message>"
};

// A golden 1.xml that holds only the <History> element MFC's batch mode adds:
// the editor exported no stats (a sprite has none; a failed export leaves
// just this).
static bool IsHistoryOnly( const fs::path &xml )
{
	std::string szXml;
	if ( !ReadBytes( xml, &szXml ) )
		return false;
	const std::string::size_type nEnd = szXml.find( "</History>" );
	return nEnd != std::string::npos && szXml.compare( nEnd + 10, 7, "</base>" ) == 0;
}

// MFC's batch mode writes the project's files at the top of its export folder
// and names the stats file 1.xml; the port writes the same files under their
// game-data folders (objects/obt/1.xml, weapons/wpn.xml). The port's root is
// the folder holding its main stats file, and golden 1.xml is compared with
// that file whatever the port calls it. A kind without a stats file (sprite)
// compares by the first file the golden shares with the port's folders.
static fs::path FindPortStatsFile( const fs::path &portData )
{
	std::error_code error;
	fs::path found;
	for ( fs::recursive_directory_iterator it( portData, error ), end; !error && it != end; it.increment( error ) )
	{
		if ( !it->is_regular_file() )
			continue;
		const std::string szName = it->path().filename().string();
		if ( SameNoCase( szName, "1.xml" ) )
			return it->path();
		if ( found.empty() && SameNoCase( szName, "wpn.xml" ) )
			found = it->path();
	}
	return found;
}

// For a port without a stats file: the folder of the port's own files, which is
// the one that holds the most of them.
static fs::path FindPortRoot( const fs::path &portData )
{
	const fs::path stats = FindPortStatsFile( portData );
	if ( !stats.empty() )
		return stats.parent_path();
	std::map<fs::path, int> counts;
	std::error_code error;
	for ( fs::recursive_directory_iterator it( portData, error ), end; !error && it != end; it.increment( error ) )
		if ( it->is_regular_file() )
			++counts[it->path().parent_path()];
	fs::path best = portData;
	int nBest = 0;
	for ( const auto &entry : counts )
		if ( entry.second > nBest )
		{
			best = entry.first;
			nBest = entry.second;
		}
	return best;
}

// Every file of a golden folder (README.md and .gitkeep aside) must exist in
// the port's root folder and compare equal.
static SGoldenResult CompareGoldenFolder( EExportKind kind, const fs::path &portData, const fs::path &golden, const SDxtTolerance &defaultTolerance, const SGoldenRules &rules = SGoldenRules() )
{
	const SDxtTolerance &tolerance = rules.pTolerance != nullptr ? *rules.pTolerance : defaultTolerance;
	SGoldenResult result;
	const fs::path portStats = FindPortStatsFile( portData );
	const fs::path portRoot = FindPortRoot( portData );
	std::error_code error;
	for ( fs::recursive_directory_iterator it( golden, error ), end; !error && it != end; it.increment( error ) )
	{
		const std::string szName = it->path().filename().string();
		if ( !it->is_regular_file() || szName == ".gitkeep" || szName == "README.md" )
			continue;
		++result.nFiles;
		const std::string szRelative = fs::relative( it->path(), golden ).generic_string();
		const fs::path port = SameNoCase( szRelative, "1.xml" ) && !portStats.empty() ? portStats : FindNoCase( portRoot, szRelative );
		if ( port.empty() && SameNoCase( szRelative, "1.xml" ) && IsHistoryOnly( it->path() ) )
		{
			result.accepted.push_back( szRelative + ": the golden holds only MFC's <History> and the port exports no stats file for this kind [a sprite's export is graphics only, MFC's 1.xml is the batch history]" );
			continue;
		}
		if ( port.empty() )
		{
			result.failures.push_back( szRelative + ": the port did not export this file" );
			continue;
		}
		SCompareResult compared = CompareGoldenFile( kind, szRelative, port, it->path(), defaultTolerance );
		if ( compared.status == ECompareStatus::DIFFERENT && rules.pTolerance != nullptr && EndsWith( szRelative, "_c.dds" ) )
		{
			// A gate of the kind's own: the difference is listed with its reason when it passes that one.
			const SCompareResult within = CompareGoldenFile( kind, szRelative, port, it->path(), tolerance );
			if ( within.status == ECompareStatus::EQUAL )
			{
				for ( const std::string &szMessage : compared.messages )
					result.accepted.push_back( szRelative + ": " + szMessage + " [" + rules.szToleranceReason + "]" );
				continue;
			}
			compared = within;
		}
		for ( const std::string &szExcused : compared.excused )
			result.accepted.push_back( szRelative + ": " + szExcused );
		for ( const std::string &szPending : compared.pending )
			result.pending.push_back( szRelative + ": " + szPending );
		if ( compared.status == ECompareStatus::EQUAL )
			continue;
		const std::pair<std::string, std::string> *pRule = nullptr;
		for ( const auto &rule : rules.pendingSuffixes )
			if ( EndsWith( szRelative, rule.first ) )
				pRule = &rule;
		if ( pRule != nullptr && compared.status == ECompareStatus::DIFFERENT )
		{
			for ( const std::string &szMessage : compared.messages )
				result.pending.push_back( szRelative + ": " + szMessage + " [" + pRule->second + "]" );
			continue;
		}
		result.failures.push_back( szRelative + ": " + CompareStatusName( compared.status ) + Messages( compared ) );
		for ( const std::string &szMessage : compared.messages )
			result.details.push_back( szRelative + ": " + szMessage );
	}
	return result;
}

// The sprite fixture with its Directory set to a frames folder that holds the
// fixture's one frame, so the port composes a real 1.san and DDS set.
static fs::path CopySpriteWithFrame( const fs::path &fixtures, const fs::path &scratch )
{
	const fs::path project = CopyFixture( fixtures, scratch, "spt" );
	std::string szXml;
	ReadBytes( project, &szXml );
	const std::string szOld = "<string_value>_.</string_value>";
	const std::string::size_type nAt = szXml.find( szOld );
	if ( nAt != std::string::npos )
		szXml.replace( nAt, szOld.size(), "<string_value>frames\\</string_value>" );
	WriteBytes( project, szXml );
	std::error_code error;
	fs::create_directories( project.parent_path() / "frames", error );
	fs::copy_file( project.parent_path() / "sprite-1frame.tga", project.parent_path() / "frames" / "sprite-1frame.tga", fs::copy_options::overwrite_existing, error );
	return project;
}

static void CopyTree( const fs::path &from, const fs::path &to )
{
	std::error_code error;
	fs::remove_all( to, error );
	fs::create_directories( to, error );
	fs::copy( from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, error );
}

static bool FlipByte( const fs::path &file, size_t nOffset )
{
	std::string bytes;
	if ( !ReadBytes( file, &bytes ) || nOffset >= bytes.size() )
		return false;
	bytes[nOffset] ^= 0x01;
	return WriteBytes( file, bytes );
}

// What tools/zig/win-home/export-goldens.ps1 does to its scratch copy of a
// fixture: MFC's batch mode refuses a project without a relative export file
// name, so <own_data><export_file_name>1.xml is put into it (a project the port
// saved holds an empty one, as MFC writes a new project's, which it fills). The
// port gets the same project, which makes it name its stats file and result
// folders the way MFC did for the golden (medals\name, not medals\mdc\name).
static void InjectExportFileName( const fs::path &project )
{
	std::string szXml;
	if ( !ReadBytes( project, &szXml ) )
		return;
	const std::string szEmpty = "<export_file_name></export_file_name>";
	const std::string::size_type nEmpty = szXml.find( szEmpty );
	if ( nEmpty != std::string::npos )
	{
		szXml.replace( nEmpty, szEmpty.size(), "<export_file_name>1.xml</export_file_name>" );
		WriteBytes( project, szXml );
		return;
	}
	if ( szXml.find( "<export_file_name>" ) != std::string::npos )
		return;
	const std::string szOpen = "<own_data>";
	const std::string szInjected = "<export_file_name>1.xml</export_file_name>";
	const std::string::size_type nAt = szXml.find( szOpen );
	if ( nAt != std::string::npos )
		szXml.insert( nAt + szOpen.size(), szInjected );
	else
	{
		const std::string::size_type nLast = szXml.rfind( "</" );
		if ( nLast == std::string::npos )
			return;
		szXml.insert( nLast, "<own_data>" + szInjected + "</own_data>\r\n" );
	}
	WriteBytes( project, szXml );
}

// The port's export laid out as MFC's batch mode lays out its golden: the
// files of the port's root folder at the top, the stats file named 1.xml.
static void FlattenAsMfc( const fs::path &portData, const fs::path &stand )
{
	std::error_code error;
	fs::remove_all( stand, error );
	fs::create_directories( stand, error );
	const fs::path stats = FindPortStatsFile( portData );
	for ( fs::directory_iterator it( FindPortRoot( portData ), error ), end; !error && it != end; it.increment( error ) )
	{
		if ( !it->is_regular_file() )
			continue;
		const bool bStats = !stats.empty() && it->path() == stats;
		fs::copy_file( it->path(), stand / ( bStats ? std::string( "1.xml" ) : it->path().filename().string() ), fs::copy_options::overwrite_existing, error );
	}
}

// The comparison must be able to fail. A stand-in golden folder is the port's
// own export; unchanged it passes, with one byte of 1.san (sprite) or one stats
// field of 1.xml (infantry) changed it must report exactly that file.
static void GoldenNegatives( const fs::path &fixtures, const fs::path &scratchRoot, const SDxtTolerance &tolerance, const SExportContext &context )
{
	const fs::path scratch = scratchRoot / "golden-negative";
	{
		const fs::path project = CopySpriteWithFrame( fixtures, scratch );
		const SExportRun run = RunExporter( "spt", project, scratch / "spt" / "data", context );
		Check( run.bExported && CountFiles( run.data, ".san" ) == 1 && CountFiles( run.data, ".dds" ) == 3,
		       "golden negative spt: the port exports 1.san and three DDS from the frame " + run.outcome.szError );
		const fs::path stand = scratch / "spt" / "stand-in-golden";
		FlattenAsMfc( run.data, stand );
		const SGoldenResult same = CompareGoldenFolder( EExportKind::WEAPON, run.data, stand, tolerance );
		Check( same.nFiles == 4 && same.failures.empty(), "golden negative spt: the port's own export passes as a golden (" + std::to_string( same.nFiles ) + " files)" );
		std::error_code error;
		fs::path sanFile;
		for ( fs::recursive_directory_iterator it( stand, error ), end; sanFile.empty() && !error && it != end; it.increment( error ) )
			if ( it->is_regular_file() && it->path().filename() == "1.san" )
				sanFile = it->path();
		Check( !sanFile.empty() && FlipByte( sanFile, 40 ), "golden negative spt: one byte of the stand-in 1.san is flipped" );
		const SGoldenResult bad = CompareGoldenFolder( EExportKind::WEAPON, run.data, stand, tolerance );
		bool bNamed = false;
		for ( const std::string &szFailure : bad.failures )
			bNamed = bNamed || szFailure.find( "1.san" ) != std::string::npos;
		Check( bad.failures.size() == 1 && bNamed, "golden negative spt: the flipped 1.san is reported as FAIL " + ( bad.failures.empty() ? std::string( "(nothing reported)" ) : bad.failures[0] ) );
	}
	{
		const fs::path project = CopyFixture( fixtures, scratch, "unt" );
		const SExportRun run = RunExporter( "unt", project, scratch / "unt" / "data", context );
		const fs::path stand = scratch / "unt" / "stand-in-golden";
		FlattenAsMfc( run.data, stand );
		const SGoldenResult same = CompareGoldenFolder( EExportKind::INFANTRY, run.data, stand, tolerance );
		Check( run.bExported && same.nFiles >= 1 && same.failures.empty(), "golden negative unt: the port's own export passes as a golden (" + std::to_string( same.nFiles ) + " files)" );
		const fs::path xml = stand / "1.xml";
		std::string szXml;
		const std::string szOld = "MaxHP=\"100\"";
		bool bChanged = !xml.empty() && ReadBytes( xml, &szXml );
		const std::string::size_type nAt = bChanged ? szXml.find( szOld ) : std::string::npos;
		if ( nAt != std::string::npos )
			szXml.replace( nAt, szOld.size(), "MaxHP=\"101\"" );
		Check( nAt != std::string::npos && WriteBytes( xml, szXml ), "golden negative unt: the stand-in 1.xml has MaxHP changed from 100 to 101" );
		const SGoldenResult bad = CompareGoldenFolder( EExportKind::INFANTRY, run.data, stand, tolerance );
		Check( bad.failures.size() == 1 && bad.failures[0].find( "1.xml" ) != std::string::npos && bad.failures[0].find( "MaxHP" ) != std::string::npos,
		       "golden negative unt: the changed stats field is reported as FAIL " + ( bad.failures.empty() ? std::string( "(nothing reported)" ) : bad.failures[0] ) );
	}
	{
		// MFC names a weapon's stats file 1.xml where the port names it wpn.xml,
		// and the port keeps it in a game-data folder: the layout alone is no
		// difference, and an unexplained field in it still is one.
		const fs::path project = CopyFixture( fixtures, scratch, "wpn" );
		InjectExportFileName( project );
		const SExportRun run = RunExporter( "wpn", project, scratch / "wpn" / "data", context );
		const fs::path stand = scratch / "wpn" / "stand-in-golden";
		FlattenAsMfc( run.data, stand );
		const SGoldenResult same = CompareGoldenFolder( EExportKind::WEAPON, run.data, stand, tolerance );
		Check( run.bExported && !FindPortStatsFile( run.data ).empty() && same.nFiles == 1 && same.failures.empty() && same.accepted.empty(),
		       "golden negative wpn: wpn.xml in its game-data folder equals a golden 1.xml at the top, with nothing to accept (" + std::to_string( same.nFiles ) + " files)" );
		const SGoldenResult wrongKind = CompareGoldenFolder( EExportKind::WEAPON, run.data, stand / "none", tolerance );
		Check( wrongKind.nFiles == 0, "golden negative wpn: a golden folder without files compares nothing" );
	}
	{
		// The six-digit rule: the golden holds the port's float as MFC's writer
		// prints it, not any float near it.
		const fs::path project = CopyFixture( fixtures, scratch, "unt" );
		const SExportRun run = RunExporter( "unt", project, scratch / "unt" / "data-digits", context );
		const fs::path port = FindPortStatsFile( run.data );
		std::string szPort;
		const std::string szOld = "MaxHP=\"100\"";
		bool bReady = run.bExported && !port.empty() && ReadBytes( port, &szPort );
		const std::string::size_type nAt = bReady ? szPort.find( szOld ) : std::string::npos;
		if ( nAt != std::string::npos )
		{
			std::string szLong = szPort;
			szLong.replace( nAt, szOld.size(), "MaxHP=\"100.123456789\"" );
			Check( WriteBytes( port, szLong ), "golden negative digits: the port's 1.xml has MaxHP 100.123456789" );
			for ( const char *pszGolden : { "100.123", "100.124" } )
			{
				const fs::path stand = scratch / "unt" / ( std::string( "stand-in-digits-" ) + pszGolden );
				FlattenAsMfc( run.data, stand );
				std::string szGolden = szPort;
				szGolden.replace( nAt, szOld.size(), std::string( "MaxHP=\"" ) + pszGolden + "\"" );
				WriteBytes( stand / "1.xml", szGolden );
				const SGoldenResult result = CompareGoldenFolder( EExportKind::INFANTRY, run.data, stand, tolerance );
				const bool bRounded = std::string( pszGolden ) == "100.123";
				Check( bRounded ? ( result.failures.empty() && result.accepted.size() == 1 ) : ( result.failures.size() == 1 && result.accepted.empty() ),
				       std::string( "golden negative digits: a golden MaxHP of " ) + pszGolden + ( bRounded ? " is the port's value printed with six digits: accepted" : " is not: reported as FAIL" ) );
			}
		}
		else
			Check( false, "golden negative digits: the port's unt 1.xml holds MaxHP=\"100\"" );
	}
}

// The kinds whose golden cannot be compared yet, each with the verified
// reason; the comparator reports them pending, never as a pass. A reason that
// names what the golden holds is checked against the golden, so a regenerated
// golden turns the kind back into a comparison.
struct SPendingGolden
{
	const char *pszExt;
	const char *pszReason;
	bool ( *pfnHolds )( const fs::path &golden );   // null: not checkable
};

static bool GoldenIsHistoryOnly( const fs::path &golden )
{
	return IsHistoryOnly( golden / "1.xml" );
}

static bool GoldenEffectHasNoParticles( const fs::path &golden )
{
	std::string szXml;
	return ReadBytes( golden / "1.xml", &szXml ) && szXml.find( "<particles/>" ) != std::string::npos && szXml.find( "<sprites><item" ) != std::string::npos;
}

static const char kCrashReason[] = "the MFC editor crashed (0xC0000005) making this golden (commit 734245b31), so none exists; regenerate it on win-home with tools/zig/win-home/export-goldens.ps1 from a commit before the MFC editor's deletion";

static const SPendingGolden kPending[] =
{
	{ "bdg", kCrashReason, nullptr }, { "3rd", kCrashReason, nullptr }, { "3rv", kCrashReason, nullptr },
	{ "mip", kCrashReason, nullptr }, { "chc", kCrashReason, nullptr }, { "cgc", kCrashReason, nullptr },
	{ "scp", "the golden holds only MFC's <History>: CSquadFrame::SaveRPGStats (SquadFrm.cpp:216) stops at MakeName (SquadFrm.cpp:198) when a member such as \"USSR\\Mosin\" is not in the objects database of the installed game, so the editor wrote no stats; regenerate the golden on win-home with a member the installed data has", &GoldenIsHistoryOnly },
	{ "til", "the golden holds only MFC's <History>: the tileset export needs editor\\terrain\\tilemask.tga in the editor data folder, which the installed data does not have, and the port refuses for the same reason (\"Cannot open terrain mask file\"); regenerate the golden on win-home with the mask installed", &GoldenIsHistoryOnly },
	{ "eff", "MFC skipped the function particle \"particle-2key\" (EffectFrm.cpp:215: no stream, an error box, continue) because the installed data has no Effects\\particles\\particle-2key.xml, so the golden's <particles/> is empty; the port stops with an error for a missing source; regenerate the golden on win-home after exporting the particle fixture into the editor data folder", &GoldenEffectHasNoParticles },
};

// The golden comparison. A golden folder holds MFC's export of the fixture
// project; the port's export of the same project (with the export file name
// the golden's maker injected) is compared with it file by file, for every
// extension that has an exporter. The result per kind is pass, accepted (equal
// but for differences the comparator lists with a verified reason), pending
// (no usable golden; the reason is logged) or FAIL, which fails the tier.
static void Goldens( const fs::path &fixtures, const fs::path &scratchRoot )
{
	struct SGolden { const char *pszExt; EExportKind kind; };
	static const SGolden kExtensions[] =
	{
		{ "wpn", EExportKind::WEAPON }, { "mcp", EExportKind::MINE }, { "trc", EExportKind::ENTRENCHMENT },
		{ "scp", EExportKind::SQUAD }, { "spt", EExportKind::WEAPON }, { "unt", EExportKind::INFANTRY },
		{ "msh", EExportKind::MECH_UNIT }, { "obt", EExportKind::OBJECT }, { "fnc", EExportKind::FENCE },
		{ "bld", EExportKind::BUILDING }, { "bdg", EExportKind::BRIDGE }, { "pcp", EExportKind::PARTICLE },
		{ "eff", EExportKind::EFFECT }, { "til", EExportKind::TILESET }, { "3rd", EExportKind::VSO },
		{ "3rv", EExportKind::VSO }, { "mip", EExportKind::MISSION }, { "chc", EExportKind::CHAPTER },
		{ "cgc", EExportKind::CAMPAIGN }, { "mdc", EExportKind::MEDAL },
	};
	const fs::path scratch = scratchRoot / "golden";
	SDxtTolerance tolerance;
	std::string szError;
	Check( LoadDxtTolerance( ( fixtures / "dxt-tolerance.json" ).string(), &tolerance, &szError ), "golden: the DXT gate loads " + szError );
	// The trench's three _c.dds: every 4 x 4 block of the 16 x 16 picture is
	// one colour, and the shipped editor's encoder writes those blocks with
	// both endpoints moved (colour 0xaf7e and 0xb79e, alpha ff and 00, for the
	// source colour 0xb2f1f8), where NDxt writes one endpoint twice; the
	// decoded colour is off by at most 6. NLegacyDxt, the MFC-era source code,
	// writes yet another block, so the shipped editor's encoder is neither.
	SDxtTolerance trenchTolerance = tolerance;
	for ( auto &format : trenchTolerance.formats )
		if ( format.first == "DXT5" )
			format.second.nColourMax = format.second.nColourP99 = std::max( format.second.nColourMax, 6 );
	SExportContext context;
	context.findUnitKey = &FixtureUnitKey;
	int nPass = 0, nAccepted = 0, nFail = 0, nPending = 0;
	for ( const SGolden &entry : kExtensions )
	{
		const std::string szExt = entry.pszExt;
		const fs::path goldenDir = fixtures / szExt / "golden";
		bool bAny = false;
		std::error_code error;
		for ( fs::recursive_directory_iterator it( goldenDir, error ), end; !error && it != end; it.increment( error ) )
		{
			const std::string szName = it->path().filename().string();
			bAny = bAny || ( it->is_regular_file() && szName != ".gitkeep" && szName != "README.md" );
		}
		const SPendingGolden *pPending = nullptr;
		for ( const SPendingGolden &pending : kPending )
			if ( szExt == pending.pszExt )
				pPending = &pending;
		if ( pPending != nullptr && ( !bAny || pPending->pfnHolds == nullptr || pPending->pfnHolds( goldenDir ) ) )
		{
			Log( "GOLDEN " + szExt + " pending: " + pPending->pszReason );
			++nPending;
			continue;
		}
		if ( !bAny )
		{
			Log( "GOLDEN " + szExt + " FAIL: no golden and no recorded reason (run tools/zig/win-home/export-goldens.ps1 on win-home)" );
			++nFail;
			++g_nFailures;
			continue;
		}
		if ( pPending != nullptr )
			Log( "GOLDEN " + szExt + " note: the recorded pending reason no longer holds for this golden, comparing it" );
		const fs::path project = CopyFixture( fixtures, scratch, szExt );
		InjectExportFileName( project );
		const SExportRun run = RunExporter( szExt, project, scratch / szExt / "data", context );
		if ( !run.bExported )
		{
			Log( "GOLDEN " + szExt + " FAIL: the port's export failed: " + run.outcome.szError );
			++nFail;
			++g_nFailures;
			continue;
		}
		SGoldenRules rules;
		if ( szExt == "trc" )
		{
			rules.pTolerance = &trenchTolerance;
			rules.szToleranceReason = "the shipped editor's DXT5 encoder writes a solid-colour 4x4 block with both endpoints moved (colour 0xaf7e and 0xb79e, alpha ff and 00 for the source colour 0xb2f1f8) where NDxt writes one endpoint twice; the decoded colour is off by 3 to 6, the gate for this kind is 6. NLegacyDxt, the MFC-era source code, writes a third block, so the shipped encoder is neither";
		}
		if ( szExt == "obt" )
		{
			static const char kReason[] = "pending, camera-dependent and not proven: the sprite packs follow the zero cross, the passability grid and its origin, which MFC takes from its live scene camera and tile lists and the port from DefaultEditorCamera and the project's desc (the fixture has none), so the packed pictures and animations differ in size";
			for ( const char *pszSuffix : { ".san", "_c.dds", "_l.dds", "_h.dds" } )
				rules.pendingSuffixes.push_back( { pszSuffix, kReason } );
		}
		const SGoldenResult result = CompareGoldenFolder( entry.kind, run.data, goldenDir, tolerance, rules );
		auto summarise = []( const std::vector<std::string> &lines, std::string *pszAll, std::map<std::string, int> *pReasons ) {
			for ( const std::string &szLine : lines )
			{
				*pszAll += szLine + "\n";
				const std::string::size_type nReason = szLine.rfind( " [" );
				std::string szReason = nReason == std::string::npos ? szLine : szLine.substr( nReason + 2 );
				if ( !szReason.empty() && szReason.back() == ']' )
					szReason.pop_back();
				++( *pReasons )[szReason];
			}
		};
		std::string szAccepted, szPendingLines;
		std::map<std::string, int> reasons, pendingReasons;
		summarise( result.accepted, &szAccepted, &reasons );
		summarise( result.pending, &szPendingLines, &pendingReasons );
		if ( !result.accepted.empty() )
			WriteBytes( scratch / szExt / "accepted.txt", szAccepted );
		if ( !result.pending.empty() )
			WriteBytes( scratch / szExt / "pending.txt", szPendingLines );
		if ( result.failures.empty() )
		{
			if ( result.accepted.empty() && result.pending.empty() )
			{
				Log( "GOLDEN " + szExt + " pass (" + std::to_string( result.nFiles ) + " files)" );
				++nPass;
				continue;
			}
			// A kind with any unproven difference is pending, whatever else
			// was accepted: the accepted part is listed beside it.
			const bool bPendingKind = !result.pending.empty();
			Log( "GOLDEN " + szExt + ( bPendingKind ? " pending (" : " accepted (" ) + std::to_string( result.nFiles ) + " files, " + std::to_string( result.accepted.size() ) + " accepted and " + std::to_string( result.pending.size() ) + " pending differences, listed in golden/" + szExt + "/accepted.txt and pending.txt)" );
			for ( const auto &reason : reasons )
				Log( "GOLDEN " + szExt + " accepted " + std::to_string( reason.second ) + " x: " + reason.first );
			for ( const auto &reason : pendingReasons )
				Log( "GOLDEN " + szExt + " pending " + std::to_string( reason.second ) + " x: " + reason.first );
			++( bPendingKind ? nPending : nAccepted );
			continue;
		}
		std::string szDetails;
		for ( const std::string &szDetail : result.details )
			szDetails += szDetail + "\n";
		WriteBytes( scratch / szExt / "differences.txt", szDetails );
		for ( const std::string &szFailure : result.failures )
			Log( "GOLDEN " + szExt + " FAIL " + szFailure );
		++nFail;
		++g_nFailures;
	}
	Log( "GOLDEN_SUMMARY extensions=20 pass=" + std::to_string( nPass ) + " accepted=" + std::to_string( nAccepted ) + " fail=" + std::to_string( nFail ) + " pending=" + std::to_string( nPending ) );
	GoldenNegatives( fixtures, scratchRoot, tolerance, context );
}

int main( int argc, char **argv )
{
	if ( argc < 5 )
	{
		std::printf( "usage: %s <installation> <scratch> <repo Data> <fixture root>\n", argv[0] );
		return 2;
	}
	const fs::path scratch = fs::path( argv[2] ) / "resource_model" / "comparator";
	const fs::path data = argv[3], fixtures = argv[4];
	std::error_code error;
	fs::remove_all( scratch, error );
	fs::create_directories( scratch, error );

	std::string szError;
	if ( !Check( StartEngineReaders( &szError ), "the engine's StreamIO loads beside the executable " + szError ) )
		return 1;
	// The graphics exports go through the engine's image processor, which the
	// game registers from the Image module at start-up.
	{
		static NPlatform::DynamicLibrary image;
#if defined(_WIN32)
		const std::string szImage = NPlatform::Paths::ModuleRoot() + "\\Image.dll";
#elif defined(__APPLE__)
		const std::string szImage = NPlatform::Paths::ModuleRoot() + "/libImage.dylib";
#else
		const std::string szImage = NPlatform::Paths::ModuleRoot() + "/libImage.so";
#endif
		typedef const SModuleDescriptor *( STDCALL *FGetDescriptor )();
		FGetDescriptor pfnGetDescriptor = image.Load( szImage.c_str() ) ? reinterpret_cast<FGetDescriptor>( image.GetFunction( "GetModuleDescriptor" ) ) : nullptr;
		const SModuleDescriptor *pDesc = pfnGetDescriptor ? pfnGetDescriptor() : nullptr;
		if ( !Check( pDesc != 0 && pDesc->pFactory != 0, "the engine's Image module loads beside the executable: " + szImage ) )
			return 1;
		CPtr<IImageProcessor> pIP = CreateObject<IImageProcessor>( pDesc->pFactory, IMAGE_PROCESSOR );
		RegisterSingleton( IImageProcessor::tidTypeID, pIP );
	}
	std::map<EExportKind, fs::path> samples;
	ShippedSelfCompare( data, scratch, &samples );
	PlantedChanges( scratch, samples );
	BytesAndDxt( data, scratch );
	DxtGate( data, scratch, fixtures );
	Exporters( fixtures, data, scratch );
	Goldens( fixtures, scratch );
	RunUiScreenTests( data, scratch, Check );

	Log( g_nFailures == 0 ? "VERDICT=PASS" : "VERDICT=FAIL failures=" + std::to_string( g_nFailures ) );
	std::ofstream log( fs::path( argv[2] ) / "resource_model" / "comparator.log", std::ios::binary | std::ios::trunc );
	log << g_szLog;
	return g_nFailures == 0 ? 0 : 1;
}
